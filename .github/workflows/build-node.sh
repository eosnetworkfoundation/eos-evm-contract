#!/bin/bash
set -eo pipefail

# print and run a command
function ee()
{
    echo "$ $*"
    eval "$@"
}

# install dependencies for self-hosted runners
ee export DEBIAN_FRONTEND='noninteractive'
ee sudo apt-get update -q
ee sudo apt-get install -yqq cmake gcc-10 g++-10

# debug code
echo "CC='${CC}'"
echo "CXX='${CXX}'"
ee cmake --version

# build
ee mkdir build
ee pushd build
ee cmake ..
ee make -j "$(nproc)"

# pack
ee popd
ee 'tar -czf build.tar.gz build/*'

echo "Done! - ${0##*/}"
