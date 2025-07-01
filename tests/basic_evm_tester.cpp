#include "basic_evm_tester.hpp"
#include <fc/io/raw.hpp>
#include <secp256k1_recovery.h>

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
   asset balance{};
   uint64_t dust = 0;
};

struct partial_account_table_row
{
   uint64_t id;
   bytes eth_address;
   uint64_t nonce;
   bytes balance;
   std::optional<uint64_t> code_id;
   uint32_t flags;
};

struct storage_table_row
{
   uint64_t id;
   bytes key;
   bytes value;
};

} // namespace evm_test

namespace fc { namespace raw {
    template<>
    inline void unpack( datastream<const char*>& ds, evm_test::partial_account_table_row& tmp)
    { try  {
      fc::raw::unpack(ds, tmp.id);
      fc::raw::unpack(ds, tmp.eth_address);
      fc::raw::unpack(ds, tmp.nonce);
      fc::raw::unpack(ds, tmp.balance);
      fc::raw::unpack(ds, tmp.code_id);
      tmp.flags=0;
      if(ds.remaining()) { fc::raw::unpack(ds, tmp.flags); }
    } FC_RETHROW_EXCEPTIONS(warn, "error unpacking partial_account_table_row") }

     template<>
    inline void unpack( datastream<const char*>& ds, evm_test::config_table_row& tmp)
    { try  {
      fc::raw::unpack(ds, tmp.version);
      fc::raw::unpack(ds, tmp.chainid);
      fc::raw::unpack(ds, tmp.genesis_time);
      fc::raw::unpack(ds, tmp.ingress_bridge_fee);
      fc::raw::unpack(ds, tmp.gas_price);
      fc::raw::unpack(ds, tmp.miner_cut);
      fc::raw::unpack(ds, tmp.status);

      tmp.evm_version = {};
      if(ds.remaining()) {
         evm_test::evm_version_type version;
         fc::raw::unpack(ds, version);
         tmp.evm_version.emplace(version);
      }
      if(ds.remaining()) {
         evm_test::consensus_parameter_type consensus_parameter;
         fc::raw::unpack(ds, consensus_parameter);
         tmp.consensus_parameter.emplace(consensus_parameter);
      }
      if(ds.remaining()) {
         name token_contract;
         fc::raw::unpack(ds, token_contract);
         tmp.token_contract.emplace(token_contract);
      }
      if(ds.remaining()) {
         uint32_t queue_front_block;
         fc::raw::unpack(ds, queue_front_block);
         tmp.queue_front_block.emplace(queue_front_block);
      }
      if(ds.remaining()) {
         uint64_t ingress_gas_limit;
         fc::raw::unpack(ds, ingress_gas_limit);
         tmp.ingress_gas_limit.emplace(ingress_gas_limit);
      }
      if(ds.remaining()) {
         evm_test::gas_prices_type prices;
         fc::raw::unpack(ds, prices);
         tmp.gas_prices.emplace(prices);
      }

    } FC_RETHROW_EXCEPTIONS(warn, "error unpacking partial_account_table_row") }
}}


FC_REFLECT(evm_test::vault_balance_row, (owner)(balance)(dust))
FC_REFLECT(evm_test::partial_account_table_row, (id)(eth_address)(nonce)(balance)(code_id)(flags))
FC_REFLECT(evm_test::storage_table_row, (id)(key)(value))

namespace evm_test {

// Copied from old silkworm code as new silkworm code do not expose this function
std::optional<evmc::address> public_key_to_address(const std::basic_string<uint8_t>& public_key) noexcept {
    if (public_key.length() != 65 || public_key[0] != 4u) {
        return std::nullopt;
    }
    // Ignore first byte of public key
    const auto key_hash{ethash::keccak256(public_key.data() + 1, 64)};
    return evmc::address(*reinterpret_cast<const evmc_address*>(&key_hash.bytes[12]));
}

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

   std::optional<evmc::address> addr = public_key_to_address(public_key);

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

void evm_eoa::sign(silkworm::Transaction& trx) {
   sign(trx, basic_evm_tester::evm_chain_id);
}

void evm_eoa::sign(silkworm::Transaction& trx, std::optional<uint64_t> evm_chain_id)
{
   silkworm::Bytes rlp;
   if(evm_chain_id.has_value())
      trx.chain_id = evm_chain_id.value();
   trx.nonce = next_nonce++;
   trx.encode_for_signing(rlp);
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

evmc::address basic_evm_tester::make_reserved_address(name account) {
   return make_reserved_address(account.to_uint64_t());
}

basic_evm_tester::basic_evm_tester(std::string native_symbol_str) :
   native_symbol(symbol::from_string(native_symbol_str))
{
   create_accounts({token_account_name, faucet_account_name, evm_account_name, vaulta_account_name});
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

   set_code(vaulta_account_name, testing::contracts::vaulta_system_wasm());
   set_abi(vaulta_account_name, testing::contracts::vaulta_system_abi().data());
   push_action(vaulta_account_name, "init"_n, vaulta_account_name, 
               mvo()("maximum_supply",asset(10'000'000'000'0000, new_gas_symbol)));
   produce_block();

   set_code(evm_account_name, testing::contracts::evm_runtime_wasm());
   set_abi(evm_account_name, testing::contracts::evm_runtime_abi().data());
   produce_block();
}

asset basic_evm_tester::make_asset(int64_t amount) const { return asset(amount, native_symbol); }

transaction_trace_ptr basic_evm_tester::swapgastoken() {

   return push_action(
      evm_account_name, "swapgastoken"_n, evm_account_name, mvo()("new_token_contract", vaulta_account_name)("new_symbol",new_gas_symbol)("swap_dest_account",vaulta_account_name)("swap_memo","swap gas token"));
}

transaction_trace_ptr basic_evm_tester::migratebal(name from_name, int limit) {

   return push_action(
      evm_account_name, "migratebal"_n, evm_account_name, mvo()("from_name", from_name)("limit",limit));
}

transaction_trace_ptr basic_evm_tester::transfer_token(name from, name to, asset quantity, std::string memo, name acct)
{
   return push_action(
      acct, "transfer"_n, from, mvo()("from", from)("to", to)("quantity", quantity)("memo", memo));
}

action basic_evm_tester::get_action( account_name code, action_name acttype, vector<permission_level> auths,
                                 const bytes& data )const { try {
   const auto& acnt = control->get_account(code);
   auto abi = acnt.get_abi();
   chain::abi_serializer abis(abi, abi_serializer::create_yield_function( abi_serializer_max_time ));

   string action_type_name = abis.get_action_type(acttype);
   FC_ASSERT( action_type_name != string(), "unknown action type ${a}", ("a",acttype) );

   action act;
   act.account = code;
   act.name = acttype;
   act.authorization = auths;
   act.data = data;
   return act;
} FC_CAPTURE_AND_RETHROW() }

transaction_trace_ptr basic_evm_tester::push_action( const account_name& code,
                                    const action_name& acttype,
                                    const account_name& actor,
                                    const bytes& data,
                                    uint32_t expiration,
                                    uint32_t delay_sec)
{
   vector<permission_level> auths;
   auths.push_back( permission_level{actor, config::active_name} );
   try {
   signed_transaction trx;
   trx.actions.emplace_back( get_action( code, acttype, auths, data ) );
   set_transaction_headers( trx, expiration, delay_sec );
   for (const auto& auth : auths) {
      trx.sign( get_private_key( auth.actor, auth.permission.to_string() ), control->get_chain_id() );
   }

   return push_transaction( trx );
} FC_CAPTURE_AND_RETHROW( (code)(acttype)(auths)(data)(expiration)(delay_sec) ) }

transaction_trace_ptr basic_evm_tester::push_action2( const account_name& code,
   const action_name& acttype,
   const account_name& actor,
   const account_name& actor2,
   const variant_object &data,
   uint32_t expiration,
   uint32_t delay_sec)
{
   vector<permission_level> auths;
   auths.push_back( permission_level{actor, config::active_name} );
   if (actor != actor2)
      auths.push_back( permission_level{actor2, config::active_name} );

   try {
      signed_transaction trx;

      trx.actions.emplace_back( base_tester::get_action( code, acttype, auths, data ));
      set_transaction_headers( trx, expiration, delay_sec );
      for (const auto& auth : auths) {
      trx.sign( get_private_key( auth.actor, auth.permission.to_string() ), control->get_chain_id() );
   }

   return push_transaction( trx );
   }
   FC_CAPTURE_AND_RETHROW( (code)(acttype)(auths)(data)(expiration)(delay_sec) ) 
}


void basic_evm_tester::init(const uint64_t chainid,
                            const uint64_t gas_price,
                            const uint32_t miner_cut,
                            const std::optional<asset> ingress_bridge_fee,
                            const bool also_prepare_self_balance,
                            const std::optional<name> token_contract)
{
   mvo fee_params;
   fee_params("gas_price", gas_price)("miner_cut", miner_cut);

   if (ingress_bridge_fee.has_value()) {
      fee_params("ingress_bridge_fee", *ingress_bridge_fee);
   } else {
      fee_params("ingress_bridge_fee", "0.0000 EOS");
   }

   if(!token_contract.has_value()) {
      push_action(evm_account_name, "init"_n, evm_account_name, mvo()("chainid", chainid)("fee_params", fee_params));
   } else {
      push_action(evm_account_name, "init"_n, evm_account_name, mvo()("chainid", chainid)("fee_params", fee_params)("token_contract", token_contract));
   }


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

statistics basic_evm_tester::get_statistics() const
{
   static constexpr eosio::chain::name statistics_singleton_name = "statistics"_n;
   const vector<char> d =
      get_row_by_account(evm_account_name, evm_account_name, statistics_singleton_name, statistics_singleton_name);
   return fc::raw::unpack<statistics>(d);
}

config2_table_row basic_evm_tester::get_config2() const
{
   static constexpr eosio::chain::name config2_singleton_name = "config2"_n;
   const vector<char> d =
      get_row_by_account(evm_account_name, evm_account_name, config2_singleton_name, config2_singleton_name);
   return fc::raw::unpack<config2_table_row>(d);
}

gcstore basic_evm_tester::get_gcstore(uint64_t id) const
{
   const vector<char> d = get_row_by_account(evm_account_name, evm_account_name, "gcstore"_n, name{id});
   FC_ASSERT(d.size(), "not found");
   return fc::raw::unpack<gcstore>(d);
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

uint64_t basic_evm_tester::get_gas_price() const {
   const auto cfg = get_config();
   BOOST_REQUIRE(cfg.evm_version.has_value() == true);
   BOOST_REQUIRE(cfg.gas_prices.has_value() == true);
   return cfg.evm_version.value().cached_version < 3 ? cfg.gas_price : cfg.gas_prices->get_storage_price();
}

silkworm::Transaction
basic_evm_tester::generate_tx(const evmc::address& to, const intx::uint256& value, uint64_t gas_limit) const
{
   const auto gas_price = get_gas_price();

   return silkworm::Transaction{
      silkworm::UnsignedTransaction {
         .type = silkworm::TransactionType::kLegacy,
         .max_priority_fee_per_gas = gas_price,
         .max_fee_per_gas = gas_price,
         .gas_limit = gas_limit,
         .to = to,
         .value = value,
      }
   };
}
transaction_trace_ptr basic_evm_tester::exec(const exec_input& input, const std::optional<exec_callback>& callback) {
   auto binary_data = fc::raw::pack<exec_input, std::optional<exec_callback>>(input, callback);
   return basic_evm_tester::push_action(evm_account_name, "exec"_n, evm_account_name, bytes{binary_data.begin(), binary_data.end()});
}

transaction_trace_ptr basic_evm_tester::call(name from, const evmc::bytes& to, const evmc::bytes& value, evmc::bytes& data, uint64_t gas_limit, name actor)
{
   bytes to_bytes;
   to_bytes.resize(to.size());
   memcpy(to_bytes.data(), to.data(), to.size());
   
   bytes data_bytes;
   data_bytes.resize(data.size());
   memcpy(data_bytes.data(), data.data(), data.size());

   bytes value_bytes;
   value_bytes.resize(value.size());
   memcpy(value_bytes.data(), value.data(), value.size());

   return push_action(evm_account_name, "call"_n, actor,  mvo()("from", from)("to", to_bytes)("value", value_bytes)("data", data_bytes)("gas_limit", gas_limit));
}

transaction_trace_ptr basic_evm_tester::callotherpay(name payer, name from, const evmc::bytes& to, const evmc::bytes& value, evmc::bytes& data, uint64_t gas_limit, name actor)
{
   bytes to_bytes;
   to_bytes.resize(to.size());
   memcpy(to_bytes.data(), to.data(), to.size());
   
   bytes data_bytes;
   data_bytes.resize(data.size());
   memcpy(data_bytes.data(), data.data(), data.size());

   bytes value_bytes;
   value_bytes.resize(value.size());
   memcpy(value_bytes.data(), value.data(), value.size());

   return push_action2(evm_account_name, "callotherpay"_n, actor, payer, mvo()("payer", payer)("from", from)("to", to_bytes)("value", value_bytes)("data", data_bytes)("gas_limit", gas_limit));
}

transaction_trace_ptr basic_evm_tester::admincall(const evmc::bytes& from, const evmc::bytes& to, const evmc::bytes& value, evmc::bytes& data, uint64_t gas_limit, name actor)
{
   bytes to_bytes;
   to_bytes.resize(to.size());
   memcpy(to_bytes.data(), to.data(), to.size());

   bytes data_bytes;
   data_bytes.resize(data.size());
   memcpy(data_bytes.data(), data.data(), data.size());

   bytes from_bytes;
   from_bytes.resize(from.size());
   memcpy(from_bytes.data(), from.data(), from.size());

   bytes value_bytes;
   value_bytes.resize(value.size());
   memcpy(value_bytes.data(), value.data(), value.size());

   return push_action(evm_account_name, "admincall"_n, actor,  mvo()("from", from_bytes)("to", to_bytes)("value", value_bytes)("data", data_bytes)("gas_limit", gas_limit));
}

transaction_trace_ptr basic_evm_tester::bridgereg(name receiver, name handler, asset min_fee, vector<account_name> extra_signers) {
   extra_signers.push_back(receiver);
   if (receiver != handler)
      extra_signers.push_back(handler);
   return basic_evm_tester::push_action(evm_account_name, "bridgereg"_n, extra_signers, 
      mvo()("receiver", receiver)("handler", handler)("min_fee", min_fee));
}

transaction_trace_ptr basic_evm_tester::bridgeunreg(name receiver) {
   return basic_evm_tester::push_action(evm_account_name, "bridgeunreg"_n, receiver, 
      mvo()("receiver", receiver));
}

transaction_trace_ptr basic_evm_tester::assertnonce(name account, uint64_t next_nonce) {
   return basic_evm_tester::push_action(evm_account_name, "assertnonce"_n, account, 
      mvo()("account", account)("next_nonce", next_nonce));
}

transaction_trace_ptr basic_evm_tester::pushtx(const silkworm::Transaction& trx, name miner, std::optional<uint64_t> min_inclusion_price)
{
   silkworm::Bytes rlp;
   silkworm::rlp::encode(rlp, trx, false);

   bytes rlp_bytes;
   rlp_bytes.resize(rlp.size());
   memcpy(rlp_bytes.data(), rlp.data(), rlp.size());

   if (min_inclusion_price.has_value()) {
      return push_action(evm_account_name, "pushtx"_n, miner, mvo()("miner", miner)("rlptx", rlp_bytes)("min_inclusion_price", min_inclusion_price));
   } else {
      return push_action(evm_account_name, "pushtx"_n, miner, mvo()("miner", miner)("rlptx", rlp_bytes));
   }
}

transaction_trace_ptr basic_evm_tester::setversion(uint64_t version, name actor) {
   return basic_evm_tester::push_action(evm_account_name, "setversion"_n, actor,
      mvo()("version", version));
}

transaction_trace_ptr basic_evm_tester::updtgasparam(asset ram_price_mb, uint64_t gas_price, name actor) {
   return basic_evm_tester::push_action(evm_account_name, "updtgasparam"_n, actor,
      mvo()("ram_price_mb", ram_price_mb)("gas_price", gas_price));
}

transaction_trace_ptr basic_evm_tester::setgasparam(uint64_t gas_txnewaccount, 
                                uint64_t gas_newaccount, 
                                uint64_t gas_txcreate, 
                                uint64_t gas_codedeposit, 
                                uint64_t gas_sset, name actor) {
   return basic_evm_tester::push_action(evm_account_name, "setgasparam"_n, actor,
      mvo()("gas_txnewaccount", gas_txnewaccount)
           ("gas_newaccount", gas_newaccount)
           ("gas_txcreate", gas_txcreate)
           ("gas_codedeposit", gas_codedeposit)
           ("gas_sset", gas_sset));
}

transaction_trace_ptr basic_evm_tester::rmgcstore(uint64_t id, name actor) {
   return basic_evm_tester::push_action(evm_account_name, "rmgcstore"_n, actor,
      mvo()("id", id));
}

transaction_trace_ptr basic_evm_tester::setkvstore(uint64_t account_id, const bytes& key, const std::optional<bytes>& value, name actor) {
   return basic_evm_tester::push_action(evm_account_name, "setkvstore"_n, actor,
      mvo()("account_id", account_id)("key", key)("value", value));
}

transaction_trace_ptr basic_evm_tester::rmaccount(uint64_t id, name actor) {
   return basic_evm_tester::push_action(evm_account_name, "rmaccount"_n, actor,
      mvo()("id", id));
}

transaction_trace_ptr basic_evm_tester::freezeaccnt(uint64_t id, bool value, name actor) {
   return basic_evm_tester::push_action(evm_account_name, "freezeaccnt"_n, actor,
      mvo()("id", id)("value",value));
}

transaction_trace_ptr basic_evm_tester::addevmbal(uint64_t id, const intx::uint256& delta, bool subtract, name actor) {
   auto d = to_bytes(delta);
   return basic_evm_tester::push_action(evm_account_name, "addevmbal"_n, actor,
      mvo()("id", id)("delta",d)("subtract",subtract));
}

transaction_trace_ptr basic_evm_tester::addopenbal(name account, const intx::uint256& delta, bool subtract, name actor) {
   auto d = to_bytes(delta);
   return basic_evm_tester::push_action(evm_account_name, "addopenbal"_n, actor,
      mvo()("account", account)("delta",d)("subtract",subtract));
}

transaction_trace_ptr basic_evm_tester::setgasprices(const gas_prices_type& prices, name actor) {
   return basic_evm_tester::push_action(evm_account_name, "setgasprices"_n, actor,
      mvo()("prices", prices));
}

evmc::address basic_evm_tester::deploy_contract(evm_eoa& eoa, evmc::bytes bytecode)
{
   uint64_t nonce = eoa.next_nonce;
   const auto gas_price = get_gas_price();

   silkworm::Transaction tx{
      silkworm::UnsignedTransaction {
         .type = silkworm::TransactionType::kLegacy,
         .max_priority_fee_per_gas = gas_price,
         .max_fee_per_gas = gas_price,
         .gas_limit = 10'000'000,
         .data = std::move(bytecode),
      }
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

balance_and_dust basic_evm_tester::inevm() const {
   return fc::raw::unpack<balance_and_dust>(get_row_by_account(evm_account_name, evm_account_name, "inevm"_n, "inevm"_n));
}

void basic_evm_tester::gc(uint32_t max) {
   push_action(evm_account_name, "gc"_n, evm_account_name, mvo()("max", max));
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
      .code_id = row.code_id,
      .flags = row.flags
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

std::optional<account_object> basic_evm_tester::find_account_by_id(uint64_t id) const
{
   static constexpr eosio::chain::name account_table_name = "account"_n;
   const vector<char> d =
      get_row_by_account(evm_account_name, evm_account_name, account_table_name, name{id});
   if(d.empty()) return {};

   partial_account_table_row row;
   fc::datastream<const char*> ds(d.data(), d.size());
   fc::raw::unpack(ds, row);

   return convert_to_account_object(row);
}

intx::uint128 basic_evm_tester::tx_data_cost(const silkworm::Transaction& txn) const {
    intx::uint128 gas{0};

    const intx::uint128 data_len{txn.data.length()};
    if (data_len == 0) {
        return gas;
    }

    const intx::uint128 non_zero_bytes{as_range::count_if(txn.data, [](uint8_t c) { return c != 0; })};
    const intx::uint128 nonZeroGas{silkworm::protocol::fee::kGTxDataNonZeroIstanbul};
    gas += non_zero_bytes * nonZeroGas;
    const intx::uint128 zero_bytes{data_len - non_zero_bytes};
    gas += zero_bytes * silkworm::protocol::fee::kGTxDataZero;

    return gas;
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

void basic_evm_tester::scan_balances(std::function<bool(vault_balance_row)> visitor) const {
   static constexpr eosio::chain::name balances_table_name = "balances"_n;
   scan_table<vault_balance_row>(
      balances_table_name, evm_account_name, [this, &visitor](vault_balance_row&& row) {
         return visitor(row);
      }
   );
}

bool basic_evm_tester::scan_gcstore(std::function<bool(gcstore)> visitor) const
{
   static constexpr eosio::chain::name gcstore_table_name = "gcstore"_n;

   scan_table<gcstore>(
      gcstore_table_name, evm_account_name, [&visitor](gcstore&& row) { return visitor(row); }
   );

   return true;
}

bool basic_evm_tester::scan_account_code(std::function<bool(account_code)> visitor) const
{
   static constexpr eosio::chain::name account_code_table_name = "accountcode"_n;

   scan_table<account_code>(
      account_code_table_name, evm_account_name, [&visitor](account_code&& row) { return visitor(row); }
   );

   return true;
}

bool basic_evm_tester::scan_price_queue(std::function<bool(price_queue)> visitor) const
{
   static constexpr eosio::chain::name price_queue_table_name = "pricequeue"_n;

   scan_table<price_queue>(
      price_queue_table_name, evm_account_name, [&visitor](price_queue&& row) { return visitor(row); }
   );

   return true;
}

bool basic_evm_tester::scan_prices_queue(std::function<bool(prices_queue)> visitor) const
{
   static constexpr eosio::chain::name prices_queue_table_name = "pricesqueue"_n;

   scan_table<prices_queue>(
      prices_queue_table_name, evm_account_name, [&visitor](prices_queue&& row) { return visitor(row); }
   );

   return true;
}

asset basic_evm_tester::get_eos_balance( const account_name& act ) {
   vector<char> data = get_row_by_account( "eosio.token"_n, act, "accounts"_n, name(native_symbol.to_symbol_code().value) );
   return data.empty() ? asset(0, native_symbol) : fc::raw::unpack<asset>(data);
}

asset basic_evm_tester::get_A_balance( const account_name& act ) {
   vector<char> data = get_row_by_account( vaulta_account_name, act, "accounts"_n, name(new_gas_symbol.to_symbol_code().value) );
   return data.empty() ? asset(0, new_gas_symbol) : fc::raw::unpack<asset>(data);
}

void basic_evm_tester::check_balances() {
   intx::uint256 total_in_evm_accounts;
   scan_accounts([&](evm_test::account_object&& account) -> bool {
      total_in_evm_accounts += account.balance;
      return false;
   });

   auto in_evm = intx::uint256(inevm());
   if(total_in_evm_accounts != in_evm) {
      dlog("inevm: ${inevm}, total_in_evm_accounts: ${total_in_evm_accounts}",("total_in_evm_accounts",intx::to_string(total_in_evm_accounts))("inevm",intx::to_string(in_evm)));
      throw std::runtime_error("total_in_evm_accounts != in_evm");
   }

   intx::uint256 total_in_accounts;
   scan_balances([&](vault_balance_row&& row) -> bool {
      total_in_accounts += intx::uint256(balance_and_dust{.balance=row.balance, .dust=row.dust});
      return false;
   });

   asset evm_eos_bal_ = get_eos_balance(evm_account_name);
   asset evm_A_bal_ = get_A_balance(evm_account_name);

   auto evm_eos_balance = intx::uint256(balance_and_dust{.balance=asset(evm_eos_bal_.get_amount() + evm_A_bal_.get_amount(), native_symbol), .dust=0});
   if(evm_eos_balance != total_in_accounts+total_in_evm_accounts) {
      dlog("evm_eos_balance: ${evm_eos_balance}, total_in_accounts+total_in_evm_accounts: ${tt}",("tt",intx::to_string(total_in_accounts+total_in_evm_accounts))("evm_eos_balance",intx::to_string(evm_eos_balance)));
      throw std::runtime_error("evm_eos_balance != total_in_accounts+total_in_evm_accounts");
   }
}

silkworm::Transaction basic_evm_tester::get_tx_from_trace(const bytes& v) {
   auto evmtx = get_event_from_trace<evm_test::evmtx_v1>(v);
   BOOST_REQUIRE(evmtx.eos_evm_version == 1);

   silkworm::Transaction tx;
   silkworm::ByteView bv{(const uint8_t*)evmtx.rlptx.data(), evmtx.rlptx.size()};
   silkworm::rlp::decode(bv, tx);
   BOOST_REQUIRE(bv.empty());

   return tx;
};


} // namespace evm_test
