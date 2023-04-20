# Enable EVM Support For Public Testnet


This document describes how to enable EVM support for public testnets, such as Jungle testnet, without token economy.
For local testnet deployments, refer to the [Enable EVM Support For Local Testnet](https://github.com/eosnetworkfoundation/eos-evm/blob/main/docs/local_testnet_deployment_plan.md) guide.

To enable EVM support, the following procedures must be performed by:
- [Block Producers](#for-block-producers)
- [EOS EVM team](#for-eos-evm-team)
- [EVM Service Providers](#for-evm-service-providers)

## For Block Producers

Block producers need to operate together to execute the following actions via multisig:

### 1. Activate Protocol Features

The following protocol features are required to support EVM in Antelope:
```json
[
  {
    "feature_digest": "0ec7e080177b2c02b278d5088611686b49d739925a92d9bfcacd7fc6b74053bd",
    "subjective_restrictions": {
      "enabled": true,
      "preactivation_required": false,
      "earliest_allowed_activation_time": "1970-01-01T00:00:00.000"
    },
    "description_digest": "64fe7df32e9b86be2b296b3f81dfd527f84e82b98e363bc97e40bc7a83733310",
    "dependencies": [],
    "protocol_feature_type": "builtin",
    "specification": [
      {
        "name": "builtin_feature_codename",
        "value": "PREACTIVATE_FEATURE"
      }
    ]
  },
  {
    "feature_digest": "1a99a59d87e06e09ec5b028a9cbb7749b4a5ad8819004365d02dc4379a8b7241",
    "subjective_restrictions": {
      "enabled": true,
      "preactivation_required": true,
      "earliest_allowed_activation_time": "1970-01-01T00:00:00.000"
    },
    "description_digest": "f3c3d91c4603cde2397268bfed4e662465293aab10cd9416db0d442b8cec2949",
    "dependencies": [],
    "protocol_feature_type": "builtin",
    "specification": [
      {
        "name": "builtin_feature_codename",
        "value": "ONLY_LINK_TO_EXISTING_PERMISSION"
      }
    ]
  },
  {
    "feature_digest": "2652f5f96006294109b3dd0bbde63693f55324af452b799ee137a81a905eed25",
    "subjective_restrictions": {
      "enabled": true,
      "preactivation_required": true,
      "earliest_allowed_activation_time": "1970-01-01T00:00:00.000"
    },
    "description_digest": "898082c59f921d0042e581f00a59d5ceb8be6f1d9c7a45b6f07c0e26eaee0222",
    "dependencies": [],
    "protocol_feature_type": "builtin",
    "specification": [
      {
        "name": "builtin_feature_codename",
        "value": "FORWARD_SETCODE"
      }
    ]
  },
  {
    "feature_digest": "299dcb6af692324b899b39f16d5a530a33062804e41f09dc97e9f156b4476707",
    "subjective_restrictions": {
      "enabled": true,
      "preactivation_required": true,
      "earliest_allowed_activation_time": "1970-01-01T00:00:00.000"
    },
    "description_digest": "ab76031cad7a457f4fd5f5fca97a3f03b8a635278e0416f77dcc91eb99a48e10",
    "dependencies": [],
    "protocol_feature_type": "builtin",
    "specification": [
      {
        "name": "builtin_feature_codename",
        "value": "WTMSIG_BLOCK_SIGNATURES"
      }
    ]
  },
  {
    "feature_digest": "35c2186cc36f7bb4aeaf4487b36e57039ccf45a9136aa856a5d569ecca55ef2b",
    "subjective_restrictions": {
      "enabled": true,
      "preactivation_required": true,
      "earliest_allowed_activation_time": "1970-01-01T00:00:00.000"
    },
    "description_digest": "e5d7992006e628a38c5e6c28dd55ff5e57ea682079bf41fef9b3cced0f46b491",
    "dependencies": [],
    "protocol_feature_type": "builtin",
    "specification": [
      {
        "name": "builtin_feature_codename",
        "value": "GET_BLOCK_NUM"
      }
    ]
  },
  {
    "feature_digest": "ef43112c6543b88db2283a2e077278c315ae2c84719a8b25f25cc88565fbea99",
    "subjective_restrictions": {
      "enabled": true,
      "preactivation_required": true,
      "earliest_allowed_activation_time": "1970-01-01T00:00:00.000"
    },
    "description_digest": "9908b3f8413c8474ab2a6be149d3f4f6d0421d37886033f27d4759c47a26d944",
    "dependencies": [],
    "protocol_feature_type": "builtin",
    "specification": [
      {
        "name": "builtin_feature_codename",
        "value": "REPLACE_DEFERRED"
      }
    ]
  },
  {
    "feature_digest": "4a90c00d55454dc5b059055ca213579c6ea856967712a56017487886a4d4cc0f",
    "subjective_restrictions": {
      "enabled": true,
      "preactivation_required": true,
      "earliest_allowed_activation_time": "1970-01-01T00:00:00.000"
    },
    "description_digest": "45967387ee92da70171efd9fefd1ca8061b5efe6f124d269cd2468b47f1575a0",
    "dependencies": [
      "ef43112c6543b88db2283a2e077278c315ae2c84719a8b25f25cc88565fbea99"
    ],
    "protocol_feature_type": "builtin",
    "specification": [
      {
        "name": "builtin_feature_codename",
        "value": "NO_DUPLICATE_DEFERRED_ID"
      }
    ]
  },
  {
    "feature_digest": "4e7bf348da00a945489b2a681749eb56f5de00b900014e137ddae39f48f69d67",
    "subjective_restrictions": {
      "enabled": true,
      "preactivation_required": true,
      "earliest_allowed_activation_time": "1970-01-01T00:00:00.000"
    },
    "description_digest": "1812fdb5096fd854a4958eb9d53b43219d114de0e858ce00255bd46569ad2c68",
    "dependencies": [],
    "protocol_feature_type": "builtin",
    "specification": [
      {
        "name": "builtin_feature_codename",
        "value": "RAM_RESTRICTIONS"
      }
    ]
  },
  {
    "feature_digest": "4fca8bd82bbd181e714e283f83e1b45d95ca5af40fb89ad3977b653c448f78c2",
    "subjective_restrictions": {
      "enabled": true,
      "preactivation_required": true,
      "earliest_allowed_activation_time": "1970-01-01T00:00:00.000"
    },
    "description_digest": "927fdf78c51e77a899f2db938249fb1f8bb38f4e43d9c1f75b190492080cbc34",
    "dependencies": [],
    "protocol_feature_type": "builtin",
    "specification": [
      {
        "name": "builtin_feature_codename",
        "value": "WEBAUTHN_KEY"
      }
    ]
  },
  {
    "feature_digest": "5443fcf88330c586bc0e5f3dee10e7f63c76c00249c87fe4fbf7f38c082006b4",
    "subjective_restrictions": {
      "enabled": true,
      "preactivation_required": true,
      "earliest_allowed_activation_time": "1970-01-01T00:00:00.000"
    },
    "description_digest": "70787548dcea1a2c52c913a37f74ce99e6caae79110d7ca7b859936a0075b314",
    "dependencies": [],
    "protocol_feature_type": "builtin",
    "specification": [
      {
        "name": "builtin_feature_codename",
        "value": "BLOCKCHAIN_PARAMETERS"
      }
    ]
  },
  {
    "feature_digest": "68dcaa34c0517d19666e6b33add67351d8c5f69e999ca1e37931bc410a297428",
    "subjective_restrictions": {
      "enabled": true,
      "preactivation_required": true,
      "earliest_allowed_activation_time": "1970-01-01T00:00:00.000"
    },
    "description_digest": "2853617cec3eabd41881eb48882e6fc5e81a0db917d375057864b3befbe29acd",
    "dependencies": [],
    "protocol_feature_type": "builtin",
    "specification": [
      {
        "name": "builtin_feature_codename",
        "value": "DISALLOW_EMPTY_PRODUCER_SCHEDULE"
      }
    ]
  },
  {
    "feature_digest": "6bcb40a24e49c26d0a60513b6aeb8551d264e4717f306b81a37a5afb3b47cedc",
    "subjective_restrictions": {
      "enabled": true,
      "preactivation_required": true,
      "earliest_allowed_activation_time": "1970-01-01T00:00:00.000"
    },
    "description_digest": "68d6405cb8df3de95bd834ebb408196578500a9f818ff62ccc68f60b932f7d82",
    "dependencies": [],
    "protocol_feature_type": "builtin",
    "specification": [
      {
        "name": "builtin_feature_codename",
        "value": "CRYPTO_PRIMITIVES"
      }
    ]
  },
  {
    "feature_digest": "8ba52fe7a3956c5cd3a656a3174b931d3bb2abb45578befc59f283ecd816a405",
    "subjective_restrictions": {
      "enabled": true,
      "preactivation_required": true,
      "earliest_allowed_activation_time": "1970-01-01T00:00:00.000"
    },
    "description_digest": "2f1f13e291c79da5a2bbad259ed7c1f2d34f697ea460b14b565ac33b063b73e2",
    "dependencies": [],
    "protocol_feature_type": "builtin",
    "specification": [
      {
        "name": "builtin_feature_codename",
        "value": "ONLY_BILL_FIRST_AUTHORIZER"
      }
    ]
  },
  {
    "feature_digest": "ad9e3d8f650687709fd68f4b90b41f7d825a365b02c23a636cef88ac2ac00c43",
    "subjective_restrictions": {
      "enabled": true,
      "preactivation_required": true,
      "earliest_allowed_activation_time": "1970-01-01T00:00:00.000"
    },
    "description_digest": "e71b6712188391994c78d8c722c1d42c477cf091e5601b5cf1befd05721a57f3",
    "dependencies": [],
    "protocol_feature_type": "builtin",
    "specification": [
      {
        "name": "builtin_feature_codename",
        "value": "RESTRICT_ACTION_TO_SELF"
      }
    ]
  },
  {
    "feature_digest": "bcd2a26394b36614fd4894241d3c451ab0f6fd110958c3423073621a70826e99",
    "subjective_restrictions": {
      "enabled": true,
      "preactivation_required": true,
      "earliest_allowed_activation_time": "1970-01-01T00:00:00.000"
    },
    "description_digest": "d2596697fed14a0840013647b99045022ae6a885089f35a7e78da7a43ad76ed4",
    "dependencies": [],
    "protocol_feature_type": "builtin",
    "specification": [
      {
        "name": "builtin_feature_codename",
        "value": "GET_CODE_HASH"
      }
    ]
  },
  {
    "feature_digest": "c3a6138c5061cf291310887c0b5c71fcaffeab90d5deb50d3b9e687cead45071",
    "subjective_restrictions": {
      "enabled": true,
      "preactivation_required": true,
      "earliest_allowed_activation_time": "1970-01-01T00:00:00.000"
    },
    "description_digest": "69b064c5178e2738e144ed6caa9349a3995370d78db29e494b3126ebd9111966",
    "dependencies": [],
    "protocol_feature_type": "builtin",
    "specification": [
      {
        "name": "builtin_feature_codename",
        "value": "ACTION_RETURN_VALUE"
      }
    ]
  },
  {
    "feature_digest": "d528b9f6e9693f45ed277af93474fd473ce7d831dae2180cca35d907bd10cb40",
    "subjective_restrictions": {
      "enabled": true,
      "preactivation_required": true,
      "earliest_allowed_activation_time": "1970-01-01T00:00:00.000"
    },
    "description_digest": "8139e99247b87f18ef7eae99f07f00ea3adf39ed53f4d2da3f44e6aa0bfd7c62",
    "dependencies": [],
    "protocol_feature_type": "builtin",
    "specification": [
      {
        "name": "builtin_feature_codename",
        "value": "CONFIGURABLE_WASM_LIMITS2"
      }
    ]
  },
  {
    "feature_digest": "e0fb64b1085cc5538970158d05a009c24e276fb94e1a0bf6a528b48fbc4ff526",
    "subjective_restrictions": {
      "enabled": true,
      "preactivation_required": true,
      "earliest_allowed_activation_time": "1970-01-01T00:00:00.000"
    },
    "description_digest": "a98241c83511dc86c857221b9372b4aa7cea3aaebc567a48604e1d3db3557050",
    "dependencies": [],
    "protocol_feature_type": "builtin",
    "specification": [
      {
        "name": "builtin_feature_codename",
        "value": "FIX_LINKAUTH_RESTRICTION"
      }
    ]
  },
  {
    "feature_digest": "f0af56d2c5a48d60a4a5b5c903edfb7db3a736a94ed589d0b797df33ff9d3e1d",
    "subjective_restrictions": {
      "enabled": true,
      "preactivation_required": true,
      "earliest_allowed_activation_time": "1970-01-01T00:00:00.000"
    },
    "description_digest": "1eab748b95a2e6f4d7cb42065bdee5566af8efddf01a55a0a8d831b823f8828a",
    "dependencies": [],
    "protocol_feature_type": "builtin",
    "specification": [
      {
        "name": "builtin_feature_codename",
        "value": "GET_SENDER"
      }
    ]
  }
]
```

## For EOS EVM Team

The EOS EVM Team must perform the following steps:

### 1. Create The EVM Account

- Prepare an Antelope test account, a.k.a. CREATOR_ACCOUNT, which will be used to create the EVM account
- Generate a temporary Antelope key pair
- Choose an EVM account name, such as `evmevmevmevm` in this document
- Run the following transaction:

```sh
./cleos create account CREATOR_ACCOUNT evmevmevmevm TEMP_PUBLIC_KEY TEMP_PUBLIC_KEY
```

See the [cleos create account](https://docs.eosnetwork.com/leap/latest/cleos/command-reference/create/account) reference for details on how to create an account.

See the [cleos create key pair](https://docs.eosnetwork.com/leap/latest/cleos/command-reference/create/key) reference for details on how to create a key pair.

### 2. Deploy The Debug EVM Contract

Later in this procedure you must use the `setbal` smart contract action to set the balance of the inital account. This action is available only in the `debug` version of the smart contract.

Compile the `debug` version of the EVM smart contract and note the wasm and abi path, e.g. `EVM_DEBUG_PATH`.

For details on how to compile the EVM smart contract see the [Compilation And Testing Guide](https://github.com/eosnetworkfoundation/eos-evm/blob/main/docs/compilation_and_testing_guide.md).

Run the following `cleos` commands to deploy the `debug` version of the EVM contract to the EVM account:

```sh
./cleos set code evmevmevmevm EVM_DEBUG_PATH/evm_runtime.wasm
./cleos set abi evmevmevmevm EVM_DEBUG_PATH/evm_runtime.abi
```

### 2a. Initialize EVM contract
The EVM contract will not allow any actions except `init` until its chain id & native token is configured. Exact values to use here are TBD.
```
./cleos push action evmevmevmevm init '{"chainid": 15555}'
```

### 3. Setup The Initial EVM Token Balance

We need to set the EVM token balance for the initial ETH account, which is specially managed by EOS EVM team.

For example:
```sh
./cleos push action evmevmevmevm setbal '{"addy":"2787b98fc4e731d0456b3941f0b3fe2e01439961", "bal":"0000000000000000000000000000000100000000000000000000000000000000"}' -p evmevmevmevm
```

> :warning: Be careful: the balance string value must be in the form of exactly 64 hex-digits (meaning a 256-bit integer)


### 4. Deploy The Release EVM Contract

Compile the `release` version of the EVM smart contract and note the wasm and abi path, e.g. `EVM_RELEASE_PATH`.

For details on how to compile the EVM smart contract see the [Compilation And Testing Guide](https://github.com/eosnetworkfoundation/eos-evm/blob/main/docs/compilation_and_testing_guide.md).

Run the following `cleos` commands to deploy the `release` version of the EVM contract to the EVM account:

```sh
./cleos set code evmevmevmevm EVM_RELEASE_PATH/evm_runtime.wasm
./cleos set abi evmevmevmevm EVM_RELEASE_PATH/evm_runtime.abi
```

### 5. Initialize The Participant Accounts Balances

Send EVM tokens from the initial account to participant accounts.

Use the token distribution script, [distribute_to_accounts.py](https://github.com/eosnetworkfoundation/eos-evm/blob/main/peripherals/token_distribution/distribute_to_accounts.py), to distribute tokens to the initial EVM accounts:

#### Prepare The Account Balance CSV File

You need to list all the account balances in a `.csv` file, without the header row, where the first column represents the account name and the second column respresents the balance number in decimal, such as:

```
0x00000000219ab540356cbb839cbe05303d7705fa,14708999007718564869804029
0xc02aaa39b223fe8d0a0e5c4f27ead9083c756cc2,3930560351933256293096325
0xf977814e90da44bfa03b6295a0616a897441acec,2604596263728749700480557
0xda9dfa130df4de4673b89022ee50ff26f6ea73cf,2113030086367224616200000
0x0716a17fbaee714f1e6ab0f9d59edbc5f09815c0,1998606574289842563751000
0xbe0eb53f46cd790cd13851d5eff43d12404d33e8,1996008352588563830743490
0x742d35cc6634c0532925a3b844bc454e4438f44e,1383424850937541446672785
```

#### Prepare Enviroment Variables

Have your EVM private key of the EVM sender account, and the `nodeos`'s RPC endpoint defined in the corresponding enviroment variables, as in the following example:

```sh
export EVM_SENDER_KEY=a3f1b69da92a0233ce29485d3049a4ace39e8d384bbc2557e3fc60940ce4e954
export NODEOS_ENDPOINT=http://127.0.0.1:8888
```

#### Find Out The Starting Nonce Number

Find out the starting nonce number and the current balance of the sender account. Use the following command to find the starting nonce number of any existing EVM account:

```sh
./cleos get table evmevmevmevm evmevmevmevm account --index 2 --key-type sha256 --limit 1 -L EVM_ACCOUNT_NAME
```

For example:
```sh
./cleos get table evmevmevmevm evmevmevmevm account --index 2 --key-type sha256 --limit 1 -L 2787b98fc4e731d0456b3941f0b3fe2e01439961
```
```json
{
  "rows": [{
      "id": 0,
      "eth_address": "2787b98fc4e731d0456b3941f0b3fe2e01439961",
      "nonce": 1005,
      "balance": "00000000000000000000000000000000ffffffffffbee2eeb107b6ea020924bd",
      "eos_account": "",
      "code": "",
      "code_hash": "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"
    }
  ],
  "more": true,
  "next_key": "27ca5d05bb99a31c1cb793c8679f605ea58c1dd3000000000000000000000000"
}
```

Ensure the sender account has enough balance to distribute to all the accounts in the distribution list.

##### Locate the starting_nonce variable

Open token distribution script, [distribute_to_accounts.py](https://github.com/eosnetworkfoundation/eos-evm/blob/main/peripherals/token_distribution/distribute_to_accounts.py), in your editor and search for the `starting_nonce` variable. It should look similar to the code lines below: 

```python
# staring_nonce is the nonce number that maps to the transfer of the first account in the list
# it is used to ensure each transfer is idempotent
# be careful when you change it
starting_nonce = 3
```

##### Update the starting_nonce variable

In the token distribution script, [distribute_to_accounts.py](https://github.com/eosnetworkfoundation/eos-evm/blob/main/peripherals/token_distribution/distribute_to_accounts.py), update the `starting_nonce` value to match your current sender account's nonce number. The nonce number is to mark the progress of distribution. You should not change the `starting_nonce` once it is correctly set. Save the script.

#### Prepare Account And Keys

Prepare your Antelope wrapping account, such as `evmevmevmevm`, and import your private key to your local Antelope wallet (managed by `keosd`).

For example:
```sh
./cleos wallet create -n w123 --to-console
./cleos wallet import -n w123 --private-key 5JURSKS1BrJ1TagNBw1uVSzTQL2m9eHGkjknWeZkjSt33Awtior
```

#### Verify Other Parameters In The Script

In the token distribution script, [distribute_to_accounts.py](https://github.com/eosnetworkfoundation/eos-evm/blob/main/peripherals/token_distribution/distribute_to_accounts.py), these parameters should read as follows:
```python
EVM_CONTRACT    = os.getenv("EVM_CONTRACT", "evmevmevmevm")
NODEOS_ENDPOINT = os.getenv("NODEOS_ENDPOINT", "http://127.0.0.1:8888")
EOS_SENDER      = os.getenv("EOS_SENDER", "evmevmevmevm")
EVM_SENDER_KEY  = os.getenv("EVM_SENDER_KEY", None)
EOS_SENDER_KEY  = os.getenv("EOS_SENDER_KEY", None)
EVM_CHAINID     = int(os.getenv("EVM_CHAINID", "15555"))
```

#### Run The Distribution Script

Command syntax:
```sh
python3 ./distribute_to_accounts.py FROM_ACCOUNT DISTRIBUTION_CSV
```

For example:
```sh
export EVM_SENDER_KEY=a3f1b69da92a0233ce29485d3049a4ace39e8d384bbc2557e3fc60940ce4e954
export NODEOS_ENDPOINT=http://127.0.0.1:8888
python3 ./distribute_to_accounts.py 2787b98fc4e731d0456b3941f0b3fe2e01439961 ~/Downloads/eth_acc_bals_100k.csv
```

The script will sign the transactions and call `cleos` to push the transactions to the `NODEOS_ENDPOINT`.

> :information_source: You can stop and restart the script from time to time and it can continue from the last point of distribution. But do not forget that the `account` and the `starting_nonce` in your script must not be changed.

#### Final Verification

For the final verification, check the nonce number again after the entire script is finished. 

```sh
./cleos get table evmevmevmevm evmevmevmevm account --index 2 --key-type sha256 --limit 1 -L EVM_ACCOUNT_NAME
```
Make sure the current nonce number has been increased by x, where x is the number of accounts in the distribution list.


## For EVM Service Providers

This part is very similar to the [Enable EVM Support For Local Testnet](https://github.com/eosnetworkfoundation/eos-evm/blob/main/docs/local_testnet_deployment_plan.md) guide.
EVM service providers will need to provide ETH compatible EVM services as follows:

### 1. Run an Antelope Node

Run at least one Antelope node to sync with the public testnet, running in irreversible mode, with `state_history_plugin` enabled.

Example command:
```sh
./build/programs/nodeos/nodeos --data-dir=./data-dir  --config-dir=./data-dir --genesis-json=./data-dir/genesis.json --disable-replay-opts --read-mode=irreversible
```

### 2. Run eos-evm-node

Run at least one eos-evm-node, a.k.a. silkworm node, to sync with the Antelope node.

Refer to the *Start up eos-evm-node (silkworm node)* section in the [Enable EVM For Local Testnet](https://github.com/eosnetworkfoundation/eos-evm/blob/main/docs/local_testnet_deployment_plan.md#5-start-up-eos-evm-node-silkworm-node) guide for more details.

```sh
./build/cmd/eos-evm-node --chain-data ./chain-data  --plugin block_conversion_plugin --plugin blockchain_plugin --nocolor 1 --verbosity=5
```

### 3. Run eos-evm-rpc

Run at least one eos-evm-rpc, a.k.a. silkworm rpc, process to sync with the eos-evm-node.
The eos-evm-rpc must be deployed on the same machine with eos-evm-node, as it needs to access the same chain-data folder.

```sh
./build/cmd/eos-evm-rpc --eos-evm-node=127.0.0.1:8080 --chaindata=./chain-data 
```

Refer to the *Start up eos-evm-rpc (silkworm RPC)* section in the [Enable EVM For Local Testnet](https://github.com/eosnetworkfoundation/eos-evm/blob/main/docs/local_testnet_deployment_plan.md#6-start-up-eos-evm-rpc-silkworm-rpc) guide for more RPC setup details.

### 4. Ensure Enough Resources

You must have at least one Antelope account, the sender account, with enough CPU/NET/RAM resources. The service providers can create one or more testnet accounts with some CPU, NET, and RAM resources.

### 5. Run a Transaction Wrapper Service

Run at least one Transaction Wrapper service to wrap ETH transactions into Antelope transactions and push to public testnet. Prepare the `.env` file to configure the Antelope RPC endpoint, listening port, EVM contract account, sender account, and the rest of the settings.

For example:

```conf
EOS_RPC="http://127.0.0.1:8888"
EOS_KEY="5JURSKS1BrJ1TagNBw1uVSzTQL2m9eHGkjknWeZkjSt33Awtior"
HOST="127.0.0.1"
PORT="18888"
EOS_EVM_ACCOUNT="evmevmevmevm"
EOS_SENDER="a123"
```

In the above environment settings, the Transaction Wrapper is set to:

- Listen to `127.0.0.1:18888` Antelope endpoint
- Use the `5JURSKS1BrJ1TagNBw1uVSzTQL2m9eHGkjknWeZkjSt33Awtior` key to wrap and sign the incoming ETH trasnactions into Antelope transactions and push them into the Antelope RPC endpoint `http://127.0.0.1:8888`

Use the `index.js` file from https://github.com/eosnetworkfoundation/eos-evm/tree/main/peripherals/tx_wrapper:

```sh
node index.js
```

### 6. Run a Proxy Service

Run at least one Proxy service to separate the read service to eos-evm-rpc node and the write service to Transaction Wrapper.

Refer to the *Setup proxy to separate read requests and write requests* section in the [Enable EVM For Local Testnet](https://github.com/eosnetworkfoundation/eos-evm/blob/main/docs/local_testnet_deployment_plan.md#7-setup-proxy-to-separate-read-requests-and-write-requests) guide for details.

## RPC Provider Architecture

```
         |                                                 [2]
         |                     WRITE       +-----------------+
         |             +------------------>|   TX-Wrapper    |
         |             |                   +-------v---------+
         |             |                   |    Nodeos       |
         |             |   [1]             +-------+---------+
 request |       +-----+-----+                     |
---------+------>|   Proxy   |                     |
         |       +-----------+                     v       [3]
         |             |                   +-----------------+
         |             |                   |                 |
         |             +------------------>|  EOS EVM Node   +
         |                     READ        |                 |
         |                                 +-----------------+
```

## Hardware Requirements

### Proxy

- 32GB
- AMD Ryzen 5 3600
- 512GB NVMe

### EOS Node / TX-Wrapper

- 64GB
- AMD Ryzen 9 5950X (*or other CPU with good single threaded performance*)
- 4TB NVMe

### eos-evm-node

- 64 GB
- AMD Ryzen 9 5950X (*or other CPU with good single threaded performance*)
- 4TB NVMe