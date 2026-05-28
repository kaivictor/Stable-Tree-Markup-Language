package com.stml;

import com.stml.ast.*;
import com.stml.serializer.Serializer;
import com.google.gson.Gson;
import com.google.gson.reflect.TypeToken;
import org.junit.jupiter.api.Test;

import java.lang.reflect.Type;
import java.util.Map;

import static org.junit.jupiter.api.Assertions.*;

class TestJsonUtil {

    private static final Gson gson = new Gson();
    private static final Type MAP_TYPE = new TypeToken<Map<String, Object>>(){}.getType();

    @Test
    void testJsonRoundtripSimpleMap() {
        AstMap map = new AstMap();
        map.put("key", new AstScalar("value"));
        String json = Serializer.toJson(map);
        Map<String, Object> parsed = gson.fromJson(json, MAP_TYPE);
        assertNotNull(parsed);
        assertEquals("value", parsed.get("key"));
    }

    @Test
    void testJsonRoundtripNested() {
        AstMap inner = new AstMap();
        inner.put("child", new AstScalar("value"));

        AstMap outer = new AstMap();
        outer.put("parent", inner);

        String json = Serializer.toJson(outer);
        @SuppressWarnings("unchecked")
        Map<String, Object> parsed = (Map<String, Object>) gson.fromJson(json, MAP_TYPE);
        assertNotNull(parsed.get("parent"));
        @SuppressWarnings("unchecked")
        Map<String, Object> innerParsed = (Map<String, Object>) parsed.get("parent");
        assertEquals("value", innerParsed.get("child"));
    }

    @Test
    void testJsonRoundtripList() {
        AstList list = new AstList();
        list.items().add(new AstScalar("a"));
        list.items().add(new AstScalar("b"));

        String json = Serializer.toJson(list);
        assertTrue(json.contains("["));
        assertTrue(json.contains("a"));
        assertTrue(json.contains("b"));
    }

    @Test
    void testJsonRoundtripNull() {
        String json = Serializer.toJson(NullNode.INSTANCE);
        assertEquals("null\n", json);
    }
}
