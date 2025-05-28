# Vaulta EVM

This is the main repository of the Vaulta EVM project. Vaulta EVM is a compatibility layer deployed on top of the Vaulta blockchain which implements the Ethereum Virtual Machine (EVM). It enables developers to deploy and run their applications on top of the Vaulta blockchain infrastructure but to build, test, and debug those applications using the common languages and tools they are used to using with other EVM compatible blockchains. It also enables users of those applications to interact with the application in ways they are familiar with (e.g. using a MetaMask wallet).

The Vaulta EVM consists of multiple components that are tracked across different repositories.

The repositories containing code relevant to the Vaulta EVM project include:
1. https://github.com/VaultaFoundation/evm-node: Vaulta EVM Node and RPC.
2. https://github.com/VaultaFoundation/blockscout: A fork of the [blockscout](https://github.com/VaultaFoundation/blockscout) blockchain explorer with adaptations to make it suitable for the Vaulta EVM project.
3. https://github.com/VaultaFoundation/evm-bridge-frontend: Frontend to operate the EVM trustless bridge.
4. This repository.

This repository in particular hosts the source to build the Vaulta EVM Contract:
1. Vaulta EVM Contract: This is the Antelope smart contract that implements the main runtime for the EVM. The source code for the smart contract can be found in the `contracts` directory. The main build artifacts are `evm_runtime.wasm` and `evm_runtime.abi`.

Beyond code, there are additional useful resources relevant to the Vaulta EVM project.
1. https://github.com/eosnetworkfoundation/evm-public-docs: A repository to hold technical documentation for an audience interested in following and participating in the operations of the Vaulta EVM project. The genesis JSON needed to stand up a Vaulta EVM Node that works with the EVM on the Vaulta blockchain can also be found in that repository.
2. https://docs.eosnetwork.com/docs/latest/eos-evm/: Official documentation for the Vaulta EVM.

## Compilation

### checkout the source code:
```
git clone https://github.com/VaultaFoundation/evm-contract.git
cd evm-contract
git submodule update --init --recursive
```


### compile EVM smart contract for Antelope blockchain:
Prerequisites:
- cmake 3.16 or later
- install cdt
```
wget https://github.com/AntelopeIO/cdt/releases/download/v4.1.0/cdt_4.1.0-1_amd64.deb
sudo apt install ./cdt_4.1.0-1_amd64.deb
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
./build/evm_runtime/evm_runtime.wasm
./build/evm_runtime/evm_runtime.abi
```

## Unit tests

We need to compile the Spring project in Antelope in order to compile unit tests:
following the instruction in https://github.com/AntelopeIO/spring to compile spring node

To compile unit tests:
```
cd evm-contract/tests
mkdir build
cd build

cmake -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_C_COMPILER=/usr/bin/clang -Deosevm_DIR:string=<EVM_CONTRACT_BUILD_FOLDER> -Dleap_DIR=<SPRING_BUILD_FOLDER>/lib/cmake/spring/ -Dcdt_DIR=<CDT_BUILD_FOLDER>/lib/cmake/cdt/ -Deosio_DIR=<SPRING_BUILD_FOLDER>/lib/cmake/eosio/ ..

cmake -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_COMPILER=gcc -Deosio_DIR=<SPRING_BUILD_FOLDER>/lib/cmake/eosio/ ..

make -j8


```

to run unit test:
```
./tests/build/unit_test --report_level=detailed --color_output
```

or run a specific test:
```
./tests/build/unit_test --report_level=detailed --color_output --run_test=gas_fee_evm_tests
```


## Deployments

For local testnet deployment and testings, please refer to 
https://github.com/VaultaFoundation/evm-contract/blob/main/docs/local_testnet_deployment_plan.md

For public testnet deployment, please refer to 
https://github.com/VaultaFoundation/evm-contract/blob/main/docs/public_testnet_deployment_plan.md


## CI
This repo contains the following GitHub Actions workflows for CI:
- Vaulta EVM Contract CI - build the Vaulta EVM Contract and its associated tests
    - [Pipeline](https://github.com/VaultaFoundation/evm-contract/actions/workflows/contract.yml)
    - [Documentation](./.github/workflows/contract.md)
- Vaulta EVM Node CI - build the Vaulta EVM node
    - [Pipeline](https://github.com/VaultaFoundation/evm-node/actions/workflows/node.yml)
    - [Documentation](./.github/workflows/node.md)

See the pipeline documentation for more information.
