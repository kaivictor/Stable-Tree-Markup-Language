package stml;

import java.util.List;

public class TestLexer {
    static int testsRun = 0;
    static int testsPassed = 0;

    static void test(String name, Runnable test) {
        testsRun++;
        try {
            test.run();
            testsPassed++;
            System.out.println("  PASS " + name);
        } catch (Exception e) {
            System.out.println("  FAIL " + name + ": " + e.getMessage());
        }
    }

    static void test_indent_basic() {
        String input = "key: value\n  sub: value2\n";
        STMLLexer.TokenizeResult result = STML.tokenize(input);
        boolean hasIndent = false, hasDedent = false;
        for (Token t : result.tokens) {
            if (t.type == TokenType.INDENT) hasIndent = true;
            if (t.type == TokenType.DEDENT) hasDedent = true;
        }
        assert hasIndent : "Should have INDENT token";
        assert hasDedent : "Should have DEDENT token";
    }

    static void test_indent_mixed() {
        String input = "key1: value1\n  key2: value2\n key3: value3\n";
        STMLLexer.TokenizeResult result = STML.tokenize(input);
        int indentCount = 0, dedentCount = 0;
        for (Token t : result.tokens) {
            if (t.type == TokenType.INDENT) indentCount++;
            if (t.type == TokenType.DEDENT) dedentCount++;
        }
        assert indentCount >= 1 : "Expected INDENT tokens";
        assert dedentCount >= 1 : "Expected DEDENT tokens";
    }

    static void test_key_value() {
        STMLLexer.TokenizeResult result = STML.tokenize("key: value\n");
        boolean hasKey = false, hasColon = false, hasScalar = false;
        for (Token t : result.tokens) {
            if (t.type == TokenType.KEY && t.asString().equals("key")) hasKey = true;
            if (t.type == TokenType.COLON) hasColon = true;
            if (t.type == TokenType.SCALAR && t.asString().equals("value")) hasScalar = true;
        }
        assert hasKey;
        assert hasColon;
        assert hasScalar;
    }

    static void test_null_values() {
        STMLLexer.TokenizeResult result = STML.tokenize("key1: null\nkey2: ~\n");
        int nullCount = 0;
        for (Token t : result.tokens) {
            if (t.type == TokenType.NULL_) nullCount++;
        }
        assert nullCount == 2;
    }

    static void test_bare_key() {
        STMLLexer.TokenizeResult result = STML.tokenize("just_a_key\n");
        boolean hasBare = false;
        for (Token t : result.tokens) {
            if (t.type == TokenType.BARE_KEY && t.asString().equals("just_a_key")) hasBare = true;
        }
        assert hasBare;
    }

    static void test_sequence() {
        STMLLexer.TokenizeResult result = STML.tokenize("list:\n  - item1\n  - item2\n");
        int dashCount = 0, scalarCount = 0;
        for (Token t : result.tokens) {
            if (t.type == TokenType.DASH) dashCount++;
            if (t.type == TokenType.SCALAR) scalarCount++;
        }
        assert dashCount == 2;
        assert scalarCount == 2;
    }

    static void test_comments() {
        STMLLexer.TokenizeResult result = STML.tokenize("# This is a comment\nkey: value\n");
        boolean hasKey = false;
        for (Token t : result.tokens) {
            if (t.type == TokenType.KEY) hasKey = true;
        }
        assert hasKey;
    }

    static void test_quoted_key() {
        STMLLexer.TokenizeResult result = STML.tokenize("\"complex key\": value\n");
        boolean hasKey = false;
        for (Token t : result.tokens) {
            if (t.type == TokenType.KEY && t.asString().equals("complex key")) hasKey = true;
        }
        assert hasKey;
    }

    static void test_doc_separator() {
        STMLLexer.TokenizeResult result = STML.tokenize("key1: value1\n---\nkey2: value2\n");
        boolean hasSep = false;
        for (Token t : result.tokens) {
            if (t.type == TokenType.DOC_SEPARATOR) hasSep = true;
        }
        assert hasSep;
    }

    static void test_inline_list() {
        STMLLexer.TokenizeResult result = STML.tokenize("key: [a, b, c]\n");
        boolean hasList = false;
        for (Token t : result.tokens) {
            if (t.type == TokenType.INLINE_LIST) {
                hasList = true;
                List<InlineElem> elems = t.asInlineList();
                assert elems.size() == 3;
                assert elems.get(0).get().equals("a");
                assert elems.get(1).get().equals("b");
                assert elems.get(2).get().equals("c");
            }
        }
        assert hasList;
    }

    static void test_multiline_basic() {
        STMLLexer.TokenizeResult result = STML.tokenize("text: {\nline1\nline2\n}\n");
        boolean hasMl = false;
        for (Token t : result.tokens) {
            if (t.type == TokenType.MULTILINE_STRING) {
                hasMl = true;
                assert t.asString().equals("line1\nline2");
            }
        }
        assert hasMl;
    }

    static void test_multiline_dash() {
        STMLLexer.TokenizeResult result = STML.tokenize("text: {\n---\n---\n}\n");
        boolean hasMl = false;
        for (Token t : result.tokens) {
            if (t.type == TokenType.MULTILINE_STRING) {
                hasMl = true;
                assert t.asString().equals("---\n---");
            }
        }
        assert hasMl;
    }

    static void test_escapes() {
        STMLLexer.TokenizeResult result = STML.tokenize("key: \"line1\\nline2\"\n");
        boolean hasScalar = false;
        for (Token t : result.tokens) {
            if (t.type == TokenType.SCALAR) {
                hasScalar = true;
                assert t.asString().equals("line1\nline2");
            }
        }
        assert hasScalar;
    }

    static void test_empty_lines() {
        STMLLexer.TokenizeResult result = STML.tokenize("key: value\n\n\nkey2: value2\n");
        int keyCount = 0;
        for (Token t : result.tokens) {
            if (t.type == TokenType.KEY) keyCount++;
        }
        assert keyCount == 2;
    }

    public static void main(String[] args) {
        System.out.println("Lexer tests:");

        test("indent_basic", TestLexer::test_indent_basic);
        test("indent_mixed", TestLexer::test_indent_mixed);
        test("key_value", TestLexer::test_key_value);
        test("null_values", TestLexer::test_null_values);
        test("bare_key", TestLexer::test_bare_key);
        test("sequence", TestLexer::test_sequence);
        test("comments", TestLexer::test_comments);
        test("quoted_key", TestLexer::test_quoted_key);
        test("doc_separator", TestLexer::test_doc_separator);
        test("inline_list", TestLexer::test_inline_list);
        test("multiline_basic", TestLexer::test_multiline_basic);
        test("multiline_dash", TestLexer::test_multiline_dash);
        test("escapes", TestLexer::test_escapes);
        test("empty_lines", TestLexer::test_empty_lines);

        System.out.println("\n" + testsPassed + "/" + testsRun + " passed");
        System.exit(testsPassed == testsRun ? 0 : 1);
    }
}