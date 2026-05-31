cmake_minimum_required(VERSION 3.14)
project(stml VERSION 1.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Library target ----
add_library(stml STATIC
    lexer/lexer.cpp
<<<<<<< Updated upstream
    parser/parser.cpp
    parser/line_tree_builder.cpp
    parser/ast_builder.cpp
=======
<<<<<<< Updated upstream
    parser/parser.cpp
    parser/line_tree_builder.cpp
    parser/ast_builder.cpp
=======
    parser/line_tree_builder.cpp
    parser/ast_builder.cpp
    parser/parser.cpp
>>>>>>> Stashed changes
>>>>>>> Stashed changes
    ast/ast.cpp
    serializer/to_stml.cpp
    serializer/to_json.cpp
    stml.cpp
    stml_stream.cpp
)

target_include_directories(stml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ---- Tests ----
option(STML_BUILD_TESTS "Build tests" ON)
if(STML_BUILD_TESTS)
    enable_testing()

    # Test executables
    add_executable(test_lexer tests/test_lexer.cpp)
    target_link_libraries(test_lexer stml)

    add_executable(test_parser tests/test_parser.cpp)
    target_link_libraries(test_parser stml)

    add_executable(test_serializer tests/test_serializer.cpp)
    target_link_libraries(test_serializer stml)

    add_executable(test_regression tests/test_regression.cpp)
    target_link_libraries(test_regression stml)

    add_executable(test_full_roundtrip tests/test_full_roundtrip.cpp)
    target_link_libraries(test_full_roundtrip stml)

    add_executable(test_streaming tests/test_streaming.cpp)
    target_link_libraries(test_streaming stml)
endif()
