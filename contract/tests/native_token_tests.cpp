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
   int64_t vault_balance_dust(name owner) const {
      return std::get<1>(vault_balance(owner));
   }

   transaction_trace_ptr open(name owner, name ram_payer) {
      return push_action("evm"_n, "open"_n, ram_payer, mvo()("owner", owner)("ram_payer", ram_payer));
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

BOOST_AUTO_TEST_SUITE(native_token_evm_tests)

BOOST_FIXTURE_TEST_CASE(basic_deposit_withdraw, native_token_evm_tester_EOS) try {
   //can't transfer to alice's balance because it isn't open
   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), "alice"),
                           eosio_assert_message_exception, eosio_assert_message_is("receiving account has not been opened"));


   open("alice"_n, "alice"_n);

   //alice sends her own tokens in to her EVM balance
   {
      const int64_t to_transfer = 1'0000;
      const int64_t alice_native_before = native_balance("alice"_n);
      const int64_t alice_evm_before = vault_balance_token("alice"_n);
      transfer_token("alice"_n, "evm"_n, make_asset(to_transfer), "alice");

      BOOST_REQUIRE_EQUAL(alice_native_before - native_balance("alice"_n), to_transfer);
      BOOST_REQUIRE_EQUAL(vault_balance_token("alice"_n), to_transfer);
   }

   //bob sends his tokens in to alice's EVM balance
   {
      const int64_t to_transfer = 1'0000;
      const int64_t bob_native_before = native_balance("bob"_n);
      const int64_t alice_evm_before = vault_balance_token("alice"_n);
      transfer_token("bob"_n, "evm"_n, make_asset(to_transfer), "alice");

      BOOST_REQUIRE_EQUAL(bob_native_before - native_balance("bob"_n), to_transfer);
      BOOST_REQUIRE_EQUAL(vault_balance_token("alice"_n) - alice_evm_before, to_transfer);
   }

   //carol can't send tokens to bob's balance because bob isn't open
   BOOST_REQUIRE_EXCEPTION(transfer_token("carol"_n, "evm"_n, make_asset(1'0000), "bob"),
                           eosio_assert_message_exception, eosio_assert_message_is("receiving account has not been opened"));

   //alice can't close her account because of outstanding balance
   BOOST_REQUIRE_EXCEPTION(close("alice"_n),
                           eosio_assert_message_exception, eosio_assert_message_is("cannot close because balance is not zero"));
   //bob can't close either because he never opened   
   BOOST_REQUIRE_EXCEPTION(close("bob"_n),
                           eosio_assert_message_exception, eosio_assert_message_is("account is not open"));

   //withdraw a little bit of Alice's balance
   {
      const int64_t to_withdraw = 5000;
      const int64_t alice_native_before = native_balance("alice"_n);
      const int64_t alice_evm_before = vault_balance_token("alice"_n);
      withdraw("alice"_n, make_asset(to_withdraw));

      BOOST_REQUIRE_EQUAL(native_balance("alice"_n) - alice_native_before, to_withdraw);
      BOOST_REQUIRE_EQUAL(alice_evm_before - vault_balance_token("alice"_n), to_withdraw);
   }

   //try and withdraw more than alice has
   {
      const int64_t to_withdraw = 2'0000;
      BOOST_REQUIRE_GT(to_withdraw, vault_balance_token("alice"_n));
      BOOST_REQUIRE_EXCEPTION(withdraw("alice"_n, make_asset(to_withdraw)),
                              eosio_assert_message_exception, eosio_assert_message_is("overdrawn balance"));
   }

   //bob can't withdraw anything, since he isn't opened
   {
      const int64_t to_withdraw = 2'0000;
      BOOST_REQUIRE_EXCEPTION(withdraw("bob"_n, make_asset(to_withdraw)),
                              eosio_assert_message_exception, eosio_assert_message_is("account is not open"));
   }

   //withdraw the remaining amount that alice has
   {
      const int64_t to_withdraw = 1'5000;
      const int64_t alice_native_before = native_balance("alice"_n);
      const int64_t alice_evm_before = vault_balance_token("alice"_n);
      withdraw("alice"_n, make_asset(to_withdraw));

      BOOST_REQUIRE_EQUAL(native_balance("alice"_n) - alice_native_before, to_withdraw);
      BOOST_REQUIRE_EQUAL(alice_evm_before - vault_balance_token("alice"_n), to_withdraw);
   }

   produce_block();

   //now alice can close out
   close("alice"_n);
   BOOST_REQUIRE_EXCEPTION(vault_balance_token("alice"_n),
                           fc::assert_exception, fc_assert_exception_message_is("EVM not open"));

   //make sure alice can't deposit any more   
   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), "alice"),
                           eosio_assert_message_exception, eosio_assert_message_is("receiving account has not been opened"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(weird_names, native_token_evm_tester_EOS) try {
   //just try some weird account names as memos

   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), "BANANA"),
                           eosio_assert_message_exception, eosio_assert_message_is("character is not in allowed character set for names"));

   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), "loooooooooooooooooong"),
                           eosio_assert_message_exception, eosio_assert_message_is("memo must be either 0x EVM address or already opened account name to credit deposit to"));

   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), "   "),
                           eosio_assert_message_exception, eosio_assert_message_is("character is not in allowed character set for names"));

   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), ""),
                           eosio_assert_message_exception, eosio_assert_message_is("memo must be either 0x EVM address or already opened account name to credit deposit to"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(non_existing_account, native_token_evm_tester_EOS) try {
   //can only open for accounts that exist

   BOOST_REQUIRE_EXCEPTION(open("spoon"_n, "alice"_n),
                           eosio_assert_message_exception, eosio_assert_message_is("owner account does not exist"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(non_standard_native_symbol, native_token_evm_tester_SPOON) try {
   //the symbol 4,EOS is fixed as the expected native symbol. try transfering in a different symbol from eosio.token

   open("alice"_n, "alice"_n);

   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), "alice"),
                           eosio_assert_message_exception, eosio_assert_message_is("received unexpected token"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(transfer_notifier_without_init, native_token_evm_tester_noinit) try {
   //make sure to not accept transfer notifications unless contract has been inited

   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), "alice"),
                           eosio_assert_message_exception, eosio_assert_message_is("contract not initialized"));

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()