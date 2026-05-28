#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>
#include <optional>

namespace stml {

// =========================================================================
// TokenType — the 14 token types produced by the lexer.
// =========================================================================
enum class TokenType : uint8_t {
    // Structural
    NEWLINE,          // line break
    INDENT,           // increase in indentation
    DEDENT,           // decrease in indentation
    DOC_SEPARATOR,    // --- at indent 0
    END,              // end of token stream (EOF)

    // Mapping
    KEY,              // normal key (already unescaped)
    BARE_KEY,         // line without structural colon; value defaults to null
    COLON,            // the ':' separator

    // Values
    SCALAR,           // scalar string (already unescaped)
    NULL_,            // null / ~ / empty container
    RAW_STRING,       // degraded raw string (unclosed quote, malformed list, etc.)

    // Sequence
    DASH,             // sequence entry prefix: '- '

    // Special
    INLINE_LIST,      // inline list [elem1, elem2, ...]
    MULTILINE_STRING, // multiline text block { ... }
};

// =========================================================================
// InlineElem — an element of an inline list.
// nullopt represents a null element.
// =========================================================================
using InlineElem = std::optional<std::string>;

// =========================================================================
// TokenValue — the payload of a token.
//   monostate:    structural tokens (NEWLINE, INDENT, DEDENT, COLON, DASH, END)
//   string:       SCALAR, KEY, BARE_KEY, RAW_STRING, MULTILINE_STRING
//   vector:       INLINE_LIST (already parsed elements)
// =========================================================================
using TokenValue = std::variant<
    std::monostate,
    std::string,
    std::vector<InlineElem>
>;

// =========================================================================
// Token — a single lexical token with source location.
// =========================================================================
struct Token {
    TokenType type;
    TokenValue value;
    int line;       // 1-based
    int column;     // 1-based (token start position on line)

    Token(TokenType type, TokenValue value, int line, int column)
        : type(type), value(std::move(value)), line(line), column(column) {}
};

} // namespace stml
