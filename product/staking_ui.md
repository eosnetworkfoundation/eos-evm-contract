---
tags: Proposal
---

# Product brief template

## Prerequisites
#### Dependencies: 
Staking Contract API

## Problem
The RPC operators need an UI for their convenience.


## Solution

#### Solution name: Staking UI
#### Purpose: 
Provide an UI for the Staking Contract
#### Success definition: 
Operators and admins can use the UI to do things they should be able to do according to the Staking Contract design.
No performance requirement.
#### Assumptions
#### Risks: 
The UI maybe eventually integreted into other projects such as the TrustEVM portal, this may cause this task to be cancelled.
#### Functionality
UI should support all methods operator and admin can call.
UI should provide a page for operator and admin to view important data.

#### Features
see user stories.

#### User stories
- UI supports connecting to wallets. At least MetaMask.
- Operator Main Page (deliverable)
  - UI supports operator to stake and set upstream info
  - UI supports operator to claim rewards
  - UI supports operator to view his current staking/upstream info/pending rewards
- Admin Main Page (deliverable)
  - UI supports admin to view all operators' info in an aggregate view
  - UI supports admin to set allowlist for users (operator or workload verifier)
  - UI supports admin to set other parameters if there's any defined in Staking Contract API

#### Additional tasks
- [ ] #issue number

## Open questions
