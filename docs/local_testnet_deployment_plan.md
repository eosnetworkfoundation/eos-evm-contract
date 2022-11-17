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

Compiled EVM contracts in DEBUG mode, from this repo (see https://github.com/eosnetworkfoundation/TrustEVM/blob/main/docs/compilation_and_testing_guide.md)
- evm_runtime.wasm
- evm_runtime.abi

<b> Ensure action "setbal" exists in evm_runtime.abi </b>

Compiled binaries from this repo
- trustevm-node: (silkworm node process that receive data from the main Antelope chain and convert to the EVM chain)
- trustevm-rpc: (silkworm rpc server that provide service for view actions and other read operations)


  
## Running a local node with Trust EVM service, Overview:

  In order to run a Trust EVM service, we need to have the follow items inside one physical server / VM:
  
  We need the following steps to setup the Antelope blockchain with capabilities to push EVM transactions:
  
  1. run a local Antelope node (nodeos process) with SHIP plugin enabled, which is a single block producer
  2. blockchain bootstrapping and initialization
  3. deploy evm contract and initilize evm
  4. setup the transaction wrapper for to wrap ETH write requests into Antelope transactions
  5. run a TrustEVM-node(silkworm node) process connecting to the local Antelope node
  6. run a TrustEVM-RPC(silkworm RPC) process locally to serve the eth RPC requests
  7. setup a trustEVM proxy to route read requests to TrustEVM-RPC and write requests to transaction wrapper

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

You can find the command line interface "cleos" in ./build/programs/cleos/cleos

try the following command:
```
./cleos get info
```
If you can get the similar response as follow:
```
{
  "server_version": "f36e59e5",
  "chain_id": "4a920ae9b3b9c99e79542834f2332201d9393adfca26cdcca10aa3fd4a3dc68d",
  "head_block_num": 362,
  "last_irreversible_block_num": 361,
  "last_irreversible_block_id": "0000016993d6b2d1ea8f0602cea8d94690f533f90555cddc7e66f36e08d3fd53",
  "head_block_id": "0000016a079b188ba800b37afbbfb5b0dae543008e7d2daf563f9a59e9127ca1",
  "head_block_time": "2022-10-14T06:06:53.500",
  "head_block_producer": "eosio",
  "virtual_block_cpu_limit": 286788,
  "virtual_block_net_limit": 1504517,
  "block_cpu_limit": 199900,
  "block_net_limit": 1048576,
  "server_version_string": "v3.1.0",
  "fork_db_head_block_num": 362,
  "fork_db_head_block_id": "0000016a079b188ba800b37afbbfb5b0dae543008e7d2daf563f9a59e9127ca1",
  "server_full_version_string": "v3.1.0-f36e59e554e8e5687e705e32e9f8aea4a39ed213",
  "total_cpu_weight": 0,
  "total_net_weight": 0,
  "earliest_available_block_num": 1,
  "last_irreversible_block_time": "2022-10-14T06:06:53.000"
}
```
It means your local node is running fine and cleos has successfully communicated with nodeos.

Generate some public/private key pairs for testing, command:
```
./cleos create key --to-console
```
You will get similar output as follow:
```
Private key: 5Ki7JeCMXQmxreZnL2JubQEYuqByehbPhKGUXyxqo6RYGNS2F3i
Public key: EOS8j4zHDrqRrf84QJLDTEgUbhB24VUkqVCjAxMzmeJuJSvZ8S1FU
```
Repeat the same command to generate multiple key pairs. Save your key pairs for later testing use.

create your new wallet named w123 (any other name is fine)
```
./build/programs/cleos/cleos wallet create -n w123 --file w123.key
```
your wallet password is saved into w123.key

import one or more private keys into wallet w123
```
./cleos wallet import -n w123 --private-key 5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3
./cleos wallet import -n w123 --private-key 5JURSKS1BrJ1TagNBw1uVSzTQL2m9eHGkjknWeZkjSt33Awtior
```

Once you have done everything with the wallet, it is fine to bootstrapping the blockchain

### activate protocol features:

First we need to use curl to schedule protocol feature activation:
```
curl --data-binary '{"protocol_features_to_activate":["0ec7e080177b2c02b278d5088611686b49d739925a92d9bfcacd7fc6b74053bd"]}' http://127.0.0.1:8888/v1/producer/schedule_protocol_feature_activations
```

You'll get the "OK" response if succeed:
```
{"result":"ok"}
```

### deploy boot contract, command:
```
./cleos set code eosio ../eos-system-contracts/build/contracts/eosio.boot/eosio.boot.wasm
```
output:
```
Reading WASM from /home/kayan-u20/workspaces/leap/../eos-system-contracts/build/contracts/eosio.boot/eosio.boot.wasm...
Setting Code...
executed transaction: acaf5ed70a7ce271627532cf76b6303ebab8d24656f57c69b03cfe8103f6f457  2120 bytes  531 us
#         eosio <= eosio::setcode               {"account":"eosio","vmtype":0,"vmversion":0,"code":"0061736d0100000001480e60000060027f7f0060017e0060...
warning: transaction executed locally, but may not be confirmed by the network yetult         ] 
```

set boot.abi, command:
```
./cleos set abi eosio ../eos-system-contracts/build/contracts/eosio.boot/eosio.boot.abi
```
output
```
Setting ABI...
executed transaction: b972e178d182c1523e9abbd1fae27efae90d7711e152261a21169372a19d9d3a  1528 bytes  171 us
#         eosio <= eosio::setabi                {"account":"eosio","abi":"0e656f73696f3a3a6162692f312e32001008616374697661746500010e666561747572655f...
warning: transaction executed locally, but may not be confirmed by the network yetult         ] 
```

activate the other protocol features:
```
./cleos push action eosio activate '["f0af56d2c5a48d60a4a5b5c903edfb7db3a736a94ed589d0b797df33ff9d3e1d"]' -p eosio 
./cleos push action eosio activate '["e0fb64b1085cc5538970158d05a009c24e276fb94e1a0bf6a528b48fbc4ff526"]' -p eosio
./cleos push action eosio activate '["d528b9f6e9693f45ed277af93474fd473ce7d831dae2180cca35d907bd10cb40"]' -p eosio
./cleos push action eosio activate '["c3a6138c5061cf291310887c0b5c71fcaffeab90d5deb50d3b9e687cead45071"]' -p eosio 
./cleos push action eosio activate '["bcd2a26394b36614fd4894241d3c451ab0f6fd110958c3423073621a70826e99"]' -p eosio
./cleos push action eosio activate '["ad9e3d8f650687709fd68f4b90b41f7d825a365b02c23a636cef88ac2ac00c43"]' -p eosio
./cleos push action eosio activate '["8ba52fe7a3956c5cd3a656a3174b931d3bb2abb45578befc59f283ecd816a405"]' -p eosio 
./cleos push action eosio activate '["6bcb40a24e49c26d0a60513b6aeb8551d264e4717f306b81a37a5afb3b47cedc"]' -p eosio
./cleos push action eosio activate '["68dcaa34c0517d19666e6b33add67351d8c5f69e999ca1e37931bc410a297428"]' -p eosio
./cleos push action eosio activate '["5443fcf88330c586bc0e5f3dee10e7f63c76c00249c87fe4fbf7f38c082006b4"]' -p eosio
./cleos push action eosio activate '["4fca8bd82bbd181e714e283f83e1b45d95ca5af40fb89ad3977b653c448f78c2"]' -p eosio
./cleos push action eosio activate '["ef43112c6543b88db2283a2e077278c315ae2c84719a8b25f25cc88565fbea99"]' -p eosio
./cleos push action eosio activate '["4a90c00d55454dc5b059055ca213579c6ea856967712a56017487886a4d4cc0f"]' -p eosio 
./cleos push action eosio activate '["35c2186cc36f7bb4aeaf4487b36e57039ccf45a9136aa856a5d569ecca55ef2b"]' -p eosio
./cleos push action eosio activate '["299dcb6af692324b899b39f16d5a530a33062804e41f09dc97e9f156b4476707"]' -p eosio
./cleos push action eosio activate '["2652f5f96006294109b3dd0bbde63693f55324af452b799ee137a81a905eed25"]' -p eosio
./cleos push action eosio activate '["1a99a59d87e06e09ec5b028a9cbb7749b4a5ad8819004365d02dc4379a8b7241"]' -p eosio
```



## 3. Deploy and initialize EVM contract

Create account evmevmevmevm (using key pair EOS8kE63z4NcZatvVWY4jxYdtLg6UEA123raMGwS6QDKwpQ69eGcP 5JURSKS1BrJ1TagNBw1uVSzTQL2m9eHGkjknWeZkjSt33Awtior)
```
./cleos create account eosio evmevmevmevm EOS8kE63z4NcZatvVWY4jxYdtLg6UEA123raMGwS6QDKwpQ69eGcP EOS8kE63z4NcZatvVWY4jxYdtLg6UEA123raMGwS6QDKwpQ69eGcP
```

deploy evm_runtime contract (wasm & abi file) to account evmevmevmevm
```
./cleos set code evmevmevmevm ../TrustEVM/contract/build/evm_runtime/evm_runtime.wasm
./cleos set abi evmevmevmevm ../TrustEVM/contract/build/evm_runtime/evm_runtime.abi
```

setting initial balance for genesis eth accounts.
In this document we use ```0x2787b98fc4e731d0456b3941f0b3fe2e01439961```, (private key ```a3f1b69da92a0233ce29485d3049a4ace39e8d384bbc2557e3fc60940ce4e954```) as genesis. Developers can use one or more other genesis eth accounts.

<b>Notice that the balance string must be in hex and must be exactly 64 bytes long (representing a full 256-bit integer value). Failure to meet such criteria will result in incorrect balance calculation in transfers. </b>
```
./cleos push action evmevmevmevm setbal '{"addy":"2787b98fc4e731d0456b3941f0b3fe2e01439961", "bal":"0000000000000000000000000000000100000000000000000000000000000000"}' -p evmevmevmevm
```
Repeat this action for all genesis accounts.

Now EVM initialization is completed. Verify all EVM account balances directly from Antelope node. 
the simplest command is (replace your contract name "evmevmevmevm" if needed):
```
./cleos get table evmevmevmevm evmevmevmevm account
```
example output:
```
{
  "rows": [{
      "id": 0,
      "eth_address": "2787b98fc4e731d0456b3941f0b3fe2e01439961",
      "nonce": 0,
      "balance": "0000000000000000000000000000000100000000000000000000000000000000",
      "eos_account": "",
      "code": "",
      "code_hash": "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"
    }
  ],
  "more": false,
  "next_key": ""
}
```
Notice that the value ```0000000000000000000000000000000100000000000000000000000000000000``` is in hexdecimal form and must be exactly 64 characters long (256-bit integer value).



## 4. setup the transaction wrapper service for to wrap ETH write requests into Antelope transactions

### Install necessary nodejs tools:
```
sudo apt install nodejs
sudo apt install npm
npm install eosjs
npm install ethereumjs-util
```

### create another Antelope account (sender account) as the wrapper account for signing wrapped Antelope transactions
We use ```a123``` for example (public key EOS8kE63z4NcZatvVWY4jxYdtLg6UEA123raMGwS6QDKwpQ69eGcP, private key 5JURSKS1BrJ1TagNBw1uVSzTQL2m9eHGkjknWeZkjSt33Awtior):
```
./cleos create account eosio a123 EOS8kE63z4NcZatvVWY4jxYdtLg6UEA123raMGwS6QDKwpQ69eGcP EOS8kE63z4NcZatvVWY4jxYdtLg6UEA123raMGwS6QDKwpQ69eGcP
```
(Note, you may need to unlock your Antelope wallet again if it was already timed out)

### prepare the .env file to configure Antelope RPC endpoint, listening port, EVM contract account, sender account, ... etc
```
EOS_RPC="http://127.0.0.1:8888"
EOS_KEY="5JURSKS1BrJ1TagNBw1uVSzTQL2m9eHGkjknWeZkjSt33Awtior"
HOST="127.0.0.1"
PORT="18888"
EOS_EVM_ACCOUNT="evmevmevmevm"
EOS_SENDER="a123"
```
In this environment settings, Tx Wrapper will listen to 127.0.0.1:18888, use ```5JURSKS1BrJ1TagNBw1uVSzTQL2m9eHGkjknWeZkjSt33Awtior``` to wrap and sign the in-coming eth trasnactions into Antelope transactions, and then push them into the Antelope RPC endpoint http://127.0.0.1:8888

### Start Tx Wrapper Service
use index.js from https://github.com/eosnetworkfoundation/TrustEVM/tree/main/peripherals/tx_wrapper
```
node index.js
```

### check if Tx Wrapper is running
```
curl http://127.0.0.1:18888 -X POST -H "Accept: application/json" -H "Content-Type: application/json" --data '{"method":"eth_gasPrice","params":[],"id":1,"jsonrpc":"2.0"}'
```
Example output:
```
{"jsonrpc":"2.0","id":1,"result":"0x22ecb25c00"}
```


### Sign a normal Ethereum transacation to produce a raw signed eth transaction
You can skip this if you are already familiar with. This is a example script sign_ethraw.py:
```
import os
import sys
from getpass import getpass
from binascii import hexlify
from ethereum.utils import privtoaddr, encode_hex, decode_hex, bytes_to_int
from ethereum import transactions
from binascii import unhexlify
import rlp
import json

EVM_SENDER_KEY  = os.getenv("EVM_SENDER_KEY", None)
EVM_CHAINID     = int(os.getenv("EVM_CHAINID", "15555"))

if len(sys.argv) < 6:
    print("{0} FROM TO AMOUNT INPUT_DATA NONCE".format(sys.argv[0]))
    sys.exit(1)

_from = sys.argv[1].lower()
if _from[:2] == '0x': _from = _from[2:]

_to     = sys.argv[2].lower()
if _to[:2] == '0x': _to = _to[2:]

_amount = int(sys.argv[3])
nonce = int(sys.argv[5])

unsigned_tx = transactions.Transaction(
    nonce,
    1000000000,   #1 GWei
    1000000,      #1m Gas
    _to,
    _amount,
    unhexlify(sys.argv[4])
)

if not EVM_SENDER_KEY:
    EVM_SENDER_KEY = getpass('Enter private key for {0}:'.format(_from))

rlptx = rlp.encode(unsigned_tx.sign(EVM_SENDER_KEY, EVM_CHAINID), transactions.Transaction)

print("Eth signed raw transaction is {}".format(rlptx.hex()))
```

Example: sign a eth transaction of transfering amount "1" (minimal positive amount) from 0x2787b98fc4e731d0456b3941f0b3fe2e01439961 to itself without input data, using nonce 0
```
python3 sign_ethraw.py 0x2787b98fc4e731d0456b3941f0b3fe2e01439961 0x2787b98fc4e731d0456b3941f0b3fe2e01439961 1 "" 0
```
Example output:
```
Enter private key for 2787b98fc4e731d0456b3941f0b3fe2e01439961:
Eth signed raw transaction is f86680843b9aca00830f4240942787b98fc4e731d0456b3941f0b3fe2e0143996101808279aaa00c028e3a5086d2ed6c4fdd8e1612691d6dd715386d35c4764726ad0f9f281fb3a0652f0fbdf0f13b3492ff0e30468efc98bef8774ea15374b64a0a13da24ba8879
```


### push Eth raw transaction via Tx Wrapper
For example:
```
curl http://127.0.0.1:18888 -X POST -H "Accept: application/json" -H "Content-Type: application/json" --data '{"method":"eth_sendRawTransaction","params":["0xf86680843b9aca00830f4240942787b98fc4e731d0456b3941f0b3fe2e0143996101808279aaa00c028e3a5086d2ed6c4fdd8e1612691d6dd715386d35c4764726ad0f9f281fb3a0652f0fbdf0f13b3492ff0e30468efc98bef8774ea15374b64a0a13da24ba8879"],"id":1,"jsonrpc":"2.0"}'
```
Example output:
```
{"jsonrpc":"2.0","id":1,"result":"0xee030784f84dde7302bdfb07e6d5eb21c406dbc7824926bb7549dad8a2112db5"}
```
Example output from Tx Wrapper:
```
{"method":"eth_sendRawTransaction","params":["0xf86680843b9aca00830f4240942787b98fc4e731d0456b3941f0b3fe2e0143996101808279aaa00c028e3a5086d2ed6c4fdd8e1612691d6dd715386d35c4764726ad0f9f281fb3a0652f0fbdf0f13b3492ff0e30468efc98bef8774ea15374b64a0a13da24ba8879"],"id":1,"jsonrpc":"2.0"}
----rlptx-----
f86680843b9aca00830f4240942787b98fc4e731d0456b3941f0b3fe2e0143996101808279aaa00c028e3a5086d2ed6c4fdd8e1612691d6dd715386d35c4764726ad0f9f281fb3a0652f0fbdf0f13b3492ff0e30468efc98bef8774ea15374b64a0a13da24ba8879
----response----
{ transaction_id:
   '75e1d63107efe7a89f06681df7804de965356ecde91d7d7ce7352ad0c837f1a6',
  processed:
   { id:
      '75e1d63107efe7a89f06681df7804de965356ecde91d7d7ce7352ad0c837f1a6',
     block_num: 1787,
     block_time: '2022-10-19T03:59:39.500',
     producer_block_id: null,
     receipt:
      { status: 'executed', cpu_usage_us: 392, net_usage_words: 26 },
     elapsed: 392,
     net_usage: 208,
     scheduled: false,
     action_traces: [ [Object] ],
     account_ram_delta: null,
     except: null,
     error_code: null } }
```


### check the ETH balance again
```
./cleos get table evmevmevmevm evmevmevmevm account
```
example output:
```
{
  "rows": [{
      "id": 0,
      "eth_address": "2787b98fc4e731d0456b3941f0b3fe2e01439961",
      "nonce": 1,
      "balance": "00000000000000000000000000000000ffffffffffffffffffffece68e75b000",
      "eos_account": "",
      "code": "",
      "code_hash": "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"
    },{
      "id": 1,
      "eth_address": "0000000000000000000000000000000000000000",
      "nonce": 0,
      "balance": "00000000000000000000000000000000000000000000000000001319718a5000",
      "eos_account": "",
      "code": "",
      "code_hash": "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"
    }
  ],
  "more": false,
  "next_key": ""
}
```
You will notice that the balance of account ```2787b98fc4e731d0456b3941f0b3fe2e01439961``` has changed due to gas fee spent, and nonce was changed to 1 from 0.


### Play with Solidity smart contract, compile simple solidity smart contract:
You can skip this one if you're already familiar. Take this example solidity smart contract:
```
// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

/**
 * @title Storage
 * @dev Store & retrieve value in a variable
 * @custom:dev-run-script ./scripts/deploy_with_ethers.ts
 */
contract Storage {

    uint256 number;

    /**
     * @dev Store value in variable
     * @param num value to store
     */
    function store(uint256 num) public {
        number = num;
    }

    /**
     * @dev Return value 
     * @return value of 'number'
     */
    function retrieve() public view returns (uint256){
        return number;
    }
}
```
Use other open source tool (for example https://remix.ethereum.org/) to compile into the solidity byte code:
```
608060405234801561001057600080fd5b50610150806100206000396000f3fe608060405234801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd5b610043610075565b60405161005091906100d9565b60405180910390f35b610073600480360381019061006e919061009d565b61007e565b005b60008054905090565b8060008190555050565b60008135905061009781610103565b92915050565b6000602082840312156100b3576100b26100fe565b5b60006100c184828501610088565b91505092915050565b6100d3816100f4565b82525050565b60006020820190506100ee60008301846100ca565b92915050565b6000819050919050565b600080fd5b61010c816100f4565b811461011757600080fd5b5056fea26469706673582212209a159a4f3847890f10bfb87871a61eba91c5dbf5ee3cf6398207e292eee22a1664736f6c63430008070033
```
According to the standard of Ethereum, to deploy the contract, we need to put the byte code into "Input_data" field, while setting the "To" field to be null. Using sign_ethraw.py:
```
python3 sign_ethraw.py 2787b98fc4e731d0456b3941f0b3fe2e01439961 "" 0 608060405234801561001057600080fd5b50610150806100206000396000f3fe608060405234801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd5b610043610075565b60405161005091906100d9565b60405180910390f35b610073600480360381019061006e919061009d565b61007e565b005b60008054905090565b8060008190555050565b60008135905061009781610103565b92915050565b6000602082840312156100b3576100b26100fe565b5b60006100c184828501610088565b91505092915050565b6100d3816100f4565b82525050565b60006020820190506100ee60008301846100ca565b92915050565b6000819050919050565b600080fd5b61010c816100f4565b811461011757600080fd5b5056fea26469706673582212209a159a4f3847890f10bfb87871a61eba91c5dbf5ee3cf6398207e292eee22a1664736f6c63430008070033 1
```
You'll get the signed ETH raw transation:
```
f901c401843b9aca00830f42408080b90170608060405234801561001057600080fd5b50610150806100206000396000f3fe608060405234801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd5b610043610075565b60405161005091906100d9565b60405180910390f35b610073600480360381019061006e919061009d565b61007e565b005b60008054905090565b8060008190555050565b60008135905061009781610103565b92915050565b6000602082840312156100b3576100b26100fe565b5b60006100c184828501610088565b91505092915050565b6100d3816100f4565b82525050565b60006020820190506100ee60008301846100ca565b92915050565b6000819050919050565b600080fd5b61010c816100f4565b811461011757600080fd5b5056fea26469706673582212209a159a4f3847890f10bfb87871a61eba91c5dbf5ee3cf6398207e292eee22a1664736f6c634300080700338279aaa0b1b54ab370d1f3e820249b64ca1a1edb53779a20cd6fbf29540af17d95546d2ca02ff89e2476a5022da7f39e0b98b895de6f73445c3961b0affd404c17141c537b
```
push it to Tx Wrapper:
```
curl http://127.0.0.1:18888 -X POST -H "Accept: application/json" -H "Content-Type: application/json" --data '{"method":"eth_sendRawTransaction","params":["0xf901c401843b9aca00830f42408080b90170608060405234801561001057600080fd5b50610150806100206000396000f3fe608060405234801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd5b610043610075565b60405161005091906100d9565b60405180910390f35b610073600480360381019061006e919061009d565b61007e565b005b60008054905090565b8060008190555050565b60008135905061009781610103565b92915050565b6000602082840312156100b3576100b26100fe565b5b60006100c184828501610088565b91505092915050565b6100d3816100f4565b82525050565b60006020820190506100ee60008301846100ca565b92915050565b6000819050919050565b600080fd5b61010c816100f4565b811461011757600080fd5b5056fea26469706673582212209a159a4f3847890f10bfb87871a61eba91c5dbf5ee3cf6398207e292eee22a1664736f6c634300080700338279aaa0b1b54ab370d1f3e820249b64ca1a1edb53779a20cd6fbf29540af17d95546d2ca02ff89e2476a5022da7f39e0b98b895de6f73445c3961b0affd404c17141c537b"],"id":1,"jsonrpc":"2.0"}'
```
Example output:
```
{"jsonrpc":"2.0","id":1,"result":"0x040892a24769881b5c9b9edbd57a869f10e5e4ec9ee185e7d586ec5694d9e639"}
```
Example output from Tx Wrapper:
```
{"method":"eth_sendRawTransaction","params":["0xf901c401843b9aca00830f42408080b90170608060405234801561001057600080fd5b50610150806100206000396000f3fe608060405234801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd5b610043610075565b60405161005091906100d9565b60405180910390f35b610073600480360381019061006e919061009d565b61007e565b005b60008054905090565b8060008190555050565b60008135905061009781610103565b92915050565b6000602082840312156100b3576100b26100fe565b5b60006100c184828501610088565b91505092915050565b6100d3816100f4565b82525050565b60006020820190506100ee60008301846100ca565b92915050565b6000819050919050565b600080fd5b61010c816100f4565b811461011757600080fd5b5056fea26469706673582212209a159a4f3847890f10bfb87871a61eba91c5dbf5ee3cf6398207e292eee22a1664736f6c634300080700338279aaa0b1b54ab370d1f3e820249b64ca1a1edb53779a20cd6fbf29540af17d95546d2ca02ff89e2476a5022da7f39e0b98b895de6f73445c3961b0affd404c17141c537b"],"id":1,"jsonrpc":"2.0"}
----rlptx-----
f901c401843b9aca00830f42408080b90170608060405234801561001057600080fd5b50610150806100206000396000f3fe608060405234801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd5b610043610075565b60405161005091906100d9565b60405180910390f35b610073600480360381019061006e919061009d565b61007e565b005b60008054905090565b8060008190555050565b60008135905061009781610103565b92915050565b6000602082840312156100b3576100b26100fe565b5b60006100c184828501610088565b91505092915050565b6100d3816100f4565b82525050565b60006020820190506100ee60008301846100ca565b92915050565b6000819050919050565b600080fd5b61010c816100f4565b811461011757600080fd5b5056fea26469706673582212209a159a4f3847890f10bfb87871a61eba91c5dbf5ee3cf6398207e292eee22a1664736f6c634300080700338279aaa0b1b54ab370d1f3e820249b64ca1a1edb53779a20cd6fbf29540af17d95546d2ca02ff89e2476a5022da7f39e0b98b895de6f73445c3961b0affd404c17141c537b
----response----
{ transaction_id:
   'c825f65575813c8e70b37fe4495fa37da58f9a2579a4b5f05e5d528c20ddf994',
  processed:
   { id:
      'c825f65575813c8e70b37fe4495fa37da58f9a2579a4b5f05e5d528c20ddf994',
     block_num: 24911,
     block_time: '2022-10-19T07:35:41.000',
     producer_block_id: null,
     receipt:
      { status: 'executed', cpu_usage_us: 481, net_usage_words: 70 },
     elapsed: 481,
     net_usage: 560,
     scheduled: false,
     action_traces: [ [Object] ],
     account_ram_delta: null,
     except: null,
     error_code: null } }
```

### Check account from Antelope blockchain to verify if your solidity bytecode has been deployed:
```
./cleos get table evmevmevmevm evmevmevmevm account
{
  "rows": [{
      "id": 0,
      "eth_address": "2787b98fc4e731d0456b3941f0b3fe2e01439961",
      "nonce": 2,
      "balance": "00000000000000000000000000000000ffffffffffffffffffff7a991984ae00",
      "eos_account": "",
      "code": "",
      "code_hash": "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"
    },{
      "id": 1,
      "eth_address": "0000000000000000000000000000000000000000",
      "nonce": 0,
      "balance": "00000000000000000000000000000000000000000000000000008566e67b5200",
      "eos_account": "",
      "code": "",
      "code_hash": "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"
    },{
      "id": 2,
      "eth_address": "51a97d86ae7c83f050056f03ebbe451001046764",
      "nonce": 1,
      "balance": "0000000000000000000000000000000000000000000000000000000000000000",
      "eos_account": "",
      "code": "608060405234801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd5b610043610075565b60405161005091906100d9565b60405180910390f35b610073600480360381019061006e919061009d565b61007e565b005b60008054905090565b8060008190555050565b60008135905061009781610103565b92915050565b6000602082840312156100b3576100b26100fe565b5b60006100c184828501610088565b91505092915050565b6100d3816100f4565b82525050565b60006020820190506100ee60008301846100ca565b92915050565b6000819050919050565b600080fd5b61010c816100f4565b811461011757600080fd5b5056fea26469706673582212209a159a4f3847890f10bfb87871a61eba91c5dbf5ee3cf6398207e292eee22a1664736f6c63430008070033",
      "code_hash": "72cdff13f49c75af3a3628ec4a6e51b6ec756a9c5bece913018e1bbf539ece2e"
    }
  ],
  "more": false,
  "next_key": ""
}
```
In the above case the solidity bytecode was deployed into eth address ```51a97d86ae7c83f050056f03ebbe451001046764``` which is determined by the "From" address and "nonce".

### Run functions defined in solidity contract
In order to execute function ```store(uint256 num)```, we need to first make the signed eth raw transaction:
```
python3 sign_ethraw.py 2787b98fc4e731d0456b3941f0b3fe2e01439961 51a97d86ae7c83f050056f03ebbe451001046764 0 6057361d000000000000000000000000000000000000000000000000000000000000007b 2
```
in this case ```6057361d``` is the function first 4 bytes of hash of ```store(uint256 num)```(more precisely, ```bytes4(keccak256("store(uint256)"))```, see https://solidity-by-example.org/function-selector/) , we use 123 as the value of ```num```, which is 7b in hex form.
Once you get the raw trasnaction, then we can push into Tx wrapper to sign as the Antelope transaction and push to Antelope blockchain
```
curl http://127.0.0.1:18888 -X POST -H "Accept: application/json" -H "Content-Type: application/json" --data '{"method":"eth_sendRawTransaction","params":["0xf88a02843b9aca00830f42409451a97d86ae7c83f050056f03ebbe45100104676480a46057361d000000000000000000000000000000000000000000000000000000000000007b8279a9a0a2fc71e4beebd9cd1a3d9a55da213f126641f7ed0bb708a3882fa2b85dd6c30ea0164a5d8a8b9b37950091665194f07b5c4e8f6d1b0d6ef162b0e0a1f9bf10c7a7"],"id":1,"jsonrpc":"2.0"}'
```
You'll get a response in Tx Wrapper:
```
{"method":"eth_sendRawTransaction","params":["0xf88a02843b9aca00830f42409451a97d86ae7c83f050056f03ebbe45100104676480a46057361d000000000000000000000000000000000000000000000000000000000000007b8279a9a0a2fc71e4beebd9cd1a3d9a55da213f126641f7ed0bb708a3882fa2b85dd6c30ea0164a5d8a8b9b37950091665194f07b5c4e8f6d1b0d6ef162b0e0a1f9bf10c7a7"],"id":1,"jsonrpc":"2.0"}
----rlptx-----
f88a02843b9aca00830f42409451a97d86ae7c83f050056f03ebbe45100104676480a46057361d000000000000000000000000000000000000000000000000000000000000007b8279a9a0a2fc71e4beebd9cd1a3d9a55da213f126641f7ed0bb708a3882fa2b85dd6c30ea0164a5d8a8b9b37950091665194f07b5c4e8f6d1b0d6ef162b0e0a1f9bf10c7a7
----response----
{ transaction_id:
   '8608a7b373d1f796c81114d19cf95083f85cc3727f47514902daf2e5e4e68664',
  processed:
   { id:
      '8608a7b373d1f796c81114d19cf95083f85cc3727f47514902daf2e5e4e68664',
     block_num: 31652,
     block_time: '2022-10-19T08:31:51.500',
     producer_block_id: null,
     receipt:
      { status: 'executed', cpu_usage_us: 1036, net_usage_words: 31 },
     elapsed: 1036,
     net_usage: 248,
     scheduled: false,
     action_traces: [ [Object] ],
     account_ram_delta: null,
     except: null,
     error_code: null } }
```

Verify from Antelope blockchain to ensure nonce & balance were updated:
```
./cleos get table evmevmevmevm evmevmevmevm account
{
  "rows": [{
      "id": 0,
      "eth_address": "2787b98fc4e731d0456b3941f0b3fe2e01439961",
      "nonce": 3,
      "balance": "00000000000000000000000000000000ffffffffffffffffffff54bdc1c8be00",
      "eos_account": "",
      "code": "",
      "code_hash": "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"
    },{
      "id": 1,
      "eth_address": "0000000000000000000000000000000000000000",
      "nonce": 0,
      "balance": "0000000000000000000000000000000000000000000000000000ab423e374200",
      "eos_account": "",
      "code": "",
      "code_hash": "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"
    },{
      "id": 2,
      "eth_address": "51a97d86ae7c83f050056f03ebbe451001046764",
      "nonce": 1,
      "balance": "0000000000000000000000000000000000000000000000000000000000000000",
      "eos_account": "",
      "code": "608060405234801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd5b610043610075565b60405161005091906100d9565b60405180910390f35b610073600480360381019061006e919061009d565b61007e565b005b60008054905090565b8060008190555050565b60008135905061009781610103565b92915050565b6000602082840312156100b3576100b26100fe565b5b60006100c184828501610088565b91505092915050565b6100d3816100f4565b82525050565b60006020820190506100ee60008301846100ca565b92915050565b6000819050919050565b600080fd5b61010c816100f4565b811461011757600080fd5b5056fea26469706673582212209a159a4f3847890f10bfb87871a61eba91c5dbf5ee3cf6398207e292eee22a1664736f6c63430008070033",
      "code_hash": "72cdff13f49c75af3a3628ec4a6e51b6ec756a9c5bece913018e1bbf539ece2e"
    }
  ],
  "more": false,
  "next_key": ""
}
```

### [Debug only] Investigate the current EVM storage state from Antelope
Since we don't support running View actions directly from Antelope node (read requests will go to TrustEVM-RPC), it is quite complicated to investigate the storage of EVM directly from Antelope. However, If you really want to do that. These are the steps:

#### Identify the "id" field of the contract address
in the above example, contract address is 51a97d86ae7c83f050056f03ebbe451001046764), we use
```
./cleos get table evmevmevmevm evmevmevmevm account --index 2 -L 51a97d86ae7c83f050056f03ebbe451001046764 --key-type sha256
```
to get the response:
```
  "rows": [{
      "id": 2,
      "eth_address": "51a97d86ae7c83f050056f03ebbe451001046764",
...
```
From the response, contract address 51a97d86ae7c83f050056f03ebbe451001046764 will use table id 2. So we get the storage table data of evmevmevmevm (with scope = 2, table name = ```storage```)
```
./cleos get table evmevmevmevm 2 storage
```
example output:
```
{
  "rows": [{
      "id": 0,
      "key": "0000000000000000000000000000000000000000000000000000000000000000",
      "value": "000000000000000000000000000000000000000000000000000000000000007b"
    }
  ],
  "more": false,
  "next_key": ""
}
```



## 5. Start up TrustEVM-node (silkworm node) 

A TrustEVM-node is a node process of the virtual ethereum blockchain that validates virtual ethereum blocks and serves the read requests coming from TrustEVM-RPC. It will not produce blocks. However, it will consume blocks from Antelope node and convert Antelope blocks into Virutal Ethereum blocks in a deterministic way. 

To set it up, we need to first make up a genesis of the virtual ethereum blockchain that maps to the same EVM state of the evm account of the Antelope chain that just initialized in the previous steps.

### Antelope to EVM Block mapping
We need choose a block x in Antelope as the starting point to build up the Virtual EVM blockchain. This block x need to be equal or eariler than the first EVM related transaction happened in Antelope. 

For example:

x = 3

Then the block mapping would be:

Antelope block 4 -> EVM virtual block 1

Antelope block 5 -> EVM virtual block 2

Antelope block 6 -> EVM virtual block 2

Antelope block 7 -> EVM virtual block 3

Antelope block 8 -> EVM virtual block 3

Antelope block 9 -> EVM virtual block 4

...

Antelope block 3 is
```
./cleos get block 3
{
  "timestamp": "2022-10-31T10:08:19.500",
  "producer": "eosio",
  "confirmed": 0,
  "previous": "00000002f38dcf0536565aabdd5a7deac4405619c136f338d850213aeb3de9d4",
  "transaction_mroot": "0000000000000000000000000000000000000000000000000000000000000000",
  "action_mroot": "823207bb72f1f98e3a27ffd69d183e9122e22c322c16027b6a62bd6ca9c45b04",
  "schedule_version": 0,
  "new_producers": null,
  "producer_signature": "SIG_K1_KYBceuuWxANn2Jwh1UbpT4UrW6TW2xq9XS9SUMHDa5ti3JktuoqQmDkrCuRQyZRp9sABRtfxT6RcoAtkNtg8SUGbmCFPud",
  "transactions": [],
  "id": "00000003548b7f7c1b914df458910723b3c52b9d3ba5f337b15c673e998560c3",
  "block_num": 3,
  "ref_block_prefix": 4098724123
}
```

This is a EVM genesis example:
```
    {
        "alloc": {
            "0x2787b98fc4e731d0456b3941f0b3fe2e01439961": { // <--- initial balance
                "balance": "0x0000000000000000000000000000000100000000000000000000000000000000" 
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
        "mixHash": "0x00000003548b7f7c1b914df458910723b3c52b9d3ba5f337b15c673e998560c3", // <--- change it to match your Antelop block id of x
        "nonce": "0x0",
        "timestamp": "0x62546fd7"
    }
```
Fields that you may need to consider to change:
- alloc: all genesis EVM balances (should set to the same value retrieve from ```./cleos get table evmevmevmevm evmevmevmevm account```)
- mixHash: the block hash of the Antelope block that represent the starting block (genesis EVM block) of the virtual EVM blockchain. You can set it to the block hash of the block containing the "setcode" action of the evmevmevmevm account.
- timestamp: block timestamp of the genesis EVM block 

### calculate the correct timestamp value:
The following code (ref TrustEVM/cmd/block_conversion_plugin.cpp) shows the conversion between Antelop block timestamp (where is the offset of micro seconds since 1970-01-01) and the virtual EVM chain timestamp.
```
   static constexpr uint64_t genesis_timestamp = 1667210899500000; //us <---- here we set it equal to the timestamp of Antelope block 3
   static constexpr uint64_t block_interval    = 1000000;          //us

   inline static uint32_t timestamp_to_evm_block(uint64_t antelope_timestamp) {
      assert(antelope_timestamp >= genesis_timestamp); // <--- this requires the antelope block timestamp must be at least some time after year 2022
      return 1 + (antelope_timestamp - genesis_timestamp)/block_interval;
   }
   
   inline static uint64_t evm_block_to_timestamp(uint32_t block_num) {
      return genesis_timestamp + block_num * block_interval;
   }
```
You probably need to change the "genesis_timestamp" to match the one in the genesis Antelop block
the following piece of code is an example to convert timestamp to integer:
```
from datetime import datetime
import sys

#datetime_string = "2022-10-27T06:02:35.500"
format = "%Y-%m-%dT%H:%M:%S.%f"
 
# converting datetime string to datetime 
# object with milliseconds..
date_object = datetime.strptime(sys.argv[1], format)
print("date_object =", date_object)
epoch_time = datetime(1970,1,1)
delta = date_object - epoch_time
print('microsec since epoch:', (int)(delta.total_seconds() * 1000000))
```
Using the above code to find out the corrent timestamp:
```
python3 time_convert.py 2022-10-31T10:08:19.500
date_object = 2022-10-31 10:08:19.500000
microsec since epoch: 166721089950000
```

Starting the TrustEVM process:
```
mkdir ./chain-data
./build/cmd/trustevm-node --chain-data ./chain-data  --plugin block_conversion_plugin --plugin blockchain_plugin --nocolor 1 --verbosity=5
```

## 6. Start up TrustEVM-RPC (silkworm RPC)

The TrustEVM-RPC process provides ethereum compatible RPC service for clients. It queries state (including blocks, accounts, storage) from TrustEVM-node, and it can also run view actions requested by clients.

### To start the trustevm-rpc process:
```
./build/cmd/trustevm-rpc --trust-evm-node=127.0.0.1:8080 --chaindata=./chain-data 
```
here ```--chain-data``` must point to the same directory of the chain-data in TrustEVM-node
by default TrustEVM-rpc will listen on port 8881 for RPC requests.

### Make sure RPC response:
Try 
```
curl --location --request POST 'localhost:8881/' --header 'Content-Type: application/json' --data-raw '{"method":"eth_blockNumber","id":0}'
```
You'll recevie a response similar to follow:
```
{"id":0,"jsonrpc":"2.0","result":"0x1"}
```

### get block by number example
Request
```
curl --location --request POST 'localhost:8881/' --header 'Content-Type: application/json' --data-raw '{"method":"eth_getBlockByNumber","params":["0x1",true],"id":0}'
```
Response:
```
{"id":0,"jsonrpc":"2.0","result":{"difficulty":"0x","extraData":"0x","gasLimit":"0xffffffffffffffff","gasUsed":"0x0","hash":"0x62438d9e228c32a3033a961161f913b700e0d6aecf0ecb141e92ae41d1fb9845","logsBloom":"0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000","miner":"0x0000000000000000000000000000000000000000","mixHash":"0x0000000000000000000000000000000000000000000000000000000000000000","nonce":"0x0000000000000000","number":"0x1","parentHash":"0x0000000000000000000000000000000000000000000000000000000000000000","receiptsRoot":"0x0000000000000000000000000000000000000000000000000000000000000000","sha3Uncles":"0x0000000000000000000000000000000000000000000000000000000000000000","size":"0x3cc","stateRoot":"0x0000000000000000000000000000000000000000000000000000000000000000","timestamp":"0x183c5f2fea0","totalDifficulty":"0x","transactions":[{"blockHash":"0x62438d9e228c32a3033a961161f913b700e0d6aecf0ecb141e92ae41d1fb9845","blockNumber":"0x1","from":"0x2787b98fc4e731d0456b3941f0b3fe2e01439961","gas":"0xf4240","gasPrice":"0x3b9aca00","hash":"0xc4372998d1f7fc02a24fbb381947f7a10ed0826c404b7533e8431df9e48a27d0","input":"0x608060405234801561001057600080fd5b50610150806100206000396000f3fe608060405234801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd5b610043610075565b60405161005091906100d9565b60405180910390f35b610073600480360381019061006e919061009d565b61007e565b005b60008054905090565b8060008190555050565b60008135905061009781610103565b92915050565b6000602082840312156100b3576100b26100fe565b5b60006100c184828501610088565b91505092915050565b6100d3816100f4565b82525050565b60006020820190506100ee60008301846100ca565b92915050565b6000819050919050565b600080fd5b61010c816100f4565b811461011757600080fd5b5056fea26469706673582212209a159a4f3847890f10bfb87871a61eba91c5dbf5ee3cf6398207e292eee22a1664736f6c63430008070033","nonce":"0x0","r":"0x8cd1b11f5a5a9a811ad415b3f3d360a4d8aa4a8bae20467ad3649cfbad25a5ae","s":"0x5eab2829885d473747727d54caae01a8076244c3f6a4af8cad742a248b7a19ec","to":null,"transactionIndex":"0x0","type":"0x0","v":"0x79aa","value":"0x0"}],"transactionsRoot":"0x0000000000000000000000000000000000000000000000000000000000000000","uncles":[]}}
```

### get balance example:
Request:
```
curl --location --request POST 'localhost:8881/' --header 'Content-Type: application/json' --data-raw '{"method":"eth_getBalance","params":["9edf022004846bc987799d552d1b8485b317b7ed","latest"],"id":0}'
```
response:
```
{"id":0,"jsonrpc":"2.0","result":"0x100"}
```


### Example: execute View actions from RPC
Request: 
data - 0x2e64cec1 is the hash of a solidity function ```retrieve() public view returns (uint256)```
```
curl --location --request POST 'localhost:8881/' --header 'Content-Type: application/json' --data-raw '{"method":"eth_call","params":[{"from":" 2787b98fc4e731d0456b3941f0b3fe2e01439961","to":"3f4b0f92007341792aa61e065484e48e583ebeb9","data":"0x2e64cec1"},"latest"],"id":11}'
```
Response:
```
{"id":11,"jsonrpc":"2.0","result":"0x000000000000000000000000000000000000000000000000000000000000007b"}
```



## 7. Setup proxy to separate read requests and write requests

The proxy program will separate Ethereum's write requests (such as eth_sendRawTransaction,eth_gasPrice) from other requests (treated as read requests). The write requests should go to Transaction Wrapper (which wrap the ETH transaction into Antelope transaction and sign it and push to the Antelope blockchain). The read requests should go to TrustEVM-RPC.

In order to get it working, docker is required. 

To install docker in Linux, see https://docs.docker.com/engine/install/ubuntu/

You can find the proxy tool from TrustEVM/perfipherals/proxy
```
cd TrustEVM/peripherals/proxy/
```
Edit the file ```nginx.conf```, find the follow settings:
```
  upstream write {
    server 192.168.56.101:18888;
  }
  
  upstream read {
    server 192.168.56.101:8881;
  }
```

change the IP & port of the write session to your Transaction Wrapper server endpoint

change the IP & port of the read session to your TrustEVM-RPC server endpoint

build the docker image for the proxy program
```
sudo docker build .
```
check the image ID after building the image
```
sudo docker image ls
```
Example output:
```
REPOSITORY   TAG       IMAGE ID       CREATED         SIZE
<none>       <none>    49564d312df7   2 hours ago     393MB
debian       jessie    3aaeab7a4777   19 months ago   129MB
```

Run the proxy in docker:
```
sudo docker run -p 81:80 -v ${PWD}/nginx.conf:/etc/nginx.conf 49564d312df7
```
Here we map the host port 81 to the port 80 inside the docker.

Check if the proxy is responding:
```
curl http://127.0.0.1:81 -X POST -H "Accept: application/json" -H "Content-Type: application/json" --data '{"method":"eth_gasPrice","params":[],"id":1,"jsonrpc":"2.0"}'
```
Example response:
```
{"jsonrpc":"2.0","id":1,"result":"0x22ecb25c00"}
```


## 8. Setup Metamask Chrome extension

- Install Metamask Plugin in Chrome
- Click Account ICON on the top right, the Find Settings -> Networks -> Add Network

![image](https://user-images.githubusercontent.com/37097018/202395747-7ee25460-7cab-4a14-af45-99835a69d86e.png)

Fill in the following information:

- Network Name: any name is OK
- New RPC URL: YOUR PROXY LISTENING ENDPOINT
- Chain ID: 15555
- Currency Symbol: EVM
  
And then click "Save"

After setting up Metamask, you should able to import or create accounts via this plugin, or go to https://metamask.github.io/test-dapp/ to test basic dapp integration.



