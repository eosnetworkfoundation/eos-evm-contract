#!/bin/bash
set -eo pipefail

# print and run a command
function ee()
{
    echo "$ $*"
    eval "$@"
}

ee pushd contract/tests/build
ee "ctest -j \"$(nproc)\" --output-on-failure -T Test || :"
cp "$(find ./Testing -name 'Test.xml' | sort | tail -n '1')" "../../../${XUNIT_FILENAME:-test-results.xml}"
echo "Done! - ${0##*/}"
