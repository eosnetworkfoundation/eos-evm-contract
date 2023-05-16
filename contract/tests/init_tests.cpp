#include "basic_evm_tester.hpp"

using namespace evm_test;

BOOST_AUTO_TEST_SUITE(evm_init_tests)
BOOST_FIXTURE_TEST_CASE(check_init, basic_evm_tester) try {
   //all of these should fail since the contract hasn't been init'd yet

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "pushtx"_n, "evm"_n, mvo()("miner", "evm"_n)("rlptx", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "setfeeparams"_n, "evm"_n, mvo()("fee_params", mvo()("gas_price", 1)("miner_cut", 0)("ingress_bridge_fee", fc::variant()))),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "addegress"_n, "evm"_n, mvo()("accounts", std::vector<name>())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "removeegress"_n, "evm"_n, mvo()("accounts", std::vector<name>())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "open"_n, "evm"_n, mvo()("owner", "evm"_n)("miner", "evm"_n)),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "close"_n, "evm"_n, mvo()("owner", "evm"_n)),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "withdraw"_n, "evm"_n, mvo()("owner", "evm"_n)("quantity", asset())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   // Test of transfer notification w/o init is handled in native_token_evm_tests/transfer_notifier_without_init test as it requires additional setup

   /*
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "testtx"_n, "evm"_n, mvo()("orlptx", std::optional<bytes>())("bi", mvo()("coinbase", bytes())("difficulty", 0)("gasLimit", 0)("number", 0)("timestamp", 0))),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "updatecode"_n, "evm"_n, mvo()("address", bytes())("incarnation", 0)("code_hash", bytes())("code", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "updateaccnt"_n, "evm"_n, mvo()("address", bytes())("initial", bytes())("current", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "updatestore"_n, "evm"_n, mvo()("address", bytes())("incarnation" ,0)("location", bytes())("initial", bytes())("current", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "dumpstorage"_n, "evm"_n, mvo()("addy", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "clearall"_n, "evm"_n, mvo()),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "dumpall"_n, "evm"_n, mvo()),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "setbal"_n, "evm"_n, mvo()("addy", bytes())("bal", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   */
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "gc"_n, "evm"_n, mvo()("max", 42)),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "freeze"_n, "evm"_n, mvo()("value", true)),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});

   BOOST_REQUIRE_EXCEPTION(init(42),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: unknown chainid");});
   init(15555);
   produce_block();
   BOOST_REQUIRE_EXCEPTION(init(15555),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract already initialized");});

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(check_freeze, basic_evm_tester) try {
   init(15555);
   produce_block();

   push_action("evm"_n, "freeze"_n, "evm"_n, mvo()("value", true));

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "pushtx"_n, "evm"_n, mvo()("miner", "evm"_n)("rlptx", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "open"_n, "evm"_n, mvo()("owner", "evm"_n)("miner", "evm"_n)),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "close"_n, "evm"_n, mvo()("owner", "evm"_n)),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "withdraw"_n, "evm"_n, mvo()("owner", "evm"_n)("quantity", asset())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});
   // Test of transfer notification w/o init is handled in native_token_evm_tests/transfer_notifier_without_init test as it requires additional setup

   /*
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "testtx"_n, "evm"_n, mvo()("orlptx", std::optional<bytes>())("bi", mvo()("coinbase", bytes())("difficulty", 0)("gasLimit", 0)("number", 0)("timestamp", 0))),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "updatecode"_n, "evm"_n, mvo()("address", bytes())("incarnation", 0)("code_hash", bytes())("code", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "updateaccnt"_n, "evm"_n, mvo()("address", bytes())("initial", bytes())("current", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "updatestore"_n, "evm"_n, mvo()("address", bytes())("incarnation" ,0)("location", bytes())("initial", bytes())("current", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});

   // dump storage is ok                        
   push_action("evm"_n, "dumpstorage"_n, "evm"_n, mvo()("addy", bytes()));

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "clearall"_n, "evm"_n, mvo()),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});

   // dumpall is ok 
   push_action("evm"_n, "dumpall"_n, "evm"_n, mvo());

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "setbal"_n, "evm"_n, mvo()("addy", bytes())("bal", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});
   */
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "gc"_n, "evm"_n, mvo()("max", 42)),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});
   // unfreeze
   push_action("evm"_n, "freeze"_n, "evm"_n, mvo()("value", false));
   produce_block();

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "pushtx"_n, "evm"_n, mvo()("miner", "evm"_n)("rlptx", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: unable to decode transaction");});

   create_account("spoon"_n);
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "withdraw"_n, "spoon"_n, mvo()("owner", "spoon"_n)("quantity", asset())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: account is not open");});

   push_action("evm"_n, "open"_n, "spoon"_n, mvo()("owner", "spoon"_n));

   push_action("evm"_n, "close"_n, "spoon"_n, mvo()("owner", "spoon"_n));

   // Test of transfer notification w/o init is handled in native_token_evm_tests/transfer_notifier_without_init test as it requires additional setup

   /*
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "testtx"_n, "evm"_n, mvo()("orlptx", std::optional<bytes>())("bi", mvo()("coinbase", bytes())("difficulty", 0)("gasLimit", 0)("number", 0)("timestamp", 0))),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: invalid coinbase");});

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "updatecode"_n, "evm"_n, mvo()("address", bytes())("incarnation", 0)("code_hash", bytes())("code", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: wrong length");});

   push_action("evm"_n, "updateaccnt"_n, "evm"_n, mvo()("address", bytes())("initial", bytes())("current", bytes()));

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "updatestore"_n, "evm"_n, mvo()("address", bytes())("incarnation" ,0)("location", bytes())("initial", bytes())("current", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: wrong length");});

   push_action("evm"_n, "dumpstorage"_n, "evm"_n, mvo()("addy", bytes()));

   push_action("evm"_n, "clearall"_n, "evm"_n, mvo());

   push_action("evm"_n, "dumpall"_n, "evm"_n, mvo());

   push_action("evm"_n, "setbal"_n, "evm"_n, mvo()("addy", bytes())("bal", bytes()));

   */
   push_action("evm"_n, "gc"_n, "evm"_n, mvo()("max", 42));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(withdraw_to, basic_evm_tester) try {
   init(15555);

   // transfer 10 EOS to "evm"_n
   transfer_token(faucet_account_name, "evm"_n, make_asset(10'0000), evm_account_name.to_string());

   // withdraw from "evm" to itself will fails
   BOOST_REQUIRE_EXCEPTION(
      push_action("evm"_n, "withdraw"_n, "evm"_n, mvo()("owner", "evm"_n)("quantity", make_asset(9'0000))),
      eosio_assert_message_exception, [](const eosio_assert_message_exception& e) {
         return testing::expect_assert_message(e, "assertion failure with message: cannot transfer to self");
      });

   // withdraw from "evm" with extra paramter "to" set to itself
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "withdraw"_n, "evm"_n,
                                       mvo()("owner", "evm"_n)("quantity", make_asset(9'0000))("to", "evm")),
                           eosio_assert_message_exception, [](const eosio_assert_message_exception& e) {
                              return testing::expect_assert_message(
                                 e, "assertion failure with message: cannot transfer to self");
                           });

   // withdraw from "evm" with extra paramter "to" set to non exist account
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "withdraw"_n, "evm"_n,
                                       mvo()("owner", "evm"_n)("quantity", make_asset(9'0000))("to", "nonexistacc")),
                           eosio_assert_message_exception, [](const eosio_assert_message_exception& e) {
                              return testing::expect_assert_message(
                                 e, "assertion failure with message: to account does not exist");
                           });

   // withdraw to eosio
   push_action("evm"_n, "withdraw"_n, "evm"_n, mvo()("owner", "evm"_n)("quantity", make_asset(9'0000))("to", "eosio"));

   // cannot withdraw more
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "withdraw"_n, "evm"_n,
                                       mvo()("owner", "evm"_n)("quantity", make_asset(5'0000))("to", "eosio")),
                           eosio_assert_message_exception, [](const eosio_assert_message_exception& e) {
                              return testing::expect_assert_message(
                                 e, "assertion failure with message: overdrawn balance");
                           });

   // test with another account
   create_accounts({"miner1"_n});
   open("miner1"_n);
   transfer_token(faucet_account_name, "evm"_n, make_asset(15'0000), "miner1");

   BOOST_REQUIRE_EXCEPTION(
      push_action("evm"_n, "withdraw"_n, "miner1"_n, mvo()("owner", "miner1"_n)("quantity", make_asset(15'0001))),
      eosio_assert_message_exception, [](const eosio_assert_message_exception& e) {
         return testing::expect_assert_message(e, "assertion failure with message: overdrawn balance");
      });

   // able to withdraw without "to" specified
   push_action("evm"_n, "withdraw"_n, "miner1"_n, mvo()("owner", "miner1"_n)("quantity", make_asset(15'0000)));
}
FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()