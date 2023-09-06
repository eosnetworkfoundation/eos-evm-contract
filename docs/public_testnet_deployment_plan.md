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

### 2. Deploy EVM Contract

For details on how to compile the EVM smart contract see the [Compilation And Testing Guide](https://github.com/eosnetworkfoundation/eos-evm/blob/main/docs/compilation_and_testing_guide.md).

Run the following `cleos` commands to EVM contract to the EVM account:

```sh
./cleos set code evmevmevmevm EVM_PATH_to_evm_runtime.wasm
./cleos set abi evmevmevmevm EVM_PATH_to_evm_runtime.abi
```

### 2a. Initialize EVM contract
The EVM contract will not allow any actions except `init` until its chain id & native token is configured. Exact values to use here are TBD.
```
./cleos push action evmevmevmevm init "{\"chainid\":$EVM_CHAINID,\"fee_params\":{\"gas_price\":150000000000,\"miner_cut\":10000,\"ingress_bridge_fee\":\"0.0100 EOS\"}}" -x 60 -p evmevmevmevm
```

add eosio.code to active permission
```
./cleos set account permission evmevmevmevm active --add-code
```

transfer initial balance
```
./cleos transfer eosio evmevmevmevm "1.0000 EOS" "evmevmevmevm"
```

## For EVM Service Providers

This part is very similar to the [Enable EVM Support For Local Testnet](https://github.com/eosnetworkfoundation/eos-evm/blob/main/docs/local_testnet_deployment_plan.md) guide.
EVM service providers will need to provide ETH compatible EVM services as follows:

### 1. Run an Antelope Node

Run at least one Antelope node to sync with the public testnet, running in irreversible mode, with `state_history_plugin` enabled.

Example command:
```sh
./build/programs/nodeos/nodeos --data-dir=./data-dir  --config-dir=./data-dir --genesis-json=./data-dir/genesis.json --disable-replay-opts
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

You must have at least one Antelope account, the sender account, with enough CPU/NET/RAM resources. The service providers can create one or more testnet accounts with some CPU, NET, and RAM resources. We use account a123 as example:

open sender account balance 
```
./cleos push action evmevmevmevm open '{"owner":"a123"}' -p a123
```

### 5. Run a Transaction Wrapper Service

Run at least one Transaction Wrapper service to wrap ETH transactions into Antelope transactions and push to public testnet. Prepare the `.env` file to configure the Antelope RPC endpoint, listening port, EVM contract account, sender account, and the rest of the settings.

For example:

```conf
# one or more EOS_RPC endpoints, separated by '|'
EOS_RPC="http://127.0.0.1:8888|http://192.168.1.1:8888"
EOS_KEY="5JURSKS1BrJ1TagNBw1uVSzTQL2m9eHGkjknWeZkjSt33Awtior"

# the listening IP & port of this service
HOST="127.0.0.1"
PORT="18888"
EOS_PERMISSION="active"
EXPIRE_SEC=60
EOS_EVM_ACCOUNT="evmevmevm"
EOS_SENDER="a123"
```

In the above environment settings, the Transaction Wrapper is set to:

- Listen to `127.0.0.1:18888`
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
