// SPDX-License-Identifier: GPL-3.0
pragma solidity >=0.7.0 <0.9.0;

contract Recursive {
    
    event Call(uint256 _value);
    function start(uint256 _depth) public {
        emit Call(_depth);
        if( _depth == 0 )
            return;
        Recursive(this).start(_depth-1);
    }
}