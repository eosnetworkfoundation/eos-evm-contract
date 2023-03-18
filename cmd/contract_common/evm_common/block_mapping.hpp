#include <cstdint>

#include <silkworm/types/block.hpp>
#include <silkworm/common/util.hpp>
namespace evm_common {

struct block_mapping
{
   /**
    * @brief Construct object that maps from Antelope timestamp to EVM block number and timestamp
    *
    * @param genesis_timestamp_sec - the EVM genesis timestamp in seconds
    * @param block_interval_sec - time interval between consecutive EVM blocks in seconds (must be positive)
    *
    * @note The timestamp of the Antelope block containing the init action rounded down to the nearest second must equal
    * genesis_timestamp_sec.
    */
   block_mapping(uint64_t genesis_timestamp_sec, uint32_t block_interval_sec = 1) :
      block_interval(block_interval_sec == 0 ? 1 : block_interval_sec), genesis_timestamp(genesis_timestamp_sec)
   {}

   const uint64_t block_interval;    // seconds
   const uint64_t genesis_timestamp; // seconds

   /**
    * @brief Map Antelope timestamp to EVM block num
    *
    * @param timestamp - Antelope timestamp in microseconds
    * @return mapped EVM block number (returns 0 for all timestamps prior to the genesis timestamp)
    */
   inline uint32_t timestamp_to_evm_block_num(uint64_t timestamp_us) const
   {
      uint64_t timestamp = timestamp_us / 1e6; // map Antelope block timestamp to EVM block timestamp
      if (timestamp < genesis_timestamp) {
         // There should not be any transactions prior to the init action.
         // But any entity with an associated timestamp prior to the genesis timestamp can be considered as part of the
         // genesis block.
         return 0;
      }
      return 1 + (timestamp - genesis_timestamp) / block_interval;
   }

   /**
    * @brief Map EVM block num to EVM block timestamp
    *
    * @param block_num - EVM block number
    * @return EVM block timestamp associated with the given EVM block number
    */
   inline uint64_t evm_block_num_to_evm_timestamp(uint32_t block_num) const
   {
      return genesis_timestamp + block_num * block_interval;
   }
};

/**
 * @brief Prepare block header
 *
 * Modifies header by setting the common fields shared between the contract code and the EVM node.
 * It sets the beneficiary, difficulty, number, gas_limit, and timestamp fields only.
 */
inline void prepare_block_header(silkworm::BlockHeader& header,
                                 const block_mapping& bm,
                                 uint64_t evm_contract_name,
                                 uint32_t evm_block_num)
{
   header.beneficiary = silkworm::make_reserved_address(evm_contract_name);
   header.difficulty = 1;
   header.number = evm_block_num;
   header.gas_limit = 0x7ffffffffff;
   header.timestamp = bm.evm_block_num_to_evm_timestamp(header.number);
}


} // namespace evm_common