# Deploy and Support EOS EVM Network for Centralized Exchanges and EOS-EVM Node operators

This document will describes the minimum requirements to deploy and support EOS EVM Network for Centralized Cryptocurrency Exchanges.

1. [Minimum Architecture](#MA)
2. [Building necessary components](#BNC)
3. [Running the EOS (leap) nodes with state_history_plugin](#REN)
4. [Running the eos-evm-node & eos-evm-rpc](#REE)
5. [Backup & Recovery of leap & eos-evm-node](#BR)
6. [Running the eos-evm-miner service](#RMS)
7. [[Exchanges Only]: Calculate the irreversible block number from EOS chain to EOS-EVM Chain](#CRB)
8. [[EVM-Node operators Only]: Setting up the read-write proxy and explorer](#RWP)


<a name="MA"></a>
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
Leap node stands for the EOS (Level 1) blockchain, and eos-evm-node stands for the EOS-EVM (Level 2) blockchain. eos-evm-rpc talk to eos-evm-node 
 in the same VM and it is used for providing read-only ETH APIs (such as eth_getBlockByNumber, eth_call, eth_blockNumber, ... ) which is compatible with standard ETH API. For ETH write requests eth_sendRawTransaction, and eth_gasPrice, they will be served via eos-evm-miner instead of eos-evm-rpc.
 
- VM1: this VM will run EOS leap node in head mode with state_history_plugin enable. A high end CPU with good single threaded performance is recommended. RAM: 64GB+, SSD 2TB+ (for storing block logs & state history from the EVM genesis time (2023-04-05T02:18:09 UTC) up to now)
- VM2: this VM will run eos-evm-node, eos-evm-rpc & eos-evm-miner. Recommend to use 8 vCPU, 32GB+ RAM, and 1TB+ SSD
- VM3: this VM will run leap (in irrversible mode), eos-evm-node & eos-evm-rpc and mainly for backup purpose. Recommend to use 8 vCPU, 64GB+ RAM, 3TB+ SSD (backup files can be large).


<a name="BNC"></a>
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

<a name="REN"></a>
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


<a name="REE"></a>
## Running the eos-evm-node & eos-evm-rpc

- Copy the mainnet EOS-EVM genesis from https://github.com/eosnetworkfoundation/evm-public-docs/blob/main/mainnet-genesis.json
- run the eos-evm-node
```
mkdir ./chain-data
./eos-evm-node --ship-endpoint=<NODEOS_IP_ADDRESS>:8999 --ship-core-account eosio.evm --chain-data ./chain-data --chain-id=17777 --plugin block_conversion_plugin --plugin blockchain_plugin --nocolor 1  --verbosity=4 --genesis-json=./genesis.json
```
- run the eos-evm-rpc (must be in the same VM as eos-evm-node)
```
./eos-evm-rpc --api-spec=eth,debug,net,trace --chain-id=17777 --http-port=0.0.0.0:8881 --eos-evm-node=127.0.0.1:8080 --chaindata=./chain-data
```
- The EVM state, logs will be stored in ./chain-data directory

The eos-evm-rpc will talk to eos-evm-node and provide the eth compatible RPC services, for example, you can check the current block number of eos-evm-node via:
```
curl --location --request POST '127.0.0.1:8881/' --header 'Content-Type: application/json' --data-raw '{"method":"eth_blockNumber","params":["0x1",false],"id":0}'
```
- if either leap or eos-evm-node can't start, follow the recovery process in the next session.

<a name="BR"></a>
## Backup & Recovery of leap & eos-evm-node
- It is quite important for node operator to backup all the state periodically (for example, once per day).
- backup must be done on the leap node running in irreversible mode. And because of such, all the blocks in eos-evm-node has been finialized and it will never has a fork.
- create the nodeos (leap) snapshot:
  ```
  curl http://127.0.0.1:8888/v1/producer/create_snapshot
  ```
- gracefull kill all processes:
```
pkill eos-evm-node
sleep 2.0
pkill eos-evm-rpc
sleep 2.0
pkill nodeos
```
- backup leap's data-dir folder and eos-evm-node's chain-data
- restart back nodeos, eos-evm-node & eos-evm-rpc
- for leap recovery, please restore the data-dir folder of the last backup and use the leap's snapshot
- for eos-evm-node recovery, please restore the chain-data folder of the last backup.

<a name="RMS"></a>
## Running the eos-evm-miner service 
The miner service will help to package the EVM transaction into EOS transaction and set to the EOS network. It will provide the following 2 eth API:
- eth_gasPrice: retrieve the currect gas price from EOS Network
- eth_sendRawTransaction: package the ETH transaction into EOS transaction and push into the EOS Network.
clone the https://github.com/eosnetworkfoundation/eos-evm-miner repo

- create your miner account (for example: a123) on EOS Network
- open account balance on EVM side:
  ```
  ./cleos push action eosio.evm open '{"owner":"a123"}' -p a123
  ```
- powerup the miner account with enough CPU & NET resource (for example: 1min CPU. 10 MB net per day). You can use some existing auto powerup service such as https://eospowerup.io/auto or push the powerup transaction (eosio::powerup) via cleos.

- prepare the .env file with the correct information
```
PRIVATE_KEY=5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3
MINER_ACCOUNT=a123
RPC_ENDPOINTS=http://127.0.0.1:8888|http://192.168.1.1:8888
PORT=18888
LOCK_GAS_PRICE=true
MINER_PERMISSION="active"
EXPIRE_SEC=60
```
- build and start the miner
```
npm install
yarn build
yarn start
```
- test if the miner is working
```
curl http://127.0.0.1:18888 -X POST -H "Accept: application/json" -H "Content-Type: application/json" --data '{"method":"eth_gasPrice","params":[],"id":1,"jsonrpc":"2.0"}'
{"jsonrpc":"2.0","id":1,"result":"0x22ecb25c00"}
```

<a name="CRB"></a>
## [For centralized exchanges] Calculate the irreversible block number from EOS (L1) chain to EOS-EVM (L2) Chain
For centralized exchange it is important to know up to which block number the chain is irrversible. This is the way for EOS-EVM:
- ensure the leap node & eos-evm-node are fully sync-up.
- do a get_info request to leap node.
```
{
  "server_version": "943d1134",
  "chain_id": "aca376f206b8fc25a6ed44dbdc66547c36c6c33e3a119ffbeaef943642f0e906",
  "head_block_num": 316609050,
...
  "earliest_available_block_num": 302853021,
  "last_irreversible_block_time": "2023-06-23T03:10:35.500"
}
```
- in the above example all EVM blocks before `"last_irreversible_block_time": "2023-06-23T03:10:35.500"` are irreversible. You can use the time conversion script:
`
python3 -c 'from datetime import datetime; print(hex(int((datetime.strptime("2023-06-23T03:10:35.500","%Y-%m-%dT%H:%M:%S.%f")-datetime(1970,1,1)).total_seconds())))'
`
to get the EVM irreversible blocktime in hex ```0x64950d2b```, in this case the EVM blocks up to ```6828746``` are irreversible.

`
curl --location --request POST '127.0.0.1:8881/' --header 'Content-Type: application/json' --data-raw '{"method":"eth_getBlockByNumber","params":["6828746",false],"id":0}'
{"id":0,"jsonrpc":"2.0","result":{"difficulty":"0x1","extraData":"0x","gasLimit":"0x7ffffffffff","gasUsed":"0x0","hash":"0x563fe6290cf38d55e4c4d2c86886032a1734ad1e467b7ce06ff52f12ee378b0d","logsBloom":"0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000","miner":"0xbbbbbbbbbbbbbbbbbbbbbbbb5530ea015b900000","mixHash":"0x12df121840088703a9fe2f305eefe25dbe97bc57f7e127d922ffa8d005aceea6","nonce":"0x0000000000000000","number":"0x6832ca","parentHash":"0xafebdcf129bd506cee25892b2f20703e5ae98bd95557a04b91ac0f56a3433824","receiptsRoot":"0x0000000000000000000000000000000000000000000000000000000000000000","sha3Uncles":"0x0000000000000000000000000000000000000000000000000000000000000000","size":"0x202","stateRoot":"0x0000000000000000000000000000000000000000000000000000000000000000","timestamp":"0x64950d2b","totalDifficulty":"0x6832cb","transactions":[],"transactionsRoot":"0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421","uncles":[]}}
`

### Monitor funds deposits into exchanges:
- For EOS tokens on EOS-EVM: Since this is the native token, similar to other ETH compatible networks, exchanges can use similar way to query EVM blocks (such as using eth_getBlockByNumber) up to the last irreversible EVM blocks as explained above. Or query the account balance using eth_getBalance if needed.
- For ERC20 tokens on EOS-EVM: Also similar to other ETH networks, exchanges can execute the ETH view action (eth_call) to extract the balance of any EVM account, or monitor each EVM blocks.
 
### Confirm if a fund withdrawal is successful or fail:
In order to monitoring fund withdrawal, exchanges need to consider:
- The ```EXPIRE_SEC``` value set in the eos-evm-miner. This value will control how long will the EOS trasaction expires in such a way that it will never be included in the blockchain after expiration.
- The irreversible EVM block number.

For example:
- 1. At 9:00:00AM UTC, the upstream signed the ETH transaction with ETH compatible private key and then call eth_sendRawTransaction
- 2. The eos-evm-miner packages the EVM transaction into EOS transaction and signed it with EOS private key, and push to native EOS network.
- 3. If `EXPIRE_SEC` is set to 60, the EOS transaction will expire at 9:01:00AM. So we need to wait until the result of `./cleos get info` shows that the last_irreversible_block_time >= 9:01:00AM. At most cases, the EOS Network will have around 3 minute finality time, so we probably need to wait until 9:04:00AM.
- 4. Since all transactions up 9:01:00AM are irreversible, we scan each EVM block between 9:00:00AM and 9:01:01AM (1 sec max timestamp difference between EOS and EOS-EVM blocks) to confirm whether the transaction is included in the EVM blockchain (so as the native EOS blockchain). We can confirm the withdrawal is successfull if we find the transaction in this range. Otherwise, the transaction is already expired and can not be included in the blockchain.
- 5. Alternative to 4, instead of scanning all blocks in the time range, we can get the nonce number of the EVM account to confirm if the withdrawal is successful. But this method only works if there is only one withdrawal pending under that EVM account.

<a name="RWP"></a>
## [Optional] For EVM-Node operators Only: Setting up the read-write proxy and explorer
This is same as https://github.com/eosnetworkfoundation/eos-evm/blob/main/docs/local_testnet_deployment_plan.md
- Setup the read-write proxy to integrate the ETH read requests (eos-evm-rpc) & write requests (eos-evm-miner) together with a single listening endpoint.
- Setup your own EOS-EVM Explorer

