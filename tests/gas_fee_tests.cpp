#include "basic_evm_tester.hpp"

using namespace eosio::testing;
using namespace evm_test;

struct gas_fee_evm_tester : basic_evm_tester
{
   evm_eoa faucet_eoa;

   static constexpr name miner_account_name = "alice"_n;

   gas_fee_evm_tester() :
      faucet_eoa(evmc::from_hex("a3f1b69da92a0233ce29485d3049a4ace39e8d384bbc2557e3fc60940ce4e954").value())
   {
      create_accounts({miner_account_name});
      transfer_token(faucet_account_name, miner_account_name, make_asset(100'0000));
   }

   void fund_evm_faucet()
   {
      transfer_token(faucet_account_name, evm_account_name, make_asset(100'0000), faucet_eoa.address_0x());
   }
};

BOOST_AUTO_TEST_SUITE(gas_fee_evm_tests)

BOOST_FIXTURE_TEST_CASE(check_init_required_gas_fee_parameters, gas_fee_evm_tester)
try {

   auto suggested_ingress_bridge_fee = make_asset(suggested_ingress_bridge_fee_amount);

   mvo missing_gas_price;
   missing_gas_price                                        //
      ("gas_price", fc::variant())                          //
      ("miner_cut", suggested_miner_cut)                    //
      ("ingress_bridge_fee", suggested_ingress_bridge_fee); //

   mvo missing_miner_cut;
   missing_miner_cut                                        //
      ("gas_price", suggested_gas_price)                    //
      ("miner_cut", fc::variant())                          //
      ("ingress_bridge_fee", suggested_ingress_bridge_fee); //

   mvo missing_ingress_bridge_fee;
   missing_ingress_bridge_fee                //
      ("gas_price", suggested_gas_price)     //
      ("miner_cut", suggested_miner_cut)     //
      ("ingress_bridge_fee", fc::variant()); //

   // gas_price must be provided during init
   BOOST_REQUIRE_EXCEPTION(
      push_action(
         evm_account_name, "init"_n, evm_account_name, mvo()("chainid", evm_chain_id)("fee_params", missing_gas_price)),
      eosio_assert_message_exception,
      eosio_assert_message_is("All required fee parameters not specified: missing gas_price"));

   // miner_cut must be provided during init
   BOOST_REQUIRE_EXCEPTION(
      push_action(
         evm_account_name, "init"_n, evm_account_name, mvo()("chainid", evm_chain_id)("fee_params", missing_miner_cut)),
      eosio_assert_message_exception,
      eosio_assert_message_is("All required fee parameters not specified: missing miner_cut"));

   // It is acceptable for the ingress_bridge_fee to not be provided during init.
   push_action(evm_account_name,
               "init"_n,
               evm_account_name,
               mvo()("chainid", evm_chain_id)("fee_params", missing_ingress_bridge_fee));
}
FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(set_fee_parameters, gas_fee_evm_tester)
try {
   uint64_t starting_gas_price = 5'000'000'000;
   uint32_t starting_miner_cut = 50'000;
   int64_t starting_ingress_bridge_fee_amount = 3;

   init(evm_chain_id, starting_gas_price, starting_miner_cut, make_asset(starting_ingress_bridge_fee_amount));

   const auto& conf1 = get_config();

   BOOST_CHECK_EQUAL(conf1.gas_price, starting_gas_price);
   BOOST_CHECK_EQUAL(conf1.miner_cut, starting_miner_cut);
   BOOST_CHECK_EQUAL(conf1.ingress_bridge_fee, make_asset(starting_ingress_bridge_fee_amount));

   // Cannot set miner_cut to above 100%.
   BOOST_REQUIRE_EXCEPTION(setfeeparams({.miner_cut = 100'001}),
                           eosio_assert_message_exception,
                           eosio_assert_message_is("miner_cut cannot exceed 100,000 (100%)"));

   // Change only miner_cut to 100%.
   setfeeparams({.miner_cut = 100'000});

   const auto& conf2 = get_config();

   BOOST_CHECK_EQUAL(conf2.gas_price, conf1.gas_price);
   BOOST_CHECK_EQUAL(conf2.miner_cut, 100'000);
   BOOST_CHECK_EQUAL(conf2.ingress_bridge_fee, conf1.ingress_bridge_fee);

   // Change only gas_price to 0
   setfeeparams({.gas_price = 0});

   const auto& conf3 = get_config();

   BOOST_CHECK_EQUAL(conf3.gas_price, 0);
   BOOST_CHECK_EQUAL(conf3.miner_cut, conf2.miner_cut);
   BOOST_CHECK_EQUAL(conf3.ingress_bridge_fee, conf2.ingress_bridge_fee);

   // Change only ingress_bridge_fee to 0.0040 EOS
   setfeeparams({.ingress_bridge_fee = make_asset(40)});

   const auto& conf4 = get_config();

   BOOST_CHECK_EQUAL(conf4.gas_price, conf3.gas_price);
   BOOST_CHECK_EQUAL(conf4.miner_cut, conf3.miner_cut);
   BOOST_CHECK_EQUAL(conf4.ingress_bridge_fee, make_asset(40));
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(reject_low_gas_price, gas_fee_evm_tester)
try {
   init(evm_chain_id, suggested_gas_price, suggested_miner_cut, make_asset(suggested_ingress_bridge_fee_amount));
   fund_evm_faucet();

   evm_eoa recipient;

   {
      // Low gas price is rejected

      static_assert(suggested_gas_price >= 2);

      auto restore_nonce = faucet_eoa.next_nonce;

      silkworm::Transaction tx{
         .type = silkworm::Transaction::Type::kLegacy,
         .max_priority_fee_per_gas = suggested_gas_price - 1,
         .max_fee_per_gas = suggested_gas_price - 1,
         .gas_limit = 21000,
         .to = recipient.address,
         .value = 1,
      };
      faucet_eoa.sign(tx);

      BOOST_REQUIRE_EXCEPTION(
         pushtx(tx), eosio_assert_message_exception, eosio_assert_message_is("gas price is too low"));

      faucet_eoa.next_nonce = restore_nonce;
   }

   {
      // Exactly matching gas price is accepted

      silkworm::Transaction tx{
         .type = silkworm::Transaction::Type::kLegacy,
         .max_priority_fee_per_gas = suggested_gas_price,
         .max_fee_per_gas = suggested_gas_price,
         .gas_limit = 21000,
         .to = recipient.address,
         .value = 1,
      };
      faucet_eoa.sign(tx);
      pushtx(tx);
   }

   {
      // Higher gas price is also okay

      silkworm::Transaction tx{
         .type = silkworm::Transaction::Type::kLegacy,
         .max_priority_fee_per_gas = suggested_gas_price + 1,
         .max_fee_per_gas = suggested_gas_price + 1,
         .gas_limit = 21000,
         .to = recipient.address,
         .value = 1,
      };
      faucet_eoa.sign(tx);
      pushtx(tx);
   }
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(miner_cut_calculation, gas_fee_evm_tester)
try {
   produce_block();
   control->abort_block();

   static constexpr uint32_t hundred_percent = 100'000;

   evm_eoa recipient;

   auto dump_accounts = [&]() {
      scan_accounts([](evm_test::account_object&& account) -> bool {
         idump((account));
         return false;
      });
   };

   struct gas_fee_data
   {
      uint64_t gas_price;
      uint32_t miner_cut;
      uint64_t expected_gas_fee_miner_portion;
      uint64_t expected_gas_fee_contract_portion;
   };

   std::vector<gas_fee_data> gas_fee_trials = {
      {0,             50'000,  0,                  0                 },
      {1'000'000'000, 0,       0,                  21'000'000'000'000},
      {1'000'000'000, 10'000,  2'100'000'000'000,  18'900'000'000'000},
      {1'000'000'000, 100'000, 21'000'000'000'000, 0                 },
   };

   // EVM contract account acts as the miner
   auto run_test_with_contract_as_miner = [this, &recipient](const gas_fee_data& trial) {
      speculative_block_starter<decltype(*this)> sb{*this};

      init(evm_chain_id, trial.gas_price, trial.miner_cut);
      fund_evm_faucet();

      const auto gas_fee = intx::uint256{trial.gas_price * 21000};

      BOOST_CHECK_EQUAL(gas_fee,
                        intx::uint256(trial.expected_gas_fee_miner_portion + trial.expected_gas_fee_contract_portion));

      const intx::uint256 special_balance_before{vault_balance(evm_account_name)};
      const intx::uint256 faucet_before = evm_balance(faucet_eoa).value();

      auto tx = generate_tx(recipient.address, 1_gwei);
      faucet_eoa.sign(tx);
      pushtx(tx);

      BOOST_CHECK_EQUAL(*evm_balance(faucet_eoa), (faucet_before - tx.value - gas_fee));
      BOOST_REQUIRE(evm_balance(recipient).has_value());
      BOOST_CHECK_EQUAL(*evm_balance(recipient), tx.value);
      BOOST_CHECK_EQUAL(static_cast<intx::uint256>(vault_balance(evm_account_name)),
                        (special_balance_before + gas_fee));

      faucet_eoa.next_nonce = 0;
   };

   for (const auto& trial : gas_fee_trials) {
      run_test_with_contract_as_miner(trial);
   }

   // alice acts as the miner
   auto run_test_with_alice_as_miner = [this, &recipient](const gas_fee_data& trial) {
      speculative_block_starter<decltype(*this)> sb{*this};

      init(evm_chain_id, trial.gas_price, trial.miner_cut);
      fund_evm_faucet();
      open(miner_account_name);

      const auto gas_fee = intx::uint256{trial.gas_price * 21000};
      const auto gas_fee_miner_portion = (gas_fee * trial.miner_cut) / hundred_percent;

      BOOST_CHECK_EQUAL(gas_fee_miner_portion, intx::uint256(trial.expected_gas_fee_miner_portion));

      const auto gas_fee_contract_portion = gas_fee - gas_fee_miner_portion;
      BOOST_CHECK_EQUAL(gas_fee_contract_portion, intx::uint256(trial.expected_gas_fee_contract_portion));

      const intx::uint256 special_balance_before{vault_balance(evm_account_name)};
      const intx::uint256 miner_balance_before{vault_balance(miner_account_name)};
      const intx::uint256 faucet_before = evm_balance(faucet_eoa).value();

      auto tx = generate_tx(recipient.address, 1_gwei);
      faucet_eoa.sign(tx);
      pushtx(tx, miner_account_name);

      BOOST_CHECK_EQUAL(*evm_balance(faucet_eoa), (faucet_before - tx.value - gas_fee));
      BOOST_REQUIRE(evm_balance(recipient).has_value());
      BOOST_CHECK_EQUAL(*evm_balance(recipient), tx.value);
      BOOST_CHECK_EQUAL(static_cast<intx::uint256>(vault_balance(evm_account_name)),
                        (special_balance_before + gas_fee - gas_fee_miner_portion));
      BOOST_CHECK_EQUAL(static_cast<intx::uint256>(vault_balance(miner_account_name)),
                        (miner_balance_before + gas_fee_miner_portion));

      faucet_eoa.next_nonce = 0;
   };

   for (const auto& trial : gas_fee_trials) {
      run_test_with_alice_as_miner(trial);
   }
}
FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()