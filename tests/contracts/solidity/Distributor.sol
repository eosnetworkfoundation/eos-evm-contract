// SPDX-License-Identifier: MIT
pragma solidity ^0.8.17;

contract Distributor {
    function distribute(address[] calldata to, uint256[] calldata amt) payable external {
        uint256 total = 0;
        for (uint256 i = 0; i < to.length; i++) {
            payable(to[i]).transfer(amt[i]);
            total += amt[i];
        }

        require(total == msg.value);
    }
}
