#pragma once

#include <eosio/eosio.hpp>
#include <intx/intx.hpp>
#include <evmc/evmc.hpp>
#include <ethash/hash_types.hpp>

namespace evm_runtime {

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