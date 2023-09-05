// SPDX-License-Identifier: MIT
pragma solidity ^0.8.17;

contract RevertTest {
    error ErrorAfterSend();

    function sendThenFail(address to) payable external {
        bool sent = payable(to).send(msg.value);
        assert(sent);
        revert ErrorAfterSend();
    }

    function recover(address toA, address toB) payable external {
        try this.sendThenFail{gas: 10000, value: msg.value}(toA) {
            assert(false);
        } catch(bytes memory err) {
            //how?
            //require(bytes4(err) == ErrorAfterSend.selector);
        }

        payable(toB).transfer(msg.value);
    }
}
