import os
import sys
from datetime import datetime, timedelta
from getpass import getpass
from binascii import hexlify
from ethereum.utils import privtoaddr, encode_hex, decode_hex, bytes_to_int
from ethereum import transactions
from binascii import unhexlify
import time
import subprocess
import requests
import rlp
import json
import csv

EVM_CONTRACT    = os.getenv("EVM_CONTRACT", "evmevmevmevm")
NODEOS_ENDPOINT = os.getenv("NODEOS_ENDPOINT", "http://127.0.0.1:8888")
EOS_SENDER      = os.getenv("EOS_SENDER", "evmevmevmevm")
EVM_SENDER_KEY  = os.getenv("EVM_SENDER_KEY", None)
EOS_SENDER_KEY  = os.getenv("EOS_SENDER_KEY", None)
EVM_CHAINID     = int(os.getenv("EVM_CHAINID", "15555"))

# staring_nonce is the nonce number that maps to the transfer of the first account in the list
# it is used to ensure each transfer is idempotent
# be careful when you change it
starting_nonce = 3

batch_size = 20
query_timeout = 10

def queryNonce():
    params = {
        "json":True,
        "code":EVM_CONTRACT,
        "scope":EVM_CONTRACT,
        "table":"account",
        "table_key":"",
        "lower_bound":_from,
        "upper_bound":_from,
        "limit":1,
        "key_type":"sha256",
        "index_position":"2",
        "encode_type":"dec",
        "reverse":False,
        "show_payer":False
    }
    resp = requests.post(url=NODEOS_ENDPOINT + "/v1/chain/get_table_rows", data=json.dumps(params))
    data = resp.json() # Check the JSON Response Content documentation below
    #print("response is {}".format(data))
    nonce = data["rows"][0]["nonce"]
    return nonce

if len(sys.argv) < 3:
    print("{0} FROM_ACCOUNT CSV_FILE".format(sys.argv[0]))
    sys.exit(1)

_from = sys.argv[1].lower()
if _from[:2] == '0x': _from = _from[2:]

print("reading accounts from {}...".format(sys.argv[2]))
to_acc_bals = []
with open(sys.argv[2], 'r') as file:
    reader = csv.reader(file)
    for row in reader:
        acc = row[0]
        if acc[:2] == '0x': 
            acc = acc[2:]
        bal = int(row[1])
        unsigned_tx = transactions.Transaction(
            starting_nonce + len(to_acc_bals),
            150000000000, #150 GWei
            100000,       #100k Gas
            acc,
            bal,
            b''
        )
        rlptx = rlp.encode(unsigned_tx.sign(EVM_SENDER_KEY, EVM_CHAINID), transactions.Transaction)
        to_acc_bals.append([acc, bal, rlptx.hex()])

print("{} accounts loaded from {}".format(len(to_acc_bals), sys.argv[2]))

current_nonce = queryNonce()
print("current nonce is {}".format(current_nonce))

if starting_nonce != current_nonce:
    print("WARNING! nonce value not same, will continue from the last point")

last_query_time = time.time()
while current_nonce - starting_nonce < len(to_acc_bals):
    i = current_nonce - starting_nonce
    batch_end = i + batch_size

    print("distribute {} to account {}, nonce {}".format(to_acc_bals[i][1], to_acc_bals[i][0], starting_nonce + i))
    act_data = {"miner":EOS_SENDER, "rlptx":to_acc_bals[i][2]}
    result = subprocess.run(["./cleos", "-u", NODEOS_ENDPOINT, "push", "action", EVM_CONTRACT, "pushtx", json.dumps(act_data), "-p", EOS_SENDER, "-s", "-j", "-d"], capture_output=True, text=True)
    txn_json = json.loads(result.stdout)
    i = i + 1
    current_nonce = current_nonce + 1

    while i < batch_end and i < len(to_acc_bals):
        print("distribute {} to account {}, nonce {}".format(to_acc_bals[i][1], to_acc_bals[i][0], starting_nonce + i))
        act_data = {"miner":EOS_SENDER, "rlptx":to_acc_bals[i][2]}
        act_json = json.loads(json.dumps(txn_json["actions"][0]))
        act_json["data"] = act_data
        txn_json["actions"].append(act_json)
        i = i + 1
        current_nonce = current_nonce + 1

    #print("txn_json is {}".format(txn_json))
    subprocess.run(["./cleos", "-u", NODEOS_ENDPOINT, "push", "transaction", json.dumps(txn_json)], capture_output=True, text=True)

    # resync current nonce, in case transactions fail
    if time.time() - last_query_time >= query_timeout:
        current_nonce = queryNonce()
        if current_nonce - starting_nonce == len(to_acc_bals):
            time.sleep(query_timeout)
            current_nonce = queryNonce()
        print("current nonce is {}".format(current_nonce))
        last_query_time = time.time()

print("Successfully distribute tokens to {} accounts".format(len(to_acc_bals)))
sys.exit(0)
