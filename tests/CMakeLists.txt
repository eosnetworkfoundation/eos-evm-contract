project(evm_runtime_tests)
cmake_minimum_required(VERSION 3.12)

find_package(eosio)

enable_testing()

configure_file(${CMAKE_SOURCE_DIR}/contracts.hpp.in ${CMAKE_BINARY_DIR}/contracts.hpp)

include_directories(
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/silkworm/node
    ${CMAKE_SOURCE_DIR}/silkworm/core
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/external/evmone/lib
    ${CMAKE_SOURCE_DIR}/external/evmone/evmc/include
    ${CMAKE_SOURCE_DIR}/external/magic_enum/include
    ${CMAKE_SOURCE_DIR}/external/abseil
    ${CMAKE_SOURCE_DIR}/external/intx/include
    ${CMAKE_SOURCE_DIR}/external/ethash/include
    ${CMAKE_SOURCE_DIR}/external/secp256k1/include
)

add_eosio_test_executable( unit_test
    ${CMAKE_SOURCE_DIR}/version_tests.cpp
    ${CMAKE_SOURCE_DIR}/account_id_tests.cpp
    ${CMAKE_SOURCE_DIR}/basic_evm_tester.cpp
    ${CMAKE_SOURCE_DIR}/evm_runtime_tests.cpp
    ${CMAKE_SOURCE_DIR}/init_tests.cpp
    ${CMAKE_SOURCE_DIR}/native_token_tests.cpp
    ${CMAKE_SOURCE_DIR}/mapping_tests.cpp
    ${CMAKE_SOURCE_DIR}/gas_fee_tests.cpp
    ${CMAKE_SOURCE_DIR}/gas_param_tests.cpp
    ${CMAKE_SOURCE_DIR}/blockhash_tests.cpp
    ${CMAKE_SOURCE_DIR}/exec_tests.cpp
    ${CMAKE_SOURCE_DIR}/call_tests.cpp
    ${CMAKE_SOURCE_DIR}/chainid_tests.cpp
    ${CMAKE_SOURCE_DIR}/bridge_message_tests.cpp
    ${CMAKE_SOURCE_DIR}/admin_actions_tests.cpp
    ${CMAKE_SOURCE_DIR}/main.cpp
    ${CMAKE_SOURCE_DIR}/silkworm/core/silkworm/rlp/encode.cpp
    ${CMAKE_SOURCE_DIR}/silkworm/core/silkworm/rlp/decode.cpp
    ${CMAKE_SOURCE_DIR}/silkworm/core/silkworm/types/block.cpp
    ${CMAKE_SOURCE_DIR}/silkworm/core/silkworm/types/transaction.cpp
    ${CMAKE_SOURCE_DIR}/silkworm/core/silkworm/types/account.cpp
    ${CMAKE_SOURCE_DIR}/silkworm/node/silkworm/common/stopwatch.cpp
    ${CMAKE_SOURCE_DIR}/silkworm/core/silkworm/common/util.cpp
    ${CMAKE_SOURCE_DIR}/silkworm/core/silkworm/common/endian.cpp
    ${CMAKE_SOURCE_DIR}/silkworm/core/silkworm/execution/address.cpp
    ${CMAKE_SOURCE_DIR}/silkworm/core/silkworm/crypto/ecdsa.cpp
    ${CMAKE_SOURCE_DIR}/external/ethash/lib/keccak/keccak.c
    ${CMAKE_SOURCE_DIR}/external/ethash/lib/ethash/ethash.cpp
    ${CMAKE_SOURCE_DIR}/external/ethash/lib/ethash/primes.c
)

add_test(NAME consensus_tests COMMAND unit_test --report_level=detailed --color_output --run_test=evm_runtime_tests -- --eos-vm-oc)

add_test(NAME unit_tests COMMAND unit_test --report_level=detailed --color_output --run_test=!evm_runtime_tests -- --eos-vm-oc)
