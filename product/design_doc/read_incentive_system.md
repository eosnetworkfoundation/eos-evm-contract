# Read Incentive System Design

## Requirements

- The proxy should direct traffic to upstream in a weighted manner.
- The upstream and weight should be updated on the fly according to some on-chain data.
- Payment need to be calculated from access log periodically and save to the chain.

## General Design

We consider using Nginx as the core of the proxy. The reason for that is:

1. Nginx is widely used proxy software
2. Nginx with upsync module can easily meet our need
3. It might be hard to develop an efficient and reliable proxy in such short time

![Figure 1: General Design](https://github.com/eosnetworkfoundation/TrustEVM/blob/yarkinwho-product-desc-update/product/design_doc/proxy%20design.drawio.png)

Figure 1: General Design

The system can be divided into several modules:

1 The proxy:

An Nginx system configured to serve traffic using a weighted round robin algorithm with the ability to sync upstream address and weight on the fly.

2 Upstream helper

A small service read the staking info from the staking contract on TrustEVM and translate them into upstream and weight for Nginx to sync from. It can either directly allow Nginx to query

3 Workload verifier

A small service read the access log of Nginx and calculate the income for each upstream. It will write the result to the staking contract on TrustEVM.

4 Staking contract on TrustEVM

A Contract on TrustEVM that RPC providers can stake and put endpoint info on.

## Module Design

### The proxy

The current design is simply using Nginx with upsync module.

https://github.com/weibocom/nginx-upsync-module

The upsync module can dynamically update upstream info from external source without reloading the config. We can let the nginx to sync upstream list from either the Upstream helper directly or use some storage such as redis or consul for it.

![Figure 2: Design with some storage service](https://github.com/eosnetworkfoundation/TrustEVM/blob/yarkinwho-product-desc-update/product/design_doc/Copy%20of%20proxy%20design.drawio.png)

Figure 2: Design with some storage service

Reliability and Performance:

This module should be designed with enough concerns regarding reliablity and performance.
- Proxy needs high availability design
- Access log needs backup

### Upstream helper

The helper that read the contract for staking info and translate it to nginx upstream info.

It then either allow nginx to directly read from it or save the info to some storage service such as redis and consul.

A tiny python/nodejs can do the work nicely. If we want to write to storage instead of serving http requests directly, even a bash script with curl might be enough.

This module should be developed together with the proxy.

Reliability and Performance:

This module should have relatively low reliability and performance requirement.

Dependency:

Staking contract API

### Workload verifier

The verifier will periodically check the access log, count the traffic for each operator and save the result to the staking contract.

The access log should be text files in standard nginx format  with $upstream_addr configured in log format.

A simple python/nodejs can do the job nicely. C++ is fine if figure out a good lib to use.

If we want to distinct read/write, the Nginx have to look into the request and that will introduce some overhead.


Reliability and Performance:

This module should have relatively low reliability and performance requirement.

Dependency:

Staking contract API

### Staking contract

The contract must have the following features:

1. Operator can stake EVM
2. Operator can set upstream address (optionally separate read/write) when staking
3. Allow list to let admin set operator/workload verifier
4. Workload verifier can update the payment
5. Operator can claim payment
6. The contract can accept EVM tokens as the source of payment

It should be a standard Ethereum contract in solidity as simple as possible.


Reliability and Performance:

This module should have relatively low reliability and performance requirement.

The API should be defined first so other modules can start working.

## Important issues
- Should the operator be paid according to what they have done (calculated from access log), or be paid according to staking shares only while access log is used for monitor the activity. Foundation can punish malicious oprators according to the log.
  - We can go with the second option in the begining and add the functionality later
- Should we distinct read and write when recording access
  - Consensus is NO. The gain is too little.
- Should we use a kv storage in upstream helper
  - Yes.
- How our current proxy based read-incentive system deal with performance problems? e.g. what happen if the proxy is in us and the upstream node is in asia?
  - We will go with single proxy in the begining.
  - We can add more in different region
  - We can add more complicated mechanism later
