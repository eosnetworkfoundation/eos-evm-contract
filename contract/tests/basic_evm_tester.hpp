#pragma once

#include <eosio/chain/abi_serializer.hpp>
#include <eosio/testing/tester.hpp>

#include <fc/variant_object.hpp>

#include <contracts.hpp>

using namespace eosio;
using namespace eosio::chain;
using mvo = fc::mutable_variant_object;

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
};