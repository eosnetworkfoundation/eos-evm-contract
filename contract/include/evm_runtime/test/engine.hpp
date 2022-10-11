#pragma once
#include <silkworm/consensus/engine.hpp>
#include <silkworm/chain/protocol_param.hpp>

namespace evm_runtime {
namespace test {

struct engine : silkworm::consensus::IEngine {
    
    ValidationResult pre_validate_block_body(const Block& block, const BlockState& state) override {
        return ValidationResult::kOk;
    }

    ValidationResult validate_ommers(const Block& block, const BlockState& state) override {
        return ValidationResult::kOk;
    }

    ValidationResult validate_block_header(const BlockHeader& header, const BlockState& state,
                                                   bool with_future_timestamp_check) override {
        return ValidationResult::kOk;
    }

    ValidationResult validate_seal(const BlockHeader& header) override {
        return ValidationResult::kOk;
    }

    void finalize(IntraBlockState& state, const Block& block, evmc_revision revision) override {
        intx::uint256 block_reward;
        if (revision >= EVMC_CONSTANTINOPLE) {
            block_reward = param::kBlockRewardConstantinople;
        } else if (revision >= EVMC_BYZANTIUM) {
            block_reward = param::kBlockRewardByzantium;
        } else {
            block_reward = param::kBlockRewardFrontier;
        }

        const uint64_t block_number{block.header.number};
        intx::uint256 miner_reward{block_reward};
        for (const BlockHeader& ommer : block.ommers) {
            intx::uint256 ommer_reward{((8 + ommer.number - block_number) * block_reward) >> 3};
            state.add_to_balance(ommer.beneficiary, ommer_reward);
            miner_reward += block_reward / 32;

        }
        eosio::print("add balance to beneficiary\n");
        state.add_to_balance(block.header.beneficiary, miner_reward);
    }

    evmc::address get_beneficiary(const BlockHeader& header) override {
        return header.beneficiary;
    }
};

} //namespace test
} //namespace evm_runtime