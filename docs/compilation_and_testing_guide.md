 ## Compile EVM Contract
 
Prerequisite:

   cmake 3.16 or later
   Compile and install cdt project (https://github.com/AntelopeIO/cdt.git), please refer to https://github.com/AntelopeIO/cdt. 
   After install, you need to ensure the following soft links exist (if not, please manually create these soft links). 
   
      /usr/local/bin/cdt-cpp
      /usr/local/bin/eosio-wast2wasm
      /usr/local/bin/eosio-wasm2wast

Checkout eos-evm repo:
```
git clone https://github.com/eosnetworkfoundation/eos-evm.git
cd eos-evm
git submodule update --init --recursive
```

Go into the contract folder
```
cd contract
mkdir build
cd build
cmake ..
make -j
```
You should now see the compile contract

   eos-evm/contract/build/evm_runtime/evm_runtime.wasm
   eos-evm/contract/build/evm_runtime/evm_runtime.abi


[Optional, but required for EVM token testings] to compile contract with debug actions, use
```
cmake .. -DWITH_TEST_ACTIONS=1
Instead of 
cmake ..
```
<b>Note: if compilation errors occur, you may need to comment out some of the debug actions</b>


## Compile eos-evm-node, eos-evm-rpc, unit_test
Prerequisite:

cmake 3.19 or later
gcc 10 or clang 10 (I personally recommend gcc 11, because clang version may not be compatible with the cdt requirements)
Other prerequisites may be needed, please refer https://github.com/eosnetworkfoundation/silkworm/tree/6ee0e2459e4c70bcabbb62e146a880b1cb42a4aa

Step:
run ./rebuild_gcc_release.sh

You will get the following binaries:
```
eos-evm/build/cmd/eos-evm-node
eos-evm/build/cmd/eos-evm-rpc
eos-evm/build/cmd/unit_test
```


## Deploy EVM contract to nodeos

Prerequisites:

   Successfully built nodeos in leap repo (https://github.com/AntelopeIO/leap.git)
   Successfully built system contracts in eos-system-contracts repo (https://github.com/eosnetworkfoundation/eos-system-contracts.git)
   Successfully compiled EVM smart contracts

<b> Bootstrapping Protocol Features </b>

Change directory to the parent folder of “leap”
Setup a soft link “./cleos” pointing to the right cleos binary built from leap
start nodeos, it will listen to rpc port 8888 by default

Activate the following protocol features 

```
echo "=== schedule protocol feature activations ==="
curl --data-binary '{"protocol_features_to_activate":["0ec7e080177b2c02b278d5088611686b49d739925a92d9bfcacd7fc6b74053bd"]}' http://127.0.0.1:8888/v1/producer/schedule_protocol_feature_activations

sleep 1.0

echo "=== set boot contract ==="
./cleos set code eosio ../eos-system-contracts/build/contracts/eosio.boot/eosio.boot.wasm
./cleos set abi eosio ../eos-system-contracts/build/contracts/eosio.boot/eosio.boot.abi

sleep 1.0

echo "=== init protocol features ==="
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


<b>Create EVM contract account</b>

Create account evmevmevmevm (here private key is 5JURSKS1BrJ1TagNBw1uVSzTQL2m9eHGkjknWeZkjSt33Awtior) 
```
./cleos create account eosio evmevmevmevm EOS8kE63z4NcZatvVWY4jxYdtLg6UEA123raMGwS6QDKwpQ69eGcP EOS8kE63z4NcZatvVWY4jxYdtLg6UEA123raMGwS6QDKwpQ69eGcP
```
Set EVM contract into account evmevmevmevm
```
./cleos set code evmevmevmevm ../eos-evm/contract/build/evm_runtime/evm_runtime.wasm
./cleos set abi evmevmevmevm ../eos-evm/contract/build/evm_runtime/evm_runtime.abi
```
(Optional) Verify if account has set code, you will got the non-zero code hash which means contract is deployed, for example:
```
./cleos get code evmevmevmevm
code hash: 8789fe904edbe10aafb5330da7ab41395f8ae1620a6d2a0a4f68fe75464c1e19
```

<b>Initialize EVM contract</b>
The EVM contract will not allow any actions except `init` until its chain id & native token is configured
```
./cleos push action evmevmevmevm init '{"chainid": 15555}'
```

## Set balance and transfer native EVM token via EVM smart contract: ##

Prerequisite:
   Compiled EVM smart contract using debug mode (with setbalance action)
   Complied leap
   Compiled CDT
   EVM smart contract deployed
   [Optional] Able to access the EVM script repo (https://github.com/elmato/evm-scripts.git), if not, please use the script code from this doc below


<b>Create test accounts</b>

First we need to have some test account (as mentioned in https://github.com/elmato/evm-scripts) :
```
> python gen_key.py
privkey: a3f1b69da92a0233ce29485d3049a4ace39e8d384bbc2557e3fc60940ce4e954
adddress: 0x2787b98fc4e731d0456b3941f0b3fe2e01439961
> python gen_key.py
privkey: e6a3409b151beef0fdad36dd66364a9ef38e7d9f33fb8ff6d1dc8f17f85ead87
adddress: 0x9edf022004846bc987799d552d1b8485b317b7ed
```

<b>Set balance & transfer</b>

Make sure your current balance is 0 and the check balance script works:
In evm-scripts, run:
```
python3 ./get_balance.py 2787b98fc4e731d0456b3941f0b3fe2e01439961
```
You’ll get 0 as the current balance

Please find the get_balance.py from https://github.com/eosnetworkfoundation/eos-evm/tree/kayan-rpc-fix/testing-utils


Push debug action setbal:
```
./cleos push action evmevmevmevm setbal '{"addy":"2787b98fc4e731d0456b3941f0b3fe2e01439961", "bal":"0000000000000000000000000000000100000000000000000000000000000000"}' -p evmevmevmevm
```
Note: the balance value need to be raw 32 hex bytes

check your balance again
In evm-scripts, run:
```
python3 ./get_balance.py 2787b98fc4e731d0456b3941f0b3fe2e01439961
```
You should see a non-zero number. Whether or not it looks like garbage, but that’s fine!

<b>Step 3: send balance</b>

please find send_via_cleos.py from https://github.com/eosnetworkfoundation/eos-evm/tree/kayan-rpc-fix/testing-utils

command:
```
python3 ./send_via_cleos.py FROM_ACCOUNT TO_ACCOUNT AMOUNT NONCE
```
For example:
```
python3 ./send_via_cleos.py 2787b98fc4e731d0456b3941f0b3fe2e01439961 0x9edf022004846bc987799d552d1b8485b317b7ed 100 2

Enter private key for 2787b98fc4e731d0456b3941f0b3fe2e01439961:*****

act_data is {'ram_payer': 'evmevmevmevm', 'rlptx': 'f867028522ecb25c00830186a0949edf022004846bc987799d552d1b8485b317b7ed64808279aaa0664bc6882fa30922d0376d1d3addc1dc0b818ead9b8909858a21779db58a46c8a00d3524bcbbdfe76413cda6027849e787f07e0d58aa1c13d8abbcbbcc1447bf5b'}
executed transaction: 2a47a2782419b16713e8c01739343a589b2a24b168411350b77ba1bd7104d653  208 bytes  382 us
#  evmevmevmevm <= evmevmevmevm::pushtx         {"ram_payer":"evmevmevmevm","rlptx":"f867028522ecb25c00830186a0949edf022004846bc987799d552d1b8485b31...
=>                                return value: {"account":{"read":6,"update":3,"create":0,"remove":0},"storage":{"read":0,"update":0,"create":0,"re...
```

<b>Common Errors seen when pushing evm action:</b>

assertion failure with message: validate_transaction error: 21, 
This is because the nonce value is incorrect

assertion failure with message: validate_transaction error: 23, 
Insufficient account balance

assertion failure with message: validate_transaction error: 20, 
kSenderNoEOA: (looks like this means the “from” account can’t be the contract account)

Other errors are defined in:
eos-evm/silkworm/core/silkworm/consensus/validation.hpp


## Playing with ethereum contract
Prerequisite:
   Compiled EVM smart contract using debug mode (with setbalance action & updatecode action)
   Complied leap
   Compiled CDT
   EVM smart contract deployed

<b>Compile a simple solidity code in https://remix.ethereum.org/ </b>

And download the hex code in json.
Example solidity contract:
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
You will get the bytecode after compilation:
```
{
	"functionDebugData": {},
	"generatedSources": [],
	"linkReferences": {},
	"object": "608060405234801561001057600080fd5b50610150806100206000396000f3fe608060405234801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd5b610043610075565b60405161005091906100d9565b60405180910390f35b610073600480360381019061006e919061009d565b61007e565b005b60008054905090565b8060008190555050565b60008135905061009781610103565b92915050565b6000602082840312156100b3576100b26100fe565b5b60006100c184828501610088565b91505092915050565b6100d3816100f4565b82525050565b60006020820190506100ee60008301846100ca565b92915050565b6000819050919050565b600080fd5b61010c816100f4565b811461011757600080fd5b5056fea26469706673582212209a159a4f3847890f10bfb87871a61eba91c5dbf5ee3cf6398207e292eee22a1664736f6c63430008070033",
	"opcodes": "PUSH1 0x80 PUSH1 0x40 MSTORE CALLVALUE DUP1 ISZERO PUSH2 0x10 JUMPI PUSH1 0x0 DUP1 REVERT JUMPDEST POP PUSH2 0x150 DUP1 PUSH2 0x20 PUSH1 0x0 CODECOPY PUSH1 0x0 RETURN INVALID PUSH1 0x80 PUSH1 0x40 MSTORE CALLVALUE DUP1 ISZERO PUSH2 0x10 JUMPI PUSH1 0x0 DUP1 REVERT JUMPDEST POP PUSH1 0x4 CALLDATASIZE LT PUSH2 0x36 JUMPI PUSH1 0x0 CALLDATALOAD PUSH1 0xE0 SHR DUP1 PUSH4 0x2E64CEC1 EQ PUSH2 0x3B JUMPI DUP1 PUSH4 0x6057361D EQ PUSH2 0x59 JUMPI JUMPDEST PUSH1 0x0 DUP1 REVERT JUMPDEST PUSH2 0x43 PUSH2 0x75 JUMP JUMPDEST PUSH1 0x40 MLOAD PUSH2 0x50 SWAP2 SWAP1 PUSH2 0xD9 JUMP JUMPDEST PUSH1 0x40 MLOAD DUP1 SWAP2 SUB SWAP1 RETURN JUMPDEST PUSH2 0x73 PUSH1 0x4 DUP1 CALLDATASIZE SUB DUP2 ADD SWAP1 PUSH2 0x6E SWAP2 SWAP1 PUSH2 0x9D JUMP JUMPDEST PUSH2 0x7E JUMP JUMPDEST STOP JUMPDEST PUSH1 0x0 DUP1 SLOAD SWAP1 POP SWAP1 JUMP JUMPDEST DUP1 PUSH1 0x0 DUP2 SWAP1 SSTORE POP POP JUMP JUMPDEST PUSH1 0x0 DUP2 CALLDATALOAD SWAP1 POP PUSH2 0x97 DUP2 PUSH2 0x103 JUMP JUMPDEST SWAP3 SWAP2 POP POP JUMP JUMPDEST PUSH1 0x0 PUSH1 0x20 DUP3 DUP5 SUB SLT ISZERO PUSH2 0xB3 JUMPI PUSH2 0xB2 PUSH2 0xFE JUMP JUMPDEST JUMPDEST PUSH1 0x0 PUSH2 0xC1 DUP5 DUP3 DUP6 ADD PUSH2 0x88 JUMP JUMPDEST SWAP2 POP POP SWAP3 SWAP2 POP POP JUMP JUMPDEST PUSH2 0xD3 DUP2 PUSH2 0xF4 JUMP JUMPDEST DUP3 MSTORE POP POP JUMP JUMPDEST PUSH1 0x0 PUSH1 0x20 DUP3 ADD SWAP1 POP PUSH2 0xEE PUSH1 0x0 DUP4 ADD DUP5 PUSH2 0xCA JUMP JUMPDEST SWAP3 SWAP2 POP POP JUMP JUMPDEST PUSH1 0x0 DUP2 SWAP1 POP SWAP2 SWAP1 POP JUMP JUMPDEST PUSH1 0x0 DUP1 REVERT JUMPDEST PUSH2 0x10C DUP2 PUSH2 0xF4 JUMP JUMPDEST DUP2 EQ PUSH2 0x117 JUMPI PUSH1 0x0 DUP1 REVERT JUMPDEST POP JUMP INVALID LOG2 PUSH5 0x6970667358 0x22 SLT KECCAK256 SWAP11 ISZERO SWAP11 0x4F CODESIZE SELFBALANCE DUP10 0xF LT 0xBF 0xB8 PUSH25 0x71A61EBA91C5DBF5EE3CF6398207E292EEE22A1664736F6C63 NUMBER STOP ADDMOD SMOD STOP CALLER ",
	"sourceMap": "199:356:0:-:0;;;;;;;;;;;;;;;;;;;"
}

```

<b> Set EVM bytecode by sending eth transaction to null destination address </b>

The standard way to deploy eth bytecode is to send eth transaction with "TO" address set to null, and with the "input" field set to the corresponding bytecode data. The resulting contract address is determined by the sender address and the nonce value

Example command:
```
python3 ./send_data_via_cleos.py 2787b98fc4e731d0456b3941f0b3fe2e01439961 "" 0 608060405234801561001057600080fd5b50610150806100206000396000f3fe608060405234801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd5b610043610075565b60405161005091906100d9565b60405180910390f35b610073600480360381019061006e919061009d565b61007e565b005b60008054905090565b8060008190555050565b60008135905061009781610103565b92915050565b6000602082840312156100b3576100b26100fe565b5b60006100c184828501610088565b91505092915050565b6100d3816100f4565b82525050565b60006020820190506100ee60008301846100ca565b92915050565b6000819050919050565b600080fd5b61010c816100f4565b811461011757600080fd5b5056fea26469706673582212209a159a4f3847890f10bfb87871a61eba91c5dbf5ee3cf6398207e292eee22a1664736f6c63430008070033 0
```
Please note that the contract address will be 3f4b0f92007341792aa61e065484e48e583ebeb9 for nonce 0
and will be 51a97d86ae7c83f050056f03ebbe451001046764 for nonce 1
At this moment please get back the contract address via 
```
./cleos get table evmevmevmevm evmevmevmevm account
```



<b>[Debug ONLY, doesn't work with eos-evm-RPC] Set EVM bytecode to EVM contract on EOSIO via debug action updatecode:</b>

Caveat: according to the EVM bytecode standard, the byte code should have the following 3 parts:
- Deploy code
- Contract code
- Auxiliary data
For more information, please refer https://medium.com/@hayeah/diving-into-the-ethereum-vm-part-5-the-smart-contract-creation-process-cb7b6133b855

Using the debug action “updatecode”, we must only update the code starting from the offset the contract code. So in the above example:
```
Deploy code:  608060405234801561001057600080fd5b50610150806100206000396000f3fe
Contract code:608060405234801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd5b610043610075565b60405161005091906100d9565b60405180910390f35b610073600480360381019061006e919061009d565b61007e565b005b60008054905090565b8060008190555050565b60008135905061009781610103565b92915050565b6000602082840312156100b3576100b26100fe565b5b60006100c184828501610088565b91505092915050565b6100d3816100f4565b82525050565b60006020820190506100ee60008301846100ca565b92915050565b6000819050919050565b600080fd5b61010c816100f4565b811461011757600080fd5b5056fea26469706673582212209a159a4f3847890f10bfb87871a61eba91c5dbf5ee3cf6398207e292eee22a1664736f6c63430008070033
```
Quite challenging (To figure out where does the actually contract code begin, I recommend to use the contract debugging function in https://remix.ethereum.org/)

Command:
```
./cleos push action evmevmevmevm updatecode '{"address":"2787b98fc4e731d0456b3941f0b3fe2e01430000","incarnation":0,"code_hash":"286e3d498e2178afc91275f11d446e62a0d85b060ce66d6ca75f6ec9d874d560","code":"608060405234801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd5b610043610075565b60405161005091906100d9565b60405180910390f35b610073600480360381019061006e919061009d565b61007e565b005b60008054905090565b8060008190555050565b60008135905061009781610103565b92915050565b6000602082840312156100b3576100b26100fe565b5b60006100c184828501610088565b91505092915050565b6100d3816100f4565b82525050565b60006020820190506100ee60008301846100ca565b92915050565b6000819050919050565b600080fd5b61010c816100f4565b811461011757600080fd5b5056fea26469706673582212209a159a4f3847890f10bfb87871a61eba91c5dbf5ee3cf6398207e292eee22a1664736f6c63430008070033"}' -p evmevmevmevm
```


<b>Verify if the ethereum contract code is deployed via EVM contract on eosio:</b>

Command: 
```
./cleos get table evmevmevmevm evmevmevmevm account
```
Example output:
```
{
  "rows": [{
      "id": 0,
      "eth_address": "2787b98fc4e731d0456b3941f0b3fe2e01439961",
      "nonce": 10,
      "balance": "00000000000000000000000000000000fffffffffffffffffff40e1d776dd79c",
      "eos_account": "",
      "code": "",
      "code_hash": "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"
    },{
      "id": 1,
      "eth_address": "0000000000000000000000000000000000000000",
      "nonce": 0,
      "balance": "000000000000000000000000000000000000000000000000000bf1e288922800",
      "eos_account": "",
      "code": "",
      "code_hash": "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"
    },{
      "id": 2,
      "eth_address": "9edf022004846bc987799d552d1b8485b317b7ed",
      "nonce": 0,
      "balance": "0000000000000000000000000000000000000000000000000000000000000064",
      "eos_account": "",
      "code": "",
      "code_hash": "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"
    },{
      "id": 3,
      "eth_address": "2787b98fc4e731d0456b3941f0b3fe2e01430000",
      "nonce": 0,
      "balance": "",
      "eos_account": "",
      "code": "608060405234801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd5b610043610075565b60405161005091906100d9565b60405180910390f35b610073600480360381019061006e919061009d565b61007e565b005b60008054905090565b8060008190555050565b60008135905061009781610103565b92915050565b6000602082840312156100b3576100b26100fe565b5b60006100c184828501610088565b91505092915050565b6100d3816100f4565b82525050565b60006020820190506100ee60008301846100ca565b92915050565b6000819050919050565b600080fd5b61010c816100f4565b811461011757600080fd5b5056fea26469706673582212209a159a4f3847890f10bfb87871a61eba91c5dbf5ee3cf6398207e292eee22a1664736f6c63430008070033",
      "code_hash": "286e3d498e2178afc91275f11d446e62a0d85b060ce66d6ca75f6ec9d874d560"
    }
  ],
  "more": false,
  "next_key": ""
}
```


<b>Execute EVM bytecode on EVM smart contract on EOSIO:</b>

Take the above solidity contract in https://remix.ethereum.org/, executing the “store(uint256 num)” function with value “123” will give you the full input data as
```
0x6057361d000000000000000000000000000000000000000000000000000000000000007b
```
please find send_data_via_cleos.py from https://github.com/eosnetworkfoundation/eos-evm/tree/kayan-rpc-fix/testing-utils


To use the script: 
```
Python3 ./send_data_via_cleos.py FROM_ACCOUNT TO_ACCOUNT AMOUNT INPUT_DATA NONCE
```
To execute an ethereum smart contract, specify TO_ACCOUNT as the contract address.

For example:
```
python3 ./send_data_via_cleos.py 2787b98fc4e731d0456b3941f0b3fe2e01439961 2787b98fc4e731d0456b3941f0b3fe2e01430000 0 6057361d000000000000000000000000000000000000000000000000000000000000007b 10

Enter private key for 2787b98fc4e731d0456b3941f0b3fe2e01439961:
act_data is {'ram_payer': 'evmevmevmevm', 'rlptx': 'f88a0a843b9aca00830186a0942787b98fc4e731d0456b3941f0b3fe2e0143000080a46057361d000000000000000000000000000000000000000000000000000000000000007b8279aaa005b8b259bdc01d0465a0086b0dc0160f50fda786d78b3fe35e7c275da087e7dfa0526b7949870d595152e9eda0c672530be7a8de668b973cf5dfc91ea88c234092'}
{
  "transaction_id": "db9abadb66809f08f14d9eca02170ca72605d38efc69ec83018541be1fd4606c",
…
        "return_value_hex_data": "0600000002000000000000000000000002000000010000000000000000000000",
        "return_value_data": {
          "account": {
            "read": 6,
            "update": 2,
            "create": 0,
            "remove": 0
          },
          "storage": {
            "read": 2,
            "update": 1,
            "create": 0,
            "remove": 0
          }
        }
      }
    ],
    "account_ram_delta": null,
    "except": null,
    "error_code": null
  }
}
```

You should able see:
   a successful eosio transaction is executed
   The “storage” in “return_value_data” should contains some non-zero number of “read” or “update” or “create”, because the execution will call “store” function


<b>Verify the actual storage in eosio blockchain via get_table_rows:</b>

Run 
```
./cleos get table evmevmevmevm evmevmevmevm account
```
Find out the table id of the ethereum smart contract storage table:

Example output (in this case the table id is 3):
```
{
      "id": 3,
      "eth_address": "2787b98fc4e731d0456b3941f0b3fe2e01430000",
      "nonce": 0,
      "balance": "",
      "eos_account": "",
      "code": "608060405234801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd5b610043610075565b60405161005091906100d9565b60405180910390f35b610073600480360381019061006e919061009d565b61007e565b005b60008054905090565b8060008190555050565b60008135905061009781610103565b92915050565b6000602082840312156100b3576100b26100fe565b5b60006100c184828501610088565b91505092915050565b6100d3816100f4565b82525050565b60006020820190506100ee60008301846100ca565b92915050565b6000819050919050565b600080fd5b61010c816100f4565b811461011757600080fd5b5056fea26469706673582212209a159a4f3847890f10bfb87871a61eba91c5dbf5ee3cf6398207e292eee22a1664736f6c63430008070033",
      "code_hash": "286e3d498e2178afc91275f11d446e62a0d85b060ce66d6ca75f6ec9d874d560"
    }
```
Run the 2nd command 
```
./cleos get table evmevmevmevm TABLE_ID storage
```
to get all the storages, for example:
```
./cleos get table evmevmevmevm 3 storage
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




# Connect eos-evm-node with eos-evm-rpc [Experimental]

## Prerequisite:
- use branch kayan-rpc-fix of this repo (https://github.com/eosnetworkfoundation/eos-evm/tree/kayan-rpc-fix)
- only use account 2787b98fc4e731d0456b3941f0b3fe2e01439961 (private key a3f1b69da92a0233ce29485d3049a4ace39e8d384bbc2557e3fc60940ce4e954) as genesis account (the balance was hacked)
- Compile Leap, CDT, EOS EVM Contract, eos-evm-node, and eos-evm-rpc binaries
- Completed the above tests

## Steps
1. From a clean database, start nodeos with SHIP
2. clean start eos-evm-node, for example:
```
./build/cmd/eos-evm-node --evm-abi ./evm.abi --chain-data ./chain-data --ship-chain-state-dir ./ship-chain-data --plugin block_conversion_plugin --plugin blockchain_plugin --nocolor 1 --verbosity=5 --ship-genesis 2
```
3. start eos-evm-rpc in the same machine, for example:
```
./build/cmd/eos-evm-rpc --eos-evm-node=127.0.0.1:8080 --chaindata=./chain-data 
```

## Make sure RPC response:
- eth_getBlockByNumber:
```
kayan-u20@kayan-u20:~/workspaces/eos-evm$ curl --location --request POST 'localhost:8881/' --header 'Content-Type: application/json' --data-raw '{"method":"eth_blockNumber","id":0}'
{"error":{"code":100,"message":"unknown bucket: SyncStage"},"id":0,"jsonrpc":"2.0"}
```
At the very beginning it is normal to see "unknown bucket: SyncStage" because there's no block


## deploy EVM bytecode to trigger the first vitual ETH block
```
python3 ./send_data_via_cleos.py 2787b98fc4e731d0456b3941f0b3fe2e01439961 "" 0 608060405234801561001057600080fd5b50610150806100206000396000f3fe608060405234801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd5b610043610075565b60405161005091906100d9565b60405180910390f35b610073600480360381019061006e919061009d565b61007e565b005b60008054905090565b8060008190555050565b60008135905061009781610103565b92915050565b6000602082840312156100b3576100b26100fe565b5b60006100c184828501610088565b91505092915050565b6100d3816100f4565b82525050565b60006020820190506100ee60008301846100ca565b92915050565b6000819050919050565b600080fd5b61010c816100f4565b811461011757600080fd5b5056fea26469706673582212209a159a4f3847890f10bfb87871a61eba91c5dbf5ee3cf6398207e292eee22a1664736f6c63430008070033 0
```

## get blocknumber again
```
kayan-u20@kayan-u20:~/workspaces/eos-evm$ curl --location --request POST 'localhost:8881/' --header 'Content-Type: application/json' --data-raw '{"method":"eth_blockNumber","id":0}'
{"id":0,"jsonrpc":"2.0","result":"0x1"}
```

## now try get block by number
```
kayan-u20@kayan-u20:~/workspaces/eos-evm$ curl --location --request POST 'localhost:8881/' --header 'Content-Type: application/json' --data-raw '{"method":"eth_getBlockByNumber","params":["0x1",true],"id":0}'
{"id":0,"jsonrpc":"2.0","result":{"difficulty":"0x","extraData":"0x","gasLimit":"0xffffffffffffffff","gasUsed":"0x0","hash":"0x62438d9e228c32a3033a961161f913b700e0d6aecf0ecb141e92ae41d1fb9845","logsBloom":"0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000","miner":"0x0000000000000000000000000000000000000000","mixHash":"0x0000000000000000000000000000000000000000000000000000000000000000","nonce":"0x0000000000000000","number":"0x1","parentHash":"0x0000000000000000000000000000000000000000000000000000000000000000","receiptsRoot":"0x0000000000000000000000000000000000000000000000000000000000000000","sha3Uncles":"0x0000000000000000000000000000000000000000000000000000000000000000","size":"0x3cc","stateRoot":"0x0000000000000000000000000000000000000000000000000000000000000000","timestamp":"0x183c5f2fea0","totalDifficulty":"0x","transactions":[{"blockHash":"0x62438d9e228c32a3033a961161f913b700e0d6aecf0ecb141e92ae41d1fb9845","blockNumber":"0x1","from":"0x2787b98fc4e731d0456b3941f0b3fe2e01439961","gas":"0xf4240","gasPrice":"0x3b9aca00","hash":"0xc4372998d1f7fc02a24fbb381947f7a10ed0826c404b7533e8431df9e48a27d0","input":"0x608060405234801561001057600080fd5b50610150806100206000396000f3fe608060405234801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd5b610043610075565b60405161005091906100d9565b60405180910390f35b610073600480360381019061006e919061009d565b61007e565b005b60008054905090565b8060008190555050565b60008135905061009781610103565b92915050565b6000602082840312156100b3576100b26100fe565b5b60006100c184828501610088565b91505092915050565b6100d3816100f4565b82525050565b60006020820190506100ee60008301846100ca565b92915050565b6000819050919050565b600080fd5b61010c816100f4565b811461011757600080fd5b5056fea26469706673582212209a159a4f3847890f10bfb87871a61eba91c5dbf5ee3cf6398207e292eee22a1664736f6c63430008070033","nonce":"0x0","r":"0x8cd1b11f5a5a9a811ad415b3f3d360a4d8aa4a8bae20467ad3649cfbad25a5ae","s":"0x5eab2829885d473747727d54caae01a8076244c3f6a4af8cad742a248b7a19ec","to":null,"transactionIndex":"0x0","type":"0x0","v":"0x79aa","value":"0x0"}],"transactionsRoot":"0x0000000000000000000000000000000000000000000000000000000000000000","uncles":[]}}
```

## now try to execute "store" function in the bytecode
```
python3 ./send_data_via_cleos.py 2787b98fc4e731d0456b3941f0b3fe2e01439961 3f4b0f92007341792aa61e065484e48e583ebeb9 0 6057361d000000000000000000000000000000000000000000000000000000000000007b 1
```

## and then execute the view action "retrieve" from RPC
```
kayan-u20@kayan-u20:~/workspaces/eos-evm$ curl --location --request POST 'localhost:8881/' --header 'Content-Type: application/json' --data-raw '{"method":"eth_call","params":[{"from":" 2787b98fc4e731d0456b3941f0b3fe2e01439961","to":"3f4b0f92007341792aa61e065484e48e583ebeb9","data":"0x2e64cec1"},"latest"],"id":11}'
{"id":11,"jsonrpc":"2.0","result":"0x000000000000000000000000000000000000000000000000000000000000007b"}
```

## try to transfer some money
```
python3 ./send_via_cleos.py 2787b98fc4e731d0456b3941f0b3fe2e01439961 0x9edf022004846bc987799d552d1b8485b317b7ed 100 2
```

## get balance via RPC
```
kayan-u20@kayan-u20:~/workspaces/eos-evm$ curl --location --request POST 'localhost:8881/' --header 'Content-Type: application/json' --data-raw '{"method":"eth_getBalance","params":["9edf022004846bc987799d552d1b8485b317b7ed","latest"],"id":0}'
{"id":0,"jsonrpc":"2.0","result":"0x100"}
```
(Note the balance of 2787b98fc4e731d0456b3941f0b3fe2e01439900 may not work, because it is hacked)


