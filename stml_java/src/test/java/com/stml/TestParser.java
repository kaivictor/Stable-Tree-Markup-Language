package com.stml;

import com.stml.ast.*;
import com.stml.diagnostics.ParseError;
import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;

class TestParser {

    @Test
    void testSimpleMapping() {
        var result = Stml.loads("\"key\": \"value\"");
        assertNotNull(result.ast());
        AstNode docs = AstNode.mapFind(result.ast(), "docs");
        assertNotNull(docs);
        assertTrue(docs.isList());
        assertEquals(1, docs.asList().size());

        AstNode doc = docs.asList().get(0);
        assertTrue(doc.isMap());
        AstNode val = AstNode.mapFind(doc, "key");
        assertNotNull(val);
        assertTrue(val.isString());
        assertEquals("value", val.asString());
    }

    @Test
    void testNullValue() {
        var result = Stml.loads("\"key\": null");
        AstNode docs = AstNode.mapFind(result.ast(), "docs");
        AstNode doc = docs.asList().get(0);
        AstNode val = AstNode.mapFind(doc, "key");
        assertTrue(val.isNull());
    }

    @Test
    void testTildeNullValue() {
        var result = Stml.loads("\"key\": ~");
        AstNode docs = AstNode.mapFind(result.ast(), "docs");
        AstNode doc = docs.asList().get(0);
        AstNode val = AstNode.mapFind(doc, "key");
        assertTrue(val.isNull());
    }

    @Test
    void testSequence() {
        var result = Stml.loads("- \"item1\"\n- \"item2\"\n- \"item3\"");
        AstNode docs = AstNode.mapFind(result.ast(), "docs");
        AstNode doc = docs.asList().get(0);
        assertTrue(doc.isList());
        assertEquals(3, doc.asList().size());
    }

    @Test
    void testNestedMapping() {
        var result = Stml.loads("\"parent\":\n  \"child\": \"value\"");
        AstNode docs = AstNode.mapFind(result.ast(), "docs");
        AstNode doc = docs.asList().get(0);
        assertTrue(doc.isMap());
        AstNode parent = AstNode.mapFind(doc, "parent");
        assertTrue(parent.isMap());
        AstNode child = AstNode.mapFind(parent, "child");
        assertEquals("value", child.asString());
    }

    @Test
    void testInlineList() {
        var result = Stml.loads("\"items\": [a, b, c]");
        AstNode docs = AstNode.mapFind(result.ast(), "docs");
        AstNode doc = docs.asList().get(0);
        AstNode items = AstNode.mapFind(doc, "items");
        assertTrue(items.isList());
        assertEquals(3, items.asList().size());
        assertEquals("a", items.asList().get(0).asString());
        assertEquals("b", items.asList().get(1).asString());
        assertEquals("c", items.asList().get(2).asString());
    }

    @Test
    void testInlineListWithNull() {
        var result = Stml.loads("\"items\": [a, null, ~]");
        AstNode docs = AstNode.mapFind(result.ast(), "docs");
        AstNode doc = docs.asList().get(0);
        AstNode items = AstNode.mapFind(doc, "items");
        assertTrue(items.isList());
        assertEquals(3, items.asList().size());
        assertEquals("a", items.asList().get(0).asString());
        assertTrue(items.asList().get(1).isNull());
        assertTrue(items.asList().get(2).isNull());
    }

    @Test
    void testEmptyInput() {
        var result = Stml.loads("");
        AstNode docs = AstNode.mapFind(result.ast(), "docs");
        assertTrue(docs.isList());
        assertTrue(docs.asList().isEmpty());
    }

    @Test
    void testSingleDoc() {
        var result = Stml.loads("\"key\": \"value\"");
        AstNode docs = AstNode.mapFind(result.ast(), "docs");
        assertEquals(1, docs.asList().size());
    }

    @Test
    void testMultiDoc() {
        var result = Stml.loads("\"key1\": \"value1\"\n---\n\"key2\": \"value2\"");
        AstNode docs = AstNode.mapFind(result.ast(), "docs");
        assertEquals(2, docs.asList().size());
    }

    @Test
    void testBareKey() {
        var result = Stml.loads("just_a_key");
        AstNode docs = AstNode.mapFind(result.ast(), "docs");
        AstNode doc = docs.asList().get(0);
        assertTrue(doc.isMap());
        // Bare key has null value
        var entries = doc.asMap();
        assertEquals(1, entries.size());
        assertEquals("just_a_key", entries.get(0).getKey());
        assertTrue(entries.get(0).getValue().isNull());
    }

    @Test
    void testMultilineString() {
        var result = Stml.loads("\"text\": {\n  line1\n  line2\n}");
        AstNode docs = AstNode.mapFind(result.ast(), "docs");
        AstNode doc = docs.asList().get(0);
        AstNode text = AstNode.mapFind(doc, "text");
        assertTrue(text.isString());
        String val = text.asString();
        assertTrue(val.contains("line1"));
        assertTrue(val.contains("line2"));
    }

    @Test
    void testTest6ThrowsParseError() {
        // test6.stml should throw ParseError
        assertThrows(ParseError.class, () -> {
            Stml.load("src/test/resources/TestData/test6.stml");
        });
    }
}
