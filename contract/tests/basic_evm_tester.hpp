#pragma once

#include <eosio/chain/abi_serializer.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/fixed_bytes.hpp>

#include <fc/variant_object.hpp>
#include <fc/crypto/rand.hpp>
#include <fc/crypto/hex.hpp>

#include <silkworm/crypto/ecdsa.hpp>
#include <silkworm/types/transaction.hpp>
#include <silkworm/rlp/encode.hpp>
#include <silkworm/common/util.hpp>
#include <silkworm/execution/address.hpp>

#include <secp256k1.h>

#include <silkworm/common/util.hpp>

#include <contracts.hpp>

using namespace eosio;
using namespace eosio::chain;
using mvo = fc::mutable_variant_object;

using intx::operator""_u256;

namespace intx {

inline std::ostream& operator<<(std::ostream& ds, const intx::uint256& num)
{
   ds << intx::to_string(num, 10);
   return ds;
}

} // namespace intx

namespace fc {

void to_variant(const intx::uint256& o, fc::variant& v);
void to_variant(const evmc::address& o, fc::variant& v);

} // namespace fc

namespace evm_test {

struct config_table_row
{
   unsigned_int version;
   uint64_t chainid;
   time_point_sec genesis_time;
   asset ingress_bridge_fee;
   uint64_t gas_price;
   uint32_t miner_cut;
   uint32_t status;
};

struct balance_and_dust
{
   asset balance;
   uint64_t dust = 0;

   explicit operator intx::uint256() const;

   bool operator==(const balance_and_dust&) const;
   bool operator!=(const balance_and_dust&) const;
};

struct account_object
{
   uint64_t id;
   evmc::address address;
   uint64_t nonce;
   intx::uint256 balance;
};

struct storage_slot
{
   uint64_t id;
   intx::uint256 key;
   intx::uint256 value;
};


struct fee_parameters
{
   std::optional<uint64_t> gas_price;
   std::optional<uint32_t> miner_cut;
   std::optional<asset> ingress_bridge_fee;
};

} // namespace evm_test


FC_REFLECT(evm_test::config_table_row,
           (version)(chainid)(genesis_time)(ingress_bridge_fee)(gas_price)(miner_cut)(status))
FC_REFLECT(evm_test::balance_and_dust, (balance)(dust));
FC_REFLECT(evm_test::account_object, (id)(address)(nonce)(balance))
FC_REFLECT(evm_test::storage_slot, (id)(key)(value))
FC_REFLECT(evm_test::fee_parameters, (gas_price)(miner_cut)(ingress_bridge_fee))

namespace evm_test {
class evm_eoa
{
public:
   explicit evm_eoa(std::basic_string<uint8_t> optional_private_key = {});

   std::string address_0x() const;

   key256_t address_key256() const;

   void sign(silkworm::Transaction& trx);

   ~evm_eoa();

   evmc::address address;
   uint64_t next_nonce = 0;

private:
   secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
   std::array<uint8_t, 32> private_key;
   std::basic_string<uint8_t> public_key;
};

class basic_evm_tester : public testing::validating_tester
{
public:
   static constexpr name token_account_name = "eosio.token"_n;
   static constexpr name faucet_account_name = "faucet"_n;
   static constexpr name evm_account_name = "evm"_n;

   static constexpr uint64_t evm_chain_id = 15555;

   // Sensible values for fee parameters passed into init:
   static constexpr uint64_t suggested_gas_price = 150'000'000'000;    // 150 gwei
   static constexpr uint32_t suggested_miner_cut = 10'000;             // 10%
   static constexpr uint64_t suggested_ingress_bridge_fee_amount = 70; // 0.0070 EOS

   const symbol native_symbol;

   static evmc::address make_reserved_address(uint64_t account);

   explicit basic_evm_tester(std::string native_symbol_str = "4,EOS");

   asset make_asset(int64_t amount) const;

   transaction_trace_ptr transfer_token(name from, name to, asset quantity, std::string memo = "");

   void init(const uint64_t chainid = evm_chain_id,
             const uint64_t gas_price = suggested_gas_price,
             const uint32_t miner_cut = suggested_miner_cut,
             const std::optional<asset> ingress_bridge_fee = std::nullopt,
             const bool also_prepare_self_balance = true);

   void prepare_self_balance(uint64_t fund_amount = 100'0000);

   config_table_row get_config() const;

   void setfeeparams(const fee_parameters& fee_params);

   silkworm::Transaction
   generate_tx(const evmc::address& to, const intx::uint256& value, uint64_t gas_limit = 21000) const;

   void pushtx(const silkworm::Transaction& trx, name miner = evm_account_name);
   evmc::address deploy_contract(evm_eoa& eoa, evmc::bytes bytecode);

   void addegress(const std::vector<name>& accounts);
   void removeegress(const std::vector<name>& accounts);

   void open(name owner);
   void close(name owner);
   void withdraw(name owner, asset quantity);

   balance_and_dust vault_balance(name owner) const;
   std::optional<intx::uint256> evm_balance(const evmc::address& address) const;
   std::optional<intx::uint256> evm_balance(const evm_eoa& account) const;

   template <typename T, typename Visitor>
   void scan_table(eosio::chain::name table_name, eosio::chain::name scope_name, Visitor&& visitor) const
   {
      const auto& db = control->db();

      const auto* t_id = db.find<chain::table_id_object, chain::by_code_scope_table>(
         boost::make_tuple(evm_account_name, scope_name, table_name));

      if (!t_id) {
         return;
      }

      const auto& idx = db.get_index<chain::key_value_index, chain::by_scope_primary>();

      for (auto itr = idx.lower_bound(boost::make_tuple(t_id->id)); itr != idx.end() && itr->t_id == t_id->id; ++itr) {
         T row{};
         fc::datastream<const char*> ds(itr->value.data(), itr->value.size());
         fc::raw::unpack(ds, row);
         if (visitor(std::move(row))) {
            // Returning true from visitor means the visitor is no longer interested in continuing the scan.
            return;
         }
      }
   }

   bool scan_accounts(std::function<bool(account_object)> visitor) const;
   std::optional<account_object> scan_for_account_by_address(const evmc::address& address) const;
   std::optional<account_object> find_account_by_address(const evmc::address& address) const;
   bool scan_account_storage(uint64_t account_id, std::function<bool(storage_slot)> visitor) const;
};

inline constexpr intx::uint256 operator"" _wei(const char* s) { return intx::from_string<intx::uint256>(s); }

inline constexpr intx::uint256 operator"" _kwei(const char* s)
{
   return intx::from_string<intx::uint256>(s) * intx::exp(10_u256, 3_u256);
}

inline constexpr intx::uint256 operator"" _mwei(const char* s)
{
   return intx::from_string<intx::uint256>(s) * intx::exp(10_u256, 6_u256);
}

inline constexpr intx::uint256 operator"" _gwei(const char* s)
{
   return intx::from_string<intx::uint256>(s) * intx::exp(10_u256, 9_u256);
}

inline constexpr intx::uint256 operator"" _szabo(const char* s)
{
   return intx::from_string<intx::uint256>(s) * intx::exp(10_u256, 12_u256);
}

inline constexpr intx::uint256 operator"" _finney(const char* s)
{
   return intx::from_string<intx::uint256>(s) * intx::exp(10_u256, 15_u256);
}

inline constexpr intx::uint256 operator"" _ether(const char* s)
{
   return intx::from_string<intx::uint256>(s) * intx::exp(10_u256, 18_u256);
}

template <typename Tester>
class speculative_block_starter
{
public:
   // Assumes user will not abort or finish blocks using the tester passed into the constructor for the lifetime of this
   // object.
   explicit speculative_block_starter(Tester& tester, uint32_t time_gap_sec = 0) : t(tester)
   {
      t.control->start_block(t.control->head_block_time() + fc::milliseconds(500 + 1000 * time_gap_sec), 0);
   }

   speculative_block_starter(speculative_block_starter&& other) : t(other.t) { other.canceled = true; }

   speculative_block_starter(const speculative_block_starter&) = delete;

   ~speculative_block_starter()
   {
      if (!canceled) {
         t.control->abort_block(); // Undo side-effects and go back to state just prior to constructor
      }
   }

   speculative_block_starter& operator=(speculative_block_starter&&) = delete;
   speculative_block_starter& operator=(const speculative_block_starter&) = delete;

   void cancel() { canceled = true; }

private:
   Tester& t;
   bool canceled = false;
};

} // namespace evm_test