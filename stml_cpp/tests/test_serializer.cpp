#include "../serializer/serializer.h"
#include "test_json.h"
#include <cassert>
#include <iostream>

using namespace stml;

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; std::cout << "  " << name << "... "; } while(0)
#define OK() do { passed++; std::cout << "OK\n"; } while(0)
#define FAIL(msg) do { std::cout << "FAIL: " << msg << "\n"; } while(0)

int main() {
    // Test 1: AST → JSON (basic map)
    TEST("to_json basic map");
    {
        AstMap map;
        map.emplace_back("key", AstNode(AstScalar{"value"}));
        std::string json = to_json(AstNode(map));
        auto parsed = test_json::parse(json);
        assert(parsed.is_map());
        assert(parsed.as_map()->size() == 1);
        assert(parsed.as_map()->at(0).first == "key");
        OK();
    }

    // Test 2: AST → JSON (list)
    TEST("to_json list");
    {
        AstList list;
        list.items.push_back(AstNode(AstScalar{"a"}));
        list.items.push_back(AstNode(AstScalar{"b"}));
        std::string json = to_json(AstNode(list));
        auto parsed = test_json::parse(json);
        assert(parsed.is_list());
        assert(parsed.as_list()->items.size() == 2);
        OK();
    }

    // Test 3: AST → JSON (null)
    TEST("to_json null");
    {
        std::string json = to_json(AstNode(NullNode{}));
        assert(json == "null");
        OK();
    }

    // Test 4: docs_to_json with single doc
    TEST("docs_to_json single doc");
    {
        AstList docs;
        AstMap doc;
        doc.emplace_back("a", AstNode(AstScalar{"1"}));
        docs.items.push_back(AstNode(std::move(doc)));
        std::string json = docs_to_json(docs);
        auto parsed = test_json::parse(json);
        assert(parsed.is_map());
        const auto* inner = map_find(*parsed.as_map(), "docs");
        assert(inner != nullptr);
        assert(inner->is_list());
        assert(inner->as_list()->items.size() == 1);
        OK();
    }

    // Test 5: docs_to_json empty
    TEST("docs_to_json empty");
    {
        AstList docs;
        std::string json = docs_to_json(docs);
        auto parsed = test_json::parse(json);
        const auto* inner = map_find(*parsed.as_map(), "docs");
        assert(inner != nullptr);
        assert(inner->as_list()->items.empty());
        OK();
    }

    // Test 6: to_stml basic map
    TEST("to_stml basic map");
    {
        AstMap map;
        map.emplace_back("key", AstNode(AstScalar{"value"}));
        std::string stml = to_stml(AstNode(map));
        assert(!stml.empty());
        // Should contain key and value
        assert(stml.find("key") != std::string::npos);
        assert(stml.find("value") != std::string::npos);
        OK();
    }

    // Test 7: to_stml list
    TEST("to_stml list");
    {
        AstList list;
        list.items.push_back(AstNode(AstScalar{"a"}));
        list.items.push_back(AstNode(AstScalar{"b"}));
        std::string stml = to_stml(AstNode(list));
        assert(stml.find("- a") != std::string::npos);
        assert(stml.find("- b") != std::string::npos);
        OK();
    }

    // Test 8: to_stml nested
    TEST("to_stml nested map");
    {
        AstMap inner;
        inner.emplace_back("c", AstNode(AstScalar{"d"}));
        AstMap outer;
        outer.emplace_back("a", AstNode(AstScalar{"b"}));
        outer.emplace_back("nested", AstNode(std::move(inner)));
        std::string stml = to_stml(AstNode(outer));
        assert(stml.find("a: b") != std::string::npos);
        assert(stml.find("nested:") != std::string::npos);
        assert(stml.find("c: d") != std::string::npos);
        OK();
    }

    // Test 9: to_stml scalar quoting
    TEST("to_stml scalar quoting");
    {
        AstMap map;
        map.emplace_back("key", AstNode(AstScalar{"value: with colon"}));
        std::string stml = to_stml(AstNode(map));
        // Should be quoted
        assert(stml.find("\"value: with colon\"") != std::string::npos);
        OK();
    }

    // Test 10: docs_to_stml multi-doc
    TEST("docs_to_stml multi-doc");
    {
        AstList docs;
        AstMap doc1;
        doc1.emplace_back("a", AstNode(AstScalar{"1"}));
        docs.items.push_back(AstNode(std::move(doc1)));
        AstMap doc2;
        doc2.emplace_back("b", AstNode(AstScalar{"2"}));
        docs.items.push_back(AstNode(std::move(doc2)));
        std::string stml = docs_to_stml(docs);
        assert(stml.find("---") != std::string::npos);
        OK();
    }

    std::cout << "\n" << passed << "/" << tests << " tests passed\n";
    return passed == tests ? 0 : 1;
}
