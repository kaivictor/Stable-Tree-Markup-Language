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
// JSON string escaping
// =========================================================================
static void json_escape(const std::string& src, std::string& dst) {
    dst.reserve(dst.size() + src.size() + 4);
    for (unsigned char c : src) {
        switch (c) {
            case '"':  dst += "\\\""; break;
            case '\\': dst += "\\\\"; break;
            case '\n': dst += "\\n";  break;
            case '\r': dst += "\\r";  break;
            case '\t': dst += "\\t";  break;
            case '\b': dst += "\\b";  break;
            case '\f': dst += "\\f";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    dst += buf;
                } else {
                    dst += static_cast<char>(c);
                }
                break;
        }
    }
}

// =========================================================================
// Recursive JSON serialization
// =========================================================================
static void to_json_impl(const AstNode& node, std::string& out, int indent) {
    std::string pad(indent, ' ');

    if (node.is_null()) {
        out += "null";
    }
    else if (node.is_scalar()) {
        out += '"';
        json_escape(node.as_scalar()->value, out);
        out += '"';
    }
    else if (node.is_list()) {
        const auto& list = *node.as_list();
        if (list.empty()) {
            out += "[]";
            return;
        }
        out += "[\n";
        std::string child_pad(indent + 2, ' ');
        for (size_t i = 0; i < list.size(); ++i) {
            out += child_pad;
            to_json_impl(list[i], out, indent + 2);
            if (i + 1 < list.size()) out += ',';
            out += '\n';
        }
        out += pad;
        out += ']';
    }
    else if (node.is_map()) {
        const auto& map = *node.as_map();
        if (map.empty()) {
            out += "{}";
            return;
        }
        out += "{\n";
        std::string child_pad(indent + 2, ' ');
        for (size_t i = 0; i < map.size(); ++i) {
            out += child_pad;
            out += '"';
            json_escape(map[i].first, out);
            out += "\": ";
            to_json_impl(map[i].second, out, indent + 2);
            if (i + 1 < map.size()) out += ',';
            out += '\n';
        }
        out += pad;
        out += '}';
    }
}

std::string to_json(const AstNode& node) {
    std::string result;
    to_json_impl(node, result, 0);
    result += '\n';
    return result;
<<<<<<< Updated upstream
=======
=======
// ============================================================
// JSON 字符串转义
// ============================================================
static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

// ============================================================
// 递归 JSON 序列化
// ============================================================
static void to_json_impl(std::ostringstream& ss, const AstNode& node, int indent, int depth) {
    std::string pad(depth * indent, ' ');
    std::string inner_pad((depth + 1) * indent, ' ');

    switch (node.kind) {
    case AstNode::Kind::Null:
        ss << "null";
        break;

    case AstNode::Kind::Scalar: {
        ss << "\"" << json_escape(node.as_scalar()->value) << "\"";
        break;
    }

    case AstNode::Kind::List: {
        const auto& items = node.as_list()->items;
        if (items.empty()) {
            ss << "[]";
            break;
        }
        ss << "[\n";
        for (size_t i = 0; i < items.size(); ++i) {
            ss << inner_pad;
            to_json_impl(ss, items[i], indent, depth + 1);
            if (i + 1 < items.size()) ss << ",";
            ss << "\n";
        }
        ss << pad << "]";
        break;
    }

    case AstNode::Kind::Map: {
        const auto& map = *node.as_map();
        if (map.empty()) {
            ss << "{}";
            break;
        }
        ss << "{\n";
        for (size_t i = 0; i < map.size(); ++i) {
            const auto& [k, v] = map[i];
            ss << inner_pad << "\"" << json_escape(k) << "\": ";
            to_json_impl(ss, v, indent, depth + 1);
            if (i + 1 < map.size()) ss << ",";
            ss << "\n";
        }
        ss << pad << "}";
        break;
    }
    }
}

std::string to_json(const AstNode& node, int indent) {
    std::ostringstream ss;
    to_json_impl(ss, node, indent, 0);
    return ss.str();
}

// ============================================================
// 文档列表 → wrapped JSON
// ============================================================
std::string docs_to_json(const AstList& docs, int indent) {
    std::ostringstream ss;
    ss << "{\n  \"docs\": [\n";

    for (size_t i = 0; i < docs.items.size(); ++i) {
        // 每个文档缩进 4 空格
        ss << "    ";
        to_json_impl(ss, docs.items[i], indent, 2);
        if (i + 1 < docs.items.size()) ss << ",";
        ss << "\n";
    }

    ss << "  ]\n}";
    return ss.str();
>>>>>>> Stashed changes
>>>>>>> Stashed changes
}

} // namespace stml
