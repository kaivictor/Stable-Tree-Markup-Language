package stml;

import java.util.List;

public class TestParser {
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

    static void test_simple_mapping() {
        STML.LoadResult result = STML.loads("key: value\n");
        AstNode ast = result.ast;
        assert ast.isMap();
        List<AstNode> docs = AstNode.mapFind(ast.asMap(), "docs").asList();
        assert docs.size() == 1;
        List<AstNode.Pair> doc1 = docs.get(0).asMap();
        assert AstNode.mapFind(doc1, "key").isString();
        assert AstNode.mapFind(doc1, "key").asString().equals("value");
    }

    static void test_nested_mapping() {
        STML.LoadResult result = STML.loads("parent:\n  child1: value1\n  child2: value2\n");
        List<AstNode> docs = AstNode.mapFind(result.ast.asMap(), "docs").asList();
        List<AstNode.Pair> doc1 = docs.get(0).asMap();
        List<AstNode.Pair> parent = AstNode.mapFind(doc1, "parent").asMap();
        assert AstNode.mapFind(parent, "child1").asString().equals("value1");
        assert AstNode.mapFind(parent, "child2").asString().equals("value2");
    }

    static void test_null_value() {
        STML.LoadResult result = STML.loads("key: null\n");
        List<AstNode> docs = AstNode.mapFind(result.ast.asMap(), "docs").asList();
        List<AstNode.Pair> doc1 = docs.get(0).asMap();
        assert AstNode.mapFind(doc1, "key").isNull();
    }

    static void test_tilde_null() {
        STML.LoadResult result = STML.loads("key: ~\n");
        List<AstNode> docs = AstNode.mapFind(result.ast.asMap(), "docs").asList();
        List<AstNode.Pair> doc1 = docs.get(0).asMap();
        assert AstNode.mapFind(doc1, "key").isNull();
    }

    static void test_bare_key() {
        STML.LoadResult result = STML.loads("just_a_key\n");
        List<AstNode> docs = AstNode.mapFind(result.ast.asMap(), "docs").asList();
        List<AstNode.Pair> doc1 = docs.get(0).asMap();
        assert AstNode.mapFind(doc1, "just_a_key").isNull();
    }

    static void test_flat_sequence() {
        STML.LoadResult result = STML.loads("items:\n  - item1\n  - item2\n  - item3\n");
        List<AstNode> docs = AstNode.mapFind(result.ast.asMap(), "docs").asList();
        List<AstNode> items = AstNode.mapFind(docs.get(0).asMap(), "items").asList();
        assert items.size() == 3;
        assert items.get(0).asString().equals("item1");
        assert items.get(1).asString().equals("item2");
        assert items.get(2).asString().equals("item3");
    }

    static void test_sequence_with_mapping() {
        STML.LoadResult result = STML.loads("items:\n  - key1: value1\n  - key2: value2\n");
        List<AstNode> docs = AstNode.mapFind(result.ast.asMap(), "docs").asList();
        List<AstNode> items = AstNode.mapFind(docs.get(0).asMap(), "items").asList();
        assert items.size() == 2;
        assert items.get(0).isMap();
        assert items.get(1).isMap();
        assert AstNode.mapFind(items.get(0).asMap(), "key1").asString().equals("value1");
        assert AstNode.mapFind(items.get(1).asMap(), "key2").asString().equals("value2");
    }

    static void test_empty_sequence() {
        STML.LoadResult result = STML.loads("items:\n  -\n  -\n");
        List<AstNode> docs = AstNode.mapFind(result.ast.asMap(), "docs").asList();
        List<AstNode> items = AstNode.mapFind(docs.get(0).asMap(), "items").asList();
        assert items.size() == 2;
        assert items.get(0).isNull();
        assert items.get(1).isNull();
    }

    static void test_inline_list() {
        STML.LoadResult result = STML.loads("key: [a, b, c]\n");
        List<AstNode> docs = AstNode.mapFind(result.ast.asMap(), "docs").asList();
        List<AstNode> lst = AstNode.mapFind(docs.get(0).asMap(), "key").asList();
        assert lst.size() == 3;
        assert lst.get(0).asString().equals("a");
        assert lst.get(1).asString().equals("b");
        assert lst.get(2).asString().equals("c");
    }

    static void test_multiline_string() {
        STML.LoadResult result = STML.loads("text: {\nline1\nline2\n}\n");
        List<AstNode> docs = AstNode.mapFind(result.ast.asMap(), "docs").asList();
        assert AstNode.mapFind(docs.get(0).asMap(), "text").asString().equals("line1\nline2");
    }

    static void test_multi_document() {
        STML.LoadResult result = STML.loads("doc1_key: value1\n---\ndoc2_key: value2\n");
        List<AstNode> docs = AstNode.mapFind(result.ast.asMap(), "docs").asList();
        assert docs.size() == 2;
    }

    static void test_empty_document() {
        STML.LoadResult result = STML.loads("---\n---\nkey: value\n");
        List<AstNode> docs = AstNode.mapFind(result.ast.asMap(), "docs").asList();
        assert docs.size() == 2;
        assert docs.get(0).isNull();
        assert docs.get(1).isMap();
    }

    static void test_empty_input() {
        STML.LoadResult result = STML.loads("");
        List<AstNode> docs = AstNode.mapFind(result.ast.asMap(), "docs").asList();
        assert docs.size() == 0;
    }

    static void test_mixed_sequence() {
        STML.LoadResult result = STML.loads("items:\n  - key1: value1\n  - plain_value\n  - null_value: null\n");
        List<AstNode> docs = AstNode.mapFind(result.ast.asMap(), "docs").asList();
        List<AstNode> items = AstNode.mapFind(docs.get(0).asMap(), "items").asList();
        assert items.size() == 3;
        assert items.get(0).isMap();
        assert AstNode.mapFind(items.get(0).asMap(), "key1").asString().equals("value1");
        assert items.get(1).isMap();
        assert items.get(2).isMap();
    }

    static void test_nested_lists() {
        STML.LoadResult result = STML.loads("outer:\n  - [a, b]\n  - [c, d]\n");
        List<AstNode> docs = AstNode.mapFind(result.ast.asMap(), "docs").asList();
        List<AstNode> outer = AstNode.mapFind(docs.get(0).asMap(), "outer").asList();
        assert outer.size() == 2;
        assert outer.get(0).isList();
        assert outer.get(1).isList();
        assert outer.get(0).asList().size() == 2;
        assert outer.get(1).asList().size() == 2;
    }

    static void test_multi_key_sequence_inline() {
        STML.LoadResult result = STML.loads("items:\n  - key1: value1\n    key2: value2\n");
        List<AstNode> docs = AstNode.mapFind(result.ast.asMap(), "docs").asList();
        List<AstNode> items = AstNode.mapFind(docs.get(0).asMap(), "items").asList();
        assert items.size() == 1;
        assert items.get(0).isMap();
        List<AstNode.Pair> m = items.get(0).asMap();
        assert m.size() == 2;
        assert AstNode.mapFind(m, "key1").asString().equals("value1");
        assert AstNode.mapFind(m, "key2").asString().equals("value2");
    }

    static void test_multi_key_sequence_mixed() {
        STML.LoadResult result = STML.loads("items:\n  - key1: value1\n    key2\n");
        List<AstNode> docs = AstNode.mapFind(result.ast.asMap(), "docs").asList();
        List<AstNode> items = AstNode.mapFind(docs.get(0).asMap(), "items").asList();
        assert items.size() == 1;
        List<AstNode.Pair> m = items.get(0).asMap();
        assert m.size() == 2;
        assert AstNode.mapFind(m, "key1").asString().equals("value1");
        assert AstNode.mapFind(m, "key2").isNull();
    }

    static void test_multi_key_sequence_all_null() {
        STML.LoadResult result = STML.loads("items:\n  - key1:\n    key2:\n    key3:\n");
        List<AstNode> docs = AstNode.mapFind(result.ast.asMap(), "docs").asList();
        List<AstNode> items = AstNode.mapFind(docs.get(0).asMap(), "items").asList();
        assert items.size() == 1;
        List<AstNode.Pair> m = items.get(0).asMap();
        assert m.size() == 3;
        assert AstNode.mapFind(m, "key1").isNull();
        assert AstNode.mapFind(m, "key2").isNull();
        assert AstNode.mapFind(m, "key3").isNull();
    }

    static void test_multi_key_sequence_three_siblings() {
        STML.LoadResult result = STML.loads("items:\n  - k1: v1\n    k2: v2\n    k3: v3\n");
        List<AstNode> docs = AstNode.mapFind(result.ast.asMap(), "docs").asList();
        List<AstNode> items = AstNode.mapFind(docs.get(0).asMap(), "items").asList();
        assert items.size() == 1;
        List<AstNode.Pair> m = items.get(0).asMap();
        assert m.size() == 3;
        assert AstNode.mapFind(m, "k1").asString().equals("v1");
        assert AstNode.mapFind(m, "k2").asString().equals("v2");
        assert AstNode.mapFind(m, "k3").asString().equals("v3");
    }

    static void test_multi_key_sequence_with_block() {
        STML.LoadResult result = STML.loads("items:\n  - key1: value1\n    key2:\n      nested: val\n");
        List<AstNode> docs = AstNode.mapFind(result.ast.asMap(), "docs").asList();
        List<AstNode> items = AstNode.mapFind(docs.get(0).asMap(), "items").asList();
        assert items.size() == 1;
        List<AstNode.Pair> m = items.get(0).asMap();
        assert m.size() == 2;
        assert AstNode.mapFind(m, "key1").asString().equals("value1");
        assert AstNode.mapFind(m, "key2").isMap();
        List<AstNode.Pair> inner = AstNode.mapFind(m, "key2").asMap();
        assert AstNode.mapFind(inner, "nested").asString().equals("val");
    }

    public static void main(String[] args) {
        System.out.println("Parser tests:");

        test("simple_mapping", TestParser::test_simple_mapping);
        test("nested_mapping", TestParser::test_nested_mapping);
        test("null_value", TestParser::test_null_value);
        test("tilde_null", TestParser::test_tilde_null);
        test("bare_key", TestParser::test_bare_key);
        test("flat_sequence", TestParser::test_flat_sequence);
        test("sequence_with_mapping", TestParser::test_sequence_with_mapping);
        test("empty_sequence", TestParser::test_empty_sequence);
        test("inline_list", TestParser::test_inline_list);
        test("multiline_string", TestParser::test_multiline_string);
        test("multi_document", TestParser::test_multi_document);
        test("empty_document", TestParser::test_empty_document);
        test("empty_input", TestParser::test_empty_input);
        test("mixed_sequence", TestParser::test_mixed_sequence);
        test("nested_lists", TestParser::test_nested_lists);
        test("multi_key_sequence_inline", TestParser::test_multi_key_sequence_inline);
        test("multi_key_sequence_mixed", TestParser::test_multi_key_sequence_mixed);
        test("multi_key_sequence_all_null", TestParser::test_multi_key_sequence_all_null);
        test("multi_key_sequence_three_siblings", TestParser::test_multi_key_sequence_three_siblings);
        test("multi_key_sequence_with_block", TestParser::test_multi_key_sequence_with_block);

        System.out.println("\n" + testsPassed + "/" + testsRun + " passed");
        System.exit(testsPassed == testsRun ? 0 : 1);
    }
}