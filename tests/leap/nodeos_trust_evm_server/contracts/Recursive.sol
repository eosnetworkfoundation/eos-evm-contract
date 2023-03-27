// SPDX-License-Identifier: GPL-3.0
pragma solidity >=0.7.0 <0.9.0;

contract Recursive {
    
    event Call(uint256 _value);

    function start(uint256 _depth) public {
        Recursive(this).doit(_depth);
    }

    function doit(uint256 _i) public {
        emit Call(_i);
        if( _i == 0 )
            return;
        Recursive(this).doit(_i-1);
    }
}