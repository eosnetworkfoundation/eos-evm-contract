#include "basic_evm_tester.hpp"

using namespace eosio::testing;
using namespace evm_test;

#include <eosevm/block_mapping.hpp>

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

   uint64_t gas_txnewaccount = 0;
   uint64_t gas_newaccount   = 25000;
   uint64_t gas_txcreate     = 32000;
   uint64_t gas_codedeposit  = 200;
   uint64_t gas_sset         = 20000;
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

    // zero gas price
    BOOST_REQUIRE_EXCEPTION(
        updtgasparam(asset(10'0000, native_symbol), 0, evm_account_name),
        eosio_assert_message_exception,
        eosio_assert_message_is("zero gas price is not allowed"));
    
     // fee too high
    BOOST_REQUIRE_EXCEPTION(
        updtgasparam(asset((1ull << 62) - 1, native_symbol), 1'000'000'000, evm_account_name),
        eosio_assert_message_exception,
        eosio_assert_message_is("gas_per_byte too big"));

    setversion(1, evm_account_name);

    produce_block();
    produce_block();

    // kick of setverion, evmtx event generated
    {
        evm_eoa recipient;
        silkworm::Transaction tx{
            silkworm::UnsignedTransaction {
                .type = silkworm::TransactionType::kLegacy,
                .max_priority_fee_per_gas = suggested_gas_price,
                .max_fee_per_gas = suggested_gas_price,
                .gas_limit = 21000,
                .to = recipient.address,
                .value = 1,
            }
        };
        faucet_eoa.sign(tx);
        chain::transaction_trace_ptr trace = pushtx(tx);
        BOOST_REQUIRE_EQUAL(trace->action_traces.size(), 2);
        BOOST_ASSERT(trace->action_traces.size() >= 2 && trace->action_traces[0].act.name == "pushtx"_n);
        BOOST_ASSERT(trace->action_traces.size() >= 2 && trace->action_traces[1].act.name == "evmtx"_n);
    }

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
        eosio_assert_message_is("gas_sset too small"));

    // Change in the gas parameters now takes effect immediately, but not gas price
    updtgasparam(asset(10'0000, native_symbol), 1'000'000'000, evm_account_name);

    auto cfg = get_config();
    eosevm::block_mapping bm(cfg.genesis_time.sec_since_epoch());

    auto t0 = (control->pending_block_time() + fc::seconds(price_queue_grace_period)).time_since_epoch().count();
    auto b0 = bm.timestamp_to_evm_block_num(t0)+1;
    while(bm.timestamp_to_evm_block_num(control->pending_block_time().time_since_epoch().count()) != b0) {
        produce_blocks(1);
    }

    // Process price queue
    transfer_token("alice"_n, evm_account_name, make_asset(1), evm_account_name.to_string());

    setgasparam(21000,21000,21000,21000,2900, evm_account_name);

    {
        evm_eoa recipient;
        silkworm::Transaction tx{
            silkworm::UnsignedTransaction {
                .type = silkworm::TransactionType::kLegacy,
                .max_priority_fee_per_gas = 1'000'000'000,
                .max_fee_per_gas = 1'000'000'000,
                .gas_limit = 21000,
                .to = recipient.address,
                .value = 1,
            }
        };
        faucet_eoa.sign(tx);
        BOOST_CHECK_NO_THROW(pushtx(tx));
    }

    produce_block();
    produce_block();

    // new gas params take effect in the next evm block, configchange event before evmtx
    {
        evm_eoa recipient;
        silkworm::Transaction tx{
            silkworm::UnsignedTransaction {
                .type = silkworm::TransactionType::kLegacy,
                .max_priority_fee_per_gas = 1'000'000'000,
                .max_fee_per_gas = 1'000'000'000,
                .gas_limit = 21000,
                .to = recipient.address,
                .value = 1,
            }
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
            silkworm::UnsignedTransaction {
                .type = silkworm::TransactionType::kLegacy,
                .max_priority_fee_per_gas = 1'000'000'000,
                .max_fee_per_gas = 1'000'000'000,
                .gas_limit = 21000,
                .to = recipient.address,
                .value = 2,
            }
        };
        faucet_eoa.sign(tx);
        chain::transaction_trace_ptr trace = pushtx(tx);
        BOOST_REQUIRE_EQUAL(trace->action_traces.size(), 2);
        BOOST_ASSERT(trace->action_traces.size() >= 2 && trace->action_traces[0].act.name == "pushtx"_n);
        BOOST_ASSERT(trace->action_traces.size() >= 2 && trace->action_traces[1].act.name == "evmtx"_n);
    }

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(gas_param_traces, gas_param_evm_tester) try {

    init();

    setversion(3, evm_account_name);
    produce_blocks(2);

    setgasparam(1, 2, 3, 4, 2900, evm_account_name);
    produce_blocks(3);

    evm_eoa evm1;
    auto trace = transfer_token("alice"_n, evm_account_name, make_asset(1), evm1.address_0x());

    BOOST_REQUIRE_EQUAL(trace->action_traces.size(), 5);
    BOOST_ASSERT(trace->action_traces[0].act.name == "transfer"_n);
    BOOST_ASSERT(trace->action_traces[1].act.name == "transfer"_n);
    BOOST_ASSERT(trace->action_traces[2].act.name == "transfer"_n);
    BOOST_ASSERT(trace->action_traces[3].act.name == "configchange"_n);
    BOOST_ASSERT(trace->action_traces[4].act.name == "evmtx"_n);

    auto cp = fc::raw::unpack<consensus_parameter_data_type>(trace->action_traces[3].act.data);

    std::visit([&](auto& v){
        BOOST_REQUIRE_EQUAL(v.gas_parameter.gas_txnewaccount, 1);
        BOOST_REQUIRE_EQUAL(v.gas_parameter.gas_newaccount, 2);
        BOOST_REQUIRE_EQUAL(v.gas_parameter.gas_txcreate, 3);
        BOOST_REQUIRE_EQUAL(v.gas_parameter.gas_codedeposit, 4);
        BOOST_REQUIRE_EQUAL(v.gas_parameter.gas_sset, 2900);
    }, cp);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(gas_param_G_txnewaccount, gas_param_evm_tester) try {

    uint64_t suggested_gas_price = 150'000'000'000ull;
    init(15555, suggested_gas_price);

    produce_block();
    fund_evm_faucet();
    produce_block();

    setversion(1, evm_account_name);
    produce_block();
    produce_block();

    // Fund evm1 address with 100 EOS
    evm_eoa evm1, evm2, evm3;
    transfer_token("alice"_n, "evm"_n, make_asset(100'0000), evm1.address_0x());

    // transfer to non-existent account (21k gas used)
    auto txn = generate_tx(evm2.address, 1, 21'000);
    evm1.sign(txn);
    pushtx(txn);
    auto bal = evm_balance(evm2.address);
    BOOST_CHECK(*bal == 1);

    // Set gas_txnewaccount = 1000
    setgasparam(1000, gas_newaccount, gas_txcreate, gas_codedeposit, gas_sset, evm_account_name);
    produce_block();
    produce_block();

    // transfer to non-existent account fail (need 22k)
    txn = generate_tx(evm3.address, 1, 21'000);
    evm1.sign(txn);
    pushtx(txn);
    bal = evm_balance(evm3.address);
    BOOST_CHECK(!bal.has_value());

    // transfer to non-existent account (22k used)
    txn = generate_tx(evm3.address, 1, 22'000);
    evm1.sign(txn);
    pushtx(txn);
    bal = evm_balance(evm3.address);
    BOOST_ASSERT(*bal == 1);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(gas_param_G_newaccount, gas_param_evm_tester) try {

    uint64_t suggested_gas_price = 150'000'000'000ull;
    init(15555, suggested_gas_price);

    produce_block();
    fund_evm_faucet();
    produce_block();

    setversion(1, evm_account_name);
    produce_block();
    produce_block();

    // Fund evm1 address with 100 EOS
    evm_eoa evm1;
    transfer_token("alice"_n, "evm"_n, make_asset(100'0000), evm1.address_0x());

    // // SPDX-License-Identifier: MIT
    // pragma solidity ^0.8.0;
    // contract Creator {
    //     function sendTo(address payable dst) public {
    //         dst.transfer(1);
    //     }
    //     receive() external payable {}
    //     fallback() external payable {}
    // }
    const std::string contract_bytecode = "608060405234801561001057600080fd5b50610165806100206000396000f3fe6080604052600436106100225760003560e01c8063e6d252451461002b57610029565b3661002957005b005b34801561003757600080fd5b50610052600480360381019061004d9190610102565b610054565b005b8073ffffffffffffffffffffffffffffffffffffffff166108fc60019081150290604051600060405180830381858888f1935050505015801561009b573d6000803e3d6000fd5b5050565b600080fd5b600073ffffffffffffffffffffffffffffffffffffffff82169050919050565b60006100cf826100a4565b9050919050565b6100df816100c4565b81146100ea57600080fd5b50565b6000813590506100fc816100d6565b92915050565b6000602082840312156101185761011761009f565b5b6000610126848285016100ed565b9150509291505056fea2646970667358221220d80ddae6515d1642665d8d23a536c6d60bfc9a5e3137367f0a4d589a1b0a4d7c64736f6c634300080d0033";

    // Deploy and fund the contract with 1000 Wei
    auto contract_address = deploy_contract(evm1, evmc::from_hex(contract_bytecode).value());
    auto txn = generate_tx(contract_address, 1000, 210000);
    evm1.sign(txn);
    pushtx(txn);

    auto call_contract = [&](auto& eoa, const evmc::address& dst) -> auto {
        auto pre = evm_balance(eoa);
        auto txn = generate_tx(contract_address, 0, 1'000'000);
        txn.data = evmc::from_hex("e6d25245").value(); //sha3(sendTo(address))
        txn.data += evmc::from_hex("000000000000000000000000").value();
        txn.data += dst;

        eoa.sign(txn);
        pushtx(txn);
        auto post = evm_balance(eoa);
        BOOST_REQUIRE(pre.has_value() && post.has_value());
        return (*pre - *post)/suggested_gas_price - tx_data_cost(txn);
    };

    evm_eoa evm2;
    auto gas_used1 = call_contract(evm1, evm2.address);
    BOOST_REQUIRE(evm_balance(evm2) == 1);

    // Set gas_newaccount = 26000, previous was 25000
    setgasparam(gas_txnewaccount, 26000, gas_txcreate, gas_codedeposit, gas_sset, evm_account_name);
    produce_block();
    produce_block();

    evm_eoa evm3;
    auto gas_used2 = call_contract(evm1, evm3.address);
    BOOST_REQUIRE(evm_balance(evm3) == 1);

    BOOST_REQUIRE(gas_used2-gas_used1 == 1000);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(gas_param_G_txcreate, gas_param_evm_tester) try {

    uint64_t suggested_gas_price = 150'000'000'000ull;
    init(15555, suggested_gas_price);

    produce_block();
    fund_evm_faucet();
    produce_block();

    setversion(1, evm_account_name);
    produce_block();
    produce_block();

    // Fund evm1 address with 100 EOS
    evm_eoa evm1;
    transfer_token("alice"_n, "evm"_n, make_asset(100'0000), evm1.address_0x());

    const std::string contract_bytecode = "600d600c600039600d6000f3600F60005260006001600FF000";

    auto deploy_contract_gas = [&](auto& eoa, const auto& bytecode){
        auto pre = evm_balance(eoa);
        auto contract_address = deploy_contract(eoa, evmc::from_hex(bytecode).value());
        auto post = evm_balance(eoa);
        BOOST_REQUIRE(pre.has_value() && post.has_value());
        return std::make_tuple((*pre - *post)/suggested_gas_price, contract_address);
    };

    auto [gas_used1, contract_address1] = deploy_contract_gas(evm1, contract_bytecode);
    produce_block();
    produce_block();

    auto call_contract = [&](auto& eoa) -> auto {
        auto pre = evm_balance(eoa);
        auto txn = generate_tx(contract_address1, 0, 1'000'000);
        eoa.sign(txn);
        pushtx(txn);
        auto post = evm_balance(eoa);
        BOOST_REQUIRE(pre.has_value() && post.has_value());
        return (*pre - *post)/suggested_gas_price;
    };

    auto gas_used = call_contract(evm1);
    BOOST_CHECK(gas_used == 21000 + 32000 + 21);

    // Set gas_txcreate = 0
    setgasparam(gas_txnewaccount, gas_newaccount, 0, gas_codedeposit, gas_sset, evm_account_name);
    produce_block();
    produce_block();

    gas_used = call_contract(evm1);
    BOOST_CHECK(gas_used == 21000 + 0 + 21);

    auto [gas_used2, contract_address2] = deploy_contract_gas(evm1, contract_bytecode);

    // Check gas used to deploy the contract (from the tx)
    BOOST_REQUIRE(gas_used1 == 32000 + 21000 + 13*200 + 366);
    BOOST_REQUIRE(gas_used2 ==     0 + 21000 + 13*200 + 366);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(gas_param_G_codedeposit, gas_param_evm_tester) try {

    uint64_t suggested_gas_price = 150'000'000'000ull;
    init(15555, suggested_gas_price);

    produce_block();
    fund_evm_faucet();
    produce_block();

    setversion(1, evm_account_name);
    produce_block();
    produce_block();

    // Fund evm1 address with 100 EOS
    evm_eoa evm1;
    transfer_token("alice"_n, "evm"_n, make_asset(100'0000), evm1.address_0x());

    const std::string contract_bytecode = "600d600c600039600d6000f3600F60005260006001600FF000";

    auto deploy_contract_gas = [&](auto& eoa, const auto& bytecode){
        auto pre = evm_balance(eoa);
        auto contract_address = deploy_contract(eoa, evmc::from_hex(bytecode).value());
        auto post = evm_balance(eoa);
        BOOST_REQUIRE(pre.has_value() && post.has_value());
        return std::make_tuple((*pre - *post)/suggested_gas_price, contract_address);
    };

    // code_len = 13
    auto [gas_used1, contract_address1] = deploy_contract_gas(evm1, contract_bytecode);
    BOOST_REQUIRE(gas_used1 == 32000 + 21000 + 13*200 + 366);

    // Set gas_codedeposit = 0
    setgasparam(gas_txnewaccount, gas_newaccount, gas_txcreate, 0, gas_sset, evm_account_name);
    produce_block();
    produce_block();

    auto [gas_used2, contract_address2] = deploy_contract_gas(evm1, contract_bytecode);
    BOOST_REQUIRE(gas_used2 == 32000 + 21000 + 13*0 + 366);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(gas_param_G_sset, gas_param_evm_tester) try {

    uint64_t suggested_gas_price = 150'000'000'000ull;
    init(15555, suggested_gas_price);

    produce_block();
    fund_evm_faucet();
    produce_block();

    setversion(1, evm_account_name);
    produce_block();
    produce_block();

    // Fund evm1 address with 100 EOS
    evm_eoa evm1;
    transfer_token("alice"_n, "evm"_n, make_asset(100'0000), evm1.address_0x());

    // pragma solidity >=0.8.2 <0.9.0;
    // contract Storage {
    //     uint256 number;
    //     function store(uint256 num) public {
    //         number = num;
    //     }
    //     function retrieve() public view returns (uint256){
    //         return number;
    //     }
    // }

    const std::string contract_bytecode = "608060405234801561001057600080fd5b50610150806100206000396000f3fe608060405234801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd5b610043610075565b60405161005091906100a1565b60405180910390f35b610073600480360381019061006e91906100ed565b61007e565b005b60008054905090565b8060008190555050565b6000819050919050565b61009b81610088565b82525050565b60006020820190506100b66000830184610092565b92915050565b600080fd5b6100ca81610088565b81146100d557600080fd5b50565b6000813590506100e7816100c1565b92915050565b600060208284031215610103576101026100bc565b5b6000610111848285016100d8565b9150509291505056fea2646970667358221220b25953072dcb52f66b23db9a7a904276bfa83cfa62a0e8feaf281d9d8c0ed59664736f6c634300080d0033";
    auto contract_address = deploy_contract(evm1, evmc::from_hex(contract_bytecode).value());
    uint64_t contract_account_id = find_account_by_address(contract_address).value().id;

    auto pad = [](const std::string& s){
        const size_t l = 64;
        if (s.length() >= l) return s;
        size_t pl = l - s.length();
        return std::string(pl, '0') + s;
    };

    auto set_value = [&](auto& eoa, const intx::uint256& v) -> auto {
        auto pre = evm_balance(eoa);
        auto txn = generate_tx(contract_address, 0, 1'000'000);
        txn.data = evmc::from_hex("6057361d").value(); //sha3(store(uint256))
        txn.data += evmc::from_hex(pad(intx::hex(v))).value();
        eoa.sign(txn);
        pushtx(txn);
        auto post = evm_balance(eoa);
        BOOST_REQUIRE(pre.has_value() && post.has_value());
        return (*pre - *post)/suggested_gas_price - tx_data_cost(txn);
    };

    auto gas_used1 = set_value(evm1, intx::uint256{3});

    // Set gas_sset = 20001
    setgasparam(gas_txnewaccount, gas_newaccount, gas_txcreate, gas_codedeposit, 20001, evm_account_name);
    produce_block();
    produce_block();
    produce_block();

    set_value(evm1, intx::uint256{0});
    produce_block();
    produce_block();

    auto gas_used2 = set_value(evm1, intx::uint256{4});

    BOOST_REQUIRE(gas_used1 + 1 == gas_used2);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(gas_limit_internal_transaction, gas_param_evm_tester) try {

    uint64_t suggested_gas_price = 150'000'000'000ull;
    init(15555, suggested_gas_price);

    produce_block();
    fund_evm_faucet();
    produce_block();

    setversion(1, evm_account_name);
    produce_block();
    produce_block();

    // Send 0.0001 EOS to a never used address (21000 GAS)
    evm_eoa evm1;
    auto trace = transfer_token("alice"_n, "evm"_n, make_asset(1), evm1.address_0x());
    auto tx = get_tx_from_trace(trace->action_traces[3].act.data);
    BOOST_REQUIRE(tx.gas_limit == 21000);
    BOOST_REQUIRE(evm_balance(evm1) == 100000000000000);

    // Set gas_txnewaccount = 1
    setgasparam(1, gas_newaccount, gas_txcreate, gas_codedeposit, gas_sset, evm_account_name);
    produce_block();
    produce_block();
    produce_block();

    // Send 0.0001 EOS to a never used address (21001 GAS)
    evm_eoa evm2;
    trace = transfer_token("alice"_n, "evm"_n, make_asset(1), evm2.address_0x());
    tx = get_tx_from_trace(trace->action_traces[4].act.data);
    BOOST_REQUIRE(tx.gas_limit == 21001);
    BOOST_REQUIRE(evm_balance(evm2) == 100000000000000);

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
