<<<<<<< Updated upstream
#include "tests/test_json.h"
#include "diagnostics/error.h"
#include <cctype>
#include <sstream>
=======
<<<<<<< Updated upstream
#include "tests/test_json.h"
#include "diagnostics/error.h"
#include <cctype>
#include <sstream>
=======
#include "test_json.h"
#include <cctype>
#include <stdexcept>
#include <vector>
>>>>>>> Stashed changes
>>>>>>> Stashed changes

namespace stml {
namespace test_json {

<<<<<<< Updated upstream
=======
<<<<<<< Updated upstream
>>>>>>> Stashed changes
// =========================================================================
// Forward declarations
// =========================================================================
static AstNode parse_value(const std::string& json, size_t& pos);
<<<<<<< Updated upstream

// =========================================================================
// Skip whitespace
// =========================================================================
static void skip_ws(const std::string& json, size_t& pos) {
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'
           || json[pos] == '\n' || json[pos] == '\r')) {
        ++pos;
    }
}

// =========================================================================
// Parse a JSON string
// =========================================================================
static std::string parse_string(const std::string& json, size_t& pos) {
    if (pos >= json.size() || json[pos] != '"') {
        throw ParseError(0, 0, "Expected '\"' at position " + std::to_string(pos));
    }
    ++pos; // skip opening "

    std::string result;
    while (pos < json.size()) {
        char c = json[pos];
        if (c == '"') {
            ++pos;
            return result;
        }
        if (c == '\\') {
            ++pos;
            if (pos >= json.size()) break;
            switch (json[pos]) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case '/':  result += '/';  break;
                case 'n':  result += '\n'; break;
                case 't':  result += '\t'; break;
                case 'r':  result += '\r'; break;
                case 'b':  result += '\b'; break;
                case 'f':  result += '\f'; break;
                case 'u': {
                    // \uXXXX — read 4 hex digits
                    if (pos + 4 >= json.size()) {
                        result += '\\';
                        result += 'u';
                        break;
                    }
                    uint16_t codepoint = 0;
                    for (int i = 0; i < 4; ++i) {
                        ++pos;
                        char h = json[pos];
                        codepoint <<= 4;
                        if (h >= '0' && h <= '9') codepoint |= (h - '0');
                        else if (h >= 'a' && h <= 'f') codepoint |= (h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') codepoint |= (h - 'A' + 10);
                        else {
                            --pos;
                            result += '?';
                            break;
                        }
                    }
                    // Simple UTF-8 encoding (BMP only)
                    if (codepoint < 0x80) {
                        result += static_cast<char>(codepoint);
                    } else if (codepoint < 0x800) {
                        result += static_cast<char>(0xC0 | (codepoint >> 6));
                        result += static_cast<char>(0x80 | (codepoint & 0x3F));
                    } else {
                        result += static_cast<char>(0xE0 | (codepoint >> 12));
                        result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (codepoint & 0x3F));
                    }
                    break;
                }
                default:
                    result += json[pos];
                    break;
            }
        } else {
            result += c;
        }
        ++pos;
    }

    throw ParseError(0, 0, "Unclosed string at position " + std::to_string(pos));
}

// =========================================================================
// Parse a JSON object
// =========================================================================
static AstNode parse_object(const std::string& json, size_t& pos) {
    if (pos >= json.size() || json[pos] != '{') {
        throw ParseError(0, 0, "Expected '{'");
    }
    ++pos; // skip '{'

    AstMap map;
    skip_ws(json, pos);

    if (pos < json.size() && json[pos] == '}') {
        ++pos;
        return AstNode(std::move(map));
    }

    while (pos < json.size()) {
        skip_ws(json, pos);
        std::string key = parse_string(json, pos);
        skip_ws(json, pos);

        if (pos >= json.size() || json[pos] != ':') {
            throw ParseError(0, 0, "Expected ':' after key \"" + key + "\"");
        }
        ++pos; // skip ':'

        skip_ws(json, pos);
        AstNode value = parse_value(json, pos);
        map.emplace_back(std::move(key), std::move(value));

        skip_ws(json, pos);
        if (pos < json.size() && json[pos] == ',') {
            ++pos;
            skip_ws(json, pos);
            if (pos < json.size() && json[pos] == '}') {
                // Trailing comma — ok, we're lenient
                ++pos;
                return AstNode(std::move(map));
            }
        } else if (pos < json.size() && json[pos] == '}') {
            ++pos;
            return AstNode(std::move(map));
        } else {
            throw ParseError(0, 0, "Expected ',' or '}' in object");
        }
    }

    throw ParseError(0, 0, "Unclosed object");
}

// =========================================================================
// Parse a JSON array
// =========================================================================
static AstNode parse_array(const std::string& json, size_t& pos) {
    if (pos >= json.size() || json[pos] != '[') {
        throw ParseError(0, 0, "Expected '['");
    }
    ++pos; // skip '['

    AstList list;
    skip_ws(json, pos);

    if (pos < json.size() && json[pos] == ']') {
        ++pos;
        return AstNode(std::move(list));
    }

    while (pos < json.size()) {
        skip_ws(json, pos);
        list.push_back(parse_value(json, pos));
        skip_ws(json, pos);

        if (pos < json.size() && json[pos] == ',') {
            ++pos;
            skip_ws(json, pos);
            if (pos < json.size() && json[pos] == ']') {
                ++pos;
                return AstNode(std::move(list));
            }
        } else if (pos < json.size() && json[pos] == ']') {
            ++pos;
            return AstNode(std::move(list));
        } else {
            throw ParseError(0, 0, "Expected ',' or ']' in array");
        }
    }

    throw ParseError(0, 0, "Unclosed array");
}

// =========================================================================
// Parse a JSON value (dispatched)
// =========================================================================
static AstNode parse_value(const std::string& json, size_t& pos) {
    skip_ws(json, pos);
    if (pos >= json.size()) {
        throw ParseError(0, 0, "Unexpected end of JSON input");
    }

    char c = json[pos];

    if (c == '"') {
        // String
        return AstNode(AstScalar(parse_string(json, pos)));
    }
    if (c == '{') {
        return parse_object(json, pos);
    }
    if (c == '[') {
        return parse_array(json, pos);
    }
    if (c == 'n' && json.substr(pos, 4) == "null") {
        pos += 4;
        return AstNode();
    }
    if (c == 't' && json.substr(pos, 4) == "true") {
        pos += 4;
        return AstNode(AstScalar("true"));
    }
    if (c == 'f' && json.substr(pos, 5) == "false") {
        pos += 5;
        return AstNode(AstScalar("false"));
    }

    // Number: read until non-digit-or-dot-or-e
    size_t start = pos;
    while (pos < json.size() && (std::isdigit(static_cast<unsigned char>(json[pos]))
           || json[pos] == '.' || json[pos] == '-' || json[pos] == '+'
           || json[pos] == 'e' || json[pos] == 'E')) {
        ++pos;
    }
    if (pos > start) {
        return AstNode(AstScalar(json.substr(start, pos - start)));
    }

    throw ParseError(0, 0,
        std::string("Unexpected character '") + c + "' at position "
        + std::to_string(pos));
}

// =========================================================================
// Public API
// =========================================================================
AstNode parse(const std::string& json) {
    size_t pos = 0;
    skip_ws(json, pos);
    if (pos >= json.size()) {
        return AstNode(); // empty input
    }
    return parse_value(json, pos);
}

=======

// =========================================================================
// Skip whitespace
// =========================================================================
static void skip_ws(const std::string& json, size_t& pos) {
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'
           || json[pos] == '\n' || json[pos] == '\r')) {
        ++pos;
    }
}

// =========================================================================
// Parse a JSON string
// =========================================================================
static std::string parse_string(const std::string& json, size_t& pos) {
    if (pos >= json.size() || json[pos] != '"') {
        throw ParseError(0, 0, "Expected '\"' at position " + std::to_string(pos));
    }
    ++pos; // skip opening "

    std::string result;
    while (pos < json.size()) {
        char c = json[pos];
        if (c == '"') {
            ++pos;
            return result;
        }
        if (c == '\\') {
            ++pos;
            if (pos >= json.size()) break;
            switch (json[pos]) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case '/':  result += '/';  break;
                case 'n':  result += '\n'; break;
                case 't':  result += '\t'; break;
                case 'r':  result += '\r'; break;
                case 'b':  result += '\b'; break;
                case 'f':  result += '\f'; break;
                case 'u': {
                    // \uXXXX — read 4 hex digits
                    if (pos + 4 >= json.size()) {
                        result += '\\';
                        result += 'u';
                        break;
                    }
                    uint16_t codepoint = 0;
                    for (int i = 0; i < 4; ++i) {
                        ++pos;
                        char h = json[pos];
                        codepoint <<= 4;
                        if (h >= '0' && h <= '9') codepoint |= (h - '0');
                        else if (h >= 'a' && h <= 'f') codepoint |= (h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') codepoint |= (h - 'A' + 10);
                        else {
                            --pos;
                            result += '?';
                            break;
                        }
                    }
                    // Simple UTF-8 encoding (BMP only)
                    if (codepoint < 0x80) {
                        result += static_cast<char>(codepoint);
                    } else if (codepoint < 0x800) {
                        result += static_cast<char>(0xC0 | (codepoint >> 6));
                        result += static_cast<char>(0x80 | (codepoint & 0x3F));
                    } else {
                        result += static_cast<char>(0xE0 | (codepoint >> 12));
                        result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (codepoint & 0x3F));
                    }
                    break;
                }
                default:
                    result += json[pos];
                    break;
            }
        } else {
            result += c;
        }
        ++pos;
    }

    throw ParseError(0, 0, "Unclosed string at position " + std::to_string(pos));
}

// =========================================================================
// Parse a JSON object
// =========================================================================
static AstNode parse_object(const std::string& json, size_t& pos) {
    if (pos >= json.size() || json[pos] != '{') {
        throw ParseError(0, 0, "Expected '{'");
    }
    ++pos; // skip '{'

    AstMap map;
    skip_ws(json, pos);

    if (pos < json.size() && json[pos] == '}') {
        ++pos;
        return AstNode(std::move(map));
    }

    while (pos < json.size()) {
        skip_ws(json, pos);
        std::string key = parse_string(json, pos);
        skip_ws(json, pos);

        if (pos >= json.size() || json[pos] != ':') {
            throw ParseError(0, 0, "Expected ':' after key \"" + key + "\"");
        }
        ++pos; // skip ':'

        skip_ws(json, pos);
        AstNode value = parse_value(json, pos);
        map.emplace_back(std::move(key), std::move(value));

        skip_ws(json, pos);
        if (pos < json.size() && json[pos] == ',') {
            ++pos;
            skip_ws(json, pos);
            if (pos < json.size() && json[pos] == '}') {
                // Trailing comma — ok, we're lenient
                ++pos;
                return AstNode(std::move(map));
            }
        } else if (pos < json.size() && json[pos] == '}') {
            ++pos;
            return AstNode(std::move(map));
        } else {
            throw ParseError(0, 0, "Expected ',' or '}' in object");
        }
    }

    throw ParseError(0, 0, "Unclosed object");
}

// =========================================================================
// Parse a JSON array
// =========================================================================
static AstNode parse_array(const std::string& json, size_t& pos) {
    if (pos >= json.size() || json[pos] != '[') {
        throw ParseError(0, 0, "Expected '['");
    }
    ++pos; // skip '['

    AstList list;
    skip_ws(json, pos);

    if (pos < json.size() && json[pos] == ']') {
        ++pos;
        return AstNode(std::move(list));
    }

    while (pos < json.size()) {
        skip_ws(json, pos);
        list.push_back(parse_value(json, pos));
        skip_ws(json, pos);

        if (pos < json.size() && json[pos] == ',') {
            ++pos;
            skip_ws(json, pos);
            if (pos < json.size() && json[pos] == ']') {
                ++pos;
                return AstNode(std::move(list));
            }
        } else if (pos < json.size() && json[pos] == ']') {
            ++pos;
            return AstNode(std::move(list));
        } else {
            throw ParseError(0, 0, "Expected ',' or ']' in array");
        }
    }

    throw ParseError(0, 0, "Unclosed array");
}

// =========================================================================
// Parse a JSON value (dispatched)
// =========================================================================
static AstNode parse_value(const std::string& json, size_t& pos) {
    skip_ws(json, pos);
    if (pos >= json.size()) {
        throw ParseError(0, 0, "Unexpected end of JSON input");
    }

    char c = json[pos];

    if (c == '"') {
        // String
        return AstNode(AstScalar(parse_string(json, pos)));
    }
    if (c == '{') {
        return parse_object(json, pos);
    }
    if (c == '[') {
        return parse_array(json, pos);
    }
    if (c == 'n' && json.substr(pos, 4) == "null") {
        pos += 4;
        return AstNode();
    }
    if (c == 't' && json.substr(pos, 4) == "true") {
        pos += 4;
        return AstNode(AstScalar("true"));
    }
    if (c == 'f' && json.substr(pos, 5) == "false") {
        pos += 5;
        return AstNode(AstScalar("false"));
    }

    // Number: read until non-digit-or-dot-or-e
    size_t start = pos;
    while (pos < json.size() && (std::isdigit(static_cast<unsigned char>(json[pos]))
           || json[pos] == '.' || json[pos] == '-' || json[pos] == '+'
           || json[pos] == 'e' || json[pos] == 'E')) {
        ++pos;
    }
    if (pos > start) {
        return AstNode(AstScalar(json.substr(start, pos - start)));
    }

    throw ParseError(0, 0,
        std::string("Unexpected character '") + c + "' at position "
        + std::to_string(pos));
}

// =========================================================================
// Public API
// =========================================================================
AstNode parse(const std::string& json) {
    size_t pos = 0;
    skip_ws(json, pos);
    if (pos >= json.size()) {
        return AstNode(); // empty input
    }
    return parse_value(json, pos);
}

=======
class Parser {
public:
    explicit Parser(const std::string& text) : text_(text), pos_(0) {}

    stml::AstNode parse_value() {
        skip_ws();
        if (pos_ >= text_.size()) throw std::runtime_error("Unexpected EOF");
        char c = text_[pos_];
        if (c == '"') return parse_string();
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == 'n') { expect("null"); return stml::AstNode(stml::NullNode{}); }
        if (c == 't' || c == 'f') return parse_bool_or_string();
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number();
        throw std::runtime_error(std::string("Unexpected char: ") + c);
    }

private:
    const std::string& text_;
    size_t pos_;

    void skip_ws() {
        while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\t' ||
                                        text_[pos_] == '\n' || text_[pos_] == '\r')) {
            ++pos_;
        }
    }
    void expect(const char* s) {
        for (; *s; ++s) {
            if (pos_ >= text_.size() || text_[pos_] != *s)
                throw std::runtime_error("Expected " + std::string(s));
            ++pos_;
        }
    }
    char peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }

    stml::AstNode parse_string() {
        ++pos_; // skip opening "
        std::string val;
        while (pos_ < text_.size() && text_[pos_] != '"') {
            if (text_[pos_] == '\\' && pos_ + 1 < text_.size()) {
                ++pos_;
                switch (text_[pos_]) {
                    case '"':  val += '"'; break;
                    case '\\': val += '\\'; break;
                    case '/':  val += '/'; break;
                    case 'n':  val += '\n'; break;
                    case 'r':  val += '\r'; break;
                    case 't':  val += '\t'; break;
                    case 'b':  val += '\b'; break;
                    case 'f':  val += '\f'; break;
                    case 'u':  val += "\\u"; break; // 简化处理
                    default:   val += text_[pos_]; break;
                }
            } else {
                val += text_[pos_];
            }
            ++pos_;
        }
        if (pos_ < text_.size()) ++pos_; // skip closing "
        return stml::AstNode(stml::AstScalar{val});
    }

    stml::AstNode parse_object() {
        ++pos_; // skip '{'
        stml::AstMap map;
        skip_ws();
        if (peek() == '}') { ++pos_; return stml::AstNode(std::move(map)); }

        while (true) {
            skip_ws();
            auto key_node = parse_string();
            std::string key = key_node.as_scalar()->value;
            skip_ws();
            if (peek() == ':') ++pos_;
            skip_ws();
            auto val = parse_value();
            map.emplace_back(key, std::move(val));
            skip_ws();
            if (peek() == ',') { ++pos_; continue; }
            if (peek() == '}') { ++pos_; break; }
            throw std::runtime_error("Expected ',' or '}' in object");
        }
        return stml::AstNode(std::move(map));
    }

    stml::AstNode parse_array() {
        ++pos_; // skip '['
        stml::AstList list;
        skip_ws();
        if (peek() == ']') { ++pos_; return stml::AstNode(std::move(list)); }

        while (true) {
            skip_ws();
            list.items.push_back(parse_value());
            skip_ws();
            if (peek() == ',') { ++pos_; continue; }
            if (peek() == ']') { ++pos_; break; }
            throw std::runtime_error("Expected ',' or ']' in array");
        }
        return stml::AstNode(std::move(list));
    }

    stml::AstNode parse_number() {
        size_t start = pos_;
        if (peek() == '-') ++pos_;
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        if (peek() == '.') {
            ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        }
        if (peek() == 'e' || peek() == 'E') {
            ++pos_;
            if (peek() == '+' || peek() == '-') ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        }
        return stml::AstNode(stml::AstScalar{text_.substr(start, pos_ - start)});
    }

    stml::AstNode parse_bool_or_string() {
        // JSON booleans → treated as strings in STML context
        size_t start = pos_;
        while (pos_ < text_.size() && std::isalpha(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        return stml::AstNode(stml::AstScalar{text_.substr(start, pos_ - start)});
    }
};

stml::AstNode parse(const std::string& json) {
    Parser p(json);
    return p.parse_value();
}

>>>>>>> Stashed changes
>>>>>>> Stashed changes
} // namespace test_json
} // namespace stml
