/// Minimal JSON parser for test verification only.
/// Parses a JSON string into AstNode for comparison with parser output.

#include "test_json.h"
#include <cctype>
#include <stdexcept>
#include <string>

namespace test_json {

class Parser {
public:
    explicit Parser(const std::string& text) : m_text(text), m_pos(0), m_len(static_cast<int>(text.size())) {}

    stml::AstNode parse_value() {
        skip_ws();
        if (m_pos >= m_len) throw std::runtime_error("Unexpected EOF");

        char ch = m_text[m_pos];
        if (ch == '"') return parse_string();
        if (ch == '{') return parse_object();
        if (ch == '[') return parse_array();
        if (ch == 'n') {
            expect("null");
            return stml::AstNode();
        }
        if (ch == 't' || ch == 'f') {
            // booleans → treated as strings in STML
            return parse_string();
        }
        if (ch == '-' || (ch >= '0' && ch <= '9')) {
            return parse_number();
        }
        throw std::runtime_error(std::string("Unexpected char: ") + ch);
    }

private:
    void skip_ws() {
        while (m_pos < m_len) {
            char ch = m_text[m_pos];
            if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
                ++m_pos;
            else
                break;
        }
    }

    stml::AstNode parse_string() {
        ++m_pos; // skip opening "
        std::string result;
        while (m_pos < m_len) {
            char ch = m_text[m_pos++];
            if (ch == '"') {
                return stml::AstNode(result);
            }
            if (ch == '\\') {
                if (m_pos >= m_len) throw std::runtime_error("Unexpected EOF in escape");
                char esc = m_text[m_pos++];
                switch (esc) {
                    case '"':  result.push_back('"'); break;
                    case '\\': result.push_back('\\'); break;
                    case '/':  result.push_back('/'); break;
                    case 'n':  result.push_back('\n'); break;
                    case 'r':  result.push_back('\r'); break;
                    case 't':  result.push_back('\t'); break;
                    case 'b':  result.push_back('\b'); break;
                    case 'f':  result.push_back('\f'); break;
                    case 'u': {
                        // Simple unicode escape (only handles BMP)
                        if (m_pos + 4 > m_len) throw std::runtime_error("Unexpected EOF in \\u");
                        std::string hex = m_text.substr(m_pos, 4);
                        m_pos += 4;
                        int codepoint = std::stoi(hex, nullptr, 16);
                        if (codepoint < 0x80) {
                            result.push_back(static_cast<char>(codepoint));
                        } else if (codepoint < 0x800) {
                            result.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
                            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                        } else {
                            result.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
                            result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                        }
                        break;
                    }
                    default: result.push_back('\\'); result.push_back(esc); break;
                }
            } else {
                result.push_back(ch);
            }
        }
        throw std::runtime_error("Unclosed string");
    }

    stml::AstNode parse_object() {
        ++m_pos; // skip {
        stml::AstMap map;
        skip_ws();
        if (m_pos < m_len && m_text[m_pos] == '}') {
            ++m_pos;
            return stml::AstNode(std::move(map));
        }
        while (true) {
            skip_ws();
            if (m_text[m_pos] != '"') throw std::runtime_error("Expected string key");
            stml::AstNode key_node = parse_string();
            std::string key = *key_node.as_string();
            skip_ws();
            if (m_text[m_pos++] != ':') throw std::runtime_error("Expected ':'");
            skip_ws();
            stml::AstNode val = parse_value();
            map.emplace_back(std::move(key), std::move(val));
            skip_ws();
            if (m_pos < m_len && m_text[m_pos] == ',') {
                ++m_pos;
                continue;
            }
            if (m_text[m_pos] == '}') {
                ++m_pos;
                break;
            }
            throw std::runtime_error("Expected ',' or '}' in object");
        }
        return stml::AstNode(std::move(map));
    }

    stml::AstNode parse_array() {
        ++m_pos; // skip [
        stml::AstList list;
        skip_ws();
        if (m_pos < m_len && m_text[m_pos] == ']') {
            ++m_pos;
            return stml::AstNode(std::move(list));
        }
        while (true) {
            skip_ws();
            list.push_back(parse_value());
            skip_ws();
            if (m_pos < m_len && m_text[m_pos] == ',') {
                ++m_pos;
                continue;
            }
            if (m_text[m_pos] == ']') {
                ++m_pos;
                break;
            }
            throw std::runtime_error("Expected ',' or ']' in array");
        }
        return stml::AstNode(std::move(list));
    }

    stml::AstNode parse_number() {
        int start = m_pos;
        if (m_text[m_pos] == '-') ++m_pos;
        while (m_pos < m_len && std::isdigit(m_text[m_pos])) ++m_pos;
        if (m_pos < m_len && m_text[m_pos] == '.') {
            ++m_pos;
            while (m_pos < m_len && std::isdigit(m_text[m_pos])) ++m_pos;
        }
        if (m_pos < m_len && (m_text[m_pos] == 'e' || m_text[m_pos] == 'E')) {
            ++m_pos;
            if (m_pos < m_len && (m_text[m_pos] == '+' || m_text[m_pos] == '-')) ++m_pos;
            while (m_pos < m_len && std::isdigit(m_text[m_pos])) ++m_pos;
        }
        // All STML scalars are strings, so return as string
        return stml::AstNode(m_text.substr(start, m_pos - start));
    }

    void expect(const char* word) {
        while (*word) {
            if (m_pos >= m_len || m_text[m_pos++] != *word++)
                throw std::runtime_error("Expected " + std::string(word));
        }
    }

    std::string m_text;
    int m_pos;
    int m_len;
};

stml::AstNode parse(const std::string& json_text) {
    Parser p(json_text);
    return p.parse_value();
}

stml::AstNode load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filepath);
    }
    std::string text((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
    return parse(text);
}

} // namespace test_json
