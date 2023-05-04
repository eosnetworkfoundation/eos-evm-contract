# evm_poc_tx_wrapper

## Build
```
yarn
```

## Run
Create a .env file:
```
# This is sample keys. Do NOT Commit .env!

# one or more EOS_RPC endpoints, separated by '|'
EOS_RPC="http://127.0.0.1:8888|http://192.168.1.1:8888"
EOS_KEY="5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"

# the listening IP & port of this service
HOST="127.0.0.1"
PORT="3335"
EOS_PERMISSION="active"
EXPIRE_SEC=60
```
Then
```
node index.js
```
