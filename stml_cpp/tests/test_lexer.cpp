/// Unit tests for the STML lexer.

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

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
// Indentation tests
// =========================================================================

static void test_indent_basic() {
    std::string input = "key: value\n  sub: value2\n";
    auto [tokens, warnings] = tokenize(input);

    // Should have: KEY, COLON, SCALAR, NEWLINE, INDENT, KEY, COLON, SCALAR, NEWLINE, DEDENT, END
    bool has_indent = false;
    bool has_dedent = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::INDENT) has_indent = true;
        if (t.type == TokenType::DEDENT) has_dedent = true;
    }
    assert(has_indent && "Should have INDENT token");
    assert(has_dedent && "Should have DEDENT token");
}

static void test_indent_mixed() {
    std::string input = "key1: value1\n  key2: value2\n key3: value3\n";
    auto [tokens, warnings] = tokenize(input);

    int indent_count = 0;
    int dedent_count = 0;
    for (const auto& t : tokens) {
        if (t.type == TokenType::INDENT) ++indent_count;
        if (t.type == TokenType::DEDENT) ++dedent_count;
    }
    // Should have at least one INDENT and DEDENT
    assert(indent_count >= 1 && "Expected INDENT tokens");
    assert(dedent_count >= 1 && "Expected DEDENT tokens");
}

// =========================================================================
// Basic token tests
// =========================================================================

static void test_key_value() {
    std::string input = "key: value\n";
    auto [tokens, warnings] = tokenize(input);

    bool has_key = false, has_colon = false, has_scalar = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::KEY && std::get<std::string>(t.value) == "key") has_key = true;
        if (t.type == TokenType::COLON) has_colon = true;
        if (t.type == TokenType::SCALAR && std::get<std::string>(t.value) == "value") has_scalar = true;
    }
    assert(has_key);
    assert(has_colon);
    assert(has_scalar);
}

static void test_null_values() {
    std::string input = "key1: null\nkey2: ~\n";
    auto [tokens, warnings] = tokenize(input);

    int null_count = 0;
    for (const auto& t : tokens) {
        if (t.type == TokenType::NULL_) ++null_count;
    }
    assert(null_count == 2);
}

static void test_bare_key() {
    std::string input = "just_a_key\n";
    auto [tokens, warnings] = tokenize(input);

    bool has_bare = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::BARE_KEY && std::get<std::string>(t.value) == "just_a_key")
            has_bare = true;
    }
    assert(has_bare);
}

static void test_sequence() {
    std::string input = "list:\n  - item1\n  - item2\n";
    auto [tokens, warnings] = tokenize(input);

    int dash_count = 0;
    int scalar_count = 0;
    for (const auto& t : tokens) {
        if (t.type == TokenType::DASH) ++dash_count;
        if (t.type == TokenType::SCALAR) ++scalar_count;
    }
    assert(dash_count == 2);
    assert(scalar_count == 2);
}

// =========================================================================
// Comment tests
// =========================================================================

static void test_comments() {
    std::string input = "# This is a comment\nkey: value\n";
    auto [tokens, warnings] = tokenize(input);

    // Comments should be skipped, so we should have KEY, COLON, SCALAR
    bool has_key = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::KEY) has_key = true;
    }
    assert(has_key);
}

// =========================================================================
// Quoted key tests
// =========================================================================

static void test_quoted_key() {
    std::string input = "\"complex key\": value\n";
    auto [tokens, warnings] = tokenize(input);

    bool has_key = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::KEY && std::get<std::string>(t.value) == "complex key")
            has_key = true;
    }
    assert(has_key);
}

// =========================================================================
// Document separator tests
// =========================================================================

static void test_doc_separator() {
    std::string input = "key1: value1\n---\nkey2: value2\n";
    auto [tokens, warnings] = tokenize(input);

    bool has_sep = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::DOC_SEPARATOR) has_sep = true;
    }
    assert(has_sep);
}

// =========================================================================
// Inline list tests
// =========================================================================

static void test_inline_list() {
    std::string input = "key: [a, b, c]\n";
    auto [tokens, warnings] = tokenize(input);

    bool has_list = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::INLINE_LIST) {
            has_list = true;
            const auto& elems = std::get<std::vector<InlineElem>>(t.value);
            assert(elems.size() == 3);
            assert(elems[0].has_value() && *elems[0] == "a");
            assert(elems[1].has_value() && *elems[1] == "b");
            assert(elems[2].has_value() && *elems[2] == "c");
        }
    }
    assert(has_list);
}

// =========================================================================
// Multiline string tests
// =========================================================================

static void test_multiline_basic() {
    std::string input = "text: {\nline1\nline2\n}\n";
    auto [tokens, warnings] = tokenize(input);

    bool has_ml = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::MULTILINE_STRING) {
            has_ml = true;
            const auto& val = std::get<std::string>(t.value);
            assert(val == "line1\nline2");
        }
    }
    assert(has_ml);
}

static void test_multiline_dash() {
    // Content inside multiline: '-' and '---' are NOT document separators
    std::string input = "text: {\n---\n---\n}\n";
    auto [tokens, warnings] = tokenize(input);

    bool has_ml = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::MULTILINE_STRING) {
            has_ml = true;
            const auto& val = std::get<std::string>(t.value);
            assert(val == "---\n---");
        }
    }
    assert(has_ml);
}

// =========================================================================
// Escape tests
// =========================================================================

static void test_escapes() {
    std::string input = "key: \"line1\\nline2\"\n";
    auto [tokens, warnings] = tokenize(input);

    bool has_scalar = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::SCALAR) {
            has_scalar = true;
            const auto& val = std::get<std::string>(t.value);
            assert(val == "line1\nline2");
        }
    }
    assert(has_scalar);
}

// =========================================================================
// Empty content tests
// =========================================================================

static void test_empty_lines() {
    std::string input = "key: value\n\n\nkey2: value2\n";
    auto [tokens, warnings] = tokenize(input);

    int key_count = 0;
    for (const auto& t : tokens) {
        if (t.type == TokenType::KEY) ++key_count;
    }
    assert(key_count == 2);
}

// =========================================================================
// Main
// =========================================================================

int main() {
    std::cout << "Lexer tests:\n";

    TEST(indent_basic);
    TEST(indent_mixed);
    TEST(key_value);
    TEST(null_values);
    TEST(bare_key);
    TEST(sequence);
    TEST(comments);
    TEST(quoted_key);
    TEST(doc_separator);
    TEST(inline_list);
    TEST(multiline_basic);
    TEST(multiline_dash);
    TEST(escapes);
    TEST(empty_lines);

    std::cout << "\n" << tests_passed << "/" << tests_run << " passed\n";
    return tests_passed == tests_run ? 0 : 1;
}
