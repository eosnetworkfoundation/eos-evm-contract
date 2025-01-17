#include "basic_evm_tester.hpp"
#include <silkworm/core/execution/address.hpp>

using intx::operator""_u256;

using namespace evm_test;
using eosio::testing::eosio_assert_message_is;
using namespace evmc::literals;

struct signature_evm_tester : basic_evm_tester {
    signature_evm_tester() {
      create_accounts({"alice"_n});
      transfer_token(faucet_account_name, "alice"_n, make_asset(10000'0000));
    }
};

BOOST_AUTO_TEST_SUITE(signature_evm_tests)

// jungle4 tx
// {
//   "chainId": "15557",
//   "type": "LegacyTransaction",
//   "valid": false,
//   "hash": "0x36fa4621efee64c291125d30d2eb93dbc58a306cf7cab182a702e2a7efc0212c",
//   "nonce": "0",
//   "gasPrice": "300000000000",
//   "gasLimit": "21000",
//   "from": "0xC6Fe5D33615a1C52c08018c47E8Bc53646A0E101",
//   "to": "0x4592d8f8d7b001e72cb26a73e4fa1806a51ac79d",
//   "v": "79ad",
//   "r": "31e11c626059befb75d707ffa66a149ddcd99da112f250f1472c83f20ffcc137",
//   "s": "c5bb4877e1440e36197d5ae934a1f31756d79d68f3529d06779dc5984f079995",
//   "value": "1"
// }

BOOST_FIXTURE_TEST_CASE(accept_big_s_jungle4, signature_evm_tester) try {

  init(15557);
  auto rlptx = "f866808545d964b800825208944592d8f8d7b001e72cb26a73e4fa1806a51ac79d01808279ada031e11c626059befb75d707ffa66a149ddcd99da112f250f1472c83f20ffcc137a0c5bb4877e1440e36197d5ae934a1f31756d79d68f3529d06779dc5984f079995";

  auto tx_from = 0xc6fe5d33615a1c52c08018c47e8bc53646a0e101_address;
  auto tx_to = 0x4592d8f8d7b001e72cb26a73e4fa1806a51ac79d_address;

  transfer_token(faucet_account_name, evm_account_name, make_asset(1'0000), "0x"+silkworm::to_hex(tx_from));

  const auto rlptx_bytes = *silkworm::from_hex(rlptx);
  silkworm::ByteView bv = rlptx_bytes;
  silkworm::Transaction trx;
  silkworm::rlp::decode_transaction(bv, trx, silkworm::rlp::Eip2718Wrapping::kNone);

  BOOST_CHECK_EQUAL(evm_balance(tx_to).has_value(), false);
  pushtx(trx);
  BOOST_CHECK_EQUAL(*evm_balance(tx_to), 1);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(reject_big_s_jungle4, signature_evm_tester) try {

  init(15557);
  setversion(3, evm_account_name);
  produce_blocks(2);

  auto rlptx = "f866808545d964b800825208944592d8f8d7b001e72cb26a73e4fa1806a51ac79d01808279ada031e11c626059befb75d707ffa66a149ddcd99da112f250f1472c83f20ffcc137a0c5bb4877e1440e36197d5ae934a1f31756d79d68f3529d06779dc5984f079995";

  const auto rlptx_bytes = *silkworm::from_hex(rlptx);
  silkworm::ByteView bv = rlptx_bytes;
  silkworm::Transaction trx;
  silkworm::rlp::decode_transaction(bv, trx, silkworm::rlp::Eip2718Wrapping::kNone);

  BOOST_REQUIRE_EXCEPTION(pushtx(trx),
    eosio_assert_message_exception,
    [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: pre_validate_transaction error: 27 Invalid signature");});

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()