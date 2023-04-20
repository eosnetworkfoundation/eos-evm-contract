// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.8.2 <0.9.0;

contract Blockhash {

    uint256 curr_block;
    bytes32 prev1;
    bytes32 prev2;
    bytes32 prev3;
    bytes32 prev4;
    bytes32 prev5;

    function go() public {
        curr_block = block.number;
        prev1 = blockhash(block.number-1);
        prev2 = blockhash(block.number-2);
        prev3 = blockhash(block.number-3);
        prev4 = blockhash(block.number-4);
        prev5 = blockhash(block.number-5);
    }

    function r_curr_block() public view returns (uint256){
        return curr_block;
    }
    function r_prev1() public view returns (bytes32){
        return prev1;
    }
    function r_prev2() public view returns (bytes32){
        return prev2;
    }
    function r_prev3() public view returns (bytes32){
        return prev3;
    }
    function r_prev4() public view returns (bytes32){
        return prev4;
    }
    function r_prev5() public view returns (bytes32){
        return prev5;
    }

}