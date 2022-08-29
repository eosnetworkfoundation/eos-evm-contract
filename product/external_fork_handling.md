---
tags: Proposal
---

# External Fork Handling

## Prerequisites
#### Dependencies: Block mapping

## Problem

#### Opportunity: Support forks occuring and not relying on Silkworm's "native" forking logic.
#### Target audience: TrustEVM node admins, RPC providers 
#### Strategic alignment: Ensure quick and succinct forking resolution immediately when determined from Antelope blocks.

## Solution

#### Solution name: External Fork Handling
#### Purpose: When the Antelope chain that is running TrustEVM forks that forking will impact the "fake" EVM chain and we need to handle it.
#### Success definition: We can successfully rollback the TrustEVM node chain immediately. The forking depth should not cause failure but can impact overall performance.
#### Risks: The TrustEVM node software already has code inplace to handle this forking. The ultimate issue is that libmdbx has some deep internal failures that cause it fail at seemingly random times.  So, the risk is that the solution to fix libmdbx will be too intensive, we won't be able to fix without redesign, we won't be able to determine the root cause of failure.
#### Functionality
On each new block we create a new `RWTxn` object from libmdbx that is using the previous `RWTxn` object as its parent state.

A map (`txns`) between block height and `RWTxn` object will be maintained.

When the system receives a LIB advancement signal then it will commit any "open" transactions in `txns` and remove them where the block height ≤ the current LIB block height.

When we recieve a fork signal we will remove the "open" transactions in `txns` where the block height ≥ the forking block height.

As mentioned before this current logic has already been implemented, but we will need to fix libmdbx.  Or, we will need to create a different solution that doesn't use libmdbx transactional system.

So, we could create a datatype called 'transaction' where we have a map of bytes to bytes. A method called `commit` would simply do nothing and erase the internal map. A method called `rollback` will reapply the the entries back to the system.

We then modify silkworm so that when we insert or update a KV entry we copy the old entry to the map. And then allow it to work as usual. We only update the map once per "transaction".

We can then use the same logic from using the `RWTxn` approach.

#### Features
N/A
#### User stories
N/A

## Open questions
Do we have a research period of a week to determine if we can resolve the issues with libmdbx? Or go straight into the other approach.
