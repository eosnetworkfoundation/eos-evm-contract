cmake_minimum_required(VERSION 3.16)
project(evm_runtime)

include(ExternalProject)
# if no cdt root is given use default path
if(CDT_ROOT STREQUAL "" OR NOT CDT_ROOT)
   find_package(cdt)
endif()

option(WITH_TEST_ACTIONS
   "Enables actions for unit testing" OFF)

option(WITH_LOGTIME
   "Use `logtime` instrisic to log the time spent in transaction execution" OFF)

ExternalProject_Add(
   evm_runtime_project
   SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src
   BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/evm_runtime
   CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release
              -DCMAKE_TOOLCHAIN_FILE=${CDT_ROOT}/lib/cmake/cdt/CDTWasmToolchain.cmake
              -DWITH_TEST_ACTIONS=${WITH_TEST_ACTIONS}
              -DWITH_LOGTIME=${WITH_LOGTIME}
              -DTRUST_EVM_SHARED_DIR=${CMAKE_CURRENT_BINARY_DIR}/../shared
              -DTRUST_EVM_MAJOR=${PROJECT_VERSION_MAJOR}
              -DTRUST_EVM_MINOR=${PROJECT_VERSION_MINOR}
              -DTRUST_EVM_PATCH=${PROJECT_VERSION_PATCH}
   UPDATE_COMMAND ""
   PATCH_COMMAND ""
   TEST_COMMAND ""
   INSTALL_COMMAND ""
   BUILD_ALWAYS 1
)