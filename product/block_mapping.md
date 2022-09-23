---
tags: Proposal
---

# Block Mapping

## Prerequisites
#### Dependencies: N/A

## Problem

#### Opportunity:
Currently block mapping between TrustEVM on EOS and TrustEVM node do not map to the same numbers. To allow for block hash to work consistently between these two we need a consistent mapping system. And to allow for deterministic block numbering across systems.
#### Target audience:
- API node operators
- Contract developers
#### Strategic alignment:
It will allow for consistency between on an Antelope chain and TrustEVM node which will allow for more tooling to work correctly and some allow for more operations at the EVM layer. 

## Solution

#### Solution name:
Consistent Block Mapping
#### Purpose: 
Define and implement the block mapping used by TrustEVM on EOS and by TrustEVM node.
#### Success definition: 
The block numbers on TrustEVM contract and TrustEVM node are the same. Running block hash produces the same hash.
#### Assumptions:
The contract block mapping needs to be finished first in this feature.

#### Risks:
The inclusion of producing empty blocks to take up slack might be slow.
#### Functionality:

##### TrustEVM Contract
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

[[eosio::action]]
int64_t gentime() const {
   return running_state.get_genesis();
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

##### TrustEVM Node
The block receiver plugin will obtain the genesis timestamp of running EVM chain by evaluating the first call the `init` action or by reading the table entry for this genesis time, either way is acceptable.

When we are constructing an EVM block we will look at the timestamp of the Antelope block and compute the time the same way as the method in the contract of `get_evm_block_number()`.

As these blocks are coming in, we will aggregate all Antelope blocks into one EVM block per time delta.

If we have gaps in the EVM block numbering, i.e. the next Antelope block that is going to be used for EVM and the generated time slot block number is not the direct successor of the last fake block produced then we should construct `N` empty blocks to fill the range that is missing.

##### Making Forking Logic Work
We will be using the built-in forking handling of Silkworm to handle the EVM forking, but we still need to determine when a fork has occurred and determine how many empty blocks to create to fill the reverted gap.

To determine that a fork has occurred we keep the last block that we received from the block receiver and determine see if it is the previous of the block we are currently operating on.  If the block  does not link, then we roll back the entirety of the chain from the block whose timestamp is after the fork to now and replay from that block.

Because we will now have a gap in the block numbering as the forked block will be at a different time but link to a predecessor.

`block 0 : antelope block 400 at time 0`
`block 1 : antelope block 402 at time 1`
`block 2 : antelope block 404 at time 2`
`block 3 : antelope block 406 at time 3`

<pre>
| block 0 ← block 1 ← block 2 ← block 3 |
</pre>
A fork occurs at antelope block 404.
So now we have a new block `block 4 : antelope 404 at time 4`, this fails because we no longer have block 2 and block 3
Antelope Chain
<pre>
| block 0 ← block 1 ← block 2 ← block3 |
|                   ↖ block 4          |
</pre>

We need to produce the intermediate empty blocks to fill the gaps.
TrustEVMNode Chain
<pre>
| block 0 ← block 1 ← block 2 ← block3              |
|                   ↖ block 2' ← block 3' ← block 4 |
</pre>

### Motivations for Doing Timebased Block Mapping
Unlike other implementations that are using the block height or block number of the parent chain to direct their EVM block numbers, we have decided to use time as that basis for that mapping.

- Using time as the basis for determining block number is useful in a few ways. It allows for the TrustEVMNode chain and the TrustEVM (on Antelope) chain to stay in sync with block numbering very easily.

- We can change the duration of Antelope block production and not have to worry about that breaking our TrustEVM system.  If you use block height of the Antelope chain then you have to account for the fact that EVM can only work with seconds in precision, so you would need to coalesce blocks which would ultimately mean that as changes are made to the underlying chain TrustEVM would need to handle those.  By this I mean if we have a protocol feature on Antelope that changes the time span for a block and we have a coalesing of N blocks to satisfy our second interval precision, then when that protocol feature is activated we then need to "fix" TrustEVM to be able to know that the change has happened and support both ways.  By abstracting this of time, we know that if is never going to change and therefore will continue to operate with any changes we need.

- It will allow us to easily adapt this model for things like multiple blocks sharing the same timestamp to increase throughput by modifying one parameter.
