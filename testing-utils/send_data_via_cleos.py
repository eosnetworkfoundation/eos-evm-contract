import os
import sys
from datetime import datetime, timedelta
from getpass import getpass
from binascii import hexlify
from ethereum.utils import privtoaddr, encode_hex, decode_hex, bytes_to_int
from ethereum import transactions
from binascii import unhexlify
import subprocess
import requests as r
import rlp
import json

EVM_CONTRACT    = os.getenv("EVM_CONTRACT", "evmevmevmevm")
NODEOS_ENDPOINT = os.getenv("NODEOS_ENDPOINT", "http://localhost:8888")
EOS_SENDER      = os.getenv("EOS_SENDER", "evmevmevmevm")
EVM_SENDER_KEY  = os.getenv("EVM_SENDER_KEY", None)
EOS_SENDER_KEY  = os.getenv("EOS_SENDER_KEY", None)
EVM_CHAINID     = int(os.getenv("EVM_CHAINID", "15555"))

if len(sys.argv) < 4:
    print("{0} FROM TO AMOUNT".format(sys.argv[0]))
    sys.exit(1)

_from = sys.argv[1].lower()
if _from[:2] == '0x': _from = _from[2:]

_to     = sys.argv[2].lower()
if _to[:2] == '0x': _to = _to[2:]

_amount = int(sys.argv[3])
nonce = int(sys.argv[5])

unsigned_tx = transactions.Transaction(
    nonce,
    1000000000,   #1 GWei
    1000000,   #1m Gas
    _to,
    _amount,
    unhexlify(sys.argv[4])
)

if not EVM_SENDER_KEY:
    EVM_SENDER_KEY = getpass('Enter private key for {0}:'.format(_from))

rlptx = rlp.encode(unsigned_tx.sign(EVM_SENDER_KEY, EVM_CHAINID), transactions.Transaction)

act_data = {"ram_payer":EOS_SENDER, "rlptx":rlptx.hex()}
print("act_data is {}".format(act_data))

subprocess.run(["./cleos", "push", "action", EVM_CONTRACT, "pushtx", json.dumps(act_data), "-p", EOS_SENDER, "-j"])

