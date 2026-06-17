package stml;

import java.util.ArrayList;
import java.util.List;

public class Serializer {

    public static String escapeString(String s) {
        StringBuilder result = new StringBuilder();
        for (int i = 0; i < s.length(); i++) {
            char ch = s.charAt(i);
            switch (ch) {
                case '\\': result.append("\\\\"); break;
                case '"': result.append("\\\""); break;
                case '\n': result.append("\\n"); break;
                case '\t': result.append("\\t"); break;
                default: result.append(ch); break;
            }
        }
        return result.toString();
    }

    public static String quoteKey(String key) {
        return "\"" + escapeString(key) + "\"";
    }

    public static String formatScalar(AstNode node) {
        if (node.isNull()) return "null";
        if (node.isString()) return "\"" + escapeString(node.asString()) + "\"";
        if (node.isMap() && node.asMap().isEmpty()) return "null";
        if (node.isList() && node.asList().isEmpty()) return "null";
        throw new RuntimeException("format_scalar called on non-scalar node");
    }

    private static boolean valuesAreAllScalars(List<AstNode> lst) {
        for (AstNode v : lst) {
            if (v.isList() || v.isMap()) return false;
        }
        return true;
    }

    private static String formatInlineList(List<AstNode> lst) {
        StringBuilder result = new StringBuilder("[");
        for (int i = 0; i < lst.size(); i++) {
            if (i > 0) result.append(", ");
            result.append(formatScalar(lst.get(i)));
        }
        result.append("]");
        return result.toString();
    }

    public static String serializeNode(AstNode node, int indentLevel) {
        String indent = " ".repeat(indentLevel * 2);
        String nextIndent = " ".repeat((indentLevel + 1) * 2);

        if (node.isNull()) {
            return indent + "null";
        }

        if (node.isString()) {
            return indent + "\"" + escapeString(node.asString()) + "\"";
        }

        if (node.isList()) {
            List<AstNode> lst = node.asList();
            if (lst.isEmpty()) return indent + "- null";

            List<String> lines = new ArrayList<>();
            for (AstNode item : lst) {
                if (item.isNull()) {
                    lines.add(indent + "-");
                } else if (item.isString()) {
                    lines.add(indent + "- " + formatScalar(item));
                } else if (item.isList()) {
                    List<AstNode> innerLst = item.asList();
                    if (innerLst.isEmpty()) {
                        lines.add(indent + "- null");
                    } else if (valuesAreAllScalars(innerLst)) {
                        lines.add(indent + "- " + formatInlineList(innerLst));
                    } else {
                        lines.add(indent + "- ");
                        lines.add(serializeNode(item, indentLevel + 1));
                    }
                } else if (item.isMap()) {
                    List<AstNode.Pair> itemMap = item.asMap();
                    if (itemMap.isEmpty()) {
                        lines.add(indent + "- null");
                        continue;
                    }
                    boolean first = true;
                    for (AstNode.Pair pair : itemMap) {
                        int entryLevel = first ? indentLevel : indentLevel + 1;
                        String entryIndent = " ".repeat(entryLevel * 2);
                        String prefix = first ? "- " : "";
                        if (pair.value.isList() || pair.value.isMap()) {
                            lines.add(entryIndent + prefix + quoteKey(pair.key) + ":");
                            lines.add(serializeNode(pair.value, entryLevel + 1));
                        } else if (pair.value.isNull()) {
                            lines.add(entryIndent + prefix + quoteKey(pair.key));
                        } else {
                            lines.add(entryIndent + prefix + quoteKey(pair.key) + ": " + formatScalar(pair.value));
                        }
                        first = false;
                    }
                }
            }
            return String.join("\n", lines);
        }

        if (node.isMap()) {
            List<AstNode.Pair> map = node.asMap();
            if (map.isEmpty()) return indent + "null";

            List<String> lines = new ArrayList<>();
            for (AstNode.Pair pair : map) {
                String kStr = quoteKey(pair.key);
                if (pair.value.isList() || pair.value.isMap()) {
                    String sub = serializeNode(pair.value, indentLevel + 1);
                    if (sub.isEmpty()) {
                        lines.add(indent + kStr + ": null");
                    } else {
                        lines.add(indent + kStr + ":");
                        lines.add(sub);
                    }
                } else {
                    lines.add(indent + kStr + ": " + formatScalar(pair.value));
                }
            }
            return String.join("\n", lines);
        }

        throw new RuntimeException("Unsupported node type in serialize_node");
    }

    public static String dumps(AstNode node) {
        if (node.isMap()) {
            List<AstNode.Pair> map = node.asMap();
            AstNode docsNode = AstNode.mapFind(map, "docs");
            if (docsNode != null && docsNode.isList()) {
                List<AstNode> docs = docsNode.asList();
                List<String> parts = new ArrayList<>();
                for (AstNode doc : docs) {
                    parts.add(serializeNode(doc, 0));
                }
                StringBuilder result = new StringBuilder();
                for (int i = 0; i < parts.size(); i++) {
                    result.append(parts.get(i));
                    if (i + 1 < parts.size()) result.append("\n---");
                    result.append("\n");
                }
                return result.toString();
            }
        }
        return serializeNode(node, 0) + "\n";
    }

    // ---- JSON serialization ----

    private static String jsonEscape(String s) {
        StringBuilder result = new StringBuilder();
        for (int i = 0; i < s.length(); i++) {
            char ch = s.charAt(i);
            switch (ch) {
                case '\\': result.append("\\\\"); break;
                case '"': result.append("\\\""); break;
                case '\n': result.append("\\n"); break;
                case '\r': result.append("\\r"); break;
                case '\t': result.append("\\t"); break;
                case '\b': result.append("\\b"); break;
                case '\f': result.append("\\f"); break;
                default:
                    if (ch < 0x20) {
                        result.append("\\u00");
                        result.append("0123456789abcdef".charAt(ch >> 4));
                        result.append("0123456789abcdef".charAt(ch & 0xf));
                    } else {
                        result.append(ch);
                    }
                    break;
            }
        }
        return result.toString();
    }

    private static void toJsonImpl(AstNode node, StringBuilder os, int indentLevel) {
        if (node.isNull()) {
            os.append("null");
        } else if (node.isString()) {
            os.append('"').append(jsonEscape(node.asString())).append('"');
        } else if (node.isList()) {
            List<AstNode> lst = node.asList();
            if (lst.isEmpty()) {
                os.append("[]");
            } else {
                os.append("[\n");
                for (int i = 0; i < lst.size(); i++) {
                    writeIndent(os, indentLevel + 1);
                    toJsonImpl(lst.get(i), os, indentLevel + 1);
                    if (i + 1 < lst.size()) os.append(",");
                    os.append("\n");
                }
                writeIndent(os, indentLevel);
                os.append("]");
            }
        } else if (node.isMap()) {
            List<AstNode.Pair> map = node.asMap();
            if (map.isEmpty()) {
                os.append("{}");
            } else {
                os.append("{\n");
                for (int i = 0; i < map.size(); i++) {
                    writeIndent(os, indentLevel + 1);
                    os.append('"').append(jsonEscape(map.get(i).key)).append("\": ");
                    toJsonImpl(map.get(i).value, os, indentLevel + 1);
                    if (i + 1 < map.size()) os.append(",");
                    os.append("\n");
                }
                writeIndent(os, indentLevel);
                os.append("}");
            }
        }
    }

    private static void writeIndent(StringBuilder os, int level) {
        for (int i = 0; i < level * 2; i++) os.append(' ');
    }

    public static String toJson(AstNode node) {
        StringBuilder os = new StringBuilder();
        toJsonImpl(node, os, 0);
        os.append("\n");
        return os.toString();
    }
}