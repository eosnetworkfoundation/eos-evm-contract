# EOS EVM

This is the main repository of the EOS EVM project. EOS EVM is a compatibility layer deployed on top of the EOS blockchain which implements the Ethereum Virtual Machine (EVM). It enables developers to deploy and run their applications on top of the EOS blockchain infrastructure but to build, test, and debug those applications using the common languages and tools they are used to using with other EVM compatible blockchains. It also enables users of those applications to interact with the application in ways they are familiar with (e.g. using a MetaMask wallet).

The EOS EVM consists of multiple components that are tracked across different repositories.

The repositories containing code relevant to the EOS EVM project include:
1. https://github.com/eosnetworkfoundation/eos-evm-node: EOS EVM Node and RPC.
2. https://github.com/eosnetworkfoundation/blockscout: A fork of the [blockscout](https://github.com/blockscout/blockscout) blockchain explorer with adaptations to make it suitable for the EOS EVM project.
3. https://github.com/eosnetworkfoundation/evm_bridge_frontend: Frontend to operate the EVM trustless bridge.
4. This repository.

This repository in particular hosts the source to build the EOS EVM Contract:
1. EOS EVM Contract: This is the Antelope smart contract that implements the main runtime for the EVM. The source code for the smart contract can be found in the `contracts` directory. The main build artifacts are `evm_runtime.wasm` and `evm_runtime.abi`.

Beyond code, there are additional useful resources relevant to the EOS EVM project.
1. https://github.com/eosnetworkfoundation/evm-public-docs: A repository to hold technical documentation for an audience interested in following and participating in the operations of the EOS EVM project. The genesis JSON needed to stand up a EOS EVM Node that works with the EVM on the EOS blockchain can also be found in that repository.
2. https://docs.eosnetwork.com/docs/latest/eos-evm/: Official documentation for the EOS EVM.

## Compilation

### checkout the source code:
```
git clone https://github.com/eosnetworkfoundation/eos-evm.git
cd eos-evm
git submodule update --init --recursive
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
mkdir build
cd build
cmake ..
make -j
```
You should get the following output files:
```
eos-evm/build/evm_runtime/evm_runtime.wasm
eos-evm/build/evm_runtime/evm_runtime.abi
```

## Unit tests

We need to compile the Leap project in Antelope in order to compile unit tests:
following the instruction in https://github.com/AntelopeIO/leap to compile leap

To compile unit tests:
```
cd eos-evm/tests
mkdir build
cd build
cmake -Deosio_DIR=/<PATH_TO_LEAP_SOURCE>/build/lib/cmake/eosio ..
make -j4 unit_test
```

to run unit test:
```
cd tests/build
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

