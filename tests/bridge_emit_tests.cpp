#include "basic_evm_tester.hpp"
#include <silkworm/execution/address.hpp>

#include "utils.hpp"

using intx::operator""_u256;

using namespace evm_test;
using eosio::testing::eosio_assert_message_is;
using eosio::testing::expect_assert_message;

struct bridge_emit_evm_tester : basic_evm_tester {
    bridge_emit_evm_tester() {
      create_accounts({"alice"_n});
      transfer_token(faucet_account_name, "alice"_n, make_asset(10000'0000));
      init();
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

    transaction_trace_ptr send_emit_message(evm_eoa& eoa, const std::string& receiver, const intx::uint256& value, const std::string& str_data) {

      silkworm::Bytes data;
      data += evmc::from_hex("44282a35").value();
      data += evmc::from_hex(int_str32(64)).value();
      data += evmc::from_hex(int_str32(128)).value();
      data += evmc::from_hex(int_str32(receiver.length())).value();
      data += evmc::from_hex(data_str32(str_to_hex(receiver))).value();
      data += evmc::from_hex(int_str32(str_data.size()/2)).value();
      data += evmc::from_hex(data_str32(str_data)).value();

      return send_raw_message(eoa, make_reserved_address("evm"_n), value, data);
    }

    transaction_trace_ptr send_raw_message(evm_eoa& eoa, const evmc::address& dest, const intx::uint256& value, const silkworm::Bytes& data) {
      auto txn = generate_tx(dest, value, 250'000);
      txn.data = data;
      eoa.sign(txn);
      return pushtx(txn);
    }
};

BOOST_AUTO_TEST_SUITE(bridge_emit_tests)
BOOST_FIXTURE_TEST_CASE(bridge_register_test, bridge_emit_evm_tester) try {

  create_accounts({"rec1"_n});
  
  // evm auth is needed
  BOOST_REQUIRE_EXCEPTION(bridgereg("rec1"_n, make_asset(-1), {}),
    missing_auth_exception, [](const missing_auth_exception& e) { return expect_assert_message(e, "missing authority of evm"); });

  // min fee only accepts EOS
  BOOST_REQUIRE_EXCEPTION(bridgereg("rec1"_n, asset::from_string("1.0000 TST")),
    eosio_assert_message_exception, eosio_assert_message_is("unexpected symbol"));

  // min fee must be non-negative
  BOOST_REQUIRE_EXCEPTION(bridgereg("rec1"_n, make_asset(-1)),
    eosio_assert_message_exception, eosio_assert_message_is("min_fee cannot be negative"));

  bridgereg("rec1"_n, make_asset(0));

  auto row = fc::raw::unpack<message_receiver>(get_row_by_account( "evm"_n, "evm"_n, "msgreceiver"_n, "rec1"_n));
  BOOST_REQUIRE(row.account == "rec1"_n);
  BOOST_REQUIRE(row.min_fee == make_asset(0));

  // Register again changing min fee
  bridgereg("rec1"_n, make_asset(1));

  row = fc::raw::unpack<message_receiver>(get_row_by_account( "evm"_n, "evm"_n, "msgreceiver"_n, "rec1"_n));
  BOOST_REQUIRE(row.account == "rec1"_n);
  BOOST_REQUIRE(row.min_fee == make_asset(1));

  // Unregister rec1
  bridgeunreg("rec1"_n);
  const auto& d = get_row_by_account( "evm"_n, "evm"_n, "msgreceiver"_n, "rec1"_n);
  BOOST_REQUIRE(d.size() == 0);
  produce_block();

  // Try to unregister again
  BOOST_REQUIRE_EXCEPTION(bridgeunreg("rec1"_n),
    eosio_assert_message_exception, eosio_assert_message_is("receiver not found"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(basic_tests, bridge_emit_evm_tester) try {

  // Create destination account
  create_accounts({"rec1"_n});
  set_code("rec1"_n, testing::contracts::evm_bridge_receiver_wasm());
  set_abi("rec1"_n, testing::contracts::evm_bridge_receiver_abi().data());

  // Register rec1 with 1.0000 EOS as min_fee
  bridgereg("rec1"_n, make_asset(10000));

  // Fund evm1 address with 100 EOS
  evm_eoa evm1;
  const int64_t to_bridge = 1000000;
  transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), evm1.address_0x());

  // Emit message
  auto res = send_emit_message(evm1, "rec1", 1_ether, "0102030405060708090a");

  // Recover message form the return value of rec1 contract
  BOOST_CHECK(res->action_traces[1].receiver == "rec1"_n);
  auto out = fc::raw::unpack<bridge_emit_message>(res->action_traces[1].return_value);

  BOOST_CHECK(out.receiver == "rec1"_n);
  BOOST_CHECK(out.sender == to_bytes(evm1.address));
  BOOST_CHECK(out.timestamp.time_since_epoch() == control->pending_block_time().time_since_epoch());
  BOOST_CHECK(out.value == to_bytes(1_ether));
  BOOST_CHECK(out.data == to_bytes(evmc::from_hex("0102030405060708090a").value()));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_emit_errors, bridge_emit_evm_tester) try {

  auto emv_reserved_address = make_reserved_address("evm"_n);

  // Fund evm1 address with 100 EOS
  evm_eoa evm1;
  const int64_t to_bridge = 1000000;
  transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), evm1.address_0x());

  // Send message without call data [ok]
  send_raw_message(evm1, emv_reserved_address, 0, silkworm::Bytes{});

  // Send message with 1 byte call data (we need at least 4 bytes for the function signature)
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0, evmc::from_hex("01").value()),
    eosio_assert_message_exception, eosio_assert_message_is("datastream attempted to read past the end"));
  evm1.next_nonce--;

  // Send message with 4 bytes NOT matching the 'emit_(string,bytes)' signature
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0, evmc::from_hex("01020304").value()),
    eosio_assert_message_exception, eosio_assert_message_is("unable to decode call message"));
  evm1.next_nonce--;

  // Send message with 4 bytes matching the 'emit_(string,bytes)' signature
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0, evmc::from_hex("44282a35").value()),
    eosio_assert_message_exception, eosio_assert_message_is("datastream attempted to read past the end"));
  evm1.next_nonce--;

  // Wrong p1 offset
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0, evmc::from_hex("44282a35").value() +
                                                                          evmc::from_hex(int_str32(11)).value()),
    eosio_assert_message_exception, eosio_assert_message_is("invalid p1 offset"));
  evm1.next_nonce--;

  // Wrong p2 offset
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0,
                                                     evmc::from_hex("44282a35").value() +
                                                     evmc::from_hex(int_str32(64)).value() +
                                                     evmc::from_hex(int_str32(11)).value()),
    eosio_assert_message_exception, eosio_assert_message_is("invalid p2 offset"));
  evm1.next_nonce--;

  // abcd is not an account
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0,
                                                     evmc::from_hex("44282a35").value() +
                                                     evmc::from_hex(int_str32(64)).value() +
                                                     evmc::from_hex(int_str32(128)).value() +
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
                                                     evmc::from_hex("44282a35").value() +
                                                     evmc::from_hex(int_str32(64)).value() +
                                                     evmc::from_hex(int_str32(128)).value() +
                                                     evmc::from_hex(int_str32(4)).value() +
                                                     evmc::from_hex(data_str32(str_to_hex("abcd"))).value() +
                                                     evmc::from_hex(int_str32(4)).value() +
                                                     evmc::from_hex(data_str32(str_to_hex("data"))).value()),
    eosio_assert_message_exception, eosio_assert_message_is("receiver not registered"));
  evm1.next_nonce--;

  // Register abcd
  bridgereg("abcd"_n, make_asset(1));

  // min fee not covered
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0,
                                                     evmc::from_hex("44282a35").value() +
                                                     evmc::from_hex(int_str32(64)).value() +
                                                     evmc::from_hex(int_str32(128)).value() +
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
                                                     evmc::from_hex("44282a35").value() +
                                                     evmc::from_hex(int_str32(64)).value() +
                                                     evmc::from_hex(int_str32(128)).value() +
                                                     evmc::from_hex(int_str32(4)).value() +
                                                     evmc::from_hex(data_str32(str_to_hex("abcd"))).value() +
                                                     evmc::from_hex(int_str32(4)).value() +
                                                     evmc::from_hex(data_str32(str_to_hex("data"))).value()),
    eosio_assert_message_exception, eosio_assert_message_is("receiver account is not open"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(emit_from_solidity, bridge_emit_evm_tester) try {
  // // SPDX-License-Identifier: GPL-3.0
  // pragma solidity >=0.7.0 <0.9.0;
  // contract Emiter {
  //     function go(string memory destination, uint256 n) public {
  //         address eosevm = 0xBbBBbbBBbBbbbBBBbBbbBBbB56E4000000000000;
  //         for(uint i=0; i<n; i++) {
  //             bytes memory n_bytes = abi.encodePacked(i+0xFFFFFF00);
  //             (bool success, ) = eosevm.call(abi.encodeWithSignature("emit_(string,bytes)", destination, n_bytes));
  //             if(!success) { revert(); }
  //         }
  //     }
  // }
  const std::string emiter_bytecode = "608060405234801561001057600080fd5b5061062b806100206000396000f3fe608060405234801561001057600080fd5b506004361061002b5760003560e01c8063a5cc93e414610030575b600080fd5b61004a60048036038101906100459190610354565b61004c565b005b600073bbbbbbbbbbbbbbbbbbbbbbbb56e4000000000000905060005b828110156101be57600063ffffff008261008291906103df565b6040516020016100929190610434565b604051602081830303815290604052905060008373ffffffffffffffffffffffffffffffffffffffff1686836040516024016100cf929190610523565b6040516020818303038152906040527f44282a35000000000000000000000000000000000000000000000000000000007bffffffffffffffffffffffffffffffffffffffffffffffffffffffff19166020820180517bffffffffffffffffffffffffffffffffffffffffffffffffffffffff83818316178352505050506040516101599190610596565b6000604051808303816000865af19150503d8060008114610196576040519150601f19603f3d011682016040523d82523d6000602084013e61019b565b606091505b50509050806101a957600080fd5b505080806101b6906105ad565b915050610068565b50505050565b6000604051905090565b600080fd5b600080fd5b600080fd5b600080fd5b6000601f19601f8301169050919050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052604160045260246000fd5b61022b826101e2565b810181811067ffffffffffffffff8211171561024a576102496101f3565b5b80604052505050565b600061025d6101c4565b90506102698282610222565b919050565b600067ffffffffffffffff821115610289576102886101f3565b5b610292826101e2565b9050602081019050919050565b82818337600083830152505050565b60006102c16102bc8461026e565b610253565b9050828152602081018484840111156102dd576102dc6101dd565b5b6102e884828561029f565b509392505050565b600082601f830112610305576103046101d8565b5b81356103158482602086016102ae565b91505092915050565b6000819050919050565b6103318161031e565b811461033c57600080fd5b50565b60008135905061034e81610328565b92915050565b6000806040838503121561036b5761036a6101ce565b5b600083013567ffffffffffffffff811115610389576103886101d3565b5b610395858286016102f0565b92505060206103a68582860161033f565b9150509250929050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052601160045260246000fd5b60006103ea8261031e565b91506103f58361031e565b925082820190508082111561040d5761040c6103b0565b5b92915050565b6000819050919050565b61042e6104298261031e565b610413565b82525050565b6000610440828461041d565b60208201915081905092915050565b600081519050919050565b600082825260208201905092915050565b60005b8381101561048957808201518184015260208101905061046e565b60008484015250505050565b60006104a08261044f565b6104aa818561045a565b93506104ba81856020860161046b565b6104c3816101e2565b840191505092915050565b600081519050919050565b600082825260208201905092915050565b60006104f5826104ce565b6104ff81856104d9565b935061050f81856020860161046b565b610518816101e2565b840191505092915050565b6000604082019050818103600083015261053d8185610495565b9050818103602083015261055181846104ea565b90509392505050565b600081905092915050565b6000610570826104ce565b61057a818561055a565b935061058a81856020860161046b565b80840191505092915050565b60006105a28284610565565b915081905092915050565b60006105b88261031e565b91507fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff82036105ea576105e96103b0565b5b60018201905091905056fea2646970667358221220b4280a81c8e0b97644a147eb1c062013ea32161a7cbfafd74e5c1adf7f21886364736f6c63430008120033";

  // Create destination account
  create_accounts({"rec1"_n});
  set_code("rec1"_n, testing::contracts::evm_bridge_receiver_wasm());
  set_abi("rec1"_n, testing::contracts::evm_bridge_receiver_abi().data());

  // Register rec1 with 0 EOS as min_fee
  bridgereg("rec1"_n, make_asset(0));

  // Fund evm1 address with 100 EOS
  evm_eoa evm1;
  const int64_t to_bridge = 1000000;
  transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), evm1.address_0x());

  // Deploy emiter contract and get its address and account ID
  auto contract_addr = deploy_contract(evm1, evmc::from_hex(emiter_bytecode).value());
  uint64_t contract_account_id = find_account_by_address(contract_addr).value().id;
  produce_blocks(1);

  // Call method "start" on emiter contract (sha3('start(string,uint)') = 0xa5cc93e4)
  // ===> start('rec1', 3)
  auto txn = generate_tx(contract_addr, 0, 500'000);
  txn.data  = evmc::from_hex("a5cc93e4").value();
  txn.data += evmc::from_hex("0000000000000000000000000000000000000000000000000000000000000040").value();
  txn.data += evmc::from_hex("0000000000000000000000000000000000000000000000000000000000000003").value();
  txn.data += evmc::from_hex("0000000000000000000000000000000000000000000000000000000000000004").value();
  txn.data += evmc::from_hex("7265633100000000000000000000000000000000000000000000000000000000").value();
  evm1.sign(txn);
  auto res = pushtx(txn);
  BOOST_CHECK(res->action_traces.size() == 4);

  for(int i=0; i<3; ++i) {
    auto out = fc::raw::unpack<bridge_emit_message>(res->action_traces[1+i].return_value);
    BOOST_CHECK(out.receiver == "rec1"_n);
    BOOST_CHECK(out.sender == to_bytes(contract_addr));
    BOOST_CHECK(out.timestamp.time_since_epoch() == control->pending_block_time().time_since_epoch());
    BOOST_CHECK(out.value == to_bytes(0_ether));
    BOOST_CHECK(out.data == to_bytes(evmc::from_hex("00000000000000000000000000000000000000000000000000000000FFFFFF0"+std::to_string(i)).value()));
  }

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()