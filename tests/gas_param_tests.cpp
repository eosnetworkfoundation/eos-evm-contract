#include "basic_evm_tester.hpp"

using namespace eosio::testing;
using namespace evm_test;

BOOST_AUTO_TEST_SUITE(evm_gas_param_tests)

struct gas_param_evm_tester : basic_evm_tester
{
   evm_eoa faucet_eoa;

   static constexpr name miner_account_name = "alice"_n;

   gas_param_evm_tester() :
      faucet_eoa(evmc::from_hex("a3f1b69da92a0233ce29485d3049a4ace39e8d384bbc2557e3fc60940ce4e954").value())
   {
      create_accounts({miner_account_name});
      transfer_token(faucet_account_name, miner_account_name, make_asset(100'0000));
   }

   void fund_evm_faucet()
   {
      transfer_token(faucet_account_name, evm_account_name, make_asset(100'0000), faucet_eoa.address_0x());
   }
};

BOOST_FIXTURE_TEST_CASE(basic, gas_param_evm_tester) try {

    uint64_t suggested_gas_price = 150'000'000'000ull;
    init(15555, suggested_gas_price);
    
    produce_block();
    fund_evm_faucet();
    produce_block();

    // wrong permission
    BOOST_REQUIRE_EXCEPTION(
        updtgasparam(asset(10'0000, native_symbol), 1'000'000'000, "alice"_n),
        missing_auth_exception, [](const missing_auth_exception & e){ return true;});

    // wrong permission
    BOOST_REQUIRE_EXCEPTION(
        setgasparam(1,2,3,4,5, "alice"_n),
        missing_auth_exception, [](const missing_auth_exception & e){ return true;});

    // require promoted evm_version larger or equal to 1
    BOOST_REQUIRE_EXCEPTION(
        updtgasparam(asset(10'0000, native_symbol), 1'000'000'000, evm_account_name),
        eosio_assert_message_exception,
        eosio_assert_message_is("evm_version must >= 1"));

    // require promoted evm_version larger or equal to 1
    BOOST_REQUIRE_EXCEPTION(
        setgasparam(1,2,3,4,5, evm_account_name),
        eosio_assert_message_exception,
        eosio_assert_message_is("evm_version must >= 1"));

    setversion(1, evm_account_name);

    produce_block();
    produce_block();

    // require promoted evm_version larger or equal to 1
    BOOST_REQUIRE_EXCEPTION(
        updtgasparam(asset(10'0000, native_symbol), 1'000'000'000, evm_account_name),
        eosio_assert_message_exception,
        eosio_assert_message_is("evm_version must >= 1"));

    // kick of setverion, evmtx event generated
    {
        evm_eoa recipient;
        silkworm::Transaction tx{
            .type = silkworm::Transaction::Type::kLegacy,
            .max_priority_fee_per_gas = suggested_gas_price,
            .max_fee_per_gas = suggested_gas_price,
            .gas_limit = 21000,
            .to = recipient.address,
            .value = 1,
        };
        faucet_eoa.sign(tx);
        chain::transaction_trace_ptr trace = pushtx(tx);
        BOOST_REQUIRE_EQUAL(trace->action_traces.size(), 2);
        BOOST_ASSERT(trace->action_traces.size() >= 2 && trace->action_traces[0].act.name == "pushtx"_n);
        BOOST_ASSERT(trace->action_traces.size() >= 2 && trace->action_traces[1].act.name == "evmtx"_n);
    }

    // require miniumum gas at least 1 gwei
    BOOST_REQUIRE_EXCEPTION(
        updtgasparam(asset(10'0000, native_symbol), 999'999'999, evm_account_name),
        eosio_assert_message_exception,
        eosio_assert_message_is("gas_price must >= 1Gwei"));

    // invalid symbol in ram_price_mb paramerter
    BOOST_REQUIRE_EXCEPTION(
        updtgasparam(asset(10'0000, symbol::from_string("0,EOS")), 1'000'000'000, evm_account_name),
        eosio_assert_message_exception,
        eosio_assert_message_is("invalid price symbol"));
    BOOST_REQUIRE_EXCEPTION(
        updtgasparam(asset(10'0000, symbol::from_string("4,SYS")), 1'000'000'000, evm_account_name),
        eosio_assert_message_exception,
        eosio_assert_message_is("invalid price symbol"));

    // G_sset must >= 2900
    BOOST_REQUIRE_EXCEPTION(
        setgasparam(21000,21000,21000,21000,2899, evm_account_name),
        eosio_assert_message_exception,
        eosio_assert_message_is("G_sset must >= 2900"));

    updtgasparam(asset(10'0000, native_symbol), 1'000'000'000, evm_account_name);

    setgasparam(21000,21000,21000,21000,2900, evm_account_name);

    {
        evm_eoa recipient;
        silkworm::Transaction tx{
            .type = silkworm::Transaction::Type::kLegacy,
            .max_priority_fee_per_gas = 1'000'000'000,
            .max_fee_per_gas = 1'000'000'000,
            .gas_limit = 21000,
            .to = recipient.address,
            .value = 1,
        };
        uint64_t cur_nonce = faucet_eoa.next_nonce;
        faucet_eoa.sign(tx);
        BOOST_REQUIRE_EXCEPTION(
            pushtx(tx),       
            eosio_assert_message_exception,
            eosio_assert_message_is("gas price is too low"));
        faucet_eoa.next_nonce = cur_nonce;
    }

    produce_block();
    produce_block();

    // new gas price take effect in the next evm block, configchange event before evmtx
    {
        evm_eoa recipient;
        silkworm::Transaction tx{
            .type = silkworm::Transaction::Type::kLegacy,
            .max_priority_fee_per_gas = 1'000'000'000,
            .max_fee_per_gas = 1'000'000'000,
            .gas_limit = 21000,
            .to = recipient.address,
            .value = 1,
        };
        faucet_eoa.sign(tx);
        chain::transaction_trace_ptr trace = pushtx(tx);
        BOOST_REQUIRE_EQUAL(trace->action_traces.size(), 3);
        BOOST_ASSERT(trace->action_traces.size() >= 3 && trace->action_traces[0].act.name == "pushtx"_n);
        BOOST_ASSERT(trace->action_traces.size() >= 3 && trace->action_traces[1].act.name == "configchange"_n);
        BOOST_ASSERT(trace->action_traces.size() >= 3 && trace->action_traces[2].act.name == "evmtx"_n);
    }

    // subsequence trxs will have no more configchange event
    {
        evm_eoa recipient;
        silkworm::Transaction tx{
            .type = silkworm::Transaction::Type::kLegacy,
            .max_priority_fee_per_gas = 1'000'000'000,
            .max_fee_per_gas = 1'000'000'000,
            .gas_limit = 21000,
            .to = recipient.address,
            .value = 2,
        };
        faucet_eoa.sign(tx);
        chain::transaction_trace_ptr trace = pushtx(tx);
        BOOST_REQUIRE_EQUAL(trace->action_traces.size(), 2);
        BOOST_ASSERT(trace->action_traces.size() >= 2 && trace->action_traces[0].act.name == "pushtx"_n);
        BOOST_ASSERT(trace->action_traces.size() >= 2 && trace->action_traces[1].act.name == "evmtx"_n);
    }

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
