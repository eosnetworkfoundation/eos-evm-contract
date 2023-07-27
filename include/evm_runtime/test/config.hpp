#pragma once
#include <silkworm/core/chain/config.hpp>

namespace evm_runtime {
namespace test {

inline constexpr ChainConfig kTestNetwork{
    .chain_id = 1,
    .protocol_rule_set = protocol::RuleSetType::kNoProof,
    .homestead_block = 0,
    .dao_block = 0,
    .tangerine_whistle_block = 0,
    .spurious_dragon_block = 0,
    .byzantium_block = 0,
    .constantinople_block = 0,
    .petersburg_block = 0,
    .istanbul_block = 0,
};

} //namespace test
} //namespace evm_runtime
