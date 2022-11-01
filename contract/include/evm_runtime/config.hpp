#pragma once
#include <silkworm/chain/config.hpp>

using evmc::operator""_bytes32;

inline silkworm::ChainConfig kJungle4{
    15555,  // chain_id
    00_bytes32,
    silkworm::SealEngineType::kNoProof,
    {
        0,          // Homestead
        0,          // Tangerine Whistle
        0,          // Spurious Dragon
        0,          // Byzantium
        0,          // Constantinople
        0,          // Petersburg
        0,          // Istanbul
        // 0,          // Berlin
        // 0,          // London
    },
};


