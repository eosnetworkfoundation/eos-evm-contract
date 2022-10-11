import sys
import os
import string
from ethereum.utils import privtoaddr, encode_hex, decode_hex, bytes_to_int
from ethereum import transactions
from ueosio.rpc import Api

NODEOS_ENDPOINT = os.environ.get('NODEOS_ENDPOINT', 'http://127.0.0.1:8888')

if len(sys.argv) < 2:
    print("{0} ACCOUNT".format(sys.argv[0]))
    sys.exit(1)

_account = sys.argv[1].lower()
if _account[:2] == '0x': _account = _account[2:]

if not all(c in string.hexdigits for c in _account) or len(_account) != 40:
    print("invalid address")
    sys.exit(1)
 
api = Api(NODEOS_ENDPOINT)
res = api.v1.chain.get_table_rows(
    code="evmevmevmevm",
    scope="evmevmevmevm",
    table="account",
    key_type="sha256",
    index_position="2",
    lower_bound=_account,
    limit=1,
    json=True
)

bal = 0
if len(res['rows']) and res['rows'][0]['eth_address'] == _account:
    bal = bytes_to_int(decode_hex(res['rows'][0]['balance']))

print(bal)
