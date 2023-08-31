#include "basic_evm_tester.hpp"
#include <silkworm/execution/address.hpp>

using intx::operator""_u256;

using namespace evm_test;
using eosio::testing::eosio_assert_message_is;
struct exec_output_row {
  uint64_t    id;
  exec_output output;
};
FC_REFLECT(exec_output_row, (id)(output))

struct call_evm_tester : basic_evm_tester {
    /*
      //SPDX-License-Identifier: lgplv3
      pragma solidity ^0.8.0;

      contract Test {
        uint256 public count;
        address public lastcaller;

        function test(uint256 input) public {
            require(input != 0);
            
            count += input;
            lastcaller = msg.sender;
        }

        function testpay() payable public {
            
        }

        function notpayable() public {
            
        }

      }
      */
    // Cost for first time call to test(), extra cost is needed for the lastcaller storage.
    const intx::uint256 gas_fee = suggested_gas_price * 63526;
    // Cost for other calls to test()
    const intx::uint256 gas_fee2 = suggested_gas_price * 29326;
    // Cost for calls to testpay()
    const intx::uint256 gas_fee_testpay = suggested_gas_price * 21206;
    // Cost for calls to notpayable with 0 value
    const intx::uint256 gas_fee_notpayable = suggested_gas_price * 21274;
    
    const std::string contract_bytecode = 
          "608060405234801561001057600080fd5b5061030f806100206000396000f3fe60806040526004361061004a5760003560e01c806306661abd1461004f57806329e99f071461007a578063a1a7d817146100a3578063d097e7a6146100ad578063d79e1b6a146100d8575b600080fd5b34801561005b57600080fd5b506100646100ef565b60405161007191906101a1565b60405180910390f35b34801561008657600080fd5b506100a1600480360381019061009c91906101ed565b6100f5565b005b6100ab61015e565b005b3480156100b957600080fd5b506100c2610160565b6040516100cf919061025b565b60405180910390f35b3480156100e457600080fd5b506100ed610186565b005b60005481565b6000810361010257600080fd5b8060008082825461011391906102a5565b9250508190555033600160006101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff16021790555050565b565b600160009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1681565b565b6000819050919050565b61019b81610188565b82525050565b60006020820190506101b66000830184610192565b92915050565b600080fd5b6101ca81610188565b81146101d557600080fd5b50565b6000813590506101e7816101c1565b92915050565b600060208284031215610203576102026101bc565b5b6000610211848285016101d8565b91505092915050565b600073ffffffffffffffffffffffffffffffffffffffff82169050919050565b60006102458261021a565b9050919050565b6102558161023a565b82525050565b6000602082019050610270600083018461024c565b92915050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052601160045260246000fd5b60006102b082610188565b91506102bb83610188565b92508282019050808211156102d3576102d2610276565b5b9291505056fea2646970667358221220ed95d8f74110a8eb6307b7ae52b8623fd3e959169b208830a960c99a9ba1dbf564736f6c63430008120033";

    call_evm_tester() {
      create_accounts({"alice"_n});
      transfer_token(faucet_account_name, "alice"_n, make_asset(10000'0000));
      create_accounts({"bob"_n});
      transfer_token(faucet_account_name, "bob"_n, make_asset(10000'0000));
      init();
    }

    evmc::address deploy_test_contract(evm_eoa& eoa) {
      // Deploy token contract
      return deploy_contract(eoa, evmc::from_hex(contract_bytecode).value());
    }

    void call_test(const evmc::address& contract_addr, uint64_t amount, name eos, name actor) {

      auto to = evmc::bytes{std::begin(contract_addr.bytes), std::end(contract_addr.bytes)};

      silkworm::Bytes data;
      data += evmc::from_hex("29e99f07").value();   // sha3(test(uint256))[:4]
      data += evmc::bytes32{amount};                // value

      evmc::bytes32 v;
      intx::be::store(v.bytes, intx::uint256(0));

      call(eos, to, silkworm::Bytes(v), data, 500000, actor);
    }

    void call_testpay(const evmc::address& contract_addr, uint128_t amount, name eos, name actor) {

      auto to = evmc::bytes{std::begin(contract_addr.bytes), std::end(contract_addr.bytes)};

      silkworm::Bytes data;
      data += evmc::from_hex("a1a7d817").value();   // sha3(testpay())[:4]

      evmc::bytes32 v;
      intx::be::store(v.bytes, intx::uint256(intx::uint128(amount)));

      call(eos, to, silkworm::Bytes(v), data, 500000, actor);
    }

    void call_notpayable(const evmc::address& contract_addr, uint128_t amount, name eos, name actor) {

      auto to = evmc::bytes{std::begin(contract_addr.bytes), std::end(contract_addr.bytes)};

      silkworm::Bytes data;
      data += evmc::from_hex("d79e1b6a").value();   // sha3(notpayable())[:4]

      evmc::bytes32 v;
      intx::be::store(v.bytes, intx::uint256(intx::uint128(amount)));

      call(eos, to, silkworm::Bytes(v), data, 500000, actor);
    }

  void admincall_testpay(const evmc::address& contract_addr, uint128_t amount, evm_eoa& eoa, name actor) {

      auto to = evmc::bytes{std::begin(contract_addr.bytes), std::end(contract_addr.bytes)};
      auto from = evmc::bytes{std::begin(eoa.address.bytes), std::end(eoa.address.bytes)};

      silkworm::Bytes data;
      data += evmc::from_hex("a1a7d817").value();   // sha3(testpay())[:4]

      evmc::bytes32 v;
      intx::be::store(v.bytes, intx::uint256(intx::uint128(amount)));

      admincall(from, to, silkworm::Bytes(v), data, 500000, actor);
    }

    void admincall_notpayable(const evmc::address& contract_addr, uint128_t amount, evm_eoa& eoa, name actor) {

      auto to = evmc::bytes{std::begin(contract_addr.bytes), std::end(contract_addr.bytes)};
      auto from = evmc::bytes{std::begin(eoa.address.bytes), std::end(eoa.address.bytes)};

      silkworm::Bytes data;
      data += evmc::from_hex("d79e1b6a").value();   // sha3(notpayable())[:4]

      evmc::bytes32 v;
      intx::be::store(v.bytes, intx::uint256(intx::uint128(amount)));

      admincall(from, to, silkworm::Bytes(v), data, 500000, actor);
    }

    void admincall_test(const evmc::address& contract_addr, uint64_t amount, evm_eoa& eoa, name actor) {
      auto to = evmc::bytes{std::begin(contract_addr.bytes), std::end(contract_addr.bytes)};
      auto from = evmc::bytes{std::begin(eoa.address.bytes), std::end(eoa.address.bytes)};
      silkworm::Bytes data;
      data += evmc::from_hex("29e99f07").value();   // sha3(test(uint256))[:4]
      data += evmc::bytes32{amount};                // value

      evmc::bytes32 v;
      intx::be::store(v.bytes, intx::uint256(0));

      admincall(from, to, silkworm::Bytes(v), data, 500000, actor);
    }

    intx::uint256 get_count(const evmc::address& contract_addr, std::optional<exec_callback> callback={}, std::optional<bytes> context={}) {
      exec_input input;
      input.context = context;
      input.to = bytes{std::begin(contract_addr.bytes), std::end(contract_addr.bytes)};

      silkworm::Bytes data;
      data += evmc::from_hex("06661abd").value();   // sha3(count())[:4]
      input.data = bytes{data.begin(), data.end()};

      auto res =  exec(input, callback);

      BOOST_REQUIRE(res);
      BOOST_REQUIRE(res->action_traces.size() == 1);

      // Since callback information was not provided the result of the
      // execution is returned in the action return_value
      auto out = fc::raw::unpack<exec_output>(res->action_traces[0].return_value);
      BOOST_REQUIRE(out.status == 0);
      BOOST_REQUIRE(out.data.size() == 32);

      auto result = intx::be::unsafe::load<intx::uint256>(reinterpret_cast<const uint8_t*>(out.data.data()));
      return result;
    }

    evmc::address get_lastcaller(const evmc::address& contract_addr, std::optional<exec_callback> callback={}, std::optional<bytes> context={}) {
      exec_input input;
      input.context = context;
      input.to = bytes{std::begin(contract_addr.bytes), std::end(contract_addr.bytes)};

      silkworm::Bytes data;
      data += evmc::from_hex("d097e7a6").value();   // sha3(lastcaller())[:4]
      input.data = bytes{data.begin(), data.end()};

      auto res = exec(input, callback);

      BOOST_REQUIRE(res);
      BOOST_REQUIRE(res->action_traces.size() == 1);

      // Since callback information was not provided the result of the
      // execution is returned in the action return_value
      auto out = fc::raw::unpack<exec_output>(res->action_traces[0].return_value);
      BOOST_REQUIRE(out.status == 0);

      BOOST_REQUIRE(out.data.size() >= silkworm::kAddressLength);

      evmc::address result;
      memcpy(result.bytes, out.data.data()+out.data.size() - silkworm::kAddressLength, silkworm::kAddressLength);
      return result;
    }
};

BOOST_AUTO_TEST_SUITE(call_evm_tests)
BOOST_FIXTURE_TEST_CASE(call_test_function, call_evm_tester) try {
  evm_eoa evm1;
  auto total_fund = intx::uint256(vault_balance(evm_account_name));
  // Fund evm1 address with 100 EOS
  transfer_token("alice"_n, evm_account_name, make_asset(1000000), evm1.address_0x());
  auto evm1_balance = evm_balance(evm1);
  BOOST_REQUIRE(!!evm1_balance);
  BOOST_REQUIRE(*evm1_balance == 100_ether);
  total_fund += *evm1_balance;

  // Deploy contract
  auto token_addr = deploy_test_contract(evm1);

  // Deployment gas fee go to evm vault
  BOOST_REQUIRE(*evm_balance(evm1) + intx::uint256(vault_balance(evm_account_name)) == total_fund);

  // Missing authority
  BOOST_REQUIRE_EXCEPTION(call_test(token_addr, 1234, "alice"_n, "bob"_n),
                          missing_auth_exception, eosio::testing::fc_exception_message_starts_with("missing authority"));

  // Account not opened
  BOOST_REQUIRE_EXCEPTION(call_test(token_addr, 1234, "alice"_n, "alice"_n),
                          eosio_assert_message_exception, eosio_assert_message_is("caller account has not been opened"));

  // Open
  open("alice"_n);

  // No sufficient funds in the account so decrementing of balance failed.
  BOOST_REQUIRE_EXCEPTION(call_test(token_addr, 1234, "alice"_n, "alice"_n),
                          eosio_assert_message_exception, eosio_assert_message_is("decrementing more than available"));

  // Transfer enough funds, save initial balance value.
  transfer_token("alice"_n, evm_account_name, make_asset(1000000), "alice");
  auto alice_balance = 100_ether;
  BOOST_REQUIRE(intx::uint256(vault_balance("alice"_n)) == alice_balance);
  auto evm_account_balance = intx::uint256(vault_balance(evm_account_name));

  BOOST_REQUIRE_EXCEPTION(call_test(token_addr, 0, "alice"_n, "alice"_n),
                          eosio_assert_message_exception, eosio_assert_message_is("tx executed inline by contract must succeed"));

  BOOST_REQUIRE(intx::uint256(vault_balance("alice"_n)) == alice_balance);
  BOOST_REQUIRE(intx::uint256(vault_balance(evm_account_name)) == evm_account_balance);

  // Call and check results 
  call_test(token_addr, 1234, "alice"_n, "alice"_n);
  auto count = get_count(token_addr);
  BOOST_REQUIRE(count == 1234);

  alice_balance -= gas_fee;
  evm_account_balance += gas_fee;

  BOOST_REQUIRE(intx::uint256(vault_balance("alice"_n)) == alice_balance);
  BOOST_REQUIRE(intx::uint256(vault_balance(evm_account_name)) == evm_account_balance);

  // Advance block so we do not generate same transaction.
  produce_block();

  call_test(token_addr, 4321, "alice"_n, "alice"_n);
  count = get_count(token_addr);
  BOOST_REQUIRE(count == 5555);

  alice_balance -= gas_fee2;
  evm_account_balance += gas_fee2;

  BOOST_REQUIRE(intx::uint256(vault_balance("alice"_n)) == alice_balance);
  BOOST_REQUIRE(intx::uint256(vault_balance(evm_account_name)) == evm_account_balance);
  BOOST_REQUIRE(*evm_balance(token_addr) == 0);

  // Function being called on behalf of reserved address of eos account "alice"
  auto caller = get_lastcaller(token_addr);
  BOOST_REQUIRE(caller == make_reserved_address("alice"_n.to_uint64_t()));


  BOOST_REQUIRE_EXCEPTION(call_notpayable(token_addr, 100, "alice"_n, "alice"_n),
                          eosio_assert_message_exception, eosio_assert_message_is("tx executed inline by contract must succeed"));

  BOOST_REQUIRE(intx::uint256(vault_balance("alice"_n)) == alice_balance);
  BOOST_REQUIRE(intx::uint256(vault_balance(evm_account_name)) == evm_account_balance);
  BOOST_REQUIRE(*evm_balance(token_addr) == 0);

  call_notpayable(token_addr, 0, "alice"_n, "alice"_n);

  alice_balance -= gas_fee_notpayable;
  evm_account_balance += gas_fee_notpayable;

  BOOST_REQUIRE(intx::uint256(vault_balance("alice"_n)) == alice_balance);
  BOOST_REQUIRE(intx::uint256(vault_balance(evm_account_name)) == evm_account_balance);
  BOOST_REQUIRE(*evm_balance(token_addr) == 0);

  call_testpay(token_addr, 0, "alice"_n, "alice"_n);

  alice_balance -= gas_fee_testpay;
  evm_account_balance += gas_fee_testpay;

  BOOST_REQUIRE(intx::uint256(vault_balance("alice"_n)) == alice_balance);
  BOOST_REQUIRE(intx::uint256(vault_balance(evm_account_name)) == evm_account_balance);
  BOOST_REQUIRE(*evm_balance(token_addr) == 0);

  call_testpay(token_addr, *((uint128_t*)intx::as_words(50_ether)), "alice"_n, "alice"_n);

  alice_balance -= gas_fee_testpay;
  alice_balance -= 50_ether;
  evm_account_balance += gas_fee_testpay;

  BOOST_REQUIRE(intx::uint256(vault_balance("alice"_n)) == alice_balance);
  BOOST_REQUIRE(intx::uint256(vault_balance(evm_account_name)) == evm_account_balance);
  BOOST_REQUIRE(*evm_balance(token_addr) == 50_ether);

  // Advance block so we do not generate same transaction.
  produce_block();

  // No enough gas
  BOOST_REQUIRE_EXCEPTION(call_testpay(token_addr, *((uint128_t*)intx::as_words(50_ether)), "alice"_n, "alice"_n),
                          eosio_assert_message_exception, eosio_assert_message_is("decrementing more than available"));
  BOOST_REQUIRE(intx::uint256(vault_balance("alice"_n)) == alice_balance);
  BOOST_REQUIRE(intx::uint256(vault_balance(evm_account_name)) == evm_account_balance);
  BOOST_REQUIRE(*evm_balance(token_addr) == 50_ether);

   call_testpay(token_addr, *((uint128_t*)intx::as_words(10_ether)), "alice"_n, "alice"_n);   

  alice_balance -= gas_fee_testpay;
  alice_balance -= 10_ether;
  evm_account_balance += gas_fee_testpay;

  BOOST_REQUIRE(intx::uint256(vault_balance("alice"_n)) == alice_balance);
  BOOST_REQUIRE(intx::uint256(vault_balance(evm_account_name)) == evm_account_balance);                
  BOOST_REQUIRE(*evm_balance(token_addr) == 60_ether);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(admincall_test_function, call_evm_tester) try {
  evm_eoa evm1;
  evm_eoa evm2;

  auto total_fund = intx::uint256(vault_balance(evm_account_name));

  // Fund evm1 address with 100 EOS
  transfer_token("alice"_n, evm_account_name, make_asset(1000000), evm1.address_0x());

  auto evm1_balance = evm_balance(evm1);
  BOOST_REQUIRE(!!evm1_balance);
  BOOST_REQUIRE(*evm1_balance == 100_ether);
  total_fund += *evm1_balance;

  // Deploy contract
  auto token_addr = deploy_test_contract(evm1);

  // Deployment gas fee go to evm vault
  BOOST_REQUIRE(*evm_balance(evm1) + intx::uint256(vault_balance(evm_account_name)) == total_fund);

  // Missing authority
  BOOST_REQUIRE_EXCEPTION(admincall_test(token_addr, 1234, evm2, "alice"_n),
                          missing_auth_exception, eosio::testing::fc_exception_message_starts_with("missing authority"));

  // Account not created
  BOOST_REQUIRE_EXCEPTION( admincall_test(token_addr, 1234, evm2, evm_account_name),
                          eosio_assert_message_exception, eosio_assert_message_is("invalid address"));

  // Transfer small amount to create account
  transfer_token("alice"_n, evm_account_name, make_asset(100), evm2.address_0x());

  auto tb = evm_balance(evm2);
  BOOST_REQUIRE(!!tb);
  BOOST_REQUIRE(*tb == 10_finney);

  // Insufficient funds
  BOOST_REQUIRE_EXCEPTION( admincall_test(token_addr, 1234, evm2, evm_account_name),
                          eosio_assert_message_exception, eosio_assert_message_is("validate_transaction error: 23 Insufficient funds"));

  // Transfer enough funds, save initial balance
  transfer_token("alice"_n, evm_account_name, make_asset(999900), evm2.address_0x());
  auto evm2_balance = 100_ether;
  BOOST_REQUIRE(evm_balance(evm2) == evm2_balance);
  auto evm_account_balance = intx::uint256(vault_balance(evm_account_name));

  BOOST_REQUIRE_EXCEPTION(admincall_test(token_addr, 0, evm2, evm_account_name),
                          eosio_assert_message_exception, eosio_assert_message_is("tx executed inline by contract must succeed"));


  // Call and check results  
  admincall_test(token_addr, 1234, evm2, evm_account_name);

  auto count = get_count(token_addr);
  BOOST_REQUIRE(count == 1234);

  evm2_balance -= gas_fee;
  evm_account_balance += gas_fee;

  BOOST_REQUIRE(evm_balance(evm2) == evm2_balance);
  BOOST_REQUIRE(intx::uint256(vault_balance(evm_account_name)) == evm_account_balance);

  // Advance block so we do not generate same transaction.
  produce_block();

  admincall_test(token_addr, 4321, evm2, evm_account_name);
  count = get_count(token_addr);
  BOOST_REQUIRE(count == 5555);

  evm2_balance -= gas_fee2;
  evm_account_balance += gas_fee2;

  BOOST_REQUIRE(evm_balance(evm2) == evm2_balance);
  BOOST_REQUIRE(intx::uint256(vault_balance(evm_account_name)) == evm_account_balance);

  // Function being called on behalf of evm address "evm2"
  auto caller = get_lastcaller(token_addr);
  BOOST_REQUIRE(caller== evm2.address);
  
  BOOST_REQUIRE_EXCEPTION(admincall_notpayable(token_addr, 100, evm2, evm_account_name),
                          eosio_assert_message_exception, eosio_assert_message_is("tx executed inline by contract must succeed"));

  BOOST_REQUIRE(evm_balance(evm2)== evm2_balance);
  BOOST_REQUIRE(intx::uint256(vault_balance(evm_account_name)) == evm_account_balance);
  BOOST_REQUIRE(*evm_balance(token_addr) == 0);

  admincall_notpayable(token_addr, 0, evm2, evm_account_name);

  evm2_balance -= gas_fee_notpayable;
  evm_account_balance += gas_fee_notpayable;

  BOOST_REQUIRE(evm_balance(evm2) == evm2_balance);
  BOOST_REQUIRE(intx::uint256(vault_balance(evm_account_name)) == evm_account_balance);
  BOOST_REQUIRE(*evm_balance(token_addr) == 0);

  admincall_testpay(token_addr, 0, evm2, evm_account_name);

  evm2_balance -= gas_fee_testpay;
  evm_account_balance += gas_fee_testpay;

  BOOST_REQUIRE(evm_balance(evm2) == evm2_balance);
  BOOST_REQUIRE(intx::uint256(vault_balance(evm_account_name)) == evm_account_balance);
  BOOST_REQUIRE(*evm_balance(token_addr) == 0);

  admincall_testpay(token_addr, *((uint128_t*)intx::as_words(50_ether)), evm2, evm_account_name);

  evm2_balance -= gas_fee_testpay;
  evm2_balance -= 50_ether;
  evm_account_balance += gas_fee_testpay;

  BOOST_REQUIRE(evm_balance(evm2)== evm2_balance);
  BOOST_REQUIRE(intx::uint256(vault_balance(evm_account_name)) == evm_account_balance);
  BOOST_REQUIRE(*evm_balance(token_addr) == 50_ether);

  // Advance block so we do not generate same transaction.
  produce_block();

  // No enough gas
  BOOST_REQUIRE_EXCEPTION(admincall_testpay(token_addr, *((uint128_t*)intx::as_words(50_ether)), evm2, evm_account_name),
                          eosio_assert_message_exception, eosio_assert_message_is("validate_transaction error: 23 Insufficient funds"));
  BOOST_REQUIRE(evm_balance(evm2) == evm2_balance);
  BOOST_REQUIRE(intx::uint256(vault_balance(evm_account_name)) == evm_account_balance);
  BOOST_REQUIRE(*evm_balance(token_addr) == 50_ether);

  admincall_testpay(token_addr, *((uint128_t*)intx::as_words(10_ether)), evm2, evm_account_name);   

  evm2_balance -= gas_fee_testpay;
  evm2_balance -= 10_ether;
  evm_account_balance += gas_fee_testpay;

  BOOST_REQUIRE(evm_balance(evm2) == evm2_balance);
  BOOST_REQUIRE(intx::uint256(vault_balance(evm_account_name)) == evm_account_balance);                
  BOOST_REQUIRE(*evm_balance(token_addr) == 60_ether);

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(deploy_contract_function, call_evm_tester) try {
  auto alice_addr = make_reserved_address("alice"_n.to_uint64_t());
  open("alice"_n);
  transfer_token("alice"_n, evm_account_name, make_asset(1000000), "alice");
  
  
  evmc::bytes32 v;

  auto to = evmc::bytes();

  auto data = evmc::from_hex(contract_bytecode);
  assertnonce("alice"_n, 0);
  call("alice"_n, to, silkworm::Bytes(v), *data, 1000000, "alice"_n); // nonce 0->1
  
  auto addr = silkworm::create_address(alice_addr, 0); 
  assertnonce("alice"_n, 1);
  call_test(addr, 1234, "alice"_n, "alice"_n); // nonce 1->2
  
  auto count = get_count(addr);
  BOOST_REQUIRE(count == 1234);

  // Advance block so we do not generate same transaction.
  produce_block();

  auto from = evmc::bytes{std::begin(alice_addr.bytes), std::end(alice_addr.bytes)};
  assertnonce("alice"_n, 2);
  admincall(from, to, silkworm::Bytes(v), *data, 1000000, evm_account_name); // nonce 2->3

  addr = silkworm::create_address(alice_addr, 2); 
  assertnonce("alice"_n, 3);
  call_test(addr, 2222, "alice"_n, "alice"_n); // nonce 3->4
  assertnonce("alice"_n, 4);
  count = get_count(addr);
  BOOST_REQUIRE(count == 2222);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(assetnonce_test, call_evm_tester) try {
  auto alice_addr = make_reserved_address("alice"_n.to_uint64_t());

  // nonce for not opened account is zero.
  assertnonce("alice"_n, 0);
  BOOST_REQUIRE_EXCEPTION(assertnonce("alice"_n, 1),
                          eosio_assert_message_exception, eosio_assert_message_is("wrong nonce"));

  // Advance block so we do not generate same transaction.
  produce_block();

  open("alice"_n);
  transfer_token("alice"_n, evm_account_name, make_asset(1000000), "alice");

  evm_eoa evm1;
  evmc::bytes32 v;

  auto to = evmc::bytes();

  auto data = evmc::from_hex(contract_bytecode);

  // Fail when nonce is 0 but tested with non-zero value
  BOOST_REQUIRE_EXCEPTION(assertnonce("alice"_n, 1),
                          eosio_assert_message_exception, eosio_assert_message_is("wrong nonce"));

  assertnonce("alice"_n, 0);
  call("alice"_n, to, silkworm::Bytes(v), *data, 1000000, "alice"_n); // nonce 0->1
  
  // Advance block so we do not generate same transaction.
  produce_block();

  assertnonce("alice"_n, 1);
  assertnonce(evm_account_name, 0);
  // Fund evm1 address with 100 EOS, should NOT increase alice nonce, but increase evm nonce.
  transfer_token("alice"_n, evm_account_name, make_asset(1000000), evm1.address_0x());

  // Advance block so we do not generate same transaction.
  produce_block();
  assertnonce("alice"_n, 1);
  assertnonce(evm_account_name, 1);

  // Advance block so we do not generate same transaction.
  produce_block();

  // Fail when nonce is non-zero but tested with another value
  BOOST_REQUIRE_EXCEPTION(assertnonce("alice"_n, 2),
                          eosio_assert_message_exception, eosio_assert_message_is("wrong nonce"));
  // Fail when nonce is non-zero but tested with 0
  BOOST_REQUIRE_EXCEPTION(assertnonce("alice"_n, 0),
                          eosio_assert_message_exception, eosio_assert_message_is("wrong nonce"));

  

} FC_LOG_AND_RETHROW()



BOOST_AUTO_TEST_SUITE_END()