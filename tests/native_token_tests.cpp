#include "native_token_tester.hpp"

static const char do_nothing_wast[] = R"=====(
(module
 (export "apply" (func $apply))
 (func $apply (param $0 i64) (param $1 i64) (param $2 i64)
   ;; nothing
 )
)
)=====";

BOOST_AUTO_TEST_SUITE(native_token_evm_tests)

BOOST_FIXTURE_TEST_CASE(basic_deposit_withdraw, native_token_evm_tester_EOS) try {
   //can't transfer to alice's balance because it isn't open
   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), "alice"),
                           eosio_assert_message_exception, eosio_assert_message_is("receiving account has not been opened"));


   open("alice"_n);

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

BOOST_FIXTURE_TEST_CASE(invalid_memos, native_token_evm_tester_EOS) try {
   //try some weird account names as memos
   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), "BANANA"),
                           eosio_assert_message_exception, eosio_assert_message_is("character is not in allowed character set for names"));

   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), "loooooooooooooooooong"),
                           eosio_assert_message_exception, eosio_assert_message_is("memo must be either 0x EVM address or already opened account name to credit deposit to"));

   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), "   "),
                           eosio_assert_message_exception, eosio_assert_message_is("character is not in allowed character set for names"));

   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), ""),
                           eosio_assert_message_exception, eosio_assert_message_is("memo must be either 0x EVM address or already opened account name to credit deposit to"));

   //try some invalid addresses as memos
   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), "0x"),
                           eosio_assert_message_exception, eosio_assert_message_is("character is not in allowed character set for names"));

   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), "0xf00"),
                           eosio_assert_message_exception, eosio_assert_message_is("character is not in allowed character set for names"));

   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), "0xf00000000000"),
                           eosio_assert_message_exception, eosio_assert_message_is("memo must be either 0x EVM address or already opened account name to credit deposit to"));

   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), "0x1234567890123456789012345678901234567890123456789012345678901234567890"),
                           eosio_assert_message_exception, eosio_assert_message_is("memo must be either 0x EVM address or already opened account name to credit deposit to"));

   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), "0x5b38da6a701c568545dzfcb03fcb875f56beddc4"),
                           eosio_assert_message_exception, eosio_assert_message_is("unable to parse destination address"));

   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), "xx5b38da6a701c568545dafcb03fcb875f56beddc4"),
                           eosio_assert_message_exception, eosio_assert_message_is("memo must be either 0x EVM address or already opened account name to credit deposit to"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(non_standard_native_symbol, native_token_evm_tester_SPOON) try {
   //the symbol 4,EOS is fixed as the expected native symbol. try transfering in a different symbol from eosio.token

   open("alice"_n);

   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), "alice"),
                           eosio_assert_message_exception, eosio_assert_message_is("received unexpected token"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(non_standard_native_symbol2, native_token_evm_tester_EOS) try {
   //the symbol 4,EOS is fixed as the expected native symbol. try transfering in a different symbol from eosio.token

   create_accounts({"usdt"_n});
   set_code("usdt"_n, testing::contracts::eosio_token_wasm());
   set_abi("usdt"_n, testing::contracts::eosio_token_abi().data());
   produce_block();

   symbol usdt_symbol = symbol::from_string("4,USDT");
   push_action("usdt"_n,
               "create"_n,
               "usdt"_n,
               mvo()("issuer", "usdt"_n)("maximum_supply", asset(10'000'000'000'0000, usdt_symbol)));
   push_action("usdt"_n,
               "issue"_n,
               "usdt"_n,
               mvo()("to", "usdt"_n)("quantity", asset(1'000'000'000'0000, usdt_symbol))("memo", ""));
   produce_block();

   BOOST_REQUIRE_EXCEPTION(push_action("usdt"_n,
                           "transfer"_n,
                           "usdt"_n,
                           mvo()("from", "usdt")("to", evm_account_name)("quantity", asset(1'000'0000, usdt_symbol))("memo", "")),
                           eosio_assert_message_exception, eosio_assert_message_is("received unexpected token"));

   create_accounts({"fakeeos"_n});
   set_code("fakeeos"_n, testing::contracts::eosio_token_wasm());
   set_abi("fakeeos"_n, testing::contracts::eosio_token_abi().data());
   produce_block();

   symbol eos_symbol = symbol::from_string("4,EOS");
   push_action("fakeeos"_n,
               "create"_n,
               "fakeeos"_n,
               mvo()("issuer", "fakeeos"_n)("maximum_supply", asset(10'000'000'000'0000, eos_symbol)));
   push_action("fakeeos"_n,
               "issue"_n,
               "fakeeos"_n,
               mvo()("to", "fakeeos"_n)("quantity", asset(1'000'000'000'0000, eos_symbol))("memo", ""));
   produce_block();

   BOOST_REQUIRE_EXCEPTION(push_action("fakeeos"_n,
                           "transfer"_n,
                           "fakeeos"_n,
                           mvo()("from", "fakeeos")("to", evm_account_name)("quantity", asset(1'000'0000, eos_symbol))("memo", "")),
                           eosio_assert_message_exception, eosio_assert_message_is("received unexpected token"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(transfer_notifier_without_init, native_token_evm_tester_noinit) try {
   //make sure to not accept transfer notifications unless contract has been inited

   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), "alice"),
                           eosio_assert_message_exception, eosio_assert_message_is("contract not initialized"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(basic_eos_evm_bridge, native_token_evm_tester_EOS) try {
   evm_eoa evm1, evm2;

   //reminder: .0001 EOS is 100 szabos
   const intx::uint256 smallest = 100_szabo;

   balance_and_dust expected_inevm = {
      make_asset(0)
   };
   BOOST_REQUIRE(expected_inevm == inevm());

   auto initial_special_balance = vault_balance("evm"_n);

   //to start with, there is no ingress bridge fee. should be 1->1

   //transfer 1.0000 EOS from alice to evm1 account
   {
      const int64_t to_bridge = 1'0000;
      const int64_t alice_native_before = native_balance("alice"_n);
      transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), evm1.address_0x());

      BOOST_REQUIRE_EQUAL(alice_native_before - native_balance("alice"_n), to_bridge);
      BOOST_REQUIRE(evm_balance(evm1) == smallest * to_bridge);

      expected_inevm.balance +=  make_asset(to_bridge);
      BOOST_REQUIRE(expected_inevm == inevm());
   }

   //transfer 0.5000 EOS from alice to evm2 account
   {
      const int64_t to_bridge = 5000;
      const int64_t alice_native_before = native_balance("alice"_n);
      transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), evm2.address_0x());

      BOOST_REQUIRE_EQUAL(alice_native_before - native_balance("alice"_n), to_bridge);
      BOOST_REQUIRE(evm_balance(evm2) == smallest * to_bridge);

      expected_inevm.balance +=  make_asset(to_bridge);
      BOOST_REQUIRE(expected_inevm == inevm());
   }

   //transfer 0.1234 EOS from alice to evm1 account
   {
      const int64_t to_bridge = 1234;
      const int64_t alice_native_before = native_balance("alice"_n);
      BOOST_REQUIRE(!!evm_balance(evm1));
      const intx::uint256 evm1_before = *evm_balance(evm1);
      transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), evm1.address_0x());

      BOOST_REQUIRE_EQUAL(alice_native_before - native_balance("alice"_n), to_bridge);
      BOOST_REQUIRE(evm_balance(evm1) == evm1_before + smallest * to_bridge);

      expected_inevm.balance +=  make_asset(to_bridge);
      BOOST_REQUIRE(expected_inevm == inevm());
   }

   //try to transfer 1000.0000 EOS from alice to evm1 account. can't do that alice!
   {
      const int64_t to_bridge = 1000'0000;
      BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), evm1.address_0x()),
                              eosio_assert_message_exception, eosio_assert_message_is("overdrawn balance"));

      BOOST_REQUIRE(expected_inevm == inevm());
   }

   //set the bridge free to 0.1000 EOS
   const int64_t bridge_fee = 1000;
   setfeeparams(fee_parameters{.ingress_bridge_fee = make_asset(bridge_fee)});

   //transferring less than the bridge fee isn't allowed
   {
      const int64_t to_bridge = 900;
      BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), evm1.address_0x()),
                              eosio_assert_message_exception, eosio_assert_message_is("must bridge more than ingress bridge fee"));
   }

   //transferring exact amount of bridge fee isn't allowed
   {
      const int64_t to_bridge = 1000;
      BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), evm1.address_0x()),
                              eosio_assert_message_exception, eosio_assert_message_is("must bridge more than ingress bridge fee"));
   }

   BOOST_REQUIRE(expected_inevm == inevm());

   //nothing should have accumulated in contract's special balance yet
   BOOST_REQUIRE(vault_balance("evm"_n) == initial_special_balance);

   //transfer 2.0000 EOS from alice to evm1 account, expect 1.9000 to be delivered to evm1 account, 0.1000 to contract balance
   {
      const int64_t to_bridge = 2'0000;
      const int64_t alice_native_before = native_balance("alice"_n);
      BOOST_REQUIRE(!!evm_balance(evm1));
      const intx::uint256 evm1_before = *evm_balance(evm1);
      transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), evm1.address_0x());

      BOOST_REQUIRE_EQUAL(alice_native_before - native_balance("alice"_n), to_bridge);
      BOOST_REQUIRE(evm_balance(evm1) == evm1_before + smallest * (to_bridge - bridge_fee));

      expected_inevm.balance +=  make_asset(to_bridge - bridge_fee);
      BOOST_REQUIRE(expected_inevm == inevm());

      intx::uint256 new_special_balance{initial_special_balance};
      new_special_balance += smallest * bridge_fee;
      BOOST_REQUIRE_EQUAL(static_cast<intx::uint256>(vault_balance("evm"_n)), new_special_balance);
   }

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(disallow_bridge_sigs_outside_bridge_trx, native_token_evm_tester_EOS) try {
   evm_eoa evm1;

   //r == 0 indicates a bridge signature. These are only allowed in contract-initiated (i.e. inline) EVM actions
   BOOST_REQUIRE_EXCEPTION(pushtx(generate_tx(evm1.address, 11111111_u256)),
                           eosio_assert_message_exception, eosio_assert_message_is("bridge signature used outside of bridge transaction"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(basic_evm_eos_bridge, native_token_evm_tester_EOS) try {
   evm_eoa evm1, evm2;

   //reminder: .0001 EOS is 100 szabos
   const intx::uint256 smallest = 100_szabo;

   const auto gas_fee = intx::uint256{get_config().gas_price} * 21000;

   //alice transfers in 10.0000 EOS to evm1
   {
      const int64_t to_bridge = 10'0000;
      const int64_t alice_native_before = native_balance("alice"_n);
      transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), evm1.address_0x());

      BOOST_REQUIRE_EQUAL(alice_native_before - native_balance("alice"_n), to_bridge);
      BOOST_REQUIRE(!!evm_balance(evm1));
      BOOST_REQUIRE(*evm_balance(evm1) == smallest * to_bridge);
   }

   //evm1 then transfers 2.0000 EOS to evm2
   {
      const int64_t to_transfer = 2'0000;
      const intx::uint256 evm1_before = *evm_balance(evm1);
      const intx::uint256 special_balance_before{vault_balance("evm"_n)};

      auto txn = generate_tx(evm2.address, 100_szabo * to_transfer);
      evm1.sign(txn);
      pushtx(txn);

      BOOST_REQUIRE_EQUAL(*evm_balance(evm1), (evm1_before - txn.value - gas_fee));
      BOOST_REQUIRE(!!evm_balance(evm2));
      BOOST_REQUIRE(*evm_balance(evm2) == txn.value);
      BOOST_REQUIRE_EQUAL(static_cast<intx::uint256>(vault_balance("evm"_n)), (special_balance_before + gas_fee));
   }

   //evm1 is going to egress 1.0000 EOS to alice. alice does not have an open balance, so this goes direct inline to native EOS balance
   {
      const int64_t to_bridge = 1'0000;
      const intx::uint256 evm1_before = *evm_balance(evm1);
      const int64_t alice_native_before = native_balance("alice"_n);

      auto txn = generate_tx(make_reserved_address("alice"_n.to_uint64_t()), 100_szabo * to_bridge);
      evm1.sign(txn);
      pushtx(txn);

      BOOST_REQUIRE_EQUAL(alice_native_before + to_bridge, native_balance("alice"_n));
      BOOST_REQUIRE_EQUAL(*evm_balance(evm1), (evm1_before - txn.value - gas_fee));
   }

   //evm1 is now going to try to egress 1.00001 EOS to alice. Since this includes dust without an open balance for alice, this fails
   {
      const int64_t to_bridge = 1'0000;
      const intx::uint256 evm1_before = *evm_balance(evm1);
      const int64_t alice_native_before = native_balance("alice"_n);

      auto txn = generate_tx(make_reserved_address("alice"_n.to_uint64_t()), 100_szabo * to_bridge + 10_szabo);
      evm1.sign(txn);
      BOOST_REQUIRE_EXCEPTION(pushtx(txn),
                              eosio_assert_message_exception, eosio_assert_message_is("egress bridging to non-open accounts must not contain dust"));

      BOOST_REQUIRE_EQUAL(alice_native_before, native_balance("alice"_n));
      BOOST_REQUIRE(evm_balance(evm1) == evm1_before);

      //alice will now open a balance
      open("alice"_n);
      //and try again
      pushtx(txn);

      BOOST_REQUIRE_EQUAL(alice_native_before, native_balance("alice"_n));          // native balance unchanged
      BOOST_REQUIRE_EQUAL(*evm_balance(evm1), (evm1_before - txn.value - gas_fee)); // EVM balance decreased
      BOOST_REQUIRE(vault_balance("alice"_n) ==
                    (balance_and_dust{make_asset(1'0000),
                                      10'000'000'000'000UL})); // vault balance increased to 1.0000 EOS, 10 szabo
   }

   //install some code on bob's account
   set_code("bob"_n, do_nothing_wast);

   //evm1 is going to try to egress 1.0000 EOS to bob. bob is neither open nor on allow list, but bob has code so egress is disallowed
   {
      const int64_t to_bridge = 1'0000;
      const intx::uint256 evm1_before = *evm_balance(evm1);

      auto txn = generate_tx(make_reserved_address("bob"_n.to_uint64_t()), 100_szabo * to_bridge);
      evm1.sign(txn);
      BOOST_REQUIRE_EXCEPTION(pushtx(txn),
                              eosio_assert_message_exception, eosio_assert_message_is("non-open accounts containing contract code must be on allow list for egress bridging"));

      //open up bob's balance
      open("bob"_n);
      //and now it'll go through
      pushtx(txn);
      BOOST_REQUIRE_EQUAL(vault_balance_token("bob"_n), to_bridge);
      BOOST_REQUIRE_EQUAL(*evm_balance(evm1), (evm1_before - txn.value - gas_fee));
   }

   //install some code on carol's account
   set_code("carol"_n, do_nothing_wast);

   //evm1 is going to try to egress 1.0000 EOS to carol. carol is neither open nor on allow list, but carol has code so egress is disallowed
   {
      const int64_t to_bridge = 1'0000;
      const int64_t carol_native_before = native_balance("carol"_n);
      const intx::uint256 evm1_before = *evm_balance(evm1);

      auto txn = generate_tx(make_reserved_address("carol"_n.to_uint64_t()), 100_szabo * to_bridge);
      evm1.sign(txn);
      BOOST_REQUIRE_EXCEPTION(pushtx(txn),
                              eosio_assert_message_exception, eosio_assert_message_is("non-open accounts containing contract code must be on allow list for egress bridging"));

      //add carol to the egress allow list
      addegress({"carol"_n});
      //and now it'll go through
      pushtx(txn);
      BOOST_REQUIRE_EQUAL(carol_native_before + to_bridge, native_balance("carol"_n));
      BOOST_REQUIRE_EQUAL(*evm_balance(evm1), (evm1_before - txn.value - gas_fee));

      //remove carol from egress allow list
      removeegress({"carol"_n});
      evm1.sign(txn);
      //and it won't go through again
      BOOST_REQUIRE_EXCEPTION(pushtx(txn),
                              eosio_assert_message_exception, eosio_assert_message_is("non-open accounts containing contract code must be on allow list for egress bridging"));
   }

   //Warning for future tests: evm1 nonce left incorrect here

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(evm_eos_nonexistant, native_token_evm_tester_EOS) try {
   evm_eoa evm1;

   transfer_token("alice"_n, "evm"_n, make_asset(10'0000), evm1.address_0x());

   //trying to bridge to a non-existing account is not allowed
   {
      const int64_t to_bridge = 1'0000;

      auto txn = generate_tx(make_reserved_address("spoon"_n.to_uint64_t()), 100_szabo * to_bridge);
      evm1.sign(txn);
      BOOST_REQUIRE_EXCEPTION(pushtx(txn),
                              eosio_assert_message_exception, eosio_assert_message_is("can only egress bridge to existing accounts"));
   }
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(evm_eos_disallow_reserved_zero, native_token_evm_tester_EOS) try {
   evm_eoa evm1;

   transfer_token("alice"_n, "evm"_n, make_asset(10'0000), evm1.address_0x());

   //doing anything with the reserved-zero address should fail; in this case just send an empty message to it
   auto txn = generate_tx(make_reserved_address(0u), 0);
   evm1.sign(txn);
   BOOST_REQUIRE_EXCEPTION(pushtx(txn),
                           eosio_assert_message_exception, eosio_assert_message_is("reserved 0 address cannot be used"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(withdraw_to, native_token_evm_tester_EOS) try {

   // transfer 10 EOS to evm_account_name
   transfer_token(faucet_account_name, evm_account_name, make_asset(10'0000), evm_account_name.to_string());

   // withdraw from "evm" to itself will fails
   BOOST_REQUIRE_EXCEPTION(
      push_action(evm_account_name, "withdraw"_n, evm_account_name, mvo()("owner", evm_account_name)("quantity", make_asset(9'0000))),
      eosio_assert_message_exception, [](const eosio_assert_message_exception& e) {
         return testing::expect_assert_message(e, "assertion failure with message: cannot transfer to self");
      });

   // withdraw from "evm" with extra paramter "to" set to itself
   BOOST_REQUIRE_EXCEPTION(push_action(evm_account_name, "withdraw"_n, evm_account_name,
                                       mvo()("owner", evm_account_name)("quantity", make_asset(9'0000))("to", "evm")),
                           eosio_assert_message_exception, [](const eosio_assert_message_exception& e) {
                              return testing::expect_assert_message(
                                 e, "assertion failure with message: cannot transfer to self");
                           });

   // withdraw from "evm" with extra paramter "to" set to non exist account
   BOOST_REQUIRE_EXCEPTION(push_action(evm_account_name, "withdraw"_n, evm_account_name,
                                       mvo()("owner", evm_account_name)("quantity", make_asset(9'0000))("to", "nonexistacc")),
                           eosio_assert_message_exception, [](const eosio_assert_message_exception& e) {
                              return testing::expect_assert_message(
                                 e, "assertion failure with message: to account does not exist");
                           });

   // withdraw to eosio
   push_action(evm_account_name, "withdraw"_n, evm_account_name, mvo()("owner", evm_account_name)("quantity", make_asset(9'0000))("to", "eosio"));

   // cannot withdraw more
   BOOST_REQUIRE_EXCEPTION(push_action(evm_account_name, "withdraw"_n, evm_account_name,
                                       mvo()("owner", evm_account_name)("quantity", make_asset(5'0000))("to", "eosio")),
                           eosio_assert_message_exception, [](const eosio_assert_message_exception& e) {
                              return testing::expect_assert_message(
                                 e, "assertion failure with message: overdrawn balance");
                           });

   // test with another account
   create_accounts({"miner1"_n});
   open("miner1"_n);
   transfer_token(faucet_account_name, evm_account_name, make_asset(15'0000), "miner1");

   BOOST_REQUIRE_EXCEPTION(
      push_action(evm_account_name, "withdraw"_n, "miner1"_n, mvo()("owner", "miner1"_n)("quantity", make_asset(15'0001))),
      eosio_assert_message_exception, [](const eosio_assert_message_exception& e) {
         return testing::expect_assert_message(e, "assertion failure with message: overdrawn balance");
      });

   // able to withdraw without "to" specified
   push_action(evm_account_name, "withdraw"_n, "miner1"_n, mvo()("owner", "miner1"_n)("quantity", make_asset(15'0000)));
}
FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(evm_eos_loopback, native_token_evm_tester_EOS) try {
   //alice does an ingress bridge to her reserved address, which just circles it back around as an egress bridge to herself
   {
      const int64_t to_bridge = 10'0000;
      const int64_t alice_native_before = native_balance("alice"_n);
      transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), silkworm::to_hex(make_reserved_address("alice"_n.to_uint64_t()), true));

      BOOST_REQUIRE_EQUAL(alice_native_before, native_balance("alice"_n));
   }

   //try it again, but this time send it to another account
   {
      const int64_t to_bridge = 10'0000;
      const int64_t alice_native_before = native_balance("alice"_n);
      const int64_t bob_native_before = native_balance("bob"_n);
      transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), silkworm::to_hex(make_reserved_address("bob"_n.to_uint64_t()), true));

      BOOST_REQUIRE_EQUAL(alice_native_before - native_balance("alice"_n), to_bridge);
      BOOST_REQUIRE_EQUAL(native_balance("bob"_n) - bob_native_before, to_bridge);
   }

   //try the above again, this time with a bridge free
   const int64_t bridge_fee = 1000;
   setfeeparams(fee_parameters{.ingress_bridge_fee = make_asset(bridge_fee)});
   produce_block();

   //alice to her own reserved address again..
   {
      const int64_t to_bridge = 10'0000;
      const int64_t alice_native_before = native_balance("alice"_n);
      transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), silkworm::to_hex(make_reserved_address("alice"_n.to_uint64_t()), true));

      BOOST_REQUIRE_EQUAL(alice_native_before - native_balance("alice"_n), bridge_fee);
   }

   //alice to bob's reserved address again..
   {
      const int64_t to_bridge = 10'0000;
      const int64_t alice_native_before = native_balance("alice"_n);
      const int64_t bob_native_before = native_balance("bob"_n);
      transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), silkworm::to_hex(make_reserved_address("bob"_n.to_uint64_t()), true));

      BOOST_REQUIRE_EQUAL(alice_native_before - native_balance("alice"_n), to_bridge);
      BOOST_REQUIRE_EQUAL(native_balance("bob"_n) - bob_native_before, to_bridge - bridge_fee);
   }
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(evm_eos_send_and_revert, native_token_evm_tester_EOS) try {
   // RevertTest.sol
   const std::string send_and_revert_bytecode =
      "608060405234801561001057600080fd5b50610236806100206000396000f3fe6080604052600436106100295760003560e01c"
      "806312c385411461002e578063648bf77414610043575b600080fd5b61004161003c366004610195565b610056565b005b6100"
      "416100513660046101b7565b6100a4565b6040516000906001600160a01b038316903480156108fc029184818181858888f193"
      "5050505090508061008b5761008b6101ea565b60405163cc57499d60e01b815260040160405180910390fd5b6040516312c385"
      "4160e01b81526001600160a01b038316600482015230906312c38541906127109034906024016000604051808303818589803b"
      "1580156100ea57600080fd5b5088f194505050505080156100fd575060015b610137573d80801561012b576040519150601f19"
      "603f3d011682016040523d82523d6000602084013e610130565b606091505b505061013f565b61013f6101ea565b6040516001"
      "600160a01b038216903480156108fc02916000818181858888f19350505050158015610174573d6000803e3d6000fd5b505050"
      "565b80356001600160a01b038116811461019057600080fd5b919050565b6000602082840312156101a757600080fd5b6101b0"
      "82610179565b9392505050565b600080604083850312156101ca57600080fd5b6101d383610179565b91506101e16020840161"
      "0179565b90509250929050565b634e487b7160e01b600052600160045260246000fdfea264697066735822122068a077777f85"
      "bfd56183f4a75a5d535181050f262de5766cc862098d59758f0a64736f6c63430008110033";

   evm_eoa evm1, evm2;
   transfer_token("alice"_n, "evm"_n, make_asset(50'0000), evm1.address_0x());
   transfer_token("alice"_n, "evm"_n, make_asset(50'0000), evm2.address_0x());
   BOOST_REQUIRE(!!evm_balance(evm2));
   open("bob"_n);
   open("carol"_n);

   const evmc::address send_and_revert_addr = deploy_contract(evm1, evmc::from_hex(send_and_revert_bytecode).value());

   //evm2 is going to send the send_and_revert_addr 1 EOS; initially the contract will send it to bob's reserved
   // address, but that will be reverted, and then it will be sent to carol's reserved address.
   const int64_t to_bridge = 1'0000;
   const intx::uint256 evm2_before = *evm_balance(evm2);

   silkworm::Transaction txn = generate_tx(send_and_revert_addr, 100_szabo * to_bridge, 100'000);
   txn.data = evmc::from_hex("0x648bf774"                                                                 //recover(address,address)
                             "000000000000000000000000bbbbbbbbbbbbbbbbbbbbbbbb3d0e000000000000"           //bob reserved
                             "000000000000000000000000bbbbbbbbbbbbbbbbbbbbbbbb41af488000000000").value(); //carol reserved
   evm2.sign(txn);
   pushtx(txn);

   BOOST_REQUIRE(vault_balance("bob"_n) == (balance_and_dust{make_asset(0), 0ULL}));
   BOOST_REQUIRE(vault_balance("carol"_n) == (balance_and_dust{make_asset(to_bridge), 0}));
   BOOST_REQUIRE(evm2_before - *evm_balance(evm2) == 100_szabo * to_bridge + 75215_u256 * get_config().gas_price);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(evm_eos_fanout, native_token_evm_tester_EOS) try {
   // Distributor.sol
   const std::string fanout_bytecode =
      "608060405234801561001057600080fd5b50610284806100206000396000f3fe60806040526004361061001e5760003560e01c"
      "80632929abe614610023575b600080fd5b610036610031366004610154565b610038565b005b6000805b848110156100f45785"
      "8582818110610056576100566101c0565b905060200201602081019061006b91906101d6565b6001600160a01b03166108fc85"
      "8584818110610089576100896101c0565b905060200201359081150290604051600060405180830381858888f1935050505015"
      "80156100bb573d6000803e3d6000fd5b508383828181106100ce576100ce6101c0565b90506020020135826100e0919061021c"
      "565b9150806100ec81610235565b91505061003c565b5034811461010157600080fd5b5050505050565b60008083601f840112"
      "61011a57600080fd5b50813567ffffffffffffffff81111561013257600080fd5b6020830191508360208260051b8501011115"
      "61014d57600080fd5b9250929050565b6000806000806040858703121561016a57600080fd5b843567ffffffffffffffff8082"
      "111561018257600080fd5b61018e88838901610108565b909650945060208701359150808211156101a757600080fd5b506101"
      "b487828801610108565b95989497509550505050565b634e487b7160e01b600052603260045260246000fd5b60006020828403"
      "12156101e857600080fd5b81356001600160a01b03811681146101ff57600080fd5b9392505050565b634e487b7160e01b6000"
      "52601160045260246000fd5b8082018082111561022f5761022f610206565b92915050565b6000600182016102475761024761"
      "0206565b506001019056fea26469706673582212209964e90f15129fadc3f3ade8e9fcd3b3d9c3f27617b3bcc28cf29cc5e3e5"
      "b5dc64736f6c63430008110033";

   evm_eoa evm1, evm2;
   transfer_token("alice"_n, "evm"_n, make_asset(50'0000), evm1.address_0x());
   transfer_token("alice"_n, "evm"_n, make_asset(10'0000), evm2.address_0x());
   BOOST_REQUIRE(!!evm_balance(evm2));

   const evmc::address fanout_addr = deploy_contract(evm1, evmc::from_hex(fanout_bytecode).value());

   //evm2 is going to try to send 3EOS, 1 to reserved alice, 1 to reserved bob, and 1 to reserved carol
   const int64_t to_bridge = 3'0000;
   const int64_t alice_native_start = native_balance("alice"_n);
   const int64_t bob_native_start = native_balance("bob"_n);
   const int64_t carol_native_start = native_balance("carol"_n);

   silkworm::Transaction txn = generate_tx(fanout_addr, 100_szabo * to_bridge, 300'000);

   txn.data = evmc::from_hex("0x2929abe6"                                                                 //distribute(address[],uint256[])
                             "0000000000000000000000000000000000000000000000000000000000000040"           //offset to 'to' list
                             "00000000000000000000000000000000000000000000000000000000000000c0"           //offset to 'amt' list
                             "0000000000000000000000000000000000000000000000000000000000000003"           //3 addresses
                             "000000000000000000000000bbbbbbbbbbbbbbbbbbbbbbbb345c850000000000"           //alice reserved
                             "000000000000000000000000bbbbbbbbbbbbbbbbbbbbbbbb3d0e000000000000"           //bob reserved
                             "000000000000000000000000bbbbbbbbbbbbbbbbbbbbbbbb41af488000000000"           //carol reserved
                             "0000000000000000000000000000000000000000000000000000000000000003"           //3 amounts
                             "0000000000000000000000000000000000000000000000000de0b6b3a7640000"           //1EOS
                             "0000000000000000000000000000000000000000000000000de0b6b3a7640000"           //1EOS
                             "0000000000000000000000000000000000000000000000000de0b6b3a7640000").value(); //1EOS
   evm2.sign(txn);

   //0 of 3 are open
   BOOST_REQUIRE_EXCEPTION(pushtx(txn),
                           eosio_assert_message_exception, eosio_assert_message_is("only one non-open account for egress bridging allowed in single transaction"));

   open("alice"_n);
   //only 1 of the 3 are open
   BOOST_REQUIRE_EXCEPTION(pushtx(txn),
                           eosio_assert_message_exception, eosio_assert_message_is("only one non-open account for egress bridging allowed in single transaction"));

   open("bob"_n);

   //nothing should have been sent to alice's nor bob's open balance yet
   BOOST_REQUIRE(vault_balance("alice"_n) == (balance_and_dust{make_asset(0), 0ULL}));
   BOOST_REQUIRE(vault_balance("bob"_n)   == (balance_and_dust{make_asset(0), 0ULL}));
   //nor should have any of their native balances changed
   BOOST_REQUIRE(native_balance("alice"_n) == alice_native_start);
   BOOST_REQUIRE(native_balance("bob"_n) == bob_native_start);
   BOOST_REQUIRE(native_balance("carol"_n) == carol_native_start);

   //2 of 3 open, this should go through
   {
      const intx::uint256 evm2_before = *evm_balance(evm2);
      pushtx(txn);

      BOOST_REQUIRE(vault_balance("alice"_n)  == (balance_and_dust{make_asset(1'0000), 0ULL}));
      BOOST_REQUIRE(vault_balance("bob"_n)    == (balance_and_dust{make_asset(1'0000), 0ULL}));
      BOOST_REQUIRE(native_balance("carol"_n) == carol_native_start + 1'0000);

      BOOST_REQUIRE(evm2_before - *evm_balance(evm2) == 100_szabo * to_bridge + 122826_u256 * get_config().gas_price);
   }

   open("carol"_n);
   evm2.sign(txn);

   //all 3 open
   {
      const intx::uint256 evm2_before = *evm_balance(evm2);
      pushtx(txn);

      BOOST_REQUIRE(vault_balance("alice"_n)  == (balance_and_dust{make_asset(2'0000), 0ULL}));
      BOOST_REQUIRE(vault_balance("bob"_n)    == (balance_and_dust{make_asset(2'0000), 0ULL}));
      BOOST_REQUIRE(vault_balance("carol"_n)  == (balance_and_dust{make_asset(1'0000), 0ULL}));
      BOOST_REQUIRE(native_balance("carol"_n) == carol_native_start + 1'0000);

      BOOST_REQUIRE(evm2_before - *evm_balance(evm2) == 100_szabo * to_bridge + 122826_u256 * get_config().gas_price);
   }
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(evm_eos_fanout_out_of_gas, native_token_evm_tester_EOS) try {
   // Distributor.sol
   const std::string fanout_bytecode =
      "608060405234801561001057600080fd5b50610284806100206000396000f3fe60806040526004361061001e5760003560e01c"
      "80632929abe614610023575b600080fd5b610036610031366004610154565b610038565b005b6000805b848110156100f45785"
      "8582818110610056576100566101c0565b905060200201602081019061006b91906101d6565b6001600160a01b03166108fc85"
      "8584818110610089576100896101c0565b905060200201359081150290604051600060405180830381858888f1935050505015"
      "80156100bb573d6000803e3d6000fd5b508383828181106100ce576100ce6101c0565b90506020020135826100e0919061021c"
      "565b9150806100ec81610235565b91505061003c565b5034811461010157600080fd5b5050505050565b60008083601f840112"
      "61011a57600080fd5b50813567ffffffffffffffff81111561013257600080fd5b6020830191508360208260051b8501011115"
      "61014d57600080fd5b9250929050565b6000806000806040858703121561016a57600080fd5b843567ffffffffffffffff8082"
      "111561018257600080fd5b61018e88838901610108565b909650945060208701359150808211156101a757600080fd5b506101"
      "b487828801610108565b95989497509550505050565b634e487b7160e01b600052603260045260246000fd5b60006020828403"
      "12156101e857600080fd5b81356001600160a01b03811681146101ff57600080fd5b9392505050565b634e487b7160e01b6000"
      "52601160045260246000fd5b8082018082111561022f5761022f610206565b92915050565b6000600182016102475761024761"
      "0206565b506001019056fea26469706673582212209964e90f15129fadc3f3ade8e9fcd3b3d9c3f27617b3bcc28cf29cc5e3e5"
      "b5dc64736f6c63430008110033";

   evm_eoa evm1, evm2;
   transfer_token("alice"_n, "evm"_n, make_asset(50'0000), evm1.address_0x());
   transfer_token("alice"_n, "evm"_n, make_asset(10'0000), evm2.address_0x());
   BOOST_REQUIRE(!!evm_balance(evm2));

   const evmc::address fanout_addr = deploy_contract(evm1, evmc::from_hex(fanout_bytecode).value());

   const int64_t to_bridge = 3'0000;
   const int64_t carol_native_start = native_balance("carol"_n);

   silkworm::Transaction txn = generate_tx(fanout_addr, 100_szabo * to_bridge, 100'000);

   txn.data = evmc::from_hex("0x2929abe6"                                                                 //distribute(address[],uint256[])
                             "0000000000000000000000000000000000000000000000000000000000000040"           //offset to 'to' list
                             "00000000000000000000000000000000000000000000000000000000000000c0"           //offset to 'amt' list
                             "0000000000000000000000000000000000000000000000000000000000000003"           //3 addresses
                             "000000000000000000000000bbbbbbbbbbbbbbbbbbbbbbbb345c850000000000"           //alice reserved
                             "000000000000000000000000bbbbbbbbbbbbbbbbbbbbbbbb3d0e000000000000"           //bob reserved
                             "000000000000000000000000bbbbbbbbbbbbbbbbbbbbbbbb41af488000000000"           //carol reserved
                             "0000000000000000000000000000000000000000000000000000000000000003"           //3 amounts
                             "0000000000000000000000000000000000000000000000000de0b6b3a7640000"           //1EOS
                             "0000000000000000000000000000000000000000000000000de0b6b3a7640000"           //1EOS
                             "0000000000000000000000000000000000000000000000000de0b6b3a7640000").value(); //1EOS
   evm2.sign(txn);

   open("alice"_n);
   open("bob"_n);

   const intx::uint256 evm2_before = *evm_balance(evm2);
   const int64_t evm_vault_before = vault_balance(evm_account_name).balance.get_amount();

   pushtx(txn);

   //transaction will have run out of gas. make sure no one received any tokens and evm2 was only deducted its max gas

   BOOST_REQUIRE(vault_balance("alice"_n)  == (balance_and_dust{make_asset(0'0000), 0ULL}));
   BOOST_REQUIRE(vault_balance("bob"_n)    == (balance_and_dust{make_asset(0'0000), 0ULL}));
   BOOST_REQUIRE(native_balance("carol"_n) == carol_native_start + 0'0000);

   BOOST_REQUIRE(evm2_before - *evm_balance(evm2) == 100000_u256 * get_config().gas_price);

   //also check that the miner & contract were credited
   BOOST_REQUIRE_EQUAL(vault_balance(evm_account_name).balance.get_amount() - evm_vault_before, get_config().gas_price / 1000000000);

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
