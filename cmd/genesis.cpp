#include "genesis.hpp"

namespace trustevm {

nlohmann::json genesis() {
    return nlohmann::json::parse(R"(
    {
        "alloc": {
            "0xe7cc2b40ae8f58a7b28cc6e9b47bcadeb4985c42": {
                "balance": "0x33b2e3c9fd0803ce8000000"
            }
        },
        "coinbase": "0x0000000000000000000000000000000000000000",
        "config": {
            "chainId": 15555,
            "homesteadBlock": 0,
            "eip150Block": 0,
            "eip155Block": 0,
            "byzantiumBlock": 0,
            "constantinopleBlock": 0,
            "petersburgBlock": 0,
            "istanbulBlock": 0,
            "noproof": {}
        },
        "difficulty": "0x01",
        "extraData": "TrustEVM",
        "gasLimit": "0x7ffffffffff",
        "mixHash": "0x005e23106263ebed6c85a238ff1f553fb2990f8b780d8cdc4042b2e48d64d542",
        "nonce": "0x0",
        "timestamp": "0x62546fd7"
    }
    )");
}

}

