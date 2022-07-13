#pragma once

#include <eosio/eosio.hpp>
#include <evm_runtime/types.hpp>
#include <silkworm/types/block.hpp>

namespace evm_runtime {
namespace test {

using namespace eosio;
using namespace silkworm;

struct block_info {
    bytes                   coinbase;
    uint64_t                difficulty;
    uint64_t                gasLimit;
    uint64_t                number;
    uint64_t                timestamp;
    //std::optional<bytes>    base_fee_per_gas;

    BlockHeader get_block_header()const {
        address beneficiary;
        check(coinbase.size() == 20, "invalid coinbase");
        memcpy(beneficiary.bytes, coinbase.data(), coinbase.size());

        BlockHeader res;
        res.beneficiary      = beneficiary;
        res.difficulty       = difficulty;
        res.gas_limit        = gasLimit;
        res.number           = number;
        res.timestamp        = timestamp;

        //if(base_fee_per_gas.has_value()) res.base_fee_per_gas = to_uint256(*base_fee_per_gas);

        return res;
    }

    //EOSLIB_SERIALIZE(block_info,(coinbase)(difficulty)(gasLimit)(number)(timestamp)(base_fee_per_gas))
    EOSLIB_SERIALIZE(block_info,(coinbase)(difficulty)(gasLimit)(number)(timestamp))
};

} //namespace test
} //namespace evm_runtime