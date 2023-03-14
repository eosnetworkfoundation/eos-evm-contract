cmake_minimum_required(VERSION 3.16)
project(evm_runtime)

include(ExternalProject)
# if no cdt root is given use default path
if(CDT_ROOT STREQUAL "" OR NOT CDT_ROOT)
   find_package(cdt)
endif()

option(WITH_LOGTIME
   "Use `logtime` instrisic to log the time spent in transaction execution" OFF)

option(BUILD_TESTS
   "Build the contract tests" ON)

ExternalProject_Add(
   evm_runtime_without_test_actions
   SOURCE_DIR ${CMAKE_SOURCE_DIR}/src
   BINARY_DIR ${CMAKE_BINARY_DIR}/evm_runtime_without_test_actions
   CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release
              -DCMAKE_TOOLCHAIN_FILE=${CDT_ROOT}/lib/cmake/cdt/CDTWasmToolchain.cmake
              -DWITH_TEST_ACTIONS=OFF
              -DWITH_LARGE_STACK=OFF
#              -DWITH_LOGTIME=${WITH_LOGTIME}
   UPDATE_COMMAND ""
   PATCH_COMMAND ""
   TEST_COMMAND ""
   INSTALL_COMMAND ""
   BUILD_ALWAYS 1
)

if(BUILD_TESTS)
   message(STATUS "Building contract tests.")

   ExternalProject_Add(
      evm_runtime_with_test_actions
      SOURCE_DIR ${CMAKE_SOURCE_DIR}/src
      BINARY_DIR ${CMAKE_BINARY_DIR}/evm_runtime_with_test_actions
      CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release
                 -DCMAKE_TOOLCHAIN_FILE=${CDT_ROOT}/lib/cmake/cdt/CDTWasmToolchain.cmake
                 -DWITH_TEST_ACTIONS=ON
                 -DWITH_LARGE_STACK=ON
                 -DWITH_LOGTIME=${WITH_LOGTIME}
      UPDATE_COMMAND ""
      PATCH_COMMAND ""
      TEST_COMMAND ""
      INSTALL_COMMAND ""
      BUILD_ALWAYS 1
   )

   enable_testing()
   add_subdirectory(tests)
else()
   message(STATUS "Contracts tests will not be built. To build contracts tests, set BUILD_TESTS to ON.")
endif()