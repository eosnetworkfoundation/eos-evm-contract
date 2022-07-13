#pragma once
#include <silkworm/chain/config.hpp>

namespace evm_runtime {
namespace test {

inline constexpr ChainConfig kTestNetwork{
    1,  // chain_id
    SealEngineType::kNoProof,
    {
        0,          // Homestead
        0,          // Tangerine Whistle
        0,          // Spurious Dragon
        0,          // Byzantium
        0,          // Constantinople
        0,          // Petersburg
        0,          // Istanbul
    },
};

} //namespace test
} //namespace evm_runtime