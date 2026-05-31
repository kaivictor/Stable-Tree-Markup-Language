cmake_minimum_required(VERSION 3.14)
project(stml VERSION 1.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Library target ----
add_library(stml
    lexer/lexer.cpp
    parser/parser.cpp
    parser/line_tree_builder.cpp
    parser/ast_builder.cpp
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
    add_subdirectory(tests)
endif()
