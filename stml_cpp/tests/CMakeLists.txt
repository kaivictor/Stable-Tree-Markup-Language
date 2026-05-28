# ---- Test helper: minimal JSON parser ----
add_library(test_json_parser STATIC
    test_json.cpp
)
target_include_directories(test_json_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR})

# ---- Unit tests ----
function(add_stml_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE stml test_json_parser)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_stml_test(test_lexer)
add_stml_test(test_parser)
add_stml_test(test_serializer)

# test_streaming needs TestData path argument
add_executable(test_streaming test_streaming.cpp)
target_link_libraries(test_streaming PRIVATE stml test_json_parser)
target_include_directories(test_streaming PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR})
add_test(NAME test_streaming COMMAND test_streaming ${CMAKE_SOURCE_DIR}/../TestData)

# test_regression / test_full_roundtrip need the TestData path argument
add_executable(test_regression test_regression.cpp)
target_link_libraries(test_regression PRIVATE stml test_json_parser)
target_include_directories(test_regression PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR})
add_test(NAME test_regression COMMAND test_regression ${CMAKE_SOURCE_DIR}/../TestData)

add_executable(test_full_roundtrip test_full_roundtrip.cpp)
target_link_libraries(test_full_roundtrip PRIVATE stml test_json_parser)
target_include_directories(test_full_roundtrip PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR})
add_test(NAME test_full_roundtrip COMMAND test_full_roundtrip ${CMAKE_SOURCE_DIR}/../TestData)
