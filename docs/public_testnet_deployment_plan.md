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

