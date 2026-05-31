#ifndef STML_TOKEN_H
#define STML_TOKEN_H

#include <string>
#include <vector>
<<<<<<< Updated upstream
=======
<<<<<<< Updated upstream
>>>>>>> Stashed changes

namespace stml {

// =========================================================================
// TokenType — all token kinds produced by the lexer (14 types).
// =========================================================================
enum class TokenType : uint8_t {
    NEWLINE,           // logical newline (emitted after line processing)
    INDENT,            // increase in indentation level
    DEDENT,            // decrease in indentation level
    DOC_SEPARATOR,     // --- on its own line
    END,               // end of token stream
    KEY,               // a mapping key (left of ':')
    BARE_KEY,          // a key-value line where the whole content is one key
    COLON,             // the ':' between key and value
    SCALAR,            // a scalar string value
    NULL_,             // null literal (~ or null)
    RAW_STRING,        // unquoted raw string (recovery fallback)
    DASH,              // '-' sequence item mark
    INLINE_LIST,       // [...]-style inline list
    MULTILINE_STRING,  // {...} multiline text block
};

/// Return a human-readable name for a token type.
inline const char* token_type_name(TokenType tt) {
    switch (tt) {
        case TokenType::NEWLINE:          return "NEWLINE";
        case TokenType::INDENT:           return "INDENT";
        case TokenType::DEDENT:           return "DEDENT";
        case TokenType::DOC_SEPARATOR:    return "DOC_SEPARATOR";
        case TokenType::END:              return "END";
        case TokenType::KEY:              return "KEY";
        case TokenType::BARE_KEY:         return "BARE_KEY";
        case TokenType::COLON:            return "COLON";
        case TokenType::SCALAR:           return "SCALAR";
        case TokenType::NULL_:            return "NULL_";
        case TokenType::RAW_STRING:       return "RAW_STRING";
        case TokenType::DASH:             return "DASH";
        case TokenType::INLINE_LIST:      return "INLINE_LIST";
        case TokenType::MULTILINE_STRING: return "MULTILINE_STRING";
    }
    return "UNKNOWN";
}

// =========================================================================
// InlineElem — a single element in an inline list.
// A monostate element represents null, a string element is a quoted or
// unquoted scalar.
// =========================================================================
struct InlineElem {
    enum class Kind : uint8_t { NULL_VAL, SCALAR };
    Kind kind;
    std::string value;  // empty for NULL_VAL

    InlineElem() : kind(Kind::NULL_VAL) {}
    explicit InlineElem(std::string v) : kind(Kind::SCALAR), value(std::move(v)) {}

    bool is_null() const { return kind == Kind::NULL_VAL; }
    bool is_scalar() const { return kind == Kind::SCALAR; }
};

// =========================================================================
// TokenValue — the data payload of a token.
// - monostate:  no associated data (NEWLINE, INDENT, DEDENT, COLON, etc.)
// - string:     KEY, SCALAR, BARE_KEY, RAW_STRING, MULTILINE_STRING
// - InlineElem vector: INLINE_LIST
// =========================================================================
using TokenValue = std::variant<std::monostate, std::string, std::vector<InlineElem>>;
<<<<<<< Updated upstream
=======
=======
#include <variant>
#include <cstdint>

namespace stml {
>>>>>>> Stashed changes

// ============================================================
// 14 Token 类型
// ============================================================
enum class TokenType : uint8_t {
    DASH,                // -
    KEY,                 // 键名（冒号前）
    COLON,               // :
    SCALAR,              // 普通字符串值
    NULL_,               // null 关键字
    BARE_KEY,            // 无冒号的键（单独一行）
    INDENT,              // 缩进增加
    DEDENT,              // 缩进减少
    NEWLINE,             // 换行
    DOC_SEPARATOR,       // --- 文档分隔符（仅 indent=0 时有效）
    INLINE_LIST,         // 行内列表值
    MULTILINE_STRING,    // 多行字符串
    RAW_STRING,          // 原始字符串
    END,                 // 输入结束
};

inline const char* token_type_name(TokenType t) {
    switch (t) {
        case TokenType::DASH:              return "DASH";
        case TokenType::KEY:               return "KEY";
        case TokenType::COLON:             return "COLON";
        case TokenType::SCALAR:            return "SCALAR";
        case TokenType::NULL_:              return "NULL_";
        case TokenType::BARE_KEY:          return "BARE_KEY";
        case TokenType::INDENT:            return "INDENT";
        case TokenType::DEDENT:            return "DEDENT";
        case TokenType::NEWLINE:           return "NEWLINE";
        case TokenType::DOC_SEPARATOR:     return "DOC_SEPARATOR";
        case TokenType::INLINE_LIST:       return "INLINE_LIST";
        case TokenType::MULTILINE_STRING:  return "MULTILINE_STRING";
        case TokenType::RAW_STRING:        return "RAW_STRING";
        case TokenType::END:               return "END";
        default:                           return "UNKNOWN";
    }
}

// InlineList 元素
struct InlineElem {
    enum class Kind { SCALAR, NULL_VAL, INLINE_LIST } kind;
    std::string value; // SCALAR 的值，其他情况为空
};
>>>>>>> Stashed changes

// ============================================================
// Token 数据结构
// ============================================================
struct Token {
    TokenType type;
<<<<<<< Updated upstream
    TokenValue value;
    int line;      // 1-based source line number
    int column;    // 1-based source column number
    int indent;    // indentation level (for INDENT tokens)

    Token() : type(TokenType::END), line(0), column(0), indent(-1) {}

    Token(TokenType t, int l, int c)
        : type(t), line(l), column(c), indent(-1) {}

    Token(TokenType t, std::string val, int l, int c)
        : type(t), value(std::move(val)), line(l), column(c), indent(-1) {}

    Token(TokenType t, std::vector<InlineElem> elems, int l, int c)
        : type(t), value(std::move(elems)), line(l), column(c), indent(-1) {}
<<<<<<< Updated upstream
=======
=======
    std::string value;                              // SCALAR/KEY/BARE_KEY 的文本
    int line = 0;                                  // 源文件行号 (1-based)
    int col = 0;                                    // 源文件列号 (1-based)
    int indent = 0;                                 // INDENT/DEDENT 的缩进列号
    std::vector<InlineElem> inline_list_items;     // INLINE_LIST 的元素
    int dedent_count = 1;                           // DEDENT 的级数

    static Token make(TokenType t, int l, int c) {
        Token tok;
        tok.type = t;
        tok.line = l;
        tok.col = c;
        return tok;
    }

    static Token make_with_val(TokenType t, std::string v, int l, int c) {
        Token tok;
        tok.type = t;
        tok.value = std::move(v);
        tok.line = l;
        tok.col = c;
        return tok;
    }
>>>>>>> Stashed changes
>>>>>>> Stashed changes
};

} // namespace stml

#endif // STML_TOKEN_H
