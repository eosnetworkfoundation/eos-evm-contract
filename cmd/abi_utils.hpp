#pragma once

#include <eosio/abi.hpp>
#include <abieos.hpp>

#include <stdexcept>
#include <cstdint>
#include <string>
#include <fstream>

#include <silkworm/common/log.hpp>

static inline void verify_variant(eosio::input_stream& bin, const eosio::abi_type& type, const char* expected) {
   uint32_t index;
   eosio::varuint32_from_bin(index, bin);
   auto v = type.as_variant();

   if (!v) {
      SILK_CRIT << type.name << " not a variant";
      throw std::runtime_error("not a variant");
   }
   if (index >= v->size()) {
      SILK_CRIT << "Expected " << expected << " got " << std::to_string(index);
      throw std::runtime_error("not expected");
   }
   if (v->at(index).name != expected) {
      SILK_CRIT << "Expected " << expected << " got " << v->at(index).name;
      throw std::runtime_error("not expected");
   }
   //SILK_DEBUG << "check_variant successful <variant = " << v->at(index).name << ">";
}

static inline const auto& get_type(const std::string& n, abieos::abi& abi) {
   auto it = abi.abi_types.find(n);
   if (it == abi.abi_types.end())
      throw std::runtime_error("unknown ABI type <"+n+">");
   return it->second;
}

template <typename Stream>
static inline abieos::abi load_abi(Stream&& s) {
   eosio::abi_def def = eosio::from_json<eosio::abi_def>(s);
   abieos::abi abi;
   eosio::convert(def, abi);
   return abi;
}

static inline abieos::abi load_abi(std::string abi_dir) {
   std::ifstream abi_file(abi_dir);
   if (!abi_file.is_open()) {
      SILK_CRIT << "ABI file <" << abi_dir << "> not found";
      throw std::runtime_error("Failed to find ABI file");
   }
   std::size_t len = 0;
   abi_file.seekg(0, std::ios::end);
   len = abi_file.tellg();
   abi_file.seekg(0, std::ios::beg);

   char* buffer = new char[len];
   abi_file.read(buffer, len);
   abi_file.close();

   abieos::abi abi = load_abi(eosio::json_token_stream{buffer});
   delete[] buffer;
   return abi;
}