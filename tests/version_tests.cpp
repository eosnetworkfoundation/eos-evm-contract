#include "basic_evm_tester.hpp"
#include <silkworm/execution/address.hpp>
#include <silkworm/types/transaction.hpp>
#include <eosevm/block_mapping.hpp>

using namespace evm_test;
using eosio::testing::eosio_assert_message_is;

struct version_tester : basic_evm_tester {
  version_tester() {
    create_accounts({"alice"_n});
    transfer_token(faucet_account_name, "alice"_n, make_asset(10000'0000));
    init();
  }

  /*
  // SPDX-License-Identifier: GPL-3.0
  pragma solidity >=0.8.2 <0.9.0;
  contract Increment {
      mapping (address => uint256) values;
      function increment() public {
          values[msg.sender]++;
      }
      function retrieve(address a) public view returns (uint256){
          return values[a];
      }
  }
  */

  const std::string contract_bytecode =
        "608060405234801561001057600080fd5b50610284806100206000396000f3fe608060405234801561001057600080fd5b50600436106100365760003560e01c80630a79309b1461003b578063d09de08a1461006b575b600080fd5b61005560048036038101906100509190610176565b610075565b60405161006291906101bc565b60405180910390f35b6100736100bd565b005b60008060008373ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff168152602001908152602001600020549050919050565b6000803373ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff168152602001908152602001600020600081548092919061010c90610206565b9190505550565b600080fd5b600073ffffffffffffffffffffffffffffffffffffffff82169050919050565b600061014382610118565b9050919050565b61015381610138565b811461015e57600080fd5b50565b6000813590506101708161014a565b92915050565b60006020828403121561018c5761018b610113565b5b600061019a84828501610161565b91505092915050565b6000819050919050565b6101b6816101a3565b82525050565b60006020820190506101d160008301846101ad565b92915050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052601160045260246000fd5b6000610211826101a3565b91507fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff8203610243576102426101d7565b5b60018201905091905056fea26469706673582212205ef4ef3ff462d43a7ce545531c2f77184355d61dcc93de7deb2d57696ceb073164736f6c63430008110033";

  auto deploy_contract(evm_eoa& eoa, evmc::bytes bytecode)
  {
    uint64_t nonce = eoa.next_nonce;

    const auto gas_price = get_config().gas_price;

    silkworm::Transaction tx{
        .type = silkworm::Transaction::Type::kLegacy,
        .max_priority_fee_per_gas = gas_price,
        .max_fee_per_gas = gas_price,
        .gas_limit = 10'000'000,
        .data = std::move(bytecode),
    };

    eoa.sign(tx);
    auto trace = pushtx(tx);

    return std::make_tuple(trace, silkworm::create_address(eoa.address, nonce));
  }

  auto deploy_test_contract(evm_eoa& eoa) {
    return deploy_contract(eoa, evmc::from_hex(contract_bytecode).value());
  }

  intx::uint256 retrieve(const evmc::address& contract_addr, const evmc::address& account) {
    exec_input input;
    input.context = {};
    input.to = bytes{std::begin(contract_addr.bytes), std::end(contract_addr.bytes)};

    silkworm::Bytes data;
    data += evmc::from_hex("0a79309b").value();   // sha3(retrieve(address))[:4]
    data += silkworm::to_bytes32(account);
    input.data = bytes{data.begin(), data.end()};

    auto res = exec(input, {});
    auto out = fc::raw::unpack<exec_output>(res->action_traces[0].return_value);
    BOOST_REQUIRE(out.status == 0);
    BOOST_REQUIRE(out.data.size() == 32);

    auto count = intx::be::unsafe::load<intx::uint256>(reinterpret_cast<const uint8_t*>(out.data.data()));
    return count;
  }

};

BOOST_AUTO_TEST_SUITE(verion_evm_tests)
BOOST_FIXTURE_TEST_CASE(set_version, version_tester) try {

    auto config = get_config();
    eosevm::block_mapping bm(config.genesis_time.sec_since_epoch());

    BOOST_REQUIRE_EXCEPTION(setversion(0, "alice"_n),
        missing_auth_exception, eosio::testing::fc_exception_message_starts_with("missing authority"));

    BOOST_REQUIRE_EXCEPTION(setversion(0, evm_account_name),
        eosio_assert_message_exception,
        eosio_assert_message_is("new version must be greater than the active one"));

    BOOST_REQUIRE_EXCEPTION(setversion(2, evm_account_name),
        eosio_assert_message_exception,
        eosio_assert_message_is("Unsupported version"));

    setversion(1, evm_account_name);
    config = get_config();

    BOOST_CHECK_EQUAL(config.evm_version.has_value(), true);
    BOOST_CHECK_EQUAL(config.evm_version.value().cached_version, 0);
    BOOST_CHECK_EQUAL(config.evm_version.value().pending_version.has_value(), true);
    BOOST_REQUIRE(config.evm_version.value().pending_version.value().time == control->pending_block_time());
    BOOST_REQUIRE(config.evm_version.value().pending_version.value().version == 1);

    // Fund evm1 address with 100 EOS (this will NOT trigger an update of the evm version)
    evm_eoa evm1;
    const int64_t to_bridge = 1000000;
    transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), evm1.address_0x());

    config = get_config();
    BOOST_CHECK_EQUAL(config.evm_version.has_value(), true);
    BOOST_CHECK_EQUAL(config.evm_version.value().cached_version, 0);
    BOOST_CHECK_EQUAL(config.evm_version.value().pending_version.has_value(), true);
    BOOST_REQUIRE(config.evm_version.value().pending_version.value().time == control->pending_block_time());
    BOOST_REQUIRE(config.evm_version.value().pending_version.value().version == 1);

    // Produce blocks until the next EVM block (pending version activation)
    const auto& pending_version = config.evm_version.value().pending_version.value();
    auto b0 = bm.timestamp_to_evm_block_num(pending_version.time.time_since_epoch().count());
    while(bm.timestamp_to_evm_block_num(control->pending_block_time().time_since_epoch().count()) <= b0) {
      produce_block();
    }

    // Fund evm1 address with 100 more EOS (this will trigger an update of the evm version)
    transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), evm1.address_0x());
    config = get_config();

    BOOST_CHECK_EQUAL(config.evm_version.has_value(), true);
    BOOST_CHECK_EQUAL(config.evm_version.value().cached_version, 1);
    BOOST_CHECK_EQUAL(config.evm_version.value().pending_version.has_value(), false);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(traces_in_different_eosevm_version, version_tester) try {

    auto config = get_config();

    evm_eoa evm1;
    const int64_t to_bridge = 1000000;

    // Open alice internal balance
    open("alice"_n);
    transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), "alice");
    auto alice_addr = make_reserved_address("alice"_n.to_uint64_t());

    // Fund evm1 address
    // Test traces of `handle_evm_transfer` (EVM VERSION=0)
    auto trace = transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), evm1.address_0x());
    BOOST_REQUIRE(trace->action_traces.size() == 4);
    BOOST_REQUIRE(trace->action_traces[0].act.account == "eosio.token"_n);
    BOOST_REQUIRE(trace->action_traces[0].act.name == "transfer"_n);
    BOOST_REQUIRE(trace->action_traces[1].act.account == "eosio.token"_n);
    BOOST_REQUIRE(trace->action_traces[1].act.name == "transfer"_n);
    BOOST_REQUIRE(trace->action_traces[2].act.account == "eosio.token"_n);
    BOOST_REQUIRE(trace->action_traces[2].act.name == "transfer"_n);
    BOOST_REQUIRE(trace->action_traces[3].act.account == "evm"_n);
    BOOST_REQUIRE(trace->action_traces[3].act.name == "pushtx"_n);

    // Test traces of `pushtx` (EVM VERSION=0)
    auto [trace2, contract_address] = deploy_test_contract(evm1);
    BOOST_REQUIRE(trace2->action_traces.size() == 1);
    BOOST_REQUIRE(trace2->action_traces[0].act.account == "evm"_n);
    BOOST_REQUIRE(trace2->action_traces[0].act.name == "pushtx"_n);

    auto to = evmc::bytes{std::begin(contract_address.bytes), std::end(contract_address.bytes)};
    auto data = evmc::from_hex("0xd09de08a");

    // Test traces of `call` (EVM VERSION=0)
    trace = call("alice"_n, to, silkworm::Bytes(evmc::bytes32{}), *data, 1000000, "alice"_n);
    BOOST_REQUIRE(trace->action_traces.size() == 2);
    BOOST_REQUIRE(trace->action_traces[0].act.account == "evm"_n);
    BOOST_REQUIRE(trace->action_traces[0].act.name == "call"_n);
    BOOST_REQUIRE(trace->action_traces[1].act.account == "evm"_n);
    BOOST_REQUIRE(trace->action_traces[1].act.name == "pushtx"_n);

    // Test traces of `admincall` (EVM VERSION=0)
    auto from = evmc::bytes{std::begin(alice_addr.bytes), std::end(alice_addr.bytes)};
    trace = admincall(from, to, silkworm::Bytes(evmc::bytes32{}), *data, 1000000, evm_account_name);
    BOOST_REQUIRE(trace->action_traces.size() == 2);
    BOOST_REQUIRE(trace->action_traces[0].act.account == "evm"_n);
    BOOST_REQUIRE(trace->action_traces[0].act.name == "admincall"_n);
    BOOST_REQUIRE(trace->action_traces[1].act.account == "evm"_n);
    BOOST_REQUIRE(trace->action_traces[1].act.name == "pushtx"_n);

    /////////////////////////////////////
    /// change EOS EVM VERSION => 1   ///
    /////////////////////////////////////

    setversion(1, evm_account_name);
    produce_blocks(2);

    auto get_tx_from_trace = [&](const bytes& v) {
      auto evmtx_v = fc::raw::unpack<evm_test::evmtx_type>(v.data(), v.size());

      BOOST_REQUIRE(std::holds_alternative<evm_test::evmtx_v0>(evmtx_v));

      const auto& evmtx = std::get<evm_test::evmtx_v0>(evmtx_v);
      BOOST_REQUIRE(evmtx.eos_evm_version == 1);

      silkworm::Transaction tx;
      silkworm::ByteView bv{(const uint8_t*)evmtx.rlptx.data(), evmtx.rlptx.size()};
      silkworm::rlp::decode(bv, tx);
      BOOST_REQUIRE(bv.empty());

      return tx;
    };

    // Test traces of `handle_evm_transfer` (EVM VERSION=1)
    trace = transfer_token("alice"_n, evm_account_name, make_asset(to_bridge), evm1.address_0x());
    BOOST_REQUIRE(trace->action_traces.size() == 4);
    BOOST_REQUIRE(trace->action_traces[0].act.account == "eosio.token"_n);
    BOOST_REQUIRE(trace->action_traces[0].act.name == "transfer"_n);
    BOOST_REQUIRE(trace->action_traces[1].act.account == "eosio.token"_n);
    BOOST_REQUIRE(trace->action_traces[1].act.name == "transfer"_n);
    BOOST_REQUIRE(trace->action_traces[2].act.account == "eosio.token"_n);
    BOOST_REQUIRE(trace->action_traces[2].act.name == "transfer"_n);
    BOOST_REQUIRE(trace->action_traces[3].act.account == "evm"_n);
    BOOST_REQUIRE(trace->action_traces[3].act.name == "evmtx"_n);

    auto tx = get_tx_from_trace(trace->action_traces[3].act.data);
    BOOST_REQUIRE(tx.value == intx::uint256(balance_and_dust{make_asset(to_bridge), 0}));
    BOOST_REQUIRE(tx.to == evm1.address);

    // Test traces of `pushtx` (EVM VERSION=1)
    silkworm::Transaction txin {
      .type = silkworm::Transaction::Type::kLegacy,
      .max_priority_fee_per_gas = config.gas_price,
      .max_fee_per_gas = config.gas_price,
      .gas_limit = 10'000'000,
      .to = contract_address,
      .data = *data
    };

    evm1.sign(txin);
    trace = pushtx(txin);

    BOOST_REQUIRE(trace->action_traces.size() == 2);
    BOOST_REQUIRE(trace->action_traces[0].act.account == "evm"_n);
    BOOST_REQUIRE(trace->action_traces[0].act.name == "pushtx"_n);
    BOOST_REQUIRE(trace->action_traces[1].act.account == "evm"_n);
    BOOST_REQUIRE(trace->action_traces[1].act.name == "evmtx"_n);

    auto txout = get_tx_from_trace(trace->action_traces[1].act.data);
    BOOST_REQUIRE(txout == txin);

    // Test traces of `call` (EVM VERSION=1)
    trace = call("alice"_n, to, silkworm::Bytes(evmc::bytes32{}), *data, 1000000, "alice"_n);
    BOOST_REQUIRE(trace->action_traces.size() == 2);
    BOOST_REQUIRE(trace->action_traces[0].act.account == "evm"_n);
    BOOST_REQUIRE(trace->action_traces[0].act.name == "call"_n);
    BOOST_REQUIRE(trace->action_traces[1].act.account == "evm"_n);
    BOOST_REQUIRE(trace->action_traces[1].act.name == "evmtx"_n);

    tx = get_tx_from_trace(trace->action_traces[1].act.data);
    tx.recover_sender();

    BOOST_REQUIRE(tx.value == intx::uint256(0));
    BOOST_REQUIRE(tx.to == contract_address);
    BOOST_REQUIRE(tx.data == *data);
    BOOST_REQUIRE(*tx.from == alice_addr);

    // Test traces of `admincall` (EVM VERSION=1)
    trace = admincall(from, to, silkworm::Bytes(evmc::bytes32{}), *data, 1000000, evm_account_name);
    BOOST_REQUIRE(trace->action_traces.size() == 2);
    BOOST_REQUIRE(trace->action_traces[0].act.account == "evm"_n);
    BOOST_REQUIRE(trace->action_traces[0].act.name == "admincall"_n);
    BOOST_REQUIRE(trace->action_traces[1].act.account == "evm"_n);
    BOOST_REQUIRE(trace->action_traces[1].act.name == "evmtx"_n);

    tx = get_tx_from_trace(trace->action_traces[1].act.data);
    tx.recover_sender();

    BOOST_REQUIRE(tx.value == intx::uint256(0));
    BOOST_REQUIRE(tx.to == contract_address);
    BOOST_REQUIRE(tx.data == *data);
    BOOST_REQUIRE(*tx.from == alice_addr);

    // Check 4 times call to `increment` from alice_addr
    BOOST_REQUIRE(retrieve(contract_address, alice_addr) == intx::uint256(4));

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
