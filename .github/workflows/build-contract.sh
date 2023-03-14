#!/bin/bash
set -eo pipefail

# print and run a command
function ee()
{
    echo "$ $*"
    eval "$@"
}

export Dleap_DIR='/usr/lib/x86_64-linux-gnu/cmake/leap'

# debug code
ee cdt-cc --version
ee cmake --version
ee cmake --version
echo 'Leap version:'
cat "$Dleap_DIR/EosioTester.cmake" | grep 'EOSIO_VERSION' | grep -oP "['\"].*['\"]" | tr -d "'\"" || :

# build
ee mkdir -p contract/build
ee pushd contract
ee pushd build
ee "cmake -DCMAKE_BUILD_TYPE=$DCMAKE_BUILD_TYPE .."
ee make -j "$(nproc)"

# pack
ee popd
ee 'tar -czf ../contract.tar.gz build/*'
ee popd

echo "Done! - ${0##*/}"
