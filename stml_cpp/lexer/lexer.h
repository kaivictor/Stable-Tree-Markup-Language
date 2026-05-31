#pragma once

#include <string>
#include <vector>
#include <optional>
#include <tuple>

#include "lexer/token.h"
#include "diagnostics/error.h"

namespace stml {

// =========================================================================
// STMLLexer — converts STML text into a token stream.
//
// Single entry point: tokenize().
// All deterministic recovery rules live here so the resulting token stream
// is fully determined for any input.
// =========================================================================
class STMLLexer {
public:
    explicit STMLLexer(std::string text);

    /// Run the full lexer. Returns (tokens, warnings).
    std::pair<std::vector<Token>, std::vector<Warning>> tokenize();

private:
    // Input
    std::vector<std::string> m_lines;
    int m_line_count;

    // Accumulators
    std::vector<Token> m_tokens;
    std::vector<Warning> m_warnings;

    // Indentation stack (starts with implicit root at indent 0)
    std::vector<int> m_indent_stack;

    // Current position in m_lines
    int m_line_idx;

    // Deferred DEDENT target: set by multiline-string parsing.
    // The main loop emits DEDENTs at the correct position (between lines).
    std::optional<int> m_deferred_dedent_to;

    // ---- token factory ----
    void _add_token(TokenType type, TokenValue value, int line, int column);
    void _add_warning(int line, int column, const std::string& message);
    int  _col(int indent, int offset_in_content) const;

    // ---- indentation ----
    int  _calc_indent(const std::string& line) const;
    bool _is_blank_or_comment(const std::string& content) const;
    void _process_indent(int indent);
    void _emit_dedents_to(int target);

    // ---- line dispatch ----
    void _lex_sequence_line(const std::string& content, int indent);
    void _lex_mapping_line(const std::string& content, int indent);

    // ---- value parsing ----
    void _lex_remainder_after_colon(const std::string& rest, int indent, int line_no);
    void _lex_value_or_key_on_line(const std::string& text, int indent, bool scan_colon);
    void _lex_scalar_or_null(const std::string& text, int indent);

    // ---- quoted key ----
    std::tuple<std::string, std::string, bool, int>
    _try_quoted_key(const std::string& content, int indent);

    // ---- structural colon ----
    int _find_unquoted_colon(const std::string& s) const;

    // ---- quote matching ----
    int _find_first_unescaped_quote(const std::string& s, int start) const;
    int _find_last_unescaped_quote(const std::string& s, int start) const;

    // ---- escape processing ----
    std::string _unescape(const std::string& s, int col_offset = 0);

    // ---- quoted value (greedy) ----
    std::tuple<std::string, int, bool> _parse_quoted_value(const std::string& s, int start, int indent);

    // ---- inline list ----
    std::tuple<std::vector<InlineElem>, bool, int> _parse_inline_list(const std::string& s, int indent);

    // ---- multiline ----
    std::tuple<std::string, int, int> _read_multiline(int key_indent);

    // ---- bare key helpers ----
    static std::string _bare_key_from_content(const std::string& content);
    std::string _reconstruct_empty_key(const std::string& content);
};

} // namespace stml
