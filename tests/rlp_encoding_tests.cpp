#include "basic_evm_tester.hpp"
#include <silkworm/core/types/address.hpp>

using namespace evm_test;
struct rlp_encoding_tester : basic_evm_tester {
   rlp_encoding_tester() {
      create_accounts({"alice"_n});
      transfer_token(faucet_account_name, "alice"_n, make_asset(10000'0000));
      init();
   }

    transaction_trace_ptr my_pushtx(const silkworm::Transaction& trx, bool wrap_eip2718_into_string)
    {
        silkworm::Bytes rlp;
        silkworm::rlp::encode(rlp, trx, wrap_eip2718_into_string);

        bytes rlp_bytes;
        rlp_bytes.resize(rlp.size());
        memcpy(rlp_bytes.data(), rlp.data(), rlp.size());

        return push_action(evm_account_name, "pushtx"_n, evm_account_name, mvo()("miner", evm_account_name)("rlptx", rlp_bytes));
    }

};

BOOST_AUTO_TEST_SUITE(rlp_encoding_tests)

BOOST_FIXTURE_TEST_CASE(wrap_tx_in_string, rlp_encoding_tester) try {

    setversion(1, evm_account_name);
    produce_blocks(2);

    // Fund evm1 address with 10.0000 EOS (triggers version 1 change)
    evm_eoa evm1;
    transfer_token("alice"_n, evm_account_name, make_asset(10'0000), evm1.address_0x());

    evm_eoa evm2;

    auto tx = generate_tx(evm2.address, 1);
    tx.type = silkworm::TransactionType::kDynamicFee;
    tx.max_priority_fee_per_gas = 0;
    tx.max_fee_per_gas = suggested_gas_price;

    evm1.sign(tx);

    // Wrap kDynamicFee tx in string
    BOOST_REQUIRE_EXCEPTION(my_pushtx(tx, true), eosio_assert_message_exception,
        [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "unable to decode transaction");});

    // Don't wrap kDynamicFee tx in string
    BOOST_CHECK_NO_THROW(my_pushtx(tx, false));

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
