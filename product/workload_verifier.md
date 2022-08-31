---
tags: Proposal
---

# Product brief template

## Prerequisites
#### Dependencies: 
Proxy for RPC log format
Staking contract API

## Problem

We need to calculate rewards for RPC nodes according to access log

## Solution

#### Solution name: workload_verifier
#### Purpose: 
Calculate rewards from access log periodically and save the result to the chain.

#### Success definidtion: 
Working and can achieve the required call frequency.
#### Assumptions
#### Risks: 
If the token economy model changes, it might happen that we may need complicated logic in reward calculation, or this module may also need to actively probe if a rpc node is working honestly. Such requirement may cause the module to be rewritten.
#### Functionality
- process accesslog and calculate rewards for each rpc node
- periodically update the reward to the Staking Contract
#### Features
See user stories
#### User stories
- A service that tracking reading rewards (deliverable)
  - The service can read access log
  - The service can calcualte rewards according to what it read
  - The service can save the result to the Staking Contract
  - Can config the update period via config file
#### Additional tasks
- [ ] #issue number

## Open questions
