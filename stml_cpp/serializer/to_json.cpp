#include "serializer/serializer.h"

#include <sstream>

namespace stml {

// =========================================================================
// to_json — AST → canonical JSON text
// =========================================================================

static std::string json_escape(const std::string& s)
{
    std::string result;
    result.reserve(s.size() + 8);
    for (char ch : s) {
        switch (ch) {
            case '\\': result += "\\\\"; break;
            case '"':  result += "\\\""; break;
            case '\n': result += "\\n";  break;
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            case '\b': result += "\\b";  break;
            case '\f': result += "\\f";  break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    result += "\\u00";
                    result += "0123456789abcdef"[ch >> 4];
                    result += "0123456789abcdef"[ch & 0xf];
                } else {
                    result.push_back(ch);
                }
                break;
        }
    }
    return result;
}

static void to_json_impl(const AstNode& node, std::ostringstream& os, int indent_level);

static void write_indent(std::ostringstream& os, int level)
{
    for (int i = 0; i < level * 2; ++i)
        os.put(' ');
}

static void to_json_impl(const AstNode& node, std::ostringstream& os, int indent_level)
{
    if (node.is_null()) {
        os << "null";
    } else if (node.is_string()) {
        os << '"' << json_escape(*node.as_string()) << '"';
    } else if (node.is_list()) {
        const auto& lst = *node.as_list();
        if (lst.empty()) {
            os << "[]";
        } else {
            os << "[\n";
            for (size_t i = 0; i < lst.size(); ++i) {
                write_indent(os, indent_level + 1);
                to_json_impl(lst[i], os, indent_level + 1);
                if (i + 1 < lst.size()) os << ",";
                os << "\n";
            }
            write_indent(os, indent_level);
            os << "]";
        }
    } else if (node.is_map()) {
        const auto& map = *node.as_map();
        if (map.empty()) {
            os << "{}";
        } else {
            os << "{\n";
            size_t i = 0;
            for (const auto& [k, v] : map) {
                write_indent(os, indent_level + 1);
                os << '"' << json_escape(k) << "\": ";
                to_json_impl(v, os, indent_level + 1);
                if (++i < map.size()) os << ",";
                os << "\n";
            }
            write_indent(os, indent_level);
            os << "}";
        }
    }
}

std::string to_json(const AstNode& node)
{
    std::ostringstream os;
    to_json_impl(node, os, 0);
    os << "\n";
    return os.str();
}

} // namespace stml
