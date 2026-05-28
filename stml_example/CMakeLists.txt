cmake_minimum_required(VERSION 3.14)
project(stml_examples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to stml_cpp
set(STML_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../stml_cpp")
add_subdirectory(${STML_ROOT} ${CMAKE_BINARY_DIR}/stml_build)

# ---- Examples ----
function(add_example name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE stml)
endfunction()

add_example(basic_usage)
add_example(config_reader)
add_example(converter)
add_example(diagnostics)
add_example(ast_manipulation)
