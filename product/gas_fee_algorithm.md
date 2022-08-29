---
tags: Proposal
---

# Gas fee algorithm

## Prerequisites
#### Dependencies: What work must be completed before this part of the solution is actionable?

A functional EVM runtime contract using the standard EVM algorithm for calculating gas consumption and gas rebate for an EVM transaction is required as a base to make modifications to. This existing contract is not required to have already implemented any particular algorithm for how the base gas fee is dynamically changed, since that algorithm is to be significantly changed compared to what Ethereum uses as their algorithm.

## Problem

#### Opportunity: What are the needs of our target user groups?

Our target users expect to pay a single fee in the native token of Trust EVM, called the EVM token, per transaction to cover all the resource costs of processing that transaction. Due to the nature of the EOS environment on top of which Trust EVM runs, the EVM runtime contract must use the collected fee from the user to provide incentive to all parties covering the EOS costs of processing this transaction which includes RAM and CPU/NET. And it must continue to provide sufficient incentive even as the price of RAM and CPU/NET independently fluctuate on the EOS network all while continuing to present the costs to the users as a simple single gas fee (though varying over time) on each transaction.

#### Target audience: Who is the target audience and why? 

The target audience are users and developers who are familiar with interacting with applications and tools in the wider EVM ecosystem, particularly the Ethereum ecosystem. We want Trust EVM to maintain as much compatibility as is feasible with the Ethereum EVM to allow the plethora of EVM-based tools in the ecosystem to be immediately leveraged with Trust EVM. We also want to meet the expectations of this target audience when it comes to how they manage and pay for resources, particularly that they expect to just pay a short-term predictable gas fee per transaction, to reduce cognitive friction with their user experience in interacting with applications built on the Trust EVM platform.
#### Strategic alignment: How does this problem align with our core strategic pillars?

A core pillar of Trust EVM is to attract developers and users in the wider EVM ecosystem. Maintaining compatibility with their existing tools and expectations helps make Trust EVM attractive. However, compatibility must be balanced with a resource model that can sustainably continue to enable operation. This means the resource and pricing model must ensure the fees collected from users are priced accordingly to cover the costs of "miners" who submit the EVM transactions for processing on the EOS blockchain and to cover the costs of allocating enough RAM for the EVM runtime contract so that it does not run out of storage space needed to continue processing EVM transactions. If the availability of the EVM runtime contract is compromised because of bad pricing, the Trust EVM platform would obviously look significantly less attractive to our target audience.

## Solution

#### Solution name: How should we refer to this product opportunity?

The Trust EVM gas fee algorithm.

#### Purpose: Define the product’s purpose briefly

Dynamically adjust the base gas price and the amount of gas to refund for freeing up storage space to ensure a sufficient fee is charged per transaction to cover the varying costs of RAM and CPU/NET on the EOS network. Additionally, direct a portion of the collected fees to a balance managed by the EVM runtime contract to pull from as needed to cover the costs of maintaining sufficient RAM for the EVM runtime contract as well as to potentially provide a discount on the gas fee paid by the user (perhaps to the point of having a negative gas fee in some cases) as a strong incentive to free up unneeded storage space used by the EVM runtime contract. The remaining portion of the collected fees should also be sufficient, after the EVM runtime contract takes a cut, to cover the CPU/NET costs for the "miner" that submits the EVM transaction to the EVM runtime contract via an EOS transaction that they pay the CPU/NET costs for by the fact that they are the first authorizer of the EOS transaction.

#### Success definition: What are the top metrics for the product (up to 5) to define success?

1. RAM allocation for the EOS account that the EVM runtime contract is deployed remains sufficient to prevent it from failing to process EVM transactions successfully.
2. Users and developers of contracts running on Trust EVM always have an incentive to free up storage slots that are no longer needed by a contract.
3. It remains profitable to miners to submit EVM transactions to the EOS blockchain for processing, assuming users are willing to pay the gas fee for their EVM transaction dictated by the gas consumption and base gas price calculated by the algorithm.
4. The gas fee that users must pay for their EVM transaction, as calculated dynamically by the algorithm, does not become significantly higher than what is needed to cover the storage and computation costs of processing the transaction on EOS plus some arbitrary profit margin chosen by the Trust EVM governance system.

#### Assumptions

The algorithm to calculate the gas consumed by a transaction must be the same as the one used in Ethereum. The algorithm to calculate the gas refund (due to freeing up storage) can be different. The algorithm to adjust the base gas price can also be different than how it is done for Ethereum.

Whatever algorithm is used for adjusting the base gas price or calculate gas consumed / refunded must be identical whether computed by the EVM runtime contract or by the downstream silkworm engine.

#### Risks: What risks should be considered? https://www.svpg.com/four-big-risks/

Significant changes to the gas price algorithm could a risk that existing EVM tools and client application software choose a max gas price for the transaction which becomes insufficient by the time the transaction gets processed on the EOS blockchain.

If the new algorithms are not designed well, there is a risk that insufficient EVM tokens will be raised from consuming more storage slots to cover the actual RAM costs imposed by the EOS blockchain on the EVM runtime contract. In that case, a benefactor for Trust EVM, e.g. the foundation, may need to put in addition money into the project to ensure the EVM runtime contract has enough RAM to continue processing transactions. 

#### Functionality

As gas consumed is calculated according to the standard Ethereum gas algorithm, the EVM runtime contract separately keeps track of gas consumption attributable to purely computational (or network bandwidth) costs versus gas consumption attributable to storage costs. It also separately keeps track of the gas refund due to freeing up existing computation (i.e. the storage gas refund), but this would be following a new algorithm and not the standard Ethereum gas algorithm.

Using the base gas price calculated for the start of the virtual EVM block, the EVM runtime contract calculates:
* the gas fee for computation which depends on the gas consumption attributable to computational costs;
* the gas fee for storage which depends on the gas consumption attributable to storage costs less the storage gas refund;
* the net gas fee pulled from the sender's balance to exactly cover the two gas fees above.

A cut of the gas fee for computation is taken by the EVM runtime contract itself and the rest goes to the balance of the account specified as the `gas_recipient` in the `pushtx` action.

The gas fee for storage is directed to a special balance in the EVM runtime contract that is meant to cover the costs of purchasing more RAM for the EVM runtime contract as well as act as the source of value to cover the storage gas refund costs which act as an incentive for users and smart contract developers to free up unused storage space in EVM contracts.

To ensure the gas fee for storage covers the expense of maintaining enough RAM for the EVM runtime contract, the base gas price needs to take into account the actual cost of RAM (priced in terms of EVM tokens) on the EOS blockchain. As that cost increases, the base gas price may need to increase as well.

The other major input into the new base gas price algorithm is the cost of CPU/NET resources on the EOS blockchain. If that gets too expensive, it may become the driving factor raising the base gas price for the EVM. This is to ensure enough gas fee is collected from each EVM transaction to incentivize a miner to process the transaction.

To ensure that the silkworm engine is able to exactly reproduce the algorithm but to also avoid creating complicated dependencies to inputs like the cost of RAM or CPU/NET, the EVM runtime contract will provide the base gas price that it used to process each transaction, perhaps as an action return value. The EVM runtime contract must restrict itself to only change the base gas price on virtual block boundaries to maintain compatibility with silkworm engine expectations; the new gas price is fed into silkworm engine through the block headers of the fake EVM chain that it processes.

#### Features

#### User stories
#### Additional tasks
<!-- - [ ] #issue number -->

## Open questions

Are we allowed to make the algorithm that determines how much of a gas refund is provided be parameterized dynamically to allow for an extra degree of freedom in aligning storage freeing incentives properly?

Is there any bound that must be enforced on how quickly the base gas price can rise?