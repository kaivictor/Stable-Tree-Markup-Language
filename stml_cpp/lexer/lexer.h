#ifndef STML_LEXER_H
#define STML_LEXER_H

<<<<<<< Updated upstream
=======
<<<<<<< Updated upstream
>>>>>>> Stashed changes
/// \file lexer.h
/// Unified batch + streaming lexer for STML.

#include "lexer/token.h"
#include "diagnostics/error.h"
#include <functional>
#include <string>
#include <vector>

namespace stml {

// =========================================================================
// Lexer — unified batch + streaming STML lexer.
//
// Batch usage:
//   Lexer lexer;
//   auto tokens = lexer.tokenize(input);
//
// Streaming usage:
//   Lexer lexer;
//   lexer.on_token([](const Token& t) { ... });
//   lexer.feed(chunk1);
//   lexer.feed(chunk2);
//   lexer.finish();
// =========================================================================
class Lexer {
public:
    using TokenCallback = std::function<void(const Token&)>;

    Lexer();

    // ---- Batch mode ----
    /// Tokenize a complete input string and return all tokens.
    std::vector<Token> tokenize(const std::string& input);

    // ---- Streaming mode ----
    /// Set callback for each token emitted.
    void on_token(TokenCallback cb);

    /// Feed a chunk of input text.
    void feed(const std::string& data);

    /// Signal end of input. Flushes remaining content and emits DEDENT/END.
    void finish();

    // ---- Diagnostics ----
    const std::vector<Warning>& warnings() const { return warnings_; }

private:
    // =====================================================================
    // State
    // =====================================================================
    std::string buffer_;            // accumulated raw input not yet processed
    size_t pos_ = 0;               // current position in buffer_
    bool done_ = false;            // finish() has been called
    int current_line_ = 1;         // 1-based line number
    int current_column_ = 1;       // 1-based column within current line
    std::vector<int> indent_stack_;// stack of current indentation levels (0-based)
    int current_indent_ = 0;       // indent of the line currently being processed
    int pending_indent_ = -1;      // indent queued for the next logical line

    // Multiline text state
    bool in_multiline_ = false;
    std::string multiline_text_;
    std::string multiline_key_line_; // original key line for warning context
    int multiline_base_indent_ = 0;
    int multiline_line_ = 0;

    // Output
    TokenCallback token_cb_;
    std::vector<Token> batch_tokens_;  // used in batch mode
    std::vector<Warning> warnings_;

    // =====================================================================
    // Internal methods
    // =====================================================================
    void process_buffer();
    void process_line(std::string line);
    void tokenize_line_content(const std::string& content, int line_no, int indent);
    void parse_inline_list(const std::string& content, size_t start,
                           int line_no, size_t& out_end,
                           std::vector<InlineElem>& out_elems);
    int count_indent(const std::string& line);
    void flush_indents(int new_indent, int line_no);
    void emit(Token token);
    void emit_token(Token token);  // same as emit
    void emit_newline(int line_no);
    void emit_dedents_to(int target_indent, int line_no);

    // Quote helpers
    static size_t find_closing_quote(const std::string& s, size_t start);
    static size_t find_closing_quote_from_end(const std::string& s, size_t end_pos);
    static bool try_unquote(std::string& val);
    static void unescape_quoted(std::string& val, size_t start, size_t end);

    // Null detection
    static bool is_null_literal(const std::string& s);
<<<<<<< Updated upstream
=======
=======
#include "token.h"
#include "../diagnostics/error.h"
#include <string>
#include <vector>
#include <functional>

namespace stml {

class Lexer {
public:
    using TokenCallback = std::function<void(Token)>;

    Lexer();

    std::vector<Token> tokenize(const std::string& input);
    void feed(const std::string& data);
    void finish();
    void on_token(TokenCallback cb) { callback_ = std::move(cb); }
    const std::vector<Warning>& warnings() const { return warnings_; }

private:
    struct IndentEntry { int indent; int line_no; };

    std::string buffer_;
    size_t pos_ = 0;
    int current_line_ = 0;
    bool done_ = false;
    std::vector<IndentEntry> indent_stack_;
    std::vector<Warning> warnings_;
    TokenCallback callback_;

    // 多行文本状态
    bool in_multiline_ = false;
    std::string multiline_text_;
    int multiline_key_line_ = 0;
    int multiline_base_indent_ = 0;

    void process_line(const std::string& raw_line);
    int count_indent(const std::string& s);
    int current_indent() const;
    void flush_indents(int new_indent, int line_no, bool is_dash = false);
    void tokenize_line_content(const std::string& content, int indent, int line_no);
    void parse_inline_list(const std::string& text, int ln, int col, std::vector<InlineElem>& items);
    void emit(Token t);
    void emit_newline(int line_no);
>>>>>>> Stashed changes
>>>>>>> Stashed changes
};

} // namespace stml

#endif
