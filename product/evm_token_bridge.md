---
tags: Proposal
---

# EVM token bridge

## Prerequisites

#### Dependencies: What work must be completed before this part of the solution is actionable?

Depends on changes for the new [gas fee algorithm](./gas_fee_algorithm.md).

## Problem

#### Opportunity: What are the needs of our target user groups?

Gas fees in the EVM tokens are collected on the EOS side of Trust EVM. There needs to be a way to move these tokens from the EOS side to the EVM side to be used within EVM contracts. This is in particular necessary to distribute some of the gas fees collected by the EVM runtime contract to a staking contract as part of the Trust EVM token economics.

#### Target audience: Who is the target audience and why? 

The new mechanism allows for moving EVM tokens between the two sides (EOS and EVM) of Trust EVM, which is valuable to any user who wishes to both use applications within the Trust EVM and applications on EOS that use the EVM token. But the primary audience for this mechanism are Trust EVM miners (the entities that provide EVM transactions for processing to the EOS blockchain) as well as anyone participating in the Trust EVM staking contract (which supports Trust EVM RPC providers).

#### Strategic alignment: How does this problem align with our core strategic pillars?

The new mechanism is a requirement for the Trust EVM token economics which creates the necessary incentives for RPC providers to provide the critical services needed for the operation of the Trust EVM platform.

## Solution

#### Solution name: How should we refer to this product opportunity?

EVM token bridge

#### Purpose: Define the product’s purpose briefly

Enable EVM tokens held as eosio.token compatible balances on the EOS side to be moved over to the EVM side as the EVM's native token, and vice versa. Additionally, allow fees collected by the EVM runtime contract to also be moved from the EOS side to the EVM side to the particular EVM address hosting the Trust EVM staking contract.

#### Success definition: What are the top metrics for the product (up to 5) to define success?

1. The fees collected in the EVM runtime contract can be moved by anyone over to the Trust EVM staking contract to be distributed according to the logic of that staking contract.
2. An EOS account with EVM tokens can move their tokens to a Trust EVM address.
2. A Trust EVM user holding EVM tokens at some address can send an EVM transaction to move those tokens to the possession of a particular EOS account.

#### Assumptions

There is an official Trust EVM staking contract deployed within Trust EVM at some address which can be provided to the EVM runtime contract through a privileged configuration action before the `collectfee` action can be used.

#### Risks: What risks should be considered? https://www.svpg.com/four-big-risks/

The solution requires reserving a range of EVM addresses. It should be virtually impossible for a legitimately constructed address (whether for externally-owned account or contract account) to conflict; someone could try to brute-force an externally-owned account address to conflict but the EVM runtime contract will reject EVM transactions that create conflicting addresses. But there is some small risk that the Ethereum protocol could evolve in the future to use addresses in the reserved range, which, if it occurred, would require the Trust EVM protocol to further diverge from other EVMs to integrate those updates while working around the conflict.

#### Functionality

Add support for eosio.token-compatible functionality to the EVM runtime contract. This means adding the `issue`, `retire`, `open`, `close`, and `transfer` actions. The `transfer` will be restricted to require the recipient to `open` a balance before funds can be sent to the recipient to avoid the complications with senders paying for the RAM of the recipients table entries. Note that the recipient account's balance entry must also be opened to send EVM tokens to the recipient from the EVM side. Otherwise, that EVM transaction would be rejected (meaning not included in the EOS blockchain) and not just reverted to avoid reproduction issues for the silkworm engine. For the same reason of avoiding reproduction issues in the silkworm engine, sending EVM tokens from the EVM side to the EOS side will increase the balance without creating a `transfer` action or running third-party smart contract code of the recipient.

A new `sendevm` action will be added to the EVM runtime contract which allows for the EOS side to initiate a transfer of EVM tokens from a balance associated to an EOS account to a specified Trust EVM address. Not only does this transfer the EVM tokens to the specified address, but it actually makes a proper EVM call to the address which would run any smart contract code at that address using call data specified in the `sendevm` action. This means that the caller of the `sendevm` action needs to have EVM tokens that they can use to pay the gas of the EVM transaction generated as a result of calling the `sendevm` action. This action can be called by EOS smart contracts as an inline action which allows a mechanism for EOS smart contracts to interact with an EVM contract; however, the initial version of `sendevm` implemented as part of this product feature will not make any data returned by the EVM contract accessible to the calling EOS smart contract. EVM transactions processed as a result of the `sendevm` action should operate in a mode where a revert of the calling code should abort the transaction; this maintains the atomicity expectations of EOS smart contracts and is particularly important since there would be no feedback mechanism to tell the calling EOS smart contract whether the EVM call reverted.

A new `collectfee` action will be added to the EVM runtime contract which is like a specialized `sendevm` action in behavior. It moves the fees collected by the EVM runtime contract to a particular EVM address which is expected to host the Trust EVM staking contract. This action can be called by anyone willing to pay the CPU/NET costs of the EOS transaction and the gas fees of the EVM transaction generated as a result of calling the `collectfee` action because the address that the EVM tokens are moved to is determined by the EVM runtime contract and not the caller. This action cannot function until the address of the Trust EVM staking contract is first established as part of the EVM runtime contract state, which must be done first by some other new action added to the EVM runtime contract which can only be called by the appropriate parties responsible for maintaining the EVM runtime contract.

#### Features

#### User stories
#### Additional tasks
<!-- - [ ] #issue number -->
## Open questions
