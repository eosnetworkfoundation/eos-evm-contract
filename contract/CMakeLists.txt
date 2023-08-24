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

option(WITH_LARGE_STACK
   "Build with 50MB of stack size, needed for unit tests" OFF)

option(WITH_ADMIN_ACTIONS
   "Enables admin actions" ON)

ExternalProject_Add(
   evm_runtime_project
   SOURCE_DIR ${CMAKE_SOURCE_DIR}/src
   BINARY_DIR ${CMAKE_BINARY_DIR}/evm_runtime
   CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release
              -DCMAKE_TOOLCHAIN_FILE=${CDT_ROOT}/lib/cmake/cdt/CDTWasmToolchain.cmake
              -DWITH_TEST_ACTIONS=${WITH_TEST_ACTIONS}
              -DWITH_LOGTIME=${WITH_LOGTIME}
              -DWITH_LARGE_STACK=${WITH_LARGE_STACK}
              -DWITH_ADMIN_ACTIONS=${WITH_ADMIN_ACTIONS}
   UPDATE_COMMAND ""
   PATCH_COMMAND ""
   TEST_COMMAND ""
   INSTALL_COMMAND ""
   BUILD_ALWAYS 1
)
