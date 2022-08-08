# EVM runtime contract

### Add python deps (needed for post-build)
```
pip install wasm
```

### Build Contract
```
git clone //https://github.com/eosnetworkfoundation/TrustEVM
cd TrustEVM/contract
git submodule update --init --recursive
mkdir build
cd build
cmake ..
make -j4
```


# Build unit tests
Contract must be built using `-DWITH_TEST_ACTIONS`
```
cd tests
mkdir build
cd build
cmake -Deosio_DIR=/path-to-mandel/build/lib/cmake/eosio ..
make -j4 unit_test
```
