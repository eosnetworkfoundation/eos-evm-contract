cmake_minimum_required(VERSION 3.16)
project(evm_runtime)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(cdt)

set(SOURCES, "")

list(APPEND SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/state.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/actions.cpp
)
if (WITH_TEST_ACTIONS)
    add_compile_definitions(WITH_TEST_ACTIONS)
endif()

if (WITH_LOGTIME)
    add_compile_definitions(WITH_LOGTIME)
endif()

add_compile_definitions(ANTELOPE)
add_compile_definitions(PROJECT_VERSION="0.2.1")

# ethash
list(APPEND SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/ethash/lib/keccak/keccak.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/ethash/lib/ethash/ethash.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/ethash/lib/ethash/primes.c
)

# evmone
list(APPEND SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/third_party/evmone/lib/evmone/instructions_calls.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/third_party/evmone/lib/evmone/advanced_analysis.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/third_party/evmone/lib/evmone/advanced_instructions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/third_party/evmone/lib/evmone/advanced_execution.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/third_party/evmone/lib/evmone/vm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/third_party/evmone/lib/evmone/eof.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/third_party/evmone/lib/evmone/baseline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/third_party/evmone/lib/evmone/baseline_instruction_table.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/third_party/evmone/lib/evmone/instructions_storage.cpp
)

# silkworm
list(APPEND SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/core/silkworm/common/util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/core/silkworm/common/endian.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/core/silkworm/consensus/engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/core/silkworm/execution/evm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/third_party/silkpre/lib/silkpre/precompile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/core/silkworm/execution/analysis_cache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/core/silkworm/execution/address.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/core/silkworm/state/intra_block_state.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/core/silkworm/state/delta.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/core/silkworm/types/account.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/core/silkworm/types/transaction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/core/silkworm/types/receipt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/core/silkworm/types/block.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/core/silkworm/types/log.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/core/silkworm/types/y_parity_and_chain_id.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/core/silkworm/rlp/encode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/core/silkworm/rlp/decode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/third_party/silkpre/lib/silkpre/ecdsa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/third_party/silkpre/lib/silkpre/secp256k1n.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/core/silkworm/chain/intrinsic_gas.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/core/silkworm/chain/config.cpp
)

add_contract( evm_contract evm_runtime ${SOURCES})

target_include_directories( evm_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cmd/contract_common
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/intx/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/ethash/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/GSL/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/core
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/third_party/silkpre/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/third_party/evmone/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/third_party/evmone/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../../silkworm/third_party/evmone/evmc/include
)

if (WITH_LARGE_STACK)
    target_link_options(evm_runtime PUBLIC --stack-size=50000000)
else()
    target_link_options(evm_runtime PUBLIC --stack-size=20480)
endif()
