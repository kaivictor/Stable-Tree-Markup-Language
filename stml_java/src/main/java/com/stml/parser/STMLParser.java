package com.stml.parser;

import com.stml.ast.*;
import com.stml.diagnostics.ParseError;
import com.stml.diagnostics.Warning;
import com.stml.lexer.InlineElem;
import com.stml.lexer.Token;
import com.stml.lexer.TokenType;

import java.util.*;

/**
 * STMLParser — recursive-descent parser operating on a flat token list.
 * INDENT / DEDENT tokens drive block nesting. The parser never computes
 * indentation values — it trusts the lexer's INDENT/DEDENT pairing.
 */
public class STMLParser {

    private static final Set<TokenType> VALUE_TOKENS = EnumSet.of(
            TokenType.SCALAR, TokenType.NULL_, TokenType.RAW_STRING,
            TokenType.INLINE_LIST, TokenType.MULTILINE_STRING
    );

    private final List<Token> tokens;
    private final int n;
    private int pos;
    private final List<Warning> warnings = new ArrayList<>();

    public STMLParser(List<Token> tokens) {
        this.tokens = tokens;
        this.n = tokens.size();
        this.pos = 0;
    }

    // =========================================================================
    // Public API
    // =========================================================================

    public ParseResult parse() {
        pos = 0;
        warnings.clear();
        List<AstNode> docs = new ArrayList<>();

        while (pos < n) {
            skipNewlines();

            if (pos >= n) break;

            var ttype = peekType();
            if (ttype.isEmpty()) break;

            if (ttype.get() == TokenType.END) break;

            // Consume trailing DEDENTs (remaining indent stack after last doc)
            if (ttype.get() == TokenType.DEDENT) {
                pos++;
                continue;
            }

            // Consume DOC_SEPARATOR between documents
            if (ttype.get() == TokenType.DOC_SEPARATOR) {
                // Leading separator (before any doc) → null doc
                if (docs.isEmpty()) {
                    docs.add(NullNode.INSTANCE);
                }
                pos++;
                skipNewlines();
                // Trailing separator at EOF → null doc
                var nextType = peekType();
                if (nextType.isEmpty() || nextType.get() == TokenType.END) {
                    docs.add(NullNode.INSTANCE);
                } else if (nextType.get() == TokenType.DOC_SEPARATOR) {
                    // Consecutive separators create a null document in between
                    docs.add(NullNode.INSTANCE);
                }
                continue;
            }

            // Parse one document
            var result = parseDocument(pos);
            AstNode node = result.node();
            int newPos = result.newPos();
            docs.add(node);

            // Issue A: top-level mixing
            int checkPos = skipNewlinesFrom(newPos);
            if (node.isList() && checkPos < n) {
                var leftoverType = peekTypeAt(checkPos);
                if (leftoverType.isPresent()
                        && leftoverType.get() != TokenType.END
                        && leftoverType.get() != TokenType.DOC_SEPARATOR
                        && leftoverType.get() != TokenType.DEDENT) {
                    addErrorAt(checkPos, "块类型冲突：序列文档中出现非 '-' 条目");
                }
            }
            pos = newPos;
        }

        return new ParseResult(docs, new ArrayList<>(warnings));
    }

    public List<Warning> getWarnings() {
        return warnings;
    }

    public record ParseResult(List<AstNode> docs, List<Warning> warnings) {}

    // =========================================================================
    // Document-level
    // =========================================================================

    private ParseNodeResult parseDocument(int pos) {
        pos = skipNewlinesFrom(pos);

        if (pos >= n || tokens.get(pos).type() == TokenType.END)
            return new ParseNodeResult(NullNode.INSTANCE, pos);

        // Skip any leading DEDENTs
        while (pos < n && tokens.get(pos).type() == TokenType.DEDENT)
            pos++;

        if (pos >= n || tokens.get(pos).type() == TokenType.END)
            return new ParseNodeResult(NullNode.INSTANCE, pos);

        // Handle root-level INDENT
        boolean rootIndented = false;
        if (tokens.get(pos).type() == TokenType.INDENT) {
            rootIndented = true;
            pos++;
        }

        var result = parseNode(pos);
        AstNode node = result.node();
        int newPos = result.newPos();

        // Consume root INDENT's matching DEDENT(s)
        while (rootIndented && newPos < n && tokens.get(newPos).type() == TokenType.DEDENT)
            newPos++;

        return new ParseNodeResult(node, newPos);
    }

    // =========================================================================
    // Node dispatch
    // =========================================================================

    private ParseNodeResult parseNode(int pos) {
        pos = skipNewlinesFrom(pos);

        if (pos >= n)
            return new ParseNodeResult(NullNode.INSTANCE, pos);

        Token t = tokens.get(pos);

        if (t.type() == TokenType.DASH) {
            var result = parseSequence(pos);
            return new ParseNodeResult(new AstList(result.list()), result.newPos());
        }

        if (t.type() == TokenType.NULL_) {
            return new ParseNodeResult(NullNode.INSTANCE, pos + 1);
        }

        if (t.type() == TokenType.KEY || t.type() == TokenType.BARE_KEY) {
            var result = parseMapping(pos);
            return new ParseNodeResult(new AstMap(result.map()), result.newPos());
        }

        return new ParseNodeResult(NullNode.INSTANCE, pos);
    }

    private record ParseNodeResult(AstNode node, int newPos) {}

    // =========================================================================
    // Mapping
    // =========================================================================

    private ParseMapResult parseMapping(int pos) {
        AstMap mapping = new AstMap();
        int indentNesting = 0;

        while (pos < n) {
            var ttype = peekTypeAt(pos);

            // Internal INDENT — increase nesting
            if (ttype.isPresent() && ttype.get() == TokenType.INDENT) {
                indentNesting++;
                pos++;
                continue;
            }

            // DEDENT — decrease internal nesting or check for re-indent
            if (ttype.isPresent() && ttype.get() == TokenType.DEDENT) {
                if (indentNesting > 0) {
                    indentNesting--;
                    pos++;
                    continue;
                }
                // DEDENT + INDENT = re-indent at same block level
                int nextP = pos + 1;
                while (nextP < n && tokens.get(nextP).type() == TokenType.NEWLINE)
                    nextP++;
                if (peekTypeAt(nextP).isPresent() && peekTypeAt(nextP).get() == TokenType.INDENT) {
                    pos = nextP + 1;
                    continue;
                }
                break;
            }

            // Other block boundaries
            if (ttype.isEmpty()) break;
            if (ttype.get() == TokenType.END || ttype.get() == TokenType.DOC_SEPARATOR)
                break;

            if (ttype.get() == TokenType.NEWLINE) {
                pos++;
                continue;
            }

            // Block type conflict: sequence item at mapping level
            if (ttype.get() == TokenType.DASH) {
                addWarningAt(pos, "块类型冲突：映射块中出现序列条目 '-'，终止当前映射");
                break;
            }

            // BARE_KEY
            if (ttype.get() == TokenType.BARE_KEY) {
                String key = tokens.get(pos).stringValue();
                pos++;
                pos = consumeNewline(pos);
                mapping.put(key, NullNode.INSTANCE);
                continue;
            }

            // KEY : value
            if (ttype.get() != TokenType.KEY) {
                pos++; // skip unexpected
                continue;
            }

            String key = tokens.get(pos).stringValue();
            pos++; // consume KEY

            // Expect COLON
            boolean colonOk = false;
            if (pos < n && tokens.get(pos).type() == TokenType.COLON) {
                colonOk = true;
                pos++; // consume COLON
            }

            if (!colonOk) {
                // Key without colon → treat as bare
                mapping.put(key, NullNode.INSTANCE);
                pos = consumeNewline(pos);
                continue;
            }

            // Value after COLON
            var nxt = peekTypeAt(pos);

            if (nxt.isPresent() && VALUE_TOKENS.contains(nxt.get())) {
                // Inline value
                var valResult = consumeValue(pos);
                mapping.put(key, valResult.node);
                pos = consumeNewline(valResult.newPos);

            } else if (nxt.isPresent() && nxt.get() == TokenType.NEWLINE) {
                int nlPos = pos + 1; // after NEWLINE
                var nxt2 = peekTypeAt(nlPos);

                if (nxt2.isPresent() && nxt2.get() == TokenType.INDENT) {
                    // Explicit INDENT block
                    pos = nlPos + 1; // consume NEWLINE + INDENT
                    var nodeResult = parseNode(pos);
                    AstNode val = nodeResult.node();
                    int newPos = nodeResult.newPos();
                    if (newPos < n && tokens.get(newPos).type() == TokenType.DEDENT)
                        newPos++; // consume matching DEDENT
                    pos = newPos;

                    // Extend list-values with same-level sequence
                    if (val.isList() && peekTypeAt(pos).isPresent() && peekTypeAt(pos).get() == TokenType.DASH) {
                        var seqResult = parseSequence(pos);
                        AstList combined = new AstList(new ArrayList<>(val.asList()));
                        combined.items().addAll(seqResult.list());
                        val = combined;
                        pos = seqResult.newPos();
                    }
                    mapping.put(key, val);

                } else if (nxt2.isPresent() && nxt2.get() == TokenType.DASH) {
                    // Same-indent sequence as block value
                    pos = nlPos;
                    var seqResult = parseSequence(pos);
                    mapping.put(key, new AstList(seqResult.list()));
                    pos = seqResult.newPos();

                } else {
                    // No block value → null
                    mapping.put(key, NullNode.INSTANCE);
                    pos = nlPos;
                }

            } else if (nxt.isPresent() && nxt.get() == TokenType.DASH) {
                // Same-indent DASH immediately after COLON (no NEWLINE)
                var seqResult = parseSequence(pos);
                mapping.put(key, new AstList(seqResult.list()));
                pos = seqResult.newPos();

            } else {
                // DEDENT, EOF, or other → null
                mapping.put(key, NullNode.INSTANCE);
            }
        }

        return new ParseMapResult(mapping.entries(), pos);
    }

    private record ParseMapResult(List<Map.Entry<String, AstNode>> map, int newPos) {}

    // =========================================================================
    // Sequence
    // =========================================================================

    private ParseSeqResult parseSequence(int pos) {
        boolean complexMode = isComplexSequence(pos);
        List<AstNode> seq = new ArrayList<>();
        int internalDepth = 0;

        while (pos < n) {
            var ttype = peekTypeAt(pos);

            // Boundaries
            if (ttype.isEmpty()) break;
            if (ttype.get() == TokenType.END || ttype.get() == TokenType.DOC_SEPARATOR)
                break;

            if (ttype.get() == TokenType.DEDENT) {
                if (internalDepth > 0) {
                    internalDepth--;
                    pos++;
                    continue;
                }
                // Check for re-indent
                int nextP = pos + 1;
                while (nextP < n && tokens.get(nextP).type() == TokenType.NEWLINE)
                    nextP++;
                if (peekTypeAt(nextP).isPresent() && peekTypeAt(nextP).get() == TokenType.INDENT) {
                    pos = nextP + 1;
                    continue;
                }
                break; // parent-level DEDENT
            }

            if (ttype.get() == TokenType.NEWLINE) {
                pos++;
                continue;
            }

            if (ttype.get() == TokenType.INDENT) {
                internalDepth++;
                pos++;
                continue;
            }

            // Non-DASH → end of sequence or block type conflict
            if (ttype.get() != TokenType.DASH) {
                if (internalDepth > 0) {
                    addErrorAt(pos, "块类型冲突：序列块中出现非 '-' 条目，终止当前序列");
                }
                break;
            }

            // Parse one sequence entry
            var itemResult = parseSequenceItem(pos, complexMode);
            seq.add(itemResult.node());
            pos = itemResult.newPos();
        }

        return new ParseSeqResult(seq, pos);
    }

    private record ParseSeqResult(List<AstNode> list, int newPos) {}

    private ParseNodeResult parseSequenceItem(int pos, boolean complexMode) {
        // Expect DASH
        if (pos >= n || tokens.get(pos).type() != TokenType.DASH)
            return new ParseNodeResult(NullNode.INSTANCE, pos);
        pos++; // consume DASH

        var nxt = peekTypeAt(pos);

        // ---- empty entry (DASH, then NEWLINE or nothing) ----
        if (nxt.isPresent() && nxt.get() == TokenType.NEWLINE) {
            int newlinePos = pos + 1;
            var nxt2 = peekTypeAt(newlinePos);

            if (nxt2.isPresent() && nxt2.get() == TokenType.INDENT) {
                // Empty entry with sub-block
                pos = newlinePos + 1; // consume NEWLINE + INDENT
                var blockResult = parseNode(pos);
                AstNode blockVal = blockResult.node();
                int newPos = blockResult.newPos();
                if (newPos < n && tokens.get(newPos).type() == TokenType.DEDENT)
                    newPos++;
                if (complexMode) {
                    AstMap m = new AstMap();
                    m.put("", blockVal);
                    return new ParseNodeResult(m, newPos);
                } else {
                    return new ParseNodeResult(blockVal, newPos);
                }
            }
            // Empty entry, no sub-block → null
            return new ParseNodeResult(NullNode.INSTANCE, newlinePos);
        }

        // ---- NULL ----
        if (nxt.isPresent() && nxt.get() == TokenType.NULL_) {
            var valResult = consumeValue(pos);
            int newPos = consumeNewline(valResult.newPos);
            return new ParseNodeResult(NullNode.INSTANCE, newPos);
        }

        // ---- INLINE_LIST ----
        if (nxt.isPresent() && nxt.get() == TokenType.INLINE_LIST) {
            var valResult = consumeValue(pos);
            int newPos = consumeNewline(valResult.newPos);
            return new ParseNodeResult(valResult.node, newPos);
        }

        // ---- KEY → inline mapping ----
        if (nxt.isPresent() && nxt.get() == TokenType.KEY) {
            String key = tokens.get(pos).stringValue();
            pos++; // consume KEY

            if (pos < n && tokens.get(pos).type() == TokenType.COLON)
                pos++; // consume COLON

            var after = peekTypeAt(pos);

            if (after.isPresent() && VALUE_TOKENS.contains(after.get())) {
                // Inline value: "- key: value"
                var valResult = consumeValue(pos);
                int newPos = consumeNewline(valResult.newPos);
                AstMap m = new AstMap();
                m.put(key, valResult.node);
                return new ParseNodeResult(m, newPos);
            }

            if (after.isPresent() && after.get() == TokenType.NEWLINE) {
                int newlinePos = pos + 1;
                var after2 = peekTypeAt(newlinePos);
                if (after2.isPresent() && after2.get() == TokenType.INDENT) {
                    // Block value
                    pos = newlinePos + 1; // consume NEWLINE + INDENT
                    var blockResult = parseNode(pos);
                    AstNode blockVal = blockResult.node();
                    int newPos = blockResult.newPos();
                    if (newPos < n && tokens.get(newPos).type() == TokenType.DEDENT)
                        newPos++;
                    AstMap m = new AstMap();
                    m.put(key, blockVal);
                    return new ParseNodeResult(m, newPos);
                } else if (after2.isPresent() && after2.get() == TokenType.DASH) {
                    // Same-indent sequence as block value
                    pos = newlinePos;
                    var seqResult = parseSequence(pos);
                    AstMap m = new AstMap();
                    m.put(key, new AstList(seqResult.list()));
                    return new ParseNodeResult(m, seqResult.newPos());
                } else {
                    // No block, no inline → null
                    AstMap m = new AstMap();
                    m.put(key, NullNode.INSTANCE);
                    return new ParseNodeResult(m, newlinePos);
                }
            }

            // DEDENT, EOF, etc.
            AstMap m = new AstMap();
            m.put(key, NullNode.INSTANCE);
            return new ParseNodeResult(m, pos);
        }

        // ---- SCALAR, RAW_STRING ----
        if (nxt.isPresent() && (nxt.get() == TokenType.SCALAR || nxt.get() == TokenType.RAW_STRING)) {
            var valResult = consumeValue(pos);
            int newPos = consumeNewline(valResult.newPos);
            if (complexMode) {
                AstMap m = new AstMap();
                m.put(valResult.node.asString(), NullNode.INSTANCE);
                return new ParseNodeResult(m, newPos);
            } else {
                return new ParseNodeResult(valResult.node, newPos);
            }
        }

        // ---- MULTILINE_STRING ----
        if (nxt.isPresent() && nxt.get() == TokenType.MULTILINE_STRING) {
            var valResult = consumeValue(pos);
            int newPos = consumeNewline(valResult.newPos);
            if (complexMode) {
                AstMap m = new AstMap();
                m.put(valResult.node.asString(), NullNode.INSTANCE);
                return new ParseNodeResult(m, newPos);
            } else {
                return new ParseNodeResult(valResult.node, newPos);
            }
        }

        // Fallback
        return new ParseNodeResult(NullNode.INSTANCE, pos);
    }

    // =========================================================================
    // Complex-sequence pre-scan
    // =========================================================================

    private boolean isComplexSequence(int startPos) {
        int scanPos = startPos;
        int depth = 0;

        while (scanPos < n) {
            Token t = tokens.get(scanPos);

            if (t.type() == TokenType.INDENT) {
                depth++;
                scanPos++;
                continue;
            }

            if (t.type() == TokenType.DEDENT) {
                if (depth > 0) {
                    depth--;
                    scanPos++;
                    continue;
                }
                break;
            }

            if (t.type() == TokenType.END || t.type() == TokenType.DOC_SEPARATOR)
                break;

            // Non-DASH content tokens end the sequence
            if (t.type() == TokenType.KEY || t.type() == TokenType.BARE_KEY)
                break;

            if (t.type() == TokenType.DASH) {
                // Examine token after DASH (skip NEWLINEs)
                int scan = scanPos + 1;
                while (scan < n) {
                    TokenType st = tokens.get(scan).type();
                    if (st == TokenType.NEWLINE) {
                        scan++;
                        continue;
                    }
                    if (st == TokenType.KEY) return true;   // DASH + KEY → inline mapping
                    if (st == TokenType.INDENT) return true; // DASH + INDENT → sub-block
                    break;
                }
                scanPos = scan;
                continue;
            }

            scanPos++;
        }

        return false;
    }

    // =========================================================================
    // Helpers
    // =========================================================================

    private Optional<TokenType> peekType() {
        if (pos >= n) return Optional.empty();
        return Optional.of(tokens.get(pos).type());
    }

    private Optional<TokenType> peekTypeAt(int p) {
        if (p >= n) return Optional.empty();
        return Optional.of(tokens.get(p).type());
    }

    private void skipNewlines() {
        while (pos < n && tokens.get(pos).type() == TokenType.NEWLINE)
            pos++;
    }

    private int skipNewlinesFrom(int p) {
        while (p < n && tokens.get(p).type() == TokenType.NEWLINE)
            p++;
        return p;
    }

    private int consumeNewline(int p) {
        if (p < n && tokens.get(p).type() == TokenType.NEWLINE)
            return p + 1;
        return p;
    }

    private ConsumeValueResult consumeValue(int p) {
        if (p >= n) return new ConsumeValueResult(NullNode.INSTANCE, p);

        Token t = tokens.get(p);
        if (t.type() == TokenType.SCALAR)
            return new ConsumeValueResult(new AstScalar(t.stringValue()), p + 1);
        if (t.type() == TokenType.NULL_)
            return new ConsumeValueResult(NullNode.INSTANCE, p + 1);
        if (t.type() == TokenType.RAW_STRING)
            return new ConsumeValueResult(new AstScalar(t.stringValue()), p + 1);
        if (t.type() == TokenType.INLINE_LIST) {
            List<AstNode> list = new ArrayList<>();
            for (InlineElem elem : t.inlineListValue()) {
                if (elem instanceof InlineElem.StringElem se)
                    list.add(new AstScalar(se.value()));
                else
                    list.add(NullNode.INSTANCE);
            }
            return new ConsumeValueResult(new AstList(list), p + 1);
        }
        if (t.type() == TokenType.MULTILINE_STRING)
            return new ConsumeValueResult(new AstScalar(t.stringValue()), p + 1);

        return new ConsumeValueResult(NullNode.INSTANCE, p);
    }

    private record ConsumeValueResult(AstNode node, int newPos) {}

    private void addWarningAt(int p, String message) {
        if (p < n) {
            Token t = tokens.get(p);
            warnings.add(new Warning(t.line(), t.column(), message));
        } else {
            warnings.add(new Warning(0, 0, message));
        }
    }

    private void addErrorAt(int p, String message) {
        if (p < n) {
            Token t = tokens.get(p);
            throw new ParseError(t.line(), t.column(), message);
        } else {
            throw new ParseError(0, 0, message);
        }
    }
}
