#include "basic_evm_tester.hpp"

#include <fc/io/raw_fwd.hpp>

using namespace eosio::testing;

static const char do_nothing_wast[] = R"=====(
(module
 (export "apply" (func $apply))
 (func $apply (param $0 i64) (param $1 i64) (param $2 i64)
   ;; nothing
 )
)
)=====";

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
      return evmc_address({0xff, 0xff, 0xff, 0xff,
                           0xff, 0xff, 0xff, 0xff,
                           0xff, 0xff, 0xff, 0xff,
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

BOOST_FIXTURE_TEST_CASE(non_standard_native_symbol, native_token_evm_tester_SPOON) try {
   //the symbol 4,EOS is fixed as the expected native symbol. try transfering in a different symbol from eosio.token

   open("alice"_n);

   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, "evm"_n, make_asset(1'0000), "alice"),
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
   setingressfee(make_asset(bridge_fee));

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
   BOOST_REQUIRE(vault_balance("evm"_n) == std::make_tuple(make_asset(0), 0UL));

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

      BOOST_REQUIRE_EQUAL(vault_balance_token("evm"_n), bridge_fee);
   }

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(disallow_bridge_sigs_outside_bridge_trx, native_token_evm_tester_EOS) try {
   evm_eoa evm1;
   silkworm::Transaction txn {
      .type = silkworm::Transaction::Type::kLegacy,
      .max_priority_fee_per_gas = 0,
      .max_fee_per_gas = 0,
      .gas_limit = 21000,
      .to = evm1.address,
      .value = 11111111_u256,
   };

   //r == 0 indicates a bridge signature. These are only allowed in contract-initiated (i.e. inline) EVM actions
   BOOST_REQUIRE_EXCEPTION(pushtx(txn),
                           eosio_assert_message_exception, eosio_assert_message_is("bridge signature used outside of bridge transaction"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(basic_evm_eos_bridge, native_token_evm_tester_EOS) try {
   evm_eoa evm1, evm2;

   //reminder: .0001 EOS is 100 szabos
   const intx::uint256 smallest = 100_szabo;

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

      silkworm::Transaction txn {
         .type = silkworm::Transaction::Type::kLegacy,
         .max_priority_fee_per_gas = 0,
         .max_fee_per_gas = 0,
         .gas_limit = 21000,
         .to = evm2.address,
         .value = 100_szabo * to_transfer,
      };
      evm1.sign(txn);
      pushtx(txn);

      BOOST_REQUIRE(*evm_balance(evm1) == evm1_before - txn.value);
      BOOST_REQUIRE(!!evm_balance(evm2));
      BOOST_REQUIRE(*evm_balance(evm2) == txn.value);
   }

   //evm1 is going to egress 1.0000 EOS to alice. alice does not have an open balance, so this goes direct inline to native EOS valance
   {
      const int64_t to_bridge = 1'0000;
      const intx::uint256 evm1_before = *evm_balance(evm1);
      const int64_t alice_native_before = native_balance("alice"_n);

      silkworm::Transaction txn {
         .type = silkworm::Transaction::Type::kLegacy,
         .max_priority_fee_per_gas = 0,
         .max_fee_per_gas = 0,
         .gas_limit = 21000,
         .to = make_reserved_address("alice"_n.to_uint64_t()),
         .value = 100_szabo * to_bridge,
      };
      evm1.sign(txn);
      pushtx(txn);

      BOOST_REQUIRE_EQUAL(alice_native_before + to_bridge, native_balance("alice"_n));
      BOOST_REQUIRE(evm_balance(evm1) == evm1_before - txn.value);
   }

   //evm1 is now going to try to egress 1.00001 EOS to alice. Since this includes dust without an open balance for alice, this fails
   {
      const int64_t to_bridge = 1'0000;
      const intx::uint256 evm1_before = *evm_balance(evm1);
      const int64_t alice_native_before = native_balance("alice"_n);

      silkworm::Transaction txn {
         .type = silkworm::Transaction::Type::kLegacy,
         .max_priority_fee_per_gas = 0,
         .max_fee_per_gas = 0,
         .gas_limit = 21000,
         .to = make_reserved_address("alice"_n.to_uint64_t()),
         .value = 100_szabo * to_bridge + 10_szabo, //dust
      };
      evm1.sign(txn);
      BOOST_REQUIRE_EXCEPTION(pushtx(txn),
                              eosio_assert_message_exception, eosio_assert_message_is("egress bridging to non-open accounts must not contain dust"));

      BOOST_REQUIRE_EQUAL(alice_native_before, native_balance("alice"_n));
      BOOST_REQUIRE(evm_balance(evm1) == evm1_before);

      //alice will now open a balance
      open("alice"_n);
      //and try again
      pushtx(txn);

      BOOST_REQUIRE_EQUAL(alice_native_before, native_balance("alice"_n));                                  //native balance unchanged
      BOOST_REQUIRE(evm_balance(evm1) == evm1_before - txn.value);                                          //EVM balance decresed
      BOOST_REQUIRE(vault_balance("alice"_n) == std::make_tuple(make_asset(1'0000), 10'000'000'000'000UL)); //vault balance increased to 1.0000EOS, 10szabo
   }

   //install some code on bob's account
   set_code("bob"_n, do_nothing_wast);

   //evm1 is going to try to egress 1.0000 EOS to bob. bob is neither open nor on allow list, but bob has code so egress is disallowed
   {
      const int64_t to_bridge = 1'0000;
      const intx::uint256 evm1_before = *evm_balance(evm1);

      silkworm::Transaction txn {
         .type = silkworm::Transaction::Type::kLegacy,
         .max_priority_fee_per_gas = 0,
         .max_fee_per_gas = 0,
         .gas_limit = 21000,
         .to = make_reserved_address("bob"_n.to_uint64_t()),
         .value = 100_szabo * to_bridge,
      };
      evm1.sign(txn);
      BOOST_REQUIRE_EXCEPTION(pushtx(txn),
                              eosio_assert_message_exception, eosio_assert_message_is("non-open accounts containing contract code must be on allow list for egress bridging"));

      //open up bob's balance
      open("bob"_n);
      //and now it'll go through
      pushtx(txn);
      BOOST_REQUIRE_EQUAL(vault_balance_token("bob"_n), to_bridge);
      BOOST_REQUIRE(evm_balance(evm1) == evm1_before - txn.value);
   }

   //install some code on carol's account
   set_code("carol"_n, do_nothing_wast);

   //evm1 is going to try to egress 1.0000 EOS to carol. carol is neither open nor on allow list, but carol has code so egress is disallowed
   {
      const int64_t to_bridge = 1'0000;
      const int64_t carol_native_before = native_balance("carol"_n);
      const intx::uint256 evm1_before = *evm_balance(evm1);

      silkworm::Transaction txn {
         .type = silkworm::Transaction::Type::kLegacy,
         .max_priority_fee_per_gas = 0,
         .max_fee_per_gas = 0,
         .gas_limit = 21000,
         .to = make_reserved_address("carol"_n.to_uint64_t()),
         .value = 100_szabo * to_bridge,
      };
      evm1.sign(txn);
      BOOST_REQUIRE_EXCEPTION(pushtx(txn),
                              eosio_assert_message_exception, eosio_assert_message_is("non-open accounts containing contract code must be on allow list for egress bridging"));

      //add carol to the egress allow list
      addegress({"carol"_n});
      //and now it'll go through
      pushtx(txn);
      BOOST_REQUIRE_EQUAL(carol_native_before + to_bridge, native_balance("carol"_n));
      BOOST_REQUIRE(evm_balance(evm1) == evm1_before - txn.value);

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

      silkworm::Transaction txn {
         .type = silkworm::Transaction::Type::kLegacy,
         .max_priority_fee_per_gas = 0,
         .max_fee_per_gas = 0,
         .gas_limit = 21000,
         .to = make_reserved_address("spoon"_n.to_uint64_t()),
         .value = 100_szabo * to_bridge,
      };
      evm1.sign(txn);
      BOOST_REQUIRE_EXCEPTION(pushtx(txn),
                              eosio_assert_message_exception, eosio_assert_message_is("can only egress bridge to existing accounts"));
   }
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(evm_eos_disallow_reserved_zero, native_token_evm_tester_EOS) try {
   evm_eoa evm1;

   transfer_token("alice"_n, "evm"_n, make_asset(10'0000), evm1.address_0x());

   //doing anything with the reserved-zero address should fail; in this case just send an empty message to it
   silkworm::Transaction txn {
      .type = silkworm::Transaction::Type::kLegacy,
      .max_priority_fee_per_gas = 0,
      .max_fee_per_gas = 0,
      .gas_limit = 21000,
      .to = make_reserved_address(0u)
   };
   evm1.sign(txn);
   BOOST_REQUIRE_EXCEPTION(pushtx(txn),
                           eosio_assert_message_exception, eosio_assert_message_is("reserved 0 address cannot be used"));
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()