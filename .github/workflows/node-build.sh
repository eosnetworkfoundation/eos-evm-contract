#!/bin/bash
set -eo pipefail

function ee()
{
    echo "$ $*"
    eval "$@"
}

ee mkdir build
ee cd build
ee cmake ..
ee make -j "$(nproc)"
ee 'tar -czf build.tar.gz build/*'
echo 'Done! - node-build.sh'
