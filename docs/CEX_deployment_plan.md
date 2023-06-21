# Deploy and Support EOS EVM Network for Centralized Exchanges

This document will describes the minimum requirements to deploy and support EOS EVM Network for Centralized Cryptocurrency Exchanges.

## Minimum Architecture 
This is the minimum setup to run a EOS EVM service. It does not contain the high availability setup. Exchanges can duplicate the real-time service part for high availability purpose if necessary. 
```
Real-time service:
    +--VM1------------------------------+       +--VM2-----------------------+
    | leap node running in head mode    | <---- | eos-evm-node & eos-evm-rpc | <-- eth compatible read requests (e.g. eth_blockNumber, eth_getBlockByNumber ...)
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
for more details please refer to https://github.com/eosnetworkfoundation/eos-evm/blob/main/docs/compilation_and_testing_guide.md

- Eos-evm-miner: please refer to https://github.com/eosnetworkfoundation/eos-evm-miner

## Preparing your EOS miner account
- create your miner account (for example: a123) on EOS Network
- open account balance on EVM side:
  ```
  ./cleos push action eosio.evm open '{"owner":"a123"}' -p a123
  ```
- powerup the miner account with enough CPU & NET resource (for example: 1min CPU. 10 MB net per day)
