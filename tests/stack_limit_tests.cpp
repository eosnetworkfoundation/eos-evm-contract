#include <boost/test/unit_test.hpp>

#include "basic_evm_tester.hpp"
#include <silkworm/core/execution/address.hpp>
#include "utils.hpp"

using namespace evm_test;
using eosio::testing::eosio_assert_message_is;
using eosio::testing::expect_assert_message;

    

struct stack_limit_tester : basic_evm_tester {

   evmc::address m_contract_addr;

   stack_limit_tester() {
      create_accounts({"alice"_n});
      transfer_token(faucet_account_name, "alice"_n, make_asset(10000'0000));
      init();
   }

   auto deploy_simple_contract(evm_eoa& evm_account) {
   // SPDX-License-Identifier: MIT
   // pragma solidity >=0.4.18;

   // contract StackTest {
   //    function internalRecursive(uint256 level) public {
   //       if (level == 0) return;
   //       internalRecursive(level - 1);
   //    }

   //    function externalRecursive(uint256 level) public {
   //       if (level == 0) return;
   //       StackTest(this).externalRecursive(level - 1);
   //    }
   // }

      const std::string simple_bytecode = "6080604052348015600f57600080fd5b506102448061001f6000396000f3fe608060405234801561001057600080fd5b50600436106100365760003560e01c8063973c32f81461003b578063e06afd0a14610057575b600080fd5b61005560048036038101906100509190610154565b610073565b005b610071600480360381019061006c9190610154565b610095565b005b60008103156100925761009160018261008c91906101b0565b610073565b5b50565b6000810315610116573073ffffffffffffffffffffffffffffffffffffffff1663e06afd0a6001836100c791906101b0565b6040518263ffffffff1660e01b81526004016100e391906101f3565b600060405180830381600087803b1580156100fd57600080fd5b505af1158015610111573d6000803e3d6000fd5b505050505b50565b600080fd5b6000819050919050565b6101318161011e565b811461013c57600080fd5b50565b60008135905061014e81610128565b92915050565b60006020828403121561016a57610169610119565b5b60006101788482850161013f565b91505092915050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052601160045260246000fd5b60006101bb8261011e565b91506101c68361011e565b92508282039050818111156101de576101dd610181565b5b92915050565b6101ed8161011e565b82525050565b600060208201905061020860008301846101e4565b9291505056fea264697066735822122006713188904378b93322ae651845d43a307ffa88f5a92afb1ca87184c28fb84464736f6c634300081a0033";

      // Deploy simple contract
      auto contract_addr = deploy_contract(evm_account, evmc::from_hex(simple_bytecode).value());
      uint64_t contract_account_id = find_account_by_address(contract_addr).value().id;
      m_contract_addr = contract_addr;
      return std::make_tuple(contract_addr, contract_account_id);
   }

   void call_test(const intx::uint256& level, evm_eoa& evm_account, bool internal) {
      auto pad = [](const std::string& s){
        const size_t l = 64;
        if (s.length() >= l) return s;
        size_t pl = l - s.length();
        return std::string(pl, '0') + s;
      };

      auto txn = generate_tx(m_contract_addr, 0, 500'000);
      txn.data = internal ? evmc::from_hex("973c32f8").value() : evmc::from_hex("e06afd0a").value();

      txn.data += evmc::from_hex(pad(intx::hex(level))).value();
      evm_account.sign(txn);

      pushtx(txn);

      produce_blocks(1);
   }

};

BOOST_AUTO_TEST_SUITE(stack_limit_tests)
BOOST_FIXTURE_TEST_CASE(max_limit, stack_limit_tester) try {

   // Fund evm1 address with 100 EOS
   evm_eoa evm1;
   const int64_t to_bridge = 1000000;
   transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), evm1.address_0x());

   deploy_simple_contract(evm1);

   int64_t level = 0;

   // At least 1024
   call_test(1024, evm1, true);

   // At least 11
   call_test(11, evm1, false);


} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
