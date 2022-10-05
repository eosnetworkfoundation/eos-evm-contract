#pragma once

#include <cstdint>

#ifndef EOSIO_REFLECT
#define EOSIO_REFLECT(...)
#endif

namespace trust_evm {
   struct [[eosio::table, eosio::contract("evm_contract")]] record {
      int64_t gen_time = 0;
      uint64_t primary_key() const { return 0; }
   };
   EOSIO_REFLECT(record, gen_time);
} // ns trust_evm