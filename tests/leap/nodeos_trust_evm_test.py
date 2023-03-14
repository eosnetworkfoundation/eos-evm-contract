#!/usr/bin/env python3

import random
import os
import json
import time

import sys
from binascii import unhexlify
from web3 import Web3

sys.path.append(os.path.join(os.getcwd(), "tests"))

os.environ["CORE_SYMBOL_NAME"]='EOS'
print(f"CORE_SYMBOL_NAME: {os.environ.get('CORE_SYMBOL_NAME')}")

from TestHarness import Cluster, TestHelper, Utils, WalletMgr, CORE_SYMBOL
from TestHarness.TestHelper import AppArgs

###############################################################
# nodeos_trust_evm_test
#
# Set up a TrustEVM env and run simple tests.
#
# Need to install:
#   web3      - pip install web3
#
# --trust-evm-contract-root should point to root of TrustEVM contract build dir
#                           contracts should be built with -DWITH_TEST_ACTIONS=On
#
# Example (Running with leap src build):
#  cd ~/leap/build
#  ~/TrustEVM/tests/leap/nodeos_trust_evm_test.py --trust-evm-contract-root ~/TrustEVM/contract/build --leave-running
#
# Example (Running with leap dev-install):
#  export PYTHONPATH=<leap-dev-install-root>/lib/python3/dist-packages
#  ~/TrustEVM/tests/leap/nodeos_trust_evm_test.py --trust-evm-contract-root ~/TrustEVM/contract/build --leave-running
#
#  Launches wallet at port: 9899
#    Example: bin/cleos --wallet-url http://127.0.0.1:9899 ...
#
###############################################################

Print=Utils.Print
errorExit=Utils.errorExit

appArgs=AppArgs()
extraArgs = appArgs.add(flag="--trust-evm-contract-root", type=str, help="TrustEVM contract build dir", default=None)

args=TestHelper.parse_args({"--keep-logs","--dump-error-details","-v","--leave-running","--clean-run" }, applicationSpecificArgs=appArgs)
debug=args.v
killEosInstances= not args.leave_running
dumpErrorDetails=args.dump_error_details
keepLogs=args.keep_logs
killAll=args.clean_run
trustEvmContractRoot=args.trust_evm_contract_root

assert trustEvmContractRoot is not None, "--trust-evm-contract-root is required"


seed=1
Utils.Debug=debug
testSuccessful=False

random.seed(seed) # Use a fixed seed for repeatability.
cluster=Cluster(walletd=True)
walletMgr=WalletMgr(True)

pnodes=1
total_nodes=pnodes + 2

def generate_evm_transactions(nonce):
    for i in range(1, 5): # execute a few
        Utils.Print("Execute ETH contract")
        nonce += 1
        toAdd = "2787b98fc4e731d0456b3941f0b3fe2e01430000"
        amount = 0
        signed_trx = w3.eth.account.sign_transaction(dict(
            nonce=nonce,
            #        maxFeePerGas=150000000000, #150 GWei
            gas=100000,       #100k Gas
            gasPrice=1,
            to=Web3.toChecksumAddress(toAdd),
            value=amount,
            data=unhexlify("6057361d000000000000000000000000000000000000000000000000000000000000007b"),
            chainId=evmChainId
        ), evmSendKey)

        actData = {"ram_payer":"evmevmevmevm", "rlptx":Web3.toHex(signed_trx.rawTransaction)[2:]}
        retValue = prodNode.pushMessage(evmAcc.name, "pushtx", json.dumps(actData), '-p evmevmevmevm')
        assert retValue[0], "pushtx to ETH contract failed."
        Utils.Print("\tBlock#", retValue[1]["processed"]["block_num"])
        row0=prodNode.getTableRow(evmAcc.name, 3, "storage", 0)
        Utils.Print("\tTable row:", row0)
        time.sleep(1)


try:
    TestHelper.printSystemInfo("BEGIN")

    w3 = Web3()

    cluster.setWalletMgr(walletMgr)

    cluster.killall(allInstances=killAll)
    cluster.cleanup()
    walletMgr.killall(allInstances=killAll)
    walletMgr.cleanup()

    specificExtraNodeosArgs={}
    shipNodeNum = total_nodes - 1
    specificExtraNodeosArgs[shipNodeNum]="--plugin eosio::state_history_plugin --state-history-endpoint 127.0.0.1:8999 --trace-history --chain-state-history --disable-replay-opts  "

    extraNodeosArgs="--contracts-console"

    Print("Stand up cluster")
    if cluster.launch(pnodes=pnodes, totalNodes=total_nodes, extraNodeosArgs=extraNodeosArgs, specificExtraNodeosArgs=specificExtraNodeosArgs) is False:
        errorExit("Failed to stand up eos cluster.")

    Print ("Wait for Cluster stabilization")
    # wait for cluster to start producing blocks
    if not cluster.waitOnClusterBlockNumSync(3):
        errorExit("Cluster never stabilized")
    Print ("Cluster stabilized")

    prodNode = cluster.getNode(0)
    nonProdNode = cluster.getNode(1)

    accounts=cluster.createAccountKeys(1)
    if accounts is None:
        Utils.errorExit("FAILURE - create keys")

    evmAcc = accounts[0]
    evmAcc.name = "evmevmevmevm"

    testWalletName="test"

    Print("Creating wallet \"%s\"." % (testWalletName))
    testWallet=walletMgr.create(testWalletName, [cluster.eosioAccount,accounts[0]])

    # create accounts via eosio as otherwise a bid is needed
    for account in accounts:
        Print("Create new account %s via %s" % (account.name, cluster.eosioAccount.name))
        trans=nonProdNode.createInitializeAccount(account, cluster.eosioAccount, stakedDeposit=0, waitForTransBlock=True, stakeNet=10000, stakeCPU=10000, buyRAM=10000000, exitOnError=True)
        transferAmount="100000000.0000 {0}".format(CORE_SYMBOL)
        Print("Transfer funds %s from account %s to %s" % (transferAmount, cluster.eosioAccount.name, account.name))
        nonProdNode.transferFunds(cluster.eosioAccount, account, transferAmount, "test transfer", waitForTransBlock=True)
        trans=nonProdNode.delegatebw(account, 20000000.0000, 20000000.0000, waitForTransBlock=True, exitOnError=True)

    contractDir=trustEvmContractRoot + "/evm_runtime"
    wasmFile="evm_runtime.wasm"
    abiFile="evm_runtime.abi"
    Utils.Print("Publish evm_runtime contract")
    prodNode.publishContract(evmAcc, contractDir, wasmFile, abiFile, waitForTransBlock=True)
    trans = prodNode.pushMessage(evmAcc.name, "init", '{"chainid":15555}', '-p evmevmevmevm')
    prodNode.waitForTransBlockIfNeeded(trans[1], True)
    transId=prodNode.getTransId(trans[1])
    blockNum = prodNode.getBlockNumByTransId(transId)
    block = prodNode.getBlock(blockNum)
    Utils.Print("Block Id: ", block["id"])
    Utils.Print("Block timestamp: ", block["timestamp"])

    Utils.Print("Set balance")
    trans = prodNode.pushMessage(evmAcc.name, "setbal", '{"addy":"2787b98fc4e731d0456b3941f0b3fe2e01439961", "bal":"0000000000000000000000000000000000100000000000000000000000000000"}', '-p evmevmevmevm')

    prodNode.waitForTransBlockIfNeeded(trans[1], True)

    Utils.Print("Send balance")
    evmChainId = 15555
    fromAdd = "2787b98fc4e731d0456b3941f0b3fe2e01439961"
    toAdd = '0x9edf022004846bc987799d552d1b8485b317b7ed'
    amount = 100
    nonce = 0
    evmSendKey = "a3f1b69da92a0233ce29485d3049a4ace39e8d384bbc2557e3fc60940ce4e954"

    signed_trx = w3.eth.account.sign_transaction(dict(
        nonce=nonce,
#        maxFeePerGas=150000000000, #150 GWei
        gas=100000,       #100k Gas
        gasPrice=1,
        to=Web3.toChecksumAddress(toAdd),
        value=amount,
        data=b'',
        chainId=evmChainId
    ), evmSendKey)

    Utils.Print("raw: ", signed_trx.rawTransaction)

    actData = {"ram_payer":"evmevmevmevm", "rlptx":Web3.toHex(signed_trx.rawTransaction)[2:]}
    trans = prodNode.pushMessage(evmAcc.name, "pushtx", json.dumps(actData), '-p evmevmevmevm')
    prodNode.waitForTransBlockIfNeeded(trans[1], True)

    #
    # Test some failure cases
    #

    # incorrect nonce
    Utils.Print("Send balance again, should fail with wrong nonce")
    retValue = prodNode.pushMessage(evmAcc.name, "pushtx", json.dumps(actData), '-p evmevmevmevm', silentErrors=True, force=True)
    assert not retValue[0], f"push trx should have failed: {retValue}"

    # correct nonce
    nonce += 1
    signed_trx = w3.eth.account.sign_transaction(dict(
        nonce=nonce,
        #        maxFeePerGas=150000000000, #150 GWei
        gas=100000,       #100k Gas
        gasPrice=1,
        to=Web3.toChecksumAddress(toAdd),
        value=amount,
        data=b'',
        chainId=evmChainId
    ), evmSendKey)

    actData = {"ram_payer":"evmevmevmevm", "rlptx":Web3.toHex(signed_trx.rawTransaction)[2:]}
    Utils.Print("Send balance again, with correct nonce")
    retValue = prodNode.pushMessage(evmAcc.name, "pushtx", json.dumps(actData), '-p evmevmevmevm', silentErrors=True, force=True)
    assert retValue[0], f"push trx should have succeeded: {retValue}"

    # incorrect chainid
    nonce += 1
    evmChainId = 8888
    signed_trx = w3.eth.account.sign_transaction(dict(
        nonce=nonce,
        #        maxFeePerGas=150000000000, #150 GWei
        gas=100000,       #100k Gas
        gasPrice=1,
        to=Web3.toChecksumAddress(toAdd),
        value=amount,
        data=b'',
        chainId=evmChainId
    ), evmSendKey)

    actData = {"ram_payer":"evmevmevmevm", "rlptx":Web3.toHex(signed_trx.rawTransaction)[2:]}
    Utils.Print("Send balance again, with invalid chainid")
    retValue = prodNode.pushMessage(evmAcc.name, "pushtx", json.dumps(actData), '-p evmevmevmevm', silentErrors=True, force=True)
    assert not retValue[0], f"push trx should have failed: {retValue}"

    # correct values for continuing
    nonce -= 1
    evmChainId = 15555

    Utils.Print("Simple Solidity contract")
    # // SPDX-License-Identifier: GPL-3.0
    # pragma solidity >=0.7.0 <0.9.0;
    # contract Storage {
    #     uint256 number;
    #     function store(uint256 num) public {
    #         number = num;
    #     }
    #     function retrieve() public view returns (uint256){
    #         return number;
    #     }
    # }
    retValue = prodNode.pushMessage(evmAcc.name, "updatecode", '{"address":"2787b98fc4e731d0456b3941f0b3fe2e01430000","incarnation":0,"code_hash":"286e3d498e2178afc91275f11d446e62a0d85b060ce66d6ca75f6ec9d874d560","code":"608060405234801561001057600080fd5b50600436106100365760003560e01c80632e64cec11461003b5780636057361d14610059575b600080fd5b610043610075565b60405161005091906100d9565b60405180910390f35b610073600480360381019061006e919061009d565b61007e565b005b60008054905090565b8060008190555050565b60008135905061009781610103565b92915050565b6000602082840312156100b3576100b26100fe565b5b60006100c184828501610088565b91505092915050565b6100d3816100f4565b82525050565b60006020820190506100ee60008301846100ca565b92915050565b6000819050919050565b600080fd5b61010c816100f4565b811461011757600080fd5b5056fea26469706673582212209a159a4f3847890f10bfb87871a61eba91c5dbf5ee3cf6398207e292eee22a1664736f6c63430008070033"}', '-p evmevmevmevm')

    testSuccessful=True

    generate_evm_transactions(nonce)

    testSuccessful=True
finally:
    TestHelper.shutdown(cluster, walletMgr, testSuccessful=testSuccessful, killEosInstances=killEosInstances, killWallet=killEosInstances, keepLogs=keepLogs, cleanRun=killAll, dumpErrorDetails=dumpErrorDetails)

exitCode = 0 if testSuccessful else 1
exit(exitCode)
