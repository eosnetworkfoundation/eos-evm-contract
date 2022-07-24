#[[
   Copyright 2020 The SilkRpc Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
]]

if(MSVC)
    add_link_options(/STACK:10000000)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_link_options(-Wl,-stack_size -Wl,0x1000000)
endif()

find_package(absl CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(protobuf CONFIG REQUIRED)
if(SILKRPC_USE_MIMALLOC)
    find_package(mimalloc 2.0 REQUIRED)
endif()

add_compile_options(-std=c++2a)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-fcoroutines)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-stdlib=libc++)
endif()

# Silkrpc daemon
set(NODE_LIBRARIES
    trustevm_node-buildinfo
    silkrpc
    absl::flags_parse
    silkinterfaces)

add_library(silkrpc_plugin silkrpc_plugin.cpp)
target_include_directories(silkrpc_plugin PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(silkrpc_plugin PUBLIC ${NODE_LIBRARIES} appbase fc ${Boost_PROGRAM_OPTIONS_LIBRARY})

add_library(silkengine_plugin silk_engine_plugin.cpp)
target_include_directories(silkengine_plugin PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(silkengine_plugin PUBLIC silkworm_core silkworm_node appbase fc ${Boost_PROGRAM_OPTIONS_LIBRARY})

add_library(shipreceiver_plugin ship_receiver_plugin.cpp)
target_include_directories(shipreceiver_plugin PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/external/mandel-abieos/src)
target_link_libraries(shipreceiver_plugin PUBLIC ${NODE_LIBRARIES} appbase fc abieos ${Boost_PROGRAM_OPTIONS_LIBRARY})

add_library(logger_plugin logger_plugin.cpp)
target_include_directories(logger_plugin PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(logger_plugin PUBLIC silkworm_core silkworm_node appbase fc ${Boost_PROGRAM_OPTIONS_LIBRARY})

add_executable(trustevm-node node.cpp)
target_include_directories(trustevm-node PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
target_link_libraries(trustevm-node PRIVATE silkrpc_plugin silkengine_plugin shipreceiver_plugin logger_plugin)

# Unit tests
enable_testing()

find_package(Catch2 CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

file(GLOB_RECURSE SILKRPC_TESTS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/silkrpc/*_test.cpp")
add_executable(unit_test unit_test.cpp ${SILKRPC_TESTS})
target_link_libraries(unit_test silkrpc Catch2::Catch2 GTest::gmock)

include(CTest)
include(Catch)
catch_discover_tests(unit_test)
