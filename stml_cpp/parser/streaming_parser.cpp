#include "parser/streaming_parser.h"

#include <set>
#include <string>

namespace stml {

// Token types that represent a value (may appear after COLON or DASH)
const std::set<TokenType> StreamingParser::VALUE_TOKENS = {
    TokenType::SCALAR, TokenType::NULL_, TokenType::RAW_STRING,
    TokenType::INLINE_LIST, TokenType::MULTILINE_STRING,
};

// =========================================================================
// Construction
// =========================================================================

StreamingParser::StreamingParser()
    : m_pos(0)
{
}

// =========================================================================
// Public API
// =========================================================================

void StreamingParser::feed(std::vector<Token> tokens)
{
    m_buffer.insert(m_buffer.end(),
                    std::make_move_iterator(tokens.begin()),
                    std::make_move_iterator(tokens.end()));
    _try_parse();
}

AstList StreamingParser::finalize()
{
    // Force-resolve any deferred scan (at EOF, no more tokens coming)
    m_deferred_seq = std::nullopt;

    // Parse remaining tokens
    _try_parse();

    return std::move(m_docs);
}

// =========================================================================
// Incremental parse loop
// =========================================================================

void StreamingParser::_try_parse()
{
    int n = static_cast<int>(m_buffer.size());

    // If buffer ends with tokens that need more context, defer parsing
    // to avoid assigning null prematurely or getting stuck.
    if (_buffer_incomplete()) {
        return;
    }

    // Resume deferred complex-sequence scan if active
    if (m_deferred_seq.has_value()) {
        auto& ds = *m_deferred_seq;
        auto result = _is_complex_sequence(ds.start_pos);
        if (!result.has_value()) {
            return;
        }
        m_deferred_seq = std::nullopt;
    }

    while (m_pos < n) {
        m_pos = _skip_newlines_from(m_pos);

        if (m_pos >= n) break;

        auto ttype = _peek_type_at(m_pos);
        if (!ttype) break;

        if (*ttype == TokenType::END) break;

        // Consume trailing DEDENTs
        if (*ttype == TokenType::DEDENT) {
            ++m_pos;
            continue;
        }

        // Consume DOC_SEPARATOR between documents
        if (*ttype == TokenType::DOC_SEPARATOR) {
            // Leading separator (before any doc) → null doc
            if (m_docs.empty()) {
                m_docs.push_back(AstNode());
            }
            ++m_pos;
            m_pos = _skip_newlines_from(m_pos);
            // Trailing separator at EOF → null doc
            auto next_type = _peek_type_at(m_pos);
            if (!next_type || *next_type == TokenType::END) {
                m_docs.push_back(AstNode());
            } else if (*next_type == TokenType::DOC_SEPARATOR) {
                // Consecutive separators create a null document in between
                m_docs.push_back(AstNode());
            }
            continue;
        }

        // Parse one document — only save if it made progress and isn't partial.
        // A partial result returns pos unchanged, indicating more tokens needed.
        int saved_pos = m_pos;
        auto [node, new_pos] = _parse_document(m_pos);

        if (new_pos <= saved_pos) {
            // Parser couldn't advance — either incomplete buffer or unexpected token.
            // Skip one token to avoid infinite loop, but DON'T save a partial document.
            ++m_pos;
            // Reset doc tracking since we didn't actually complete a document
            continue;
        }

        // Issue A: top-level mixing — after a sequence document (root-level
        // DASH items), leftover non-DASH content is a structural conflict.
        int check_pos = _skip_newlines_from(new_pos);
        if (node.is_list() && check_pos < n) {
            auto leftover_type = _peek_type_at(check_pos);
            if (leftover_type && *leftover_type != TokenType::END
                             && *leftover_type != TokenType::DOC_SEPARATOR
                             && *leftover_type != TokenType::DEDENT) {
                _add_error_at(check_pos,
                             u8"块类型冲突：序列文档中出现非 '-' 条目");
            }
        }
        m_pos = new_pos;
        m_docs.push_back(std::move(node));
    }
}

// =========================================================================
// Document-level
// =========================================================================

std::pair<AstNode, int> StreamingParser::_parse_document(int pos)
{
    pos = _skip_newlines_from(pos);

    int n = static_cast<int>(m_buffer.size());
    if (pos >= n || m_buffer[pos].type == TokenType::END)
        return {AstNode(), pos};

    // Skip any leading DEDENTs
    while (pos < n && m_buffer[pos].type == TokenType::DEDENT)
        ++pos;

    if (pos >= n || m_buffer[pos].type == TokenType::END)
        return {AstNode(), pos};

    // Handle root-level INDENT
    bool root_indented = false;
    if (m_buffer[pos].type == TokenType::INDENT) {
        root_indented = true;
        ++pos;
    }

    auto [node, new_pos] = _parse_node(pos);

    // Consume root INDENT's matching DEDENT(s)
    while (root_indented && new_pos < n && m_buffer[new_pos].type == TokenType::DEDENT)
        ++new_pos;

    return {std::move(node), new_pos};
}

// =========================================================================
// Node dispatch
// =========================================================================

std::pair<AstNode, int> StreamingParser::_parse_node(int pos)
{
    pos = _skip_newlines_from(pos);

    int n = static_cast<int>(m_buffer.size());
    if (pos >= n)
        return {AstNode(), pos};

    const auto& t = m_buffer[pos];

    if (t.type == TokenType::DASH) {
        auto [lst, new_pos] = _parse_sequence(pos);
        return {AstNode(std::move(lst)), new_pos};
    }

    if (t.type == TokenType::NULL_) {
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

std::pair<AstMap, int> StreamingParser::_parse_mapping(int pos)
{
    AstMap mapping;
    int indent_nesting = 0;
    int n = static_cast<int>(m_buffer.size());

    while (pos < n) {
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
            while (next_p < n && m_buffer[next_p].type == TokenType::NEWLINE)
                ++next_p;
            if (_peek_type_at(next_p) == TokenType::INDENT) {
                pos = next_p + 1;
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
            const auto& token = m_buffer[pos];
            std::string key = std::get<std::string>(token.value);
            ++pos;
            pos = _consume_newline(pos);
            mapping.emplace_back(std::move(key), AstNode());
            continue;
        }

        // KEY : value
        if (ttype != TokenType::KEY) {
            ++pos;
            continue;
        }

        std::string key = std::get<std::string>(m_buffer[pos].value);
        ++pos; // consume KEY

        // Expect COLON
        bool colon_ok = false;
        if (pos < n && m_buffer[pos].type == TokenType::COLON) {
            colon_ok = true;
            ++pos; // consume COLON
        }

        if (!colon_ok) {
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
            int nl_pos = pos + 1;
            auto nxt2 = _peek_type_at(nl_pos);

            if (!nxt2.has_value()) {
                // Buffer ends after NEWLINE → return early, don't assign null
                return {std::move(mapping), pos};
            } else if (nxt2 == TokenType::INDENT) {
                // Explicit INDENT block
                pos = nl_pos + 1;
                auto [val, new_pos] = _parse_node(pos);
                if (new_pos < n && m_buffer[new_pos].type == TokenType::DEDENT)
                    ++new_pos;
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

        } else if (nxt == TokenType::DEDENT || nxt == TokenType::END ||
                   nxt == TokenType::DOC_SEPARATOR) {
            // Block boundary → null value
            mapping.emplace_back(std::move(key), AstNode());
        } else if (!nxt.has_value()) {
            // Buffer ended right after COLON → don't assign null yet,
            // return with pos at COLON so parsing resumes correctly.
            return {std::move(mapping), pos};
        } else {
            // Unknown token → null with warning
            _add_warning_at(pos, u8"映射键 '" + key + u8"' 后出现意外的 token，设为 null");
            mapping.emplace_back(std::move(key), AstNode());
        }
    }

    return {std::move(mapping), pos};
}

// =========================================================================
// Sequence
// =========================================================================

std::pair<AstList, int> StreamingParser::_parse_sequence(int pos)
{
    // Determine complex mode.
    // _has_dangling_dash() should prevent reaching here with an unresolved
    // scan, but default to simple as a safe fallback.
    auto complex_result = _is_complex_sequence(pos);
    bool complex_mode = complex_result.value_or(false);

    AstList seq;
    int internal_depth = 0;
    int n = static_cast<int>(m_buffer.size());

    while (pos < n) {
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
            while (next_p < n && m_buffer[next_p].type == TokenType::NEWLINE)
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
            break;
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
        auto [item, new_pos] = _parse_sequence_item(pos, complex_mode);
        seq.push_back(std::move(item));
        pos = new_pos;
    }

    return {std::move(seq), pos};
}

std::pair<AstNode, int> StreamingParser::_parse_sequence_item(int pos, bool complex_mode)
{
    int n = static_cast<int>(m_buffer.size());

    // Expect DASH
    if (pos >= n || m_buffer[pos].type != TokenType::DASH)
        return {AstNode(), pos};
    ++pos; // consume DASH

    auto nxt = _peek_type_at(pos);

    // ---- empty entry (DASH, then NEWLINE or nothing) ----
    if (nxt == TokenType::NEWLINE) {
        int newline_pos = pos + 1;
        auto nxt2 = _peek_type_at(newline_pos);

        if (!nxt2.has_value()) {
            // Buffer ends — can't determine yet
            return {AstNode(), pos}; // return pos unchanged (still at DASH+1)
        } else if (nxt2 == TokenType::INDENT) {
            // Empty entry with sub-block
            pos = newline_pos + 1;
            auto [block_val, new_pos] = _parse_node(pos);
            if (new_pos < n && m_buffer[new_pos].type == TokenType::DEDENT)
                ++new_pos;
            if (complex_mode) {
                AstMap m;
                m.emplace_back("", std::move(block_val));
                return {AstNode(std::move(m)), new_pos};
            } else {
                return {std::move(block_val), new_pos};
            }
        }
        // Empty entry, no sub-block → null
        return {AstNode(), newline_pos};
    }

    // ---- NULL ----
    if (nxt == TokenType::NULL_) {
        auto [val, new_pos] = _consume_value(pos);
        (void)val;
        new_pos = _consume_newline(new_pos);
        return {AstNode(), new_pos};
    }

    // ---- INLINE_LIST ----
    if (nxt == TokenType::INLINE_LIST) {
        auto [val, new_pos] = _consume_value(pos);
        new_pos = _consume_newline(new_pos);
        return {std::move(val), new_pos};
    }

    // ---- KEY → inline mapping ----
    if (nxt == TokenType::KEY) {
        std::string key = std::get<std::string>(m_buffer[pos].value);
        ++pos; // consume KEY

        if (pos < n && m_buffer[pos].type == TokenType::COLON)
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
            return {AstNode(std::move(m)), new_pos};
        }

        if (after == TokenType::NEWLINE) {
            int newline_pos = pos + 1;
            auto after2 = _peek_type_at(newline_pos);
            if (!after2.has_value()) {
                // Buffer ends — return early
                AstMap m;
                m.emplace_back(std::move(key), AstNode());
                return {AstNode(std::move(m)), pos}; // stay at pos before NEWLINE
            } else if (after2 == TokenType::INDENT) {
                // Check INDENT diff to distinguish sibling from child block
                const auto& indent_tok = m_buffer[newline_pos];
                int diff = std::stoi(std::get<std::string>(indent_tok.value));
                if (diff == 2) {
                    // Content-indent level: could be sibling keys or block value.
                    // Peek past NEWLINE + INDENT to decide.
                    int after_indent = _skip_newlines_from(newline_pos + 1);
                    auto after_indent_type = _peek_type_at(after_indent);

                    // Need buffer guard for peek
                    if (!after_indent_type.has_value()) {
                        AstMap m;
                        m.emplace_back(std::move(key), AstNode());
                        return {AstNode(std::move(m)), pos};
                    }

                    if (after_indent_type == TokenType::KEY ||
                        after_indent_type == TokenType::BARE_KEY) {
                        // Sibling keys at content-indent level
                        AstMap m;
                        m.emplace_back(std::move(key), AstNode());
                        int sib_pos = _parse_sibling_map_entries(m, newline_pos);
                        return {AstNode(std::move(m)), sib_pos};
                    }

                    // Block value (DASH, SCALAR, NULL, etc.)
                    pos = after_indent;
                    auto [block_val, new_pos] = _parse_node(pos);
                    if (new_pos < n && m_buffer[new_pos].type == TokenType::DEDENT)
                        ++new_pos;
                    AstMap m;
                    m.emplace_back(std::move(key), std::move(block_val));
                    return {AstNode(std::move(m)), new_pos};
                }
                // Block value (diff > 2)
                pos = newline_pos + 1;
                auto [block_val, new_pos] = _parse_node(pos);
                if (new_pos < n && m_buffer[new_pos].type == TokenType::DEDENT)
                    ++new_pos;
                AstMap m;
                m.emplace_back(std::move(key), std::move(block_val));
                return {AstNode(std::move(m)), new_pos};
            } else if (after2 == TokenType::DASH) {
                // Same-indent sequence as block value
                pos = newline_pos;
                auto [block_val, new_pos] = _parse_sequence(pos);
                AstMap m;
                m.emplace_back(std::move(key), AstNode(std::move(block_val)));
                return {AstNode(std::move(m)), new_pos};
            } else {
                // No block, no inline → null
                AstMap m;
                m.emplace_back(std::move(key), AstNode());
                return {AstNode(std::move(m)), newline_pos};
            }
        }

        // nullopt after colon → return early without assigning
        if (!after.has_value()) {
            AstMap m;
            m.emplace_back(std::move(key), AstNode());
            return {AstNode(std::move(m)), pos}; // stay at pos (after COLON)
        }

        // DEDENT, EOF, etc.
        AstMap m;
        m.emplace_back(std::move(key), AstNode());
        return {AstNode(std::move(m)), pos};
    }

    // ---- SCALAR, RAW_STRING ----
    if (nxt == TokenType::SCALAR || nxt == TokenType::RAW_STRING) {
        auto [val, new_pos] = _consume_value(pos);
        new_pos = _consume_newline(new_pos);
        if (complex_mode) {
            AstMap m;
            m.emplace_back(std::move(*val.as_string_mut()), AstNode());
            new_pos = _parse_sibling_map_entries(m, new_pos);
            return {AstNode(std::move(m)), new_pos};
        } else {
            return {std::move(val), new_pos};
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
            return {AstNode(std::move(m)), new_pos};
        } else {
            return {std::move(val), new_pos};
        }
    }

    // Fallback
    return {AstNode(), pos};
}

// =========================================================================
// Sibling map entries (multi-key sequence entry)
// =========================================================================

int StreamingParser::_parse_sibling_map_entries(AstMap& map, int pos)
{
    int n = static_cast<int>(m_buffer.size());
    // No in_sibling_level bool needed — we accept KEY/BARE_KEY in all iterations

    while (pos < n) {
        pos = _skip_newlines_from(pos);

        auto peek = _peek_type_at(pos);
        int key_pos = pos;

        if (peek == TokenType::INDENT) {
            // Accept any INDENT whose next token is a key (lenient)
            int candidate = pos + 1;
            auto after_indent = _peek_type_at(candidate);
            if (!after_indent.has_value()) return pos; // buffer-end
            if (after_indent == TokenType::KEY ||
                after_indent == TokenType::BARE_KEY) {
                key_pos = candidate;
            } else {
                break; // Not a key → not a sibling
            }
        } else if (peek == TokenType::KEY || peek == TokenType::BARE_KEY) {
            // Sibling at same indent level (no INDENT needed).
            // Covers both regular subsequent siblings and irregular first sibling.
        } else {
            break;
        }

        auto key_type = _peek_type_at(key_pos);

        // Buffer-end guard: need token at key position
        if (!key_type.has_value()) return pos;

        if (key_type == TokenType::BARE_KEY) {
            std::string sibling_key = std::get<std::string>(m_buffer[key_pos].value);
            ++key_pos;
            key_pos = _consume_newline(key_pos);
            map.emplace_back(std::move(sibling_key), AstNode());
            pos = key_pos;
            continue;
        }

        if (key_type != TokenType::KEY) break;

        // Parse KEY: value for sibling
        std::string sibling_key = std::get<std::string>(m_buffer[key_pos].value);
        ++key_pos; // consume KEY

        if (key_pos < n && m_buffer[key_pos].type == TokenType::COLON)
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
            if (!after_nl.has_value()) {
                // Buffer ends — return early, don't assign null
                return pos;
            }
            if (after_nl == TokenType::INDENT) {
                // Block value (deeper indent from sibling level)
                int value_pos = nl_pos + 1; // skip NEWLINE + INDENT
                auto [block_val, block_end] = _parse_node(value_pos);
                if (block_end < n && m_buffer[block_end].type == TokenType::DEDENT)
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
        } else if (!after_colon.has_value()) {
            // Buffer ended after COLON → return early
            return pos;
        } else {
            // DEDENT, EOF, or other → null
            map.emplace_back(std::move(sibling_key), AstNode());
            pos = key_pos;
        }
    }
    return pos;
}

// =========================================================================
// Complex-sequence pre-scan (incremental)
// =========================================================================

std::optional<bool> StreamingParser::_is_complex_sequence(int start_pos) const
{
    int pos = start_pos;
    int depth = 0;
    int n = static_cast<int>(m_buffer.size());

    while (pos < n) {
        const auto& t = m_buffer[pos];

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
            int scan = pos + 1;
            int scan_depth = 0;
            while (scan < n) {
                auto st = m_buffer[scan].type;
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
            while (scan < n) {
                auto st = m_buffer[scan].type;
                if (st == TokenType::NEWLINE) {
                    ++scan;
                    continue;
                }
                if (st == TokenType::KEY) return true;   // DASH + KEY → complex
                if (st == TokenType::INDENT) return true; // DASH + INDENT → complex
                break; // non-KEY/non-INDENT → simple entry
            }
            // Reached end of buffer while skipping NEWLINEs after DASH
            // → don't know what follows → need more tokens
            if (scan >= n) {
                return std::nullopt;
            }
            pos = scan;
            continue;
        }

        // Skip any other token
        ++pos;
    }

    return false;
}

// =========================================================================
// Buffer-incomplete detection
// =========================================================================

bool StreamingParser::_buffer_incomplete() const
{
    // Defer all parsing until the lexer has finalized (END token received).
    // Parsing incrementally is unsafe because arbitrary chunk boundaries can
    // split blocks mid-document, causing premature document finalization.
    // By deferring, we guarantee streaming == batch results.
    int n = static_cast<int>(m_buffer.size());
    if (n == 0) return false;

    for (int i = m_pos; i < n; ++i) {
        if (m_buffer[i].type == TokenType::END)
            return false; // END present → lexer finalized → safe to parse
    }
    return true; // No END → defer
}

// =========================================================================
// Helpers
// =========================================================================

std::optional<TokenType> StreamingParser::_peek_type_at(int pos) const
{
    if (pos >= static_cast<int>(m_buffer.size()) || pos < 0) return std::nullopt;
    return m_buffer[pos].type;
}

int StreamingParser::_skip_newlines_from(int pos) const
{
    int n = static_cast<int>(m_buffer.size());
    while (pos < n && m_buffer[pos].type == TokenType::NEWLINE)
        ++pos;
    return pos;
}

int StreamingParser::_consume_newline(int pos) const
{
    int n = static_cast<int>(m_buffer.size());
    if (pos < n && m_buffer[pos].type == TokenType::NEWLINE)
        return pos + 1;
    return pos;
}

std::pair<AstNode, int> StreamingParser::_consume_value(int pos)
{
    int n = static_cast<int>(m_buffer.size());
    if (pos >= n) return {AstNode(), pos};

    const auto& t = m_buffer[pos];
    if (t.type == TokenType::SCALAR)
        return {AstNode(std::get<std::string>(t.value)), pos + 1};
    if (t.type == TokenType::NULL_)
        return {AstNode(), pos + 1};
    if (t.type == TokenType::RAW_STRING)
        return {AstNode(std::get<std::string>(t.value)), pos + 1};
    if (t.type == TokenType::INLINE_LIST) {
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

void StreamingParser::_add_warning_at(int pos, const std::string& message)
{
    int n = static_cast<int>(m_buffer.size());
    if (pos < n) {
        const auto& t = m_buffer[pos];
        m_warnings.emplace_back(t.line, t.column, message);
    } else {
        m_warnings.emplace_back(0, 0, message);
    }
}

void StreamingParser::_add_error_at(int pos, const std::string& message)
{
    int n = static_cast<int>(m_buffer.size());
    if (pos < n) {
        const auto& t = m_buffer[pos];
        throw ParseError(t.line, t.column, message);
    } else {
        throw ParseError(0, 0, message);
    }
}

} // namespace stml
