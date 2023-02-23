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

#include <secp256k1.h>

#include <contracts.hpp>

using namespace eosio;
using namespace eosio::chain;
using mvo = fc::mutable_variant_object;

using intx::operator""_u256;

class evm_eoa {
public:
   evm_eoa() {
      fc::rand_bytes((char*)private_key.data(), private_key.size());
      public_key.resize(65);

      secp256k1_pubkey pubkey;
      BOOST_REQUIRE(secp256k1_ec_pubkey_create(ctx, &pubkey, private_key.data()));

      size_t serialized_result_sz = public_key.size();
      secp256k1_ec_pubkey_serialize(ctx, public_key.data(), &serialized_result_sz, &pubkey, SECP256K1_EC_UNCOMPRESSED);

      std::optional<evmc::address> addr = silkworm::ecdsa::public_key_to_address(public_key);
      BOOST_REQUIRE(!!addr);
      address = *addr;
   }

   std::string address_0x() const {
      return std::string("0x") + fc::to_hex((char*)address.bytes, sizeof(address.bytes));
   }

   key256_t address_key256() const {
      uint8_t buffer[32]={0};
      memcpy(buffer, address.bytes, sizeof(address.bytes));
      return fixed_bytes<32>(buffer).get_array();
   }

   void sign(silkworm::Transaction& trx) {
      silkworm::Bytes rlp;
      trx.chain_id = 15555; /// TODO: don't hardcode this
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

   ~evm_eoa() {
      secp256k1_context_destroy(ctx);
   }

   evmc::address address;
private:
   secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
   std::array<uint8_t, 32> private_key;
   std::basic_string<uint8_t> public_key;

   unsigned next_nonce = 0;
};

struct balance_and_dust {
   asset balance;
   uint64_t dust = 0;

   bool operator==(const balance_and_dust& o) const {
      return balance == o.balance && dust == o.dust;
   }
   bool operator!=(const balance_and_dust& o) const {
      return !(*this == o);
   }
};
FC_REFLECT(balance_and_dust, (balance)(dust));

class basic_evm_tester : public testing::validating_tester {
public:
   basic_evm_tester() {
      create_accounts({"evm"_n});

      set_code("evm"_n, testing::contracts::evm_runtime_wasm());
      set_abi("evm"_n, testing::contracts::evm_runtime_abi().data());
   }

   void init(const uint64_t chainid) {
      push_action("evm"_n, "init"_n, "evm"_n, mvo()("chainid", chainid));
   }

   void setingressfee(const asset& ingress_bridge_fee) {
      push_action("evm"_n, "setingressfee"_n, "evm"_n, mvo()("ingress_bridge_fee", ingress_bridge_fee));
   }

   void pushtx(const silkworm::Transaction& trx) {
      silkworm::Bytes rlp;
      silkworm::rlp::encode(rlp, trx);

      bytes rlp_bytes;
      rlp_bytes.resize(rlp.size());
      memcpy(rlp_bytes.data(), rlp.data(), rlp.size());

      push_action("evm"_n, "pushtx"_n, "evm"_n, mvo()("ram_payer", "evm"_n)("rlptx", rlp_bytes));
   }

   balance_and_dust inevm() const {
      return fc::raw::unpack<balance_and_dust>(get_row_by_account("evm"_n, "evm"_n, "inevm"_n, "inevm"_n));
   }

   std::optional<intx::uint256> evm_balance(const evm_eoa& account) {
      const int64_t account_table_id = get<table_id_object, by_code_scope_table>(boost::make_tuple("evm"_n, "evm"_n, "account"_n)).id._id;

      const index256_object* secondary_row = find<index256_object, by_secondary>(boost::make_tuple(account_table_id, account.address_key256()));
      if(secondary_row == nullptr)
         return std::nullopt;

      const key_value_object& row = get<key_value_object, by_scope_primary>(boost::make_tuple(account_table_id, secondary_row->primary_key));

      // cheat a little here: uint64_t id, bytes eth_address, uint64_t nonce, bytes balance; 32 bytes of balance must start at byte 38
      return intx::be::unsafe::load<intx::uint256>((uint8_t*)row.value.data()+38);
   }

   void addegress(const std::vector<name> accounts) {
      push_action("evm"_n, "addegress"_n, "evm"_n, mvo()("accounts",accounts));
   }

   void removeegress(const std::vector<name> accounts) {
      push_action("evm"_n, "removeegress"_n, "evm"_n, mvo()("accounts",accounts));
   }
};

inline constexpr intx::uint256 operator"" _wei(const char* s) {
    return intx::from_string<intx::uint256>(s);
}

inline constexpr intx::uint256 operator"" _kwei(const char* s) {
    return intx::from_string<intx::uint256>(s) * intx::exp(10_u256, 3_u256);
}

inline constexpr intx::uint256 operator"" _mwei(const char* s) {
    return intx::from_string<intx::uint256>(s) * intx::exp(10_u256, 6_u256);
}

inline constexpr intx::uint256 operator"" _gwei(const char* s) {
    return intx::from_string<intx::uint256>(s) * intx::exp(10_u256, 9_u256);
}

inline constexpr intx::uint256 operator"" _szabo(const char* s) {
    return intx::from_string<intx::uint256>(s) * intx::exp(10_u256, 12_u256);
}

inline constexpr intx::uint256 operator"" _finney(const char* s) {
    return intx::from_string<intx::uint256>(s) * intx::exp(10_u256, 15_u256);
}

inline constexpr intx::uint256 operator"" _ether(const char* s) {
    return intx::from_string<intx::uint256>(s) * intx::exp(10_u256, 18_u256);
}