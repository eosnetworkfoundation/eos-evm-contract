---
tags: Proposal
---

# Product brief template

## Prerequisites
#### Dependencies: What work must be completed before this part of the solution is actionable?

## Problem

The RPC operators supposed to stake and claim rewards on EVM universe. So we need a Staking Contract for it.
Meanwhile, we want to push info such as upstream addresses in the same place so it would easier to make things consistent.

## Solution

#### Solution name: Staking Contract
#### Purpose: 
Staking contract for operators to stake and claim rewards. 
The contract should also allow interaction with proxy/workload verifier
#### Success definition: 
Functioning is enough, should be no major performance requirement.
#### Assumptions
#### Risks: What risks should be considered? 
We may want more complicated mechanism for the staking, which will cause this staking contract to face constant overhaul.
#### Functionality
- The staking contract should allow permitted RPC operators to stake EVM to take part in the RPC reward mechanism. 
- The staking contract should serve as a location to save PRC operator upstream info and rewards info.
#### Features
See user stories
#### User stories
1. Operator can stake EVM
2. Operator can set upstream address (optionally separate read/write) when staking
3. API for proxy to read upstream address and weight(staking amount)
4. Allow list to let admin set valid operator/workload verifier
5. API for workload verifier to update the payment
6. Operator can claim rewards, rewards is coming from the EVM the contract holds
7. The contract can accept EVM tokens as the source of payment
#### Additional tasks
- [ ] #issue number

## Open questions
