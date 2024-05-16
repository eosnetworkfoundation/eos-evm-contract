#include "basic_evm_tester.hpp"

using namespace eosio::testing;
using namespace evm_test;

#include <eosevm/block_mapping.hpp>

struct different_gas_token_tester : basic_evm_tester
{
   static constexpr name miner_account_name = "theminer"_n;
   static constexpr name gas_token_account_name = "gastoken"_n;

   symbol gas_symbol = symbol::from_string("6,GAS");
   extended_asset gas_token{asset(0, gas_symbol), gas_token_account_name};

   different_gas_token_tester()
   {
      create_accounts({miner_account_name});

      create_accounts({gas_token_account_name});
      set_code(gas_token_account_name, testing::contracts::eosio_token_wasm());
      set_abi(gas_token_account_name, testing::contracts::eosio_token_abi().data());
      produce_block();

      push_action(gas_token_account_name, "create"_n, gas_token_account_name,
                  mvo()("issuer", gas_token_account_name)("maximum_supply", asset(10'000'000'000'0000, gas_symbol)));
      push_action(gas_token_account_name, "issue"_n, gas_token_account_name,
                  mvo()("to", gas_token_account_name)("quantity", asset(1'000'000'000'0000, gas_symbol))("memo", ""));
      produce_block();
   }

   transaction_trace_ptr transfer_gas_token(name from, name to, asset quantity, std::string memo="") {
      return transfer_token(from, to, quantity, memo, gas_token_account_name);
   }

   asset gas_balance(name acct) {
      return get_currency_balance(gas_token_account_name, gas_symbol, acct);
   }

};

BOOST_AUTO_TEST_SUITE(different_gas_token_tests)

BOOST_FIXTURE_TEST_CASE(basic_test, different_gas_token_tester)
try {

   // Init with GAS token
   init(
      evm_chain_id,
      suggested_gas_price,
      suggested_miner_cut,
      asset::from_string("0.000001 GAS"),
      false,
      gas_token_account_name
   );
   
   // Prepare self balance
   transfer_gas_token(gas_token_account_name, evm_account_name, asset::from_string("1.000000 GAS"), evm_account_name.to_string());

   open(miner_account_name);

   auto cfg = get_config();
   BOOST_REQUIRE(cfg.token_contract.has_value() && cfg.token_contract == gas_token_account_name );

   // Fund evm1 address with 10.000000 GAS
   evm_eoa evm1;
   transfer_gas_token(gas_token_account_name, evm_account_name, asset::from_string("10.000000 GAS"), evm1.address_0x());

   BOOST_REQUIRE(evm_balance(evm1) == intx::uint256(balance_and_dust{asset::from_string("9.999999 GAS"), 0}));
   BOOST_REQUIRE(vault_balance(evm_account_name) == (balance_and_dust{asset::from_string("1.000001 GAS"), 0}));
   BOOST_REQUIRE(vault_balance(miner_account_name) == (balance_and_dust{asset::from_string("0.000000 GAS"), 0ULL}));

   // Transfer 1 Wei to evm2
   evm_eoa evm2;
   auto tx = generate_tx(evm2.address, 1);
   evm1.sign(tx);
   pushtx(tx, miner_account_name);

   BOOST_REQUIRE(evm_balance(evm1) == intx::uint256(balance_and_dust{asset::from_string("9.996848 GAS"), 999999999999}));
   BOOST_REQUIRE(evm_balance(evm2) == intx::uint256(balance_and_dust{asset::from_string("0.000000 GAS"), 1}));
   BOOST_REQUIRE(vault_balance(evm_account_name) == (balance_and_dust{asset::from_string("1.002836 GAS"), 0}));
   BOOST_REQUIRE(vault_balance(miner_account_name) == (balance_and_dust{asset::from_string("0.000315 GAS"), 0}));

}
FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
