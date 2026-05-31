<<<<<<< Updated upstream
#include "serializer/serializer.h"
<<<<<<< Updated upstream
=======
=======
#include "serializer.h"
>>>>>>> Stashed changes
>>>>>>> Stashed changes
#include <sstream>

namespace stml {

<<<<<<< Updated upstream
// =========================================================================
// STML string escaping (for quoted values)
// =========================================================================
static void stml_escape(const std::string& src, std::string& dst) {
    dst.reserve(dst.size() + src.size() + 4);
    for (unsigned char c : src) {
        switch (c) {
            case '"':  dst += "\\\""; break;
            case '\\': dst += "\\\\"; break;
            case '\n': dst += "\\n";  break;
            case '\t': dst += "\\t";  break;
            case '\r': dst += "\\r";  break;
            default:
                dst += static_cast<char>(c);
                break;
        }
    }
}

// =========================================================================
// Helper: can this node be printed inline?
// =========================================================================
static bool is_inline_candidate(const AstNode& node) {
    if (node.is_null() || node.is_scalar()) return true;
    if (node.is_list()) {
        const auto& l = *node.as_list();
        // Only scalars and nulls in a short list can be inline
        if (l.size() > 10) return false;
        for (const auto& e : l) {
            if (!e.is_null() && !e.is_scalar()) return false;
            if (e.is_scalar() && e.as_scalar()->value.size() > 40) return false;
        }
        return true;
    }
    return false;
}

// =========================================================================
// Forward declaration
// =========================================================================
static void serialize_node(const AstNode& node, std::string& out, int indent,
                            bool is_list_item, bool inline_ok);

// =========================================================================
// Serialize a scalar value (quoted or null)
// =========================================================================
static void serialize_scalar(const std::string& value, std::string& out) {
    out += '"';
    stml_escape(value, out);
    out += '"';
}

// =========================================================================
// Serialize a map
// =========================================================================
static void serialize_map(const AstMap& map, std::string& out, int indent,
                           bool is_list_item) {
    std::string pad(indent, ' ');
    bool first = true;

    for (const auto& [key, val] : map) {
        if (!first) out += '\n';
        first = false;

        out += pad;
        serialize_scalar(key, out); // key always quoted

        if (val.is_null()) {
            out += ": null";
        }
        else if (val.is_scalar()) {
            out += ": ";
            serialize_scalar(val.as_scalar()->value, out);
        }
        else if (is_inline_candidate(val)) {
            out += ": ";
            serialize_node(val, out, indent, false, true);
        }
        else {
            out += ':';
            if (val.is_list() && !val.as_list()->empty()) {
                out += '\n';
                serialize_node(val, out, indent + 2, false, false);
            }
            else if (val.is_map() && !val.as_map()->empty()) {
                out += '\n';
                serialize_node(val, out, indent + 2, false, false);
            }
            // empty lists/maps stay inline
        }
    }
}

// =========================================================================
// Serialize a list (block format)
// =========================================================================
static void serialize_list_block(const AstList& list, std::string& out, int indent) {
    std::string pad(indent, ' ');
    for (size_t i = 0; i < list.size(); ++i) {
        if (i > 0) out += '\n';
        const auto& elem = list[i];

        out += pad;
        out += "- ";

        if (elem.is_null()) {
            out += "null";
        }
        else if (elem.is_scalar()) {
            serialize_scalar(elem.as_scalar()->value, out);
        }
        else if (elem.is_map()) {
            const auto& m = *elem.as_map();
            if (m.empty()) {
                // skip empty map
            } else if (m.size() == 1) {
                // Inline single-entry map on same line as dash
                const auto& [k, v] = m[0];
                serialize_scalar(k, out);
                if (v.is_null()) {
                    out += ": null";
                } else if (v.is_scalar()) {
                    out += ": ";
                    serialize_scalar(v.as_scalar()->value, out);
                } else {
                    out += ':';
                    if (!v.is_null() || (v.is_list() && !v.as_list()->empty())) {
                        out += '\n';
                        serialize_node(v, out, indent + 2, false, false);
                    }
                }
            } else {
                // Multi-entry map after dash
                out += '\n';
                serialize_map(m, out, indent + 2, false);
            }
        }
        else if (elem.is_list()) {
            const auto& l = *elem.as_list();
            if (is_inline_candidate(elem)) {
                serialize_node(elem, out, indent, false, true);
            } else if (!l.empty()) {
                out += '\n';
                serialize_list_block(l, out, indent + 2);
            }
        }
    }
}

// =========================================================================
// Serialize an inline list: [a, b, c]
// =========================================================================
static void serialize_inline_list(const AstList& list, std::string& out) {
    out += '[';
    for (size_t i = 0; i < list.size(); ++i) {
        if (i > 0) out += ", ";
        const auto& elem = list[i];
        if (elem.is_null()) {
            out += "null";
        } else if (elem.is_scalar()) {
            serialize_scalar(elem.as_scalar()->value, out);
<<<<<<< Updated upstream
        }
    }
    out += ']';
}

=======
=======
// ============================================================
// STML 值是否需要引号
// ============================================================
static bool needs_quoting(const std::string& s) {
    if (s.empty()) return true;
    for (char c : s) {
        if (c == ':' || c == '#' || c == '[' || c == ']' ||
            c == '{' || c == '}' || c == '"' || c == '\'' ||
            c == '\n' || c == '\r' || c == '|') {
            return true;
        }
    }
    // 以特殊字符开头
    if (s[0] == '-' || s[0] == ' ' || s[0] == '\t' ||
        s[0] == '>' || s[0] == '!' || s[0] == '@' ||
        s[0] == '`' || s[0] == '%' || s[0] == '&' ||
        s[0] == '*') {
        return true;
    }
    // null 关键字
    if (s == "null" || s == "NULL" || s == "Null") return true;
    // 纯数字可能被误解
    return false;
}

// ============================================================
// 引号包裹
// ============================================================
static std::string quote_value(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    out += "\"";
    return out;
}

// ============================================================
// 判断节点是否需要展开（嵌套输出）
// ============================================================
static bool is_expandable(const AstNode& node) {
    if (node.is_map()) {
        return !node.as_map()->empty();
    }
    if (node.is_list()) {
        return !node.as_list()->items.empty();
    }
    return false;
}

// ============================================================
// 递归 STML 输出
// ============================================================
static void to_stml_impl(std::ostringstream& ss, const AstNode& node,
                          int indent, int indent_step, bool is_list_item) {
    std::string pad(indent, ' ');

    switch (node.kind) {
    case AstNode::Kind::Null:
        // 不输出任何值
        break;

    case AstNode::Kind::Scalar: {
        const std::string& val = node.as_scalar()->value;
        if (needs_quoting(val)) {
            ss << quote_value(val);
        } else {
            ss << val;
        }
        break;
    }

    case AstNode::Kind::List: {
        const auto& items = node.as_list()->items;
        for (size_t i = 0; i < items.size(); ++i) {
            ss << pad << "- ";
            if (is_expandable(items[i])) {
                ss << "\n";
                to_stml_impl(ss, items[i], indent + indent_step, indent_step, true);
            } else {
                to_stml_impl(ss, items[i], indent, indent_step, true);
            }
            if (i + 1 < items.size()) ss << "\n";
        }
        break;
    }

    case AstNode::Kind::Map: {
        const auto& map = *node.as_map();
        for (size_t i = 0; i < map.size(); ++i) {
            const auto& [k, v] = map[i];

            ss << pad << k << ":";

            if (v.is_null() || (v.is_scalar() && v.as_scalar()->value.empty())) {
                // 空值 → 冒号后面不输出
            } else if (v.is_scalar()) {
                ss << " ";
                to_stml_impl(ss, v, indent, indent_step, false);
            } else if (v.is_map()) {
                if (v.as_map()->empty()) {
                    ss << " {}";
                } else {
                    ss << "\n";
                    to_stml_impl(ss, v, indent + indent_step, indent_step, false);
                }
            } else if (v.is_list()) {
                if (v.as_list()->items.empty()) {
                    ss << " []";
                } else {
                    ss << "\n";
                    to_stml_impl(ss, v, indent + indent_step, indent_step, true);
                }
            }

            if (i + 1 < map.size()) {
                ss << "\n";
            }
        }
        break;
    }
    }
}

std::string to_stml(const AstNode& node, int base_indent, int indent_step) {
    std::ostringstream ss;
    to_stml_impl(ss, node, base_indent, indent_step, false);
    return ss.str();
}

// ============================================================
// 文档列表 → STML 多文档
// ============================================================
std::string docs_to_stml(const AstList& docs, int indent_step) {
    std::ostringstream ss;

    for (size_t i = 0; i < docs.items.size(); ++i) {
        if (i > 0) {
            ss << "---\n";
        }
        ss << to_stml(docs.items[i], 0, indent_step);
        if (i + 1 < docs.items.size() && !to_stml(docs.items[i], 0, indent_step).empty()) {
            ss << "\n";
>>>>>>> Stashed changes
        }
    }
    out += ']';
}

<<<<<<< Updated upstream
>>>>>>> Stashed changes
// =========================================================================
// Main recursive serializer
// =========================================================================
static void serialize_node(const AstNode& node, std::string& out, int indent,
                            bool is_list_item, bool inline_ok) {
    if (node.is_null()) {
        out += "null";
    }
    else if (node.is_scalar()) {
        serialize_scalar(node.as_scalar()->value, out);
    }
    else if (node.is_list()) {
        const auto& list = *node.as_list();
        if (inline_ok && is_inline_candidate(node)) {
            serialize_inline_list(list, out);
        } else {
            serialize_list_block(list, out, indent);
        }
    }
    else if (node.is_map()) {
        const auto& map = *node.as_map();
        serialize_map(map, out, indent, is_list_item);
    }
}

// =========================================================================
// Top-level: unwrap {"docs": [...]} and output each doc with --- separator
// =========================================================================
std::string to_stml(const AstNode& node) {
    std::string result;

    // The top-level node should be {"docs": [...]}
    const AstMap* top_map = node.as_map();
    if (!top_map) return result;

    const AstNode* docs_node = map_find(*top_map, "docs");
    if (!docs_node || !docs_node->is_list()) return result;

    const auto& docs_list = *docs_node->as_list();

    for (size_t i = 0; i < docs_list.size(); ++i) {
        if (i > 0) {
            result += "---\n";
        }

        const auto& doc = docs_list[i];
        if (doc.is_map()) {
            const auto& map = *doc.as_map();
            serialize_map(map, result, 0, false);
        }
        else if (doc.is_list()) {
            serialize_list_block(*doc.as_list(), result, 0);
        }
        else if (doc.is_null()) {
            result += "null";
        }
        else if (doc.is_scalar()) {
            serialize_scalar(doc.as_scalar()->value, result);
        }

        result += '\n';
    }

    return result;
<<<<<<< Updated upstream
=======
=======
    return ss.str();
>>>>>>> Stashed changes
>>>>>>> Stashed changes
}

} // namespace stml
