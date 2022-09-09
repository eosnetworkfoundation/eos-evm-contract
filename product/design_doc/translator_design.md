The translator module does not have detailed design doc as there are too many unknown things before the development start. Now it’s a good time to review the design and have some docs.

# The Proof of Concept Translator

The proof of concept translator is coded with Nodejs. The reason for selecting Nodejs is that the Nodejs version of Ethereum client project is quite complete and active. Also, js/ts projects are relatively easy to develop.

The proof of concept translator works as the following:

- Call get_block to Nodeos through http API
- For every 10 EOS blocks, the Translator generate one EVM block
    1. Transactions in EOS blocks are included sequentially
    2. Gas limit is locked to 0x7ffffffffff
    3. Difficulty is locked to 1
    4. ExtraData field is used to store the blockid of the last of the 10 EOS blocks
    5. Timestamp is the timestamp of the last of the 10 EOS blocks
- Sync the new blocks to a slightly modified (gas limit and difficulty) Geth (with PoW check off obviously) client via p2p protocol

The Geth client is then used to serve all read access.

![Figure 1: Overview of the system.]([https://s3-us-west-2.amazonaws.com/secure.notion-static.com/9c4472c1-969f-4032-8739-923be9f4f67a/EOS-EVM_design.drawio.png](https://github.com/eosnetworkfoundation/TrustEVM/blob/yarkinwho-product-desc-update/product/design_doc/EOS-EVM%20design.drawio.png)

Figure 1: Overview of the system.

## Limitation and Findings

- **It’s Nodejs.** Heathen!
- **http API is used.** Use it for easier development instead of using ship protocol.
- **The p2p connection to the Geth is quite unstable.** The instability usually happens when there is something wrong during the sync. If Geth missed one block, it will never retry fetching that block from the same peer and will wait for another peer to fix this issue eventually, which will never happen in our design. Even with a lot of hacks to force retry, there are still some instability remains, mainly happen when EOSIO micro-forks causing the EVM chain to switch branch. We didn’t spend more time to debugging for that as the p2p thing will be removed in silkworm design anyway.
- **No version/fork control.** The translator will never aware if there’s any broken changes in the system.
- **Block height and Block hash.** The EVM runtime is not aware of the translator so it has no knowledge of block height and block hash used in the fake chain. The testnet have such function disabled in contract
- **Lacking a way to detect data difference between EVM runtime and Translator.** The translator has the root of data calculated using the standard Ethereum approach but the runtime does not have such thing.
- **Faster block speed is possible.** Faster (e.g. 5 to 1 mapping or 2 to 1 mapping) block speed is tested and seems fine. We even tested 1 to 1 mapping by skipping timestamp validation in Geth code. And it works at least for simple smoke tests. But for user experience, extremely fast mapping (1 to 1) shows no clear advantages as most client (e.g. metamask) will not query for transaction result that frequently anyway. This hints that maybe it is better to use more conservative 2 to 1 (or even slower) mapping in the final design so that we can avoid changing Ethereum timestamp logic.

# The Silkworm based Translator

The Silkworm based translator will have the following advantages:

- It’s Modem C++.
- Silkworm has the faster Erigon design.
- Ideally it can serve as a plugin of Nodeos so it can be notified for new blocks just in time and can read required data directly from memory.
- It can have a standalone RPC process reading from the same database. This is good for deployment.
- No Geth is needed so we do not need to debug the p2p craps again.
- It can be configured to use the exact same VM code as the EVM runtime for better consistency.

![Figure 2: System with Silkworm](https://github.com/eosnetworkfoundation/TrustEVM/blob/yarkinwho-product-desc-update/product/design_doc/EOS-EVM%20design%20silkworm.drawio.png)

Figure 2: System with Silkworm

## Current state

It is working with the same 10 to 1 mapping. Matias and Bucky is currently working on making it share same VM code as the EVM runtime and increase some stability there.

# Works to solve the discovered issues

Clearly, some issues discovered in the PoC are solved automatically with the Silkworm but some are not:

- Nodejs: Solved by Silkworm
- http API used: Solved by Silkworm
- p2p unstable: Solved by Silkworm
- No version control: Proposed solution:
    - EVM runtime save some version number in certain table together with the id of the block that the new version/fork should apply.
    - Translator should reject to process after that if it is not patched to the new version.
- In contract block height and hash: Proposed solution:
    - Use timestamp to calculate block height
        - e.g. if T is defined as genesis and it’s 1s block, then any transactions in any EOS block with timestamp in [T+h, T+h+1) belongs to the h’th EVM block. (assume genesis height is 0)
    - Calculate hash from block height and some other deterministic things
- Lacking data integrity check: Proposed solution:
    - Take snapshots periodically for recovery and checks.
    - Use some external service to scan.
    - Were there any issue, EVM runtime states shall prevail.
    - Patch the silkworm and recover from snapshots.
