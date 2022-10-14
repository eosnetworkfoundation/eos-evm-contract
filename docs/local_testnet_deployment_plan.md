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

make a data-dir directory:
```
mkdir data-dir
```
prepare the genesis file in ./data-dir/genesis.json, for example
```
{
  "initial_key": "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV",
  "initial_timestamp": "2022-01-01T00:00:00",
  "initial_parameters": {
  },
  "initial_configuration": {
  }
}
```
In this case the initial genesis public key - private key pair is "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV"/"5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"
You can you any other key pair as the genesis key.

prepare the config file in ./data-dir/config.ini, for example
```
chain-state-db-size-mb = 16384

# Track only transactions whose scopes involve the listed accounts. Default is to track all transactions.
# filter_on_accounts = 

# override the initial timestamp in the Genesis State file
# genesis-timestamp = 


# Pairs of [BLOCK_NUM,BLOCK_ID] that should be enforced as checkpoints.
# checkpoint = 


# The local IP and port to listen for incoming http connections.
http-server-address = 127.0.0.1:8888

# Specify the Access-Control-Allow-Origin to be returned on each request.
# access-control-allow-origin = 

# Specify the Access-Control-Allow-Headers to be returned on each request.
# access-control-allow-headers = 

# Specify if Access-Control-Allow-Credentials: true should be returned on each request.
access-control-allow-credentials = false

# The actual host:port used to listen for incoming p2p connections.
p2p-listen-endpoint = 0.0.0.0:9876

# An externally accessible host:port for identifying this node. Defaults to p2p-listen-endpoint.
# p2p-server-address = 

p2p-max-nodes-per-host = 10

# The public endpoint of a peer node to connect to. Use multiple p2p-peer-address options as needed to compose a network.
# p2p-peer-address = 

# The name supplied to identify this node amongst the peers.
agent-name = "EOS Test Agent"

# Can be 'any' or 'producers' or 'specified' or 'none'. If 'specified', peer-key must be specified at least once. If only 'producers', peer-key is not required. 'producers' and 'specified' may be combined.
allowed-connection = any

# Optional public key of peer allowed to connect.  May be used multiple times.
#peer-key = "EOS5RCMdVJ8JuzxKSbWArbYUGTVcMBc4FVpsqT9qHGYkvnHUrKnrg"

# Tuple of [PublicKey, WIF private key] (may specify multiple times)
peer-private-key = ["EOS7ZRw8XrYEk5AJgJnL4C8d6pYJg8GH76gobfLYtixFwWh763GSC", "5JVoee8UEMgoYAbNoJqWUTonpSeThLueatQBCqC2JXU3fCUMebj"]

# Maximum number of clients from which connections are accepted, use 0 for no limit
max-clients = 25

# number of seconds to wait before cleaning up dead connections
connection-cleanup-period = 30

# True to require exact match of peer network version.
# network-version-match = 0

# number of blocks to retrieve in a chunk from any individual peer during synchronization
sync-fetch-span = 100

# Enable block production, even if the chain is stale.
enable-stale-production = true

# ID of producer controlled by this node (e.g. inita; may specify multiple times)
producer-name = eosio

# Tuple of [public key, WIF private key] for block production (may specify multiple times)
private-key = ["EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV","5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"]
private-key = ["EOS8kE63z4NcZatvVWY4jxYdtLg6UEA123raMGwS6QDKwpQ69eGcP","5JURSKS1BrJ1TagNBw1uVSzTQL2m9eHGkjknWeZkjSt33Awtior"]
private-key = ["EOS7mGPTufZzCw1GxobnS9qkbdBkV7ajd1Apz9SmxXmPnKyxw2g6u","5KXMcXZjwC69uTHyYKF5vtFP8NLyq8ZaNrbjVc5HoMKBsH3My4D"]
private-key = ["EOS7RaMheR2Fw4VYBj9dj6Vv7jMyV54NrdmgTe7GRkosEPxCTtTPR","5Jo1cABt1KLCTmYmePetiU8A5uDKVZGM44PgvQ4SiJVo2gA9Son"]
private-key = ["EOS5sUpxhaC5V231cAVxGVH9RXtN9n4KDxZG6ZUwHRgYoEpTBUidU","5JK68f7PifEtGhN2T4xK9mMxCrtYLmPp6cNdKSSYJmTJqCFhNVX"]

# state-history
trace-history = true
chain-state-history = true

state-history-endpoint = 127.0.0.1:8999

# Plugin(s) to enable, may be specified multiple times
plugin = eosio::producer_plugin
plugin = eosio::chain_api_plugin
plugin = eosio::http_plugin
plugin = eosio::txn_test_gen_plugin
plugin = eosio::producer_api_plugin
plugin = eosio::state_history_plugin
plugin = eosio::net_plugin
plugin = eosio::net_api_plugin

```
startup Antelope node:
```
./build/programs/nodeos/nodeos --data-dir=./data-dir  --config-dir=./data-dir --genesis-json=./data-dir/genesis.json --disable-replay-opts --contracts-console
```
you will see the node is started and blocks are produced, for example:

```
info  2022-10-14T04:03:19.911 nodeos    producer_plugin.cpp:2437      produce_block        ] Produced block 12ef38e0bcf48b35... #2 @ 2022-10-14T04:03:20.000 signed by eosio [trxs: 0, lib: 1, confirmed: 0]
info  2022-10-14T04:03:20.401 nodeos    producer_plugin.cpp:2437      produce_block        ] Produced block df3ab0d68f1d0aaf... #3 @ 2022-10-14T04:03:20.500 signed by eosio [trxs: 0, lib: 2, confirmed: 0]
```

If you want to start by discarding all previous blockchain data, add --delete-all-blocks:
```
./build/programs/nodeos/nodeos --data-dir=./data-dir  --config-dir=./data-dir --genesis-json=./data-dir/genesis.json --disable-replay-opts --contracts-console --delete-all-blocks
```

If you want to start with the previous blockchain data, but encounter the "dirty flag" error, try to restart with --hard-replay (in this case the state will be disgarded, the node will validate and apply every block from the beginning.
```
./build/programs/nodeos/nodeos --data-dir=./data-dir  --config-dir=./data-dir --genesis-json=./data-dir/genesis.json --disable-replay-opts --contracts-console --hard-replay
```


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


