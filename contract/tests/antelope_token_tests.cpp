#include "basic_evm_tester.hpp"

class antelope_token_tester : public basic_evm_tester {
public:
   antelope_token_tester() {
      create_accounts({"alice"_n, "bob"_n, "carol"_n});
      produce_block();

      const auto& accnt = control->db().get<account_object,by_name>("evm"_n);
      abi_def abi;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      abi_ser.set_abi(abi, abi_serializer::create_yield_function(abi_serializer_max_time));
   }

   action_result push_action(const account_name& signer, const action_name &name, const variant_object &data) {
      string action_type_name = abi_ser.get_action_type(name);

      action act;
      act.account = "evm"_n;
      act.name    = name;
      act.data    = abi_ser.variant_to_binary(action_type_name, data, abi_serializer::create_yield_function(abi_serializer_max_time));

      return base_tester::push_action(std::move(act), signer.to_uint64_t());
   }

   fc::variant get_stats(const string& symbolname) {
      auto symb = eosio::chain::symbol::from_string(symbolname);
      auto symbol_code = symb.to_symbol_code().value;
      vector<char> data = get_row_by_account("evm"_n, name(symbol_code), "stat"_n, account_name(symbol_code));
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant("currency_stats", data, abi_serializer::create_yield_function(abi_serializer_max_time));
   }

   fc::variant get_account(account_name acc, const string& symbolname) {
      auto symb = eosio::chain::symbol::from_string(symbolname);
      auto symbol_code = symb.to_symbol_code().value;
      vector<char> data = get_row_by_account("evm"_n, acc, "accounts"_n, account_name(symbol_code));
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant("account", data, abi_serializer::create_yield_function(abi_serializer_max_time));
   }

   action_result issue(account_name issuer, asset quantity, string memo) {
      return push_action(issuer, "issue"_n, mvo()
           ("to", issuer)
           ("quantity", quantity)
           ("memo", memo)
      );
   }

   action_result retire(account_name issuer, asset quantity, string memo) {
      return push_action(issuer, "retire"_n, mvo()
           ("quantity", quantity)
           ("memo", memo)
      );
   }

   action_result transfer(account_name from,
                  account_name to,
                  asset        quantity,
                  string       memo) {
      return push_action(from, "transfer"_n, mvo()
           ("from", from)
           ("to", to)
           ("quantity", quantity)
           ("memo", memo)
      );
   }

   action_result open(account_name owner,
                       const string& symbolname,
                       account_name ram_payer    ) {
      return push_action(ram_payer, "open"_n, mvo()
           ("owner", owner)
           ("symbol", symbolname)
           ("ram_payer", ram_payer)
      );
   }

   action_result close(account_name owner,
                        const string& symbolname) {
      return push_action(owner, "close"_n, mvo()
           ("owner", owner)
           ("symbol", symbolname)
      );
   }

   abi_serializer abi_ser;
};

BOOST_AUTO_TEST_SUITE(antelope_token_tests)

BOOST_FIXTURE_TEST_CASE(create_tests, antelope_token_tester) try {
   init(15555, symbol(4, "EVM"));

   auto stats = get_stats("4,EVM");
   REQUIRE_MATCHING_OBJECT(stats, mvo()
      ("supply", "0.0000 EVM")
      ("max_supply", "461168601842738.7903 EVM")
      ("issuer", "evm")
   );
   produce_block();

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(issue_tests, antelope_token_tester) try {
   init(15555, symbol(3, "EVM"));
   produce_block();

   issue("evm"_n, asset::from_string("500.000 EVM"), "hola");

   auto stats = get_stats("3,EVM");
   REQUIRE_MATCHING_OBJECT(stats, mvo()
      ("supply", "500.000 EVM")
      ("max_supply", "4611686018427387.903 EVM")
      ("issuer", "evm")
   );

   auto evm_balance = get_account("evm"_n, "3,EVM");
   REQUIRE_MATCHING_OBJECT(evm_balance, mvo()
      ("balance", "500.000 EVM")
      ("dust", 0)
   );

   BOOST_REQUIRE_EQUAL(wasm_assert_msg("quantity exceeds available supply"),
                       issue("evm"_n, asset::from_string("4611686018427387.000 EVM"), "hola")
   );

   BOOST_REQUIRE_EQUAL(wasm_assert_msg("must issue positive quantity"),
                       issue("evm"_n, asset::from_string("-1.000 EVM"), "hola")
   );

   BOOST_REQUIRE_EQUAL(success(),
                       issue("evm"_n, asset::from_string("1.000 EVM"), "hola")
   );

   BOOST_REQUIRE_EQUAL(wasm_assert_msg("symbol precision mismatch"),
                       issue("evm"_n, asset::from_string("1 EVM"), "hola")
   );

   BOOST_REQUIRE_EQUAL(wasm_assert_msg("requested token symbol to issue does not match symbol from init action"),
                       issue("evm"_n, asset::from_string("1 SPOON"), "hola")
   );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(retire_tests, antelope_token_tester) try {
   init(15555, symbol(3, "EVM"));
   produce_block();

   BOOST_REQUIRE_EQUAL(success(), issue("evm"_n, asset::from_string("500.000 EVM"), "hola"));

   auto stats = get_stats("3,EVM");
   REQUIRE_MATCHING_OBJECT(stats, mvo()
      ("supply", "500.000 EVM")
      ("max_supply", "4611686018427387.903 EVM")
      ("issuer", "evm")
   );

   auto evm_balance = get_account("evm"_n, "3,EVM");
   REQUIRE_MATCHING_OBJECT(evm_balance, mvo()
      ("balance", "500.000 EVM")
      ("dust", 0)
   );

   BOOST_REQUIRE_EQUAL(success(), retire("evm"_n, asset::from_string("200.000 EVM"), "hola"));
   stats = get_stats("3,EVM");
   REQUIRE_MATCHING_OBJECT(stats, mvo()
      ("supply", "300.000 EVM")
      ("max_supply", "4611686018427387.903 EVM")
      ("issuer", "evm")
   );
   evm_balance = get_account("evm"_n, "3,EVM");
   REQUIRE_MATCHING_OBJECT(evm_balance, mvo()
      ("balance", "300.000 EVM")
      ("dust", 0)
   );

   //should fail to retire more than current supply
   BOOST_REQUIRE_EQUAL(wasm_assert_msg("overdrawn balance"), retire("evm"_n, asset::from_string("500.000 EVM"), "hola"));

   //account must already be open before transfer
   BOOST_REQUIRE_EQUAL(wasm_assert_msg("account transfering to must already be open"), transfer("evm"_n, "bob"_n, asset::from_string("200.000 EVM"), "hola"));
   BOOST_REQUIRE_EQUAL(success(), open("bob"_n, "3,EVM", "evm"_n));
   BOOST_REQUIRE_EQUAL(success(), transfer("evm"_n, "bob"_n, asset::from_string("200.000 EVM"), "hola"));

   //should fail to retire since tokens are not on the issuer's balance
   BOOST_REQUIRE_EQUAL(wasm_assert_msg("overdrawn balance"), retire("evm"_n, asset::from_string("300.000 EVM"), "hola"));
   //transfer tokens back
   BOOST_REQUIRE_EQUAL(success(), transfer("bob"_n, "evm"_n, asset::from_string("200.000 EVM"), "hola"));

   BOOST_REQUIRE_EQUAL(success(), retire("evm"_n, asset::from_string("300.000 EVM"), "hola"));
   stats = get_stats("3,EVM");
   REQUIRE_MATCHING_OBJECT(stats, mvo()
      ("supply", "0.000 EVM")
      ("max_supply", "4611686018427387.903 EVM")
      ("issuer", "evm")
   );
   evm_balance = get_account("evm"_n, "3,EVM");
   REQUIRE_MATCHING_OBJECT(evm_balance, mvo()
      ("balance", "0.000 EVM")
      ("dust", 0)
   );

   //trying to retire tokens with zero supply
   BOOST_REQUIRE_EQUAL(wasm_assert_msg("overdrawn balance"), retire("evm"_n, asset::from_string("1.000 EVM"), "hola"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(transfer_tests, antelope_token_tester) try {
   init(15555, symbol(0, "EVM"));
   produce_block();

   issue("evm"_n, asset::from_string("1000 EVM"), "hola");

   auto stats = get_stats("0,EVM");
   REQUIRE_MATCHING_OBJECT(stats, mvo()
      ("supply", "1000 EVM")
      ("max_supply", "4611686018427387903 EVM")
      ("issuer", "evm")
   );

   auto evm_balance = get_account("evm"_n, "0,EVM");
   REQUIRE_MATCHING_OBJECT(evm_balance, mvo()
      ("balance", "1000 EVM")
      ("dust", 0)
   );

   open("bob"_n, "0,EVM", "bob"_n);
   transfer("evm"_n, "bob"_n, asset::from_string("300 EVM"), "hola");

   evm_balance = get_account("evm"_n, "0,EVM");
   REQUIRE_MATCHING_OBJECT(evm_balance, mvo()
      ("balance", "700 EVM")
      ("dust", 0)
   );

   auto bob_balance = get_account("bob"_n, "0,EVM");
   REQUIRE_MATCHING_OBJECT(bob_balance, mvo()
      ("balance", "300 EVM")
      ("dust", 0)
   );

   BOOST_REQUIRE_EQUAL(wasm_assert_msg("overdrawn balance"),
      transfer("evm"_n, "bob"_n, asset::from_string("701 EVM"), "hola")
   );

   BOOST_REQUIRE_EQUAL(wasm_assert_msg("must transfer positive quantity"),
      transfer("evm"_n, "bob"_n, asset::from_string("-1000 EVM"), "hola")
   );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(open_tests, antelope_token_tester) try {
   init(15555, symbol(0, "EVM"));
   produce_block();

   auto evm_balance = get_account("evm"_n, "0,EVM");
   BOOST_REQUIRE_EQUAL(true, evm_balance.is_null());
   BOOST_REQUIRE_EQUAL(wasm_assert_msg("tokens can only be issued to issuer account"),
                       issue("bob"_n, asset::from_string("1000 EVM"), "issue"));
   BOOST_REQUIRE_EQUAL(success(), issue("evm"_n, asset::from_string("1000 EVM"), "issue"));

   evm_balance = get_account("evm"_n, "0,EVM");
   REQUIRE_MATCHING_OBJECT(evm_balance, mvo()
      ("balance", "1000 EVM")
      ("dust", 0)
   );

   auto bob_balance = get_account("bob"_n, "0,EVM");
   BOOST_REQUIRE_EQUAL(true, bob_balance.is_null());

   BOOST_REQUIRE_EQUAL(wasm_assert_msg("owner account does not exist"),
                       open("nonexistent"_n, "0,EVM", "evm"_n));
   BOOST_REQUIRE_EQUAL(success(),
                       open("bob"_n,         "0,EVM", "bob"_n));

   bob_balance = get_account("bob"_n, "0,EVM");
   REQUIRE_MATCHING_OBJECT(bob_balance, mvo()
      ("balance", "0 EVM")
      ("dust", 0)
   );

   BOOST_REQUIRE_EQUAL(success(), transfer("evm"_n, "bob"_n, asset::from_string("200 EVM"), "hola"));

   bob_balance = get_account("bob"_n, "0,EVM");
   REQUIRE_MATCHING_OBJECT(bob_balance, mvo()
      ("balance", "200 EVM")
      ("dust", 0)
   );

   BOOST_REQUIRE_EQUAL(wasm_assert_msg("symbol does not exist"),
                       open("carol"_n, "0,INVALID", "carol"_n));

   BOOST_REQUIRE_EQUAL(wasm_assert_msg("symbol precision mismatch"),
                       open("carol"_n, "1,EVM", "carol"_n));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(close_tests, antelope_token_tester) try {
   init(15555, symbol(0, "EVM"));
   produce_block();

   auto evm_balance = get_account("evm"_n, "0,EVM");
   BOOST_REQUIRE_EQUAL(true, evm_balance.is_null());

   BOOST_REQUIRE_EQUAL(success(), issue("evm"_n, asset::from_string("1000 EVM"), "hola"));

   evm_balance = get_account("evm"_n, "0,EVM");
   REQUIRE_MATCHING_OBJECT(evm_balance, mvo()
      ("balance", "1000 EVM")
      ("dust", 0)
   );

   open("bob"_n, "0,EVM", "bob"_n);
   BOOST_REQUIRE_EQUAL(success(), transfer("evm"_n, "bob"_n, asset::from_string("1000 EVM"), "hola"));

   evm_balance = get_account("evm"_n, "0,EVM");
   REQUIRE_MATCHING_OBJECT(evm_balance, mvo()
      ("balance", "0 EVM")
      ("dust", 0)
   );

   BOOST_REQUIRE_EQUAL(success(), close("evm"_n, "0,EVM"));
   evm_balance = get_account("evm"_n, "0,EVM");
   BOOST_REQUIRE_EQUAL(true, evm_balance.is_null());

   BOOST_REQUIRE_EQUAL(wasm_assert_msg("Balance row already deleted or never existed. Action won't have any effect."), close("evm"_n, "0,EVM"));
   BOOST_REQUIRE_EQUAL(wasm_assert_msg("Cannot close because the balance is not zero."), close("bob"_n, "0,EVM"));

   //issue again
   BOOST_REQUIRE_EQUAL(success(), issue("evm"_n, asset::from_string("1000 EVM"), "hola"));

   evm_balance = get_account("evm"_n, "0,EVM");
   REQUIRE_MATCHING_OBJECT(evm_balance, mvo()
      ("balance", "1000 EVM")
      ("dust", 0)
   );

   //issue moar!
   BOOST_REQUIRE_EQUAL(success(), issue("evm"_n, asset::from_string("1000 EVM"), "hola"));

   evm_balance = get_account("evm"_n, "0,EVM");
   REQUIRE_MATCHING_OBJECT(evm_balance, mvo()
      ("balance", "2000 EVM")
      ("dust", 0)
   );

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()