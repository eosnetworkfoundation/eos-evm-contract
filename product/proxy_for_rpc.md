---
tags: Proposal
---

# Product brief template

## Prerequisites
#### Dependencies: 
The API of the Staking Contract

## Problem

#### Opportunity: 
We want to fairly assign works to RPC providers and get them rewarded fairly. 
The central proxy approach is a worth trying one.


## Solution

#### Solution name: Proxy for RPC (Maybe balance loader?)
#### Purpose: 
Develop a proxy that can forward incoming traffic according to weights based on the staking of the RPC providers. 
Record detailed access log so that other module can use to calculate rpc rewards.
#### Success definition: 
The proxy should running with all the required functionalities and can achieve required performane (TBD) 
#### Assumptions
#### Risks: 
This task will need multiple 3rd party tools. This may impose some risk in safety/license.
#### Functionality
- The proxy should direct traffic to upstream in a weighted manner.
- The upstream and weight should be updated periodically on the fly according to some on-chain data.
- Log enough access log so other modules can calculate rpc rewards from.
#### Features
See user stories
#### User stories
- The proxy is based on nginx
- Access log is formatted to meet the need of other modules
- Upsync module for nginx is used to update upstream
- Storage service (e.g. Redis or Consul) to hold upstream info for upsync module to read
- A small helper service reading staking info from chain, translate it into upstream info and save it to storage service

#### Additional tasks


## Open questions
The central proxy solution may have sub-optimized performance if upstream servers are located in a far way location. This is something beyond fix with this task alone.
