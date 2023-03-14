// SPDX-License-Identifier: GPL-3.0

pragma solidity >=0.7.0 <0.9.0;

contract Eventor {
    event Deposit(address indexed _from, bytes32 indexed _id, uint _value);
    function deposit(bytes32 _id, uint256 _value) public {
        emit Deposit(msg.sender, _id, _value);
    }
}
