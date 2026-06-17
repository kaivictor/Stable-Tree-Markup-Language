package stml;

import java.util.ArrayList;
import java.util.List;

public class TestSerializer {
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

    static void test_serialize_null() {
        AstNode node = new AstNode();
        String result = Serializer.serializeNode(node, 0);
        assert result.equals("null");
    }

    static void test_serialize_string() {
        AstNode node = new AstNode("hello");
        String result = Serializer.serializeNode(node, 0);
        assert result.equals("\"hello\"");
    }

    static void test_serialize_string_with_quote() {
        AstNode node = new AstNode("he\"llo");
        String result = Serializer.serializeNode(node, 0);
        assert result.equals("\"he\\\"llo\"");
    }

    static void test_serialize_simple_map() {
        List<AstNode.Pair> m = new ArrayList<>();
        m.add(new AstNode.Pair("key", new AstNode("value")));
        AstNode node = new AstNode(m);
        String result = Serializer.serializeNode(node, 0);
        assert result.equals("\"key\": \"value\"");
    }

    static void test_serialize_nested_map() {
        List<AstNode.Pair> inner = new ArrayList<>();
        inner.add(new AstNode.Pair("a", new AstNode("1")));
        inner.add(new AstNode.Pair("b", new AstNode("2")));

        List<AstNode.Pair> outer = new ArrayList<>();
        outer.add(new AstNode.Pair("parent", new AstNode(inner)));

        AstNode node = new AstNode(outer);
        String result = Serializer.serializeNode(node, 0);
        assert result.contains("\"parent\":");
        assert result.contains("  \"a\": \"1\"");
        assert result.contains("  \"b\": \"2\"");
    }

    static void test_serialize_list() {
        List<AstNode> lst = new ArrayList<>();
        lst.add(new AstNode("a"));
        lst.add(new AstNode("b"));
        lst.add(new AstNode("c"));
        AstNode node = new AstNode(lst);
        String result = Serializer.serializeNode(node, 0);
        assert result.contains("- \"a\"");
        assert result.contains("- \"b\"");
        assert result.contains("- \"c\"");
    }

    static void test_serialize_inline_list() {
        List<AstNode> inner = new ArrayList<>();
        inner.add(new AstNode("x"));
        inner.add(new AstNode("y"));

        List<AstNode> outer = new ArrayList<>();
        outer.add(new AstNode(inner));

        AstNode node = new AstNode(outer);
        String result = Serializer.serializeNode(node, 0);
        assert result.contains("[\"x\", \"y\"]");
    }

    static void test_serialize_multi_key_sequence() {
        List<AstNode.Pair> inner = new ArrayList<>();
        inner.add(new AstNode.Pair("k1", new AstNode("v1")));
        inner.add(new AstNode.Pair("k2", new AstNode("v2")));

        List<AstNode> seq = new ArrayList<>();
        seq.add(new AstNode(inner));

        List<AstNode.Pair> doc = new ArrayList<>();
        doc.add(new AstNode.Pair("items", new AstNode(seq)));

        String result = Serializer.serializeNode(new AstNode(doc), 0);
        assert result.contains("- \"k1\": \"v1\"");
        assert result.contains("  \"k2\": \"v2\"");
    }

    static void test_roundtrip_simple() {
        String input = "\"key\": \"value\"\n";
        AstNode ast1 = STML.loads(input).ast;
        String output = STML.dumps(ast1);
        AstNode ast2 = STML.loads(output).ast;
        assert ast1.equals(ast2);
    }

    static void test_roundtrip_nested() {
        String input = "\"outer\":\n  \"inner\": \"value\"\n";
        AstNode ast1 = STML.loads(input).ast;
        String output = STML.dumps(ast1);
        AstNode ast2 = STML.loads(output).ast;
        assert ast1.equals(ast2);
    }

    static void test_roundtrip_sequence() {
        String input = "\"items\":\n  - \"a\"\n  - \"b\"\n  - \"c\"\n";
        AstNode ast1 = STML.loads(input).ast;
        String output = STML.dumps(ast1);
        AstNode ast2 = STML.loads(output).ast;
        assert ast1.equals(ast2);
    }

    static void test_roundtrip_null() {
        String input = "\"key\": null\n";
        AstNode ast1 = STML.loads(input).ast;
        String output = STML.dumps(ast1);
        AstNode ast2 = STML.loads(output).ast;
        assert ast1.equals(ast2);
    }

    static void test_roundtrip_multi_key_sequence() {
        String input = "\"items\":\n  - \"k1\": \"v1\"\n    \"k2\": \"v2\"\n";
        AstNode ast1 = STML.loads(input).ast;
        String output = STML.dumps(ast1);
        AstNode ast2 = STML.loads(output).ast;
        assert ast1.equals(ast2);
    }

    static void test_roundtrip_empty() {
        List<AstNode> docs = new ArrayList<>();
        List<AstNode.Pair> wrapper = new ArrayList<>();
        wrapper.add(new AstNode.Pair("docs", new AstNode(docs)));
        AstNode ast = new AstNode(wrapper);

        String output = STML.dumps(ast);
        AstNode ast2 = STML.loads(output).ast;
        assert ast.equals(ast2);
    }

    public static void main(String[] args) {
        System.out.println("Serializer tests:");

        test("serialize_null", TestSerializer::test_serialize_null);
        test("serialize_string", TestSerializer::test_serialize_string);
        test("serialize_string_with_quote", TestSerializer::test_serialize_string_with_quote);
        test("serialize_simple_map", TestSerializer::test_serialize_simple_map);
        test("serialize_nested_map", TestSerializer::test_serialize_nested_map);
        test("serialize_list", TestSerializer::test_serialize_list);
        test("serialize_inline_list", TestSerializer::test_serialize_inline_list);
        test("serialize_multi_key_sequence", TestSerializer::test_serialize_multi_key_sequence);

        System.out.println("\nRound-trip tests:");
        test("roundtrip_simple", TestSerializer::test_roundtrip_simple);
        test("roundtrip_nested", TestSerializer::test_roundtrip_nested);
        test("roundtrip_sequence", TestSerializer::test_roundtrip_sequence);
        test("roundtrip_null", TestSerializer::test_roundtrip_null);
        test("roundtrip_empty", TestSerializer::test_roundtrip_empty);
        test("roundtrip_multi_key_sequence", TestSerializer::test_roundtrip_multi_key_sequence);

        System.out.println("\n" + testsPassed + "/" + testsRun + " passed");
        System.exit(testsPassed == testsRun ? 0 : 1);
    }
}