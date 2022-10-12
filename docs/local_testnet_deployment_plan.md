# Setting up a Local EOSIO testnet with EVM support


## Hardware requirments

- CPU
  A high end CPU with good single threaded performance is recommended, such as i9 or Ryzen 9 or Server grade CPU.
- RAM
  32GB+ is recommended. Using 16GB is OK but it can't support much data and compilation will be significantly slow
- SSD
  A big SSD is required for storing all history (blocks + State History). Recommend 1TB+
- Network
  A low latency network is recommened if you plan to run multiple nodes. A simple network (or even wifi) would works for single node.
  
  
## software requirements
- Operating System: Ubuntu 20.04 or 22.04

Have the following binaries built from https://github.com/AntelopeIO/leap
- Nodeos: the main process of an EOSIO node
- Cleos: the command line interface for queries and transaction 
- keosd: the key and wallet manager.

Have the following binaries built from https://github.com/AntelopeIO/cdt
- cdt-cpp: the EOSIO smart contract compiler
- eosio-wast2wasm & eosio-wasm2wast: conversion tools for building EVM contract

List of compiled system contracts from https://github.com/eosnetworkfoundation/eos-system-contracts (compiled by cdt):
- eosio.boot.wasm
- eosio.bios.wasm
- eosio.msig.wasm (optional, if you want to test multisig)
- eosio.token.wasm (optional, if you want to test token econonmy)
- eosio.system.wasm (optional, if you want to test resources, RAM, ... etc)

Compiled EVM contracts from this repo (see https://github.com/eosnetworkfoundation/TrustEVM/blob/main/docs/compilation_and_testing_guide.md)
- evm_runtime.wasm



  
## Running a local node with Trust EVM service

  In order to run a Trust EVM service, we need to have the follow items inside one physical server / VM:
  - run a local EOSIO node (nodeos process) with SHIP plugin enabled
  - run a TrustEVM-node process connecting to the local EOSIO node
  - run one or more TrustEVM-RPC process locally to service the public
  - setup a trustEVM proxy to route read requests to TrustEVM-RPC and write requests to EOSIO public network
  - prepare a eosio account with necessary CPU/NET/RAM resources for signing EVM transactions (this account can be shared to multiple EVM service)
  

### configuration and command parameters:
- nodeos:
- TrustEVM-node:
- TrustEVM-RPC




## Bootstrapping protocol features, Deploying EVM contracts, Setup genesis/initial EVM virtual accounts & tokens

### Protocol features required to activate:
- ACTION_RETURN_VALUE
- CRYPTO_PRIMITIVES
- GET_BLOCK_NUM

### Create main EVM account & Deploy EVM contract to EOSIO blockchain


### Setup EVM token


### disturbute EVM tokens according to genesis


