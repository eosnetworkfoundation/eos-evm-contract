#pragma once

#include <eosio/chain/abi_serializer.hpp>
#include <eosio/testing/tester.hpp>

#include <fc/variant_object.hpp>

#include <contracts.hpp>

using namespace eosio;
using namespace eosio::chain;
using mvo = fc::mutable_variant_object;

static const char wasm_level_up_wast[] = R"=====(
(module
 (import "env" "set_wasm_parameters_packed" (func $set_wasm_parameters_packed (param i32 i32)))
 (memory $0 1)
 (export "apply" (func $apply))
 (func $apply (param $0 i64) (param $1 i64) (param $2 i64)
  (call $set_wasm_parameters_packed
   (i32.const 0)
   (i32.const 48)
  )
 )
 ;; this is intended to be the same settings as used by eosio.system's 3.1.1 "high" setting
 (data (i32.const 0)  "\00\00\00\00")     ;; version
 (data (i32.const 4)  "\00\20\00\00")     ;; max_mutable_global_bytes
 (data (i32.const 8)  "\00\20\00\00")     ;; max_table_elements
 (data (i32.const 12) "\00\20\00\00")     ;; max_section_elements
 (data (i32.const 16) "\00\00\10\00")     ;; max_linear_memory_init
 (data (i32.const 20) "\00\20\00\00")     ;; max_func_local_bytes
 (data (i32.const 24) "\00\04\00\00")     ;; max_nested_structures
 (data (i32.const 28) "\00\20\00\00")     ;; max_symbol_bytes
 (data (i32.const 32) "\00\00\40\01")     ;; max_module_bytes
 (data (i32.const 36) "\00\00\40\01")     ;; max_code_bytes
 (data (i32.const 40) "\10\02\00\00")     ;; max_pages
 (data (i32.const 44) "\00\04\00\00")     ;; max_call_depth
)
)=====";

static const char wasm_level_up_abi[] = R"=====(
{
   "version": "eosio::abi/1.2",
   "types": [],
   "structs": [
      {
         "name": "dothedew",
         "base": "",
         "fields": []
      }
   ],
   "actions": [
      {
         "name": "dothedew",
         "type": "dothedew"
      }
   ]
}
)=====";

class basic_evm_tester : public testing::validating_tester {
public:
   basic_evm_tester() {
      create_accounts({"wasmlevelup"_n, "evm"_n});
      push_action(config::system_account_name, "setpriv"_n, config::system_account_name, mvo()("account", "wasmlevelup"_n)("is_priv", 1));
      produce_blocks(2);

      set_code("wasmlevelup"_n, wasm_level_up_wast);
      set_abi("wasmlevelup"_n, wasm_level_up_abi);
      push_action("wasmlevelup"_n, "dothedew"_n, "wasmlevelup"_n, mvo());
      produce_blocks(2);

      set_code("evm"_n, testing::contracts::evm_runtime_wasm());
      set_abi("evm"_n, testing::contracts::evm_runtime_abi().data());
   }

   void init(const uint64_t chainid) {
      push_action("evm"_n, "init"_n, "evm"_n, mvo()("chainid", chainid));
   }
};