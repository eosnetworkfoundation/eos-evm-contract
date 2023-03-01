#!/usr/bin/env python3

import random
import os
import json
import time
import sys
import signal
import shutil
import calendar
from datetime import datetime
from ctypes import c_uint8

import sys
from binascii import unhexlify
from web3 import Web3

sys.path.append(os.getcwd())
sys.path.append(os.path.join(os.getcwd(), "tests"))

from TestHarness import Cluster, TestHelper, Utils, WalletMgr
from TestHarness.TestHelper import AppArgs
from TestHarness.testUtils import ReturnType
from core_symbol import CORE_SYMBOL


###############################################################
# nodeos_trust_evm_test
#
# Set up a TrustEVM env and run simple tests.
#
# Need to install:
#   web3      - pip install web3
#
# --turst-evm-root should point to the root of TrustEVM build dir
# --trust-evm-contract-root should point to root of TrustEVM contract build dir
#                           contracts should be built with -DWITH_TEST_ACTIONS=On
#
# Example:
#  cd ~/ext/leap/build
#  edit core_symbol.py to be EOS
#  ~/ext/TrustEVM/tests/leap/nodeos_trust_evm_test.py --trust-evm-contract-root ~/ext/TrustEVM/contract/build --trust-evm-build-root ~/ext/TrustEVM/cmake-build-release-gcc  --leave-running
#
#  Launches wallet at port: 9899
#    Example: bin/cleos --wallet-url http://127.0.0.1:9899 ...
#
###############################################################

Print=Utils.Print
errorExit=Utils.errorExit

appArgs=AppArgs()
appArgs.add(flag="--trust-evm-contract-root", type=str, help="TrustEVM contract build dir", default=None)
appArgs.add(flag="--trust-evm-build-root", type=str, help="TrustEVM build dir", default=None)
appArgs.add(flag="--genesis-json", type=str, help="File to save generated genesis json", default="trust-evm-genesis.json")

args=TestHelper.parse_args({"--keep-logs","--dump-error-details","-v","--leave-running","--clean-run" }, applicationSpecificArgs=appArgs)
debug=args.v
killEosInstances= not args.leave_running
dumpErrorDetails=args.dump_error_details
keepLogs=args.keep_logs
killAll=args.clean_run
trustEvmContractRoot=args.trust_evm_contract_root
trustEvmBuildRoot=args.trust_evm_build_root
gensisJson=args.genesis_json

assert trustEvmContractRoot is not None, "--trust-evm-contract-root is required"
assert trustEvmBuildRoot is not None, "--trust-evm-build-root is required"

szabo = 1000000000000
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


def makeReservedEvmAddress(account):
    bytearr = [0xff, 0xff, 0xff, 0xff,
               0xff, 0xff, 0xff, 0xff,
               0xff, 0xff, 0xff, 0xff,
               c_uint8(account >> 56).value,
               c_uint8(account >> 48).value,
               c_uint8(account >> 40).value,
               c_uint8(account >> 32).value,
               c_uint8(account >> 24).value,
               c_uint8(account >> 16).value,
               c_uint8(account >>  8).value,
               c_uint8(account >>  0).value]
    return "0x" + bytes(bytearr).hex()

def charToSymbol(c: str):
    assert len(c) == 1
    if c >= 'a' and c <= 'z':
        return ord(c) - ord('a') + 6
    if c >= '1' and c <= '5':
        return ord(c) - ord('1') + 1
    return 0

def nameStrToInt(s: str):
    n = 0
    for i, c in enumerate(s):
        if i >= 12:
            break
        n |= (charToSymbol(c) & 0x1f) << (64 - (5 * (i + 1)))
        i = i + 1

    assert i > 0
    if i < len(s) and i == 12:
        n |= charToSymbol(s[12]) & 0x0F
    return n

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

    accounts=cluster.createAccountKeys(2)
    if accounts is None:
        Utils.errorExit("FAILURE - create keys")

    evmAcc = accounts[0]
    evmAcc.name = "evmevmevmevm"
    testAcc = accounts[1]

    testWalletName="test"

    Print("Creating wallet \"%s\"." % (testWalletName))
    testWallet=walletMgr.create(testWalletName, [cluster.eosioAccount,accounts[0],accounts[1]])

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

    genesis_info = {
        "alloc": {},
        "coinbase": "0x0000000000000000000000000000000000000000",
        "config": {
            "chainId": 15555,
            "homesteadBlock": 0,
            "eip150Block": 0,
            "eip155Block": 0,
            "byzantiumBlock": 0,
            "constantinopleBlock": 0,
            "petersburgBlock": 0,
            "istanbulBlock": 0,
            "trust": {}
        },
        "difficulty": "0x01",
        "extraData": "TrustEVM",
        "gasLimit": "0x7ffffffffff",
        "mixHash": "0x"+block["id"],
        "nonce": hex(1000),
        "timestamp": hex(int(calendar.timegm(datetime.strptime(block["timestamp"].split(".")[0], '%Y-%m-%dT%H:%M:%S').timetuple())))
    }

    # add eosio.code permission
    cmd="set account permission evmevmevmevm active --add-code -p evmevmevmevm@active"
    prodNode.processCleosCmd(cmd, cmd, silentErrors=True, returnType=ReturnType.raw)

    Utils.Print("Set balance")
    addys = {
        "0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266":"0x038318535b54105d4a7aae60c08fc45f9687181b4fdfc625bd1a753fa7397fed75,0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80"
    }

    for i,k in enumerate(addys):
        print("addys: [{0}] [{1}] [{2}]".format(i,k[2:].lower(), len(k[2:])))
        trans = prodNode.pushMessage(evmAcc.name, "setbal", '{"addy":"' + k[2:].lower() + '", "bal":"0000000000000000000000000000000000100000000000000000000000000000"}', '-p evmevmevmevm')
        genesis_info["alloc"][k.lower()] = {"balance":"0x100000000000000000000000000000"}
        if not (i+1) % 20: time.sleep(1)
    prodNode.waitForTransBlockIfNeeded(trans[1], True)

    Utils.Print("Send balance")
    evmChainId = 15555
    fromAdd = "f39Fd6e51aad88F6F4ce6aB8827279cffFb92266"
    toAdd = '0x9edf022004846bc987799d552d1b8485b317b7ed'
    amount = 100
    nonce = 0
    evmSendKey = "ac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80"

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

    generate_evm_transactions(nonce)

    if gensisJson[0] != '/': gensisJson = os.path.realpath(gensisJson)
    f=open(gensisJson,"w")
    f.write(json.dumps(genesis_info))
    f.close()

    Utils.Print("#####################################################")
    Utils.Print("Generated EVM json genesis file in: %s" % gensisJson)
    Utils.Print("")
    Utils.Print("You can now run:")
    Utils.Print("  trustevm-node --plugin=blockchain_plugin --ship-endpoint=127.0.0.1:8999 --genesis-json=%s --chain-data=/tmp/data --verbosity=5" % gensisJson)
    Utils.Print("  trustevm-rpc --trust-evm-node=127.0.0.1:8080 --http-port=0.0.0.0:8881 --chaindata=/tmp/data --api-spec=eth,debug,net,trace")
    Utils.Print("")

    #
    # Test EOS/EVM Bridge
    #
    Utils.Print("Test EOS/EVM Bridge")

    # Verify starting values
    expectedAmount="60000000.0000 {0}".format(CORE_SYMBOL)
    evmAccAcutalAmount=prodNode.getAccountEosBalanceStr(evmAcc.name)
    testAccActualAmount=prodNode.getAccountEosBalanceStr(testAcc.name)
    Utils.Print("\tAccount balances: evm %s, test %s", (evmAccAcutalAmount,testAccActualAmount))
    if expectedAmount != evmAccAcutalAmount or expectedAmount != testAccActualAmount:
        Utils.errorExit("Unexpected starting conditions. Excepted %s, evm actual: %s, test actual" % (expectedAmount, evmAccAcutalAmount, testAccActualAmount))

    # set ingress bridge fee
    data="[\"0.0100 EOS\"]"
    opts="--permission evmevmevmevm@active"
    trans=prodNode.pushMessage("evmevmevmevm", "setingressfee", data, opts)

    rows=prodNode.getTable(evmAcc.name, evmAcc.name, "balances")
    Utils.Print("\tBefore transfer table rows:", rows)

    # EOS -> EVM
    transferAmount="97.5321 {0}".format(CORE_SYMBOL)
    Print("Transfer funds %s from account %s to %s" % (transferAmount, testAcc.name, evmAcc.name))
    prodNode.transferFunds(testAcc, evmAcc, transferAmount, "0xF0cE7BaB13C99bA0565f426508a7CD8f4C247E5a", waitForTransBlock=True)

    row0=prodNode.getTableRow(evmAcc.name, evmAcc.name, "balances", 0)
    Utils.Print("\tAfter transfer table row:", row0)
    assert(row0["balance"]["balance"] == "0.0100 {0}".format(CORE_SYMBOL)) # should have fee at end of transaction

    evmAccAcutalAmount=prodNode.getAccountEosBalanceStr(evmAcc.name)
    Utils.Print("\tEVM Account balance %s", evmAccAcutalAmount)
    expectedAmount="60000097.5321 {0}".format(CORE_SYMBOL)
    if expectedAmount != evmAccAcutalAmount:
        Utils.errorExit("Transfer verification failed. Excepted %s, actual: %s" % (expectedAmount, evmAccAcutalAmount))

    row4=prodNode.getTableRow(evmAcc.name, evmAcc.name, "account", 4) # 4th balance of this integration test
    assert(row4["eth_address"] == "f0ce7bab13c99ba0565f426508a7cd8f4c247e5a")
    assert(row4["balance"] == "000000000000000000000000000000000000000000000005496419417a1f4000") # 0x5496419417a1f4000 => 97522100000000000000 (97.5321 - 0.0100)

    # EOS -> EVM to the same address
    transferAmount="10.0000 {0}".format(CORE_SYMBOL)
    Print("Transfer funds %s from account %s to %s" % (transferAmount, testAcc.name, evmAcc.name))
    prodNode.transferFunds(testAcc, evmAcc, transferAmount, "0xF0cE7BaB13C99bA0565f426508a7CD8f4C247E5a", waitForTransBlock=True)

    row0=prodNode.getTableRow(evmAcc.name, evmAcc.name, "balances", 0)
    Utils.Print("\tAfter transfer table row:", row0)
    assert(row0["balance"]["balance"] == "0.0200 {0}".format(CORE_SYMBOL)) # should have fee from both transfers

    evmAccAcutalAmount=prodNode.getAccountEosBalanceStr(evmAcc.name)
    Utils.Print("\tEVM Account balance %s", evmAccAcutalAmount)
    expectedAmount="60000107.5321 {0}".format(CORE_SYMBOL)
    if expectedAmount != evmAccAcutalAmount:
        Utils.errorExit("Transfer verification failed. Excepted %s, actual: %s" % (expectedAmount, evmAccAcutalAmount))

    row4=prodNode.getTableRow(evmAcc.name, evmAcc.name, "account", 4) # 4th balance of this integration test
    assert(row4["eth_address"] == "f0ce7bab13c99ba0565f426508a7cd8f4c247e5a")
    assert(row4["balance"] == "000000000000000000000000000000000000000000000005d407b55394464000") # 0x5d407b55394464000 => 107512100000000000000 (97.5321 + 10.000 - 0.0100 - 0.0100)

    # EOS -> EVM to diff address
    transferAmount="42.4242 {0}".format(CORE_SYMBOL)
    Print("Transfer funds %s from account %s to %s" % (transferAmount, testAcc.name, evmAcc.name))
    prodNode.transferFunds(testAcc, evmAcc, transferAmount, "0x9E126C57330FA71556628e0aabd6B6B6783d99fA", waitForTransBlock=True)

    row0=prodNode.getTableRow(evmAcc.name, evmAcc.name, "balances", 0)
    Utils.Print("\tAfter transfer table row:", row0)
    assert(row0["balance"]["balance"] == "0.0300 {0}".format(CORE_SYMBOL)) # should have fee from all three transfers

    evmAccAcutalAmount=prodNode.getAccountEosBalanceStr(evmAcc.name)
    Utils.Print("\tEVM Account balance %s", evmAccAcutalAmount)
    expectedAmount="60000149.9563 {0}".format(CORE_SYMBOL)
    if expectedAmount != evmAccAcutalAmount:
        Utils.errorExit("Transfer verification failed. Excepted %s, actual: %s" % (expectedAmount, evmAccAcutalAmount))

    row5=prodNode.getTableRow(evmAcc.name, evmAcc.name, "account", 5) # 5th balance of this integration test
    assert(row5["eth_address"] == "9e126c57330fa71556628e0aabd6b6b6783d99fa")
    assert(row5["balance"] == "0000000000000000000000000000000000000000000000024c9d822e105f8000") # 0x24c9d822e105f8000 => 42414200000000000000 (42.4242 - 0.0100)

    # EVM -> EOS
    #   0x9E126C57330FA71556628e0aabd6B6B6783d99fA private key: 0xba8c9ff38e4179748925335a9891b969214b37dc3723a1754b8b849d3eea9ac0
    toAdd = makeReservedEvmAddress(nameStrToInt(testAcc.name))
    evmSendKey = "ba8c9ff38e4179748925335a9891b969214b37dc3723a1754b8b849d3eea9ac0"
    amount=13.1313 # 13.131313
    nonce = 0
    signed_trx = w3.eth.account.sign_transaction(dict(
        nonce=nonce,
        #        maxFeePerGas=150000000000, #150 GWei
        gas=100000,       #100k Gas
        gasPrice=1,
        to=Web3.toChecksumAddress(toAdd),
        value=int(amount*10000*szabo*100),
        data=b'',
        chainId=evmChainId
    ), evmSendKey)

    actData = {"ram_payer":"evmevmevmevm", "rlptx":Web3.toHex(signed_trx.rawTransaction)[2:]}
    Utils.Print("Send EVM->EOS")
    trans = prodNode.pushMessage(evmAcc.name, "pushtx", json.dumps(actData), '-p evmevmevmevm', silentErrors=True, force=True)
    prodNode.waitForTransBlockIfNeeded(trans[1], True)

    row5=prodNode.getTableRow(evmAcc.name, evmAcc.name, "account", 5) # 5th balance of this integration test
    Utils.Print("row5: ", row5)
    assert(row5["eth_address"] == "9e126c57330fa71556628e0aabd6b6b6783d99fa")
    assert(row5["balance"] == "0000000000000000000000000000000000000000000000019661c2670d48edf8") # 0x19661c2670d48edf8 => 29282899999999979000 (42.4242 - 0.0100 - 13.131313 - gas 21000wei)

    # Launch trustevm-node & trustevm-rpc
    dataDir = Utils.DataDir + "/eos_evm"
    nodeStdOutDir = dataDir + "/trustevm-node.stdout"
    nodeStdErrDir = dataDir + "/trustevm-node.stderr"
    nodeRPCStdOutDir = dataDir + "/trustevm-rpc.stdout"
    nodeRPCStdErrDir = dataDir + "/trustevm-rpc.stderr"
    shutil.rmtree(dataDir, ignore_errors=True)
    os.makedirs(dataDir)
    outFile = open(nodeStdOutDir, "w")
    errFile = open(nodeStdErrDir, "w")
    outRPCFile = open(nodeRPCStdOutDir, "w")
    errRPCFile = open(nodeRPCStdErrDir, "w")
    cmd = "%s/cmd/trustevm-node --plugin=blockchain_plugin --ship-endpoint=127.0.0.1:8999 --genesis-json=%s --chain-data=%s --verbosity=5 --nocolor=1" % (trustEvmBuildRoot, gensisJson, dataDir)
    Utils.Print("Launching: %s", cmd)
    popen=Utils.delayedCheckOutput(cmd, stdout=outFile, stderr=errFile)

    cmdRPC = "%s/cmd/trustevm-rpc --trust-evm-node=127.0.0.1:8080 --http-port=0.0.0.0:8881 --chaindata=%s --api-spec=eth,debug,net,trace" % (trustEvmBuildRoot, dataDir)
    Utils.Print("Launching: %s", cmdRPC)
    popenRPC=Utils.delayedCheckOutput(cmdRPC, stdout=outRPCFile, stderr=errRPCFile)

    time.sleep(5) # allow time to sync trxs

    Utils.Print("\n")
    foundErr = False
    stdErrFile = open(nodeStdErrDir, "r")
    lines = stdErrFile.readlines()
    for line in lines:
        if line.find("ERROR") != -1 or line.find("CRIT") != -1:
            Utils.Print("  Found ERROR in trustevm log: ", line)
            foundErr = True

    if killEosInstances:
        popenRPC.kill()
        popen.kill()

    testSuccessful= not foundErr
finally:
    TestHelper.shutdown(cluster, walletMgr, testSuccessful=testSuccessful, killEosInstances=killEosInstances, killWallet=killEosInstances, keepLogs=keepLogs, cleanRun=killAll, dumpErrorDetails=dumpErrorDetails)

exitCode = 0 if testSuccessful else 1
exit(exitCode)
