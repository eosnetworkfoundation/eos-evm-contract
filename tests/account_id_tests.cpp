#include "basic_evm_tester.hpp"
#include <silkworm/core/types/address.hpp>

using namespace evm_test;
struct account_id_tester : basic_evm_tester {
   account_id_tester() {
      create_accounts({"alice"_n});
      transfer_token(faucet_account_name, "alice"_n, make_asset(10000'0000));
      init();
   }

   std::optional<uint64_t> get_max_account_id() {
      const auto& db = control->db();
      const auto* t_id = db.find<chain::table_id_object, chain::by_code_scope_table>( boost::make_tuple( evm_account_name, evm_account_name, "account"_n ) );
      if ( !t_id ) return {};

      const auto& idx = db.get_index<chain::key_value_index, chain::by_scope_primary>();
      if( idx.begin() == idx.end() ) return {};
      
      decltype(t_id->id) next_tid(t_id->id._id + 1);
      auto itr = idx.lower_bound(boost::make_tuple(next_tid));
      if(itr == idx.end() || itr->t_id == next_tid) --itr;

      if(itr->t_id._id != t_id->id._id) return {};
      return itr->primary_key;
   }

   std::string int_str32(uint32_t x) {
        std::stringstream hex_ss;
        hex_ss << std::hex << x; 
        int hex_length = hex_ss.str().length();

        std::stringstream ss;
        ss << std::setfill('0') << std::setw(64 - hex_length) << 0 << std::hex << std::uppercase << x;
        return ss.str();
    }

   // // SPDX-License-Identifier: MIT
   // pragma solidity ^0.8.17;
   // contract Factory {
   //     // Returns the address of the newly deployed contract
   //     function deploy(
   //         bytes32 _salt
   //     ) public payable returns (address) {
   //         return address(new TestContract{salt: _salt}());
   //     }
   // }
   // contract TestContract {
   //     uint foo;
   //     function set_foo(uint val) public {
   //         foo = val;
   //     }
   //     function killme() public {
   //         address payable addr = payable(address(0));
   //         selfdestruct(addr);
   //     }
   // }

   const std::string factory_and_test_bytecode = "608060405234801561001057600080fd5b506102c1806100206000396000f3fe60806040526004361061001e5760003560e01c80632b85ba3814610023575b600080fd5b61003d600480360381019061003891906100d2565b610053565b60405161004a9190610140565b60405180910390f35b6000816040516100629061008a565b8190604051809103906000f5905080158015610082573d6000803e3d6000fd5b509050919050565b6101308061015c83390190565b600080fd5b6000819050919050565b6100af8161009c565b81146100ba57600080fd5b50565b6000813590506100cc816100a6565b92915050565b6000602082840312156100e8576100e7610097565b5b60006100f6848285016100bd565b91505092915050565b600073ffffffffffffffffffffffffffffffffffffffff82169050919050565b600061012a826100ff565b9050919050565b61013a8161011f565b82525050565b60006020820190506101556000830184610131565b9291505056fe608060405234801561001057600080fd5b50610110806100206000396000f3fe6080604052348015600f57600080fd5b506004361060325760003560e01c806324d97a4a146037578063e5d5dfbc14603f575b600080fd5b603d6057565b005b605560048036038101906051919060b2565b6072565b005b60008073ffffffffffffffffffffffffffffffffffffffff16ff5b8060008190555050565b600080fd5b6000819050919050565b6092816081565b8114609c57600080fd5b50565b60008135905060ac81608b565b92915050565b60006020828403121560c55760c4607c565b5b600060d184828501609f565b9150509291505056fea2646970667358221220e59ba0023d9a6f99a6734000d8812c1830a2a61597b322310827ec350a4304dd64736f6c63430008120033a26469706673582212205e94c73549f6e1751509ff5704aec0a8ada3a60ab2b8cc1b9df9febc7ed2bd2f64736f6c63430008120033";

   size_t get_storage_slots_count(uint64_t account_id) {
      size_t total_slots{0};
      scan_account_storage(account_id, [&](const storage_slot& slot)->bool{
         total_slots++;
         return true;
      });
      return total_slots;
   }


};

BOOST_AUTO_TEST_SUITE(account_id_tests)

BOOST_FIXTURE_TEST_CASE(new_account_id_logic, account_id_tester) try {
   
   BOOST_CHECK(!get_max_account_id().has_value());
   BOOST_REQUIRE_EXCEPTION(get_config2(), fc::out_of_range_exception, [](const fc::out_of_range_exception& e) {return true;});

   // Fund evm1 address with 100 EOS
   evm_eoa evm1;
   const int64_t to_bridge = 1000000;
   transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), evm1.address_0x());
   BOOST_CHECK(get_max_account_id().value() == 0);
   BOOST_CHECK(get_config2().next_account_id == 1);

   // Deploy Factory and Test contracts
   auto contract_addr = deploy_contract(evm1, evmc::from_hex(factory_and_test_bytecode).value());
   uint64_t contract_account_id = find_account_by_address(contract_addr).value().id;
   BOOST_CHECK(get_max_account_id().value() == 1);
   BOOST_CHECK(get_config2().next_account_id == 2);

   // Call 'Factory::deploy'
   auto txn = generate_tx(contract_addr, 0, 1'000'000);

   silkworm::Bytes data;
   data += evmc::from_hex("2b85ba38").value();     //deploy
   data += evmc::from_hex(int_str32(555)).value(); //salt=555
   txn.data = data;
   evm1.sign(txn);
   pushtx(txn);

   BOOST_CHECK(get_max_account_id().value() == 2);
   BOOST_CHECK(get_config2().next_account_id == 3);

   // Recover TestContract address
   auto test_contract_address = find_account_by_id(2).value().address;

   // Call 'TestContract::set_foo(1234)'
   txn = generate_tx(test_contract_address, 0, 1'000'000);
   data.clear();
   data += evmc::from_hex("e5d5dfbc").value(); //set_foo
   data += evmc::from_hex(int_str32(1234)).value(); //val=1234
   txn.data = data;
   evm1.sign(txn);
   pushtx(txn);

   BOOST_CHECK(get_storage_slots_count(2) == 1);

   // Call 'TestContract::killme'
   txn = generate_tx(test_contract_address, 0, 1'000'000);
   data.clear();
   data += evmc::from_hex("24d97a4a").value(); //killme
   txn.data = data;
   evm1.sign(txn);
   pushtx(txn);

   BOOST_CHECK(get_storage_slots_count(2) == 1);

   BOOST_CHECK(get_max_account_id().value() == 1);
   BOOST_CHECK(get_config2().next_account_id == 3);

   // Call 'Factory::deploy' again
   txn = generate_tx(contract_addr, 0, 1'000'000);
   data.clear();
   data += evmc::from_hex("2b85ba38").value();     //deploy
   data += evmc::from_hex(int_str32(555)).value(); //salt=555
   txn.data = data;
   evm1.sign(txn);
   pushtx(txn);

   BOOST_CHECK(get_max_account_id().value() == 3);
   BOOST_CHECK(get_config2().next_account_id == 4);

   BOOST_CHECK(get_storage_slots_count(3) == 0);

   BOOST_CHECK(test_contract_address == find_account_by_id(3).value().address);

   // Call 'TestContract::set_foo(1234)'
   txn = generate_tx(test_contract_address, 0, 1'000'000);
   data.clear();
   data += evmc::from_hex("e5d5dfbc").value(); //set_foo
   data += evmc::from_hex(int_str32(1234)).value(); //val=1234
   txn.data = data;
   evm1.sign(txn);
   pushtx(txn);

   BOOST_CHECK(get_storage_slots_count(3) == 1);

   // Call gc(100)
   gc(100);
   BOOST_CHECK(get_storage_slots_count(2) == 0);


} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(transition_from_0_5_1, account_id_tester) try {

   // Set old code
   set_code(evm_account_name, testing::contracts::evm_runtime_wasm_0_5_1());
   set_abi(evm_account_name, testing::contracts::evm_runtime_abi_0_5_1().data());

   BOOST_CHECK(!get_max_account_id().has_value());

   // Fund evm1 address with 100 EOS
   evm_eoa evm1;
   const int64_t to_bridge = 1000000;
   transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), evm1.address_0x());
   BOOST_CHECK(get_max_account_id().value() == 0);

   // Deploy Factory and Test contracts
   auto contract_addr = deploy_contract(evm1, evmc::from_hex(factory_and_test_bytecode).value());
   uint64_t contract_account_id = find_account_by_address(contract_addr).value().id;
   BOOST_CHECK(get_max_account_id().value() == 1);

   // Call 'Factory::deploy'
   auto txn = generate_tx(contract_addr, 0, 1'000'000);

   silkworm::Bytes data;
   data += evmc::from_hex("2b85ba38").value();     //deploy
   data += evmc::from_hex(int_str32(555)).value(); //salt=555
   txn.data = data;
   evm1.sign(txn);
   pushtx(txn);

   BOOST_CHECK(get_max_account_id().value() == 2);

   // Recover TestContract address
   auto test_contract_address = find_account_by_id(2).value().address;

   // Call 'TestContract::set_foo(1234)'
   txn = generate_tx(test_contract_address, 0, 1'000'000);
   data.clear();
   data += evmc::from_hex("e5d5dfbc").value(); //set_foo
   data += evmc::from_hex(int_str32(1234)).value(); //val=1234
   txn.data = data;
   evm1.sign(txn);
   pushtx(txn);

   BOOST_CHECK(get_storage_slots_count(2) == 1);

   // Call 'TestContract::killme'
   txn = generate_tx(test_contract_address, 0, 1'000'000);
   data.clear();
   data += evmc::from_hex("24d97a4a").value(); //killme
   txn.data = data;
   evm1.sign(txn);
   pushtx(txn);

   BOOST_CHECK(get_storage_slots_count(2) == 1);
   BOOST_CHECK(get_max_account_id().value() == 1);

   // Make a transfer to a new address to create a new account
   evm_eoa evm2;
   transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), evm2.address_0x());
   BOOST_CHECK(get_max_account_id().value() == 2);

   // Set new code
   set_code(evm_account_name, testing::contracts::evm_runtime_wasm());
   set_abi(evm_account_name, testing::contracts::evm_runtime_abi().data());

   BOOST_REQUIRE_EXCEPTION(get_config2(), fc::out_of_range_exception, [](const fc::out_of_range_exception& e) {return true;});

   // Call 'Factory::deploy' again
   txn = generate_tx(contract_addr, 0, 1'000'000);
   data.clear();
   data += evmc::from_hex("2b85ba38").value();     //deploy
   data += evmc::from_hex(int_str32(555)).value(); //salt=555
   txn.data = data;
   evm1.sign(txn);
   pushtx(txn);

   BOOST_CHECK(get_max_account_id().value() == 3);
   BOOST_CHECK(get_config2().next_account_id == 4);

   // Make a transfer to a new address to create a new account
   evm_eoa evm3;
   transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), evm3.address_0x());
   BOOST_CHECK(get_max_account_id().value() == 4);
   BOOST_CHECK(get_config2().next_account_id == 5);

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
