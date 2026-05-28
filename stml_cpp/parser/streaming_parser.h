#pragma once

#include <string>
#include <vector>
#include <set>
#include <optional>

#include "lexer/token.h"
#include "ast/ast.h"
#include "diagnostics/error.h"

namespace stml {

// =========================================================================
// StreamingParser — incremental STML parser for streaming input.
//
// Receives tokens incrementally via feed() and parses as much as possible.
// finalize() completes parsing and returns the document AST map.
//
// Token ordering must match the batch lexer output exactly.
// =========================================================================
class StreamingParser {
public:
    StreamingParser();

    /// Feed new tokens. Parses as much as possible from the accumulated buffer.
    void feed(std::vector<Token> tokens);

    /// Complete parsing. Returns a list of document AST nodes.
    /// Caller wraps in {"docs": [...]} for the public API shape.
    AstList finalize();

    /// Accumulated warnings from all parse operations.
    const std::vector<Warning>& warnings() const { return m_warnings; }

private:
    // Token buffer
    std::vector<Token> m_buffer;
    int m_pos;                      // consumed position in m_buffer
    // Accumulated documents
    AstList m_docs;

    // Warnings
    std::vector<Warning> m_warnings;

    // Deferred complex-sequence decision
    struct DeferredSeq {
        int start_pos;              // where the sequence starts (in m_buffer)
        int scan_pos;               // where to resume scanning
    };
    std::optional<DeferredSeq> m_deferred_seq;

    // Value token types
    static const std::set<TokenType> VALUE_TOKENS;

    // ---- incremental parsing ----
    void _try_parse();

    // ---- token stream helpers ----
    std::optional<TokenType> _peek_type_at(int pos) const;
    int _skip_newlines_from(int pos) const;
    int _consume_newline(int pos) const;

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

    /// Incremental complex-sequence scan.
    /// Returns: true=complex, false=simple, nullopt=need more tokens.
    std::optional<bool> _is_complex_sequence(int start_pos) const;

    /// Parse sibling keys in a multi-key sequence entry map (incremental).
    int _parse_sibling_map_entries(AstMap& map, int pos);

    /// Check whether the buffer ends with tokens that need more context
    /// (e.g. DASH+NEWLINE without next token, or KEY:COLON at buffer end).
    /// Returns true if parsing should be deferred.
    bool _buffer_incomplete() const;

    // ---- diagnostics ----
    void _add_warning_at(int pos, const std::string& message);
    [[noreturn]] void _add_error_at(int pos, const std::string& message);
};

} // namespace stml
