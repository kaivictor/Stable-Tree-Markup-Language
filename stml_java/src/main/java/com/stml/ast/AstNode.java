package com.stml.ast;

import java.util.List;
import java.util.Map;

/**
 * AST node — the four STML value types.
 * NullNode = null, AstScalar = string, AstList = ordered sequence, AstMap = ordered mapping.
 */
public sealed interface AstNode
        permits NullNode, AstScalar, AstList, AstMap {

    default boolean isNull()    { return this instanceof NullNode; }
    default boolean isString()  { return this instanceof AstScalar; }
    default boolean isList()    { return this instanceof AstList; }
    default boolean isMap()     { return this instanceof AstMap; }

    default String asString() {
        if (this instanceof AstScalar s) return s.value();
        return null;
    }

    default List<AstNode> asList() {
        if (this instanceof AstList l) return l.items();
        return null;
    }

    default List<Map.Entry<String, AstNode>> asMap() {
        if (this instanceof AstMap m) return m.entries();
        return null;
    }

    /** Deep equality comparison (insertion-order sensitive for maps). */
    static boolean deepEquals(AstNode a, AstNode b) {
        if (a == null && b == null) return true;
        if (a == null || b == null) return false;
        if (a.isNull() && b.isNull()) return true;
        if (a.isString() && b.isString()) return a.asString().equals(b.asString());
        if (a.isList() && b.isList()) {
            var la = a.asList();
            var lb = b.asList();
            if (la.size() != lb.size()) return false;
            for (int i = 0; i < la.size(); i++) {
                if (!deepEquals(la.get(i), lb.get(i))) return false;
            }
            return true;
        }
        if (a.isMap() && b.isMap()) {
            var ma = a.asMap();
            var mb = b.asMap();
            if (ma.size() != mb.size()) return false;
            for (int i = 0; i < ma.size(); i++) {
                var ea = ma.get(i);
                var eb = mb.get(i);
                if (!ea.getKey().equals(eb.getKey())) return false;
                if (!deepEquals(ea.getValue(), eb.getValue())) return false;
            }
            return true;
        }
        return false;
    }

    /** Find a value by key in a map node. Returns null if not found or not a map. */
    static AstNode mapFind(AstNode node, String key) {
        if (!node.isMap()) return null;
        for (var e : node.asMap()) {
            if (e.getKey().equals(key)) return e.getValue();
        }
        return null;
    }
}
