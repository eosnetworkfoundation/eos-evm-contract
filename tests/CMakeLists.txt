project(evm_runtime_tests)
cmake_minimum_required(VERSION 3.12)

find_package(eosio)

enable_testing()

configure_file(${CMAKE_SOURCE_DIR}/contracts.hpp.in ${CMAKE_BINARY_DIR}/contracts.hpp)

include_directories(
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/../silkworm/
    ${CMAKE_SOURCE_DIR}/../silkworm/third_party/evmone/lib
    ${CMAKE_SOURCE_DIR}/../silkworm/third_party/evmone/evmc/include
    ${CMAKE_SOURCE_DIR}/../silkworm/third_party/intx/include
    ${CMAKE_SOURCE_DIR}/../silkworm/third_party/ethash/include
    ${CMAKE_SOURCE_DIR}/../silkworm/third_party/secp256k1/include
    ${CMAKE_SOURCE_DIR}/../external/expected/include
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/external/magic_enum/include
    ${CMAKE_SOURCE_DIR}/external/abseil
)

add_eosio_test_executable( unit_test
    ${CMAKE_SOURCE_DIR}/different_gas_token_tests.cpp
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
    ${CMAKE_SOURCE_DIR}/../silkworm/silkworm/core/rlp/encode.cpp
    ${CMAKE_SOURCE_DIR}/../silkworm/silkworm/core/rlp/decode.cpp
    ${CMAKE_SOURCE_DIR}/../silkworm/silkworm/core/types/block.cpp
    ${CMAKE_SOURCE_DIR}/../silkworm/silkworm/core/types/withdrawal.cpp
    ${CMAKE_SOURCE_DIR}/../silkworm/silkworm/core/types/transaction.cpp
    ${CMAKE_SOURCE_DIR}/../silkworm/silkworm/core/types/account.cpp
    ${CMAKE_SOURCE_DIR}/../silkworm/silkworm/core/types/y_parity_and_chain_id.cpp
    ${CMAKE_SOURCE_DIR}/../silkworm/silkworm/core/common/util.cpp
    ${CMAKE_SOURCE_DIR}/../silkworm/silkworm/core/common/endian.cpp
    ${CMAKE_SOURCE_DIR}/../silkworm/silkworm/core/common/assert.cpp
    ${CMAKE_SOURCE_DIR}/../silkworm/silkworm/core/execution/address.cpp
    ${CMAKE_SOURCE_DIR}/../silkworm/silkworm/core/crypto/ecdsa.c
    ${CMAKE_SOURCE_DIR}/../silkworm/silkworm/infra/common/stopwatch.cpp
    ${CMAKE_SOURCE_DIR}/../silkworm/third_party/ethash/lib/keccak/keccak.c
    ${CMAKE_SOURCE_DIR}/../silkworm/third_party/ethash/lib/ethash/ethash.cpp
    ${CMAKE_SOURCE_DIR}/../silkworm/third_party/ethash/lib/ethash/primes.c
)

add_test(NAME consensus_tests COMMAND unit_test --report_level=detailed --color_output --run_test=evm_runtime_tests -- --eos-vm-oc)

add_test(NAME unit_tests COMMAND unit_test --report_level=detailed --color_output --run_test=!evm_runtime_tests -- --eos-vm-oc)
