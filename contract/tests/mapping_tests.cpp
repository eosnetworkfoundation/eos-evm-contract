#include "basic_evm_tester.hpp"

#include <evm_common/block_mapping.hpp>

using namespace eosio::testing;
using namespace evm_test;

struct mapping_evm_tester : basic_evm_tester
{

   // Compiled from BlockNumTimestamp.sol
   static constexpr const char* blocknum_timestamp_contract_bytecode_hex =
      "608060405234801561001057600080fd5b50436000819055504260018190555060dd8061002d6000396000f3fe6080604052348015600f"
      "57600080fd5b506004361060325760003560e01c806357e871e7146037578063b80777ea146051575b600080fd5b603d606b565b604051"
      "60489190608e565b60405180910390f35b60576071565b60405160629190608e565b60405180910390f35b60005481565b60015481565b"
      "6000819050919050565b6088816077565b82525050565b600060208201905060a160008301846081565b9291505056fea2646970667358"
      "221220574648f86a6daeaf2c309ad6d05df02cfd0d83cdcb153889194852dea07068dc64736f6c63430008120033";

   evm_eoa faucet_eoa;

   mapping_evm_tester() :
      faucet_eoa(evmc::from_hex("a3f1b69da92a0233ce29485d3049a4ace39e8d384bbc2557e3fc60940ce4e954").value())
   {}

   bool produce_blocks_until_timestamp_satisfied(std::function<bool(time_point)> cond, unsigned int limit = 10)
   {
      for (unsigned int blocks_produced = 0; blocks_produced < limit && !cond(control->pending_block_time());
           ++blocks_produced) {
         produce_block();
      }
      return cond(control->pending_block_time());
   }

   time_point_sec get_genesis_time() { return get_config().genesis_time; }

   void fund_evm_faucet()
   {
      transfer_token(faucet_account_name, evm_account_name, make_asset(100'0000), faucet_eoa.address_0x());
   }
};

BOOST_AUTO_TEST_SUITE(mapping_evm_tests)

BOOST_AUTO_TEST_CASE(block_mapping_tests)
try {
   using evm_common::block_mapping;

   {
      // Tests using default 1 second block interval

      block_mapping bm(10);

      BOOST_CHECK_EQUAL(bm.block_interval, 1); // Block interval should default to 1 second.
      BOOST_CHECK_EQUAL(bm.genesis_timestamp, 10);

      BOOST_CHECK_EQUAL(bm.timestamp_to_evm_block_num(9'500'000), 0);
      BOOST_CHECK_EQUAL(bm.timestamp_to_evm_block_num(10'000'000), 1);
      BOOST_CHECK_EQUAL(bm.timestamp_to_evm_block_num(10'500'000), 1);
      BOOST_CHECK_EQUAL(bm.timestamp_to_evm_block_num(11'000'000), 2);

      BOOST_CHECK_EQUAL(bm.evm_block_num_to_evm_timestamp(0), 10);
      // An EVM transaction in an Antelope block with timestamp 10 ends up in EVM block 1 which has timestamp 11. This
      // is intentional. It means that an EVM transaction included in the same block immediately after the init action
      // will end up in a block that has a timestamp greater than the genesis timestamp.
      BOOST_CHECK_EQUAL(bm.evm_block_num_to_evm_timestamp(1), 11);
      BOOST_CHECK_EQUAL(bm.evm_block_num_to_evm_timestamp(2), 12);
   }

   {
      // Tests using a 5 second block interval

      block_mapping bm(123, 5);

      BOOST_CHECK_EQUAL(bm.block_interval, 5);
      BOOST_CHECK_EQUAL(bm.genesis_timestamp, 123);

      BOOST_CHECK_EQUAL(bm.timestamp_to_evm_block_num(100'000'000), 0);
      BOOST_CHECK_EQUAL(bm.timestamp_to_evm_block_num(122'000'000), 0);
      BOOST_CHECK_EQUAL(bm.timestamp_to_evm_block_num(123'000'000), 1);
      BOOST_CHECK_EQUAL(bm.timestamp_to_evm_block_num(124'000'000), 1);
      BOOST_CHECK_EQUAL(bm.timestamp_to_evm_block_num(127'000'000), 1);
      BOOST_CHECK_EQUAL(bm.timestamp_to_evm_block_num(127'999'999), 1);
      BOOST_CHECK_EQUAL(bm.timestamp_to_evm_block_num(128'000'000), 2);

      BOOST_CHECK_EQUAL(bm.evm_block_num_to_evm_timestamp(0), 123);
      BOOST_CHECK_EQUAL(bm.evm_block_num_to_evm_timestamp(1), 128);
      BOOST_CHECK_EQUAL(bm.evm_block_num_to_evm_timestamp(2), 133);
   }
}
FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(init_on_second_boundary, mapping_evm_tester)
try {
   auto timestamp_at_second_boundary = [](time_point timestamp) -> bool {
      return timestamp.time_since_epoch().count() % 1'000'000 == 0;
   };
   BOOST_REQUIRE(produce_blocks_until_timestamp_satisfied(timestamp_at_second_boundary));

   init();
   time_point_sec expected_genesis_time = control->pending_block_time(); // Rounds down to nearest second.

   time_point_sec actual_genesis_time = get_genesis_time();
   ilog("Genesis time: ${time}", ("time", actual_genesis_time));

   BOOST_REQUIRE_EQUAL(actual_genesis_time.sec_since_epoch(), expected_genesis_time.sec_since_epoch());
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(init_not_on_second_boundary, mapping_evm_tester)
try {
   auto timestamp_not_at_second_boundary = [](time_point timestamp) -> bool {
      return timestamp.time_since_epoch().count() % 1'000'000 != 0;
   };
   BOOST_REQUIRE(produce_blocks_until_timestamp_satisfied(timestamp_not_at_second_boundary));

   init();
   time_point_sec expected_genesis_time = control->pending_block_time(); // Rounds down to nearest second.

   time_point_sec actual_genesis_time = get_genesis_time();
   ilog("Genesis time: ${time}", ("time", actual_genesis_time));

   BOOST_REQUIRE_EQUAL(actual_genesis_time.sec_since_epoch(), expected_genesis_time.sec_since_epoch());
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(values_seen_by_contracts, mapping_evm_tester)
try {
   produce_block();

   auto timestamp_not_at_second_boundary = [](time_point timestamp) -> bool {
      return timestamp.time_since_epoch().count() % 1'000'000 != 0;
   };
   BOOST_REQUIRE(produce_blocks_until_timestamp_satisfied(timestamp_not_at_second_boundary));

   evm_common::block_mapping bm(control->pending_block_time().sec_since_epoch());
   ilog("Genesis timestamp should be ${genesis_timestamp} since init action is targetting block at timestamp "
        "${block_time}",
        ("genesis_timestamp", bm.genesis_timestamp)("block_time", control->pending_block_time()));
   control->abort_block();

   auto dump_accounts = [&]() {
      scan_accounts([](evm_test::account_object&& account) -> bool {
         idump((account));
         return false;
      });
   };

   auto dump_account_storage = [&](uint64_t account_id) {
      scan_account_storage(account_id, [](storage_slot&& slot) -> bool {
         idump((slot));
         return false;
      });
   };

   auto get_stored_blocknum_and_timestamp = [&](uint64_t account_id) -> std::pair<uint32_t, uint64_t> {
      uint64_t block_number = 0;
      uint32_t block_timestamp = 0;

      static const intx::uint256 threshold = (1_u256 << 64);

      unsigned int ordinal = 0;
      bool successful_scan = scan_account_storage(account_id, [&](storage_slot&& slot) -> bool {
         if (ordinal == 0) {
            // The first storage slot of the contract is the block number.

            // ilog("block number = ${bn}", ("bn", slot.value));

            BOOST_REQUIRE(slot.key == 0);
            BOOST_REQUIRE(slot.value < threshold);
            block_number = slot.value[0];
         } else if (ordinal == 1) {
            // The second storage slot of the contract is the block timestamp.

            // ilog("timestamp = ${time}", ("time", slot.value));

            BOOST_REQUIRE(slot.key == 1);
            BOOST_REQUIRE(slot.value < threshold);
            block_timestamp = slot.value[0];
         } else {
            BOOST_ERROR("unexpected storage in contract");
            return true;
         }
         ++ordinal;
         return false;
      });
      BOOST_REQUIRE(successful_scan);
      BOOST_REQUIRE_EQUAL(ordinal, 2);

      BOOST_CHECK(block_number <= std::numeric_limits<uint32_t>::max());

      return {static_cast<uint32_t>(block_number), block_timestamp};
   };

   auto blocknum_timestamp_contract_bytecode = evmc::from_hex(blocknum_timestamp_contract_bytecode_hex).value();

   {
      // Test transaction within virtual block 1. (Same block as init action.)
      speculative_block_starter<decltype(*this)> sb{*this};

      ilog("Initial test within speculative block at time: ${time} (${time_sec})",
           ("time", control->pending_block_time())("time_sec", control->pending_block_time().sec_since_epoch()));

      uint32_t expected_evm_block_num =
         bm.timestamp_to_evm_block_num(control->pending_block_time().time_since_epoch().count());
      uint64_t expected_evm_timestamp = bm.evm_block_num_to_evm_timestamp(expected_evm_block_num);

      init();

      BOOST_REQUIRE_EQUAL(expected_evm_block_num, 1);
      BOOST_REQUIRE_EQUAL(expected_evm_timestamp, bm.genesis_timestamp + 1);

      fund_evm_faucet();

      dump_accounts();

      auto contract_address = deploy_contract(faucet_eoa, blocknum_timestamp_contract_bytecode);
      ilog("address of deployed contract is ${address}", ("address", contract_address));

      dump_accounts();

      auto contract_account = find_account_by_address(contract_address);

      BOOST_REQUIRE(contract_account.has_value());

      {
         // Scan for account should also find the same account
         auto acct = scan_for_account_by_address(contract_address);
         BOOST_REQUIRE(acct.has_value());
         BOOST_CHECK_EQUAL(acct->id, contract_account->id);
      }

      dump_account_storage(contract_account->id);

      auto [block_num, timestamp] = get_stored_blocknum_and_timestamp(contract_account->id);

      BOOST_CHECK_EQUAL(block_num, 1);
      BOOST_CHECK_EQUAL(timestamp, bm.genesis_timestamp + 1);

      faucet_eoa.next_nonce = 0;
   }

   // Now actually commit the init action into a block to do further testing on top of that state in later blocks.
   {
      control->start_block(control->head_block_time() + fc::milliseconds(500), 0);
      BOOST_REQUIRE_EQUAL(control->pending_block_time().sec_since_epoch(), bm.genesis_timestamp);

      init();
      auto init_block_num = control->pending_block_num();

      BOOST_REQUIRE_EQUAL(get_genesis_time().sec_since_epoch(), bm.genesis_timestamp);

      // Also fund the from account prior to completing the block.

      fund_evm_faucet();

      produce_block();
      BOOST_REQUIRE_EQUAL(control->head_block_num(), init_block_num);

      ilog("Timestamp of block containing init action: ${time} (${time_sec})",
           ("time", control->head_block_time())("time_sec", control->head_block_time().sec_since_epoch()));

      BOOST_REQUIRE_EQUAL(control->head_block_time().sec_since_epoch(), bm.genesis_timestamp);

      // Produce one more block so the next block with closest allowed timestamp has a timestamp exactly one second
      // after the timestamp of the block containing the init action. This is not necessary and the furhter tests
      // below would also work if this second produce_block was commented out.
      produce_block();
      control->abort_block();
   }

   auto start_speculative_block = [tester = this, &bm](uint32_t time_gap_sec) {
      speculative_block_starter sbs{*tester, time_gap_sec};

      uint32_t expected_evm_block_num =
         bm.timestamp_to_evm_block_num(tester->control->pending_block_time().time_since_epoch().count());
      uint64_t expected_evm_timestamp = bm.evm_block_num_to_evm_timestamp(expected_evm_block_num);

      BOOST_CHECK_EQUAL(expected_evm_block_num, 2 + time_gap_sec);
      BOOST_CHECK_EQUAL(expected_evm_timestamp, bm.genesis_timestamp + 2 + time_gap_sec);

      return sbs;
   };


   for (uint32_t vbn = 2; vbn <= 4; ++vbn) {
      // Test transaction within virtual block vbn.
      auto sb = start_speculative_block(vbn - 2);
      ilog("Speculative block time: ${time} (${time_sec})",
           ("time", control->pending_block_time())("time_sec", control->pending_block_time().sec_since_epoch()));

      auto contract_address = deploy_contract(faucet_eoa, blocknum_timestamp_contract_bytecode);
      auto contract_account = find_account_by_address(contract_address);

      BOOST_REQUIRE(contract_account.has_value());

      auto [block_num, timestamp] = get_stored_blocknum_and_timestamp(contract_account->id);

      BOOST_CHECK_EQUAL(block_num, vbn);
      BOOST_CHECK_EQUAL(timestamp, bm.genesis_timestamp + vbn);

      faucet_eoa.next_nonce = 0;
   }
}
FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()