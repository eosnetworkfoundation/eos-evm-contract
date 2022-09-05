---
tags: Proposal
---

# Staking Contract

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
- The staking contract should expose data for users to view.
- The staking contract should be able to accept and hold tokens as source of rewards.
#### Features
See user stories
#### User stories
- A detailed API definition first (deliverable)
- A Staking Contract in solidity (deliverable)
  - Operator API 
    - Methods for Operator to stake EVM
    - Methods for Operator to set upstream address (optionally separate read/write) when staking
    - Methods for Operator to claim rewards, rewards is coming from the EVM the contract holds
    - Methods for Operator to view related data
  - Proxy API
    - Methods for proxy to read upstream address and weight(based on staking amount)
  - Workload Verifier API
    - Methods for workload verifier to update the payment
  - Admin API
    - Methods for admin to set allow list for valid operator/workload verifier
    - Methods for admin to view related data in an aggragate way
  - Extra functionality
    - Make the contract be capable of accepting EVM tokens as the source of payment.
#### Additional tasks
- [ ] #issue number

## Open questions
