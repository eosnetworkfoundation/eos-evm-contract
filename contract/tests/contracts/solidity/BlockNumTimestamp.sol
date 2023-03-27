// SPDX-License-Identifier: GPL-3.0
pragma solidity >=0.8.2 <0.9.0;

contract BlockNumTimestamp {
    uint256 public blockNumber;
    uint256 public timestamp;

    constructor() {
        blockNumber = block.number;
        timestamp = block.timestamp;
    }
}