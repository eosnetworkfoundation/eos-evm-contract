find_package(leap)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/contracts.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/contracts.hpp)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/silkworm/node
    ${CMAKE_CURRENT_SOURCE_DIR}/silkworm/core
    ${CMAKE_SOURCE_DIR}/../silkworm/third_party/evmone/lib
    ${CMAKE_SOURCE_DIR}/../silkworm/third_party/evmone/evmc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/magic_enum/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/abseil
    ${CMAKE_SOURCE_DIR}/external/intx/include
    ${CMAKE_SOURCE_DIR}/external/ethash/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/../silkworm/third_party/silkpre/third_party/secp256k1/include
)

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated-declarations")

add_eosio_test_executable( unit_test
    ${CMAKE_CURRENT_SOURCE_DIR}/evm_runtime_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/init_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/native_token_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/silkworm/core/silkworm/rlp/encode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/silkworm/core/silkworm/rlp/decode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/silkworm/core/silkworm/types/block.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/silkworm/core/silkworm/types/transaction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/silkworm/core/silkworm/types/account.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/silkworm/node/silkworm/common/stopwatch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/silkworm/core/silkworm/common/util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/silkworm/core/silkworm/common/endian.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/silkworm/core/silkworm/crypto/ecdsa.cpp
    ${CMAKE_SOURCE_DIR}/external/ethash/lib/keccak/keccak.c
    ${CMAKE_SOURCE_DIR}/external/ethash/lib/ethash/ethash.cpp
    ${CMAKE_SOURCE_DIR}/external/ethash/lib/ethash/primes.c
)


set(CONSENSUS_TESTS_LIMIT_PER_SHARD 100)
set(CONSENSUS_TESTS_NUM_SHARDS 28) # Must be at least 2

# Add ${CONSENSUS_TESTS_NUM_SHARDS} tests with the consensus_tests label for shards of the consensus tests.
math(EXPR CONSENSUS_TESTS_PENULTIMATE_SHARD_INDEX "${CONSENSUS_TESTS_NUM_SHARDS} - 2")
foreach(CONSENSUS_TESTS_SHARD_INDEX RANGE 0 ${CONSENSUS_TESTS_PENULTIMATE_SHARD_INDEX})
    math(EXPR CONSENSUS_TESTS_START_ORDINAL "${CONSENSUS_TESTS_LIMIT_PER_SHARD} * ${CONSENSUS_TESTS_SHARD_INDEX}")
    # TODO: add back eos-vm-oc once change to disable EOS VM OC subjective limits during unit test are added
    add_test(NAME consensus_tests_${CONSENSUS_TESTS_SHARD_INDEX} COMMAND unit_test --report_level=detailed --color_output --run_test=evm_runtime_tests -- --limit=${CONSENSUS_TESTS_LIMIT_PER_SHARD} --start-ordinal=${CONSENSUS_TESTS_START_ORDINAL}) 
    set_tests_properties(consensus_tests_${CONSENSUS_TESTS_SHARD_INDEX} PROPERTIES LABELS "consensus_tests" )
endforeach()
math(EXPR CONSENSUS_TESTS_LAST_SHARD_INDEX "${CONSENSUS_TESTS_NUM_SHARDS} - 1")
math(EXPR CONSENSUS_TESTS_LAST_START_ORDINAL "${CONSENSUS_TESTS_LIMIT_PER_SHARD} * ${CONSENSUS_TESTS_LAST_SHARD_INDEX}")
# TODO: add back eos-vm-oc once change to disable EOS VM OC subjective limits during unit test are added
add_test(NAME consensus_tests_${CONSENSUS_TESTS_LAST_SHARD_INDEX} COMMAND unit_test --report_level=detailed --color_output --run_test=evm_runtime_tests -- --start-ordinal=${CONSENSUS_TESTS_LAST_START_ORDINAL}) 
set_tests_properties(consensus_tests_${CONSENSUS_TESTS_LAST_SHARD_INDEX} PROPERTIES LABELS "consensus_tests" )

# Add single test to cover all the remaining tests.
add_test(NAME unit_tests COMMAND unit_test --report_level=detailed --color_output --run_test=!evm_runtime_tests)
