# Deploy and Support EOS EVM Network for Centralized Exchanges

This document will describes the minimum requirements to deploy and support EOS EVM Network for Centralized Cryptocurrency Exchanges.

## Minimum Architecture 
This is the minimum setup to run a EOS EVM service. It does not contain the high availability setup. Exchanges can duplicate the real-time service part for high availability purpose if necessary. 
```
Real-time service:
    +--VM1------------------------------+       +--VM2-----------------------+
    | leap node running in head mode    | <---- | eos-evm-node & eos-evm-rpc | <-- eth compatible read requests (e.g. eth_getBlockByNumber, eth_call ...)
    | with state_history_plugin enabled |       +----------------------------+
    +-----------------------------------+              
                                       ^
                                       |                           +--VM2--------------------+
                                       \-- push EOS transactions --| eos-evm-miner (wrapper) | <-- eth_gasPrice / eth_sendRawTransaction
                                                                   +-------------------------+

Periodic Backup service: 
    +--VM3-------------------------------------+        +--VM3-----------------------+
    | leap node running in irrversible mode    | <----- | eos-evm-node & eos-evm-rpc | 
    | with state_history_plugin enabled        |        +----------------------------+
    +------------------------------------------+         
```
- VM1: this VM will run EOS leap node in head mode with state_history_plugin enable. A high end CPU with good single threaded performance is recommended. RAM: 64GB+, SSD 2TB+ (for storing block logs & state history from the EVM genesis time (2023-04-05T02:18:09 UTC) up to now)
- VM2: this VM will run eos-evm-node, eos-evm-rpc & eos-evm-miner. Recommend to use 8 vCPU, 32GB+ RAM, and 1TB+ SSD
- VM3: this VM will run leap (in irrversible mode), eos-evm-node & eos-evm-rpc and mainly for backup purpose. Recommend to use 8 vCPU, 64GB+ RAM, 3TB+ SSD (backup files can be large).


## Building necessary components:
OS: Recommend to use ubuntu 22.04
- EOS (leap) Node: please refer to https://github.com/AntelopeIO/leap
- eos-evm-node, eos-evm-rpc:
```
git clone https://github.com/eosnetworkfoundation/eos-evm.git
cd eos-evm
git checkout release/0.5
git submodule update --init --recursive
mkdir build; cd build;
cmake .. && make -j8
```
for more details please refer to https://github.com/eosnetworkfoundation/eos-evm

- Eos-evm-miner: please refer to https://github.com/eosnetworkfoundation/eos-evm-miner

## Preparing your EOS miner account
- create your miner account (for example: a123) on EOS Network
- open account balance on EVM side:
  ```
  ./cleos push action eosio.evm open '{"owner":"a123"}' -p a123
  ```
- powerup the miner account with enough CPU & NET resource (for example: 1min CPU. 10 MB net per day). You can use some existing auto powerup service such as https://eospowerup.io/auto or push the powerup transaction (eosio::powerup) via cleos.

## Running the EOS (leap) nodes with state_history_plugin (with trace-history=true)

- For the first time: You need a snapshot file whose timestamps is before the EVM genesis timestamp 2023-04-05T02:18:09 UTC.
- The block log and state history logs need to be replayed from the snapshot time and need to be saved together in the periodic backup.
- The block logs, state-history logs can not be truncated in the future. This is because eos-evm-node may ask for old blocks for replaying the EVM chain. 
- You can download the snapshot from any public EOS snapshot service providers (such as https://snapshots-main.eossweden.org/), or use your own snapshot.
- Supported version: Leap 4.x (recommend), Leap 3.x
  
example data-dir/config.ini
```
# 48GB for VM with 64GB RAM, possible require bigger VM in the future
chain-state-db-size-mb = 49152

access-control-allow-credentials = false

allowed-connection = any
p2p-listen-endpoint = 0.0.0.0:9876
p2p-max-nodes-per-host = 10
http-server-address = 0.0.0.0:8888
state-history-endpoint = 0.0.0.0:8999

trace-history = true
chain-state-history = false

http-max-response-time-ms = 1000

# add or remove peers if needed
p2p-peer-address=eos.p2p.eosusa.io:9882
p2p-peer-address=p2p.eos.cryptolions.io:9876
p2p-peer-address=p2p.eossweden.se:9876
p2p-peer-address=fullnode.eoslaomao.com:443
p2p-peer-address=mainnet.eosamsterdam.net:9876

# Plugin(s) to enable, may be specified multiple times
plugin = eosio::producer_plugin
plugin = eosio::chain_api_plugin
plugin = eosio::http_plugin
plugin = eosio::producer_api_plugin
plugin = eosio::state_history_plugin
plugin = eosio::net_plugin
plugin = eosio::net_api_plugin
plugin = eosio::db_size_api_plugin
```
example run command (VM1, head or speculative mode):
```
sudo ./nodeos --p2p-accept-transactions=0 --database-map-mode=locked --data-dir=./data-dir  --config-dir=./data-dir --http-max-response-time-ms=200 --disable-replay-opts --max-body-size=10000000
```
example run command (VM3, irreversible mode):
```
sudo ./nodeos --read-mode=irreversible --p2p-accept-transactions=0 --database-map-mode=locked --data-dir=./data-dir  --config-dir=./data-dir --http-max-response-time-ms=200 --disable-replay-opts --max-body-size=10000000
```
Notes:
- To boost performance, it is important to set "--p2p-accept-transactions=0" to disallow executing transactions (which are not yet included in a blocks) received from other peers.
- `--database-map-mode=locked` requires sudo access. it is recommended but not mandatory.
- for the 1st time, run it also with `--snapshot=SNAPSHOT_FILE` to begin with the snapshot state.


## Running the eos-evm-node & eos-evm-rpc

