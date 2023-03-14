#!/bin/bash
set -eo pipefail

# print and run a command
function ee()
{
    echo "$ $*"
    eval "$@"
}

# debug code
ee cdt-cc --version
ee cmake --version

# build
ee mkdir -p contract/build
ee pushd contract
ee pushd build
ee "cmake -DCMAKE_BUILD_TYPE=$DCMAKE_BUILD_TYPE -DWITH_TEST_ACTIONS=$DWITH_TEST_ACTIONS -DWITH_LARGE_STACK=$DWITH_TEST_ACTIONS .."
ee make -j "$(nproc)"

# pack
ee popd
ee 'tar -czf ../contract.tar.gz build/*'
ee popd

echo "Done! - ${0##*/}"
