#pragma once

#include "basic_evm_tester.hpp"

#include <fc/io/raw_fwd.hpp>

using namespace eosio::testing;
struct native_token_evm_tester : basic_evm_tester {
   native_token_evm_tester(std::string native_smybol_str, bool doinit) : native_symbol(symbol::from_string(native_smybol_str)) {
      if(doinit)
         init(15555);
      create_accounts({"eosio.token"_n, "alice"_n, "bob"_n, "carol"_n});
      produce_block();

      set_code("eosio.token"_n, contracts::eosio_token_wasm());
      set_abi("eosio.token"_n, contracts::eosio_token_abi().data());

      push_action("eosio.token"_n, "create"_n, "eosio.token"_n, mvo()("issuer", "eosio.token"_n)
                                                                     ("maximum_supply", asset(1'000'000'0000, native_symbol)));
      for(const name& n : {"alice"_n, "bob"_n, "carol"_n})
         push_action("eosio.token"_n, "issue"_n, "eosio.token"_n, mvo()("to", n)
                                                                       ("quantity", asset(100'0000, native_symbol))
                                                                       ("memo", ""));
   }

   transaction_trace_ptr transfer_token(name from, name to, asset quantity, std::string memo) {
      return push_action("eosio.token"_n, "transfer"_n, from, mvo()("from", from)
                                                                   ("to", to)
                                                                   ("quantity", quantity)
                                                                   ("memo", memo));
   }

   int64_t native_balance(name owner) const {
      return get_currency_balance("eosio.token"_n, native_symbol, owner).get_amount();
   }

   std::tuple<asset, uint64_t> vault_balance(name owner) const {
      const vector<char> d = get_row_by_account("evm"_n, "evm"_n, "balances"_n, owner);
      FC_ASSERT(d.size(), "EVM not open");
      auto [_, amount, dust] = fc::raw::unpack<vault_balance_row>(d);
      return std::make_tuple(amount, dust);
   }
   int64_t vault_balance_token(name owner) const {
      return std::get<0>(vault_balance(owner)).get_amount();
   }
   uint64_t vault_balance_dust(name owner) const {
      return std::get<1>(vault_balance(owner));
   }

   transaction_trace_ptr open(name owner) {
      return push_action("evm"_n, "open"_n, owner, mvo()("owner", owner));
   }
   transaction_trace_ptr close(name owner) {
      return push_action("evm"_n, "close"_n, owner, mvo()("owner", owner));
   }
   transaction_trace_ptr withdraw(name owner, asset quantity) {
      return push_action("evm"_n, "withdraw"_n, owner, mvo()("owner", owner)("quantity", quantity));
   }

   symbol native_symbol;
   asset make_asset(int64_t amount) {
      return asset(amount, native_symbol);
   }

   struct vault_balance_row {
      name     owner;
      asset    balance;
      uint64_t dust = 0;
   };

   evmc::address make_reserved_address(uint64_t account) const {
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
   }
};
FC_REFLECT(native_token_evm_tester::vault_balance_row, (owner)(balance)(dust))

struct native_token_evm_tester_EOS : native_token_evm_tester {
   native_token_evm_tester_EOS() : native_token_evm_tester("4,EOS", true) {}
};
struct native_token_evm_tester_SPOON : native_token_evm_tester {
   native_token_evm_tester_SPOON() : native_token_evm_tester("4,SPOON", true) {}
};
struct native_token_evm_tester_noinit : native_token_evm_tester {
   native_token_evm_tester_noinit() : native_token_evm_tester("4,EOS", false) {}
};
