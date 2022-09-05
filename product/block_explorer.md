---
tags: Proposal
---

# Block Explorer

## Prerequisites
#### Dependencies: 
Silkworm testnet
## Problem
We need block explorers for both testnet and mainnet

## Solution

#### Solution name: Block Explorer
#### Purpose: 
Setup a block explorer for both the testnet and mainnet.
This work will probably be done via outsourcing, the brief is just high level description and will not go through too detailed things.

#### Success definition: 
Support etherscan.io level access (500k access per day)
#### Assumptions
#### Risks: 
Silkworm's support for Web3 API maybe not that complete. It's possibble that block exploreer will rely on some of those unfinished API

#### Functionality
All what etherscan support. 
Plus:
- Link from EVM block to corresponding EOS block in certain EOS explorer
- Find EVM block via EOS block id (if possible)

#### Features
#### User stories
#### Additional tasks
- [ ] #issue number

## Open questions

Talked to some outsourcing guys with experience in block explorer development. 
The existing API seems enough, except for eth_getTransactionReceipt, which is marked as partially implemented for silkworm.
This issue will be a blocker if not properly solved.
