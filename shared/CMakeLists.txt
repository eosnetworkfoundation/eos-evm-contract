set(TRUST_EVM_SHARED_DIR ${CMAKE_CURRENT_BINARY_DIR})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tx_meta.hpp.in
               ${TRUST_EVM_SHARED_DIR}/tx_meta.hpp
               @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/record.hpp
               ${TRUST_EVM_SHARED_DIR}/record.hpp
               COPYONLY)