#include "basic_evm_tester.hpp"

using namespace eosio::testing;
using namespace evm_test;

#include <eosevm/block_mapping.hpp>

BOOST_AUTO_TEST_SUITE(evm_gas_prices_tests)

struct gas_prices_evm_tester : basic_evm_tester
{
   evm_eoa faucet_eoa;

   static constexpr name miner_account_name = "alice"_n;

   gas_prices_evm_tester() :
      faucet_eoa(evmc::from_hex("a3f1b69da92a0233ce29485d3049a4ace39e8d384bbc2557e3fc60940ce4e954").value())
   {
      create_accounts({miner_account_name});
      transfer_token(faucet_account_name, miner_account_name, make_asset(10000'0000));
   }

   void fund_evm_faucet()
   {
      transfer_token(faucet_account_name, evm_account_name, make_asset(1000'0000), faucet_eoa.address_0x());
   }

   auto send_tx(auto& eoa, silkworm::Transaction& txn, const evmone::eosevm::gas_prices& gas_prices, const evmone::gas_parameters& gas_params) -> auto {
      auto pre = evm_balance(eoa);
      eoa.sign(txn);
      pushtx(txn);
      auto post = evm_balance(eoa);
      BOOST_REQUIRE(pre.has_value() && post.has_value());
      auto inclusion_price = static_cast<uint64_t>(txn.priority_fee_per_gas(gas_prices.get_base_price()));
      auto effective_gas_price =  inclusion_price + static_cast<uint64_t>(gas_prices.get_base_price());
      auto cost = *pre - *post - txn.value;
      auto total_gas_used = cost/effective_gas_price;
      const intx::uint256 factor_num{gas_prices.storage_price};
      const intx::uint256 factor_den{gas_prices.get_base_price() + inclusion_price};
      auto scaled_gp = evmone::gas_parameters::apply_discount_factor(factor_num, factor_den, gas_params);
      return std::make_tuple(cost, inclusion_price, effective_gas_price, total_gas_used, scaled_gp);
   }

   auto get_code_len(uint64_t code_id) {
      size_t len=0;
      scan_account_code([&](const evm_test::account_code& ac) -> bool {
         len = ac.code.size();
         return ac.id == code_id;
      });
      return len;
   }

   auto pad(const std::string& s) {
      const size_t l = 64;
      if (s.length() >= l) return s;
      size_t pl = l - s.length();
      return std::string(pl, '0') + s;
   }

   auto validate_final_fee(const auto& res, uint64_t cpu_gas, uint64_t storage_gas, const evmone::eosevm::gas_prices& gas_prices) {
      
      auto [cost, inclusion_price, effective_gas_price, total_gas_billed, scaled_gp] = res;
      intx::uint256 total_gas_used = cpu_gas+storage_gas;

      intx::uint256 gas_refund = 0;
      if( gas_prices.storage_price >= gas_prices.overhead_price ) {
         gas_refund = cpu_gas;
         gas_refund *= intx::uint256(gas_prices.storage_price-gas_prices.overhead_price);
         gas_refund /= intx::uint256(effective_gas_price);
      }

      total_gas_used -= gas_refund;
      BOOST_REQUIRE(total_gas_used == total_gas_billed);
      BOOST_REQUIRE(cost == total_gas_used*effective_gas_price);
   }

};

BOOST_FIXTURE_TEST_CASE(gas_price_validation, gas_prices_evm_tester) try {
   using silkworm::kGiga;

   uint64_t suggested_gas_price = 150*kGiga;
   init(15555, suggested_gas_price);
   produce_block();

   auto get_prices_queue = [&]() -> std::vector<prices_queue> {
      std::vector<prices_queue> queue;
      scan_prices_queue([&](prices_queue&& row) -> bool {
         queue.push_back(row);
         return false;
      });
      return queue;
   };

   // no price specified
   BOOST_REQUIRE_EXCEPTION(
      setgasprices({.overhead_price={}, .storage_price={}}),
      eosio_assert_message_exception,
      eosio_assert_message_is("at least one price must be specified"));

   // zero storage price
   BOOST_REQUIRE_EXCEPTION(
      setgasprices({.overhead_price={}, .storage_price=0}),
      eosio_assert_message_exception,
      eosio_assert_message_is("zero storage price is not allowed"));

   // switch to v3 without gas prices
   BOOST_REQUIRE_EXCEPTION(
      setversion(3, evm_account_name),
      eosio_assert_message_exception,
      eosio_assert_message_is("storage price must be set"));

   // gas price before switch
   BOOST_REQUIRE( get_config().gas_price == 150*kGiga );

   setgasprices({.overhead_price={}, .storage_price=1});
   setversion(3, evm_account_name);
   fund_evm_faucet();
   produce_block();
   produce_block();

   // queue is empty
   BOOST_REQUIRE( get_prices_queue().size() == 0 );

   // old gas price == 0
   BOOST_REQUIRE( get_config().gas_price == 0 );

   // overhead={} storage=1
   auto gas_prices = get_config().gas_prices.value();
   BOOST_REQUIRE(gas_prices.overhead_price.has_value() == false);
   BOOST_REQUIRE(gas_prices.storage_price.value() == 1);

   // same as current
   setgasprices({.overhead_price={}, .storage_price=1});
   BOOST_REQUIRE( get_prices_queue().size() == 0 );

   // new value (queued)
   setgasprices({.overhead_price={}, .storage_price=2});
   BOOST_REQUIRE( get_prices_queue().size() == 1 );
   produce_block();

   // repeat last value (not queued)
   setgasprices({.overhead_price={}, .storage_price=2});
   BOOST_REQUIRE( get_prices_queue().size() == 1 );

   // wait and trigger queue processing
   produce_blocks(2*181);
   fund_evm_faucet();

   // empty queue
   BOOST_REQUIRE( get_prices_queue().size() == 0 );

   // overhead={} storage=2
   gas_prices = get_config().gas_prices.value();
   BOOST_REQUIRE(gas_prices.overhead_price.has_value() == false);
   BOOST_REQUIRE(gas_prices.storage_price.value() == 2);

   // set only overhead
   setgasprices({.overhead_price=10, .storage_price={}});
   BOOST_REQUIRE( get_prices_queue().size() == 1 );

   // wait and trigger queue processing
   produce_blocks(2*181);
   fund_evm_faucet();

   // empty queue
   BOOST_REQUIRE( get_prices_queue().size() == 0 );

   // overhead=10 storage=2
   gas_prices = get_config().gas_prices.value();
   BOOST_REQUIRE(gas_prices.overhead_price.value() == 10);
   BOOST_REQUIRE(gas_prices.storage_price.value() == 2);

   // reject change of gas with setfeeparams
   BOOST_REQUIRE_EXCEPTION(
      setfeeparams({.gas_price = 1}),
      eosio_assert_message_exception,
      eosio_assert_message_is("Can't use set_fee_parameters to set gas_price"));

   // updtgasparam should enqueue price
   updtgasparam(asset(10'0000, native_symbol), 1'000'000'000, evm_account_name);
   auto q = get_prices_queue();
   BOOST_REQUIRE(q.size() == 1);
   BOOST_REQUIRE(q[0].prices.overhead_price.has_value() == false);
   BOOST_REQUIRE(q[0].prices.storage_price.has_value() == true);

   // wait and trigger queue processing
   produce_blocks(2*181);
   fund_evm_faucet();

   // empty queue
   BOOST_REQUIRE( get_prices_queue().size() == 0 );

   // set only overhead (should not enqueue, since we already have that value as the current value)
   setgasprices({.overhead_price=10, .storage_price={}});
   BOOST_REQUIRE( get_prices_queue().size() == 0 );


} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(gas_param_scale, gas_prices_evm_tester) try {
   using silkworm::kGiga;

   intx::uint256 gas_refund{0};

   uint64_t suggested_gas_price = 150*kGiga;
   init(15555, suggested_gas_price);
   produce_block();

   /////////////////////////////////////
   /// change EOS EVM VERSION => 3   ///
   /////////////////////////////////////

   // We need to have some gas prices active before switching to v3
   evmone::eosevm::gas_prices gas_prices1{
      .overhead_price = 80*kGiga,
      .storage_price  = 70*kGiga
   };
   setgasprices({.overhead_price=gas_prices1.overhead_price, .storage_price=gas_prices1.storage_price});

   setversion(3, evm_account_name);
   fund_evm_faucet();
   produce_block();
   produce_block();

   auto run_gasparams_scale_test = [&](const evmone::eosevm::gas_prices& gas_prices) {

      fund_evm_faucet();
      produce_block();

      setgasprices({.overhead_price=gas_prices.overhead_price, .storage_price=gas_prices.storage_price});
      produce_blocks(2*181);

      // Test traces of `handle_evm_transfer` (EVM VERSION=3)
      evm_eoa evm1;
      const int64_t to_bridge = 1000000;
      auto trace = transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), evm1.address_0x());

      BOOST_REQUIRE(trace->action_traces.size() == 4);
      BOOST_REQUIRE(trace->action_traces[0].act.account == token_account_name);
      BOOST_REQUIRE(trace->action_traces[0].act.name == "transfer"_n);
      BOOST_REQUIRE(trace->action_traces[1].act.account == token_account_name);
      BOOST_REQUIRE(trace->action_traces[1].act.name == "transfer"_n);
      BOOST_REQUIRE(trace->action_traces[2].act.account == token_account_name);
      BOOST_REQUIRE(trace->action_traces[2].act.name == "transfer"_n);
      BOOST_REQUIRE(trace->action_traces[3].act.account == evm_account_name);
      BOOST_REQUIRE(trace->action_traces[3].act.name == "evmtx"_n);

      auto event_v3 = get_event_from_trace<evm_test::evmtx_v3>(trace->action_traces[3].act.data);
      BOOST_REQUIRE(event_v3.eos_evm_version == 3);
      BOOST_REQUIRE(event_v3.overhead_price == gas_prices.overhead_price);
      BOOST_REQUIRE(event_v3.storage_price == gas_prices.storage_price);

      evmone::gas_parameters gas_params;
      gas_params.values_.G_txnewaccount = 10000;
      gas_params.values_.G_newaccount = 25000;
      gas_params.values_.G_txcreate = 32000;
      gas_params.values_.G_codedeposit = 200;
      gas_params.values_.G_sset = 20000;

      setgasparam(gas_params.values_.G_txnewaccount, gas_params.values_.G_newaccount, gas_params.values_.G_txcreate, gas_params.values_.G_codedeposit, gas_params.values_.G_sset, evm_account_name);
      produce_blocks(3);

      // *****************************************************
      // TEST G_codedeposit and G_txcreate
      // *****************************************************

      // pragma solidity >=0.8.2 <0.9.0;
      // contract Storage {
      //     uint256 number;
      //     function store(uint256 num) public {
      //         number = num;
      //     }
      //     function retrieve() public view returns (uint256) {
      //         return number;
      //     }
      //     function transferRandomWei() public {
      //         address payable randomAddress = payable(address(uint160(uint256(keccak256(abi.encodePacked(block.timestamp, block.difficulty, block.number))))));
      //         randomAddress.transfer(1 wei);
      //     }
      //     receive() external payable {}
      // }


      auto deploy_contract_ex = [&](evm_eoa& eoa, const evmc::bytes& bytecode) {
         auto pre = evm_balance(eoa);
         const auto gas_price = get_config().gas_price;
         const auto base_fee_per_gas = gas_prices.get_base_price();
         auto contract_address = deploy_contract(evm1, bytecode);
         auto post = evm_balance(eoa);
         BOOST_REQUIRE(pre.has_value() && post.has_value());
         auto inclusion_price = std::min(gas_price, gas_price - base_fee_per_gas);
         auto effective_gas_price =  inclusion_price + base_fee_per_gas;
         auto cost = *pre - *post;
         auto total_gas_used = cost/effective_gas_price;
         const intx::uint256 factor_num{gas_prices.storage_price};
         const intx::uint256 factor_den{gas_prices.get_base_price() + inclusion_price};
         auto scaled_gp = evmone::gas_parameters::apply_discount_factor(factor_num, factor_den, gas_params);
         return std::make_tuple(std::make_tuple(cost, inclusion_price, effective_gas_price, total_gas_used, scaled_gp), contract_address);
      };

      const std::string contract_bytecode = "608060405234801561001057600080fd5b50610265806100206000396000f3fe6080604052600436106100385760003560e01c80632e64cec11461004457806357756c591461006f5780636057361d146100865761003f565b3661003f57005b600080fd5b34801561005057600080fd5b506100596100af565b6040516100669190610158565b60405180910390f35b34801561007b57600080fd5b506100846100b8565b005b34801561009257600080fd5b506100ad60048036038101906100a891906101a4565b610135565b005b60008054905090565b60004244436040516020016100cf939291906101f2565b6040516020818303038152906040528051906020012060001c90508073ffffffffffffffffffffffffffffffffffffffff166108fc60019081150290604051600060405180830381858888f19350505050158015610131573d6000803e3d6000fd5b5050565b8060008190555050565b6000819050919050565b6101528161013f565b82525050565b600060208201905061016d6000830184610149565b92915050565b600080fd5b6101818161013f565b811461018c57600080fd5b50565b60008135905061019e81610178565b92915050565b6000602082840312156101ba576101b9610173565b5b60006101c88482850161018f565b91505092915050565b6000819050919050565b6101ec6101e78261013f565b6101d1565b82525050565b60006101fe82866101db565b60208201915061020e82856101db565b60208201915061021e82846101db565b60208201915081905094935050505056fea26469706673582212206c1cecefa543d1df1237047de88cf3e3fe2f7a6ed26ffd4ee4f844ef2987845964736f6c634300080d0033";
      auto [res, contract_address] = deploy_contract_ex(evm1, evmc::from_hex(contract_bytecode).value());
      auto contract_account = find_account_by_address(contract_address).value();
      auto code_len = get_code_len(contract_account.code_id.value());
      // total_cpu_gas_consumed: 30945
      validate_final_fee(res, 30945, code_len*std::get<4>(res).values_.G_codedeposit + std::get<4>(res).values_.G_txcreate, gas_prices);

      // *****************************************************
      // TEST G_sset
      // *****************************************************
      auto set_value = [&](auto& eoa, const intx::uint256& v) -> auto {
         auto txn = generate_tx(contract_address, 0, 1'000'000);
         txn.data = evmc::from_hex("6057361d").value(); //sha3(store(uint256))
         txn.data += evmc::from_hex(pad(intx::hex(v))).value();
         return send_tx(eoa, txn, gas_prices, gas_params);
      };
      res = set_value(evm1, intx::uint256{77});
      // total_cpu_gas_consumed: 26646
      validate_final_fee(res, 26646, std::get<4>(res).values_.G_sset, gas_prices); //sset - reset

      // *****************************************************
      // TEST G_txnewaccount
      // *****************************************************
      auto send_to_new_address = [&](auto& eoa, const intx::uint256& v) -> auto {
         evm_eoa new_address;
         auto txn = generate_tx(new_address.address, v, 1'000'000);
         return send_tx(eoa, txn, gas_prices, gas_params);
      };
      res = send_to_new_address(evm1, intx::uint256{1});
      // total_cpu_gas_consumed: 21000
      validate_final_fee(res, 21000, std::get<4>(res).values_.G_txnewaccount, gas_prices);

      // *****************************************************
      // TEST G_newaccount
      // *****************************************************

      // Fund contract with 100Wei
      auto txn_fund = generate_tx(contract_address, intx::uint256{100}, 1'000'000);
      evm1.sign(txn_fund);
      pushtx(txn_fund);
      produce_block();

      auto transfer_random_wei = [&](auto& eoa) -> auto {
         auto txn = generate_tx(contract_address, 0, 1'000'000);
         txn.data = evmc::from_hex("57756c59").value(); //sha3(transferRandomWei())
         return send_tx(eoa, txn, gas_prices, gas_params);
      };

      res = transfer_random_wei(evm1);
      // total_cpu_gas_consumed: 31250
      validate_final_fee(res, 31250, std::get<4>(res).values_.G_newaccount, gas_prices);
   };

   run_gasparams_scale_test(gas_prices1);

   evmone::eosevm::gas_prices gas_prices2{
      .overhead_price = 30*kGiga,
      .storage_price  = 50*kGiga
   };
   run_gasparams_scale_test(gas_prices2);

   evmone::eosevm::gas_prices gas_prices3{
      .overhead_price = 100*kGiga,
      .storage_price  = 100*kGiga
   };
   run_gasparams_scale_test(gas_prices3);

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
