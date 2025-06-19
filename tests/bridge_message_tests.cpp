#include "basic_evm_tester.hpp"
#include <silkworm/core/execution/address.hpp>

#include "utils.hpp"

using intx::operator""_u256;

using namespace evm_test;
using eosio::testing::eosio_assert_message_is;
using eosio::testing::expect_assert_message;

struct bridge_message_tester : basic_evm_tester {

    static constexpr const char* bridgeMsgV0_method_id = "f781185b";

    bridge_message_tester() {
      create_accounts({"alice"_n});
      transfer_token(faucet_account_name, "alice"_n, make_asset(10000'0000));
      init();
      open("alice"_n);
    }

   ~bridge_message_tester() {
      dlog("~bridge_message_tester()");
      check_balances();
    }

    std::string int_str32(uint32_t x) {
      std::stringstream hex_ss;
      hex_ss << std::hex << x; 
      int hex_length = hex_ss.str().length();

      std::stringstream ss;
      ss << std::setfill('0') << std::setw(64 - hex_length) << 0 << std::hex << std::uppercase << x;
      return ss.str();
    }

    std::string str_to_hex(const std::string& str) {
      std::stringstream ss;
      for (char c : str) {
          ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
      }
      return ss.str();
    }

    std::string data_str32(const std::string& str) {
      std::stringstream ss;
      ss << str;
      int ps = 64 - (str.length() % 64);
      if (ps == 64) {ps = 0;}
      ss << std::setw(ps) << std::setfill('0') << "";
      return ss.str();
    }

    transaction_trace_ptr send_bridge_message(evm_eoa& eoa, const std::string& receiver, const intx::uint256& value, const std::string& str_data) {

      silkworm::Bytes data;
      data += evmc::from_hex(bridgeMsgV0_method_id).value();
      data += evmc::from_hex(int_str32(96)).value();                     //offset param1 (receiver: string)
      data += evmc::from_hex(int_str32(1)).value();                      //param2 (true) (force_atomic: bool)
      data += evmc::from_hex(int_str32(160)).value();                    //offset param3 (data: bytes)
      data += evmc::from_hex(int_str32(receiver.length())).value();      //param1 length
      data += evmc::from_hex(data_str32(str_to_hex(receiver))).value();  //param1 data
      data += evmc::from_hex(int_str32(str_data.size()/2)).value();      //param3 length
      data += evmc::from_hex(data_str32(str_data)).value();              //param3 data

      return send_raw_message(eoa, make_reserved_address(evm_account_name), value, data);
    }

    transaction_trace_ptr send_raw_message(evm_eoa& eoa, const evmc::address& dest, const intx::uint256& value, const silkworm::Bytes& data) {
      auto txn = generate_tx(dest, value, 250'000);
      txn.data = data;
      eoa.sign(txn);
      return pushtx(txn, "alice"_n);
    }
};

BOOST_AUTO_TEST_SUITE(bridge_message_tests)
BOOST_FIXTURE_TEST_CASE(bridge_register_test, bridge_message_tester) try {

  create_accounts({"rec1"_n});

  // evm auth is needed
  BOOST_REQUIRE_EXCEPTION(bridgereg("rec1"_n, "rec1"_n, make_asset(-1), {}),
    missing_auth_exception, [](const missing_auth_exception& e) { return expect_assert_message(e, "missing authority of evm"); });

  // min fee only accepts EOS
  BOOST_REQUIRE_EXCEPTION(bridgereg("rec1"_n, "rec1"_n, asset::from_string("1.0000 TST")),
    eosio_assert_message_exception, eosio_assert_message_is("unexpected symbol"));

  // min fee must be non-negative
  BOOST_REQUIRE_EXCEPTION(bridgereg("rec1"_n, "rec1"_n, make_asset(-1)),
    eosio_assert_message_exception, eosio_assert_message_is("min_fee cannot be negative"));

  bridgereg("rec1"_n, "rec1"_n, make_asset(0));

  auto row = fc::raw::unpack<message_receiver>(get_row_by_account( evm_account_name, evm_account_name, "msgreceiver"_n, "rec1"_n));
  BOOST_REQUIRE(row.account == "rec1"_n);
  BOOST_REQUIRE(row.min_fee == make_asset(0));
  BOOST_REQUIRE(row.flags == 0x1);

  // Register again changing min fee
  bridgereg("rec1"_n, "rec1"_n, make_asset(1));

  row = fc::raw::unpack<message_receiver>(get_row_by_account( evm_account_name, evm_account_name, "msgreceiver"_n, "rec1"_n));
  BOOST_REQUIRE(row.account == "rec1"_n);
  BOOST_REQUIRE(row.min_fee == make_asset(1));
  BOOST_REQUIRE(row.flags == 0x1);

  // Unregister rec1
  bridgeunreg("rec1"_n);
  const auto& d = get_row_by_account( evm_account_name, evm_account_name, "msgreceiver"_n, "rec1"_n);
  BOOST_REQUIRE(d.size() == 0);
  produce_block();

  // Try to unregister again
  BOOST_REQUIRE_EXCEPTION(bridgeunreg("rec1"_n),
    eosio_assert_message_exception, eosio_assert_message_is("receiver not found"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(basic_tests, bridge_message_tester) try {

  // Create destination account
  create_accounts({"rec1"_n});
  set_code("rec1"_n, testing::contracts::evm_bridge_receiver_wasm());
  set_abi("rec1"_n, testing::contracts::evm_bridge_receiver_abi().data());

  // Register rec1 with 1000.0000 EOS as min_fee
  bridgereg("rec1"_n, "rec1"_n, make_asset(1000'0000));

  // Fund evm1 address with 1001 EOS
  evm_eoa evm1;
  const int64_t to_bridge = 1001'0000;
  transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), evm1.address_0x());

  // Check rec1 balance before sending the message
  BOOST_REQUIRE(vault_balance("rec1"_n) == (balance_and_dust{make_asset(0), 0}));

  // Emit message
  auto res = send_bridge_message(evm1, "rec1", 1000_ether, "0102030405060708090a");

  // Check rec1 balance after sending the message
  BOOST_REQUIRE(vault_balance("rec1"_n) == (balance_and_dust{make_asset(1000'0000), 0}));

  // Recover message form the return value of rec1 contract
  BOOST_CHECK(res->action_traces[1].receiver == "rec1"_n);
  auto msgout = fc::raw::unpack<bridge_message>(res->action_traces[1].return_value);
  auto out = std::get<bridge_message_v0>(msgout);

  BOOST_CHECK(out.receiver == "rec1"_n);
  BOOST_CHECK(out.sender == to_bytes(evm1.address));
  BOOST_CHECK(out.timestamp.time_since_epoch() == control->pending_block_time().time_since_epoch());
  BOOST_CHECK(out.value == to_bytes(1000_ether));
  BOOST_CHECK(out.data == to_bytes(evmc::from_hex("0102030405060708090a").value()));
} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(swap_tests, bridge_message_tester) try {

  // Create destination account
  create_accounts({"rec1"_n});
  set_code("rec1"_n, testing::contracts::evm_bridge_receiver_wasm());
  set_abi("rec1"_n, testing::contracts::evm_bridge_receiver_abi().data());

  // Register rec1 with 1000.0000 EOS as min_fee
  bridgereg("rec1"_n, "rec1"_n, make_asset(1000'0000));

  swapgastoken();
  
  // Fund evm1 address with 1001 EOS
  evm_eoa evm1;
  const int64_t to_bridge = 1001'0000;
  transfer_token("alice"_n, vaulta_account_name, make_asset(to_bridge)); // EOS->A
  transfer_token("alice"_n, evm_account_name, asset(to_bridge, new_gas_symbol), evm1.address_0x(), vaulta_account_name);

  // Check rec1 balance (in EOS) before sending the message
  BOOST_REQUIRE(vault_balance("rec1"_n) == (balance_and_dust{make_asset(0), 0}));

  // Emit message
  auto res = send_bridge_message(evm1, "rec1", 1000_ether, "0102030405060708090a");

  // Check rec1 balance (in EOS) after sending the message
  BOOST_REQUIRE(vault_balance("rec1"_n) == (balance_and_dust{make_asset(1000'0000), 0}));

  // can't migrate any account afer zzzzzzzzzzzz
  BOOST_REQUIRE_EXCEPTION(migratebal("zzzzzzzzzzzz"_n, 100),
  eosio_assert_message_exception, eosio_assert_message_is("nothing changed"));

  // migrate balance for up to 100 accounts, from beginning
  migratebal(""_n, 100);

  // Check rec1 balance (in A) after sending the message
  BOOST_REQUIRE(vault_balance("rec1"_n) == (balance_and_dust{asset(1000'0000, new_gas_symbol), 0}));

  // can't migrate anymore
  BOOST_REQUIRE_EXCEPTION(migratebal(""_n, 101),
      eosio_assert_message_exception, eosio_assert_message_is("nothing changed"));

  // Recover message form the return value of rec1 contract
  BOOST_CHECK(res->action_traces[1].receiver == "rec1"_n);
  auto msgout = fc::raw::unpack<bridge_message>(res->action_traces[1].return_value);
  auto out = std::get<bridge_message_v0>(msgout);

  BOOST_CHECK(out.receiver == "rec1"_n);
  BOOST_CHECK(out.sender == to_bytes(evm1.address));
  BOOST_CHECK(out.timestamp.time_since_epoch() == control->pending_block_time().time_since_epoch());
  BOOST_CHECK(out.value == to_bytes(1000_ether));
  BOOST_CHECK(out.data == to_bytes(evmc::from_hex("0102030405060708090a").value()));

  // can't register again changing min fee in EOS
  BOOST_REQUIRE_EXCEPTION(bridgereg("rec1"_n, "rec1"_n, make_asset(1)),
      eosio_assert_message_exception, eosio_assert_message_is("unexpected symbol"));

  // register again changing min fee in A
  bridgereg("rec1"_n, "rec1"_n, asset(1, new_gas_symbol));

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(test_bridge_errors, bridge_message_tester) try {

  auto emv_reserved_address = make_reserved_address(evm_account_name);

  // Fund evm1 address with 100 EOS
  evm_eoa evm1;
  const int64_t to_bridge = 1000000;
  transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), evm1.address_0x());

  // Send message without call data [ok]
  send_raw_message(evm1, emv_reserved_address, 0, silkworm::Bytes{});

  // Send message with 1 byte call data (we need at least 4 bytes for the function signature)
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0, evmc::from_hex("01").value()),
    eosio_assert_message_exception, eosio_assert_message_is("datastream attempted to read past the end"));
  evm1.next_nonce--;

  // Send message with 4 bytes NOT matching the 'bridgeMsgV0(string,bool,bytes)' signature
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0, evmc::from_hex("01020304").value()),
    eosio_assert_message_exception, eosio_assert_message_is("unable to decode bridge message"));
  evm1.next_nonce--;

  // Send message with 4 bytes matching the 'bridgeMsgV0(string,bool,bytes)' signature
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0, evmc::from_hex(bridgeMsgV0_method_id).value()),
    eosio_assert_message_exception, eosio_assert_message_is("datastream attempted to read past the end"));
  evm1.next_nonce--;

  // Wrong p1 offset
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0, evmc::from_hex(bridgeMsgV0_method_id).value() +
                                                                          evmc::from_hex(int_str32(11)).value()),
    eosio_assert_message_exception, eosio_assert_message_is("invalid p1 offset"));
  evm1.next_nonce--;

  // Wrong p2 value
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0,
                                                     evmc::from_hex(bridgeMsgV0_method_id).value() +
                                                     evmc::from_hex(int_str32(96)).value() +
                                                     evmc::from_hex(int_str32(11)).value()),
    eosio_assert_message_exception, eosio_assert_message_is("invalid p2 value"));
  evm1.next_nonce--;

  // Wrong p3 offset
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0,
                                                     evmc::from_hex(bridgeMsgV0_method_id).value() +
                                                     evmc::from_hex(int_str32(96)).value()+
                                                     evmc::from_hex(int_str32(1)).value()+
                                                     evmc::from_hex(int_str32(11)).value()),
    eosio_assert_message_exception, eosio_assert_message_is("invalid p3 offset"));
  evm1.next_nonce--;

  // abcd is not an account
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0,
                                                     evmc::from_hex(bridgeMsgV0_method_id).value() +
                                                     evmc::from_hex(int_str32(96)).value() +
                                                     evmc::from_hex(int_str32(1)).value() +
                                                     evmc::from_hex(int_str32(160)).value() +
                                                     evmc::from_hex(int_str32(4)).value() +
                                                     evmc::from_hex(data_str32(str_to_hex("abcd"))).value() +
                                                     evmc::from_hex(int_str32(4)).value() +
                                                     evmc::from_hex(data_str32(str_to_hex("data"))).value()),
    eosio_assert_message_exception, eosio_assert_message_is("receiver is not account"));
  evm1.next_nonce--;

  // Create destination account
  create_accounts({"abcd"_n});

  // abcd not registered
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0,
                                                     evmc::from_hex(bridgeMsgV0_method_id).value() +
                                                     evmc::from_hex(int_str32(96)).value() +
                                                     evmc::from_hex(int_str32(1)).value() +
                                                     evmc::from_hex(int_str32(160)).value() +
                                                     evmc::from_hex(int_str32(4)).value() +
                                                     evmc::from_hex(data_str32(str_to_hex("abcd"))).value() +
                                                     evmc::from_hex(int_str32(4)).value() +
                                                     evmc::from_hex(data_str32(str_to_hex("data"))).value()),
    eosio_assert_message_exception, eosio_assert_message_is("receiver not registered"));
  evm1.next_nonce--;

  // Register abcd
  bridgereg("abcd"_n, "abcd"_n, make_asset(1));

  // min fee not covered
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0,
                                                     evmc::from_hex(bridgeMsgV0_method_id).value() +
                                                     evmc::from_hex(int_str32(96)).value() +
                                                     evmc::from_hex(int_str32(1)).value() +
                                                     evmc::from_hex(int_str32(160)).value() +
                                                     evmc::from_hex(int_str32(4)).value() +
                                                     evmc::from_hex(data_str32(str_to_hex("abcd"))).value() +
                                                     evmc::from_hex(int_str32(4)).value() +
                                                     evmc::from_hex(data_str32(str_to_hex("data"))).value()),
    eosio_assert_message_exception, eosio_assert_message_is("min_fee not covered"));
  evm1.next_nonce--;

  // Close abcd balance
  close("abcd"_n);

  // receiver account is not open
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 1e14,
                                                     evmc::from_hex(bridgeMsgV0_method_id).value() +
                                                     evmc::from_hex(int_str32(96)).value() +
                                                     evmc::from_hex(int_str32(1)).value() +
                                                     evmc::from_hex(int_str32(160)).value() +
                                                     evmc::from_hex(int_str32(4)).value() +
                                                     evmc::from_hex(data_str32(str_to_hex("abcd"))).value() +
                                                     evmc::from_hex(int_str32(4)).value() +
                                                     evmc::from_hex(data_str32(str_to_hex("data"))).value()),
    eosio_assert_message_exception, eosio_assert_message_is("receiver account is not open"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_send_message_from_solidity, bridge_message_tester) try {
  // // SPDX-License-Identifier: GPL-3.0
  // pragma solidity >=0.7.0 <0.9.0;
  // contract Emiter {
  //     function go(string memory destination, bool force_atomic, uint256 n) public {
  //         address eosevm = 0xBbBBbbBBbBbbbBBBbBbbBBbB56E4000000000000;
  //         for(uint i=0; i<n; i++) {
  //             bytes memory n_bytes = abi.encodePacked(i+0xFFFFFF00);
  //             (bool success, ) = eosevm.call(abi.encodeWithSignature("bridgeMsgV0(string,bool,bytes)", destination, force_atomic, n_bytes));
  //             if(!success) { revert(); }
  //         }
  //     }
  // }
  const std::string emiter_bytecode = "608060405234801561001057600080fd5b50610696806100206000396000f3fe608060405234801561001057600080fd5b506004361061002b5760003560e01c8063e1963a3114610030575b600080fd5b61004a6004803603810190610045919061038f565b61004c565b005b600073bbbbbbbbbbbbbbbbbbbbbbbb56e4000000000000905060005b828110156101c057600063ffffff0082610082919061042d565b6040516020016100929190610482565b604051602081830303815290604052905060008373ffffffffffffffffffffffffffffffffffffffff168787846040516024016100d193929190610580565b6040516020818303038152906040527ff781185b000000000000000000000000000000000000000000000000000000007bffffffffffffffffffffffffffffffffffffffffffffffffffffffff19166020820180517bffffffffffffffffffffffffffffffffffffffffffffffffffffffff838183161783525050505060405161015b9190610601565b6000604051808303816000865af19150503d8060008114610198576040519150601f19603f3d011682016040523d82523d6000602084013e61019d565b606091505b50509050806101ab57600080fd5b505080806101b890610618565b915050610068565b5050505050565b6000604051905090565b600080fd5b600080fd5b600080fd5b600080fd5b6000601f19601f8301169050919050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052604160045260246000fd5b61022e826101e5565b810181811067ffffffffffffffff8211171561024d5761024c6101f6565b5b80604052505050565b60006102606101c7565b905061026c8282610225565b919050565b600067ffffffffffffffff82111561028c5761028b6101f6565b5b610295826101e5565b9050602081019050919050565b82818337600083830152505050565b60006102c46102bf84610271565b610256565b9050828152602081018484840111156102e0576102df6101e0565b5b6102eb8482856102a2565b509392505050565b600082601f830112610308576103076101db565b5b81356103188482602086016102b1565b91505092915050565b60008115159050919050565b61033681610321565b811461034157600080fd5b50565b6000813590506103538161032d565b92915050565b6000819050919050565b61036c81610359565b811461037757600080fd5b50565b60008135905061038981610363565b92915050565b6000806000606084860312156103a8576103a76101d1565b5b600084013567ffffffffffffffff8111156103c6576103c56101d6565b5b6103d2868287016102f3565b93505060206103e386828701610344565b92505060406103f48682870161037a565b9150509250925092565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052601160045260246000fd5b600061043882610359565b915061044383610359565b925082820190508082111561045b5761045a6103fe565b5b92915050565b6000819050919050565b61047c61047782610359565b610461565b82525050565b600061048e828461046b565b60208201915081905092915050565b600081519050919050565b600082825260208201905092915050565b60005b838110156104d75780820151818401526020810190506104bc565b60008484015250505050565b60006104ee8261049d565b6104f881856104a8565b93506105088185602086016104b9565b610511816101e5565b840191505092915050565b61052581610321565b82525050565b600081519050919050565b600082825260208201905092915050565b60006105528261052b565b61055c8185610536565b935061056c8185602086016104b9565b610575816101e5565b840191505092915050565b6000606082019050818103600083015261059a81866104e3565b90506105a9602083018561051c565b81810360408301526105bb8184610547565b9050949350505050565b600081905092915050565b60006105db8261052b565b6105e581856105c5565b93506105f58185602086016104b9565b80840191505092915050565b600061060d82846105d0565b915081905092915050565b600061062382610359565b91507fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff8203610655576106546103fe565b5b60018201905091905056fea2646970667358221220b0b317b0ac391546d4bac13af7b3c9e21e5b5ca2c091dbd21a971f492f8adaaa64736f6c63430008120033";

  // Create destination account
  create_accounts({"rec1"_n});
  set_code("rec1"_n, testing::contracts::evm_bridge_receiver_wasm());
  set_abi("rec1"_n, testing::contracts::evm_bridge_receiver_abi().data());

  // Register rec1 with 0 EOS as min_fee
  bridgereg("rec1"_n, "rec1"_n, make_asset(0));

  // Fund evm1 address with 100 EOS
  evm_eoa evm1;
  const int64_t to_bridge = 1000000;
  transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), evm1.address_0x());

  // Deploy emiter contract and get its address and account ID
  auto contract_addr = deploy_contract(evm1, evmc::from_hex(emiter_bytecode).value());
  uint64_t contract_account_id = find_account_by_address(contract_addr).value().id;
  produce_blocks(1);

  // Call method "go" on emiter contract (sha3('go(string,bool,uint)') = 0xe1963a31)
  // ===> go('rec1', true, 3)
  auto txn = generate_tx(contract_addr, 0, 500'000);
  txn.data  = evmc::from_hex("e1963a31").value();
  txn.data += evmc::from_hex(int_str32(96)).value(); //offset of param1
  txn.data += evmc::from_hex(int_str32(1)).value();  //param2
  txn.data += evmc::from_hex(int_str32(3)).value();  //param3
  txn.data += evmc::from_hex(int_str32(4)).value();  //param1 size
  txn.data += evmc::from_hex(data_str32(str_to_hex("rec1"))).value(); //param1 data
  evm1.sign(txn);

  auto res = pushtx(txn);
  BOOST_CHECK(res->action_traces.size() == 4);

  for(int i=0; i<3; ++i) {
    auto msgout = fc::raw::unpack<bridge_message>(res->action_traces[1+i].return_value);
    auto out = std::get<bridge_message_v0>(msgout);

    BOOST_CHECK(out.receiver == "rec1"_n);
    BOOST_CHECK(out.sender == to_bytes(contract_addr));
    BOOST_CHECK(out.timestamp.time_since_epoch() == control->pending_block_time().time_since_epoch());
    BOOST_CHECK(out.value == to_bytes(0_ether));
    BOOST_CHECK(out.data == to_bytes(evmc::from_hex("00000000000000000000000000000000000000000000000000000000FFFFFF0"+std::to_string(i)).value()));
  }
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(handler_tests, bridge_message_tester) try {
  auto emv_reserved_address = make_reserved_address(evm_account_name);

  // Fund evm1 address with 1001 EOS
  evm_eoa evm1;
  const int64_t to_bridge = 1001'0000;
  transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), evm1.address_0x());

  BOOST_REQUIRE_EXCEPTION(send_bridge_message(evm1, "receiver", 0, "0102030405060708090a"),
    eosio_assert_message_exception, eosio_assert_message_is("receiver is not account"));
  evm1.next_nonce--;

  // Create receiver account
  create_accounts({"receiver"_n});

  // receiver not registered
  BOOST_REQUIRE_EXCEPTION(send_bridge_message(evm1, "receiver", 0, "0102030405060708090a"),
    eosio_assert_message_exception, eosio_assert_message_is("receiver not registered"));
  evm1.next_nonce--;

  // Create handler account
  create_accounts({"handler"_n});
  set_code("handler"_n, testing::contracts::evm_bridge_receiver_wasm());
  set_abi("handler"_n, testing::contracts::evm_bridge_receiver_abi().data());
  // Register handler with 1000.0000 EOS as min_fee
  bridgereg("receiver"_n, "handler"_n, make_asset(1000'0000));


  // Corner case: receiver account is not open
  // We can only test in this way because bridgereg will open receiver.
  close("receiver"_n);
  
  BOOST_REQUIRE_EXCEPTION(send_bridge_message(evm1, "receiver", 1000_ether, "0102030405060708090a"),
    eosio_assert_message_exception, eosio_assert_message_is("receiver account is not open"));
  evm1.next_nonce--;
  open("receiver"_n);

  // Check handler balance before sending the message
  BOOST_REQUIRE(vault_balance("receiver"_n) == (balance_and_dust{make_asset(0), 0}));

  // Emit message
  auto res = send_bridge_message(evm1, "receiver", 1000_ether, "0102030405060708090a");

  // Check handler balance after sending the message
  BOOST_REQUIRE(vault_balance("receiver"_n) == (balance_and_dust{make_asset(1000'0000), 0}));

  // Recover message form the return value of handler contract
  // Make sure "handler" received a message sent to "receiver"
  BOOST_CHECK(res->action_traces[1].receiver == "handler"_n);
  auto msgout = fc::raw::unpack<bridge_message>(res->action_traces[1].return_value);
  auto out = std::get<bridge_message_v0>(msgout);
  
  BOOST_CHECK(out.receiver == "receiver"_n);
  BOOST_CHECK(out.sender == to_bytes(evm1.address));
  BOOST_CHECK(out.timestamp.time_since_epoch() == control->pending_block_time().time_since_epoch());
  BOOST_CHECK(out.value == to_bytes(1000_ether));
  BOOST_CHECK(out.data == to_bytes(evmc::from_hex("0102030405060708090a").value()));

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()