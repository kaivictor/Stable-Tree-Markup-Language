/// Unit tests for the STML parser.

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
// Basic parsing tests
// =========================================================================

static void test_simple_mapping() {
    auto [ast, warnings] = loads("key: value\n");
    assert(ast.is_map());
    const auto& docs = *ast.as_map();
    assert(map_find(docs, "docs") != nullptr);
    const auto& docs_list = *map_find(docs, "docs")->as_list();
    assert(docs_list.size() == 1);
    const auto& doc1 = *docs_list[0].as_map();
    assert(map_find(doc1, "key") != nullptr);
    assert(map_find(doc1, "key")->is_string());
    assert(*map_find(doc1, "key")->as_string() == "value");
}

static void test_nested_mapping() {
    auto [ast, warnings] = loads(
        "parent:\n"
        "  child1: value1\n"
        "  child2: value2\n"
    );
    const auto& docs = *ast.as_map();
    const auto& doc1 = *map_find(docs, "docs")->as_list()->at(0).as_map();
    assert(map_find(doc1, "parent") != nullptr);
    const auto& parent = *map_find(doc1, "parent")->as_map();
    assert(map_find(parent, "child1") != nullptr);
    assert(*map_find(parent, "child1")->as_string() == "value1");
    assert(*map_find(parent, "child2")->as_string() == "value2");
}

static void test_null_value() {
    auto [ast, warnings] = loads("key: null\n");
    const auto& docs = *ast.as_map();
    const auto& doc1 = *map_find(docs, "docs")->as_list()->at(0).as_map();
    assert(map_find(doc1, "key") != nullptr);
    assert(map_find(doc1, "key")->is_null());
}

static void test_tilde_null() {
    auto [ast, warnings] = loads("key: ~\n");
    const auto& docs = *ast.as_map();
    const auto& doc1 = *map_find(docs, "docs")->as_list()->at(0).as_map();
    assert(map_find(doc1, "key")->is_null());
}

static void test_bare_key() {
    auto [ast, warnings] = loads("just_a_key\n");
    const auto& docs = *ast.as_map();
    const auto& doc1 = *map_find(docs, "docs")->as_list()->at(0).as_map();
    assert(map_find(doc1, "just_a_key") != nullptr);
    assert(map_find(doc1, "just_a_key")->is_null());
}

// =========================================================================
// Sequence tests
// =========================================================================

static void test_flat_sequence() {
    auto [ast, warnings] = loads(
        "items:\n"
        "  - item1\n"
        "  - item2\n"
        "  - item3\n"
    );
    const auto& docs = *ast.as_map();
    const auto& doc1 = *map_find(docs, "docs")->as_list()->at(0).as_map();
    const auto& items = *map_find(doc1, "items")->as_list();
    assert(items.size() == 3);
    assert(*items[0].as_string() == "item1");
    assert(*items[1].as_string() == "item2");
    assert(*items[2].as_string() == "item3");
}

static void test_sequence_with_mapping() {
    auto [ast, warnings] = loads(
        "items:\n"
        "  - key1: value1\n"
        "  - key2: value2\n"
    );
    const auto& docs = *ast.as_map();
    const auto& doc1 = *map_find(docs, "docs")->as_list()->at(0).as_map();
    const auto& items = *map_find(doc1, "items")->as_list();
    assert(items.size() == 2);
    assert(items[0].is_map());
    assert(items[1].is_map());
    const auto& m1 = *items[0].as_map();
    const auto& m2 = *items[1].as_map();
    assert(*map_find(m1, "key1")->as_string() == "value1");
    assert(*map_find(m2, "key2")->as_string() == "value2");
}

static void test_empty_sequence() {
    auto [ast, warnings] = loads(
        "items:\n"
        "  -\n"
        "  -\n"
    );
    const auto& docs = *ast.as_map();
    const auto& doc1 = *map_find(docs, "docs")->as_list()->at(0).as_map();
    const auto& items = *map_find(doc1, "items")->as_list();
    assert(items.size() == 2);
    assert(items[0].is_null());
    assert(items[1].is_null());
}

// =========================================================================
// Inline list tests
// =========================================================================

static void test_inline_list() {
    auto [ast, warnings] = loads("key: [a, b, c]\n");
    const auto& docs = *ast.as_map();
    const auto& doc1 = *map_find(docs, "docs")->as_list()->at(0).as_map();
    const auto& lst = *map_find(doc1, "key")->as_list();
    assert(lst.size() == 3);
    assert(*lst[0].as_string() == "a");
    assert(*lst[1].as_string() == "b");
    assert(*lst[2].as_string() == "c");
}

// =========================================================================
// Multiline string tests
// =========================================================================

static void test_multiline_string() {
    auto [ast, warnings] = loads(
        "text: {\n"
        "line1\n"
        "line2\n"
        "}\n"
    );
    const auto& docs = *ast.as_map();
    const auto& doc1 = *map_find(docs, "docs")->as_list()->at(0).as_map();
    assert(*map_find(doc1, "text")->as_string() == "line1\nline2");
}

// =========================================================================
// Document tests
// =========================================================================

static void test_multi_document() {
    auto [ast, warnings] = loads(
        "doc1_key: value1\n"
        "---\n"
        "doc2_key: value2\n"
    );
    const auto& docs = *ast.as_map();
    const auto& docs_list = *map_find(docs, "docs")->as_list();
    assert(docs_list.size() == 2);
}

static void test_empty_document() {
    // Two consecutive separators: produces one null doc + one mapped doc
    auto [ast, warnings] = loads("---\n---\nkey: value\n");
    const auto& docs = *ast.as_map();
    const auto& docs_list = *map_find(docs, "docs")->as_list();
    assert(docs_list.size() == 2);
    assert(docs_list[0].is_null());
    assert(docs_list[1].is_map());
}

static void test_empty_input() {
    auto [ast, warnings] = loads("");
    const auto& docs = *ast.as_map();
    const auto& docs_list = *map_find(docs, "docs")->as_list();
    assert(docs_list.size() == 0);
}

// =========================================================================
// Complex sequence tests
// =========================================================================

static void test_mixed_sequence() {
    auto [ast, warnings] = loads(
        "items:\n"
        "  - key1: value1\n"
        "  - plain_value\n"
        "  - null_value: null\n"
    );
    const auto& docs = *ast.as_map();
    const auto& doc1 = *map_find(docs, "docs")->as_list()->at(0).as_map();
    const auto& items = *map_find(doc1, "items")->as_list();
    assert(items.size() == 3);
    // First: mapping with key1: value1
    assert(items[0].is_map());
    assert(*map_find(*items[0].as_map(), "key1")->as_string() == "value1");
    // Second: mapping with plain_value: null (complex mode)
    assert(items[1].is_map());
    // Third: mapping with null_value: null
    assert(items[2].is_map());
}

// =========================================================================
// Nested list test
// =========================================================================

static void test_nested_lists() {
    auto [ast, warnings] = loads(
        "outer:\n"
        "  - [a, b]\n"
        "  - [c, d]\n"
    );
    const auto& docs = *ast.as_map();
    const auto& doc1 = *map_find(docs, "docs")->as_list()->at(0).as_map();
    const auto& outer = *map_find(doc1, "outer")->as_list();
    assert(outer.size() == 2);
    assert(outer[0].is_list());
    assert(outer[1].is_list());
    assert((*outer[0].as_list()).size() == 2);
    assert((*outer[1].as_list()).size() == 2);
}

// =========================================================================
// Multi-key mapping in sequence tests
// =========================================================================

static void test_multi_key_sequence_inline() {
    // Multi-key mapping with inline values
    auto [ast, warnings] = loads(
        "items:\n"
        "  - key1: value1\n"
        "    key2: value2\n"
    );
    const auto& docs = *ast.as_map();
    const auto& doc1 = *map_find(docs, "docs")->as_list()->at(0).as_map();
    const auto& items = *map_find(doc1, "items")->as_list();
    assert(items.size() == 1);
    assert(items[0].is_map());
    const auto& m = *items[0].as_map();
    assert(m.size() == 2);
    assert(*map_find(m, "key1")->as_string() == "value1");
    assert(*map_find(m, "key2")->as_string() == "value2");
}

static void test_multi_key_sequence_mixed() {
    // Multi-key mapping: first has inline value, second is bare (null)
    auto [ast, warnings] = loads(
        "items:\n"
        "  - key1: value1\n"
        "    key2\n"
    );
    const auto& docs = *ast.as_map();
    const auto& doc1 = *map_find(docs, "docs")->as_list()->at(0).as_map();
    const auto& items = *map_find(doc1, "items")->as_list();
    assert(items.size() == 1);
    const auto& m = *items[0].as_map();
    assert(m.size() == 2);
    assert(*map_find(m, "key1")->as_string() == "value1");
    assert(map_find(m, "key2")->is_null());
}

static void test_multi_key_sequence_all_null() {
    // Multi-key mapping: all keys without values
    auto [ast, warnings] = loads(
        "items:\n"
        "  - key1:\n"
        "    key2:\n"
        "    key3:\n"
    );
    const auto& docs = *ast.as_map();
    const auto& doc1 = *map_find(docs, "docs")->as_list()->at(0).as_map();
    const auto& items = *map_find(doc1, "items")->as_list();
    assert(items.size() == 1);
    const auto& m = *items[0].as_map();
    assert(m.size() == 3);
    assert(map_find(m, "key1")->is_null());
    assert(map_find(m, "key2")->is_null());
    assert(map_find(m, "key3")->is_null());
}

static void test_multi_key_sequence_three_siblings() {
    // Three sibling keys with inline values
    auto [ast, warnings] = loads(
        "items:\n"
        "  - k1: v1\n"
        "    k2: v2\n"
        "    k3: v3\n"
    );
    const auto& docs = *ast.as_map();
    const auto& doc1 = *map_find(docs, "docs")->as_list()->at(0).as_map();
    const auto& items = *map_find(doc1, "items")->as_list();
    assert(items.size() == 1);
    const auto& m = *items[0].as_map();
    assert(m.size() == 3);
    assert(*map_find(m, "k1")->as_string() == "v1");
    assert(*map_find(m, "k2")->as_string() == "v2");
    assert(*map_find(m, "k3")->as_string() == "v3");
}

static void test_multi_key_sequence_with_block() {
    // Multi-key mapping with a sibling that has a block value
    auto [ast, warnings] = loads(
        "items:\n"
        "  - key1: value1\n"
        "    key2:\n"
        "      nested: val\n"
    );
    const auto& docs = *ast.as_map();
    const auto& doc1 = *map_find(docs, "docs")->as_list()->at(0).as_map();
    const auto& items = *map_find(doc1, "items")->as_list();
    assert(items.size() == 1);
    const auto& m = *items[0].as_map();
    assert(m.size() == 2);
    assert(*map_find(m, "key1")->as_string() == "value1");
    assert(map_find(m, "key2")->is_map());
    const auto& inner = *map_find(m, "key2")->as_map();
    assert(*map_find(inner, "nested")->as_string() == "val");
}

// =========================================================================
// Main
// =========================================================================

int main() {
    std::cout << "Parser tests:\n";

    TEST(simple_mapping);
    TEST(nested_mapping);
    TEST(null_value);
    TEST(tilde_null);
    TEST(bare_key);
    TEST(flat_sequence);
    TEST(sequence_with_mapping);
    TEST(empty_sequence);
    TEST(inline_list);
    TEST(multiline_string);
    TEST(multi_document);
    TEST(empty_document);
    TEST(empty_input);
    TEST(mixed_sequence);
    TEST(nested_lists);
    TEST(multi_key_sequence_inline);
    TEST(multi_key_sequence_mixed);
    TEST(multi_key_sequence_all_null);
    TEST(multi_key_sequence_three_siblings);
    TEST(multi_key_sequence_with_block);

    std::cout << "\n" << tests_passed << "/" << tests_run << " passed\n";
    return tests_passed == tests_run ? 0 : 1;
}
