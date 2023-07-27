# Mesuring EVM contract performance

### Create working folder
```
mkdir ~/evmperf
cd ~/evmperf
```


### Build EVM contract 
_set stack-size in src/CMakeList.txt to_ **16384** _before building_

```
cd ~/evmperf
git clone https://github.com/eosnetworkfoundation/eos-evm
cd eos-evm/contract
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DWITH_LOGTIME=ON ..
make -j4
```

### Build eos-evm-node and eos-evm-rpc
```
cd ~/evmperf/eos-evm
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4 eos-evm-node eos-evm-rpc
```


### Build leap v3.2.2-logtime
```
cd ~/evmperf
git clone https://github.com/elmato/leap
cd leap
git checkout v3.2.2-logtime
git submodules update --init --recursive
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
```

### Setup nodeos_eos_evm_server.py python env
```
cd ~/evmperf/leap/build/tests
ln -s ~/evmperf/eos-evm/tests/leap/nodeos_eos_evm_server.py nodeos_eos_evm_server.py
sed -i 's/SYS/EOS/g' core_symbol.py
python3 -m venv venv
source venv/bin/activate
pip install 'web3<6' flask flask-cors
```


### Launch nodeos and deploy EVM contract
_use --use-eos-vm-oc=1 when launching **nodeos_eos_evm_server.py** if you want to measure OC performance_

```
cd ~/evmperf/leap/build/tests
source venv/bin/activate
cd ..
./tests/nodeos_eos_evm_server.py --leave-running --eos-evm-contract-root ~/evmperf/eos-evm/contract/build
```

(_wait until nodeos_eos_evm_server start listening at localhost:5000_)

### Launch eos-evm-node
```
cd ~/evmperf/eos-evm/build/cmd
rm -rf chaindata etl-temp config-dir
./eos-evm-node --plugin=blockchain_plugin --ship-endpoint=127.0.0.1:8999 --genesis-json=$HOME/evmperf/leap/build/eos-evm-genesis.json --verbosity=4
```

### Launch eos-evm-rpc
```
cd ~/evmperf/eos-evm/build/cmd
./eos-evm-rpc --eos-evm-node=127.0.0.1:8080 --http-port=0.0.0.0:8881 --chaindata=./ --api-spec=eth,debug,net,trace --verbosity=4
```

### Install scripts dependencies
```
cd ~/evmperf/eos-evm/tests/leap/nodeos_eos_evm_server
yarn install
```


### Deploy Uniswap V2
```
cd ~/evmperf/eos-evm/tests/leap/nodeos_eos_evm_server
npx hardhat run scripts/deploy-uniswap.js
```

### Deploy Recursive contract
```
cd ~/evmperf/eos-evm/tests/leap/nodeos_eos_evm_server
npx hardhat run scripts/deploy-recursive.js
```

### Allow Uniswap router to transfer AAA tokens
```
cd ~/evmperf/eos-evm/tests/leap/nodeos_eos_evm_server
npx hardhat approve --erc20 0xCf7Ed3AccA5a467e9e704C703E8D87F634fB0Fc9 --router 0x9fE46736679d2D9a65F0992F2272dE9f3c7fa6e0 --amount 10000
```

### Tail logs _(separate console)_
```
cd ~/evmperf/eos-evm/tests/leap/nodeos_eos_evm_server
./extract-logtime-cmd.sh ~/evmperf/leap/build/var/lib/node_01/stderr.txt
```

### Execute AAA=>BBB swaps
```
while true
do
npx hardhat swap --router 0x9fE46736679d2D9a65F0992F2272dE9f3c7fa6e0 --path 0xCf7Ed3AccA5a467e9e704C703E8D87F634fB0Fc9,0xDc64a140Aa3E981100a9becA4E685f962f0cF6C9 --amount 0.5
done

```

### Execute AAA=>BBB=>CCC swaps
```
while true
do
npx hardhat swap --router 0x9fE46736679d2D9a65F0992F2272dE9f3c7fa6e0 --path 0xCf7Ed3AccA5a467e9e704C703E8D87F634fB0Fc9,0xDc64a140Aa3E981100a9becA4E685f962f0cF6C9,0x5FC8d32690cc91D4c39d9d3abcBD16989F875707 --amount 0.5
done
```

### Execute erc20 transfers
```
while true
do
npx hardhat transfer --from 0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266 --to 0x70997970C51812dc3A010C7d01b50e0d17dc79C8 --contract 0xCf7Ed3AccA5a467e9e704C703E8D87F634fB0Fc9 --amount 0.1
done
```

### Execute native transfers
```
while true
do
npx hardhat native-transfer --from 0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266 --to 0x70997970C51812dc3A010C7d01b50e0d17dc79C8 --amount 1
done
```