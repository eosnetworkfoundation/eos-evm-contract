#pragma once

#include "basic_evm_tester.hpp"

#include <fc/io/raw_fwd.hpp>

using namespace eosio::testing;
using namespace evm_test;
struct native_token_evm_tester : basic_evm_tester {
   enum class init_mode
   {
      do_not_init,
      init_without_ingress_bridge_fee,
      init_with_ingress_bridge_fee,
   };

   native_token_evm_tester(std::string native_symbol_str, init_mode mode, uint64_t ingress_bridge_fee_amount = 0) :
      basic_evm_tester(std::move(native_symbol_str))
   {
      std::vector<name> new_accounts = {"alice"_n, "bob"_n, "carol"_n};

      create_accounts(new_accounts);

      for(const name& recipient : new_accounts) {
         transfer_token(faucet_account_name, recipient, make_asset(100'0000));
      }

      if (mode != init_mode::do_not_init) {
         std::optional<asset> ingress_bridge_fee;
         if (mode == init_mode::init_with_ingress_bridge_fee) {
            ingress_bridge_fee.emplace(make_asset(ingress_bridge_fee_amount));
         }

         init(evm_chain_id,
              suggested_gas_price,
              suggested_miner_cut,
              ingress_bridge_fee,
              mode == init_mode::init_with_ingress_bridge_fee);
      }

      produce_block();
   }

   int64_t native_balance(name owner) const {
      return get_currency_balance(token_account_name, native_symbol, owner).get_amount();
   }

   int64_t vault_balance_token(name owner) const {
      return vault_balance(owner).balance.get_amount();
   }
   uint64_t vault_balance_dust(name owner) const {
      return vault_balance(owner).dust;
   }

   balance_and_dust inevm() const
   {
      return fc::raw::unpack<balance_and_dust>(get_row_by_account("evm"_n, "evm"_n, "inevm"_n, "inevm"_n));
   }
};

struct native_token_evm_tester_EOS : native_token_evm_tester {
   native_token_evm_tester_EOS() : native_token_evm_tester("4,EOS", init_mode::init_with_ingress_bridge_fee) {}
};
struct native_token_evm_tester_SPOON : native_token_evm_tester {
   native_token_evm_tester_SPOON() : native_token_evm_tester("4,SPOON", init_mode::init_without_ingress_bridge_fee) {}
};
struct native_token_evm_tester_noinit : native_token_evm_tester {
   native_token_evm_tester_noinit() : native_token_evm_tester("4,EOS", init_mode::do_not_init) {}
};
