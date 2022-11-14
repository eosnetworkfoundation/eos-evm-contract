---
tags: Proposal
---

# Delegate Staking Contract

## Prerequisites
#### Dependencies: What work must be completed before this part of the solution is actionable?
The staking contract
Overall token design
## Problem

We want to provide a way for regular users to delegate their evm to us to stake

## Solution

#### Solution name: Delegate Staking Contract
#### Purpose: 
A contract for users to delegate stake their evm

#### Success definition: 
Functioning is enough, should be no major performance requirement.
#### Assumptions
#### Risks: What risks should be considered? 
- If the lock mechanism of the origin staking contract is too restrictive, this contract may be hard to design
- If there's any problem, it's quite possible that funds got stuck in the contract. So extra care need to be taken.
- The overall token design (i.e. initial distribution, whether locked token can be staked/vote etc.) may affect the design significantly. Even a seemly small change can potentially break the whole design.

#### Functionality
Depends on the token mechanism, we have different potential plans:

##### 1 if we want to support delegate staking from public:

There are two potential easy designs:
1 siimilar to a aggragator: when user stake here, he got a share of the pool. The fund in the pool will be staked in the origin staking contract and some script will collect rewards and add them to the pool (and invest again) periodically. There will be no concept of reward from a user's point of view. The value of the share of pool will grow and this is what user gain.
2 serve kind of like a proxy. The user will be as if he is directly staking to the origin contract. Rewards are calculated use the same formula as the origin contract.

The first design is quite common for yield aggregators. 
The advantage of the second one is the behavior is similar, but the risk is there are less code to reference.

##### 2 if we only want to support delegate staking from limited number of investor:

In this case the locking is not a problem, we can have the same locking mechanism as the basic staking contract. Or we can have an even simpler design: **let the VC use the staking contract directly. The can just specify the backend URL of other nodes.**


#### Features
See user stories
#### User stories
- A detailed API definition first (deliverable)
- A Contract in solidity (deliverable)
  - User API 
    - Methods for user to stake and withdraw EVM
      - if there are no enough fund, the contract should withdraw from the orgin contract.
    - Methods for Operator to claim rewards (only if we pick the second design)
    - Methods for Operator to view related data
  - Admin API
    - Methods for admin to blacklist users
    - Methods for admin to view related data in an aggragate way
    - Methods for re-invest (only if we pick the first design)
    - Methods for admin to set the return rate for the users.
  - Extra functionality
    - Make the contract be capable of accepting EVM tokens as the source of payment.
    - Allow vTokens or tokens locked in other specific contract to stake (only if we choose to allow this)

#### Additional tasks
- [ ] #issue number

## Open questions
- Need to get the final design of the token mechanism first.
- Need to pick a design
- Need to determine the behavior when there are locking mechanisms in the origin contract. i.e. Some withdraw from user my trigger withdraw in locking period. 
