#include "lexer/streaming_lexer.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace stml {

// =========================================================================
// Construction
// =========================================================================

StreamingLexer::StreamingLexer()
    : m_line_idx(0)
    , m_indent_stack({0})
    , m_first_chunk(true)
    , m_in_multiline(false)
    , m_multiline_key_indent(0)
    , m_multiline_start_line(0)
    , m_multiline_newline_line(0)
    , m_multiline_newline_col(0)
    , m_last_emitted(0)
    , m_deferred_dedent_to(std::nullopt)
{
}

// =========================================================================
// Public API
// =========================================================================

std::vector<Token> StreamingLexer::feed(const std::string& chunk)
{
    m_first_chunk = false;

    // Normalise line endings: \r\n → \n, \r → \n
    std::string normalised;
    normalised.reserve(chunk.size());
    for (size_t i = 0; i < chunk.size(); ++i) {
        if (chunk[i] == '\r') {
            if (i + 1 < chunk.size() && chunk[i + 1] == '\n') {
                ++i;
            }
            normalised.push_back('\n');
        } else {
            normalised.push_back(chunk[i]);
        }
    }

    // Append to buffer
    m_buffer += normalised;

    // Extract complete lines from buffer
    size_t pos;
    while ((pos = m_buffer.find('\n')) != std::string::npos) {
        m_lines.push_back(m_buffer.substr(0, pos));
        m_buffer.erase(0, pos + 1);
    }

    // Process new lines
    _process_lines();

    // Return tokens emitted since last call
    std::vector<Token> new_tokens(
        m_tokens.begin() + static_cast<ptrdiff_t>(m_last_emitted),
        m_tokens.end());
    m_last_emitted = m_tokens.size();
    return new_tokens;
}

std::vector<Token> StreamingLexer::finalize()
{
    // Process any remaining content in buffer as a final line
    if (!m_buffer.empty()) {
        m_lines.push_back(m_buffer);
        m_buffer.clear();
        _process_lines();
    }

    // Auto-close multiline if still open
    if (m_in_multiline) {
        _add_warning(m_multiline_start_line, 1,
                     u8"多行字符串未找到闭合 '}'，已由 EOF 自动闭合");

        // Build multiline value from accumulated parts
        std::string ml_value;
        for (size_t j = 0; j < m_multiline_parts.size(); ++j) {
            if (j > 0) ml_value.push_back('\n');
            ml_value.append(m_multiline_parts[j]);
        }

        _add_token(TokenType::MULTILINE_STRING, std::move(ml_value),
                   m_multiline_start_line, _col(m_multiline_key_indent, 0));
        _add_token(TokenType::NEWLINE, std::monostate{},
                   m_multiline_newline_line, m_multiline_newline_col);

        // Emit deferred DEDENT if any
        if (m_deferred_dedent_to.has_value()) {
            int target = m_deferred_dedent_to.value();
            m_deferred_dedent_to = std::nullopt;
            _emit_dedents_to(target);
        }

        m_in_multiline = false;
        m_multiline_parts.clear();
    }

    // EOF: pop remaining indentation → DEDENTs
    _emit_dedents_to(0);
    int last_line = static_cast<int>(m_lines.size());
    _add_token(TokenType::END, std::monostate{}, last_line, 1);

    // Return remaining tokens
    std::vector<Token> new_tokens(
        m_tokens.begin() + static_cast<ptrdiff_t>(m_last_emitted),
        m_tokens.end());
    m_last_emitted = m_tokens.size();
    return new_tokens;
}

// =========================================================================
// Internal line processing
// =========================================================================

void StreamingLexer::_process_lines()
{
    while (m_line_idx < static_cast<int>(m_lines.size())) {

        // ---- multiline accumulation mode ----
        if (m_in_multiline) {
            const auto& line = m_lines[m_line_idx];

            int line_indent = _calc_indent(line);
            std::string content = line.substr(line_indent);

            // Strip spaces for content check
            const char* cp = content.c_str();
            while (*cp == ' ') ++cp;
            const char* ce = cp + strlen(cp);
            while (ce > cp && *(ce - 1) == ' ') --ce;
            std::string stripped(cp, ce - cp);

            // Standalone '}' at or before key indent → close multiline
            if (stripped == "}" && line_indent <= m_multiline_key_indent) {
                // Build multiline value
                std::string ml_value;
                for (size_t j = 0; j < m_multiline_parts.size(); ++j) {
                    if (j > 0) ml_value.push_back('\n');
                    ml_value.append(m_multiline_parts[j]);
                }

                // Emit MULTILINE_STRING before the deferred NEWLINE (matches batch order)
                _add_token(TokenType::MULTILINE_STRING, std::move(ml_value),
                           m_multiline_start_line, _col(m_multiline_key_indent, 0));
                _add_token(TokenType::NEWLINE, std::monostate{},
                           m_multiline_newline_line, m_multiline_newline_col);

                // Defer DEDENT if close indent is less than key indent
                if (line_indent < m_multiline_key_indent) {
                    m_deferred_dedent_to = line_indent;
                }

                m_in_multiline = false;
                m_multiline_parts.clear();
                ++m_line_idx; // consume '}' line

                // Emit deferred DEDENT
                if (m_deferred_dedent_to.has_value()) {
                    int target = m_deferred_dedent_to.value();
                    m_deferred_dedent_to = std::nullopt;
                    _emit_dedents_to(target);
                }
                continue;
            }

            // Not a close — accumulate this line
            m_multiline_parts.push_back(line);
            ++m_line_idx;
            continue;
        }

        // ---- normal line processing ----
        _process_one_line(m_line_idx);
        ++m_line_idx;

        // Emit deferred DEDENTs (from batch-style multiline, if any)
        if (m_deferred_dedent_to.has_value()) {
            int target = m_deferred_dedent_to.value();
            m_deferred_dedent_to = std::nullopt;
            _emit_dedents_to(target);
        }
    }
}

void StreamingLexer::_process_one_line(int /*line_idx*/)
{
    const auto& line = m_lines[m_line_idx];
    int indent = _calc_indent(line);
    std::string content = line.substr(indent);

    // Skip empty lines and whole-line comments
    if (_is_blank_or_comment(content)) {
        return;
    }

    // Document separator (indent must be 0)
    {
        const char* p = content.c_str();
        while (*p == ' ') ++p;
        const char* end = p + strlen(p);
        while (end > p && *(end - 1) == ' ') --end;
        std::string stripped(p, end - p);

        if (indent == 0 && stripped == "---") {
            _emit_dedents_to(0);
            _add_token(TokenType::DOC_SEPARATOR, std::string("---"),
                       m_line_idx + 1, indent + 1);
            return;
        }
    }

    // Process indentation changes
    _process_indent(indent);

    // Dispatch on line type
    {
        const char* p = content.c_str();
        while (*p == ' ') ++p;
        const char* end = p + strlen(p);
        while (end > p && *(end - 1) == ' ') --end;
        std::string content_stripped(p, end - p);

        if (content_stripped == "---" && indent > 0) {
            _lex_mapping_line(content, indent);
        } else if (!content.empty() && content[0] == '-') {
            _lex_sequence_line(content, indent);
        } else {
            _lex_mapping_line(content, indent);
        }
    }
}

// =========================================================================
// Token factory
// =========================================================================

void StreamingLexer::_add_token(TokenType type, TokenValue value, int line, int column)
{
    m_tokens.emplace_back(type, std::move(value), line, column);
}

void StreamingLexer::_add_warning(int line, int column, const std::string& message)
{
    m_warnings.emplace_back(line, column, message);
}

int StreamingLexer::_col(int indent, int offset_in_content) const
{
    return indent + offset_in_content + 1;
}

// =========================================================================
// Indentation helpers
// =========================================================================

int StreamingLexer::_calc_indent(const std::string& line) const
{
    int n = 0;
    for (char ch : line) {
        if (ch == ' ')
            ++n;
        else
            break;
    }
    return n;
}

bool StreamingLexer::_is_blank_or_comment(const std::string& content) const
{
    const char* p = content.c_str();
    while (*p == ' ') ++p;
    return *p == '\0' || *p == '#';
}

void StreamingLexer::_process_indent(int indent)
{
    int top = m_indent_stack.back();
    if (indent > top) {
        m_indent_stack.push_back(indent);
        _add_token(TokenType::INDENT, std::to_string(indent - top), m_line_idx + 1, 1);
    } else if (indent < top) {
        _emit_dedents_to(indent);
        if (indent > m_indent_stack.back()) {
            int diff = indent - m_indent_stack.back();
            m_indent_stack.push_back(indent);
            _add_token(TokenType::INDENT, std::to_string(diff), m_line_idx + 1, 1);
        }
    }
}

void StreamingLexer::_emit_dedents_to(int target)
{
    while (m_indent_stack.back() > target) {
        m_indent_stack.pop_back();
        _add_token(TokenType::DEDENT, std::monostate{}, m_line_idx + 1, 1);
    }
}

// =========================================================================
// Line-level dispatch
// =========================================================================

void StreamingLexer::_lex_sequence_line(const std::string& content, int indent)
{
    int line_no = m_line_idx + 1;
    int content_len = static_cast<int>(content.size());
    int newline_col = _col(indent, content_len);
    _add_token(TokenType::DASH, std::monostate{}, line_no, _col(indent, 0));

    std::string rest = content.substr(1);
    if (!rest.empty() && rest[0] == ' ')
        rest = rest.substr(1);

    if (rest.empty()) {
        _add_token(TokenType::NEWLINE, std::monostate{}, line_no, newline_col);
        return;
    }

    _lex_value_or_key_on_line(rest, indent, true);

    // Defer NEWLINE if we entered multiline mode (MULTILINE_STRING must come first)
    if (m_in_multiline) {
        m_multiline_newline_line = line_no;
        m_multiline_newline_col = newline_col;
    } else {
        _add_token(TokenType::NEWLINE, std::monostate{}, line_no, newline_col);
    }
}

void StreamingLexer::_lex_mapping_line(const std::string& content, int indent)
{
    int line_no = m_line_idx + 1;
    int content_len = static_cast<int>(content.size());
    int newline_col = _col(indent, content_len);

    // (1) Try quoted-key mode
    if (!content.empty() && content[0] == '"') {
        auto [key, rest, ok, colon_pos] = _try_quoted_key(content, indent);
        if (ok) {
            _add_token(TokenType::KEY, std::move(key), line_no, _col(indent, 0));
            _add_token(TokenType::COLON, std::monostate{}, line_no, _col(indent, colon_pos));
            _lex_remainder_after_colon(rest, indent, line_no);
            if (m_in_multiline) {
                m_multiline_newline_line = line_no;
                m_multiline_newline_col = newline_col;
            } else {
                _add_token(TokenType::NEWLINE, std::monostate{}, line_no, newline_col);
            }
            return;
        }
    }

    // (2) Scan for structural colon
    int colon = _find_unquoted_colon(content);

    if (colon == -1) {
        // No structural colon → bare-key or null-scalar
        const char* cp = content.c_str();
        while (*cp == ' ') ++cp;
        const char* ep = cp + strlen(cp);
        while (ep > cp && *(ep - 1) == ' ') --ep;
        std::string stripped(cp, ep - cp);

        {
            const char* tp = cp;
            while (tp < ep && (*tp == ' ' || *tp == '\t')) ++tp;
            const char* te = ep;
            while (te > tp && (*(te - 1) == ' ' || *(te - 1) == '\t')) --te;
            std::string all_stripped(tp, te - tp);
            if (all_stripped == "null" || all_stripped == "~") {
                _add_token(TokenType::NULL_, std::monostate{}, line_no, _col(indent, 0));
                _add_token(TokenType::NEWLINE, std::monostate{}, line_no, newline_col);
                return;
            }
        }

        std::string key = _bare_key_from_content(content);
        _add_token(TokenType::BARE_KEY, std::move(key), line_no, _col(indent, 0));
        _add_token(TokenType::NEWLINE, std::monostate{}, line_no, newline_col);
        return;
    }

    // (3) Colon at position 0 → empty key → treat as BARE_KEY
    if (colon == 0) {
        std::string key = _reconstruct_empty_key(content);
        _add_token(TokenType::BARE_KEY, std::move(key), line_no, _col(indent, 0));
        _add_token(TokenType::NEWLINE, std::monostate{}, line_no, newline_col);
        return;
    }

    // (4) Normal key: content before colon
    std::string key = content.substr(0, colon);
    std::string rest = content.substr(colon + 1);

    _add_token(TokenType::KEY, std::move(key), line_no, _col(indent, 0));
    _add_token(TokenType::COLON, std::monostate{}, line_no, _col(indent, colon));
    _lex_remainder_after_colon(rest, indent, line_no);

    // Defer NEWLINE if we entered multiline mode (MULTILINE_STRING must come first)
    if (m_in_multiline) {
        m_multiline_newline_line = line_no;
        m_multiline_newline_col = newline_col;
    } else {
        _add_token(TokenType::NEWLINE, std::monostate{}, line_no, newline_col);
    }
}

// =========================================================================
// Value parsing (inline content after COLON or DASH)
// =========================================================================

void StreamingLexer::_lex_remainder_after_colon(const std::string& rest, int indent, int line_no)
{
    (void)line_no;
    if (rest.empty()) return;

    std::string r = rest;
    if (r[0] == ' ' || r[0] == '\t')
        r = r.substr(1);

    if (r.empty()) return;

    _lex_value_or_key_on_line(r, indent, false);
}

void StreamingLexer::_lex_value_or_key_on_line(const std::string& text, int indent, bool scan_colon)
{
    int line_no = m_line_idx + 1;

    // Strip leading/trailing spaces for check
    const char* ts = text.c_str();
    while (*ts == ' ') ++ts;
    const char* te = ts + strlen(ts);
    while (te > ts && *(te - 1) == ' ') --te;
    std::string stripped(ts, te - ts);

    // Multiline string trigger: content is exactly '{'
    if (stripped == "{") {
        // Start multiline accumulation mode.
        // The next lines will be accumulated by _process_lines.
        // MULTILINE_STRING + NEWLINE will be emitted when '}' is found.
        // If there are no more lines yet, multiline stays open and
        // finalize() will auto-close.
        m_in_multiline = true;
        m_multiline_key_indent = indent;
        m_multiline_start_line = line_no;
        m_multiline_parts.clear();
        return;
    }

    // Inline list: only triggers when both '[' and ']' are on the line
    if (!text.empty() && text[0] == '[') {
        const char* ep = text.c_str() + text.size();
        while (ep > text.c_str() && *(ep - 1) == ' ') --ep;
        if (ep > text.c_str() + 1 && *(ep - 1) == ']') {
            auto [elems, ok, col] = _parse_inline_list(text, indent);
            if (ok) {
                _add_token(TokenType::INLINE_LIST, std::move(elems),
                           line_no, _col(indent, 0));
                return;
            }
            _add_warning(line_no, col ? col : _col(indent, 0),
                         u8"内联列表缺少结尾 ']'，降级为原始字符串");
            _add_token(TokenType::RAW_STRING, std::string(text),
                       line_no, _col(indent, 0));
            return;
        }
    }

    // Value starts with '[' but no matching ']' → malformed
    if (!text.empty() && text[0] == '[') {
        if (text.find(']') == std::string::npos) {
            _add_warning(line_no, _col(indent, 0),
                         u8"内联列表缺少结尾 ']'，降级为原始字符串");
            _add_token(TokenType::RAW_STRING, std::string(text),
                       line_no, _col(indent, 0));
            return;
        }
    }

    // Quoted content
    if (!text.empty() && text[0] == '"') {
        if (scan_colon) {
            // Non-greedy: try "key": value pattern
            auto [key, rest, ok, colon_pos] = _try_quoted_key(text, indent);
            if (ok) {
                _add_token(TokenType::KEY, key, line_no, _col(indent, 0));
                _add_token(TokenType::COLON, std::monostate{}, line_no, _col(indent, colon_pos));
                int consumed = static_cast<int>(text.size() - rest.size());
                if (!rest.empty() && rest[0] == ' ')
                    rest = rest.substr(1);
                _lex_value_or_key_on_line(rest, indent + consumed, true);
                return;
            }
        }
        // Greedy quoted value
        auto [val, end, ok] = _parse_quoted_value(text, 0, indent);
        if (ok) {
            _add_token(TokenType::SCALAR, std::move(val), line_no, _col(indent, 0));
        } else {
            _add_warning(line_no, _col(indent, 0),
                         u8"引号未闭合，降级为原始字符串");
            _add_token(TokenType::RAW_STRING, std::string(text), line_no, _col(indent, 0));
        }
        return;
    }

    // Unquoted value → for sequence entries, check for inline mapping key:value
    if (scan_colon) {
        int c = _find_unquoted_colon(text);
        if (c >= 0) {
            std::string inline_key = text.substr(0, c);
            std::string inline_rest = text.substr(c + 1);
            if (!inline_rest.empty() && inline_rest[0] == ' ')
                inline_rest = inline_rest.substr(1);
            _add_token(TokenType::KEY, inline_key, line_no, _col(indent, 0));
            _add_token(TokenType::COLON, std::monostate{},
                       line_no, _col(indent, static_cast<int>(inline_key.size())));
            _lex_value_or_key_on_line(inline_rest,
                                       indent + static_cast<int>(inline_key.size()) + 1,
                                       true);
            return;
        }
    }

    // Plain scalar
    _lex_scalar_or_null(text, indent);
}

void StreamingLexer::_lex_scalar_or_null(const std::string& text, int indent)
{
    int line_no = m_line_idx + 1;

    const char* p = text.c_str();
    while (*p == ' ') ++p;
    const char* e = p + strlen(p);
    while (e > p && *(e - 1) == ' ') --e;
    std::string stripped(p, e - p);

    if (stripped.empty()) return;

    const char* tp = p;
    while (tp < e && (*tp == ' ' || *tp == '\t')) ++tp;
    const char* te = e;
    while (te > tp && (*(te - 1) == ' ' || *(te - 1) == '\t')) --te;
    std::string all_stripped(tp, te - tp);

    if (all_stripped == "null" || all_stripped == "~") {
        _add_token(TokenType::NULL_, std::monostate{}, line_no, _col(indent, 0));
    } else {
        _add_token(TokenType::SCALAR, std::move(stripped), line_no, _col(indent, 0));
    }
}

// =========================================================================
// Quoted-key attempt — non-greedy
// =========================================================================

std::tuple<std::string, std::string, bool, int>
StreamingLexer::_try_quoted_key(const std::string& content, int indent)
{
    int end = _find_first_unescaped_quote(content, 1);
    if (end == -1)
        return {"", "", false, -1};
    if (end + 1 >= static_cast<int>(content.size()) || content[end + 1] != ':')
        return {"", "", false, -1};
    std::string raw_key = content.substr(1, end - 1);
    std::string key = _unescape(raw_key, indent + 1);
    std::string rest = content.substr(end + 2);
    return {std::move(key), std::move(rest), true, end + 1};
}

// =========================================================================
// Structural colon scan
// =========================================================================

int StreamingLexer::_find_unquoted_colon(const std::string& s) const
{
    bool in_quote = false;
    bool had_close = false;
    int i = 0;
    int n = static_cast<int>(s.size());
    while (i < n) {
        char ch = s[i];
        if (ch == '\\') {
            i += 2;
            continue;
        }
        if (ch == '"') {
            if (in_quote)
                had_close = true;
            in_quote = !in_quote;
        } else if (ch == ':' && !in_quote) {
            return i;
        }
        ++i;
    }

    if (in_quote && !had_close) {
        auto pos = s.find(':');
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }
    return -1;
}

// =========================================================================
// Quote matching
// =========================================================================

int StreamingLexer::_find_first_unescaped_quote(const std::string& s, int start) const
{
    int i = start;
    int n = static_cast<int>(s.size());
    while (i < n) {
        if (s[i] == '\\') {
            i += 2;
            continue;
        }
        if (s[i] == '"')
            return i;
        ++i;
    }
    return -1;
}

int StreamingLexer::_find_last_unescaped_quote(const std::string& s, int start) const
{
    int last = -1;
    int i = start;
    int n = static_cast<int>(s.size());
    while (i < n) {
        if (s[i] == '\\') {
            i += 2;
            continue;
        }
        if (s[i] == '"')
            last = i;
        ++i;
    }
    return last;
}

// =========================================================================
// Escape processing
// =========================================================================

std::string StreamingLexer::_unescape(const std::string& s, int col_offset)
{
    std::string result;
    result.reserve(s.size());
    int i = 0;
    int n = static_cast<int>(s.size());
    while (i < n) {
        if (s[i] == '\\' && i + 1 < n) {
            char nxt = s[i + 1];
            if (nxt == '"') {
                result.push_back('"');
            } else if (nxt == '\\') {
                result.push_back('\\');
            } else if (nxt == 'n') {
                result.push_back('\n');
            } else if (nxt == 't') {
                result.push_back('\t');
            } else {
                result.push_back('\\');
                result.push_back(nxt);
                _add_warning(m_line_idx + 1, col_offset + i + 1,
                             std::string(u8"非法转义序列 '\\") + nxt + u8"'，保留原样");
            }
            i += 2;
        } else {
            result.push_back(s[i]);
            ++i;
        }
    }
    return result;
}

// =========================================================================
// Quoted value — greedy
// =========================================================================

std::tuple<std::string, int, bool>
StreamingLexer::_parse_quoted_value(const std::string& s, int start, int indent)
{
    int end = _find_last_unescaped_quote(s, start + 1);
    if (end == -1)
        return {"", start, false};
    std::string raw = s.substr(start + 1, end - start - 1);
    std::string val = _unescape(raw, indent + start + 1);
    return {std::move(val), end + 1, true};
}

// =========================================================================
// Inline list
// =========================================================================

std::tuple<std::vector<InlineElem>, bool, int>
StreamingLexer::_parse_inline_list(const std::string& s, int indent)
{
    int close_pos = static_cast<int>(s.rfind(']'));
    if (close_pos == -1)
        return {{}, false, indent + static_cast<int>(s.size())};

    for (int i = close_pos + 1; i < static_cast<int>(s.size()); ++i) {
        if (s[i] != ' ') {
            return {{}, false, indent + i};
        }
    }

    std::string inner = s.substr(1, close_pos - 1);
    std::vector<InlineElem> elements;
    int i = 0;
    int n = static_cast<int>(inner.size());

    while (i < n) {
        while (i < n && inner[i] == ' ') ++i;
        if (i >= n) break;

        if (inner[i] == '"') {
            int eq = _find_first_unescaped_quote(inner, i + 1);
            if (eq != -1) {
                std::string raw_elem = inner.substr(i + 1, eq - i - 1);
                elements.push_back(_unescape(raw_elem, indent + i + 2));
                i = eq + 1;
            } else {
                elements.push_back(inner.substr(i));
                i = n;
            }
            while (i < n && (inner[i] == ',' || inner[i] == ' ')) ++i;
        } else {
            int start_i = i;
            while (i < n && inner[i] != ',') ++i;
            std::string elem = inner.substr(start_i, i - start_i);
            const char* ep = elem.c_str();
            while (*ep == ' ') ++ep;
            const char* ee = ep + strlen(ep);
            while (ee > ep && *(ee - 1) == ' ') --ee;
            std::string elem_stripped(ep, ee - ep);

            if (elem_stripped == "null" || elem_stripped == "~") {
                elements.push_back(std::nullopt);
            } else if (elem_stripped.empty()) {
                elements.push_back(std::nullopt);
            } else {
                elements.push_back(elem_stripped);
            }
            while (i < n && (inner[i] == ',' || inner[i] == ' ')) ++i;
        }
    }

    // Trailing comma → extra null element
    {
        const char* cp = inner.c_str();
        const char* ce = cp + inner.size();
        while (ce > cp && *(ce - 1) == ' ') --ce;
        if (ce > cp && *(ce - 1) == ',')
            elements.push_back(std::nullopt);
    }

    return {std::move(elements), true, 0};
}

// =========================================================================
// Bare-key helpers
// =========================================================================

std::string StreamingLexer::_bare_key_from_content(const std::string& content)
{
    const char* p = content.c_str();
    const char* e = p + content.size();
    while (e > p && *(e - 1) == ' ') --e;
    std::string stripped(p, e - p);

    if (stripped.size() >= 2 && stripped[0] == '"' && stripped.back() == '"') {
        std::string inner = stripped.substr(1, stripped.size() - 2);
        return inner + content.substr(stripped.size());
    }
    return stripped;
}

std::string StreamingLexer::_reconstruct_empty_key(const std::string& content)
{
    std::string rest = content.substr(1);
    bool had_space = !rest.empty() && rest[0] == ' ';
    if (had_space)
        rest = rest.substr(1);

    std::string processed;
    if (!rest.empty() && rest[0] == '"') {
        auto [val, end, ok] = _parse_quoted_value(rest, 0, 0);
        if (ok)
            processed = val;
        else
            processed = rest;
    } else {
        const char* p = rest.c_str();
        while (*p == ' ') ++p;
        const char* e = p + strlen(p);
        while (e > p && *(e - 1) == ' ') --e;
        processed = std::string(p, e - p);
    }

    if (had_space)
        return ":" + std::string(" ") + processed;
    return ":" + processed;
}

} // namespace stml
