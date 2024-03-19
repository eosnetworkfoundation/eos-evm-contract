#include <boost/test/unit_test.hpp>

#include "basic_evm_tester.hpp"
#include <silkworm/core/execution/address.hpp>
#include "utils.hpp"

using namespace evm_test;
using eosio::testing::eosio_assert_message_is;
using eosio::testing::expect_assert_message;

struct vault_balance_row;

struct admin_action_tester : basic_evm_tester {
   admin_action_tester() {
      create_accounts({"alice"_n});
      transfer_token(faucet_account_name, "alice"_n, make_asset(10000'0000));
      init();
   }

   intx::uint256 getval(const evmc::address& contract_addr) {
      exec_input input;
      input.context = {};
      input.to = bytes{std::begin(contract_addr.bytes), std::end(contract_addr.bytes)};

      silkworm::Bytes data;
      data += evmc::from_hex("31b6bd06").value();   // sha3(getval())[:4]
      input.data = bytes{data.begin(), data.end()};

      auto res = exec(input, {});
      BOOST_REQUIRE(res);
      BOOST_REQUIRE(res->action_traces.size() == 1);

      auto out = fc::raw::unpack<exec_output>(res->action_traces[0].return_value);
      BOOST_REQUIRE(out.status == 0);
      BOOST_REQUIRE(out.data.size() == 32);
      auto val = intx::be::unsafe::load<intx::uint256>(reinterpret_cast<const uint8_t*>(out.data.data()));
      
      dlog("GETVAL: ${v}", ("v", static_cast<uint64_t>(val)));
      return val;
   }

   auto deploy_simple_contract(evm_eoa& evm_account) {

      // // SPDX-License-Identifier: GPL-3.0
      // pragma solidity >=0.7.0 <0.9.0;
      // contract Simple {
      //    uint256 val;
      //    address payable public owner;
      //    constructor() {
      //       owner = payable(msg.sender);
      //    }
      //    function setval(uint256 v) public {
      //       val=v;
      //    }
      //    function getval() public view returns (uint256) {
      //       return val;
      //    }
      //    function killme() public {
      //       selfdestruct(owner);
      //    }
      // }

      const std::string simple_bytecode = "608060405234801561001057600080fd5b5033600160006101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff16021790555061024b806100616000396000f3fe608060405234801561001057600080fd5b506004361061004c5760003560e01c806324d97a4a1461005157806331b6bd061461005b578063559c9c4a146100795780638da5cb5b14610095575b600080fd5b6100596100b3565b005b6100636100ee565b6040516100709190610140565b60405180910390f35b610093600480360381019061008e919061018c565b6100f7565b005b61009d610101565b6040516100aa91906101fa565b60405180910390f35b600160009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16ff5b60008054905090565b8060008190555050565b600160009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1681565b6000819050919050565b61013a81610127565b82525050565b60006020820190506101556000830184610131565b92915050565b600080fd5b61016981610127565b811461017457600080fd5b50565b60008135905061018681610160565b92915050565b6000602082840312156101a2576101a161015b565b5b60006101b084828501610177565b91505092915050565b600073ffffffffffffffffffffffffffffffffffffffff82169050919050565b60006101e4826101b9565b9050919050565b6101f4816101d9565b82525050565b600060208201905061020f60008301846101eb565b9291505056fea26469706673582212204abac6746f4497f12044fdf4568905d560328e27fa03b3b954fc303865b8429764736f6c63430008120033";

      // Deploy simple contract
      auto contract_addr = deploy_contract(evm_account, evmc::from_hex(simple_bytecode).value());
      uint64_t contract_account_id = find_account_by_address(contract_addr).value().id;

      return std::make_tuple(contract_addr, contract_account_id);
   }

   size_t total_gcrows() {
      size_t total=0;
      scan_gcstore([&total](evm_test::gcstore row) -> bool {
         ++total;
         return false;
      });
      return total;
   };

   size_t total_evm_accounts() {
      size_t total=0;
      scan_accounts([&total](evm_test::account_object) -> bool {
         ++total;
         return false;
      });
      return total;
   };

   size_t total_account_code() {
      size_t total=0;
      scan_account_code([&total](evm_test::account_code) -> bool {
         ++total;
         return false;
      });
      return total;
   }

};

BOOST_AUTO_TEST_SUITE(admin_actions_tests)
BOOST_FIXTURE_TEST_CASE(rmgcstore_tests, admin_action_tester) try {

   // Fund evm1 address with 100 EOS
   evm_eoa evm1;
   const int64_t to_bridge = 1000000;
   transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), evm1.address_0x());

   auto [contract_addr, contract_account_id] = deploy_simple_contract(evm1);

   BOOST_REQUIRE(total_gcrows() == 0);
   BOOST_REQUIRE(total_evm_accounts() == 2);

   // Call method "killme" on simple contract (sha3('killme()') = 0x24d97a4a)
   auto txn = generate_tx(contract_addr, 0, 500'000);
   txn.data = evmc::from_hex("0x24d97a4a").value();
   evm1.sign(txn);
   pushtx(txn);

   BOOST_REQUIRE(total_gcrows() == 1);
   BOOST_REQUIRE(total_evm_accounts() == 1);
   
   auto gcs = get_gcstore(0);
   BOOST_REQUIRE(gcs.storage_id == contract_account_id);

   BOOST_REQUIRE_EXCEPTION(rmgcstore(0, "alice"_n),
      missing_auth_exception, eosio::testing::fc_exception_message_starts_with("missing authority"));

   rmgcstore(0, evm_account_name);

   BOOST_REQUIRE(total_gcrows() == 0);
   BOOST_REQUIRE(total_evm_accounts() == 1);

   produce_blocks(5);
   BOOST_REQUIRE_EXCEPTION(rmgcstore(0, evm_account_name),
      eosio_assert_message_exception, eosio_assert_message_is("gc row not found"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(setkvstore_tests, admin_action_tester) try {

   // Fund evm1 address with 100 EOS
   evm_eoa evm1;
   const int64_t to_bridge = 1000000;
   transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), evm1.address_0x());

   evmc::address contract_addr;
   uint64_t contract_account_id;
   std::tie(contract_addr, contract_account_id) = deploy_simple_contract(evm1);

   // Call method "setval" on simple contract (sha3('setval(uint256)') = 0x559c9c4a)
   auto txn = generate_tx(contract_addr, 0, 500'000);
   txn.data = evmc::from_hex("0x559c9c4a").value();
   txn.data += evmc::from_hex("0x0000000000000000000000000000000000000000000000000000000000000042").value();
   evm1.sign(txn);
   pushtx(txn);

   std::map<uint64_t, intx::uint256> slots;
   auto load_slots = [&]() {
      slots.clear();
      scan_account_storage(contract_account_id, [&](storage_slot&& slot) -> bool {
         auto key_u64 = static_cast<uint64_t>(slot.key);
         slots[key_u64] = slot.value;
         return false;
      });
   };

   load_slots();
   BOOST_REQUIRE(slots.size() == 2);
   BOOST_REQUIRE(slots[0] == intx::uint256(66));
   BOOST_REQUIRE(getval(contract_addr) == intx::uint256(66));

   BOOST_REQUIRE_EXCEPTION(setkvstore(contract_account_id, to_bytes(intx::uint256(0)), to_bytes(intx::uint256(55)), "alice"_n),
      missing_auth_exception, eosio::testing::fc_exception_message_starts_with("missing authority"));

   BOOST_REQUIRE_EXCEPTION(setkvstore(contract_account_id, {}, to_bytes(intx::uint256(55))),
      eosio_assert_message_exception, eosio_assert_message_is("invalid key/value size"));

   BOOST_REQUIRE_EXCEPTION(setkvstore(contract_account_id, to_bytes(intx::uint256(0)), bytes{}),
      eosio_assert_message_exception, eosio_assert_message_is("invalid key/value size"));

   setkvstore(contract_account_id, to_bytes(intx::uint256(0)), to_bytes(intx::uint256(55)));

   produce_blocks(5);

   load_slots();
   BOOST_REQUIRE(slots.size() == 2);
   BOOST_REQUIRE(slots[0] == intx::uint256(55));
   BOOST_REQUIRE(getval(contract_addr) == intx::uint256(55));

   BOOST_REQUIRE_EXCEPTION(setkvstore(contract_account_id, to_bytes(intx::uint256(111)), {}),
      eosio_assert_message_exception, eosio_assert_message_is("key not found"));

   setkvstore(contract_account_id, to_bytes(intx::uint256(0)), {});
   produce_blocks(5);

   load_slots();
   BOOST_REQUIRE(slots.size() == 1);
   BOOST_REQUIRE(slots.find(0) == slots.end());
   BOOST_REQUIRE(getval(contract_addr) == intx::uint256(0));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(rmaccount_tests, admin_action_tester) try {

   // Fund evm1 address with 100 EOS
   evm_eoa evm1;
   const int64_t to_bridge = 1000000;
   transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), evm1.address_0x());

   auto [contract_addr, contract_account_id] = deploy_simple_contract(evm1);
   auto [contract_addr2, contract_account_id2] = deploy_simple_contract(evm1);
   
   BOOST_REQUIRE(total_evm_accounts() == 3);
   BOOST_REQUIRE(total_account_code() == 1);
   BOOST_REQUIRE(total_gcrows() == 0);

   BOOST_REQUIRE_EXCEPTION(rmaccount(contract_account_id, "alice"_n),
      missing_auth_exception, eosio::testing::fc_exception_message_starts_with("missing authority"));

   rmaccount(contract_account_id);
   BOOST_REQUIRE(total_evm_accounts() == 2);
   BOOST_REQUIRE(total_account_code() == 1);
   BOOST_REQUIRE(total_gcrows() == 1);
   BOOST_REQUIRE(get_gcstore(0).storage_id == contract_account_id);

   rmaccount(contract_account_id2);
   BOOST_REQUIRE(total_evm_accounts() == 1);
   BOOST_REQUIRE(total_account_code() == 0);
   BOOST_REQUIRE(total_gcrows() == 2);
   BOOST_REQUIRE(get_gcstore(0).storage_id == contract_account_id);
   BOOST_REQUIRE(get_gcstore(1).storage_id == contract_account_id2);

   produce_blocks(5);

   BOOST_REQUIRE_EXCEPTION(rmaccount(contract_account_id),
      eosio_assert_message_exception, eosio_assert_message_is("account not found"));

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(freezeaccnt_tests, admin_action_tester) try {

   // Fund evm1 address with 100 EOS
   evm_eoa evm1;
   const int64_t to_bridge = 1000000;
   transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), evm1.address_0x());

   auto evm1_account = find_account_by_address(evm1.address).value();
   BOOST_REQUIRE( evm1_account.has_flag(evm_test::account_object::flag::frozen) == false );

   BOOST_REQUIRE_EXCEPTION(freezeaccnt(evm1_account.id, true, "alice"_n),
      missing_auth_exception, eosio::testing::fc_exception_message_starts_with("missing authority"));

   freezeaccnt(evm1_account.id, true);
   evm1_account = find_account_by_address(evm1.address).value();

   BOOST_REQUIRE( evm1_account.has_flag(evm_test::account_object::flag::frozen) == true );

   BOOST_REQUIRE_EXCEPTION(deploy_simple_contract(evm1),
         eosio_assert_message_exception, eosio_assert_message_is("account is frozen"));
   evm1.next_nonce--;

   freezeaccnt(evm1_account.id, false);
   evm1_account = find_account_by_address(evm1.address).value();

   BOOST_REQUIRE( evm1_account.has_flag(evm_test::account_object::flag::frozen) == false );

   auto [contract_addr, contract_account_id] = deploy_simple_contract(evm1);
   freezeaccnt(contract_account_id, true);

   // We allow evm::exec action to work with frozen accounts
   BOOST_REQUIRE(getval(contract_addr) == 0);

   BOOST_REQUIRE_EXCEPTION(transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), fc::variant(contract_addr).as_string()),
         eosio_assert_message_exception, eosio_assert_message_is("account is frozen"));

   auto txn = generate_tx(contract_addr, 1_ether, 21'000);
   evm1.sign(txn);
   BOOST_REQUIRE_EXCEPTION(pushtx(txn),
         eosio_assert_message_exception, eosio_assert_message_is("account is frozen"));

   freezeaccnt(contract_account_id, false);
   pushtx(txn);

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(addevmbal_subtract_tests, admin_action_tester) try {

   // Fund evm1 address with 100 EOS
   evm_eoa evm1;
   const int64_t to_bridge = 1000000;
   transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), evm1.address_0x());
   auto evm1_account = find_account_by_address(evm1.address).value();

   BOOST_REQUIRE_EXCEPTION(addevmbal(evm1_account.id, 1_ether, true, "alice"_n),
      missing_auth_exception, eosio::testing::fc_exception_message_starts_with("missing authority"));

   BOOST_REQUIRE_EXCEPTION(addevmbal(2, 1_ether, true),
         eosio_assert_message_exception, eosio_assert_message_is("account not found"));

   auto b100_plus_one = 100_ether + 1;

   // Error when decrementing inevm
   BOOST_REQUIRE_EXCEPTION(addevmbal(evm1_account.id, b100_plus_one, true),
         eosio_assert_message_exception, eosio_assert_message_is("decrementing more than available"));

   // Fund evm2 address with 0.0001 EOS
   evm_eoa evm2;
   const int64_t to_bridge2 = 1;
   transfer_token("alice"_n, evm_account_name, make_asset(to_bridge2), evm2.address_0x());
   auto evm2_account = find_account_by_address(evm2.address).value();

   // inevm: 100 eth (evm1) + 10^14 wei (evm2)

   // Error when decrementing address balance (carry check)
   BOOST_REQUIRE_EXCEPTION(addevmbal(evm1_account.id, b100_plus_one, true),
         eosio_assert_message_exception, eosio_assert_message_is("underflow detected"));

   check_balances();

   //substract 10^14 wei to evm1
   intx::uint256 minimum_natively_representable = intx::exp(10_u256, intx::uint256(18 - 4));
   
   addevmbal(evm1_account.id, minimum_natively_representable, true);

   // This will fail since the eosio.token balance of `evm_account_name` is 0.0001 greater than the sum of all balances (evm + open balances)
   BOOST_REQUIRE_THROW(check_balances(), std::runtime_error);

   // Send 0.0001 EOS from `evm_account_name` to alice
   transfer_token(evm_account_name, "alice"_n, make_asset(1));

   check_balances();

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(addevmbal_add_tests, admin_action_tester) try {

   // Fund evm1 address with 100 EOS
   evm_eoa evm1;
   const int64_t to_bridge = 1000000;
   transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), evm1.address_0x());
   auto evm1_account = find_account_by_address(evm1.address).value();

   BOOST_REQUIRE_EXCEPTION(addevmbal(evm1_account.id, 1_ether, false, "alice"_n),
      missing_auth_exception, eosio::testing::fc_exception_message_starts_with("missing authority"));

   BOOST_REQUIRE_EXCEPTION(addevmbal(2, 1_ether, false),
         eosio_assert_message_exception, eosio_assert_message_is("account not found"));

   intx::uint256 minimum_natively_representable = intx::exp(10_u256, intx::uint256(18 - 4));

   //error when incrementing address balance (carry check)
   intx::uint256 v = std::numeric_limits<intx::uint256>::max() - to_bridge*minimum_natively_representable + 1;
   BOOST_REQUIRE_EXCEPTION(addevmbal(evm1_account.id, v, false),
         eosio_assert_message_exception, eosio_assert_message_is("overflow detected"));

   //error when incrementing inevm
   v = intx::uint256(asset::max_amount - to_bridge + 1) * minimum_natively_representable;
   BOOST_REQUIRE_EXCEPTION(addevmbal(evm1_account.id, v, false),
         eosio_assert_message_exception, eosio_assert_message_is("accumulation overflow"));

   check_balances();

   // add 0.0001 EOS to evm1
   v = 1 * minimum_natively_representable;

   // inevm: 100.0000 EOS
   // evm1: 100.0000 EOS
   // `evm_account_name` 1.0000 EOS
   // eosio.token balance: 101.0000 EOS

   addevmbal(evm1_account.id, v, false);

   // inevm: 100.0001 EOS
   // evm1: 100.0001 EOS
   // `evm_account_name` 1.0000 EOS
   // eosio.token balance: 101.0000 EOS

   // This will fail since the eosio.token balance of `evm_account_name` is 0.0001 less than the sum of all balances (evm + open balances)
   BOOST_REQUIRE_THROW(check_balances(), std::runtime_error);

   // send 0.0001 EOS from alice to `evm_account_name` (to be credited to `evm_account_name`)
   transfer_token("alice"_n, evm_account_name, make_asset(1), evm_account_name.to_string());

   // inevm: 100.0001 EOS
   // evm1: 100.0001 EOS
   // `evm_account_name` 1.0001 EOS
   // eosio.token balance: 101.0001 EOS

   // This will fail since the 0.0001 EOS sent in the previous transfer has been credited to:
   // - the open balance of `evm_account_name`
   // - the eosio.token balance of `evm_account_name`
   BOOST_REQUIRE_THROW(check_balances(), std::runtime_error);

   // We need to substract 0.0001 from the open balance of `evm_account_name`
   addopenbal(evm_account_name, minimum_natively_representable, true);

   check_balances();

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(addopenbal_tests, admin_action_tester) try {
   open("alice"_n);
   transfer_token("alice"_n, evm_account_name, make_asset(100'0000), "alice");

   intx::uint256 minimum_natively_representable = intx::exp(10_u256, intx::uint256(18 - 4));

   BOOST_REQUIRE_EXCEPTION(addopenbal("beto"_n, intx::uint256(100'0000)*minimum_natively_representable, false, "alice"_n),
      missing_auth_exception, eosio::testing::fc_exception_message_starts_with("missing authority"));

   BOOST_REQUIRE_EXCEPTION(addopenbal("beto"_n, intx::uint256(100'0000)*minimum_natively_representable, false),
         eosio_assert_message_exception, eosio_assert_message_is("account not found"));

   BOOST_REQUIRE_EXCEPTION(addopenbal("alice"_n, intx::uint256(100'0001)*minimum_natively_representable, true),
         eosio_assert_message_exception, eosio_assert_message_is("decrementing more than available"));

   BOOST_REQUIRE_EXCEPTION(addopenbal("alice"_n, std::numeric_limits<intx::uint256>::max()-intx::uint256(99'9999)*minimum_natively_representable, false),
         eosio_assert_message_exception, eosio_assert_message_is("accumulation overflow"));

   check_balances();

   addopenbal("alice"_n, minimum_natively_representable, true);
   BOOST_REQUIRE(intx::uint256(vault_balance("alice"_n)) == intx::uint256(99'9999)*minimum_natively_representable);

   // This will fail since the eosio.token balance of `evm_account_name` is 0.0001 greater than the sum of all balances (evm + open balances)
   BOOST_REQUIRE_THROW(check_balances(), std::runtime_error);

   // send 0.0001 EOS from `evm_account_name` to alice 
   transfer_token(evm_account_name, "alice"_n, make_asset(1));

   check_balances();

   addopenbal("alice"_n, minimum_natively_representable, false);
   BOOST_REQUIRE(intx::uint256(vault_balance("alice"_n)) == intx::uint256(100'0000)*minimum_natively_representable);

   // This will fail since the eosio.token balance of `evm_account_name` is 0.0001 less than the sum of all balances (evm + open balances)
   BOOST_REQUIRE_THROW(check_balances(), std::runtime_error);
   produce_blocks(1);

   // This combination is to adjust just the eosio.token EOS balance of the `evm_account_name`
   transfer_token("alice"_n, evm_account_name, make_asset(1), evm_account_name.to_string());
   addopenbal(evm_account_name, minimum_natively_representable, true);

   check_balances();

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()