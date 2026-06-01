#include "serializer/serializer.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace stml {

// =========================================================================
// Escape helpers
// =========================================================================

std::string escape_string(const std::string& s)
{
    std::string result;
    result.reserve(s.size() + 8);
    for (char ch : s) {
        switch (ch) {
            case '\\': result += "\\\\"; break;
            case '"':  result += "\\\""; break;
            case '\n': result += "\\n";  break;
            case '\t': result += "\\t";  break;
            default:   result.push_back(ch); break;
        }
    }
    return result;
}

std::string quote_key(const std::string& key)
{
    return "\"" + escape_string(key) + "\"";
}

// =========================================================================
// format_scalar
// =========================================================================

std::string format_scalar(const AstNode& node)
{
    if (node.is_null()) {
        return "null";
    }
    if (node.is_string()) {
        return "\"" + escape_string(*node.as_string()) + "\"";
    }
    // For non-scalar nodes, shouldn't happen but handle gracefully
    if (node.is_map() && node.as_map()->empty()) {
        return "null";
    }
    if (node.is_list() && node.as_list()->empty()) {
        return "null";
    }
    throw std::runtime_error("format_scalar called on non-scalar node");
}

// =========================================================================
// serialize_node
// =========================================================================

// Internal helper
static bool values_are_all_scalars(const AstList& lst)
{
    for (const auto& v : lst) {
        if (v.is_list() || v.is_map()) return false;
    }
    return true;
}

static std::string format_inline_list(const AstList& lst)
{
    std::string result = "[";
    for (size_t i = 0; i < lst.size(); ++i) {
        if (i > 0) result += ", ";
        result += format_scalar(lst[i]);
    }
    result += "]";
    return result;
}

std::string serialize_node(const AstNode& node, int indent_level)
{
    std::string indent(indent_level * 2, ' ');
    std::string next_indent((indent_level + 1) * 2, ' ');

    // 1. Null
    if (node.is_null()) {
        return indent + "null";
    }

    // 2. String (scalar)
    if (node.is_string()) {
        return indent + "\"" + escape_string(*node.as_string()) + "\"";
    }

    // 3. List (sequence)
    if (node.is_list()) {
        const auto& lst = *node.as_list();
        if (lst.empty()) {
            return indent + "- null";
        }

        std::vector<std::string> lines;
        for (const auto& item : lst) {
            if (item.is_null()) {
                // Null entry: just "-" (avoids "- null" re-parsing as {"": null})
                lines.push_back(indent + "-");
            } else if (item.is_string()) {
                // Scalar entry: - value
                lines.push_back(indent + "- " + format_scalar(item));
            } else if (item.is_list()) {
                // Nested list item
                const auto& inner_lst = *item.as_list();
                if (inner_lst.empty()) {
                    lines.push_back(indent + "- null");
                } else if (values_are_all_scalars(inner_lst)) {
                    // Inline list: - [v1, v2, v3]
                    lines.push_back(indent + "- " + format_inline_list(inner_lst));
                } else {
                    // Complex nested list: sub-block
                    lines.push_back(indent + "- ");
                    lines.push_back(serialize_node(item, indent_level + 1));
                }
            } else if (item.is_map()) {
                // Map entry (supports multi-key)
                const auto& item_map = *item.as_map();
                if (item_map.empty()) {
                    lines.push_back(indent + "- null");
                    continue;
                }
                bool first = true;
                for (const auto& [key, val] : item_map) {
                    int entry_level = first ? indent_level : indent_level + 1;
                    std::string entry_indent(entry_level * 2, ' ');
                    std::string prefix = first ? "- " : "";
                    if (val.is_list() || val.is_map()) {
                        // Compound value
                        lines.push_back(entry_indent + prefix + quote_key(key) + ":");
                        lines.push_back(serialize_node(val, entry_level + 1));
                    } else if (val.is_null()) {
                        // Null value: omit ": null" to avoid ambiguity
                        // (": null" makes has_inline_value()=false, causing
                        //  children to be treated as the key's value)
                        lines.push_back(entry_indent + prefix + quote_key(key));
                    } else {
                        // Scalar value
                        lines.push_back(entry_indent + prefix + quote_key(key) + ": " + format_scalar(val));
                    }
                    first = false;
                }
            }
        }
        // Join lines with newlines
        std::string result;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i > 0) result += "\n";
            result += lines[i];
        }
        return result;
    }

    // 4. Map (mapping)
    if (node.is_map()) {
        const auto& map = *node.as_map();
        if (map.empty()) {
            return indent + "null";
        }

        std::vector<std::string> lines;
        for (const auto& [key, val] : map) {
            std::string k_str = quote_key(key);
            if (val.is_list() || val.is_map()) {
                // Compound value
                std::string sub = serialize_node(val, indent_level + 1);
                // Strip trailing whitespace for null check
                const char* sp = sub.c_str();
                while (*sp == ' ') ++sp; // shouldn't happen for indented
                if (*sp == '\0' || sub.empty()) {
                    lines.push_back(indent + k_str + ": null");
                } else {
                    lines.push_back(indent + k_str + ":");
                    lines.push_back(sub);
                }
            } else {
                // Scalar value
                lines.push_back(indent + k_str + ": " + format_scalar(val));
            }
        }

        std::string result;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i > 0) result += "\n";
            result += lines[i];
        }
        return result;
    }

    throw std::runtime_error("Unsupported node type in serialize_node");
}

// =========================================================================
// dumps — top-level serialization
// =========================================================================

std::string dumps(const AstNode& node)
{
    if (node.is_map()) {
        const auto& map = *node.as_map();
        // Check if it's a "docs"-wrapped document list
        const AstNode* docs_node = map_find(map, "docs");
        if (docs_node != nullptr && docs_node->is_list()) {
            const auto& docs = *docs_node->as_list();

            std::vector<std::string> parts;
            for (const auto& doc : docs) {
                parts.push_back(serialize_node(doc, 0));
            }
            // Join with "\n---\n" document separator
            std::string result;
            for (size_t i = 0; i < parts.size(); ++i) {
                result += parts[i];
                if (i + 1 < parts.size())
                    result += "\n---";
                result += "\n";
            }
            return result;
        }
    }

    // Unwrapped single document
    return serialize_node(node, 0) + "\n";
}

} // namespace stml
