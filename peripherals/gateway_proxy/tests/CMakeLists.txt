cmake_minimum_required(VERSION 3.12)
project(gateway_tests)

set(CMAKE_CXX_STANDARD 20)

find_package(Boost REQUIRED COMPONENTS json unit_test_framework)

add_executable(gateway_tests main.cpp mock_evm.cpp nginx.cpp counting_http_server.cpp http_cannon.cpp)
target_link_libraries(gateway_tests Boost::boost Boost::json Boost::unit_test_framework)

configure_file(../main.mjs             . COPYONLY)
configure_file(../cli.mjs              . COPYONLY)
configure_file(../TrustEVMStaking.json . COPYONLY)

enable_testing()
add_test(NAME gateway_tests COMMAND gateway_tests)
