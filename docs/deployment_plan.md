# TrustEVM deployment plan

## Goals

We need to provide a solid deployment plan for TrustEVM, which fullfills the following goals:

- Scalability

  Operations must be scalable in such a way that it can in theory provide unlimited number of read operations per second, including execution of view actions of any deployed EVM bytecodes.
  This implies the ability to scale up to unlimited number of read EVM nodes.
  
- High availability

  Operations should not have a single point of failure. Both read & write operations are de-centralized 
  
- Compatibility

  Support RPC APIs that are compatible with Ethereum and other Ethereum like blockchains. 
  
- Deterministric 

  All EVM nodes should run exactly the same EVM byte codes and should produce the same results
  
- Performance & Latency

  No significant lagging of RPC requests, nor laggings between EOS nodes and TrustEVM nodes.
  
  
## [For service providers] Running a Trust EVM service

  In order to run a Trust EVM service, we need to have the follow items inside one physical server / VM:
  - run a local EOSIO node (nodeos process) with SHIP plugin enabled
  - run a TrustEVM-node process connecting to the local EOSIO node
  - run one or more TrustEVM-RPC process locally to service the public
  - setup a trustEVM proxy to route read requests to TrustEVM-RPC and write requests to EOSIO public network
  - prepare a eosio account with necessary CPU/NET/RAM resources for signing EVM transactions (this account can be shared to multiple EVM service)
  
### Hardware requirments
- CPU
  A server grade CPU with good single threaded performance is recommended, which is to sync-up with the public EOSIO blockchain. Check with the Leap repo for detailed info.
- RAM
  64GB+ for EOS public blockchain, 128GB+ for WAX. The size of the RAM should be more than enough to hold the full EOSIO node and TrustEVM node. ECC RAM is required. 
  More RAM are needed if the chain has more state data.
- SSD
  A big SSD is required for storing all history (blocks + State History). Recommend 2TB+ at the beginning and the size may probably go up in the future.
- Network
  A low latency network that can catch up the EOS blockchain in time is required.
  
### configuration and command parameters:
- nodeos:
- TrustEVM-node:
- TrustEVM-RPC




## [For block producers] Bootstrapping protocol features, Deploying EVM contracts, Setup genesis/initial EVM virtual accounts & tokens

### Protocol features required to activate:
- ACTION_RETURN_VALUE
- CRYPTO_PRIMITIVES
- GET_BLOCK_NUM

### Create main EVM account & Deploy EVM contract to EOSIO blockchain


### Setup EVM token


### disturbute EVM tokens according to genesis


