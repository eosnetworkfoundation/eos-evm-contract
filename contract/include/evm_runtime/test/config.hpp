#pragma once
#include <silkworm/chain/config.hpp>

namespace evm_runtime {
namespace test {

using evmc::operator""_bytes32;

inline ChainConfig kTestNetwork{
    1,  // chain_id
    0_bytes32,
    silkworm::SealEngineType::kNoProof,
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