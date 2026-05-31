#include "../lexer/lexer.h"
#include <cassert>
#include <iostream>

using namespace stml;

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; std::cout << "  " << name << "... "; } while(0)
#define OK() do { passed++; std::cout << "OK\n"; } while(0)
#define FAIL(msg) do { std::cout << "FAIL: " << msg << "\n"; } while(0)

int main() {
    Lexer lexer;

    // Test 1: basic key-value
    TEST("basic key-value");
    {
        auto tokens = lexer.tokenize("name: STML");
        assert(tokens.size() >= 3);
        assert(tokens[0].type == TokenType::KEY);
        assert(tokens[0].value == "name");
        assert(tokens[1].type == TokenType::COLON);
        assert(tokens[2].type == TokenType::SCALAR);
        assert(tokens[2].value == "STML");
        OK();
    }

    // Test 2: dash item
    TEST("dash item");
    {
        auto tokens = lexer.tokenize("- item1");
        assert(tokens.size() >= 3);
        assert(tokens[0].type == TokenType::DASH);
        assert(tokens[1].type == TokenType::SCALAR);
        assert(tokens[1].value == "item1");
        OK();
    }

    // Test 3: empty input
    TEST("empty input");
    {
        auto tokens = lexer.tokenize("");
        assert(tokens.size() == 1);
        assert(tokens[0].type == TokenType::END);
        OK();
    }

    // Test 4: comment line skipped
    TEST("comment skipped");
    {
        auto tokens = lexer.tokenize("# comment\nkey: val");
        assert(tokens.size() >= 4);
        assert(tokens[0].type == TokenType::KEY);
        assert(tokens[0].value == "key");
        OK();
    }

    // Test 5: doc separator
    TEST("doc separator");
    {
        auto tokens = lexer.tokenize("---\nkey: val");
        bool found_sep = false;
        for (auto& t : tokens) {
            if (t.type == TokenType::DOC_SEPARATOR) { found_sep = true; break; }
        }
        assert(found_sep);
        OK();
    }

    // Test 6: indentation
    TEST("indent/dedent");
    {
        auto tokens = lexer.tokenize("parent:\n  child: val");
        bool has_indent = false, has_dedent = false;
        for (auto& t : tokens) {
            if (t.type == TokenType::INDENT) has_indent = true;
            if (t.type == TokenType::DEDENT) has_dedent = true;
        }
        assert(has_indent);
        assert(has_dedent);
        OK();
    }

    // Test 7: null value
    TEST("null value");
    {
        auto tokens = lexer.tokenize("key: null");
        assert(tokens.size() >= 3);
        assert(tokens[2].type == TokenType::NULL_);
        OK();
    }

    // Test 8: bare key
    TEST("bare key");
    {
        auto tokens = lexer.tokenize("standalone");
        assert(tokens.size() >= 2);
        assert(tokens[0].type == TokenType::BARE_KEY);
        assert(tokens[0].value == "standalone");
        OK();
    }

    // Test 9: multiple lines
    TEST("multiple lines");
    {
        auto tokens = lexer.tokenize("a: 1\nb: 2\nc: 3");
        int key_count = 0;
        for (auto& t : tokens) {
            if (t.type == TokenType::KEY) key_count++;
        }
        assert(key_count == 3);
        OK();
    }

    // Test 10: dash with key-value
    TEST("dash with key-value");
    {
        auto tokens = lexer.tokenize("- key: value");
        assert(tokens[0].type == TokenType::DASH);
        assert(tokens[1].type == TokenType::KEY);
        assert(tokens[1].value == "key");
        assert(tokens[2].type == TokenType::COLON);
        assert(tokens[3].type == TokenType::SCALAR);
        assert(tokens[3].value == "value");
        OK();
    }

    std::cout << "\n" << passed << "/" << tests << " tests passed\n";
    return passed == tests ? 0 : 1;
}
