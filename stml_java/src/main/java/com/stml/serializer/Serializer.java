package com.stml.serializer;

import com.stml.ast.*;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Serializer: AST → canonical STML text / JSON text.
 */
public final class Serializer {

    private Serializer() {}

    // =========================================================================
    // Escape helpers
    // =========================================================================

    /** Escape special characters for double-quoted strings. */
    public static String escapeString(String s) {
        StringBuilder sb = new StringBuilder(s.length() + 8);
        for (int i = 0; i < s.length(); i++) {
            char ch = s.charAt(i);
            switch (ch) {
                case '\\': sb.append("\\\\"); break;
                case '"':  sb.append("\\\""); break;
                case '\n': sb.append("\\n"); break;
                case '\t': sb.append("\\t"); break;
                default:   sb.append(ch); break;
            }
        }
        return sb.toString();
    }

    /** JSON string escape (more thorough than STML escape). */
    private static String jsonEscape(String s) {
        StringBuilder sb = new StringBuilder(s.length() + 8);
        for (int i = 0; i < s.length(); i++) {
            char ch = s.charAt(i);
            switch (ch) {
                case '\\': sb.append("\\\\"); break;
                case '"':  sb.append("\\\""); break;
                case '\n': sb.append("\\n"); break;
                case '\r': sb.append("\\r"); break;
                case '\t': sb.append("\\t"); break;
                case '\b': sb.append("\\b"); break;
                case '\f': sb.append("\\f"); break;
                default:
                    if (ch < 0x20) {
                        sb.append("\\u00");
                        sb.append("0123456789abcdef".charAt(ch >> 4));
                        sb.append("0123456789abcdef".charAt(ch & 0xf));
                    } else {
                        sb.append(ch);
                    }
                    break;
            }
        }
        return sb.toString();
    }

    /** Format a key (always double-quoted). */
    public static String quoteKey(String key) {
        return "\"" + escapeString(key) + "\"";
    }

    // =========================================================================
    // formatScalar
    // =========================================================================

    /** Format a scalar value as STML representation. */
    public static String formatScalar(AstNode node) {
        if (node.isNull()) {
            return "null";
        }
        if (node.isString()) {
            return "\"" + escapeString(node.asString()) + "\"";
        }
        if (node.isMap() && node.asMap().isEmpty()) {
            return "null";
        }
        if (node.isList() && node.asList().isEmpty()) {
            return "null";
        }
        throw new IllegalArgumentException("formatScalar called on non-scalar node");
    }

    // =========================================================================
    // serializeNode
    // =========================================================================

    /** Convert an AST node to an STML fragment (possibly multi-line). Indentation: 2 spaces per level. */
    public static String serializeNode(AstNode node, int indentLevel) {
        String indent = " ".repeat(indentLevel * 2);

        // 1. Null
        if (node.isNull()) {
            return indent + "null";
        }

        // 2. String (scalar)
        if (node.isString()) {
            return indent + "\"" + escapeString(node.asString()) + "\"";
        }

        // 3. List (sequence)
        if (node.isList()) {
            List<AstNode> lst = node.asList();
            if (lst.isEmpty()) {
                return indent + "- null";
            }

            List<String> lineList = new ArrayList<>();
            for (AstNode item : lst) {
                if (item.isNull() || item.isString()) {
                    lineList.add(indent + "- " + formatScalar(item));
                } else if (item.isList()) {
                    List<AstNode> innerLst = item.asList();
                    if (innerLst.isEmpty()) {
                        lineList.add(indent + "- null");
                    } else if (valuesAreAllScalars(innerLst)) {
                        lineList.add(indent + "- " + formatInlineList(innerLst));
                    } else {
                        lineList.add(indent + "- ");
                        lineList.add(serializeNode(item, indentLevel + 1));
                    }
                } else if (item.isMap()) {
                    List<Map.Entry<String, AstNode>> itemMap = item.asMap();
                    if (itemMap.isEmpty()) {
                        lineList.add(indent + "- null");
                        continue;
                    }
                    if (itemMap.size() > 1) {
                        throw new IllegalArgumentException(
                                "STML list entries do not support multi-key dicts. Use single-key mapping wrapper.");
                    }
                    var entry = itemMap.get(0);
                    String key = entry.getKey();
                    AstNode val = entry.getValue();
                    if (val.isList() || val.isMap()) {
                        lineList.add(indent + "- " + quoteKey(key) + ":");
                        lineList.add(serializeNode(val, indentLevel + 1));
                    } else {
                        lineList.add(indent + "- " + quoteKey(key) + ": " + formatScalar(val));
                    }
                }
            }
            return String.join("\n", lineList);
        }

        // 4. Map (mapping)
        if (node.isMap()) {
            List<Map.Entry<String, AstNode>> map = node.asMap();
            if (map.isEmpty()) {
                return indent + "null";
            }

            List<String> lineList = new ArrayList<>();
            for (var entry : map) {
                String key = entry.getKey();
                AstNode val = entry.getValue();
                String kStr = quoteKey(key);
                if (val.isList() || val.isMap()) {
                    String sub = serializeNode(val, indentLevel + 1);
                    if (sub.isEmpty()) {
                        lineList.add(indent + kStr + ": null");
                    } else {
                        lineList.add(indent + kStr + ":");
                        lineList.add(sub);
                    }
                } else {
                    lineList.add(indent + kStr + ": " + formatScalar(val));
                }
            }
            return String.join("\n", lineList);
        }

        throw new IllegalArgumentException("Unsupported node type in serializeNode");
    }

    private static boolean valuesAreAllScalars(List<AstNode> lst) {
        for (AstNode v : lst) {
            if (v.isList() || v.isMap()) return false;
        }
        return true;
    }

    private static String formatInlineList(List<AstNode> lst) {
        StringBuilder sb = new StringBuilder("[");
        for (int i = 0; i < lst.size(); i++) {
            if (i > 0) sb.append(", ");
            sb.append(formatScalar(lst.get(i)));
        }
        sb.append("]");
        return sb.toString();
    }

    // =========================================================================
    // dumps — top-level serialization
    // =========================================================================

    /** Convert docs-wrapped AST to canonical STML text. */
    public static String dumps(AstNode node) {
        if (node.isMap()) {
            AstNode docsNode = AstNode.mapFind(node, "docs");
            if (docsNode != null && docsNode.isList()) {
                List<AstNode> docs = docsNode.asList();
                List<String> parts = new ArrayList<>();
                for (AstNode doc : docs) {
                    parts.add(serializeNode(doc, 0));
                }
                StringBuilder result = new StringBuilder();
                for (int i = 0; i < parts.size(); i++) {
                    result.append(parts.get(i));
                    if (i + 1 < parts.size())
                        result.append("\n---");
                    result.append("\n");
                }
                return result.toString();
            }
        }

        // Unwrapped single document
        return serializeNode(node, 0) + "\n";
    }

    // =========================================================================
    // toJson — AST → canonical JSON text
    // =========================================================================

    /** Convert an AST node to JSON text. */
    public static String toJson(AstNode node) {
        StringBuilder sb = new StringBuilder();
        toJsonImpl(node, sb, 0);
        sb.append("\n");
        return sb.toString();
    }

    private static void toJsonImpl(AstNode node, StringBuilder sb, int indentLevel) {
        if (node.isNull()) {
            sb.append("null");
        } else if (node.isString()) {
            sb.append('"').append(jsonEscape(node.asString())).append('"');
        } else if (node.isList()) {
            List<AstNode> lst = node.asList();
            if (lst.isEmpty()) {
                sb.append("[]");
            } else {
                sb.append("[\n");
                for (int i = 0; i < lst.size(); i++) {
                    writeIndent(sb, indentLevel + 1);
                    toJsonImpl(lst.get(i), sb, indentLevel + 1);
                    if (i + 1 < lst.size()) sb.append(",");
                    sb.append("\n");
                }
                writeIndent(sb, indentLevel);
                sb.append("]");
            }
        } else if (node.isMap()) {
            List<Map.Entry<String, AstNode>> map = node.asMap();
            if (map.isEmpty()) {
                sb.append("{}");
            } else {
                sb.append("{\n");
                int i = 0;
                for (var entry : map) {
                    writeIndent(sb, indentLevel + 1);
                    sb.append('"').append(jsonEscape(entry.getKey())).append("\": ");
                    toJsonImpl(entry.getValue(), sb, indentLevel + 1);
                    if (++i < map.size()) sb.append(",");
                    sb.append("\n");
                }
                writeIndent(sb, indentLevel);
                sb.append("}");
            }
        }
    }

    private static void writeIndent(StringBuilder sb, int level) {
        for (int i = 0; i < level * 2; i++)
            sb.append(' ');
    }
}
