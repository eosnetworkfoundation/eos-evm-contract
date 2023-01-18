#!/bin/bash
set -eo pipefail

# print and run a command
function ee()
{
    echo "$ $*"
    eval "$@"
}

# debug code
ee cmake --version
ee gcc --version

# build
ee mkdir build
ee pushd build
ee cmake ..
ee make -j "$(nproc)"

# pack
ee popd
ee 'tar -czf build.tar.gz build/*'

echo "Done! - ${0##*/}"
