# Deploy EVM support to public testnet

This document will describle how to enable EVM support for public testnet (such as Jungle testnet) without token economy. 
(For local testnet deployment, please refer to https://github.com/eosnetworkfoundation/TrustEVM/blob/main/docs/local_testnet_deployment_plan.md)

## For Block Producers
Block producers need to operate together to execute the following actions via multisig:

### 1. ensure the required protocol features are activated

These protocol features are required to support EVM in Antelope:
```
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


### 2. create the EVM account
Prepare an Antelope test account (Creator account)
Generate a temporary Antelope key pair:
Choose an EVM account name (such as evmevmevmevm in this document).
Run the following transaction:
```
./cleos create account CREATOR_ACCOUNT evmevmevmevm TEMP_PUBLIC_KEY TEMP_PUBLIC_KEY
```

### 3. deploy EVM contract to the EVM account
Ensure the EVM smart contract is well compiled. 
(for how to compile, please refer to https://github.com/eosnetworkfoundation/TrustEVM/blob/main/docs/compilation_and_testing_guide.md)

```
./cleos set code evmevmevmevm ../TrustEVM/contract/build/evm_runtime/evm_runtime.wasm
./cleos set abi evmevmevmevm ../TrustEVM/contract/build/evm_runtime/evm_runtime.abi
```

### 4. setup initial Eth genesis balances
In order to send ETH transaction, the sending account must have a positive balance for paying gas. Because of that we need to setup initial EVM balances for Eth genesis accounts.
for example:
```
./cleos push action evmevmevmevm setbal '{"addy":"2787b98fc4e731d0456b3941f0b3fe2e01439961", "bal":"0000000000000000000000000000000100000000000000000000000000000000"}' -p evmevmevmevm
```
Repeat this for every different genesis ETH address. 
<b>Be careful: the balance string value must be in the form of exactly 64 hex-digits (meaning a 256-bit integer)</b>


### 5. change "active" & "owner" permission to "eosio.prods":
After everything is setup, we need to give control of account "evmevmevmevm" to eosio.prods, which means we need more than 2/3 of block producers to co-sign every transaction authorized by evmevmevmevm.

```
./cleos set account permission evmevmevmevm active '{"threshold":1,"keys":[],"accounts":[{"permission":{"actor":"eosio.prods","permission":"active"},"weight":1}],"waits":[]}'

./cleos set account permission evmevmevmevm owner '{"threshold":1,"keys":[],"accounts":[{"permission":{"actor":"eosio.prods","permission":"active"},"weight":1}],"waits":[]}' -p evmevmevmevm@owner
```

Get the account again to ensure the permission is set correctly:
```
./cleos get account evmevmevmevm
```
example output:
```
permissions: 
     owner     1:    1 eosio.prods@active
        active     1:    1 eosio.prods@active
```


## For EVM service providers
Service providers will need to provide ETH compatiable EVM services as follows:
### 1. Run at least one Antelope node to sync with the public testnet, running in irreversiable mode, with state history plugin enabled
### 2. Run at least one TrustEVM-node (silkworm node) to sync with the Antelope node
### 3. Run at least one TrustEVM-RPC (silkworm rpc) process to sync with the TrustEVM-node
### 4. Have at least one Antelope account (sender account) with enough CPU/NET/RAM resource 
### 5. Run at least one Transaction Wrapper service to wrap ETH transaction into Antelope transaction and push to public testnet
### 6. Run at least one Proxy service to separate read service to TrustEVM-RPC node and write service to Transaction Wrapper.

