#include "../stml.h"
#include "test_json.h"
#include <cassert>
#include <iostream>

using namespace stml;

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; std::cout << "  " << name << "... "; } while(0)
#define OK() do { passed++; std::cout << "OK\n"; } while(0)
#define FAIL(msg) do { std::cout << "FAIL: " << msg << "\n"; } while(0)

#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return 1; } } while(0)

int main() {
    // Test 1: empty input → {"docs": []}
    TEST("empty input");
    {
        AstList docs = loads("");
        CHECK(docs.items.empty(), "expected empty docs");
        OK();
    }

    // Test 2: single key-value
    TEST("single key-value");
    {
        AstList docs = loads("name: STML");
        CHECK(docs.items.size() == 1, "expected 1 doc");
        const AstMap* doc = docs.items[0].as_map();
        CHECK(doc != nullptr, "expected map");
        CHECK(doc->size() == 1, "expected 1 entry");
        CHECK(doc->at(0).first == "name", "key mismatch");
        CHECK(doc->at(0).second.is_scalar(), "expected scalar");
        CHECK(doc->at(0).second.as_scalar()->value == "STML", "value mismatch");
        OK();
    }

    // Test 3: simple sequence
    TEST("simple sequence");
    {
        AstList docs = loads("- a\n- b\n- c");
        CHECK(docs.items.size() == 1, "expected 1 doc");
        const AstList* seq = docs.items[0].as_list();
        CHECK(seq != nullptr, "expected list");
        CHECK(seq->items.size() == 3, "expected 3 items");
        OK();
    }

    // Test 4: complex sequence (dash with key:value)
    TEST("complex sequence");
    {
        AstList docs = loads("- key1: val1\n- key2: val2");
        CHECK(docs.items.size() == 1, "expected 1 doc");
        const AstList* seq = docs.items[0].as_list();
        CHECK(seq != nullptr, "expected list");
        CHECK(seq->items.size() == 2, "expected 2 items");
        const AstMap* m0 = seq->items[0].as_map();
        CHECK(m0 != nullptr, "item 0 expected map");
        CHECK(m0->at(0).first == "key1", "key1 mismatch");
        OK();
    }

    // Test 5: nested mapping
    TEST("nested mapping");
    {
        AstList docs = loads("parent:\n  child1: v1\n  child2: v2");
        CHECK(docs.items.size() == 1, "expected 1 doc");
        const AstMap* doc = docs.items[0].as_map();
        CHECK(doc != nullptr, "expected map");
        CHECK(doc->size() == 1, "expected 1 entry");
        CHECK(doc->at(0).first == "parent", "key mismatch");
        const AstMap* child = doc->at(0).second.as_map();
        CHECK(child != nullptr, "expected child map");
        CHECK(child->size() == 2, "expected 2 children");
        OK();
    }

    // Test 6: complex sequence with sibling keys
    TEST("complex seq with siblings");
    {
        AstList docs = loads("- key1: val1\n  extra: val2\n- key3: val3");
        CHECK(docs.items.size() == 1, "expected 1 doc");
        const AstList* seq = docs.items[0].as_list();
        CHECK(seq != nullptr, "expected list");
        CHECK(seq->items.size() == 2, "expected 2 items");
        const AstMap* m0 = seq->items[0].as_map();
        CHECK(m0->size() == 2, "expected 2 entries in first map");
        CHECK(m0->at(1).first == "extra", "sibling key mismatch");
        OK();
    }

    // Test 7: dash empty with children (simple sequence)
    TEST("dash empty with children");
    {
        AstList docs = loads("-\n  inner: val");
        CHECK(docs.items.size() == 1, "expected 1 doc");
        const AstList* seq = docs.items[0].as_list();
        CHECK(seq != nullptr, "expected list");
        CHECK(seq->items.size() == 1, "expected 1 item");
        const AstMap* inner = seq->items[0].as_map();
        CHECK(inner != nullptr, "expected map from children");
        CHECK(inner->at(0).first == "inner", "inner key mismatch");
        OK();
    }

    // Test 8: multi-document
    TEST("multi-document");
    {
        AstList docs = loads("---\nkey1: val1\n---\nkey2: val2");
        CHECK(docs.items.size() == 2, "expected 2 docs");
        OK();
    }

    // Test 9: bare key
    TEST("bare key");
    {
        AstList docs = loads("standalone");
        CHECK(docs.items.size() == 1, "expected 1 doc");
        const AstMap* doc = docs.items[0].as_map();
        CHECK(doc != nullptr, "expected map");
        CHECK(doc->at(0).first == "standalone", "bare key mismatch");
        CHECK(doc->at(0).second.is_null(), "expected null value");
        OK();
    }

    // Test 10: inline list
    TEST("inline list value");
    {
        AstList docs = loads("items: [a, b, c]");
        CHECK(docs.items.size() == 1, "expected 1 doc");
        const AstMap* doc = docs.items[0].as_map();
        CHECK(doc->size() == 1, "expected 1 entry");
        OK();
    }

    std::cout << "\n" << passed << "/" << tests << " tests passed\n";
    return passed == tests ? 0 : 1;
}
