#include "basic_evm_tester.hpp"

namespace fc {

void to_variant(const intx::uint256& o, fc::variant& v)
{
   std::string output = intx::to_string(o, 10);
   v = std::move(output);
}

void to_variant(const evmc::address& o, fc::variant& v)
{
   std::string output = "0x";
   output += fc::to_hex((char*)o.bytes, sizeof(o.bytes));
   v = std::move(output);
}

} // namespace fc

namespace evm_test {

bool balance_and_dust::operator==(const balance_and_dust& o) const { return balance == o.balance && dust == o.dust; }
bool balance_and_dust::operator!=(const balance_and_dust& o) const { return !(*this == o); }

balance_and_dust::operator intx::uint256() const
{
   intx::uint256 result = balance.get_amount();
   result *= intx::exp(10_u256, intx::uint256{18 - balance.decimals()});
   result += dust;
   return result;
}

struct vault_balance_row
{
   name owner;
   asset balance;
   uint64_t dust = 0;
};

struct partial_account_table_row
{
   uint64_t id;
   bytes eth_address;
   uint64_t nonce;
   bytes balance;
};

struct storage_table_row
{
   uint64_t id;
   bytes key;
   bytes value;
};

} // namespace evm_test

FC_REFLECT(evm_test::vault_balance_row, (owner)(balance)(dust))
FC_REFLECT(evm_test::partial_account_table_row, (id)(eth_address)(nonce)(balance))
FC_REFLECT(evm_test::storage_table_row, (id)(key)(value))

namespace evm_test {

evm_eoa::evm_eoa(std::basic_string<uint8_t> optional_private_key)
{
   if (optional_private_key.size() == 0) {
      // No private key specified. So randomly generate one.
      fc::rand_bytes((char*)private_key.data(), private_key.size());
   } else {
      if (optional_private_key.size() != 32) {
         throw std::runtime_error("private key provided to evm_eoa must be exactly 32 bytes");
      }
      std::memcpy(private_key.data(), optional_private_key.data(), private_key.size());
   }

   public_key.resize(65);

   secp256k1_pubkey pubkey;
   BOOST_REQUIRE(secp256k1_ec_pubkey_create(ctx, &pubkey, private_key.data()));

   size_t serialized_result_sz = public_key.size();
   secp256k1_ec_pubkey_serialize(ctx, public_key.data(), &serialized_result_sz, &pubkey, SECP256K1_EC_UNCOMPRESSED);

   std::optional<evmc::address> addr = silkworm::ecdsa::public_key_to_address(public_key);
   BOOST_REQUIRE(!!addr);
   address = *addr;
}

std::string evm_eoa::address_0x() const { return fc::variant(address).as_string(); }

key256_t evm_eoa::address_key256() const
{
   uint8_t buffer[32] = {0};
   memcpy(buffer, address.bytes, sizeof(address.bytes));
   return fixed_bytes<32>(buffer).get_array();
}

void evm_eoa::sign(silkworm::Transaction& trx)
{
   silkworm::Bytes rlp;
   trx.chain_id = basic_evm_tester::evm_chain_id;
   trx.nonce = next_nonce++;
   silkworm::rlp::encode(rlp, trx, true, false);
   ethash::hash256 hash{silkworm::keccak256(rlp)};

   secp256k1_ecdsa_recoverable_signature sig;
   BOOST_REQUIRE(secp256k1_ecdsa_sign_recoverable(ctx, &sig, hash.bytes, private_key.data(), NULL, NULL));
   uint8_t r_and_s[64];
   int recid;
   secp256k1_ecdsa_recoverable_signature_serialize_compact(ctx, r_and_s, &recid, &sig);

   trx.r = intx::be::unsafe::load<intx::uint256>(r_and_s);
   trx.s = intx::be::unsafe::load<intx::uint256>(r_and_s + 32);
   trx.odd_y_parity = recid;
}

evm_eoa::~evm_eoa() { secp256k1_context_destroy(ctx); }


evmc::address basic_evm_tester::make_reserved_address(uint64_t account)
{
   // clang-format off
   return evmc_address({0xbb, 0xbb, 0xbb, 0xbb,
                        0xbb, 0xbb, 0xbb, 0xbb,
                        0xbb, 0xbb, 0xbb, 0xbb,
                        static_cast<uint8_t>(account >> 56),
                        static_cast<uint8_t>(account >> 48),
                        static_cast<uint8_t>(account >> 40),
                        static_cast<uint8_t>(account >> 32),
                        static_cast<uint8_t>(account >> 24),
                        static_cast<uint8_t>(account >> 16),
                        static_cast<uint8_t>(account >> 8),
                        static_cast<uint8_t>(account >> 0)});
   // clang-format on
}

basic_evm_tester::basic_evm_tester(std::string native_symbol_str) :
   native_symbol(symbol::from_string(native_symbol_str))
{
   create_accounts({token_account_name, faucet_account_name, evm_account_name});
   produce_block();

   set_code(token_account_name, testing::contracts::eosio_token_wasm());
   set_abi(token_account_name, testing::contracts::eosio_token_abi().data());

   push_action(token_account_name,
               "create"_n,
               token_account_name,
               mvo()("issuer", "eosio.token"_n)("maximum_supply", asset(10'000'000'000'0000, native_symbol)));
   push_action(token_account_name,
               "issue"_n,
               token_account_name,
               mvo()("to", faucet_account_name)("quantity", asset(1'000'000'000'0000, native_symbol))("memo", ""));

   produce_block();

   set_code(evm_account_name, testing::contracts::evm_runtime_wasm());
   set_abi(evm_account_name, testing::contracts::evm_runtime_abi().data());
   produce_block();
}

asset basic_evm_tester::make_asset(int64_t amount) const { return asset(amount, native_symbol); }

transaction_trace_ptr basic_evm_tester::transfer_token(name from, name to, asset quantity, std::string memo)
{
   return push_action(
      token_account_name, "transfer"_n, from, mvo()("from", from)("to", to)("quantity", quantity)("memo", memo));
}

void basic_evm_tester::init(const uint64_t chainid,
                            const uint64_t gas_price,
                            const uint32_t miner_cut,
                            const std::optional<asset> ingress_bridge_fee,
                            const bool also_prepare_self_balance)
{
   mvo fee_params;
   fee_params("gas_price", gas_price)("miner_cut", miner_cut);

   if (ingress_bridge_fee.has_value()) {
      fee_params("ingress_bridge_fee", *ingress_bridge_fee);
   } else {
      fee_params("ingress_bridge_fee", fc::variant());
   }

   push_action(evm_account_name, "init"_n, evm_account_name, mvo()("chainid", chainid)("fee_params", fee_params));

   if (also_prepare_self_balance) {
      prepare_self_balance();
   }
}

void basic_evm_tester::prepare_self_balance(uint64_t fund_amount)
{
   // Ensure internal balance for evm_account_name has at least 1 EOS to cover max bridge gas fee with even high gas
   // price.
   transfer_token(faucet_account_name, evm_account_name, make_asset(1'0000), evm_account_name.to_string());
}

config_table_row basic_evm_tester::get_config() const
{
   static constexpr eosio::chain::name config_singleton_name = "config"_n;
   const vector<char> d =
      get_row_by_account(evm_account_name, evm_account_name, config_singleton_name, config_singleton_name);
   return fc::raw::unpack<config_table_row>(d);
}

void basic_evm_tester::setfeeparams(const fee_parameters& fee_params)
{
   mvo fee_params_vo;

   if (fee_params.gas_price.has_value()) {
      fee_params_vo("gas_price", *fee_params.gas_price);
   } else {
      fee_params_vo("gas_price", fc::variant());
   }

   if (fee_params.miner_cut.has_value()) {
      fee_params_vo("miner_cut", *fee_params.miner_cut);
   } else {
      fee_params_vo("miner_cut", fc::variant());
   }

   if (fee_params.ingress_bridge_fee.has_value()) {
      fee_params_vo("ingress_bridge_fee", *fee_params.ingress_bridge_fee);
   } else {
      fee_params_vo("ingress_bridge_fee", fc::variant());
   }

   push_action(evm_account_name, "setfeeparams"_n, evm_account_name, mvo()("fee_params", fee_params_vo));
}

silkworm::Transaction
basic_evm_tester::generate_tx(const evmc::address& to, const intx::uint256& value, uint64_t gas_limit) const
{
   const auto gas_price = get_config().gas_price;

   return silkworm::Transaction{
      .type = silkworm::Transaction::Type::kLegacy,
      .max_priority_fee_per_gas = gas_price,
      .max_fee_per_gas = gas_price,
      .gas_limit = gas_limit,
      .to = to,
      .value = value,
   };
}

void basic_evm_tester::pushtx(const silkworm::Transaction& trx, name miner)
{
   silkworm::Bytes rlp;
   silkworm::rlp::encode(rlp, trx);

   bytes rlp_bytes;
   rlp_bytes.resize(rlp.size());
   memcpy(rlp_bytes.data(), rlp.data(), rlp.size());

   push_action(evm_account_name, "pushtx"_n, miner, mvo()("miner", miner)("rlptx", rlp_bytes));
}

evmc::address basic_evm_tester::deploy_contract(evm_eoa& eoa, evmc::bytes bytecode)
{
   uint64_t nonce = eoa.next_nonce;

   const auto gas_price = get_config().gas_price;

   silkworm::Transaction tx{
      .type = silkworm::Transaction::Type::kLegacy,
      .max_priority_fee_per_gas = gas_price,
      .max_fee_per_gas = gas_price,
      .gas_limit = 10'000'000,
      .data = std::move(bytecode),
   };

   eoa.sign(tx);
   pushtx(tx);

   return silkworm::create_address(eoa.address, nonce);
}

void basic_evm_tester::addegress(const std::vector<name>& accounts)
{
   push_action(evm_account_name, "addegress"_n, evm_account_name, mvo()("accounts", accounts));
}

void basic_evm_tester::removeegress(const std::vector<name>& accounts)
{
   push_action(evm_account_name, "removeegress"_n, evm_account_name, mvo()("accounts", accounts));
}

void basic_evm_tester::open(name owner) { push_action(evm_account_name, "open"_n, owner, mvo()("owner", owner)); }

void basic_evm_tester::close(name owner) { push_action(evm_account_name, "close"_n, owner, mvo()("owner", owner)); }

void basic_evm_tester::withdraw(name owner, asset quantity)
{
   push_action(evm_account_name, "withdraw"_n, owner, mvo()("owner", owner)("quantity", quantity));
}

balance_and_dust basic_evm_tester::vault_balance(name owner) const
{
   const vector<char> d = get_row_by_account(evm_account_name, evm_account_name, "balances"_n, owner);
   FC_ASSERT(d.size(), "EVM not open");
   auto [_, amount, dust] = fc::raw::unpack<vault_balance_row>(d);
   return {.balance = amount, .dust = dust};
}

std::optional<intx::uint256> basic_evm_tester::evm_balance(const evmc::address& address) const
{
   const auto& a = find_account_by_address(address);

   if (!a) {
      return std::nullopt;
   }

   return a->balance;
}

std::optional<intx::uint256> basic_evm_tester::evm_balance(const evm_eoa& account) const
{
   return evm_balance(account.address);
}

std::optional<account_object> convert_to_account_object(const partial_account_table_row& row)
{
   evmc::address address(0);

   if (row.eth_address.size() != sizeof(address.bytes)) {
      return std::nullopt;
   }

   if (row.balance.size() != 32) {
      return std::nullopt;
   }

   std::memcpy(address.bytes, row.eth_address.data(), sizeof(address.bytes));

   return account_object{
      .id = row.id,
      .address = std::move(address),
      .nonce = row.nonce,
      .balance = intx::be::unsafe::load<intx::uint256>(reinterpret_cast<const uint8_t*>(row.balance.data())),
   };
}

bool basic_evm_tester::scan_accounts(std::function<bool(account_object)> visitor) const
{
   static constexpr eosio::chain::name account_table_name = "account"_n;

   bool successful = true;

   scan_table<partial_account_table_row>(
      account_table_name, evm_account_name, [this, &visitor, &successful](partial_account_table_row&& row) {
         if (auto obj = convert_to_account_object(row)) {
            return visitor(std::move(*obj));
         }
         successful = false;
         return true;
      });

   return successful;
}

std::optional<account_object> basic_evm_tester::scan_for_account_by_address(const evmc::address& address) const
{
   std::optional<account_object> result;

   std::basic_string_view<uint8_t> address_view{address};

   scan_accounts([&](account_object&& account) -> bool {
      if (account.address == address) {
         result.emplace(account);
         return true;
      }
      return false;
   });

   return result;
}

std::optional<account_object> basic_evm_tester::find_account_by_address(const evmc::address& address) const
{
   static constexpr eosio::chain::name account_table_name = "account"_n;

   std::optional<account_object> result;

   const auto& db = control->db();

   const auto* t_id = db.find<chain::table_id_object, chain::by_code_scope_table>(
      boost::make_tuple(evm_account_name, evm_account_name, account_table_name));

   if (!t_id) {
      return std::nullopt;
   }

   uint8_t address_buffer[32] = {0};
   std::memcpy(address_buffer, address.bytes, sizeof(address.bytes));

   const auto* secondary_row = db.find<chain::index256_object, chain::by_secondary>(
      boost::make_tuple(t_id->id, fixed_bytes<32>(address_buffer).get_array()));

   if (!secondary_row) {
      return std::nullopt;
   }

   const auto* primary_row = db.find<chain::key_value_object, chain::by_scope_primary>(
      boost::make_tuple(t_id->id, secondary_row->primary_key));

   if (!primary_row) {
      return std::nullopt;
   }

   {
      partial_account_table_row row;
      fc::datastream<const char*> ds(primary_row->value.data(), primary_row->value.size());
      fc::raw::unpack(ds, row);

      result = convert_to_account_object(row);
   }

   return result;
}

bool basic_evm_tester::scan_account_storage(uint64_t account_id, std::function<bool(storage_slot)> visitor) const
{
   static constexpr eosio::chain::name storage_table_name = "storage"_n;

   bool successful = true;

   scan_table<storage_table_row>(
      storage_table_name, name{account_id}, [&visitor, &successful](storage_table_row&& row) {
         if (row.key.size() != 32 || row.value.size() != 32) {
            successful = false;
            return true;
         }
         return visitor(storage_slot{
            .id = row.id,
            .key = intx::be::unsafe::load<intx::uint256>(reinterpret_cast<const uint8_t*>(row.key.data())),
            .value = intx::be::unsafe::load<intx::uint256>(reinterpret_cast<const uint8_t*>(row.value.data()))});
      });

   return successful;
}

} // namespace evm_test