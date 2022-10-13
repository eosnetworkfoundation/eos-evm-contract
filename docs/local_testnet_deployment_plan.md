# Setting up a Local Antelope testnet with EVM support

Setting up a local testing environment of Antelope with EVM support allows developers to speed up their smart contract developments without worrying about any resource, network, version or other stabliliy issues that public testnet may introduce. Developers are free to modify, debug, or reset the environment to facilite their dApps developments.

## Hardware requirments

- CPU
  A high end CPU with good single threaded performance is recommended, such as i9 or Ryzen 9 or Server grade CPU. Middle or Low end CPU would also be OK but will end up having less transactions per second.
- RAM
  32GB+ is recommended. Using 16GB is OK but it can't support much data and compilation will be significantly slow
- SSD
  A big SSD is required for storing all history (blocks + State History). Recommend 1TB+. Using very small amount of storage like 100GB would still work fine but it will only support much fewer transactions / data.
- Network
  A low latency network is recommened if you plan to run multiple nodes. A simple network (or even WiFi) would works for single node.
  
  
## software requirements
- Operating System: Ubuntu 20.04 or 22.04

Have the following binaries built from https://github.com/AntelopeIO/leap
- Nodeos: the main process of an Antelope node
- Cleos: the command line interface for queries and transaction 
- keosd: the key and wallet manager.

Have the following binaries built from https://github.com/AntelopeIO/cdt
- cdt-cpp: the Antelope smart contract compiler
- eosio-wast2wasm & eosio-wasm2wast: conversion tools for building EVM contract

List of compiled system contracts from https://github.com/eosnetworkfoundation/eos-system-contracts (compiled by cdt):
- eosio.boot.wasm
- eosio.bios.wasm
- eosio.msig.wasm (optional, if you want to test multisig)
- eosio.token.wasm (optional, if you want to test token econonmy)
- eosio.system.wasm (optional, if you want to test resources, RAM, ... etc)

Compiled EVM contracts from this repo (see https://github.com/eosnetworkfoundation/TrustEVM/blob/main/docs/compilation_and_testing_guide.md)
- evm_runtime.wasm

Compiled binaries from this repo
- trustevm-node: (silkworm node process that receive data from the main Antelope chain and convert to the EVM chain)
- trustevm-rpc: (silkworm rpc server that provide service for view actions and other read operations)


  
## Running a local node with Trust EVM service, Overview:

  In order to run a Trust EVM service, we need to have the follow items inside one physical server / VM:
  1. run a local Antelope node (nodeos process) with SHIP plugin enabled, which is a single block producer
  2. blockchain bootstrapping and initialization
  3. deploy evm contract and initilize evm
  4. run a TrustEVM-node(silkworm node) process connecting to the local Antelope node
  5. run a TrustEVM-RPC(silkworm RPC) process locally to serve the eth RPC requests
  6. setup the transaction wrapper for write transactions
  7. setup a trustEVM proxy to route read requests to TrustEVM-RPC and write requests to Antelope public network
  - prepare a eosio account with necessary CPU/NET/RAM resources for signing EVM transactions (this account can be shared to multiple EVM service)
  

## Step by Step in details:

## 1. Run a local Antelope node 

## 2. Blockchain bootstrapping and initialization

## 3. Deploy and initialize EVM contract

## 4. Start up TrustEVM-node (silkworm node) 

## 5. Start up TrustEVM-RPC (silkworm RPC)

## 6. Setup transaction wrapper

## 7. Setup proxy 



### configuration and command parameters:
- nodeos:
- TrustEVM-node:
- TrustEVM-RPC




## Bootstrapping protocol features, Deploying EVM contracts, Setup genesis/initial EVM virtual accounts & tokens

### Protocol features required to activate:
- ACTION_RETURN_VALUE
- CRYPTO_PRIMITIVES
- GET_BLOCK_NUM

### Create main EVM account & Deploy EVM contract to Antelope blockchain


### Setup EVM token


### disturbute EVM tokens according to genesis


