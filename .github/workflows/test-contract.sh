#!/bin/bash
set -eo pipefail

# print and run a command
function ee()
{
    echo "$ $*"
    eval "$@"
}

ee pushd contract/tests/build
if [ "$DWITH_TEST_ACTIONS" = "on" ] || [ "$DWITH_TEST_ACTIONS" = "true" ]; then
ee "ctest -R consensus_tests -j \"$(nproc)\" --output-on-failure -T Test"
else
ee "ctest -E consensus_tests -j \"$(nproc)\" --output-on-failure -T Test"
fi

cp "$(find ./Testing -name 'Test.xml' | sort | tail -n '1')" "../../../${XUNIT_FILENAME:-test-results.xml}"
echo "Done! - ${0##*/}"
