# EOS EVM

This is the main repository of the EOS EVM project. EOS EVM is a compatibility layer deployed on top of the EOS blockchain which implements the Ethereum Virtual Machine (EVM). It enables developers to deploy and run their applications on top of the EOS blockchain infrastructure but to build, test, and debug those applications using the common languages and tools they are used to using with other EVM compatible blockchains. It also enables users of those applications to interact with the application in ways they are familiar with (e.g. using a MetaMask wallet).

The EOS EVM consists of multiple components that are tracked across different repositories.

The repositories containing code relevant to the EOS EVM project include:
1. https://github.com/eosnetworkfoundation/blockscout: A fork of the [blockscout](https://github.com/blockscout/blockscout) blockchain explorer with adaptations to make it suitable for the EOS EVM project.
2. https://github.com/eosnetworkfoundation/evm_bridge_frontend: Frontend to operate the EVM trustless bridge.
3. This repository.

This repository in particular hosts the source to build three significant components of EOS EVM:
1. EOS EVM Contract: This is the Antelope smart contract that implements the main runtime for the EVM. The source code for the smart contract can be found in the `contracts` directory. The main build artifacts are `evm_runtime.wasm` and `evm_runtime.abi`.
2. EOS EVM Node and RPC: These are programs, EOS EVM Node (`eos-evm-node` executable) and EOS EVM RPC (`eos-evm-rpc` executable), that are based on Silkworm and which together allow a node to service a subset of the RPC methods supported in the Ethereum JSON-RPC which involve reading the state of the (virtual) EVM blockchain enabled by the EOS EVM Contract. The `eos-evm-node` program relies on a SHiP connection to a [Leap](https://github.com/AntelopeIO/leap) node that is connected to the blockchain network hosting the desired EOS EVM Contract (i.e. the EOS network in the case of the EOS EVM Mainnet).
3. TX-Wrapper: This is a Node.js application which specifically services two RPC methods of the Ethereum JSON-RPC: `eth_sendRawTransaction` and `eth_gasPrice`. It relies on chain API access to a [Leap](https://github.com/AntelopeIO/leap) node connected to the blockchain network hosting the desired EOS EVM Contract. The source code for TX-Wrapper can be found in the `peripherals/tx_wrapper` directory.

## Overview

The EOS EVM Node consumes Antelope (EOS) blocks from a Leap node via state history (SHiP) endpoint and builds the virtual EVM blockchain in a deterministic way.
The EOS EVM RPC will talk with the EOS EVM node, and provide read-only Ethereum compatible RPC services for clients (such as MetaMask).

Clients can also push Ethereum compatible transactions (aka EVM transactions) to the EOS blockchain, via proxy and Transaction Wrapper (TX-Wrapper), which encapsulates EVM transactions into Antelope transactions. All EVM transactions will be validated and executed by the EOS EVM Contract deployed on the EOS blockchain.

```
         |                                                 
         |                     WRITE              +-----------------+
         |             +------------------------->|    TX-Wrapper   |
         |             |                          +-------v---------+
         |             |                          |    Leap node    | ---> connect to the other nodes in the blockchain network
 client  |             |                          +-------+---------+
 request |       +-----+-----+                            |
---------+------>|   Proxy   |                            |
         |       +-----------+                            v       
         |             |                          +-----------------+
         |        READ |     +--------------+     |                 |
         |             +---->|  EOS EVM RPC |---->|   EOS EVM Node  +
         |                   +--------------+     |                 |
         |                                        +-----------------+
```
         
## Compilation

### checkout the source code:
```
git clone https://github.com/eosnetworkfoundation/eos-evm.git
cd eos-evm
git submodule update --init --recursive
```

### compile eos-evm-node, eos-evm-rpc, unittests

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
cmd/eos-evm-node
cmd/eos-evm-rpc
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
eos-evm/contract/build/evm_runtime/evm_runtime.wasm
eos-evm/contract/build/evm_runtime/evm_runtime.abi
```

## Unit tests

We need to compile the Leap project in Antelope in order to compile unit tests:
following the instruction in https://github.com/AntelopeIO/leap to compile leap

To compile unit tests:
```
cd eos-evm/contract/tests
mkdir build
cd build
cmake -Deosio_DIR=/<PATH_TO_LEAP_SOURCE>/build/lib/cmake/eosio ..
make -j4 unit_test
```

to run unit test:
```
cd contract/tests/build
./unit_test
```

## Deployments

For local testnet deployment and testings, please refer to 
https://github.com/eosnetworkfoundation/eos-evm/blob/main/docs/local_testnet_deployment_plan.md

For public testnet deployment, please refer to 
https://github.com/eosnetworkfoundation/eos-evm/blob/main/docs/public_testnet_deployment_plan.md

## CI
This repo contains the following GitHub Actions workflows for CI:
- EOS EVM Contract CI - build the EOS EVM Contract and its associated tests
    - [Pipeline](https://github.com/eosnetworkfoundation/eos-evm/actions/workflows/contract.yml)
    - [Documentation](./.github/workflows/contract.md)
- EOS EVM Node CI - build the EOS EVM node
    - [Pipeline](https://github.com/eosnetworkfoundation/eos-evm/actions/workflows/node.yml)
    - [Documentation](./.github/workflows/node.md)

See the pipeline documentation for more information.
