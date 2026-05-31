#include "parser/parser.h"

#include <set>
#include <string>

namespace stml {

// Token types that represent a value (may appear after COLON or DASH)
static const std::set<TokenType> VALUE_TOKENS = {
    TokenType::SCALAR, TokenType::NULL_, TokenType::RAW_STRING,
    TokenType::INLINE_LIST, TokenType::MULTILINE_STRING,
};

// =========================================================================
// Construction
// =========================================================================

STMLParser::STMLParser(std::vector<Token> tokens)
    : m_tokens(std::move(tokens))
    , m_n(static_cast<int>(m_tokens.size()))
    , m_pos(0)
{
}

// =========================================================================
// Public API
// =========================================================================

std::pair<AstList, std::vector<Warning>> STMLParser::parse()
{
    m_pos = 0;
    m_warnings.clear();
    AstList docs;

    while (m_pos < m_n) {
        _skip_newlines();

        if (m_pos >= m_n) break;

        auto ttype = _peek_type();
        if (!ttype) break;

        if (*ttype == TokenType::END) break;

        // Consume trailing DEDENTs (remaining indent stack after last doc)
        if (*ttype == TokenType::DEDENT) {
            ++m_pos;
            continue;
        }

        // Consume DOC_SEPARATOR between documents
        if (*ttype == TokenType::DOC_SEPARATOR) {
            // Leading separator (before any doc) → null doc
            if (docs.empty()) {
                docs.push_back(AstNode());
            }
            ++m_pos;
            _skip_newlines();
            // Trailing separator at EOF → null doc
            auto next_type = _peek_type();
            if (!next_type || *next_type == TokenType::END) {
                docs.push_back(AstNode());
            } else if (*next_type == TokenType::DOC_SEPARATOR) {
                // Consecutive separators create a null document in between
                docs.push_back(AstNode());
            }
            continue;
        }

        // Parse one document
        auto [node, new_pos] = _parse_document(m_pos);
        docs.push_back(std::move(node));

        // Issue A: top-level mixing — after a sequence document (root-level
        // DASH items), leftover non-DASH content is a structural conflict.
        int check_pos = _skip_newlines_from(new_pos);
        if (docs.back().is_list() && check_pos < m_n) {
            auto leftover_type = _peek_type_at(check_pos);
            if (leftover_type && *leftover_type != TokenType::END
                             && *leftover_type != TokenType::DOC_SEPARATOR
                             && *leftover_type != TokenType::DEDENT) {
                _add_error_at(check_pos,
                             u8"块类型冲突：序列文档中出现非 '-' 条目");
            }
        }
        m_pos = new_pos;
    }

    return {std::move(docs), std::move(m_warnings)};
}

// =========================================================================
// Document-level
// =========================================================================

std::pair<AstNode, int> STMLParser::_parse_document(int pos)
{
    pos = _skip_newlines_from(pos);

    if (pos >= m_n || m_tokens[pos].type == TokenType::END)
        return {AstNode(), pos};

    // Skip any leading DEDENTs
    while (pos < m_n && m_tokens[pos].type == TokenType::DEDENT)
        ++pos;

    if (pos >= m_n || m_tokens[pos].type == TokenType::END)
        return {AstNode(), pos};

    // Handle root-level INDENT
    bool root_indented = false;
    if (m_tokens[pos].type == TokenType::INDENT) {
        root_indented = true;
        ++pos;
    }

    auto [node, new_pos] = _parse_node(pos);

    // Consume root INDENT's matching DEDENT(s)
    while (root_indented && new_pos < m_n && m_tokens[new_pos].type == TokenType::DEDENT)
        ++new_pos;

    return {std::move(node), new_pos};
}

// =========================================================================
// Node dispatch
// =========================================================================

std::pair<AstNode, int> STMLParser::_parse_node(int pos)
{
    pos = _skip_newlines_from(pos);

    if (pos >= m_n)
        return {AstNode(), pos};

    const auto& t = m_tokens[pos];

    if (t.type == TokenType::DASH) {
        auto [lst, new_pos] = _parse_sequence(pos);
        return {AstNode(std::move(lst)), new_pos};
    }

    if (t.type == TokenType::NULL_) {
        // Top-level null scalar
        return {AstNode(), pos + 1};
    }

    if (t.type == TokenType::KEY || t.type == TokenType::BARE_KEY) {
        auto [map, new_pos] = _parse_mapping(pos);
        return {AstNode(std::move(map)), new_pos};
    }

    return {AstNode(), pos};
}

// =========================================================================
// Mapping
// =========================================================================

std::pair<AstMap, int> STMLParser::_parse_mapping(int pos)
{
    AstMap mapping;
    int indent_nesting = 0;

    while (pos < m_n) {
        auto ttype = _peek_type_at(pos);

        // Internal INDENT — increase nesting
        if (ttype == TokenType::INDENT) {
            ++indent_nesting;
            ++pos;
            continue;
        }

        // DEDENT — decrease internal nesting or check for re-indent
        if (ttype == TokenType::DEDENT) {
            if (indent_nesting > 0) {
                --indent_nesting;
                ++pos;
                continue;
            }
            // DEDENT + INDENT = re-indent at same block level
            int next_p = pos + 1;
            while (next_p < m_n && m_tokens[next_p].type == TokenType::NEWLINE)
                ++next_p;
            if (_peek_type_at(next_p) == TokenType::INDENT) {
                pos = next_p + 1; // skip DEDENT, NEWLINEs, INDENT
                continue;
            }
            break;
        }

        // Other block boundaries
        if (ttype == TokenType::END || ttype == TokenType::DOC_SEPARATOR)
            break;

        if (ttype == TokenType::NEWLINE) {
            ++pos;
            continue;
        }

        // Block type conflict: sequence item at mapping level
        if (ttype == TokenType::DASH) {
            _add_warning_at(pos,
                           u8"块类型冲突：映射块中出现序列条目 '-'，终止当前映射");
            break;
        }

        // BARE_KEY
        if (ttype == TokenType::BARE_KEY) {
            const auto& token = m_tokens[pos];
            std::string key = std::get<std::string>(token.value);
            ++pos;
            pos = _consume_newline(pos);
            mapping.emplace_back(std::move(key), AstNode());
            continue;
        }

        // KEY : value
        if (ttype != TokenType::KEY) {
            ++pos; // skip unexpected
            continue;
        }

        std::string key = std::get<std::string>(m_tokens[pos].value);
        ++pos; // consume KEY

        // Expect COLON
        bool colon_ok = false;
        if (pos < m_n && m_tokens[pos].type == TokenType::COLON) {
            colon_ok = true;
            ++pos; // consume COLON
        }

        if (!colon_ok) {
            // Key without colon → treat as bare
            mapping.emplace_back(std::move(key), AstNode());
            pos = _consume_newline(pos);
            continue;
        }

        // Value after COLON
        auto nxt = _peek_type_at(pos);

        if (nxt && VALUE_TOKENS.count(*nxt)) {
            // Inline value
            auto [val, new_pos] = _consume_value(pos);
            mapping.emplace_back(std::move(key), std::move(val));
            pos = _consume_newline(new_pos);

        } else if (nxt == TokenType::NEWLINE) {
            int nl_pos = pos + 1; // after NEWLINE
            auto nxt2 = _peek_type_at(nl_pos);

            if (nxt2 == TokenType::INDENT) {
                // Explicit INDENT block
                pos = nl_pos + 1; // consume NEWLINE + INDENT
                auto [val, new_pos] = _parse_node(pos);
                if (new_pos < m_n && m_tokens[new_pos].type == TokenType::DEDENT)
                    ++new_pos; // consume matching DEDENT
                pos = new_pos;

                // Extend list-values with same-level sequence
                if (val.is_list() && _peek_type_at(pos) == TokenType::DASH) {
                    auto [seq, new_pos2] = _parse_sequence(pos);
                    auto& lst = *val.as_list_mut();
                    lst.insert(lst.end(),
                               std::make_move_iterator(seq.begin()),
                               std::make_move_iterator(seq.end()));
                    pos = new_pos2;
                }
                mapping.emplace_back(std::move(key), std::move(val));

            } else if (nxt2 == TokenType::DASH) {
                // Same-indent sequence as block value
                pos = nl_pos;
                auto [val, new_pos] = _parse_sequence(pos);
                mapping.emplace_back(std::move(key), AstNode(std::move(val)));
                pos = new_pos;

            } else {
                // No block value → null
                mapping.emplace_back(std::move(key), AstNode());
                pos = nl_pos;
            }

        } else if (nxt == TokenType::DASH) {
            // Same-indent DASH immediately after COLON (no NEWLINE)
            auto [val, new_pos] = _parse_sequence(pos);
            mapping.emplace_back(std::move(key), AstNode(std::move(val)));
            pos = new_pos;

        } else {
            // DEDENT, EOF, or other → null
            mapping.emplace_back(std::move(key), AstNode());
        }
    }

    return {std::move(mapping), pos};
}

// =========================================================================
// Sequence
// =========================================================================

std::pair<AstList, int> STMLParser::_parse_sequence(int pos)
{
    bool complex_mode = _is_complex_sequence(pos);
    AstList seq;
    int internal_depth = 0;

    while (pos < m_n) {
        auto ttype = _peek_type_at(pos);

        // Boundaries
        if (ttype == TokenType::END || ttype == TokenType::DOC_SEPARATOR)
            break;

        if (ttype == TokenType::DEDENT) {
            if (internal_depth > 0) {
                --internal_depth;
                ++pos;
                continue;
            }
            // DEDENT at sequence base level — may still continue if followed
            // by INDENT (re-indent) or DASH (next entry after sibling block).
            int next_p = pos + 1;
            while (next_p < m_n && m_tokens[next_p].type == TokenType::NEWLINE)
                ++next_p;
            auto after = _peek_type_at(next_p);
            if (after == TokenType::INDENT) {
                // DEDENT + INDENT = re-indent at same block depth
                pos = next_p + 1;
                continue;
            }
            if (after == TokenType::DASH) {
                // DEDENT + DASH = sibling block ended, next entry follows
                pos = next_p;
                continue;
            }
            break; // parent-level DEDENT, sequence really ends
        }

        if (ttype == TokenType::NEWLINE) {
            ++pos;
            continue;
        }

        if (ttype == TokenType::INDENT) {
            ++internal_depth;
            ++pos;
            continue;
        }

        // Non-DASH → end of sequence or block type conflict
        if (ttype != TokenType::DASH) {
            if (internal_depth > 0) {
                // Inside a sequence indent block: structural conflict (Issue B)
                _add_error_at(pos,
                             u8"块类型冲突：序列块中出现非 '-' 条目，终止当前序列");
            }
            break; // At parent level: end of sequence, let caller continue
        }

        // Parse one sequence entry
        auto [items, new_pos] = _parse_sequence_item(pos, complex_mode);
        for (auto& item : items)
            seq.push_back(std::move(item));
        pos = new_pos;
    }

    return {std::move(seq), pos};
}

std::pair<std::vector<AstNode>, int> STMLParser::_parse_sequence_item(int pos, bool complex_mode)
{
    // Expect DASH
    if (pos >= m_n || m_tokens[pos].type != TokenType::DASH)
        return {{AstNode()}, pos};
    ++pos; // consume DASH

    auto nxt = _peek_type_at(pos);

    // ---- empty entry (DASH, then NEWLINE or nothing) ----
    if (nxt == TokenType::NEWLINE) {
        int newline_pos = pos + 1;
        auto nxt2 = _peek_type_at(newline_pos);

        if (nxt2 == TokenType::INDENT) {
            // Empty entry with sub-block
            pos = newline_pos + 1; // consume NEWLINE + INDENT
            auto [block_val, new_pos] = _parse_node(pos);
            if (new_pos < m_n && m_tokens[new_pos].type == TokenType::DEDENT)
                ++new_pos;
            if (complex_mode) {
                AstMap m;
                m.emplace_back("", std::move(block_val));
                return {{AstNode(std::move(m))}, new_pos};
            } else {
                // Simple mode: promote children of list block (DASH_EMPTY promotion)
                if (block_val.is_list()) {
                    std::vector<AstNode> items;
                    items.push_back(AstNode()); // null for the empty DASH
                    auto& children = *block_val.as_list_mut();
                    for (auto& child : children)
                        items.push_back(std::move(child));
                    return {std::move(items), new_pos};
                }
                return {{std::move(block_val)}, new_pos};
            }
        }
        // Empty entry, no sub-block → null
        return {{AstNode()}, newline_pos};
    }

    // ---- NULL ----
    if (nxt == TokenType::NULL_) {
        auto [val, new_pos] = _consume_value(pos);
        (void)val;
        new_pos = _consume_newline(new_pos);
        return {{AstNode()}, new_pos};
    }

    // ---- INLINE_LIST ----
    if (nxt == TokenType::INLINE_LIST) {
        auto [val, new_pos] = _consume_value(pos);
        new_pos = _consume_newline(new_pos);
        return {{std::move(val)}, new_pos};
    }

    // ---- KEY → inline mapping ----
    if (nxt == TokenType::KEY) {
        std::string key = std::get<std::string>(m_tokens[pos].value);
        ++pos; // consume KEY

        if (pos < m_n && m_tokens[pos].type == TokenType::COLON)
            ++pos; // consume COLON

        auto after = _peek_type_at(pos);

        if (after && VALUE_TOKENS.count(*after)) {
            // Inline value: "- key: value"
            auto [val, new_pos] = _consume_value(pos);
            new_pos = _consume_newline(new_pos);
            AstMap m;
            m.emplace_back(std::move(key), std::move(val));
            // Check for sibling keys at content-indent level
            new_pos = _parse_sibling_map_entries(m, new_pos);
            return {{AstNode(std::move(m))}, new_pos};
        }

        if (after == TokenType::NEWLINE) {
            int newline_pos = pos + 1;
            auto after2 = _peek_type_at(newline_pos);
            if (after2 == TokenType::INDENT) {
                // Check INDENT diff to distinguish sibling from child block
                const auto& indent_tok = m_tokens[newline_pos];
                int diff = std::stoi(std::get<std::string>(indent_tok.value));
                if (diff == 2) {
                    // Content-indent level: could be sibling keys or block value.
                    // Peek past NEWLINE + INDENT to decide.
                    int after_indent = _skip_newlines_from(newline_pos + 1);
                    auto after_indent_type = _peek_type_at(after_indent);

                    if (after_indent_type == TokenType::KEY ||
                        after_indent_type == TokenType::BARE_KEY) {
                        // Sibling keys at content-indent level
                        AstMap m;
                        m.emplace_back(std::move(key), AstNode());
                        int sib_pos = _parse_sibling_map_entries(m, newline_pos);
                        return {{AstNode(std::move(m))}, sib_pos};
                    }

                    // Block value (DASH, SCALAR, NULL, etc.)
                    pos = after_indent;
                    auto [block_val, new_pos] = _parse_node(pos);
                    if (new_pos < m_n && m_tokens[new_pos].type == TokenType::DEDENT)
                        ++new_pos;
                    AstMap m;
                    m.emplace_back(std::move(key), std::move(block_val));
                    return {{AstNode(std::move(m))}, new_pos};
                }
                // Block value (diff > 2)
                pos = newline_pos + 1; // consume NEWLINE + INDENT
                auto [block_val, new_pos] = _parse_node(pos);
                if (new_pos < m_n && m_tokens[new_pos].type == TokenType::DEDENT)
                    ++new_pos;
                AstMap m;
                m.emplace_back(std::move(key), std::move(block_val));
                return {{AstNode(std::move(m))}, new_pos};
            } else if (after2 == TokenType::DASH) {
                // Same-indent sequence as block value
                pos = newline_pos;
                auto [block_val, new_pos] = _parse_sequence(pos);
                AstMap m;
                m.emplace_back(std::move(key), AstNode(std::move(block_val)));
                return {{AstNode(std::move(m))}, new_pos};
            } else {
                // No block, no inline → null
                AstMap m;
                m.emplace_back(std::move(key), AstNode());
                return {{AstNode(std::move(m))}, newline_pos};
            }
        }

        // DEDENT, EOF, etc.
        AstMap m;
        m.emplace_back(std::move(key), AstNode());
        return {{AstNode(std::move(m))}, pos};
    }

    // ---- BARE_KEY ----
    if (nxt == TokenType::BARE_KEY) {
        std::string key = std::get<std::string>(m_tokens[pos].value);
        ++pos; // consume BARE_KEY
        pos = _consume_newline(pos);
        if (complex_mode) {
            AstMap m;
            m.emplace_back(std::move(key), AstNode());
            pos = _parse_sibling_map_entries(m, pos);
            return {{AstNode(std::move(m))}, pos};
        } else {
            return {{AstNode(std::move(key))}, pos};
        }
    }

    // ---- SCALAR, RAW_STRING ----
    if (nxt == TokenType::SCALAR || nxt == TokenType::RAW_STRING) {
        auto [val, new_pos] = _consume_value(pos);
        new_pos = _consume_newline(new_pos);
        if (complex_mode) {
            AstMap m;
            m.emplace_back(std::move(*val.as_string_mut()), AstNode());
            new_pos = _parse_sibling_map_entries(m, new_pos);
            return {{AstNode(std::move(m))}, new_pos};
        } else {
            return {{std::move(val)}, new_pos};
        }
    }

    // ---- MULTILINE_STRING ----
    if (nxt == TokenType::MULTILINE_STRING) {
        auto [val, new_pos] = _consume_value(pos);
        new_pos = _consume_newline(new_pos);
        if (complex_mode) {
            AstMap m;
            m.emplace_back(std::move(*val.as_string_mut()), AstNode());
            new_pos = _parse_sibling_map_entries(m, new_pos);
            return {{AstNode(std::move(m))}, new_pos};
        } else {
            return {{std::move(val)}, new_pos};
        }
    }

    // Fallback
    return {{AstNode()}, pos};
}

// =========================================================================
// Sibling map entries (multi-key sequence entry)
// =========================================================================

int STMLParser::_parse_sibling_map_entries(AstMap& map, int pos)
{
    while (pos < m_n) {
        pos = _skip_newlines_from(pos);

        auto peek = _peek_type_at(pos);
        int key_pos = pos;

        if (peek == TokenType::INDENT) {
            // Accept any INDENT whose next token is a key (lenient)
            int candidate = pos + 1;
            auto after_indent = _peek_type_at(candidate);
            if (after_indent == TokenType::KEY ||
                after_indent == TokenType::BARE_KEY) {
                key_pos = candidate;
            } else {
                break; // Not a key → not a sibling
            }
        } else {
            break;
        }

        auto key_type = _peek_type_at(key_pos);

        if (key_type == TokenType::BARE_KEY) {
            std::string sibling_key = std::get<std::string>(m_tokens[key_pos].value);
            ++key_pos;
            key_pos = _consume_newline(key_pos);
            map.emplace_back(std::move(sibling_key), AstNode());
            pos = key_pos;
            continue;
        }

        if (key_type != TokenType::KEY) break;

        // Parse KEY: value for sibling
        std::string sibling_key = std::get<std::string>(m_tokens[key_pos].value);
        ++key_pos; // consume KEY

        if (key_pos < m_n && m_tokens[key_pos].type == TokenType::COLON)
            ++key_pos; // consume COLON

        auto after_colon = _peek_type_at(key_pos);

        if (after_colon && VALUE_TOKENS.count(*after_colon)) {
            // Inline value
            auto [val, new_pos] = _consume_value(key_pos);
            new_pos = _consume_newline(new_pos);
            map.emplace_back(std::move(sibling_key), std::move(val));
            pos = new_pos;
        } else if (after_colon == TokenType::NEWLINE) {
            int nl_pos = key_pos + 1;
            auto after_nl = _peek_type_at(nl_pos);
            if (after_nl == TokenType::INDENT) {
                // Block value (deeper indent from sibling level)
                int value_pos = nl_pos + 1; // skip NEWLINE + INDENT
                auto [block_val, block_end] = _parse_node(value_pos);
                if (block_end < m_n && m_tokens[block_end].type == TokenType::DEDENT)
                    ++block_end;
                map.emplace_back(std::move(sibling_key), std::move(block_val));
                pos = block_end;
            } else if (after_nl == TokenType::DASH) {
                // Same-indent sequence as block value
                auto [seq_val, seq_end] = _parse_sequence(nl_pos);
                map.emplace_back(std::move(sibling_key), AstNode(std::move(seq_val)));
                pos = seq_end;
            } else {
                // No block value → null
                map.emplace_back(std::move(sibling_key), AstNode());
                pos = nl_pos;
            }
        } else {
            // DEDENT, EOF, or other → null
            map.emplace_back(std::move(sibling_key), AstNode());
            pos = key_pos;
        }
    }
    return pos;
}

// =========================================================================
// Complex-sequence pre-scan
// =========================================================================

bool STMLParser::_is_complex_sequence(int start_pos) const
{
    int pos = start_pos;
    int depth = 0;

    while (pos < m_n) {
        const auto& t = m_tokens[pos];

        if (t.type == TokenType::INDENT) {
            ++depth;
            ++pos;
            continue;
        }

        if (t.type == TokenType::DEDENT) {
            if (depth > 0) {
                --depth;
                ++pos;
                continue;
            }
            break;
        }

        if (t.type == TokenType::END || t.type == TokenType::DOC_SEPARATOR)
            break;

        // BARE_KEY at depth>0 (inside sub-block) → complex mode (sibling keys)
        // BARE_KEY at depth==0 → could be irregular sibling at DASH indent
        if (t.type == TokenType::BARE_KEY) {
            if (depth > 0) return true;
            // Lenient: scan ahead for another DASH (irregular indentation).
            // If a DASH follows the BARE_KEY(s), the sequence continues →
            // this BARE_KEY is a sibling in a complex entry.
            int scan = pos + 1;
            int scan_depth = 0;
            while (scan < m_n) {
                auto st = m_tokens[scan].type;
                if (st == TokenType::NEWLINE) { ++scan; continue; }
                if (st == TokenType::INDENT) { ++scan_depth; ++scan; continue; }
                if (st == TokenType::DEDENT) {
                    if (scan_depth > 0) { --scan_depth; ++scan; continue; }
                    break;
                }
                if (st == TokenType::DASH) return true;
                if (st == TokenType::BARE_KEY || st == TokenType::KEY ||
                    st == TokenType::SCALAR || st == TokenType::MULTILINE_STRING ||
                    st == TokenType::INLINE_LIST || st == TokenType::NULL_ ||
                    st == TokenType::RAW_STRING) {
                    ++scan;
                    continue;
                }
                break;
            }
            break;
        }
        if (t.type == TokenType::KEY) {
            // KEY at depth>0 → sibling key in sub-block → complex
            // KEY at depth==0 → parent-level key, not part of sequence → end scan
            if (depth > 0) return true;
            break;
        }

        if (t.type == TokenType::DASH) {
            // Examine token after DASH (skip NEWLINEs)
            int scan = pos + 1;
            while (scan < m_n) {
                auto st = m_tokens[scan].type;
                if (st == TokenType::NEWLINE) {
                    ++scan;
                    continue;
                }
                if (st == TokenType::KEY) return true;   // DASH + KEY → inline mapping
                if (st == TokenType::INDENT) {
                    // DASH + INDENT → could be nested list or sub-block.
                    // Peek past NEWLINE+INDENT: if next is DASH → nested list, not complex.
                    int after_indent = scan + 1;
                    while (after_indent < m_n && m_tokens[after_indent].type == TokenType::NEWLINE)
                        ++after_indent;
                    auto st2 = _peek_type_at(after_indent);
                    if (st2 == TokenType::KEY || st2 == TokenType::BARE_KEY)
                        return true; // sub-block with keys → complex
                    // DASH or value after INDENT → nested list / simple sub-block
                }
                break;
            }
            pos = scan; // advance past this entry
            continue;
        }

        // Skip any other token
        ++pos;
    }

    return false;
}

// =========================================================================
// Helpers
// =========================================================================

std::optional<TokenType> STMLParser::_peek_type() const
{
    if (m_pos >= m_n) return std::nullopt;
    return m_tokens[m_pos].type;
}

std::optional<TokenType> STMLParser::_peek_type_at(int pos) const
{
    if (pos >= m_n) return std::nullopt;
    return m_tokens[pos].type;
}

void STMLParser::_skip_newlines()
{
    while (m_pos < m_n && m_tokens[m_pos].type == TokenType::NEWLINE)
        ++m_pos;
}

int STMLParser::_skip_newlines_from(int pos) const
{
    while (pos < m_n && m_tokens[pos].type == TokenType::NEWLINE)
        ++pos;
    return pos;
}

int STMLParser::_consume_newline(int pos) const
{
    if (pos < m_n && m_tokens[pos].type == TokenType::NEWLINE)
        return pos + 1;
    return pos;
}

std::pair<AstNode, int> STMLParser::_consume_value(int pos)
{
    if (pos >= m_n) return {AstNode(), pos};

    const auto& t = m_tokens[pos];
    if (t.type == TokenType::SCALAR)
        return {AstNode(std::get<std::string>(t.value)), pos + 1};
    if (t.type == TokenType::NULL_)
        return {AstNode(), pos + 1};
    if (t.type == TokenType::RAW_STRING)
        return {AstNode(std::get<std::string>(t.value)), pos + 1};
    if (t.type == TokenType::INLINE_LIST) {
        // Inline list elements → AST list
        AstList list;
        for (const auto& elem : std::get<std::vector<InlineElem>>(t.value)) {
            if (elem.has_value())
                list.push_back(AstNode(*elem));
            else
                list.push_back(AstNode());
        }
        return {AstNode(std::move(list)), pos + 1};
    }
    if (t.type == TokenType::MULTILINE_STRING)
        return {AstNode(std::get<std::string>(t.value)), pos + 1};

    return {AstNode(), pos};
}

void STMLParser::_add_warning_at(int pos, const std::string& message)
{
    if (pos < m_n) {
        const auto& t = m_tokens[pos];
        m_warnings.emplace_back(t.line, t.column, message);
    } else {
        m_warnings.emplace_back(0, 0, message);
    }
}

void STMLParser::_add_error_at(int pos, const std::string& message)
{
    if (pos < m_n) {
        const auto& t = m_tokens[pos];
        throw ParseError(t.line, t.column, message);
    } else {
        throw ParseError(0, 0, message);
    }
}

} // namespace stml
