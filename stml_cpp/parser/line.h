#pragma once

#include "ast/ast.h"
#include <string>
#include <vector>

namespace stml {

// =========================================================================
// Line — a single logical line in the intermediate line-tree representation.
//
// The line tree is built from the token stream before AST construction.
// Each line represents one key-value pair, bare key, or dash entry.
// Children are the indented lines that logically belong to this line.
// =========================================================================
struct Line {
    enum class Kind : uint8_t {
        DASH_EMPTY,     // "-" with no content (null entry in a sequence)
        DASH_SCALAR,    // "- value" (scalar entry in a sequence)
        DASH_KEY_VAL,   // "- key: value" (mapping entry in a sequence)
        KEY_VAL,        // "key: value" (standard mapping entry)
        BARE_KEY,       // "key" (bare key without value, implicitly null)
    };

    Kind kind;
    std::string key;            // empty for DASH_EMPTY/DASH_SCALAR items
    AstNode inline_value;       // the inline scalar/null/list value
    int indent;                 // 0-based indentation level
    int line_no;                // 1-based source line number
    std::vector<Line> children; // indented child lines

    Line() : kind(Kind::BARE_KEY), indent(0), line_no(0) {}

    Line(Kind k, std::string kkey, AstNode val, int ind, int ln)
        : kind(k), key(std::move(kkey)), inline_value(std::move(val)),
          indent(ind), line_no(ln) {}

    /// True when this line is a dash-prefixed sequence entry.
    bool is_dash() const noexcept {
        return kind == Kind::DASH_EMPTY || kind == Kind::DASH_SCALAR
            || kind == Kind::DASH_KEY_VAL;
    }
};

} // namespace stml
