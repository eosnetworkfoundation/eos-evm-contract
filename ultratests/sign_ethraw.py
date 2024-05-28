from web3 import Web3, EthereumTesterProvider
from eth_account import Account

# Initialize Web3
w3 = Web3(EthereumTesterProvider())

# Sender's private key and the transaction details
private_key = '0x4bbbf85ce3377467afe5d46f804f221813b2bb87f24d81f60f1fcdbf7cbf4356'
nonce = 0
to = '0x4ea3b729669bF6C34F7B80E5D6c17DB71F89F21F'

from web3 import Web3

address = '0x4ea3b729669bf6c34f7b80e5d6c17db71f89f21f'
checksummed_address = Web3.to_checksum_address(address)
print(checksummed_address)

#18 precision
gas = 1000000000
gas_price = 1100000000

# Create transaction dictionary
transaction = {
    'to': to,
    'value': 1,
    'gas': gas,
    'gasPrice': gas_price,
    'nonce': nonce,
    'chainId': 15555
}

# Sign the transaction
signed_txn = w3.eth.account.sign_transaction(transaction, private_key)

# Print the signed transaction's raw data
print(signed_txn.raw_transaction.hex())
