#include "basic_evm_tester.hpp"

#include <evm_common/block_mapping.hpp>

#include <fc/io/raw_fwd.hpp>

using namespace eosio::testing;

using eosio::chain::time_point;
using eosio::chain::time_point_sec;
using eosio::chain::unsigned_int;

struct mapping_evm_tester : basic_evm_tester {
   static constexpr eosio::chain::name evm_account_name = "evm"_n;

   bool produce_blocks_until_timestamp_satisfied(std::function<bool(time_point)> cond, unsigned int limit = 10) {
      for (unsigned int blocks_produced = 0; blocks_produced < limit && !cond(control->pending_block_time()); ++blocks_produced) {
         produce_block();
      }
      return cond(control->pending_block_time());
   }

   struct config_table_row {
      unsigned_int   version;
      uint64_t       chainid;
      time_point_sec genesis_time;
      //additional fields ignored in this test
   };

   time_point_sec get_genesis_time() {
      static constexpr eosio::chain::name config_singleton_name = "config"_n;
      const vector<char> d =
         get_row_by_account(evm_account_name, evm_account_name, config_singleton_name, config_singleton_name);
      return fc::raw::unpack<config_table_row>(d).genesis_time;
   }
};

FC_REFLECT(mapping_evm_tester::config_table_row, (version)(chainid)(genesis_time))

BOOST_AUTO_TEST_SUITE(mapping_evm_tests)

BOOST_AUTO_TEST_CASE(block_mapping_tests) try {
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
      // An EVM transaction in an Antelope block with timestamp 10 ends up in EVM block 1 which has timestamp 11. This is intentional.
      // It means that an EVM transaction included in the same block immediately after the init action will end up in a block
      // that has a timestamp greater than the genesis timestamp.
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

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(init_on_second_boundary, mapping_evm_tester) try {
   auto timestamp_at_second_boundary = [](time_point timestamp) -> bool {
      return timestamp.time_since_epoch().count() % 1'000'000 == 0;
   };
   BOOST_REQUIRE(produce_blocks_until_timestamp_satisfied(timestamp_at_second_boundary));

   init(15555);
   time_point_sec expected_genesis_time = control->pending_block_time(); // Rounds down to nearest second.
 
   time_point_sec actual_genesis_time = get_genesis_time();
   ilog("Genesis time: ${time}", ("time", actual_genesis_time));
 
   BOOST_REQUIRE_EQUAL(actual_genesis_time.sec_since_epoch(), expected_genesis_time.sec_since_epoch());
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(init_not_on_second_boundary, mapping_evm_tester) try {
   auto timestamp_not_at_second_boundary = [](time_point timestamp) -> bool {
      return timestamp.time_since_epoch().count() % 1'000'000 != 0;
   };
   BOOST_REQUIRE(produce_blocks_until_timestamp_satisfied(timestamp_not_at_second_boundary));

   init(15555);
   time_point_sec expected_genesis_time = control->pending_block_time(); // Rounds down to nearest second.

   time_point_sec actual_genesis_time = get_genesis_time();
   ilog("Genesis time: ${time}", ("time", actual_genesis_time));

   BOOST_REQUIRE_EQUAL(actual_genesis_time.sec_since_epoch(), expected_genesis_time.sec_since_epoch());
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()