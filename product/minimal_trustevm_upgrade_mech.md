[200~---
tags: Proposal
---

# Minimal TrustEVM Upgrade Mechanism

## Prerequisites
#### Dependencies: N/A

## Problem

#### Opportunity:
Before we launch this service if we don't have in place some of the core tables and actions and use those to at the very least determine that version in TrustEVM node != TrustEVM contract then the first group then we cannot just add it later as the nodes will not have the knowledge of when to stop.

#### Target audience:
- API node operators

#### Strategic alignment:
It will allow for safe usage of our node software for API node providers.

## Solution

#### Solution name:
Minimal TrustEVM Upgrade Mechanism
#### Purpose: 
Define and implement the minimal mechanism and interface for future upgrades.
#### Success definition: 
For TrustEVM node to shutdown gracefully if the on chain contract version does not match what it expects.
#### Assumptions:


#### Risks:
Without this nodes could continue to operate with invalid operations.

#### Functionality:

##### TrustEVM Contract
The action `pushtx` should return meta data for use by the TrustEVM node and future tooling.

```c++
struct tx_meta_v0 {
   struct {
      uint16_t major = <CMAKE_PROJECT_MAJOR>;
      uint16_t minor = <CMAKE_PROJECT_MINOR>;
      uint16_t patch = <CMAKE_PROJECT_PATCH>;
   } version;
};

using tx_meta = std::variant<tx_meta_v0>;

[[eosio::action]]
tx_meta pushtx(eosio::name ram_payer, const bytes& rlptx);
```

##### TrustEVM Node
The block receiver plugin will get and decode this `tx_meta` object from the trace of the action from the received Antelope block.

From this meta data it will check if it's major and minor version match (for now), in the future we might change that to just if the major doesn't match (i.e. follow semantic versioning).

If `tx_meta->major != node->major` or `tx_meta->minor != node->minor` then present a message that the user should update their node to the version required and shutdown.

#### Features
#### User stories
#### Additional tasks

## Open questions
