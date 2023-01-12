// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

/**
 * @title Storage
 * @dev Store & retrieve value in a variable
 * @custom:dev-run-script ./scripts/deploy_with_ethers.ts
 */
contract BlockNum {
    uint256 public blockNumber;
    uint256 public timestamp;
    uint256 public difficulty;
    address public coinbase;
    constructor() {
        blockNumber = block.number;
        timestamp = block.timestamp;
        difficulty = block.difficulty;
        coinbase = block.coinbase;
    }
    function setValues() public payable {
        blockNumber = block.number;
        timestamp = block.timestamp;
        difficulty = block.difficulty;
        coinbase = block.coinbase;
    }
    function estimateBlockNumber() public view returns (uint256) {
        return block.number;
    }
    function estimateTimestamp() public view returns (uint256) {
        return block.timestamp;
    }
    function kill(address payable _to) public payable {
        selfdestruct(_to);
    }
}
