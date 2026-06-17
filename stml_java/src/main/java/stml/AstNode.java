package stml;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Objects;

public class AstNode {
    public enum Kind { NULL, STRING, LIST, MAP }

    private Kind kind;
    private Object value; // null, String, List<AstNode>, or List<Pair>

    public static class Pair {
        public final String key;
        public final AstNode value;
        public Pair(String key, AstNode value) { this.key = key; this.value = value; }
    }

    public AstNode() { this.kind = Kind.NULL; this.value = null; }
    public AstNode(String s) { this.kind = Kind.STRING; this.value = s; }
    public AstNode(List<?> list) { this.kind = list.isEmpty() ? Kind.LIST : (list.get(0) instanceof Pair ? Kind.MAP : Kind.LIST); this.value = list; }
    public AstNode(List<?> list, Kind kind) { this.kind = kind; this.value = list; }

    public boolean isNull() { return kind == Kind.NULL; }
    public boolean isString() { return kind == Kind.STRING; }
    public boolean isList() { return kind == Kind.LIST; }
    public boolean isMap() { return kind == Kind.MAP; }

    @SuppressWarnings("unchecked")
    public String asString() { return (String) value; }

    @SuppressWarnings("unchecked")
    public List<AstNode> asList() { return (List<AstNode>) value; }

    @SuppressWarnings("unchecked")
    public List<Pair> asMap() { return (List<Pair>) value; }

    public static AstNode mapFind(List<Pair> map, String key) {
        for (Pair p : map) {
            if (p.key.equals(key)) return p.value;
        }
        return null;
    }

    public static AstNode deepClone(AstNode node) {
        if (node.isNull()) return new AstNode();
        if (node.isString()) return new AstNode(node.asString());
        if (node.isList()) {
            List<AstNode> list = new ArrayList<>();
            for (AstNode item : node.asList()) list.add(deepClone(item));
            return new AstNode(list, list.isEmpty() ? node.kind : Kind.LIST);
        }
        if (node.isMap()) {
            List<Pair> map = new ArrayList<>();
            for (Pair p : node.asMap()) map.add(new Pair(p.key, deepClone(p.value)));
            return new AstNode(map, map.isEmpty() ? node.kind : Kind.MAP);
        }
        return new AstNode();
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof AstNode)) return false;
        AstNode other = (AstNode) o;
        if (isNull() && other.isNull()) return true;
        if (isString() && other.isString()) return asString().equals(other.asString());
        if (isList() && other.isList()) {
            List<AstNode> a = asList(), b = other.asList();
            if (a.size() != b.size()) return false;
            for (int i = 0; i < a.size(); i++) if (!a.get(i).equals(b.get(i))) return false;
            return true;
        }
        if (isMap() && other.isMap()) {
            List<Pair> a = asMap(), b = other.asMap();
            if (a.size() != b.size()) return false;
            for (int i = 0; i < a.size(); i++) {
                if (!a.get(i).key.equals(b.get(i).key)) return false;
                if (!a.get(i).value.equals(b.get(i).value)) return false;
            }
            return true;
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(value);
    }
}