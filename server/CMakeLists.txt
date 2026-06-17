add_library(stego_server_broker
    src/broker.cpp)

target_include_directories(stego_server_broker PUBLIC include)

target_link_libraries(stego_server_broker PUBLIC
    nlohmann_json::nlohmann_json)

add_library(stego_server_net
    src/ws_server.cpp)

target_include_directories(stego_server_net PUBLIC include)

target_link_libraries(stego_server_net PUBLIC
    stego_server_broker
    ixwebsocket::ixwebsocket
    nlohmann_json::nlohmann_json)

add_executable(server
    src/main.cpp)

target_link_libraries(server PRIVATE stego_server_net)

add_executable(server_tests
    tests/test_broker.cpp)

target_link_libraries(server_tests PRIVATE
    stego_server_broker
    doctest::doctest)

add_test(NAME server_tests COMMAND server_tests)
