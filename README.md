# TrustEVM
Main Repository of the Trust Ethereum Virtual Machine (TrustEVM-node, TrustEVM-RPC, EVM smart contract) on the Antelope Network.

## Overview
The TrustEVM node consumes Antelope(EOS) blocks from Antelope node (nodeos) via state history (SHIP) endpoint, and build up the virtual Ethereum blockchain in a deterministic way.
The TrustEVM RPC will talk with the TrustEVM node, and provide read-only Ethereum compatible RPC services for clients (such as MetaMask)

Clients can also push Ethereum compatible transactions to the Antelope blockchain, via proxy and Transaction Wrapper (TX-Wrapper), which encapsulates Ethereum raw transactions into Antelope transactions. All ethereum transactions will be validated and executed by the EVM smart contracts deployed in the Antelope Blockchain network. 

```
         |                                                 
         |                     WRITE              +-----------------+
         |             +------------------------->|   TX-Wrapper    |
         |             |                          +-------v---------+
         |             |                          | Antelope node   | ---> connect to the other nodes in the Antelope blockchain network
 client  |             |                          +-------+---------+
 request |       +-----+-----+                            |
---------+------>|   Proxy   |                            |
         |       +-----------+                            v       
         |             |                          +-----------------+
         |        READ |     +--------------+     |                 |
         |             +---->| TrustEVM RPC |---->|  TrustEVM Node  +
         |                   +--------------+     |                 |
         |                                        +-----------------+
```
         
## Compilation

### checkout the source code:
```
git clone https://github.com/eosnetworkfoundation/TrustEVM.git
cd TrustEVM
git submodule update --init --recursive
```

### compile TrustEVM-node, TrustEVM-rpc, unittests

Prerequisites:
- Ubuntu 20 or later or other compatible Linux
- gcc 10 or later

Easy Steps:
```
mkdir build
cd build
cmake ..
make -j8
```
You'll get the list of binaries with other tools:
```
cmd/trustevm-node
cmd/trustevm-rpc
cmd/unit_test
cmd/silkrpc_toolbox
cmd/silkrpcdaemon
```

Alternatively, to build with specific compiler:
```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ..
make -j8
```


### compile EVM smart contract for Antelope blockchain:
Prerequisites:
- cmake 3.16 or later
- install cdt
```
wget https://github.com/AntelopeIO/cdt/releases/download/v3.1.0/cdt_3.1.0_amd64.deb
sudo apt install ./cdt_3.1.0_amd64.deb
```
or refer to the detail instructions from https://github.com/AntelopeIO/cdt

steps of building EVM smart contracts:
```
cd contract
mkdir build
cd build
cmake ..
make -j
```
You should get the following output files:
```
TrustEVM/contract/build/evm_runtime/evm_runtime.wasm
TrustEVM/contract/build/evm_runtime/evm_runtime.abi
```

## Unittests

We need to compile the leap project in Antelope in order to compile unittests:
following the instruction in https://github.com/AntelopeIO/leap to compile leap

To compile unittests:
```
cd TrustEVM/contract/tests
mkdir build
cd build
cmake -Deosio_DIR=/<PATH_TO_LEAP_SOURCE>/build/lib/cmake/eosio ..
make -j4 unit_test
```

to run unittest:
```
cd contract/tests/build
./unit_test
```

## Deployments

For local testnet deployment and testings, please refer to 
https://github.com/eosnetworkfoundation/TrustEVM/blob/main/docs/local_testnet_deployment_plan.md

For public testnet deployment, please refer to 
https://github.com/eosnetworkfoundation/TrustEVM/blob/main/docs/public_testnet_deployment_plan.md
