#!/bin/bash
set -eo pipefail

# print and run a command
function ee()
{
    echo "$ $*"
    eval "$@"
}

export Deosio_DIR='/usr/lib/x86_64-linux-gnu/cmake/eosio'
# debug code
ee nodeos --full-version
ee cmake --version

# build
ee mkdir -p contract/tests/build
ee pushd contract/tests
ee pushd build
ee "cmake -Deosio_DIR=$Deosio_DIR .."
ee make -j "$(nproc)" unit_test

# pack
ee popd
ee 'tar -czf ../../contract-unit-test.tar.gz build/*'
ee popd

echo "Done! - ${0##*/}"
