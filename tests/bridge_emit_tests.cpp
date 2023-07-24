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
      data += evmc::from_hex("7e95a247").value();
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
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0, evmc::from_hex("7e95a247").value()),
    eosio_assert_message_exception, eosio_assert_message_is("datastream attempted to read past the end"));
  evm1.next_nonce--;

  // Wrong p1 offset
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0, evmc::from_hex("7e95a247").value() +
                                                                          evmc::from_hex(int_str32(11)).value()),
    eosio_assert_message_exception, eosio_assert_message_is("invalid p1 offset"));
  evm1.next_nonce--;

  // Wrong p2 offset
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0,
                                                     evmc::from_hex("7e95a247").value() +
                                                     evmc::from_hex(int_str32(64)).value() +
                                                     evmc::from_hex(int_str32(11)).value()),
    eosio_assert_message_exception, eosio_assert_message_is("invalid p2 offset"));
  evm1.next_nonce--;

  // abcd is not an account
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0,
                                                     evmc::from_hex("7e95a247").value() +
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
                                                     evmc::from_hex("7e95a247").value() +
                                                     evmc::from_hex(int_str32(64)).value() +
                                                     evmc::from_hex(int_str32(128)).value() +
                                                     evmc::from_hex(int_str32(4)).value() +
                                                     evmc::from_hex(data_str32(str_to_hex("abcd"))).value() +
                                                     evmc::from_hex(int_str32(4)).value() +
                                                     evmc::from_hex(data_str32(str_to_hex("data"))).value()),
    eosio_assert_message_exception, eosio_assert_message_is("receiver not registered"));
  evm1.next_nonce--;

  // Register rec1 with 0 EOS as min_fee
  bridgereg("abcd"_n, make_asset(1));

  // min fee not covered
  BOOST_REQUIRE_EXCEPTION(send_raw_message(evm1, emv_reserved_address, 0,
                                                     evmc::from_hex("7e95a247").value() +
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
                                                     evmc::from_hex("7e95a247").value() +
                                                     evmc::from_hex(int_str32(64)).value() +
                                                     evmc::from_hex(int_str32(128)).value() +
                                                     evmc::from_hex(int_str32(4)).value() +
                                                     evmc::from_hex(data_str32(str_to_hex("abcd"))).value() +
                                                     evmc::from_hex(int_str32(4)).value() +
                                                     evmc::from_hex(data_str32(str_to_hex("data"))).value()),
    eosio_assert_message_exception, eosio_assert_message_is("receiver account is not open"));

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()