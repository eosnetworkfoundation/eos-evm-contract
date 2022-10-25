# Deploy EVM support to public testnet

This document will describle how to enable EVM support for public testnet (such as Jungle testnet) without token economy.

## For Block Producers
Block producers need to operate together to execute the following actions via multisig:
- ensure the required protocol features are activated
- create the EVM account
- deploy EVM contract to the EVM account
- setup initial EVM balance (EVM genesis)

## For EVM service providers
Service providers will need to provide ETH compatiable EVM services as follows:
- Run at least one Antelope node to sync with the public testnet, running in irreversiable mode, with state history plugin enabled
- Run at least one TrustEVM-node (silkworm node) to sync with the Antelope node
- Run at least one TrustEVM-RPC (silkworm rpc) process to sync with the TrustEVM-node
- Have at least one Antelope account (sender account) with enough CPU/NET/RAM resource 
- Run at least one Transaction Wrapper service to wrap ETH transaction into Antelope transaction and push to public testnet
- Run at least one Proxy service to separate read service to TrustEVM-RPC node and write service to Transaction Wrapper.

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
         |             +------------------>|  TrustEVM Node  +
         |                     READ        |                 |
         |                                 +-----------------+
```

## Hardware requirments

#### [1] Proxy:
- 32GB
- AMD Ryzen 5 3600
- 512GB NVMe

#### [2] EOS Node / TX-Wrapper:
- 64GB
- AMD Ryzen 9 5950X (*or other CPU with good single threaded performance*)
- 4TB NVMe

#### [3] TrustEVM Node:
- 64 GB
- AMD Ryzen 9 5950X (*or other CPU with good single threaded performance*)
- 4TB NVMe
