#!/usr/bin/env python3
import random
import os
import sys
import json
import time
import signal
import calendar
from datetime import datetime

from flask import Flask, request, jsonify
from flask_cors import CORS
from eth_hash.auto import keccak
import requests
import json

from binascii import unhexlify

sys.path.append(os.getcwd())
sys.path.append(os.path.join(os.getcwd(), "tests"))

from TestHarness import Cluster, TestHelper, Utils, WalletMgr
from TestHarness.TestHelper import AppArgs
from TestHarness.testUtils import ReturnType
from core_symbol import CORE_SYMBOL

from antelope_name import convert_name_to_value

###############################################################
# nodeos_eos_evm_server
#
# Set up a EOS EVM env
#
# This test sets up 2 producing nodes and one "bridge" node using test_control_api_plugin.
#   One producing node has 3 of the elected producers and the other has 1 of the elected producers.
#   All the producers are named in alphabetical order, so that the 3 producers, in the one production node, are
#       scheduled first, followed by the 1 producer in the other producer node. Each producing node is only connected
#       to the other producing node via the "bridge" node.
#   The bridge node has the test_control_api_plugin, which exposes a restful interface that the test script uses to kill
#       the "bridge" node when /fork endpoint called.
#
# --eos-evm-contract-root should point to root of EOS EVM Contract build dir
# --genesis-json file to save generated EVM genesis json
# --read-endpoint eos-evm-rpc endpoint (read endpoint)
#
# Example:
#  cd ~/ext/leap/build
#  ~/ext/eos-evm/tests/leap/nodeos_eos_evm_server.py --eos-evm-contract-root ~/ext/eos-evm/contract/build --leave-running
#
#  Launches wallet at port: 9899
#    Example: bin/cleos --wallet-url http://127.0.0.1:9899 ...
#
#  Sets up endpoint on port 5000
#    /            - for req['method'] == "eth_sendRawTransaction"
#    /fork        - create forked chain, does not return until a fork has started
#    /restore     - resolve fork and stabilize chain
#
# Dependencies:
#    pip install eth-hash requests flask flask-cors
###############################################################

Print=Utils.Print
errorExit=Utils.errorExit

appArgs=AppArgs()
appArgs.add(flag="--eos-evm-contract-root", type=str, help="EOS EVM Contract build dir", default=None)
appArgs.add(flag="--genesis-json", type=str, help="File to save generated genesis json", default="eos-evm-genesis.json")
appArgs.add(flag="--read-endpoint", type=str, help="EVM read endpoint (eos-evm-rpc)", default="http://localhost:8881")

args=TestHelper.parse_args({"--keep-logs","--dump-error-details","-v","--leave-running","--clean-run" }, applicationSpecificArgs=appArgs)
debug=args.v
killEosInstances= not args.leave_running
dumpErrorDetails=args.dump_error_details
keepLogs=args.keep_logs
killAll=args.clean_run
eosEvmContractRoot=args.eos_evm_contract_root
gensisJson=args.genesis_json
readEndpoint=args.read_endpoint

assert eosEvmContractRoot is not None, "--eos-evm-contract-root is required"

totalProducerNodes=2
totalNonProducerNodes=1
totalNodes=totalProducerNodes+totalNonProducerNodes
maxActiveProducers=21
totalProducers=maxActiveProducers

seed=1
Utils.Debug=debug
testSuccessful=False

random.seed(seed) # Use a fixed seed for repeatability.
cluster=Cluster(walletd=True)
walletMgr=WalletMgr(True)


try:
    TestHelper.printSystemInfo("BEGIN")

    cluster.setWalletMgr(walletMgr)
    cluster.killall(allInstances=killAll)
    cluster.cleanup()
    walletMgr.killall(allInstances=killAll)
    walletMgr.cleanup()

    # ***   setup topogrophy   ***

    # "bridge" shape connects defprocera through defproducerc (3 in node0) to each other and defproduceru (1 in node01)
    # and the only connection between those 2 groups is through the bridge node

    specificExtraNodeosArgs={}
    # Connect SHiP to node01 so it will switch forks as they are resolved
    specificExtraNodeosArgs[1]="--plugin eosio::state_history_plugin --state-history-endpoint 127.0.0.1:8999 --trace-history --chain-state-history --disable-replay-opts  "
    # producer nodes will be mapped to 0 through totalProducerNodes-1, so the number totalProducerNodes will be the non-producing node
    specificExtraNodeosArgs[totalProducerNodes]="--plugin eosio::test_control_api_plugin  "
    extraNodeosArgs="--contracts-console"

    Print("Stand up cluster")
    if cluster.launch(topo="bridge", pnodes=totalProducerNodes,
                      totalNodes=totalNodes, totalProducers=totalProducers,
                      useBiosBootFile=False, extraNodeosArgs=extraNodeosArgs, specificExtraNodeosArgs=specificExtraNodeosArgs) is False:
        Utils.cmdError("launcher")
        Utils.errorExit("Failed to stand up eos cluster.")

    Print("Validating system accounts after bootstrap")
    cluster.validateAccounts(None)

    Print ("Wait for Cluster stabilization")
    # wait for cluster to start producing blocks
    if not cluster.waitOnClusterBlockNumSync(3):
        errorExit("Cluster never stabilized")
    Print ("Cluster stabilized")

    prodNode = cluster.getNode(0)
    prodNode0 = prodNode
    prodNode1 = cluster.getNode(1)
    nonProdNode = cluster.getNode(2)

    accounts=cluster.createAccountKeys(6)
    if accounts is None:
        Utils.errorExit("FAILURE - create keys")

    evmAcc = accounts[0]
    evmAcc.name = "evmevmevmevm"
    evmAcc.activePrivateKey="5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"
    evmAcc.activePublicKey="EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV"
    accounts[1].name="tester111111" # needed for voting
    accounts[2].name="tester222222" # needed for voting
    accounts[3].name="tester333333" # needed for voting
    accounts[4].name="tester444444" # needed for voting
    accounts[5].name="tester555555" # needed for voting

    testWalletName="test"

    Print("Creating wallet \"%s\"." % (testWalletName))
    testWallet=walletMgr.create(testWalletName, [cluster.eosioAccount,accounts[0],accounts[1],accounts[2],accounts[3],accounts[4],accounts[5]])

    for _, account in cluster.defProducerAccounts.items():
        walletMgr.importKey(account, testWallet, ignoreDupKeyWarning=True)

    for i in range(0, totalNodes):
        node=cluster.getNode(i)
        node.producers=Cluster.parseProducers(i)
        numProducers=len(node.producers)
        for prod in node.producers:
            prodName = cluster.defProducerAccounts[prod].name
            if prodName == "defproducera" or prodName == "defproducerb" or prodName == "defproducerc" or prodName == "defproduceru":
                Print("Register producer %s" % cluster.defProducerAccounts[prod].name)
                trans=node.regproducer(cluster.defProducerAccounts[prod], "http::/mysite.com", 0, waitForTransBlock=False, exitOnError=True)

    # create accounts via eosio as otherwise a bid is needed
    for account in accounts:
        Print("Create new account %s via %s with private key: %s" % (account.name, cluster.eosioAccount.name, account.activePrivateKey))
        trans=nonProdNode.createInitializeAccount(account, cluster.eosioAccount, stakedDeposit=0, waitForTransBlock=True, stakeNet=10000, stakeCPU=10000, buyRAM=10000000, exitOnError=True)
        #   max supply 1000000000.0000 (1 Billion)
        transferAmount="100000000.0000 {0}".format(CORE_SYMBOL)
        Print("Transfer funds %s from account %s to %s" % (transferAmount, cluster.eosioAccount.name, account.name))
        nonProdNode.transferFunds(cluster.eosioAccount, account, transferAmount, "test transfer", waitForTransBlock=True)
        trans=nonProdNode.delegatebw(account, 20000000.0000, 20000000.0000, waitForTransBlock=False, exitOnError=True)

    # ***   vote using accounts   ***

    cluster.waitOnClusterSync(blockAdvancing=3)

    # vote a,b,c  u
    voteProducers=[]
    voteProducers.append("defproducera")
    voteProducers.append("defproducerb")
    voteProducers.append("defproducerc")
    voteProducers.append("defproduceru")
    for account in accounts:
        Print("Account %s vote for producers=%s" % (account.name, voteProducers))
        trans=prodNode.vote(account, voteProducers, exitOnError=True, waitForTransBlock=False)

    #verify nodes are in sync and advancing
    cluster.waitOnClusterSync(blockAdvancing=3)
    Print("Shutdown unneeded bios node")
    cluster.biosNode.kill(signal.SIGTERM)

    # setup evm

    contractDir=eosEvmContractRoot + "/evm_runtime"
    wasmFile="evm_runtime.wasm"
    abiFile="evm_runtime.abi"
    Utils.Print("Publish evm_runtime contract")
    prodNode.publishContract(evmAcc, contractDir, wasmFile, abiFile, waitForTransBlock=True)

    # add eosio.code permission
    cmd="set account permission evmevmevmevm active --add-code -p evmevmevmevm@active"
    prodNode.processCleosCmd(cmd, cmd, silentErrors=True, returnType=ReturnType.raw)

    trans = prodNode.pushMessage(evmAcc.name, "init", '{"chainid":15555, "fee_params": {"gas_price": "150000000000", "miner_cut": 10000, "ingress_bridge_fee": null}}', '-p evmevmevmevm')

    prodNode.waitForTransBlockIfNeeded(trans[1], True)

    transId=prodNode.getTransId(trans[1])
    blockNum = prodNode.getBlockNumByTransId(transId)
    block = prodNode.getBlock(blockNum)
    Utils.Print("Block Id: ", block["id"])
    Utils.Print("Block timestamp: ", block["timestamp"])

    genesis_info = {
        "alloc": {
            "0x0000000000000000000000000000000000000000" : {"balance":"0x00"}
        },
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
        "extraData": "EOSEVM",
        "gasLimit": "0x7ffffffffff",
        "mixHash": "0x"+block["id"],
        "nonce": f'{convert_name_to_value(evmAcc.name):#0x}',
        "timestamp": hex(int(calendar.timegm(datetime.strptime(block["timestamp"].split(".")[0], '%Y-%m-%dT%H:%M:%S').timetuple())))
    }

    Utils.Print("Send small balance to special balance to allow the bridge to work")
    transferAmount="1.0000 {0}".format(CORE_SYMBOL)
    Print("Transfer funds %s from account %s to %s" % (transferAmount, cluster.eosioAccount.name, evmAcc.name))
    nonProdNode.transferFunds(cluster.eosioAccount, evmAcc, transferAmount, evmAcc.name, waitForTransBlock=True)

    # accounts: {
    #    mnemonic: "test test test test test test test test test test test junk",
    #    path: "m/44'/60'/0'/0",
    #    initialIndex: 0,
    #    count: 20,
    #    passphrase: "",
    # }

    addys = {
        "0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266":"0x038318535b54105d4a7aae60c08fc45f9687181b4fdfc625bd1a753fa7397fed75,0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80",
        "0x70997970C51812dc3A010C7d01b50e0d17dc79C8":"0x02ba5734d8f7091719471e7f7ed6b9df170dc70cc661ca05e688601ad984f068b0,0x59c6995e998f97a5a0044966f0945389dc9e86dae88c7a8412f4603b6b78690d",
        "0x3C44CdDdB6a900fa2b585dd299e03d12FA4293BC":"0x039d9031e97dd78ff8c15aa86939de9b1e791066a0224e331bc962a2099a7b1f04,0x5de4111afa1a4b94908f83103eb1f1706367c2e68ca870fc3fb9a804cdab365a",
        "0x90F79bf6EB2c4f870365E785982E1f101E93b906":"0x0220b871f3ced029e14472ec4ebc3c0448164942b123aa6af91a3386c1c403e0eb,0x7c852118294e51e653712a81e05800f419141751be58f605c371e15141b007a6",
        "0x15d34AAf54267DB7D7c367839AAf71A00a2C6A65":"0x03bf6ee64a8d2fdc551ec8bb9ef862ef6b4bcb1805cdc520c3aa5866c0575fd3b5,0x47e179ec197488593b187f80a00eb0da91f1b9d0b13f8733639f19c30a34926a",
        "0x9965507D1a55bcC2695C58ba16FB37d819B0A4dc":"0x0337b84de6947b243626cc8b977bb1f1632610614842468dfa8f35dcbbc55a515e,0x8b3a350cf5c34c9194ca85829a2df0ec3153be0318b5e2d3348e872092edffba",
        "0x976EA74026E726554dB657fA54763abd0C3a0aa9":"0x029a4ab212cb92775d227af4237c20b81f4221e9361d29007dfc16c79186b577cb,0x92db14e403b83dfe3df233f83dfa3a0d7096f21ca9b0d6d6b8d88b2b4ec1564e",
        "0x14dC79964da2C08b23698B3D3cc7Ca32193d9955":"0x0201f2bf1fa920e77a43c7aec2587d0b3814093420cc59a9b3ad66dd5734dda7be,0x4bbbf85ce3377467afe5d46f804f221813b2bb87f24d81f60f1fcdbf7cbf4356",
        "0x23618e81E3f5cdF7f54C3d65f7FBc0aBf5B21E8f":"0x03931e7fda8da226f799f791eefc9afebcd7ae2b1b19a03c5eaa8d72122d9fe74d,0xdbda1821b80551c9d65939329250298aa3472ba22feea921c0cf5d620ea67b97",
        "0xa0Ee7A142d267C1f36714E4a8F75612F20a79720":"0x023255458e24278e31d5940f304b16300fdff3f6efd3e2a030b5818310ac67af45,0x2a871d0798f97d79848a013d4936a73bf4cc922c825d33c1cf7073dff6d409c6",
        "0xBcd4042DE499D14e55001CcbB24a551F3b954096":"0x030bb316cf4dbaeff8df7c6a8f3c55a11c72f7f2f8d79c274f27cdce2220f36371,0xf214f2b2cd398c806f84e317254e0f0b801d0643303237d97a22a48e01628897",
        "0x71bE63f3384f5fb98995898A86B02Fb2426c5788":"0x02e393c954d127d79d56b594a46df6b2e053f49446759eac612dbe12ade3095c67,0x701b615bbdfb9de65240bc28bd21bbc0d996645a3dd57e7b12bc2bdf6f192c82",
        "0xFABB0ac9d68B0B445fB7357272Ff202C5651694a":"0x02463e7db0f9c35ba7ae68a8098f1024019b90191281276eeef294acb3f1354b0a,0xa267530f49f8280200edf313ee7af6b827f2a8bce2897751d06a843f644967b1",
        "0x1CBd3b2770909D4e10f157cABC84C7264073C9Ec":"0x037805be0fc5186c4437306b531c8e981d3922e5fc81d72d527931b995445fb78e,0x47c99abed3324a2707c28affff1267e45918ec8c3f20b8aa892e8b065d2942dd",
        "0xdF3e18d64BC6A983f673Ab319CCaE4f1a57C7097":"0x03407a862c69cbc66dceea40079757697abcf931043f5a5f128a56ae6e51bdbce7,0xc526ee95bf44d8fc405a158bb884d9d1238d99f0612e9f33d006bb0789009aaa",
        "0xcd3B766CCDd6AE721141F452C550Ca635964ce71":"0x02d5ab34891bfe989bfa557f983cb5d031c4acc845ad990baaefb58ca1db0e7716,0x8166f546bab6da521a8369cab06c5d2b9e46670292d85c875ee9ec20e84ffb61",
        "0x2546BcD3c84621e976D8185a91A922aE77ECEc30":"0x0364c1c85d9aa8081a8ef94c35379fa7532942b2d8cbbd1e3ea71c0e3609b96cc0,0xea6c44ac03bff858b476bba40716402b03e41b8e97e276d1baec7c37d42484a0",
        "0xbDA5747bFD65F08deb54cb465eB87D40e51B197E":"0x02c216848622dfc38a2ad2a921f524103cf654a22b8679736ecedc4901453ea3f7,0x689af8efa8c651a91ad287602527f3af2fe9f6501a7ac4b061667b5a93e037fd",
        "0xdD2FD4581271e230360230F9337D5c0430Bf44C0":"0x02302a94bd084ff317493db7c2fe07e0935c0f6d3e6772d6af3c58e92abebfb402,0xde9be858da4a475276426320d5e9262ecfc3ba460bfac56360bfa6c4c28b4ee0",
        "0x8626f6940E2eb28930eFb4CeF49B2d1F2C9C1199":"0x027bf824b28c4bf11ce553fa746a18754949ab4959e2ea73465778d14179211f8c,0xdf57089febbacf7ba0bc227dafbffa9fc08a93fdc68e1e42411a14efcf23656e",
        "0x09DB0a93B389bEF724429898f539AEB7ac2Dd55f":"0x02ecc9c3f6303fddcd44f5c9ecaa225eafaa18b8464a022389a4c4be474e4557a9,0xeaa861a9a01391ed3d587d8a5a84ca56ee277629a8b02c22093a419bf240e65d",
        "0x02484cb50AAC86Eae85610D6f4Bf026f30f6627D":"0x031a7044a0e2549a114c92dfa83a25bfc17f4b7c2db4a7e7e48d35ed67c285de3b,0xc511b2aa70776d4ff1d376e8537903dae36896132c90b91d52c1dfbae267cd8b",
        "0x08135Da0A343E492FA2d4282F2AE34c6c5CC1BbE":"0x02abca850bc9472d92402203e9cbafc2e8dedb19f1103911a62b11ba2a4370e635,0x224b7eb7449992aac96d631d9677f7bf5888245eef6d6eeda31e62d2f29a83e4",
        "0x5E661B79FE2D3F6cE70F5AAC07d8Cd9abb2743F1":"0x02092a3ba818f25a71a947d3daa7f9d501c3f18296057c928bc5feffa8b61d54f3,0x4624e0802698b9769f5bdb260a3777fbd4941ad2901f5966b854f953497eec1b",
        "0x61097BA76cD906d2ba4FD106E757f7Eb455fc295":"0x030b1d683d363d0704f04e4fb59a33b6e22bd0187303f9cce881d0809d3a535f28,0x375ad145df13ed97f8ca8e27bb21ebf2a3819e9e0a06509a812db377e533def7",
        "0xDf37F81dAAD2b0327A0A50003740e1C935C70913":"0x02da995e8954b8c4681db2986bc3004f403c8f5600d1d33a949a63045c50abf41d,0x18743e59419b01d1d846d97ea070b5a3368a3e7f6f0242cf497e1baac6972427",
        "0x553BC17A05702530097c3677091C5BB47a3a7931":"0x027bc620a41c572668c4d7b508e020cd8f4e9e50074016c461370e7995838b5dbd,0xe383b226df7c8282489889170b0f68f66af6459261f4833a781acd0804fafe7a",
        "0x87BdCE72c06C21cd96219BD8521bDF1F42C78b5e":"0x0335aafbd21e02e958638c90c562dc048df89452b743dd7c3c7eadb8a223925895,0xf3a6b71b94f5cd909fb2dbb287da47badaa6d8bcdc45d595e2884835d8749001",
        "0x40Fc963A729c542424cD800349a7E4Ecc4896624":"0x032cd2af66ea588191488e7a647190584a3999630e2db8d5abba0ac290fbd291ab,0x4e249d317253b9641e477aba8dd5d8f1f7cf5250a5acadd1229693e262720a19",
        "0x9DCCe783B6464611f38631e6C851bf441907c710":"0x0329fdd393d57616eafdf67ebb6272a8ddb2ebf1eaa9b95b9e0702de79bf90da02,0x233c86e887ac435d7f7dc64979d7758d69320906a0d340d2b6518b0fd20aa998",
        "0x1BcB8e569EedAb4668e55145Cfeaf190902d3CF2":"0x02a3b615dcceb0b919884b12fed0d3125c5324dd01e95fd0e5dfbee1fc51cbbe15,0x85a74ca11529e215137ccffd9c95b2c72c5fb0295c973eb21032e823329b3d2d",
        "0x8263Fce86B1b78F95Ab4dae11907d8AF88f841e7":"0x02352c8ff139eb4b92f0b24d60e434c365ee202acc25aaa4f438852fce49eb41c8,0xac8698a440d33b866b6ffe8775621ce1a4e6ebd04ab7980deb97b3d997fc64fb",
        "0xcF2d5b3cBb4D7bF04e3F7bFa8e27081B52191f91":"0x02eb8d75ab6953483cb4df4fae49e0199a220b3cd204852960e55e1f1dbb3cfa21,0xf076539fbce50f0513c488f32bf81524d30ca7a29f400d68378cc5b1b17bc8f2",
        "0x86c53Eb85D0B7548fea5C4B4F82b4205C8f6Ac18":"0x0353db5e250d783f4368322de94bab64276c80a7284b9db0eae18bcae1f3eb0e28,0x5544b8b2010dbdbef382d254802d856629156aba578f453a76af01b81a80104e",
        "0x1aac82773CB722166D7dA0d5b0FA35B0307dD99D":"0x02e749e4fb83ae7afc7d19acdfc6a97ec5ae677019decf2c46758096a679021c6c,0x47003709a0a9a4431899d4e014c1fd01c5aad19e873172538a02370a119bae11",
        "0x2f4f06d218E426344CFE1A83D53dAd806994D325":"0x022a04ffd79e7334daa2724d9227d11ecf8124f0f2e2ae82b78cbfba14bca47577,0x9644b39377553a920edc79a275f45fa5399cbcf030972f771d0bca8097f9aad3",
        "0x1003ff39d25F2Ab16dBCc18EcE05a9B6154f65F4":"0x023f1e0c738f9604febf39304f7a8c918da05a7d0fc574cca239893b1f24e10639,0xcaa7b4a2d30d1d565716199f068f69ba5df586cf32ce396744858924fdf827f0",
        "0x9eAF5590f2c84912A08de97FA28d0529361Deb9E":"0x0354d2c78f37e8efe3014735fcca95cabae6a1e7901262a3f4e71355b1b406e555,0xfc5a028670e1b6381ea876dd444d3faaee96cffae6db8d93ca6141130259247c",
        "0x11e8F3eA3C6FcF12EcfF2722d75CEFC539c51a1C":"0x023e21c805452630d51c2116c0be5d78586f09f0239e5e0c78c1301bca129dd60a,0x5b92c5fe82d4fabee0bc6d95b4b8a3f9680a0ed7801f631035528f32c9eb2ad5",
        "0x7D86687F980A56b832e9378952B738b614A99dc6":"0x03cae5771b32d2b0639db908805738158abd3943623f23eaf875cc7e7eaefa9662,0xb68ac4aa2137dd31fd0732436d8e59e959bb62b4db2e6107b15f594caf0f405f",
        "0x9eF6c02FB2ECc446146E05F1fF687a788a8BF76d":"0x036fa7e0bea88907ab4753030c9e2bde73f1740ade3e134eba7414729be25eddbf,0xc95eaed402c8bd203ba04d81b35509f17d0719e3f71f40061a2ec2889bc4caa7",
        "0x08A2DE6F3528319123b25935C92888B16db8913E":"0x033f52ffb1962b253b7b62f5a3a6e68b71f25fa8f5e3eeeeeb666c3fb73eac90f2,0x55afe0ab59c1f7bbd00d5531ddb834c3c0d289a4ff8f318e498cb3f004db0b53",
        "0xe141C82D99D85098e03E1a1cC1CdE676556fDdE0":"0x0389b4c6ccde6226f5efdaad52831601004334d110e5eceb8060143a8adf234a12,0xc3f9b30f83d660231203f8395762fa4257fa7db32039f739630f87b8836552cc",
        "0x4b23D303D9e3719D6CDf8d172Ea030F80509ea15":"0x039141c8858729dd0c6849821158bce66f986cc07c8f02915798b559b0bfd54d4f,0x3db34a7bcc6424e7eadb8e290ce6b3e1423c6e3ef482dd890a812cd3c12bbede",
        "0xC004e69C5C04A223463Ff32042dd36DabF63A25a":"0x03a4c793122ddb400993b158b5fc2c49537892aca278a78800d901ced771af1b49,0xae2daaa1ce8a70e510243a77187d2bc8da63f0186074e4a4e3a7bfae7fa0d639",
        "0x5eb15C0992734B5e77c888D713b4FC67b3D679A2":"0x02a634ff1801b05722f7c5d1f90c49a9647bbb1687ff78f6def5aa37b3fe99e4f3,0x5ea5c783b615eb12be1afd2bdd9d96fae56dda0efe894da77286501fd56bac64",
        "0x7Ebb637fd68c523613bE51aad27C35C4DB199B9c":"0x02f339bf0e15d6b2fff7b2770bca8b08204cde82272e8c86116996aba7ac9f86aa,0xf702e0ff916a5a76aaf953de7583d128c013e7f13ecee5d701b49917361c5e90",
        "0x3c3E2E178C69D4baD964568415a0f0c84fd6320A":"0x02a7dbabe4d7d8ce4b97b2a6d35d7e3258f02cc961d2c83477dbaeadbc04627c9b,0x7ec49efc632757533404c2139a55b4d60d565105ca930a58709a1c52d86cf5d3",
        "0x35304262b9E87C00c430149f28dD154995d01207":"0x03e719f368cc8eb4573b9d22e88141ae459d886145219064923ff564afb432c9ac,0x755e273950f5ae64f02096ae99fe7d4f478a28afd39ef2422068ee7304c636c0",
        "0xD4A1E660C916855229e1712090CcfD8a424A2E33":"0x021a7791080bafa8601bb57bd37697f6791b525476b97289b71da04ab3a4a2b30c,0xaf6ecabcdbbfb2aefa8248b19d811234cd95caa51b8e59b6ffd3d4bbc2a6be4c",
        "0xEe7f6A930B29d7350498Af97f0F9672EaecbeeFf":"0x03d41afe51df3bc945686fc68692bbf1b842625d237f37fc6be7e7f48b12dd179a,0x70c2bd1b41084c2e2238551eace483321f8c1a413a471c3b49c8a5d1d6b3d0c4",
        "0x145e2dc5C8238d1bE628F87076A37d4a26a78544":"0x024982335a4f5adc6a12deb2e993b4a3a758c934cfd7f039f9b7f2da9bba3584e5,0xcb8e373c93609268cdcec93450f3578b92bb20c3ac2e77968d106025005f97b5",
        "0xD6A098EbCc5f8Bd4e174D915C54486B077a34A51":"0x03a3d9eb25c55ec1b2f806bf74467179ba5dafc0b60a71df8584186bb5e00d33e9,0x6f29f6e0b750bcdd31c3403f48f11d72215990375b6d23380b39c9bbf854a7d3",
        "0x042a63149117602129B6922ecFe3111168C2C323":"0x0324b9bda0e580b02d781560fd0e0cbfd2eaeba437385f8cdef34be6ec9bd8a3e5,0xff249f7eba6d8d3a65794995d724400a23d3b0bd1714265c965870ef47808be8",
        "0xa0EC9eE47802CeB56eb58ce80F3E41630B771b04":"0x02293160c8d9443c5005f079f69c8d6ccd300bb6ca602047d441cadd34fce69e6a,0x5599a7be5589682da3e0094806840e8510dae6493665a701b06c59cbe9d97968",
        "0xe8B1ff302A740fD2C6e76B620d45508dAEc2DDFf":"0x02e9c73d939eca4abac1a04ae265b74ff31cdd24be067aae13dcd54128e649161f,0x93de2205919f5b472723722fedb992e962c34d29c4caaedd82cd33e16f1fd3cf",
        "0xAb707cb80e7de7C75d815B1A653433F3EEc44c74":"0x026d93cfa091c70da11e27ee30dbcca537002ed21b19e05ac93e7d851a96138046,0xd20ecf81c6c3ad87a4e8dbeb7ceef41dd0eebc7a1657efb9d34e47217694b5cb",
        "0x0d803cdeEe5990f22C2a8DF10A695D2312dA26CC":"0x026af2062598d0630cc1c6f5353e66932434d1eb575bafab8da38741e34e82e281,0xe4058704ed240d68a94b6fb226824734ddabd4b1fe37bc85ce22f5b17f98830e",
        "0x1c87Bb9234aeC6aDc580EaE6C8B59558A4502220":"0x02505ad7c2117c6a2eef85f5b70fb590a8f13f9f37b01f7e5b10139186bef06f2a,0x4ae4408221b5042c0ee36f6e9e6b586a00d0452aa89df2e7f4f5aec42152ec43",
        "0x4779d18931B35540F84b0cd0e9633855B84df7b8":"0x03dd485c6f4db4f3978db121fba82550acef94eaf7f5ab88ecc3f6715b903c7184,0x0e7c38ba429fa5081692121c4fcf6304ca5895c6c86d31ed155b0493c516107f",
        "0xC0543b0b980D8c834CBdF023b2d2A75b5f9D1909":"0x0270f7e08d92e184babc782d65b155a9b7dfbc0b671cc7b1a8c164e279df10524c,0xd5df67c2e4da3ff9c8c6045d9b7c41581efeb2a3660921ad4ba863cc4b8c211c",
        "0x73B3074ac649A8dc31c2C90a124469456301a30F":"0x02f8660f6999a86ebe5cd7a3d70406e9a40b91b2adb9f5a4c0bc417d9782fdda90,0x92456ac1fa1ef65a04fb4689580ad5e4cda7369f3620ef3a02fa4015725f460a",
        "0x265188114EB5d5536BC8654d8e9710FE72C28c4d":"0x037e2b32c6115bd151f79cb03a02eda9a8e358fb6be1eaec990ea5a60edc5371dc,0x65b10e7d7315bb8b7f7c6eefcbd87b36ad4007c4ade9c032354f016e84ad9c5e",
        "0x924Ba5Ce9f91ddED37b4ebf8c0dc82A40202fc0A":"0x02744c74fdfbaf21ce24cb7b8b388cbdbb5d5f6cb17cbd8947937c101b5ddabd72,0x365820b3376c77dab008476d49f7cd7af87fc7bbd57dc490378106c3353b2b33",
        "0x64492E25C30031EDAD55E57cEA599CDB1F06dad1":"0x027aba8dcc8f89805a671b6f5c9cb045bbdffcc34be8d67c08676834be4e05108e,0xb07579b9864bb8e69e8b6e716284ab5b5f39fe5bb57ae4c83af795a242390202",
        "0x262595fa2a3A86adACDe208589614d483e3eF1C0":"0x03d886c44c0449efb2b3e8786b84526b145312be2218b0df7e9cd36acfab8a7906,0xbf071d2b017426fcbf763cce3b3efe3ffc9663a42c77a431df521ef6c79cacad",
        "0xDFd99099Fa13541a64AEe9AAd61c0dbf3D32D492":"0x02261caf03c748989a5d7d540b2fbf31f1a7e073a79f44b970ab2eb6de8aefe618,0x8bbffff1588b3c4eb8d415382546f6f6d5f0f61087c3be7c7c4d9e0d41d97258",
        "0x63c3686EF31C03a641e2Ea8993A91Ea351e5891a":"0x03ed764675312dedf399f1bc201d3793228ccf560d0af20ea647362c82232838be,0xb658f0575a14a7ac05075cb0f8727f0aae168a091dfb32d92514d1a7c11cf498",
        "0x9394cb5f737Bd3aCea7dcE90CA48DBd42801EE5d":"0x02317b1f8a1fbe4fe6f797c699557e934c32ba84124b84d96c5169739aea200aee,0x228330af91fa515d7514cf5ac6594ab90b296cbd8ff7bc4567306aa66cacd79f",
        "0x344dca30F5c5f74F2f13Dc1d48Ad3A9069d13Ad9":"0x033faa1f92cfcc003fb2881ebc43ec06afb7e827affb98e31ee4322730351cc422,0xe6f80f9618311c0cd58f6a3fc6621cdbf6da4a72cc42e2974c98829343e7927b",
        "0xF23E054D8b4D0BECFa22DeEF5632F27f781f8bf5":"0x0364c2c11140a075eb0091cf670323d36e13befec4e8a10cba171a9a2de9f1ab45,0x36d0435aa9a2c24d72a0aa69673b3acc2649969c38a581103df491aac6c33dd4",
        "0x6d69F301d1Da5C7818B5e61EECc745b30179C68b":"0x03c550d4a3aaabcf2489acc382f8f8809b1e1601115564e39b239348690ae701bc,0xf3ed98f9148171cfed177aef647e8ac0e2579075f640d05d37df28e6e0551083",
        "0xF0cE7BaB13C99bA0565f426508a7CD8f4C247E5a":"0x03a5d94447bc763e337e2dd4a88a4d5be5bda78f7ebb891ed9ed9cdf9d49a01ec4,0x8fc20c439fd7cf4f36217471a5db7594188540cf9997a314520a018de27544dd",
        "0x011bD5423C5F77b5a0789E27f922535fd76B688F":"0x0351ee10626b0a786de6055e2ae0d5425ba3f17059b40219fe8820946d16249e1e,0x549078aab3adafeff862b2d40b6b27756c5c4669475c3367edfb8dcf63ea1ae5",
        "0xD9065f27e9b706E5F7628e067cC00B288dddbF19":"0x0229157c9e5a73e54e55a062cf19104f6061dbcb11a23e31f60913d0b12c553d3d,0xacf192decb2e4ddd8ad61693ab8edd67e3620b2ed79880ff4e1e04482c52c916",
        "0x54ccCeB38251C29b628ef8B00b3cAB97e7cAc7D5":"0x03feca36c85fc354f47dfcc23f0e5baca85357c64bbe8616560675cd3333b8acbc,0x47dc59330fb8c356ef61c55c11f9bb49ee463df50cbfe59f389de7637037b029",
        "0xA1196426b41627ae75Ea7f7409E074BE97367da2":"0x0352536b2204dc725b8b98c2931079fcb912bc702215da262e9d5cd444d226cc68,0xf0050439b33fd77f7183f44375bc43a869a9880dca82a187fab9be91e020d029",
        "0xE74cEf90b6CF1a77FEfAd731713e6f53e575C183":"0x026ffa4ca4e8e4e9e9426f2b762e31a9b0b3586f8eafb470bb32aca6e5fd46ffcd,0xe995cc7ea38e5c2927b97607765c2a20f4a6052d6810a3a1102e84d77c0df13b",
        "0x7Df8Efa6d6F1CB5C4f36315e0AcB82b02Ae8BA40":"0x03c85a8ab2a97379d1c3ff45e28815a7dc5485d2246362bd4823fceee257b7e798,0x8232e778c8e32eddb268e12aee5e82c7bb540cc176e150d64f35ee4ae2faf2b2",
        "0x9E126C57330FA71556628e0aabd6B6B6783d99fA":"0x034d7b61c8dd53a761ab44d1e06be6b1338de4095c620112494b8830792c84f64b,0xba8c9ff38e4179748925335a9891b969214b37dc3723a1754b8b849d3eea9ac0"
    }

    # init with 100,000 EOS
    for i,k in enumerate(addys):
        print("addys: [{0}] [{1}] [{2}]".format(i,k[2:].lower(), len(k[2:])))
        transferAmount="100000.0000 {0}".format(CORE_SYMBOL)
        Print("Transfer funds %s from account %s to %s" % (transferAmount, cluster.eosioAccount.name, evmAcc.name))
        trans = prodNode.transferFunds(cluster.eosioAccount, evmAcc, transferAmount, "0x" + k[2:].lower(), waitForTransBlock=False)
        if not (i+1) % 20: time.sleep(1)
    prodNode.waitForTransBlockIfNeeded(trans, True)

    if gensisJson[0] != '/': gensisJson = os.path.realpath(gensisJson)
    f=open(gensisJson,"w")
    f.write(json.dumps(genesis_info))
    f.close()
    Utils.Print("#####################################################")
    Utils.Print("Generated EVM json genesis file in: %s" % gensisJson)
    Utils.Print("")
    Utils.Print("You can now run:")
    Utils.Print("  eos-evm-node --plugin=blockchain_plugin --ship-endpoint=127.0.0.1:8999 --genesis-json=%s --chain-data=/tmp --verbosity=4" % gensisJson)
    Utils.Print("  eos-evm-rpc --eos-evm-node=127.0.0.1:8080 --http-port=0.0.0.0:8881 --chaindata=/tmp --api-spec=eth,debug,net,trace")
    Utils.Print("")
    Utils.Print("Web3 endpoint:")
    Utils.Print("  http://localhost:5000")

    app = Flask(__name__)
    CORS(app)

    @app.route("/fork", methods=["POST"])
    def fork():
        Print("Sending command to kill bridge node to separate the 2 producer groups.")
        forkAtProducer="defproducera"
        prodNode1Prod="defproduceru"
        preKillBlockNum=nonProdNode.getBlockNum()
        preKillBlockProducer=nonProdNode.getBlockProducerByNum(preKillBlockNum)
        nonProdNode.killNodeOnProducer(producer=forkAtProducer, whereInSequence=1)
        Print("Current block producer %s fork will be at producer %s" % (preKillBlockProducer, forkAtProducer))
        prodNode0.waitForProducer(forkAtProducer)
        prodNode1.waitForProducer(prodNode1Prod)
        if nonProdNode.verifyAlive(): # if on defproducera, need to wait again
            prodNode0.waitForProducer(forkAtProducer)
            prodNode1.waitForProducer(prodNode1Prod)

        if nonProdNode.verifyAlive():
            Print("Bridge did not shutdown")
            return "Bridge did not shutdown"

        Print("Fork started")
        return "Fork started"

    @app.route("/restore", methods=["POST"])
    def restore():
        Print("Relaunching the non-producing bridge node to connect the producing nodes again")

        if nonProdNode.verifyAlive():
            return "bridge is already running"

        if not nonProdNode.relaunch():
            Utils.errorExit("Failure - (non-production) node %d should have restarted" % (nonProdNode.nodeNum))

        return "restored fork should resolve"

    @app.route("/", methods=["POST"])
    def default():
        def forward_request(req):
            if req['method'] == "eth_sendRawTransaction":
                actData = {"miner":"evmevmevmevm", "rlptx":req['params'][0][2:]}
                prodNode1.pushMessage(evmAcc.name, "pushtx", json.dumps(actData), '-p evmevmevmevm')
                return {
                    "id": req['id'],
                    "jsonrpc": "2.0",
                    "result": '0x'+keccak(unhexlify(req['params'][0][2:])).hex()
                }
            return requests.post(readEndpoint, json.dumps(req), headers={"Content-Type":"application/json"}).json()

        request_data = request.get_json()
        if type(request_data) == dict:
            return jsonify(forward_request(request_data))

        res = []
        for r in request_data:
            res.append(forward_request(r))

        return jsonify(res)

    app.run(host='0.0.0.0', port=5000)

finally:
    TestHelper.shutdown(cluster, walletMgr, testSuccessful=testSuccessful, killEosInstances=killEosInstances, killWallet=killEosInstances, keepLogs=keepLogs, cleanRun=killAll, dumpErrorDetails=dumpErrorDetails)

exitCode = 0 if testSuccessful else 1
exit(exitCode)
