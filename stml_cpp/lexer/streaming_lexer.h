#pragma once

#include <string>
#include <vector>
#include <optional>
#include <tuple>

#include "lexer/token.h"
#include "diagnostics/error.h"

namespace stml {

// =========================================================================
// StreamingLexer — incremental STML lexer for streaming input.
//
// Usage:
//   StreamingLexer lexer;
//   while (chunk = get_next_chunk()) {
//       auto new_tokens = lexer.feed(chunk);
//       // forward new_tokens to StreamingParser
//   }
//   auto final_tokens = lexer.finalize();
//   auto warnings = lexer.warnings();
//
// The lexer buffers incomplete lines and multiline strings across chunks.
// =========================================================================
class StreamingLexer {
public:
    StreamingLexer();

    /// Feed a text chunk. Returns tokens emitted since last call.
    std::vector<Token> feed(const std::string& chunk);

    /// Signal end of input. Returns remaining tokens (DEDENTs + END).
    std::vector<Token> finalize();

    /// Accumulated warnings from all chunks.
    const std::vector<Warning>& warnings() const { return m_warnings; }

private:
    // ---- state ----
    std::string m_buffer;           // un-terminated text after last \n
    std::vector<std::string> m_lines; // accumulated complete lines
    int m_line_idx;                 // next line to process (0-based index into m_lines)
    std::vector<int> m_indent_stack;
    bool m_first_chunk;             // true until first feed

    // Multiline string state
    bool m_in_multiline;
    std::vector<std::string> m_multiline_parts;
    int m_multiline_key_indent;
    int m_multiline_start_line;     // 1-based line where '{' appeared
    int m_multiline_newline_line;   // line for deferred NEWLINE after MULTILINE_STRING
    int m_multiline_newline_col;    // column for deferred NEWLINE

    // Token emission
    std::vector<Token> m_tokens;
    size_t m_last_emitted;          // m_tokens.size() after previous feed()

    // Warnings
    std::vector<Warning> m_warnings;

    // Deferred DEDENT (for multiline close between lines)
    std::optional<int> m_deferred_dedent_to;

    // ---- internal line processing ----
    void _process_lines();
    void _process_one_line(int line_idx);

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

    // ---- bare key helpers ----
    static std::string _bare_key_from_content(const std::string& content);
    std::string _reconstruct_empty_key(const std::string& content);
};

} // namespace stml
