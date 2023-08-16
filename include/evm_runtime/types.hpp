#pragma once

#include <eosio/eosio.hpp>
#include <eosio/name.hpp>
#include <eosio/symbol.hpp>
#include <intx/intx.hpp>
#include <evmc/evmc.hpp>
#include <ethash/hash_types.hpp>

#define TOKEN_ACCOUNT_NAME "eosio.token"

namespace evm_runtime {
   using intx::operator""_u256;

   constexpr unsigned evm_precision = 18;
   constexpr eosio::name token_account(eosio::name(TOKEN_ACCOUNT_NAME));
   constexpr eosio::symbol token_symbol("EOS", 4u);
   constexpr intx::uint256 minimum_natively_representable = intx::exp(10_u256, intx::uint256(evm_precision - token_symbol.precision()));
   static_assert(evm_precision - token_symbol.precision() <= 14, "dust math may overflow a uint64_t");

   typedef intx::uint<256>         uint256;
   typedef intx::uint<512>         uint512;
   typedef std::vector<char>       bytes;
   typedef evmc::address           address;
   typedef ethash::hash256         hash256;
   typedef evmc::bytes32           bytes32;
   typedef evmc::bytes32           uint256be;

   eosio::checksum256 make_key(bytes data);
   eosio::checksum256 make_key(const evmc::address& addr);
   eosio::checksum256 make_key(const evmc::bytes32& data);

   bytes to_bytes(const uint256& val);
   bytes to_bytes(const evmc::bytes32& val);
   bytes to_bytes(const evmc::address& addr);

   evmc::address to_address(const bytes& addr);
   evmc::bytes32 to_bytes32(const bytes& data);
   uint256 to_uint256(const bytes& value);

   struct exec_input {
      std::optional<bytes> context;
      std::optional<bytes> from;
      bytes                to;
      bytes                data;
      std::optional<bytes> value;

      EOSLIB_SERIALIZE(exec_input, (context)(from)(to)(data)(value));
   };

   struct exec_callback {
      eosio::name contract;
      eosio::name action;

      EOSLIB_SERIALIZE(exec_callback, (contract)(action));
   };

   struct exec_output {
      int32_t              status;
      bytes                data;
      std::optional<bytes> context;

      EOSLIB_SERIALIZE(exec_output, (status)(data)(context));
   };

   struct bridge_message_v0 {
      eosio::name        receiver;
      bytes              sender;
      eosio::time_point  timestamp;
      bytes              value;
      bytes              data;

      EOSLIB_SERIALIZE(bridge_message_v0, (receiver)(sender)(timestamp)(value)(data));
   };

   using bridge_message = std::variant<bridge_message_v0>;

} //namespace evm_runtime

namespace eosio {

template<typename Stream>
inline datastream<Stream>& operator>>(datastream<Stream>& ds, evm_runtime::uint256& v) {
   evm_runtime::bytes buffer(32,0);
   ds.read((char*)buffer.data(), 32);
   v = intx::be::unsafe::load<evm_runtime::uint256>((const uint8_t *)buffer.data());
   return ds;
}

} //namespace eosio