#include "basic_evm_tester.hpp"

using namespace eosio::testing;
using namespace evm_test;

#include <eosevm/block_mapping.hpp>

struct statistics_evm_tester : basic_evm_tester
{
   evm_eoa faucet_eoa;

   static constexpr name miner_account_name = "alice"_n;

   statistics_evm_tester() :
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

BOOST_AUTO_TEST_SUITE(statistics_evm_tests)


BOOST_FIXTURE_TEST_CASE(gas_fee_statistics_miner_cut, statistics_evm_tester)
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
      {1'000'000'000, 50'000,  10'500'000'000'000, 10'500'000'000'000},
      {1'000'000'000, 0,       0,                  21'000'000'000'000},
      {1'000'000'000, 10'000,  2'100'000'000'000,  18'900'000'000'000},
      {1'000'000'000, 90'000, 18'900'000'000'000,   2'100'000'000'000},
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

      // Gas fee statistics
      const auto s = get_statistics();
      const auto gas_fee_sum_before = s.gas_fee_income;

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

      // Gas fee statistics
      const auto s2 = get_statistics();
      BOOST_CHECK_EQUAL(static_cast<intx::uint256>(s2.gas_fee_income), 
                        static_cast<intx::uint256>(gas_fee_sum_before) + gas_fee - gas_fee_miner_portion);

      faucet_eoa.next_nonce = 0;
   };

   for (const auto& trial : gas_fee_trials) {
      run_test_with_alice_as_miner(trial);
   }
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(gas_fee_statistics_miner_cut_v1, statistics_evm_tester)
try {
   static constexpr uint64_t base_gas_price = 300'000'000'000;    // 300 gwei

   init();

   auto miner_account = "miner"_n;
   create_accounts({miner_account});
   open(miner_account);

   // Set base price
   setfeeparams({.gas_price=base_gas_price});

   auto config = get_config();
   BOOST_REQUIRE(config.miner_cut == suggested_miner_cut);

   // Set version 1
   setversion(1, evm_account_name);

   produce_blocks(3);

   // Fund evm1 address with 10.0000 EOS / trigger version change and sets miner_cut to 0
   evm_eoa evm1;
   transfer_token("alice"_n, evm_account_name, make_asset(10'0000), evm1.address_0x());

   config = get_config();
   BOOST_REQUIRE(config.miner_cut == 0);

   // miner_cut can't be changed when version >= 1
   BOOST_REQUIRE_EXCEPTION(setfeeparams({.miner_cut=10'0000}),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: can't set miner_cut");});

   auto inclusion_price = 50'000'000'000;    // 50 gwei

   evm_eoa evm2;
   auto tx = generate_tx(evm2.address, 1);
   tx.type = silkworm::TransactionType::kDynamicFee;
   tx.max_priority_fee_per_gas = inclusion_price*2;
   tx.max_fee_per_gas = base_gas_price + inclusion_price;

   BOOST_REQUIRE(vault_balance(miner_account) == (balance_and_dust{make_asset(0), 0ULL}));

   evm1.sign(tx);
   pushtx(tx, miner_account);

   //21'000 * 50'000'000'000 = 0.00105
   BOOST_REQUIRE(vault_balance(miner_account) == (balance_and_dust{make_asset(10), 50'000'000'000'000ULL}));

   // Gas fee statistics
   //21'000 * 300'000'000'000 = 0.0063
   const auto s = get_statistics();
   BOOST_REQUIRE(s.gas_fee_income == (balance_and_dust{make_asset(63), 0ULL}));

   tx = generate_tx(evm2.address, 1);
   tx.type = silkworm::TransactionType::kDynamicFee;
   tx.max_priority_fee_per_gas = 0;
   tx.max_fee_per_gas = base_gas_price + inclusion_price;

   evm1.sign(tx);
   pushtx(tx, miner_account);

   //0.00105 + 0
   BOOST_REQUIRE(vault_balance(miner_account) == (balance_and_dust{make_asset(10), 50'000'000'000'000ULL}));

   // Gas fee statistics
   // 0.0063 + 0.0063 = 0.0126
   const auto s2 = get_statistics();
   BOOST_REQUIRE(s2.gas_fee_income == (balance_and_dust{make_asset(126), 0ULL}));
}
FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(gas_fee_statistics_basic_eos_evm_bridge, statistics_evm_tester) try {

   init();
   fund_evm_faucet();
   open(miner_account_name);

   evm_eoa evm1, evm2;

   //reminder: .0001 EOS is 100 szabos
   const intx::uint256 smallest = 100_szabo;

   //to start with, there is no ingress bridge fee. should be 1->1

   const auto s = get_statistics();
   const auto initial_gas_count = s.gas_fee_income;
   const auto initial_bridge_count = s.ingress_bridge_fee_income;
   
   //transfer 1.0000 EOS from alice to evm1 account
   {
      const int64_t to_bridge = 1'0000;

      transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), evm1.address_0x());

      const auto s2 = get_statistics();
      BOOST_REQUIRE_EQUAL(s2.ingress_bridge_fee_income.balance, make_asset(0));
      // Bridge transfers should not generate gas income as the gas fee for the evm transfer is paid by evm_runtime.
      BOOST_REQUIRE(s2.gas_fee_income == initial_gas_count);
   }

   //transfer 0.5000 EOS from alice to evm2 account
   {
      const int64_t to_bridge = 5000;

      transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), evm2.address_0x());

      const auto s2 = get_statistics();
      BOOST_REQUIRE(s2.ingress_bridge_fee_income == initial_bridge_count);
      // Bridge transfers should not generate gas income as the gas fee for the evm transfer is paid by evm_runtime.
      BOOST_REQUIRE(s2.gas_fee_income == initial_gas_count);
   }

   //transfer 0.1234 EOS from alice to evm1 account
   {
      const int64_t to_bridge = 1234;

      const intx::uint256 evm1_before = *evm_balance(evm1);
      transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), evm1.address_0x());

      const auto s2 = get_statistics();
      BOOST_REQUIRE(s2.ingress_bridge_fee_income == initial_bridge_count);
      // Bridge transfers should not generate gas income as the gas fee for the evm transfer is paid by evm_runtime.
      BOOST_REQUIRE(s2.gas_fee_income == initial_gas_count);

   }

   //set the bridge free to 0.1000 EOS
   const int64_t bridge_fee = 1000;
   setfeeparams(fee_parameters{.ingress_bridge_fee = make_asset(bridge_fee)});

   //transfer 2.0000 EOS from alice to evm1 account, expect 1.9000 to be delivered to evm1 account, 0.1000 to contract balance
   {
      const int64_t to_bridge = 2'0000;

      const intx::uint256 evm1_before = *evm_balance(evm1);
      transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), evm1.address_0x());

      const auto s2 = get_statistics();
      BOOST_REQUIRE_EQUAL(s2.ingress_bridge_fee_income.balance, make_asset(bridge_fee));
      // Bridge transfers should not generate gas income as the gas fee for the evm transfer is paid by evm_runtime.
      BOOST_REQUIRE(s2.gas_fee_income == initial_gas_count);
   }

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(gas_fee_statistics_basic_eos_evm_bridge_tokeswap, statistics_evm_tester) try {

   init();
   fund_evm_faucet();
   open(miner_account_name);

   // ensure stats are using old symbol
   const auto s0 = get_statistics();
   BOOST_REQUIRE_EQUAL(s0.gas_fee_income.balance.get_symbol(), native_symbol);
   BOOST_REQUIRE_EQUAL(s0.ingress_bridge_fee_income.balance.get_symbol(), native_symbol);      

   // swap EOS->A
   swapgastoken();
   transfer_token("alice"_n, vaulta_account_name, make_asset(50'0000), "");

   evm_eoa evm1, evm2;

   //reminder: .0001 A is 100 szabos
   const intx::uint256 smallest = 100_szabo;

   //to start with, there is no ingress bridge fee. should be 1->1

   const auto s = get_statistics();
   const auto initial_gas_count = s.gas_fee_income;
   const auto initial_bridge_count = s.ingress_bridge_fee_income;

   BOOST_REQUIRE_EQUAL(s.gas_fee_income.balance.get_symbol(), new_gas_symbol);
   BOOST_REQUIRE_EQUAL(s.ingress_bridge_fee_income.balance.get_symbol(), new_gas_symbol);
   
   //transfer 0.5000 A from alice to evm2 account
   {
      const int64_t to_bridge = 5000;

      transfer_token("alice"_n, "evm"_n, asset(to_bridge, new_gas_symbol), evm2.address_0x(), vaulta_account_name);

      const auto s2 = get_statistics();
      BOOST_REQUIRE(s2.ingress_bridge_fee_income == initial_bridge_count);
      // Bridge transfers should not generate gas income as the gas fee for the evm transfer is paid by evm_runtime.
      BOOST_REQUIRE(s2.gas_fee_income == initial_gas_count);
   }

   // can't set the bridge free to 0.1000 EOS after token swap
   const int64_t bridge_fee = 1000;
   BOOST_REQUIRE_EXCEPTION(setfeeparams(fee_parameters{.ingress_bridge_fee = make_asset(bridge_fee)}),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "bridge symbol can't change");});

   // set the bridge free to 0.1000 A
   setfeeparams(fee_parameters{.ingress_bridge_fee = asset(bridge_fee, new_gas_symbol)});

} FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()
