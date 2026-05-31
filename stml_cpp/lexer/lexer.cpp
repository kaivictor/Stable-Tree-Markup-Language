<<<<<<< Updated upstream
#include "lexer/lexer.h"
#include <algorithm>
=======
#include "lexer.h"
>>>>>>> Stashed changes
#include <cctype>

namespace stml {

<<<<<<< Updated upstream
// =========================================================================
// Constructor
// =========================================================================

Lexer::Lexer() {
    indent_stack_.push_back(0);
}

// =========================================================================
// Batch mode
// =========================================================================

std::vector<Token> Lexer::tokenize(const std::string& input) {
    batch_tokens_.clear();
    warnings_.clear();
    buffer_.clear();
    pos_ = 0;
    done_ = false;
    current_line_ = 1;
    current_column_ = 1;
    indent_stack_.clear();
    indent_stack_.push_back(0);
    current_indent_ = 0;
    pending_indent_ = -1;
    in_multiline_ = false;
    multiline_text_.clear();

    // Collect tokens via batch callback
    token_cb_ = [this](const Token& t) {
        batch_tokens_.push_back(t);
    };

    feed(input);
    finish();

    return batch_tokens_;
}

// =========================================================================
// Streaming mode
// =========================================================================

void Lexer::on_token(TokenCallback cb) {
    token_cb_ = std::move(cb);
}

void Lexer::feed(const std::string& data) {
    if (done_) return;
    buffer_ += data;
    process_buffer();
}

void Lexer::finish() {
    if (done_) return;
    done_ = true;

    // If in multiline mode, close it
    if (in_multiline_) {
        emit(Token(TokenType::MULTILINE_STRING, multiline_text_,
                    multiline_line_, 1));
        in_multiline_ = false;
        multiline_text_.clear();
    }

    // Process any remaining content in buffer
    if (!buffer_.empty()) {
        // Treat remaining as a line (may not end with \n)
        process_line(buffer_);
        buffer_.clear();
    }

    // Flush remaining indent stack
    emit_dedents_to(0, current_line_);

    // Emit END token
    emit_newline(current_line_);
    emit(Token(TokenType::END, current_line_, 1));
}

// =========================================================================
// Buffer processing: split into lines and process each
// =========================================================================

void Lexer::process_buffer() {
    while (!buffer_.empty()) {
        // Find the next line ending
        size_t nl_pos = buffer_.find('\n', pos_);

        if (nl_pos == std::string::npos) {
            // No complete line yet; wait for more input
            // But if done_, process everything
            if (done_) {
                std::string line = buffer_;
                buffer_.clear();
                pos_ = 0;
                process_line(line);
            }
            break;
        }

        // Extract line including the \n
        std::string line = buffer_.substr(pos_, nl_pos - pos_ + 1);
        pos_ = 0;
        buffer_.erase(0, nl_pos + 1);

        // Strip trailing \r\n or \n
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        process_line(line);
    }
}

// =========================================================================
// Count leading spaces (spaces only count as indent)
// =========================================================================

int Lexer::count_indent(const std::string& line) {
    int count = 0;
    for (char c : line) {
        if (c == ' ') {
            ++count;
        } else {
            break;
        }
    }
    return count;
}

// =========================================================================
// Process a single line
// =========================================================================

void Lexer::process_line(std::string line) {
    int line_no = current_line_;
    ++current_line_;

    // Empty or whitespace-only lines
    // In multiline mode, empty lines are part of the multiline content
    size_t first_non_space = line.find_first_not_of(' ');
    if (first_non_space == std::string::npos) {
        if (in_multiline_) {
            multiline_text_ += '\n'; // preserve empty line in multiline
        }
        // Reset pending indent on blank lines
        pending_indent_ = -1;
        return;
    }

    // Count indentation
    int indent = count_indent(line);
    std::string content = line.substr(indent);

    // Strip trailing spaces from content (but not for multiline)
    if (!in_multiline_) {
        while (!content.empty() && content.back() == ' ') {
            content.pop_back();
        }
    }

    // Handle comments: # at the start of content (not inside multiline)
    if (!in_multiline_ && !content.empty() && content[0] == '#') {
        pending_indent_ = -1;
        return;
    }

    // Handle document separator: ---
    if (!in_multiline_ && content == "---") {
        flush_indents(0, line_no);
        emit_newline(line_no);
        emit(Token(TokenType::DOC_SEPARATOR, line_no, indent + 1));
        pending_indent_ = -1;
        return;
    }

    // Multiline text handling
    if (in_multiline_) {
        // Check if this line closes the multiline block
        // Closing: a '}' at indent level <= multiline_base_indent_
        // But to avoid closing on nested '}', we check if the line
        // consists solely of '}' (possibly with leading spaces)
        std::string trimmed = content;
        // Remove leading spaces (already done by content = line.substr(indent))
        // Check if content is exactly "}" or starts with "}" and the rest is whitespace/comments
        if (!content.empty() && content[0] == '}' && indent <= multiline_base_indent_) {
            // This line closes the multiline
            // The '}' itself is not part of the text
            // But check if there's text after } on the same line
            size_t close_brace = 0;
            // If after '}' there's only whitespace or nothing
            bool only_whitespace_after = true;
            for (size_t j = 1; j < content.size(); ++j) {
                if (content[j] != ' ' && content[j] != '\t') {
                    only_whitespace_after = false;
                    break;
                }
            }
            if (only_whitespace_after) {
                // Check nested braces: if the multiline text has unclosed {,
                // then this } might close the nested one, not the outer one.
                // Count braces in multiline_text_
                int brace_depth = 0;
                for (char c : multiline_text_) {
                    if (c == '{') ++brace_depth;
                    else if (c == '}') --brace_depth;
                }
                if (brace_depth <= 0) {
                    // Not a nested brace — this closes the multiline
                    // Remove trailing newline from multiline text if present
                    if (!multiline_text_.empty() && multiline_text_.back() == '\n') {
                        multiline_text_.pop_back();
                    }
                    emit(Token(TokenType::MULTILINE_STRING, multiline_text_,
                                multiline_line_, 1));
                    in_multiline_ = false;
                    multiline_text_.clear();
                    flush_indents(indent, line_no);
                    emit_newline(line_no);
                    pending_indent_ = -1;
                    return;
                }
            }
        }

        // Append this line to multiline text
        multiline_text_ += line;
        multiline_text_ += '\n';
        return;
    }

    // Not in multiline — process normal line
    int effective_indent = indent;

    // Handle pending indent from the previous line
    if (pending_indent_ >= 0) {
        effective_indent = pending_indent_;
        pending_indent_ = -1;
    }

    flush_indents(effective_indent, line_no);
    tokenize_line_content(content, line_no, indent);
}

// =========================================================================
// Indent management
// =========================================================================

void Lexer::flush_indents(int new_indent, int line_no) {
    if (indent_stack_.empty()) {
        indent_stack_.push_back(0);
    }

    // Wait — if we're at the same line with existing indents but the stack was
    // reset for a new document, this is fine.
    int top = indent_stack_.back();

    if (new_indent > top) {
        // Increased indent
        indent_stack_.push_back(new_indent);
        current_indent_ = new_indent;
        emit_newline(line_no);
        Token itok(TokenType::INDENT, line_no, 1);
        itok.indent = new_indent;
        emit(itok);
    } else if (new_indent < top) {
        // Decreased indent — pop to matching level
        emit_dedents_to(new_indent, line_no);
    }

    // If equal, nothing to do
}

void Lexer::emit_dedents_to(int target_indent, int line_no) {
    while (!indent_stack_.empty() && indent_stack_.back() > target_indent) {
        indent_stack_.pop_back();
        // Emit newline before DEDENT (logical line end)
        emit_newline(line_no);
        // DEDENT tokens always have line as line_no, column as 1
        Token dtok(TokenType::DEDENT, line_no, 1);
        dtok.indent = target_indent;
        emit(dtok);
    }
    current_indent_ = target_indent;

    // Ensure stack has at least the 0 level
    if (indent_stack_.empty()) {
        indent_stack_.push_back(0);
    }
}

void Lexer::emit_newline(int line_no) {
    emit(Token(TokenType::NEWLINE, line_no, 1));
}

// =========================================================================
// Line content tokenization
// =========================================================================

void Lexer::tokenize_line_content(const std::string& content, int line_no, int indent) {
    if (content.empty()) {
        emit_newline(line_no);
        return;
    }

    std::string working = content;
    bool has_dash = false;

    // Check for DASH (- )
    if (working.size() >= 1 && working[0] == '-') {
        if (working.size() == 1) {
            // Single "-" → DASH with null entry
            emit_newline(line_no);
            emit(Token(TokenType::DASH, line_no, indent + 1));
            return;
        }
        if (working[1] == ' ') {
            // "- something"
            emit_newline(line_no);
            emit(Token(TokenType::DASH, line_no, indent + 1));
            working = working.substr(2); // skip "- "
            has_dash = true;

            // Trim leading spaces
            while (!working.empty() && working[0] == ' ') {
                working.erase(0, 1);
            }
            if (working.empty()) {
                // "- " with nothing after → DASH_EMPTY
                emit_newline(line_no);
                return;
            }
        }
    }

    // Check for multiline start: content is just "{"
    // (only when not after dash, or after dash with no colon)
    if (working == "{" && !has_dash) {
        in_multiline_ = true;
        multiline_text_.clear();
        multiline_base_indent_ = indent;
        multiline_line_ = line_no;
        return;
    }

    // Check for multi-line after colon: "key: {" or "- key: {"
    // First, find colon, then check for "{"
    size_t colon = std::string::npos;

    // Find first unquoted colon
    bool in_quote_s = false;
    bool in_quote_d = false;
    for (size_t i = 0; i < working.size(); ++i) {
        char c = working[i];
        if (c == '\\' && (in_quote_s || in_quote_d) && i + 1 < working.size()) {
            ++i; // skip escaped char
            continue;
        }
        if (c == '\'' && !in_quote_d) {
            in_quote_s = !in_quote_s;
            continue;
        }
        if (c == '"' && !in_quote_s) {
            in_quote_d = !in_quote_d;
            continue;
        }
        if (c == ':' && !in_quote_s && !in_quote_d) {
            colon = i;
            break;
        }
    }

    // Special case: if colon is at position 0, the whole content is BARE_KEY
    // (colon at content start is NOT key:value)
    if (colon == 0) {
        emit_newline(line_no);
        emit(Token(TokenType::BARE_KEY, working, line_no, indent + 1));
        return;
    }

    // If no colon found, it's a BARE_KEY
    if (colon == std::string::npos) {
        // But if after DASH, check for null/inline list/scalar
        if (has_dash) {
            // After dash without colon
            if (is_null_literal(working)) {
                emit(Token(TokenType::NULL_, line_no, indent + 1));
                emit_newline(line_no);
                return;
            }
            if (!working.empty() && working[0] == '[') {
                // Inline list after dash
                std::vector<InlineElem> elems;
                size_t end_pos;
                parse_inline_list(working, 0, line_no, end_pos, elems);
                emit(Token(TokenType::INLINE_LIST, std::move(elems), line_no, indent + 1));
                emit_newline(line_no);
                return;
            }
            // Try to unquote if it's quoted
            std::string val = working;
            if (try_unquote(val)) {
                emit(Token(TokenType::SCALAR, val, line_no, indent + 1));
            } else {
                emit(Token(TokenType::SCALAR, working, line_no, indent + 1));
            }
            emit_newline(line_no);
            return;
        }

        // Not after dash, no colon → BARE_KEY
        // Check if it's a standalone quoted string that could be unquoted
        std::string key = working;
        // For BARE_KEY, try unquoting only if it's fully quoted
        bool was_quoted = false;
        if (key.size() >= 2 && key.front() == '"' && key.back() == '"') {
            key = key.substr(1, key.size() - 2);
            unescape_quoted(key, 1, key.size() + 1);
            was_quoted = true;
        } else if (key.size() >= 2 && key.front() == '\'' && key.back() == '\'') {
            key = key.substr(1, key.size() - 2);
            was_quoted = true;
        }

        emit_newline(line_no);
        emit(Token(TokenType::BARE_KEY, key, line_no, indent + 1));
        return;
    }

    // We have a colon at position > 0 → key: value
    std::string key = working.substr(0, colon);
    std::string value_part = working.substr(colon + 1);

    // Process key: trim trailing whitespace (for unquoted keys)
    // For quoted keys, the unquoting handles it
    bool key_was_quoted = false;
    if (key.size() >= 2 && key.front() == '"' && key.back() == '"') {
        key = key.substr(1, key.size() - 2);
        unescape_quoted(key, 1, key.size() + 1);
        key_was_quoted = true;
    } else if (key.size() >= 2 && key.front() == '\'' && key.back() == '\'') {
        key = key.substr(1, key.size() - 2);
        key_was_quoted = true;
    } else {
        // Unquoted key: trim trailing spaces
        while (!key.empty() && key.back() == ' ') {
            key.pop_back();
        }
    }

    // Strip exactly ONE whitespace char (space or tab) after colon
    if (!value_part.empty() && (value_part[0] == ' ' || value_part[0] == '\t')) {
        value_part.erase(0, 1);
    }

    emit_newline(line_no);
    emit(Token(TokenType::KEY, key, line_no, indent + 1));
    emit(Token(TokenType::COLON, line_no, indent + 1 + static_cast<int>(colon) + 1));

    // Process value
    if (value_part.empty()) {
        // key:  (no value) — value is implicitly null
        // But wait for multiline start "{" after colon
        // "key: {"  — the "{" triggers multiline mode
        // "key: " — the empty value is null
        emit_newline(line_no);
        return;
    }

    // Check for multiline start after colon: key: {
    if (value_part == "{") {
        in_multiline_ = true;
        multiline_text_.clear();
        multiline_base_indent_ = indent;
        multiline_line_ = line_no;
        return;
    }

    // Check for null
    if (is_null_literal(value_part)) {
        emit(Token(TokenType::NULL_, line_no,
                    indent + 1 + static_cast<int>(colon) + 2));
        emit_newline(line_no);
        return;
    }

    // Check for inline list
    if (!value_part.empty() && value_part[0] == '[') {
        std::vector<InlineElem> elems;
        size_t end_pos;
        parse_inline_list(value_part, 0, line_no, end_pos, elems);
        emit(Token(TokenType::INLINE_LIST, std::move(elems), line_no,
                    indent + 1 + static_cast<int>(colon) + 2));
        emit_newline(line_no);
        return;
    }

    // Scalar value: try to unquote
    std::string val = value_part;
    bool was_quoted = try_unquote(val);
    if (was_quoted) {
        // Quoted empty string → SCALAR with empty value (not null!)
        emit(Token(TokenType::SCALAR, val, line_no,
                    indent + 1 + static_cast<int>(colon) + 2));
    } else {
        emit(Token(TokenType::SCALAR, value_part, line_no,
                    indent + 1 + static_cast<int>(colon) + 2));
    }
    emit_newline(line_no);
}

// =========================================================================
// Inline list parsing: [a, b, "c", null, ~]
// =========================================================================

void Lexer::parse_inline_list(const std::string& content, size_t start,
                               int line_no, size_t& out_end,
                               std::vector<InlineElem>& out_elems) {
    out_elems.clear();

    if (start >= content.size() || content[start] != '[') {
        out_end = start;
        return;
    }

    size_t i = start + 1; // skip '['

    while (i < content.size()) {
        // Skip whitespace
        while (i < content.size() && (content[i] == ' ' || content[i] == '\t')) {
            ++i;
        }

        if (i >= content.size()) {
            // Unclosed bracket — recovery
            warnings_.emplace_back(line_no, static_cast<int>(start) + 1,
                                    "Unclosed inline list bracket '['");
            out_end = i;
            return;
        }

        char c = content[i];

        // Closing bracket
        if (c == ']') {
            ++i;
            out_end = i;
            return;
        }

        // Comma separator
        if (c == ',') {
            // Check for trailing comma: [a,] → append null
            size_t next = i + 1;
            while (next < content.size() && (content[next] == ' ' || content[next] == '\t')) {
                ++next;
            }
            if (next < content.size() && content[next] == ']') {
                // Trailing comma
                out_elems.emplace_back(); // NULL_VAL
                i = next + 1; // skip ']'
                out_end = i;
                return;
            }
            // Leading comma: [,a] → emit null first
            if (out_elems.empty()) {
                out_elems.emplace_back(); // NULL_VAL
            }
            ++i;
            continue;
        }

        // Quoted string
        if (c == '"' || c == '\'') {
            char quote = c;
            size_t elem_start = i;
            ++i;

            while (i < content.size()) {
                if (content[i] == '\\' && i + 1 < content.size()) {
                    i += 2;
                    continue;
                }
                if (content[i] == quote) {
                    break;
                }
                ++i;
            }

            if (i >= content.size()) {
                // Unclosed quote in inline list
                warnings_.emplace_back(line_no, static_cast<int>(elem_start) + 1,
                                        "Unclosed quote in inline list");
                // Treat rest as raw string
                std::string raw = content.substr(elem_start);
                out_elems.emplace_back(raw);
                out_end = content.size();
                return;
            }

            // Extract quoted content
            std::string elem_val = content.substr(elem_start + 1, i - elem_start - 1);
            unescape_quoted(elem_val, elem_start + 1, i);

            // Quoted empty string "" → SCALAR (not null!)
            out_elems.emplace_back(elem_val);
            ++i; // skip closing quote
            continue;
        }

        // Null literal
        if (c == '~') {
            // Check that '~' is followed by comma, ']', or whitespace
            size_t check = i + 1;
            while (check < content.size() && (content[check] == ' ' || content[check] == '\t')) {
                ++check;
            }
            if (check >= content.size() || content[check] == ',' || content[check] == ']') {
                out_elems.emplace_back(); // NULL_VAL
                i = check;
                continue;
            }
            // Otherwise '~' is part of a scalar
        }

        // Check for null keyword
        if (i + 3 < content.size() &&
            (content.substr(i, 4) == "null" || content.substr(i, 4) == "NULL" ||
             content.substr(i, 4) == "Null")) {
            size_t check = i + 4;
            while (check < content.size() && (content[check] == ' ' || content[check] == '\t')) {
                ++check;
            }
            if (check >= content.size() || content[check] == ',' || content[check] == ']') {
                out_elems.emplace_back(); // NULL_VAL
                i = check;
                continue;
            }
        }

        // Unquoted scalar: read until comma, ']', or end
        size_t scalar_start = i;
        while (i < content.size()) {
            c = content[i];
            if (c == ',' || c == ']') break;
            if (c == '"' || c == '\'') break; // quote in unquoted → problem, break
<<<<<<< Updated upstream
            ++i;
        }

        std::string scalar = content.substr(scalar_start, i - scalar_start);
        // Trim trailing whitespace
        while (!scalar.empty() && (scalar.back() == ' ' || scalar.back() == '\t')) {
            scalar.pop_back();
        }

        if (scalar.empty()) {
            // Unquoted empty → NULL
            out_elems.emplace_back(); // NULL_VAL
        } else {
            out_elems.emplace_back(scalar);
        }
    }

    // If we get here, bracket was never closed
    warnings_.emplace_back(line_no, static_cast<int>(start) + 1,
                            "Unclosed inline list bracket '['");
    out_end = content.size();
}

// =========================================================================
// Quote helpers
// =========================================================================

size_t Lexer::find_closing_quote(const std::string& s, size_t start) {
    if (start >= s.size()) return std::string::npos;
    char quote = s[start];
    if (quote != '"' && quote != '\'') return std::string::npos;

    for (size_t i = start + 1; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i; // skip escaped char
            continue;
        }
        if (s[i] == quote) {
            return i;
        }
    }
    return std::string::npos; // unclosed
}

size_t Lexer::find_closing_quote_from_end(const std::string& s, size_t end_pos) {
    if (end_pos == 0 || end_pos > s.size()) return std::string::npos;
    // Work backwards: find the quote char that opens the string ending at end_pos
    char quote = s[end_pos - 1];
    if (quote != '"' && quote != '\'') return std::string::npos;

    // Search backwards from end_pos-2 for the matching open quote
    // Handles escaped quotes
    for (size_t i = end_pos - 2; i > 0; --i) {
        if (s[i] == '\\') {
            --i; // skip potential escaped quote
            if (i == 0) break;
            continue;
        }
        if (s[i] == quote) {
            return i;
        }
    }
    // Check position 0
    if (s[0] == quote) return 0;
    return std::string::npos;
}

bool Lexer::try_unquote(std::string& val) {
    if (val.size() < 2) return false;

    char first = val.front();
    char last = val.back();

    if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
        // Only unquote if the string is entirely enclosed
        // Check that the closing quote isn't escaped
        size_t close_pos = find_closing_quote(val, 0);
        if (close_pos != val.size() - 1) {
            // There's content after the quote or the quote is escaped
            return false;
        }
        val = val.substr(1, val.size() - 2);
        unescape_quoted(val, 1, val.size() + 1);
        return true;
    }
    return false;
}

void Lexer::unescape_quoted(std::string& val, size_t /*start*/, size_t /*end*/) {
    std::string result;
    result.reserve(val.size());

    for (size_t i = 0; i < val.size(); ++i) {
        if (val[i] == '\\' && i + 1 < val.size()) {
            char next = val[i + 1];
            switch (next) {
                case '"':  result += '"';  ++i; break;
                case '\\': result += '\\'; ++i; break;
                case 'n':  result += '\n'; ++i; break;
                case 't':  result += '\t'; ++i; break;
                case 'r':  result += '\r'; ++i; break;
                case 'b':  result += '\b'; ++i; break;
                case 'f':  result += '\f'; ++i; break;
                case '\'': result += '\''; ++i; break;
                default:
                    // Unknown escape: keep as-is
                    result += '\\';
                    break;
            }
        } else {
            result += val[i];
        }
    }

    val = std::move(result);
}

// =========================================================================
// Null detection
// =========================================================================

bool Lexer::is_null_literal(const std::string& s) {
    return s == "null" || s == "NULL" || s == "Null" || s == "~";
}

// =========================================================================
// Token emission
// =========================================================================

void Lexer::emit(Token token) {
    if (token_cb_) {
        token_cb_(token);
    }
}

void Lexer::emit_token(Token token) {
    emit(std::move(token));
=======
            ++i;
        }

        std::string scalar = content.substr(scalar_start, i - scalar_start);
        // Trim trailing whitespace
        while (!scalar.empty() && (scalar.back() == ' ' || scalar.back() == '\t')) {
            scalar.pop_back();
        }

        if (scalar.empty()) {
            // Unquoted empty → NULL
            out_elems.emplace_back(); // NULL_VAL
        } else {
            out_elems.emplace_back(scalar);
        }
    }

    // If we get here, bracket was never closed
    warnings_.emplace_back(line_no, static_cast<int>(start) + 1,
                            "Unclosed inline list bracket '['");
    out_end = content.size();
}

// =========================================================================
// Quote helpers
// =========================================================================

size_t Lexer::find_closing_quote(const std::string& s, size_t start) {
    if (start >= s.size()) return std::string::npos;
    char quote = s[start];
    if (quote != '"' && quote != '\'') return std::string::npos;

    for (size_t i = start + 1; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i; // skip escaped char
            continue;
        }
        if (s[i] == quote) {
            return i;
        }
    }
    return std::string::npos; // unclosed
}

size_t Lexer::find_closing_quote_from_end(const std::string& s, size_t end_pos) {
    if (end_pos == 0 || end_pos > s.size()) return std::string::npos;
    // Work backwards: find the quote char that opens the string ending at end_pos
    char quote = s[end_pos - 1];
    if (quote != '"' && quote != '\'') return std::string::npos;

    // Search backwards from end_pos-2 for the matching open quote
    // Handles escaped quotes
    for (size_t i = end_pos - 2; i > 0; --i) {
        if (s[i] == '\\') {
            --i; // skip potential escaped quote
            if (i == 0) break;
            continue;
        }
        if (s[i] == quote) {
            return i;
        }
    }
    // Check position 0
    if (s[0] == quote) return 0;
    return std::string::npos;
}

bool Lexer::try_unquote(std::string& val) {
    if (val.size() < 2) return false;

    char first = val.front();
    char last = val.back();

    if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
        // Only unquote if the string is entirely enclosed
        // Check that the closing quote isn't escaped
        size_t close_pos = find_closing_quote(val, 0);
        if (close_pos != val.size() - 1) {
            // There's content after the quote or the quote is escaped
            return false;
        }
        val = val.substr(1, val.size() - 2);
        unescape_quoted(val, 1, val.size() + 1);
        return true;
    }
    return false;
}

void Lexer::unescape_quoted(std::string& val, size_t /*start*/, size_t /*end*/) {
    std::string result;
    result.reserve(val.size());

    for (size_t i = 0; i < val.size(); ++i) {
        if (val[i] == '\\' && i + 1 < val.size()) {
            char next = val[i + 1];
            switch (next) {
                case '"':  result += '"';  ++i; break;
                case '\\': result += '\\'; ++i; break;
                case 'n':  result += '\n'; ++i; break;
                case 't':  result += '\t'; ++i; break;
                case 'r':  result += '\r'; ++i; break;
                case 'b':  result += '\b'; ++i; break;
                case 'f':  result += '\f'; ++i; break;
                case '\'': result += '\''; ++i; break;
                default:
                    // Unknown escape: keep as-is
                    result += '\\';
                    break;
            }
        } else {
            result += val[i];
        }
    }

    val = std::move(result);
}

// =========================================================================
// Null detection
// =========================================================================

bool Lexer::is_null_literal(const std::string& s) {
    return s == "null" || s == "NULL" || s == "Null" || s == "~";
}

// =========================================================================
// Token emission
// =========================================================================

void Lexer::emit(Token token) {
    if (token_cb_) {
        token_cb_(token);
=======
Lexer::Lexer() {
    indent_stack_.push_back({0, 0});
}

std::vector<Token> Lexer::tokenize(const std::string& input) {
    std::vector<Token> tokens;
    auto old_cb = callback_;
    on_token([&](Token t) { tokens.push_back(std::move(t)); });
    feed(input);
    finish();
    on_token(old_cb);
    return tokens;
}

void Lexer::feed(const std::string& data) {
    if (done_) return;
    buffer_ += data;

    while (pos_ < buffer_.size()) {
        size_t end = pos_;
        while (end < buffer_.size() && buffer_[end] != '\n' && buffer_[end] != '\r') ++end;
        if (end >= buffer_.size()) return; // 不完整行

        std::string line = buffer_.substr(pos_, end - pos_);
        pos_ = end;
        if (pos_ < buffer_.size() && buffer_[pos_] == '\r') ++pos_;
        if (pos_ < buffer_.size() && buffer_[pos_] == '\n') ++pos_;

        process_line(line);
    }

    if (pos_ > 0) { buffer_.erase(0, pos_); pos_ = 0; }
}

void Lexer::finish() {
    if (done_) return;
    done_ = true;

    // 最后不完整行
    if (pos_ < buffer_.size()) {
        std::string line = buffer_.substr(pos_);
        process_line(line);
    }

    // 未闭合的多行文本
    if (in_multiline_) {
        emit(Token::make_with_val(TokenType::SCALAR, multiline_text_,
                                   multiline_key_line_, 1));
        in_multiline_ = false;
        emit_newline(current_line_ + 1);
    }

    flush_indents(-1, current_line_ + 1);
    emit(Token::make(TokenType::END, current_line_ + 1, 1));
}

// ============================================================
// 行处理（支持多行文本 { ... }）
// ============================================================
void Lexer::process_line(const std::string& raw_line) {
    ++current_line_;

    if (in_multiline_) {
        // 多行文本模式：保留原始行内容
        int indent = count_indent(raw_line);
        std::string content = raw_line.substr(indent);

        if (content == "}" && indent <= multiline_base_indent_) {
            // 闭合多行文本（} 的缩进 ≤ 键的缩进时闭合）
            emit(Token::make_with_val(TokenType::SCALAR, multiline_text_,
                                       multiline_key_line_, 1));
            in_multiline_ = false;
            // 闭合后发出 NEWLINE，让 Parser 知道这一行已结束
            emit_newline(current_line_);
        } else {
            // 累积文本行
            if (!multiline_text_.empty()) multiline_text_ += "\n";
            multiline_text_ += raw_line;
        }
        return;
    }

    // 空行 → 丢弃
    if (raw_line.empty()) return;

    int indent = count_indent(raw_line);
    std::string content = raw_line.substr(indent);

    // 注释 → 丢弃
    if (!content.empty() && content[0] == '#') return;

    // "---" 文档分隔符（仅 indent=0）
    if (indent == 0 && content == "---") {
        flush_indents(0, current_line_);
        emit(Token::make(TokenType::DOC_SEPARATOR, current_line_, 1));
        emit_newline(current_line_);
        return;
    }

    // 检测是否为序列条目行（以 - 开头），用于缩进容错
    bool is_dash_line = false;
    if (!content.empty() && content[0] == '-') {
        size_t next = 1;
        while (next < content.size() && content[next] == ' ') ++next;
        // 负号后面跟数字的不是序列条目
        is_dash_line = !(next < content.size() && std::isdigit(static_cast<unsigned char>(content[next])));
    }

    // 缩进变化
    flush_indents(indent, current_line_, is_dash_line);

    // 词法分析行内容
    if (!content.empty()) {
        tokenize_line_content(content, indent, current_line_);
    }

    // 如果刚进入多行文本模式，暂不发出 NEWLINE，因为值还未到达
    // (SCALAR token 会在 } 闭合时发出，届时再发 NEWLINE)
    if (!in_multiline_) {
        emit_newline(current_line_);
    }
}

int Lexer::count_indent(const std::string& s) {
    int n = 0;
    for (char c : s) {
        if (c == ' ') ++n;
        else if (c == '\t') return n;
        else break;
    }
    return n;
}

int Lexer::current_indent() const { return indent_stack_.back().indent; }

void Lexer::flush_indents(int new_indent, int line_no, bool is_dash) {
    int cur = current_indent();
    if (new_indent > cur) {
        Token tok = Token::make(TokenType::INDENT, line_no, 1);
        tok.indent = new_indent;
        emit(std::move(tok));
        indent_stack_.push_back({new_indent, line_no});
    } else if (new_indent < cur) {
        // 对于不规则缩进，缩进只需 ≥ 父缩进（最上层-1级）即可
        // 若 new_indent 虽然小于当前缩进，但仍 > 祖父级缩进，则不退级
        int grandparent_indent = -1;
        if (indent_stack_.size() >= 2) {
            grandparent_indent = indent_stack_[indent_stack_.size() - 2].indent;
        }
        if (new_indent > grandparent_indent) {
            // 缩进容错：不退级，只发警告（适用于序列条目和映射条目）
            warnings_.emplace_back(line_no, 1,
                "Inconsistent indentation: expected " +
                std::to_string(cur) + " but found " +
                std::to_string(new_indent));
        } else {
            while (new_indent < current_indent()) {
                if (indent_stack_.size() <= 1) break;
                indent_stack_.pop_back();
                Token tok = Token::make(TokenType::DEDENT, line_no, 1);
                tok.indent = current_indent();
                emit(std::move(tok));
            }
            if (new_indent != current_indent() && new_indent >= 0) {
                warnings_.emplace_back(line_no, 1,
                    "Inconsistent indentation: expected " +
                    std::to_string(current_indent()) + " but found " +
                    std::to_string(new_indent));
            }
        }
    }
}

// ============================================================
// 引号处理辅助函数
// ============================================================

// 查找匹配的闭合引号，返回位置或 npos
static size_t find_closing_quote(const std::string& s, size_t start) {
    // start 指向开头的 "
    for (size_t i = start + 1; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) { ++i; continue; }
        if (s[i] == '"') return i;
    }
    return std::string::npos;
}

// 反转义引号内的内容
static std::string unescape_quoted(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i;
            switch (s[i]) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case 'n':  out += '\n'; break;
                case 't':  out += '\t'; break;
                default:   out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

// 查找闭合引号（从末尾查找，用于值/键的去引号）
static size_t find_closing_quote_from_end(const std::string& s) {
    if (s.size() < 2 || s[0] != '"') return std::string::npos;
    // 从末尾找最后一个未转义的 "
    for (size_t i = s.size() - 1; i > 0; --i) {
        if (s[i] == '"') {
            // 检查是否被转义
            int backslash_count = 0;
            size_t j = i;
            while (j > 0 && s[j - 1] == '\\') { ++backslash_count; --j; }
            if (backslash_count % 2 == 0) return i; // 偶数个反斜杠 → 未转义
        }
    }
    return std::string::npos;
}

// 如果 s 是完整引号包围的字符串，去除引号并反转义
// 使用贪婪匹配：第一个 " 和最后一个 " 配对
static bool try_unquote(std::string& s) {
    size_t close = find_closing_quote_from_end(s);
    if (close != std::string::npos && close > 0) {
        s = unescape_quoted(s.substr(1, close - 1));
        return true;
    }
    return false;
}

// ============================================================
// 单行词法分析
// ============================================================
void Lexer::tokenize_line_content(const std::string& content, int indent, int line_no) {
    int col_base = indent + 1;
    size_t i = 0;

    auto col = [&]() -> int { return col_base + (int)i; };

    while (i < content.size() && content[i] == ' ') ++i;
    if (i >= content.size()) return;

    // ── DASH ──
    bool has_dash = false;
    if (content[i] == '-') {
        size_t next = i + 1;
        while (next < content.size() && content[next] == ' ') ++next;
        bool looks_like_dash = (next >= content.size() || content[next] == ':');
        if (!looks_like_dash && next < content.size() && std::isdigit(static_cast<unsigned char>(content[next]))) {
            looks_like_dash = false;
        } else if (!looks_like_dash) {
            looks_like_dash = true;
        }
        if (looks_like_dash) {
            has_dash = true;
            emit(Token::make(TokenType::DASH, line_no, col()));
            ++i;
            while (i < content.size() && content[i] == ' ') ++i;
        }
    }

    if (i >= content.size()) return;

    // ── 找最外层冒号（跳过引号内的内容）──
    size_t colon_pos = std::string::npos;
    for (size_t j = i; j < content.size(); ++j) {
        if (content[j] == '\\' && j + 1 < content.size()) { ++j; continue; }
        if (content[j] == '"') {
            size_t close = find_closing_quote(content, j);
            if (close != std::string::npos) {
                j = close; // 跳过整个引号段
                continue;
            }
            // 无闭合引号 → " 是普通字符，继续
            continue;
        }
        if (content[j] == ':') { colon_pos = j; break; }
    }

    if (colon_pos != std::string::npos && colon_pos > i) {
        // ── KEY + COLON [+ VALUE] ──
        std::string key = content.substr(i, colon_pos - i);

        // 只有当 key 以 " 结尾时才尝试去引号（因为引号键后面紧跟冒号，无空格）
        // 如果 key 是 "key" : value（引号后有空格的键），则保留引号和空格
        bool key_unquoted = false;
        if (!key.empty() && key.back() == '"') {
            key_unquoted = try_unquote(key);
        }

        // 没有通过引号去皮 → 只 trim 前导空格，保留尾部空格
        // （冒号前不是引号而是空格时，整个含空格都是键）
        if (!key_unquoted) {
            size_t ks = 0;
            while (ks < key.size() && key[ks] == ' ') ++ks;
            key = key.substr(ks);
        }

        if (!key.empty()) {
            emit(Token::make_with_val(TokenType::KEY, key, line_no, col_base + (int)i));
        }
        emit(Token::make(TokenType::COLON, line_no, col_base + (int)colon_pos));

        i = colon_pos + 1;
        // 冒号后：跳过所有连续空格；若冒号后紧跟 Tab（无空格），则只跳过一个 Tab
        if (i < content.size() && content[i] == ' ') {
            while (i < content.size() && content[i] == ' ') ++i;
        } else if (i < content.size() && content[i] == '\t') {
            ++i;
        }
        if (i >= content.size()) return; // key: 无值

        std::string val = content.substr(i);
        while (!val.empty() && val.back() == ' ') val.pop_back();
        if (val.empty()) return; // key:   (只有空格)

        // ── 引号包围的值 → 始终为 SCALAR ──
        if (!val.empty() && val.front() == '"') {
            size_t close = find_closing_quote_from_end(val);
            if (close != std::string::npos && close > 0) {
                std::string unquoted = unescape_quoted(val.substr(1, close - 1));
                // 追加闭合引号后的剩余内容
                if (close + 1 < val.size()) {
                    unquoted += val.substr(close + 1);
                }
                emit(Token::make_with_val(TokenType::SCALAR, unquoted, line_no, col()));
                return;
            }
            // 无闭合引号 → 当作普通值处理（fall through）
        }

        // ── null 字面量 ──
        if (val == "null" || val == "NULL" || val == "Null" || val == "~") {
            emit(Token::make_with_val(TokenType::NULL_, val, line_no, col()));
            return;
        }

        // ── 多行文本开启: key: { ──
        if (val == "{") {
            in_multiline_ = true;
            multiline_text_.clear();
            multiline_key_line_ = line_no;
            multiline_base_indent_ = indent;
            return;
        }

        // ── 行内列表 ──
        if (val.size() >= 2 && val.front() == '[' && val.back() == ']') {
            auto tok = Token::make_with_val(TokenType::INLINE_LIST, val, line_no, col());
            parse_inline_list(val, line_no, col(), tok.inline_list_items);
            emit(std::move(tok));
            return;
        }

        // ── 普通标量 ──
        emit(Token::make_with_val(TokenType::SCALAR, val, line_no, col()));
    } else {
        // ── 无冒号 ──
        std::string raw_rest = content.substr(i);
        while (!raw_rest.empty() && raw_rest.back() == ' ') raw_rest.pop_back();
        if (raw_rest.empty()) return;

        // ── 引号包围的值 → 始终为纯文本，不作特殊解析 ──
        // 只有当整个内容是完整引号字符串时才去引号（闭合引号在末尾）
        // 否则引号是字面字符
        std::string rest;
        bool is_quoted = false;
        if (!raw_rest.empty() && raw_rest.front() == '"') {
            size_t close = find_closing_quote_from_end(raw_rest);
            if (close != std::string::npos && close > 0 && close + 1 >= raw_rest.size()) {
                // 完整引号字符串：闭合引号是最后一个字符
                rest = unescape_quoted(raw_rest.substr(1, close - 1));
                is_quoted = true;
            }
        }
        if (!is_quoted) {
            rest = raw_rest;
            // 处理冒号在开头的情况：`: "值41"` → 键 `: 值41`
            // 冒号后紧跟的引号字符串需要去引号
            if (!rest.empty() && rest[0] == ':') {
                size_t after_colon = 1;
                while (after_colon < rest.size() && rest[after_colon] == ' ') ++after_colon;
                if (after_colon < rest.size() && rest[after_colon] == '"') {
                    std::string after = rest.substr(after_colon);
                    if (try_unquote(after)) {
                        rest = rest.substr(0, after_colon) + after;
                    }
                }
            }
        }

        // ── 行内列表（无冒号时也支持，但引号内容除外）──
        if (!is_quoted && rest.size() >= 2 && rest.front() == '[' && rest.back() == ']') {
            auto tok = Token::make_with_val(TokenType::INLINE_LIST, rest, line_no, col());
            parse_inline_list(rest, line_no, col(), tok.inline_list_items);
            emit(std::move(tok));
            return;
        }

        if (has_dash) {
            if (rest == "null" || rest == "NULL" || rest == "Null" || rest == "~") {
                emit(Token::make_with_val(TokenType::NULL_, rest, line_no, col()));
            } else {
                emit(Token::make_with_val(TokenType::SCALAR, rest, line_no, col()));
            }
        } else {
            emit(Token::make_with_val(TokenType::BARE_KEY, rest, line_no, col()));
        }
    }
}

// ============================================================
// 行内列表解析 — 逗号分割，支持引号
// [a, b, c] → ["a", "b", "c"]
// ["a", "b"] → ["a", "b"]（去除引号）
// [a, ]  → ["a", null]
// [ ,b]  → [null, "b"]
// ============================================================
void Lexer::parse_inline_list(const std::string& text, int ln, int col,
                                std::vector<InlineElem>& items) {
    if (text.size() < 2) return;
    std::string inner = text.substr(1, text.size() - 2);

    // 按逗号分割
    size_t i = 0;
    while (i <= inner.size()) {
        // 跳过空格
        while (i < inner.size() && inner[i] == ' ') ++i;

        // 找下一个逗号（跳过引号段）
        size_t comma = i;
        while (comma < inner.size()) {
            if (inner[comma] == '"') {
                // 跳过引号段
                size_t qclose = find_closing_quote(inner, comma);
                if (qclose != std::string::npos) {
                    comma = qclose + 1;
                    continue;
                }
            }
            if (inner[comma] == ',') break;
            ++comma;
        }

        std::string seg = inner.substr(i, comma - i);

        // trim
        while (!seg.empty() && seg.front() == ' ') seg.erase(0, 1);
        while (!seg.empty() && seg.back() == ' ') seg.pop_back();

        // 去除引号（返回是否成功去除了引号，用于区分 "" 显式空字符串和空段）
        bool unquoted = try_unquote(seg);

        // null 字面量检查
        if (seg == "null" || seg == "NULL" || seg == "Null" || seg == "~") {
            items.push_back({InlineElem::Kind::NULL_VAL, seg});
            i = comma;
            if (i < inner.size() && inner[i] == ',') ++i;
            if (i >= inner.size() && comma >= inner.size()) break;
            continue;
        }

        if (seg.empty() && comma < inner.size()) {
            // 空段：引号包围的空字符串 "" → 空标量；否则 → null
            if (unquoted) {
                items.push_back({InlineElem::Kind::SCALAR, seg});
            } else {
                items.push_back({InlineElem::Kind::NULL_VAL, ""});
            }
        } else if (!seg.empty()) {
            items.push_back({InlineElem::Kind::SCALAR, seg});
        } else if (comma >= inner.size() && !items.empty()) {
            // 末尾空段：引号包围的空字符串 "" → 空标量；否则 → null
            if (unquoted) {
                items.push_back({InlineElem::Kind::SCALAR, seg});
            } else {
                items.push_back({InlineElem::Kind::NULL_VAL, ""});
            }
        }

        i = comma;
        if (i < inner.size() && inner[i] == ',') ++i;
        if (i >= inner.size() && comma >= inner.size()) break;
>>>>>>> Stashed changes
    }
}

<<<<<<< Updated upstream
void Lexer::emit_token(Token token) {
    emit(std::move(token));
=======
    if (inner.empty()) return;
}

void Lexer::emit(Token t) {
    if (callback_) callback_(std::move(t));
}

void Lexer::emit_newline(int line_no) {
    emit(Token::make(TokenType::NEWLINE, line_no, 1));
>>>>>>> Stashed changes
>>>>>>> Stashed changes
}

} // namespace stml
