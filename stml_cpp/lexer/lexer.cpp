#include "lexer/lexer.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace stml {

// =========================================================================
// Construction
// =========================================================================

STMLLexer::STMLLexer(std::string text)
{
    // Normalise line endings: \r\n → \n, \r → \n
    std::string normalised;
    normalised.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                ++i; // skip \n of \r\n
            }
            normalised.push_back('\n');
        } else {
            normalised.push_back(text[i]);
        }
    }

    // Split into lines
    m_lines.clear();
    size_t start = 0;
    for (size_t i = 0; i < normalised.size(); ++i) {
        if (normalised[i] == '\n') {
            m_lines.push_back(normalised.substr(start, i - start));
            start = i + 1;
        }
    }
    if (start <= normalised.size()) {
        m_lines.push_back(normalised.substr(start));
    }
    m_line_count = static_cast<int>(m_lines.size());

    m_indent_stack = {0};
    m_line_idx = 0;
    m_deferred_dedent_to = std::nullopt;
}

// =========================================================================
// Public API
// =========================================================================

std::pair<std::vector<Token>, std::vector<Warning>> STMLLexer::tokenize()
{
    m_tokens.clear();
    m_warnings.clear();
    m_indent_stack = {0};
    m_line_idx = 0;
    m_deferred_dedent_to = std::nullopt;

    while (m_line_idx < m_line_count) {
        const auto& line = m_lines[m_line_idx];
        int indent = _calc_indent(line);
        std::string content = line.substr(indent);

        // Skip empty lines and whole-line comments
        if (_is_blank_or_comment(content)) {
            ++m_line_idx;
            continue;
        }

        // Document separator (indent must be 0)
        {
            // strip leading/trailing spaces
            const char* p = content.c_str();
            while (*p == ' ') ++p;
            const char* end = p + strlen(p);
            while (end > p && *(end - 1) == ' ') --end;
            std::string stripped(p, end - p);

            if (indent == 0 && stripped == "---") {
                _emit_dedents_to(0);
                _add_token(TokenType::DOC_SEPARATOR, std::string("---"),
                           m_line_idx + 1, indent + 1);
                ++m_line_idx;
                continue;
            }
        }

        // Process indentation changes
        _process_indent(indent);

        // Dispatch on line type
        {
            // strip for checking "---"
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

        ++m_line_idx;

        // Emit deferred DEDENTs from multiline-string closure
        if (m_deferred_dedent_to.has_value()) {
            int target = m_deferred_dedent_to.value();
            m_deferred_dedent_to = std::nullopt;
            _emit_dedents_to(target);
        }
    }

    // EOF: pop remaining indentation → DEDENTs
    _emit_dedents_to(0);
    _add_token(TokenType::END, std::monostate{}, m_line_count, 1);

    return {std::move(m_tokens), std::move(m_warnings)};
}

// =========================================================================
// Indentation helpers
// =========================================================================

int STMLLexer::_calc_indent(const std::string& line) const
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

bool STMLLexer::_is_blank_or_comment(const std::string& content) const
{
    const char* p = content.c_str();
    while (*p == ' ') ++p;
    return *p == '\0' || *p == '#';
}

void STMLLexer::_process_indent(int indent)
{
    int top = m_indent_stack.back();
    if (indent > top) {
        m_indent_stack.push_back(indent);
        _add_token(TokenType::INDENT, std::to_string(indent - top), m_line_idx + 1, 1);
    } else if (indent < top) {
        _emit_dedents_to(indent);
        // If indent > new top, emit INDENT to re-enter at new level
        if (indent > m_indent_stack.back()) {
            int diff = indent - m_indent_stack.back();
            m_indent_stack.push_back(indent);
            _add_token(TokenType::INDENT, std::to_string(diff), m_line_idx + 1, 1);
        }
    }
    // indent == top → nothing to do
}

void STMLLexer::_emit_dedents_to(int target)
{
    while (m_indent_stack.back() > target) {
        m_indent_stack.pop_back();
        _add_token(TokenType::DEDENT, std::monostate{}, m_line_idx + 1, 1);
    }
}

// =========================================================================
// Token factory
// =========================================================================

void STMLLexer::_add_token(TokenType type, TokenValue value, int line, int column)
{
    m_tokens.emplace_back(type, std::move(value), line, column);
}

void STMLLexer::_add_warning(int line, int column, const std::string& message)
{
    m_warnings.emplace_back(line, column, message);
}

int STMLLexer::_col(int indent, int offset_in_content) const
{
    return indent + offset_in_content + 1;
}

// =========================================================================
// Line-level dispatch
// =========================================================================

void STMLLexer::_lex_sequence_line(const std::string& content, int indent)
{
    int line_no = m_line_idx + 1;
    _add_token(TokenType::DASH, std::monostate{}, line_no, _col(indent, 0));

    std::string rest = content.substr(1); // strip '-'
    if (!rest.empty() && rest[0] == ' ')
        rest = rest.substr(1); // strip optional space

    if (rest.empty()) {
        // Empty sequence entry — parser decides block vs null
        _add_token(TokenType::NEWLINE, std::monostate{}, line_no,
                   _col(indent, static_cast<int>(content.size())));
        return;
    }

    // Sequence entries scan for inline mapping colon
    _lex_value_or_key_on_line(rest, indent, true);
    _add_token(TokenType::NEWLINE, std::monostate{}, line_no,
               _col(indent, static_cast<int>(content.size())));
}

void STMLLexer::_lex_mapping_line(const std::string& content, int indent)
{
    int line_no = m_line_idx + 1;

    // (1) Try quoted-key mode
    if (!content.empty() && content[0] == '"') {
        auto [key, rest, ok, colon_pos] = _try_quoted_key(content, indent);
        if (ok) {
            _add_token(TokenType::KEY, std::move(key), line_no, _col(indent, 0));
            _add_token(TokenType::COLON, std::monostate{}, line_no, _col(indent, colon_pos));
            _lex_remainder_after_colon(rest, indent, line_no);
            _add_token(TokenType::NEWLINE, std::monostate{}, line_no,
                       _col(indent, static_cast<int>(content.size())));
            return;
        }
    }

    // (2) Scan for structural colon
    int colon = _find_unquoted_colon(content);

    if (colon == -1) {
        // No structural colon → bare-key or null-scalar
        // Strip leading/trailing spaces for check
        const char* cp = content.c_str();
        while (*cp == ' ') ++cp;
        const char* ep = cp + strlen(cp);
        while (ep > cp && *(ep - 1) == ' ') --ep;
        std::string stripped(cp, ep - cp);

        // Also strip tabs for null check
        {
            const char* tp = cp;
            while (tp < ep && (*tp == ' ' || *tp == '\t')) ++tp;
            const char* te = ep;
            while (te > tp && (*(te - 1) == ' ' || *(te - 1) == '\t')) --te;
            std::string all_stripped(tp, te - tp);
            if (all_stripped == "null" || all_stripped == "~") {
                // Entire line is null
                _add_token(TokenType::NULL_, std::monostate{}, line_no, _col(indent, 0));
                _add_token(TokenType::NEWLINE, std::monostate{}, line_no,
                           _col(indent, static_cast<int>(content.size())));
                return;
            }
        }

        std::string key = _bare_key_from_content(content);
        _add_token(TokenType::BARE_KEY, std::move(key), line_no, _col(indent, 0));
        _add_token(TokenType::NEWLINE, std::monostate{}, line_no,
                   _col(indent, static_cast<int>(content.size())));
        return;
    }

    // (3) Colon at position 0 → empty key → treat as BARE_KEY
    if (colon == 0) {
        std::string key = _reconstruct_empty_key(content);
        _add_token(TokenType::BARE_KEY, std::move(key), line_no, _col(indent, 0));
        _add_token(TokenType::NEWLINE, std::monostate{}, line_no,
                   _col(indent, static_cast<int>(content.size())));
        return;
    }

    // (4) Normal key: content before colon
    std::string key = content.substr(0, colon); // preserve trailing spaces
    std::string rest = content.substr(colon + 1);

    _add_token(TokenType::KEY, std::move(key), line_no, _col(indent, 0));
    _add_token(TokenType::COLON, std::monostate{}, line_no, _col(indent, colon));
    _lex_remainder_after_colon(rest, indent, line_no);
    _add_token(TokenType::NEWLINE, std::monostate{}, line_no,
               _col(indent, static_cast<int>(content.size())));
}

// =========================================================================
// Value parsing (inline content after COLON or DASH)
// =========================================================================

void STMLLexer::_lex_remainder_after_colon(const std::string& rest, int indent, int line_no)
{
    (void)line_no;
    if (rest.empty()) return;

    std::string r = rest;
    if (r[0] == ' ' || r[0] == '\t')
        r = r.substr(1); // consume optional single space or tab

    if (r.empty()) return;

    _lex_value_or_key_on_line(r, indent, false);
}

void STMLLexer::_lex_value_or_key_on_line(const std::string& text, int indent, bool scan_colon)
{
    int line_no = m_line_idx + 1;

    // Strip leading/trailing spaces for check
    const char* ts = text.c_str();
    while (*ts == ' ') ++ts;
    const char* te = ts + strlen(ts);
    while (te > ts && *(te - 1) == ' ') --te;
    std::string stripped(ts, te - ts);

    // Multiline string trigger: content is exactly '{'
    if (stripped == "{" && m_line_idx + 1 < m_line_count) {
        auto [ml_value, consumed, close_indent] = _read_multiline(indent);
        _add_token(TokenType::MULTILINE_STRING, std::move(ml_value),
                   line_no, _col(indent, indent));
        m_line_idx = consumed - 1; // -1 because caller will ++
        // Defer DEDENT emission to main loop
        m_deferred_dedent_to = close_indent;
        return;
    }

    // Inline list: only triggers when both '[' and ']' are on the line
    if (!text.empty() && text[0] == '[') {
        // rstrip the line for ] check
        const char* ep = text.c_str() + text.size();
        while (ep > text.c_str() && *(ep - 1) == ' ') --ep;
        if (ep > text.c_str() + 1 && *(ep - 1) == ']') {
            auto [elems, ok, col] = _parse_inline_list(text, indent);
            if (ok) {
                _add_token(TokenType::INLINE_LIST, std::move(elems),
                           line_no, _col(indent, 0));
                return;
            }
            // Malformed inline list → degrade to raw string
            _add_warning(line_no, col ? col : _col(indent, 0),
                         u8"内联列表缺少结尾 ']'，降级为原始字符串");
            _add_token(TokenType::RAW_STRING, std::string(text),
                       line_no, _col(indent, 0));
            return;
        }
    }

    // Value starts with '[' but no matching ']' → malformed
    if (!text.empty() && text[0] == '[') {
        // Check if ']' exists anywhere
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
            // Inline mapping: "key: value" inside a sequence entry
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

void STMLLexer::_lex_scalar_or_null(const std::string& text, int indent)
{
    int line_no = m_line_idx + 1;

    // Strip only spaces (U+0020)
    const char* p = text.c_str();
    while (*p == ' ') ++p;
    const char* e = p + strlen(p);
    while (e > p && *(e - 1) == ' ') --e;
    std::string stripped(p, e - p);

    if (stripped.empty()) return; // no token; parser handles

    // For null/~ check, strip all whitespace (including tabs)
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
STMLLexer::_try_quoted_key(const std::string& content, int indent)
{
    // 从位置1开始搜索闭合引号+冒号的组合
    int pos = 1;
    while (pos < static_cast<int>(content.size())) {
        int end = _find_first_unescaped_quote(content, pos);
        if (end == -1)
            return {"", "", false, -1};
        // 检查闭合引号后是否紧跟冒号
        if (end + 1 < static_cast<int>(content.size()) && content[end + 1] == ':') {
            std::string raw_key = content.substr(1, end - 1);
            std::string key = _unescape(raw_key, indent + 1);
            std::string rest = content.substr(end + 2);
            return {std::move(key), std::move(rest), true, end + 1};
        }
        // 不是闭合引号+冒号，继续搜索下一个引号
        pos = end + 1;
    }
    return {"", "", false, -1};
}

// =========================================================================
// Structural colon scan
// =========================================================================

int STMLLexer::_find_unquoted_colon(const std::string& s) const
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

    // Unclosed quote that was never closed → '"' is probably key-name char
    if (in_quote && !had_close) {
        auto pos = s.find(':');
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }
    return -1;
}

// =========================================================================
// Quote matching
// =========================================================================

int STMLLexer::_find_first_unescaped_quote(const std::string& s, int start) const
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

int STMLLexer::_find_last_unescaped_quote(const std::string& s, int start) const
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

std::string STMLLexer::_unescape(const std::string& s, int col_offset)
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
                // Illegal escape — keep as-is
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
STMLLexer::_parse_quoted_value(const std::string& s, int start, int indent)
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
STMLLexer::_parse_inline_list(const std::string& s, int indent)
{
    // Find closing ']' — must be at end (with optional trailing spaces)
    int close_pos = static_cast<int>(s.rfind(']'));
    if (close_pos == -1)
        return {{}, false, indent + static_cast<int>(s.size())};

    // Check content after ']' is just whitespace
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
        // Skip leading spaces
        while (i < n && inner[i] == ' ') ++i;
        if (i >= n) break;

        // Quoted element (non-greedy)
        if (inner[i] == '"') {
            int eq = _find_first_unescaped_quote(inner, i + 1);
            if (eq != -1) {
                std::string raw_elem = inner.substr(i + 1, eq - i - 1);
                elements.push_back(_unescape(raw_elem, indent + i + 2));
                i = eq + 1;
            } else {
                // Unclosed quote — take rest as raw
                elements.push_back(inner.substr(i));
                i = n;
            }
            // Skip comma and spaces
            while (i < n && (inner[i] == ',' || inner[i] == ' ')) ++i;
        } else {
            // Unquoted element — stops at ','
            int start_i = i;
            while (i < n && inner[i] != ',') ++i;
            std::string elem = inner.substr(start_i, i - start_i);
            // Strip spaces
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
            // Skip comma and spaces
            while (i < n && (inner[i] == ',' || inner[i] == ' ')) ++i;
        }
    }

    // Trailing comma → extra null element
    {
        // Check if inner ends with ',' (after stripping spaces)
        const char* cp = inner.c_str();
        const char* ce = cp + inner.size();
        while (ce > cp && *(ce - 1) == ' ') --ce;
        if (ce > cp && *(ce - 1) == ',')
            elements.push_back(std::nullopt);
    }

    return {std::move(elements), true, 0};
}

// =========================================================================
// Multiline string
// =========================================================================

std::tuple<std::string, int, int> STMLLexer::_read_multiline(int key_indent)
{
    std::vector<std::string> parts;
    int i = m_line_idx + 1; // start after '{' line
    int close_indent = key_indent; // default (EOF case)

    while (i < m_line_count) {
        const auto& line = m_lines[i];

        // Calculate indent
        int line_indent = 0;
        for (char ch : line) {
            if (ch == ' ') ++line_indent;
            else break;
        }
        std::string content = line.substr(line_indent);

        // Strip spaces for content check
        const char* cp = content.c_str();
        while (*cp == ' ') ++cp;
        const char* ce = cp + strlen(cp);
        while (ce > cp && *(ce - 1) == ' ') --ce;
        std::string stripped(cp, ce - cp);

        // Standalone '}' at any indentation → close
        // The closing brace is syntax, not content — indent after
        // multiline always restores to the key's indent level.
        if (stripped == "}" && line_indent <= key_indent) {
            ++i; // consume closing '}'
            break;
        }

        // Preserve raw line
        parts.push_back(line);
        ++i;
    }

    // i == m_line_count → EOF auto-close
    if (i >= m_line_count) {
        _add_warning(i, 1,
                     u8"多行字符串未找到闭合 '}'，已由 EOF 自动闭合");
    }

    // Join parts with newline
    std::string result;
    for (size_t j = 0; j < parts.size(); ++j) {
        if (j > 0) result.push_back('\n');
        result.append(parts[j]);
    }

    return {std::move(result), i, close_indent};
}

// =========================================================================
// Bare-key helpers
// =========================================================================

std::string STMLLexer::_bare_key_from_content(const std::string& content)
{
    // rstrip
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

std::string STMLLexer::_reconstruct_empty_key(const std::string& content)
{
    std::string rest = content.substr(1); // after ':'
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
        // strip spaces
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
