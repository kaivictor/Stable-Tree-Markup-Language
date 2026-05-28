#pragma once

#include <string>
#include <vector>
#include <optional>

#include "lexer/token.h"
#include "ast/ast.h"
#include "diagnostics/error.h"

namespace stml {

// =========================================================================
// STMLParser — recursive-descent parser operating on a flat token list.
//
// INDENT / DEDENT tokens drive block nesting.  The parser never computes
// indentation values — it trusts the lexer's INDENT/DEDENT pairing.
// =========================================================================
class STMLParser {
public:
    explicit STMLParser(std::vector<Token> tokens);

    /// Parse the full token stream.
    /// Returns (list of document AST nodes, warnings).
    /// Caller wraps in {"docs": [...]} for the public API shape.
    std::pair<AstList, std::vector<Warning>> parse();

private:
    std::vector<Token> m_tokens;
    int m_n;
    int m_pos;
    std::vector<Warning> m_warnings;

    // ---- token stream helpers ----
    std::optional<TokenType> _peek_type() const;
    std::optional<TokenType> _peek_type_at(int pos) const;
    void _skip_newlines();
    int  _skip_newlines_from(int pos) const;
    int  _consume_newline(int pos) const;

    // ---- value consumption ----
    std::pair<AstNode, int> _consume_value(int pos);

    // ---- document level ----
    std::pair<AstNode, int> _parse_document(int pos);

    // ---- node dispatch ----
    std::pair<AstNode, int> _parse_node(int pos);

    // ---- mapping ----
    std::pair<AstMap, int> _parse_mapping(int pos);

    // ---- sequence ----
    std::pair<AstList, int> _parse_sequence(int pos);
    std::pair<AstNode, int> _parse_sequence_item(int pos, bool complex_mode);
    bool _is_complex_sequence(int start_pos) const;
    /// Parse sibling keys in a multi-key sequence entry map.
    /// Reads INDENT(diff=2)+KEY/BARE_KEY patterns at content-indent level
    /// and appends to `map`. Returns updated position.
    int _parse_sibling_map_entries(AstMap& map, int pos);

    // ---- diagnostics ----
    void _add_warning_at(int pos, const std::string& message);
    [[noreturn]] void _add_error_at(int pos, const std::string& message);
};

} // namespace stml
