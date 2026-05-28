package com.stml.ast;

import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Ordered mapping AST node.
 * Uses List<Entry<String, AstNode>> preserving insertion order (like C++ vector<pair<>>).
 * Duplicate keys are allowed.
 */
public record AstMap(List<Map.Entry<String, AstNode>> entries) implements AstNode {
    public AstMap() { this(new ArrayList<>()); }

    /** Add a key-value pair, returning this for chaining. */
    public AstMap put(String key, AstNode value) {
        entries.add(new AbstractMap.SimpleEntry<>(key, value));
        return this;
    }
}
