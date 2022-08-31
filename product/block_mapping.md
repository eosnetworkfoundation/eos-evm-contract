---
tags: Proposal
---

# Block Mapping

## Prerequisites
#### Dependencies: N/A

## Problem

#### Opportunity:
Currently block mapping between TrustEVM on EOS and TrustEVM node do not map to the same numbers. To allow for block hash to work consistently between these two we need a consistent mapping system.
#### Target audience:
- API node operators
- contract developers
#### Strategic alignment:
It will allow for consistency between on an Antelope chain and TrustEVM node which will allow for more tooling to work correctly and some allow for more operations at the EVM layer. 

## Solution

#### Solution name: Consistent Block Mapping
#### Purpose: 
Define and implement the block mapping used by TrustEVM on EOS and by TrustEVM node.
#### Success definition: 
The block numbers on TrustEVM contract and TrustEVM node are the same. Running block hash produces the same hash.
#### Assumptions:
- The contract block mapping needs to be finished first in this feature.

#### Risks:
The inclusion of producing empty blocks to take up slack might be slow.
#### Functionality:
A "genesis" timestamp will be captured. Proposing a new action called `init` in TrustEVM contract. Much like `init` for the system contract needs to be run first, this `init` will need to be run first.

```c++
[[eosio::action]]
void init() {
   // this action may also be used for setting 
   // the initial genesis state too
   running_state.set_genesis(current_time_point());
}

int64_t get_evm_block_number() const {
   const auto& genesis = running_state.get_genesis().sec_since_epoch();
   const auto& now = current_time_point().sec_since_epoch();
   
   return now - genesis;
}

.
.
.

[[eosio::action]]
void pushtx(...) {
   // inside of pushtx
   block.header.number = eosio::internal_use_do_not_use::get_block_num();
   block.header.timestamp   = eosio::current_time_point().sec_since_epoch();
```
Based on the block timestamp of the Antelope block and the time from initialization we simply get the slot block number for the time frame we are in.

#### Features
#### User stories
#### Additional tasks

## Open questions
