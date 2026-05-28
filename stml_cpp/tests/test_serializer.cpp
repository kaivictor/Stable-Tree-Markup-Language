/// Round-trip tests for the STML serializer.

#include <cassert>
#include <iostream>
#include <string>

#include "stml.h"

using namespace stml;

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        ++tests_run; \
        try { \
            test_##name(); \
            ++tests_passed; \
            std::cout << "  PASS " << #name << "\n"; \
        } catch (const std::exception& e) { \
            std::cout << "  FAIL " << #name << ": " << e.what() << "\n"; \
        } \
    } while(0)

// =========================================================================
// Basic serialization tests
// =========================================================================

static void test_serialize_null() {
    AstNode node;
    std::string result = serialize_node(node, 0);
    assert(result == "null");
}

static void test_serialize_string() {
    AstNode node(std::string("hello"));
    std::string result = serialize_node(node, 0);
    assert(result == "\"hello\"");
}

static void test_serialize_string_with_quote() {
    AstNode node(std::string("he\"llo"));
    std::string result = serialize_node(node, 0);
    assert(result == "\"he\\\"llo\"");
}

static void test_serialize_simple_map() {
    AstMap m;
    m.emplace_back("key", AstNode(std::string("value")));
    AstNode node(std::move(m));
    std::string result = serialize_node(node, 0);
    assert(result == "\"key\": \"value\"");
}

static void test_serialize_nested_map() {
    AstMap inner;
    inner.emplace_back("a", AstNode(std::string("1")));
    inner.emplace_back("b", AstNode(std::string("2")));

    AstMap outer;
    outer.emplace_back("parent", AstNode(std::move(inner)));

    AstNode node(std::move(outer));
    std::string result = serialize_node(node, 0);
    // Should contain indented children
    assert(result.find("\"parent\":") != std::string::npos);
    assert(result.find("  \"a\": \"1\"") != std::string::npos);
    assert(result.find("  \"b\": \"2\"") != std::string::npos);
}

static void test_serialize_list() {
    AstList lst;
    lst.push_back(AstNode(std::string("a")));
    lst.push_back(AstNode(std::string("b")));
    lst.push_back(AstNode(std::string("c")));
    AstNode node(std::move(lst));
    std::string result = serialize_node(node, 0);
    assert(result.find("- \"a\"") != std::string::npos);
    assert(result.find("- \"b\"") != std::string::npos);
    assert(result.find("- \"c\"") != std::string::npos);
}

static void test_serialize_inline_list() {
    AstList inner;
    inner.push_back(AstNode(std::string("x")));
    inner.push_back(AstNode(std::string("y")));

    AstList outer;
    outer.push_back(AstNode(std::move(inner)));

    AstNode node(std::move(outer));
    std::string result = serialize_node(node, 0);
    assert(result.find("[\"x\", \"y\"]") != std::string::npos);
}

// =========================================================================
// Round-trip tests (STML → AST → STML → AST)
// =========================================================================

static void test_roundtrip_simple() {
    std::string input = "\"key\": \"value\"\n";
    auto [ast1, w1] = loads(input);
    std::string output = dumps(ast1);
    auto [ast2, w2] = loads(output);
    assert(ast1 == ast2);
}

static void test_roundtrip_nested() {
    std::string input =
        "\"outer\":\n"
        "  \"inner\": \"value\"\n";
    auto [ast1, w1] = loads(input);
    std::string output = dumps(ast1);
    auto [ast2, w2] = loads(output);
    assert(ast1 == ast2);
}

static void test_roundtrip_sequence() {
    std::string input =
        "\"items\":\n"
        "  - \"a\"\n"
        "  - \"b\"\n"
        "  - \"c\"\n";
    auto [ast1, w1] = loads(input);
    std::string output = dumps(ast1);
    auto [ast2, w2] = loads(output);
    assert(ast1 == ast2);
}

static void test_roundtrip_null() {
    std::string input = "\"key\": null\n";
    auto [ast1, w1] = loads(input);
    std::string output = dumps(ast1);
    auto [ast2, w2] = loads(output);
    assert(ast1 == ast2);
}

static void test_serialize_multi_key_sequence() {
    // Build AST: {items: [{k1: v1, k2: v2}]}
    AstMap inner;
    inner.emplace_back("k1", AstNode(std::string("v1")));
    inner.emplace_back("k2", AstNode(std::string("v2")));

    AstList seq;
    seq.push_back(AstNode(std::move(inner)));

    AstMap doc;
    doc.emplace_back("items", AstNode(std::move(seq)));

    std::string result = serialize_node(AstNode(std::move(doc)), 0);
    // Should produce multi-key output
    assert(result.find("- \"k1\": \"v1\"") != std::string::npos);
    assert(result.find("  \"k2\": \"v2\"") != std::string::npos);
}

static void test_roundtrip_multi_key_sequence() {
    std::string input =
        "\"items\":\n"
        "  - \"k1\": \"v1\"\n"
        "    \"k2\": \"v2\"\n";
    auto [ast1, w1] = loads(input);
    std::string output = dumps(ast1);
    auto [ast2, w2] = loads(output);
    assert(ast1 == ast2);
}

static void test_roundtrip_empty() {
    // Build minimal AST: empty docs list
    AstList docs;
    AstMap wrapper;
    wrapper.emplace_back("docs", AstNode(std::move(docs)));
    AstNode ast(std::move(wrapper));

    std::string output = dumps(ast);
    auto [ast2, w2] = loads(output);
    assert(ast == ast2);
}

// =========================================================================
// Main
// =========================================================================

int main() {
    std::cout << "Serializer tests:\n";

    TEST(serialize_null);
    TEST(serialize_string);
    TEST(serialize_string_with_quote);
    TEST(serialize_simple_map);
    TEST(serialize_nested_map);
    TEST(serialize_list);
    TEST(serialize_inline_list);
    TEST(serialize_multi_key_sequence);

    std::cout << "\nRound-trip tests:\n";
    TEST(roundtrip_simple);
    TEST(roundtrip_nested);
    TEST(roundtrip_sequence);
    TEST(roundtrip_null);
    TEST(roundtrip_empty);
    TEST(roundtrip_multi_key_sequence);

    std::cout << "\n" << tests_passed << "/" << tests_run << " passed\n";
    return tests_passed == tests_run ? 0 : 1;
}
