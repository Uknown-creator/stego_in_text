add_library(stego_client_lib
    src/crypto.cpp
    src/stego.cpp
    src/net_client.cpp)

target_include_directories(stego_client_lib PUBLIC include)

target_link_libraries(stego_client_lib PUBLIC
    cryptopp::cryptopp
    ixwebsocket::ixwebsocket
    nlohmann_json::nlohmann_json)

add_executable(client src/main.cpp)
target_link_libraries(client PRIVATE stego_client_lib)

add_executable(client_tests
    tests/test_main.cpp
    tests/test_crypto.cpp
    tests/test_stego.cpp)
target_link_libraries(client_tests PRIVATE stego_client_lib doctest::doctest)
target_compile_definitions(client_tests PRIVATE
    STEGO_DICTIONARY_PATH="${CMAKE_SOURCE_DIR}/data/dictionary.txt")

add_test(NAME client_tests COMMAND client_tests)
