---
tags: Proposal
---

# RPC Compatibility

## Prerequisites
#### Dependencies: What work must be completed before this part of the solution is actionable?

Depends on:
- [Gas fee algorithm](./gas_fee_algorithm.md).
- [Block mapping](./block_mapping.md).

## Problem

#### Opportunity: What are the needs of our target user groups?
Any person interacting with an EVM chain (in particular Developers) expect to use the tools/apps they already know and are familiar with.

In order for this interaction to happen, the node must implement a standard Ethereum JSON-RPC protocol that applications can communicate with.

#### Target audience: Who is the target audience and why?

Developers and regular users.

#### Strategic alignment: How does this problem align with our core strategic pillars?

Full RPC compatibility is one of the core value proposition of the TrustEVM initiative since it allows users and developers to easily switch to this network in a transparent way.

## Solution

#### Solution name: How should we refer to this product opportunity?
RPC Compatibility

#### Purpose: Define the product’s purpose briefly

Make TrustEVM compatible with all the EVM ecosystem tools/apps.

#### Success definition: What are the top metrics for the product (up to 5) to define success?

- Interact with standard EVM wallets (Metamask)
- Populate a block explorer database. (Blockscout)
- Create a graph for Uniswap V2 (The Graph).

#### Assumptions
#### Risks: What risks should be considered? https://www.svpg.com/four-big-risks/

All the Antelope core codebase is C++.
The majority of Ethereum nodes are written in Go/Rust.
The only C++ Ethereum client that is actively developed is Silkworm (Aleth is discontinued at this point).

There is benefit in leveraging the work made by the Silkworm team instead of write our own solution from scratch, but at the same time make us dependant on their release timelines and planned functionalities.

Also `Silkworm` mention in their documentation that is under active development and hasn't reached the alpha phase yet.

#### Functionality

Use `Silkworm` core libraries to generate an `Erigon` compatible database that can be accessed by the `Silkrpc` (or any other RPC daemon compatible with that database format) to serve RPC requests from clients.

#### Features

We already have a `trust_evm` node that uses the `Silkworm` core libraries to generate the db and provide rpc services in wich functionalities are split among different plugins.

Propossed extensions to the already implemented functionality:

`ship_receiver_plugin`:
 - keeps no state
 - determine where to start based `I(HEAD)` of the fake chain
 - query SHiP status `get_status_request_v0` and stops if unable to start from I(HEAD)
 - read from [`I(HEAD)`, infinite]
 - decode traces and emit `native_block` (together with EOS lib information)

`block_conversion_plugin`:
 - receives a stream of native_block
 - keep native_blocks after LIB
 - based on the time frecuency of the fake chain it will:
    * Agregate many native_block blocks into 1 EVM block
    * Generate intermediate empty EVM blocks between two native_blocks if needed
 - the header of the EVM block will:
   - reference the previous fake chain blockhash
   - use difficulty=1
   - use EOS upper bound block id
   - will put extra header information that comes from the involved transactions traces (for GAS algo).
     (we can assert that this information remains constant between block boundaries since the contract will be aware of the fake chain block number the transaction being executed will land on)
 - will emit `EVM blocks` ( with (EOS_lib, EVM_lib) )

`blockchain_plugin`:
 - receives a stream of EVM blocks
 - persists EOS and EVM LIB (ship_receiver_plugin)
 - uses `HeaderPersistence` and `BodyPersistence` to store the new EVM block
 - apply stages using `DefaultForwardOrder`
 - if a fork is detected when `HeaderPersistence` (unwind needed), unwind stages based on the `DefaultUnwindOrder`
 - when shuting down => revert all blocks in the fake chain that are not irreversible

On stages execution:

Note : at current state of development Silkworm can't actually "sync" the chain like Erigon does. 
What instead does is a one-pass loop over all implemented stages to process all blocks which are already 
in the database. Due to that you NEED a primed database from Erigon.

```
export STOP_BEFORE_STAGE="Senders"
./build/bin/erigon --datadir <path-where-to-store-data>
cmd/silkworm --datadir <same-datadir-path-used-for-erigon>
```

- We will have headers/bodies already downloaded
- `HeaderPersistence` will write headers and detect the need to unwind stages (forks)
- `BodyPersistence` will write block bodies

Stages:
- stage_blockhashes
- stage_senders
- stage_execution
- stage_hashstate
- stage_interhashes
- stage_history_index
- stage_log_index
- stage_tx_lookup
- stage_finish

In `stage_execution` we will use our own:
  - ExecutionProcessor
  - Engine

The `ExecutionProcessor` and `Engine` can be on a library shared between `contract` and `trustevm-node`

#### EVM block generation diagrams
```
     EOS blocks
======> time ======>
     EVM blocks

.   ->      reversile block
x   ->   irreversible block
```

#### When block `4` is received, `C` and `D` will be generated
```
 1  2  3         4
 .  .  .         .
 ___|___|___|___|___
  A   B   C   D  (p)
```

#### When block 6 is received, `E` is generated by aggregating `4` and `5`
```
 1  2  3         4 5   6
 .  .  .         . .   .
 ___|___|___|___|___|___
  A   B   C   D   E  (p) 
```

#### EVM LIB
```
              EOS_LIB
                 v
 1  2  3         4 5   6
 x  x  x         x .   .
 ___|___|___|___|___|___
  A   B   C   D   E  (p) 
              ^
           EVM_LIB
```

#### Fork handling when block _4'_ timestamp is after _6_
```
 1  2  3         4 5   6   4'
 .  .  .         . .   .   . 
 ___|___|___|___|___|___
  A   B   C   D   E  (p) 
```

```
 1  2  3                   4'
 .  .  .                   . 
 ___|___|___|___|___|___|___
  A   B   C'  D'  E'  F' (p)
```


#### Fork handling when block _4'_ timestamp is before old _4_
```
             4'
             |
 1  2  3     v   4 5   6
 .  .  .         . .   .
 ___|___|___|___|___|___
  A   B   C   D   E  (p) 
```

```
 1  2  3     4'
 .  .  .     .
 ___|___|___|___
  A   B   C' (p)
```


#### `block_conversion_plugin` pseudo code 
```python
INTERVAL = 1s
def to_evm(block):
     return ceil(ceil(block.ts-genesis.ts)/INTERVAL)

def on_init():
    evm_lib, eos_lib = load_evm_state()
    blocks.push_back(eos_lib)
    last_evm = evm_lib

def on_new_block(new_lib, new_block):
     remove(blocks[0].num, new_lib.num)
     prev = find_prev(new_block.id):
     assert(prev != None)
     if new_block.prev_id != blocks[-1].id:
          remove(prev.num+1, blocks[-1].num)
          last_evm = to_evm(prev)
          if( last_evm == to_evm(new_block) ) last_evm-=1
     blocks.push_back(new_block)

     .......

```


#### User stories
#### Additional tasks
- [ ] #issue number

## Open questions
