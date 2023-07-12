#include "basic_evm_tester.hpp"
#include <silkworm/execution/address.hpp>

using namespace evm_test;
struct chain_id_tester : basic_evm_tester {
   chain_id_tester() {
      create_accounts({"alice"_n});
      transfer_token(faucet_account_name, "alice"_n, make_asset(10000'0000));
      init();
   }
};

BOOST_AUTO_TEST_SUITE(chainid_evm_tests)
BOOST_FIXTURE_TEST_CASE(chainid_tests, chain_id_tester) try {

   // Bridge transactions are sent without chain id specified (must succeed)
   evm_eoa evm1;
   const int64_t to_bridge = 1000000; //100.000 EOS
   transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), evm1.address_0x());
   auto balance_evm1 = evm_balance(evm1);
   BOOST_REQUIRE(balance_evm1.has_value() && *balance_evm1 == 100_ether);

   // Transactions without chain id specified must fail
   evm_eoa evm2;
   auto txn = generate_tx(evm2.address, 1_ether, 21000);
   evm1.sign(txn, {});
   BOOST_REQUIRE_EXCEPTION(pushtx(txn), eosio_assert_message_exception,
    [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: tx without chain-id");});

   // reuse nonce since previous signed transaction was rejected
   evm1.next_nonce--;

   // Transactions with chain id must succeed
   evm_eoa evm3;
   txn = generate_tx(evm3.address, 20_ether, 21000);
   evm1.sign(txn, basic_evm_tester::evm_chain_id);
   pushtx(txn);
   auto balance_evm3 = evm_balance(evm3);
   BOOST_REQUIRE(balance_evm3.has_value() && *balance_evm3 == 20_ether);

} FC_LOG_AND_RETHROW()
BOOST_AUTO_TEST_SUITE_END()
