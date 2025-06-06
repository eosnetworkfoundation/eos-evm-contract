cmake_minimum_required(VERSION 3.16)
project(evm_runtime)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(cdt)

set(SOURCES, "")

list(APPEND SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/state.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/actions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/config_wrapper.cpp
)
if (WITH_TEST_ACTIONS)
    add_compile_definitions(WITH_TEST_ACTIONS)
    list(APPEND SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test_actions.cpp)
endif()

if (WITH_LOGTIME)
    add_compile_definitions(WITH_LOGTIME)
endif()

if (WITH_ADMIN_ACTIONS)
    add_compile_definitions(WITH_ADMIN_ACTIONS)
    list(APPEND SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/admin_actions.cpp)
endif()

if (WITH_SOFT_FORKS)
    add_compile_definitions(WITH_SOFT_FORKS)
endif()

add_compile_definitions(ANTELOPE)
add_compile_definitions(PROJECT_VERSION="2.0.0-rc2")

# ethash
list(APPEND SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/third_party/ethash/lib/keccak/keccak.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/third_party/ethash/lib/ethash/ethash.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/third_party/ethash/lib/ethash/primes.c
)

# evmone
list(APPEND SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/third_party/evmone/lib/evmone/instructions_calls.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/third_party/evmone/lib/evmone/vm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/third_party/evmone/lib/evmone/eof.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/third_party/evmone/lib/evmone/baseline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/third_party/evmone/lib/evmone/baseline_instruction_table.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/third_party/evmone/lib/evmone/instructions_storage.cpp
)

# silkworm
list(APPEND SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/common/util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/common/endian.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/common/assert.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/protocol/rule_set.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/protocol/validation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/protocol/intrinsic_gas.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/execution/evm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/execution/precompile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/execution/address.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/execution/processor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/state/intra_block_state.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/state/delta.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/types/account.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/types/transaction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/types/receipt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/types/block.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/types/log.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/types/y_parity_and_chain_id.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/rlp/encode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/rlp/decode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/crypto/ecdsa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/crypto/secp256k1n.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/silkworm/core/chain/config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/eosevm/refund_v3.cpp
)

add_contract( evm_contract evm_runtime ${SOURCES})

target_include_directories( evm_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/third_party/intx/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/third_party/ethash/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/third_party/evmone/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/third_party/evmone/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../silkworm/third_party/evmone/evmc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/expected/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/GSL/include
)

target_compile_options(evm_runtime PUBLIC --no-missing-ricardian-clause)

if (WITH_LARGE_STACK)
    target_link_options(evm_runtime PUBLIC --stack-size=50000000)
else()
    target_link_options(evm_runtime PUBLIC --stack-size=35984)
endif()
