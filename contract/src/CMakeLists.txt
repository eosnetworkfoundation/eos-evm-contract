cmake_minimum_required(VERSION 3.19)
project(evm_runtime)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(cdt)

set(SOURCES, "")

list(APPEND SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/state.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/actions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/processor.cpp
)
if (WITH_TEST_ACTIONS)
    add_compile_definitions(WITH_TEST_ACTIONS)
endif()

if (WITH_LOGTIME)
    add_compile_definitions(WITH_LOGTIME)
endif()

# ethash
list(APPEND SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/ethash/lib/keccak/keccak.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/ethash/lib/ethash/ethash.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/ethash/lib/ethash/primes.c
)

# evmone
list(APPEND SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/evmone/lib/evmone/instructions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/evmone/lib/evmone/instructions_calls.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/evmone/lib/evmone/execution.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/evmone/lib/evmone/analysis.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/evmone/lib/evmone/vm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/evmone/lib/evmone/baseline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/evmone/lib/evmone/baseline_instruction_table.cpp
)

# silkworm
list(APPEND SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/silkworm/common/util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/silkworm/common/endian.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/silkworm/execution/evm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/silkworm/execution/precompiled.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/silkworm/execution/address.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/silkworm/state/intra_block_state.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/silkworm/state/delta.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/silkworm/types/account.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/silkworm/types/transaction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/silkworm/types/receipt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/silkworm/types/block.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/silkworm/types/log.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/silkworm/rlp/encode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/silkworm/rlp/decode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/silkworm/crypto/ecdsa.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/silkworm/chain/intrinsic_gas.cpp
)

add_contract( evm_contract evm_runtime ${SOURCES})

target_include_directories( evm_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../external
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/intx/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/ethash/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/rlpvalue/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/evmone/evmc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/evmone/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/evmone/lib
)

target_include_directories( evm_runtime PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_link_options(evm_runtime PUBLIC -stack-size=20000)

add_custom_command(TARGET evm_runtime
   # make wasm data segments 8k long max
   POST_BUILD
   COMMENT "#### making wasm data segments 8k long max ####"
   COMMAND eosio-wasm2wast ${CMAKE_CURRENT_BINARY_DIR}/evm_runtime.wasm | grep -v "(data" > ${CMAKE_CURRENT_BINARY_DIR}/evm_runtime.wat
   COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/../fix-data-segment.py ${CMAKE_CURRENT_BINARY_DIR}/evm_runtime.wasm > ${CMAKE_CURRENT_BINARY_DIR}/fixed-segments.wat
   COMMAND cat ${CMAKE_CURRENT_BINARY_DIR}/fixed-segments.wat >> ${CMAKE_CURRENT_BINARY_DIR}/evm_runtime.wat
   COMMAND echo ")" >> ${CMAKE_CURRENT_BINARY_DIR}/evm_runtime.wat
   COMMAND eosio-wast2wasm ${CMAKE_CURRENT_BINARY_DIR}/evm_runtime.wat -o ${CMAKE_CURRENT_BINARY_DIR}/evm_runtime.wasm
   VERBATIM
)
