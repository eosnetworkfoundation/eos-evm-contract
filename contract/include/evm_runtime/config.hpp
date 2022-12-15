#pragma once
#include <silkworm/chain/config.hpp>

using evmc::operator""_bytes32;

inline silkworm::ChainConfig kJungle4{

#ifdef OVERRIDE_CHAIN_ID
#warning "overriding chain id"
    OVERRIDE_CHAIN_ID, 
#else
    15555, // chain id
#endif
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


