#pragma once
#include <silkworm/core/chain/config.hpp>

namespace evm_runtime {
namespace test {

inline constexpr ChainConfig kTestNetwork{
    .chain_id = 1,
    .protocol_rule_set = protocol::RuleSetType::kNoProof,
    ._homestead_block = 0,
    ._dao_block = 0,
    ._tangerine_whistle_block = 0,
    ._spurious_dragon_block = 0,
    ._byzantium_block = 0,
    ._constantinople_block = 0,
    ._petersburg_block = 0,
    ._istanbul_block = 0,
    ._muir_glacier_block = 0,
    ._berlin_block = 0,
    ._london_block = 0,
    ._arrow_glacier_block = 0,
    ._gray_glacier_block = 0,
    ._terminal_total_difficulty = 0,
    ._shanghai_time = 0
};

} //namespace test
} //namespace evm_runtime
