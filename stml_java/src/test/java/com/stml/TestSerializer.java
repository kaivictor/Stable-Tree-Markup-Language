package com.stml;

import com.stml.ast.*;
import com.stml.serializer.Serializer;
import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;

class TestSerializer {

    @Test
    void testSerializeNull() {
        assertEquals("null", Serializer.serializeNode(NullNode.INSTANCE, 0));
    }

    @Test
    void testSerializeString() {
        assertEquals("\"hello\"", Serializer.serializeNode(new AstScalar("hello"), 0));
    }

    @Test
    void testSerializeStringWithQuote() {
        assertEquals("\"he\\\"llo\"", Serializer.serializeNode(new AstScalar("he\"llo"), 0));
    }

    @Test
    void testSerializeSimpleMap() {
        AstMap map = new AstMap();
        map.put("key", new AstScalar("value"));
        assertEquals("\"key\": \"value\"", Serializer.serializeNode(map, 0));
    }

    @Test
    void testSerializeNestedMap() {
        AstMap inner = new AstMap();
        inner.put("child", new AstScalar("value"));

        AstMap outer = new AstMap();
        outer.put("parent", inner);

        String result = Serializer.serializeNode(outer, 0);
        assertTrue(result.contains("\"parent\":"));
        assertTrue(result.contains("  \"child\": \"value\""));
    }

    @Test
    void testSerializeList() {
        AstList list = new AstList();
        list.items().add(new AstScalar("a"));
        list.items().add(new AstScalar("b"));
        list.items().add(new AstScalar("c"));

        String result = Serializer.serializeNode(list, 0);
        assertTrue(result.contains("- \"a\""));
        assertTrue(result.contains("- \"b\""));
        assertTrue(result.contains("- \"c\""));
    }

    @Test
    void testSerializeInlineList() {
        AstList inner = new AstList();
        inner.items().add(new AstScalar("x"));
        inner.items().add(new AstScalar("y"));

        AstList outer = new AstList();
        outer.items().add(inner);

        String result = Serializer.serializeNode(outer, 0);
        assertTrue(result.contains("["));
    }

    @Test
    void testSerializeNullMapValue() {
        AstMap map = new AstMap();
        map.put("key", NullNode.INSTANCE);
        assertEquals("\"key\": null", Serializer.serializeNode(map, 0));
    }

    @Test
    void testEscapeString() {
        assertEquals("hello\\\\world", Serializer.escapeString("hello\\world"));
    }

    @Test
    void testQuoteKey() {
        assertEquals("\"mykey\"", Serializer.quoteKey("mykey"));
    }

    @Test
    void testFormatScalarNull() {
        assertEquals("null", Serializer.formatScalar(NullNode.INSTANCE));
    }

    @Test
    void testFormatScalarString() {
        assertEquals("\"hello\"", Serializer.formatScalar(new AstScalar("hello")));
    }

    @Test
    void testRoundtripSimple() {
        var result = Stml.loads("\"key\": \"value\"");
        String stml = Stml.dumps(result.ast());
        var result2 = Stml.loads(stml);
        assertTrue(AstNode.deepEquals(result.ast(), result2.ast()));
    }

    @Test
    void testRoundtripNested() {
        var result = Stml.loads("\"parent\":\n  \"child\": \"value\"");
        String stml = Stml.dumps(result.ast());
        var result2 = Stml.loads(stml);
        assertTrue(AstNode.deepEquals(result.ast(), result2.ast()));
    }

    @Test
    void testRoundtripSequence() {
        var result = Stml.loads("- \"item1\"\n- \"item2\"\n- \"item3\"");
        String stml = Stml.dumps(result.ast());
        var result2 = Stml.loads(stml);
        assertTrue(AstNode.deepEquals(result.ast(), result2.ast()));
    }

    @Test
    void testRoundtripNull() {
        var result = Stml.loads("\"key\": ~");
        String stml = Stml.dumps(result.ast());
        var result2 = Stml.loads(stml);
        assertTrue(AstNode.deepEquals(result.ast(), result2.ast()));
    }

    @Test
    void testToJsonEmptyList() {
        assertEquals("[]\n", Serializer.toJson(new AstList()));
    }

    @Test
    void testToJsonEmptyMap() {
        assertEquals("{}\n", Serializer.toJson(new AstMap()));
    }
}
