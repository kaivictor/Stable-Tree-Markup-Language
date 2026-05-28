package com.stml.parser;

import com.stml.ast.*;
import com.stml.diagnostics.ParseError;
import com.stml.diagnostics.Warning;
import com.stml.lexer.InlineElem;
import com.stml.lexer.Token;
import com.stml.lexer.TokenType;

import java.util.*;

/**
 * StreamingParser — incremental STML parser for streaming input.
 * Receives tokens incrementally via feed() and parses as much as possible.
 * finish() completes parsing and returns the document AST list.
 */
public class StreamingParser {

    static final Set<TokenType> VALUE_TOKENS = EnumSet.of(
            TokenType.SCALAR, TokenType.NULL_, TokenType.RAW_STRING,
            TokenType.INLINE_LIST, TokenType.MULTILINE_STRING
    );

    private final List<Token> buffer = new ArrayList<>();
    private int pos = 0;
    private final List<AstNode> docs = new ArrayList<>();
    private final List<Warning> warnings = new ArrayList<>();
    private DeferredSeq deferredSeq = null;

    private record DeferredSeq(int startPos) {}

    // =========================================================================
    // Public API
    // =========================================================================

    /** Feed new tokens. Parses as much as possible from the accumulated buffer. */
    public void feed(List<Token> tokens) {
        buffer.addAll(tokens);
        tryParse();
    }

    /** Complete parsing. Returns a list of document AST nodes. */
    public List<AstNode> finish() {
        deferredSeq = null;
        tryParse();
        return new ArrayList<>(docs);
    }

    public List<Warning> getWarnings() {
        return warnings;
    }

    // =========================================================================
    // Incremental parse loop
    // =========================================================================

    private void tryParse() {
        int n = buffer.size();

        if (bufferIncomplete()) return;

        while (pos < n) {
            pos = skipNewlinesFrom(pos);
            if (pos >= n) break;

            var ttype = peekTypeAt(pos);
            if (ttype.isEmpty()) break;
            if (ttype.get() == TokenType.END) break;

            // Consume trailing DEDENTs
            if (ttype.get() == TokenType.DEDENT) {
                pos++;
                continue;
            }

            // Consume DOC_SEPARATOR between documents
            if (ttype.get() == TokenType.DOC_SEPARATOR) {
                if (docs.isEmpty()) docs.add(NullNode.INSTANCE);
                pos++;
                pos = skipNewlinesFrom(pos);
                var nextType = peekTypeAt(pos);
                if (nextType.isEmpty() || nextType.get() == TokenType.END) {
                    docs.add(NullNode.INSTANCE);
                } else if (nextType.get() == TokenType.DOC_SEPARATOR) {
                    docs.add(NullNode.INSTANCE);
                }
                continue;
            }

            int savedPos = pos;
            var docResult = parseDocument(pos);
            AstNode node = docResult.node();
            int newPos = docResult.newPos();

            if (newPos <= savedPos) {
                // Parser couldn't advance — skip one token
                pos++;
                continue;
            }

            if (node.isList() && checkPosForMix(newPos, n)) {
                var leftoverType = peekTypeAt(skipNewlinesFrom(newPos));
                if (leftoverType.isPresent()
                        && leftoverType.get() != TokenType.END
                        && leftoverType.get() != TokenType.DOC_SEPARATOR
                        && leftoverType.get() != TokenType.DEDENT) {
                    addErrorAt(skipNewlinesFrom(newPos), "块类型冲突：序列文档中出现非 '-' 条目");
                }
            }
            pos = newPos;
            docs.add(node);
        }
    }

    private boolean checkPosForMix(int newPos, int n) {
        return skipNewlinesFrom(newPos) < n;
    }

    // =========================================================================
    // Document-level
    // =========================================================================

    private ParseNodeResult parseDocument(int pos) {
        pos = skipNewlinesFrom(pos);
        int n = buffer.size();
        if (pos >= n || buffer.get(pos).type() == TokenType.END)
            return new ParseNodeResult(NullNode.INSTANCE, pos);

        while (pos < n && buffer.get(pos).type() == TokenType.DEDENT) pos++;
        if (pos >= n || buffer.get(pos).type() == TokenType.END)
            return new ParseNodeResult(NullNode.INSTANCE, pos);

        boolean rootIndented = false;
        if (buffer.get(pos).type() == TokenType.INDENT) {
            rootIndented = true;
            pos++;
        }

        var result = parseNode(pos);
        AstNode node = result.node();
        int newPos = result.newPos();

        while (rootIndented && newPos < n && buffer.get(newPos).type() == TokenType.DEDENT)
            newPos++;

        return new ParseNodeResult(node, newPos);
    }

    // =========================================================================
    // Node dispatch
    // =========================================================================

    private ParseNodeResult parseNode(int pos) {
        pos = skipNewlinesFrom(pos);
        int n = buffer.size();
        if (pos >= n) return new ParseNodeResult(NullNode.INSTANCE, pos);

        Token t = buffer.get(pos);
        if (t.type() == TokenType.DASH) {
            var result = parseSequence(pos);
            return new ParseNodeResult(new AstList(result.list()), result.newPos());
        }
        if (t.type() == TokenType.NULL_) return new ParseNodeResult(NullNode.INSTANCE, pos + 1);
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
        int n = buffer.size();

        while (pos < n) {
            var ttype = peekTypeAt(pos);

            if (ttype.isPresent() && ttype.get() == TokenType.INDENT) {
                indentNesting++;
                pos++;
                continue;
            }

            if (ttype.isPresent() && ttype.get() == TokenType.DEDENT) {
                if (indentNesting > 0) {
                    indentNesting--;
                    pos++;
                    continue;
                }
                int nextP = pos + 1;
                while (nextP < n && buffer.get(nextP).type() == TokenType.NEWLINE) nextP++;
                if (peekTypeAt(nextP).isPresent() && peekTypeAt(nextP).get() == TokenType.INDENT) {
                    pos = nextP + 1;
                    continue;
                }
                break;
            }

            if (ttype.isEmpty()) break;
            if (ttype.get() == TokenType.END || ttype.get() == TokenType.DOC_SEPARATOR) break;

            if (ttype.get() == TokenType.NEWLINE) { pos++; continue; }

            if (ttype.get() == TokenType.DASH) {
                addWarningAt(pos, "块类型冲突：映射块中出现序列条目 '-'，终止当前映射");
                break;
            }

            if (ttype.get() == TokenType.BARE_KEY) {
                String key = buffer.get(pos).stringValue();
                pos++;
                pos = consumeNewline(pos);
                mapping.put(key, NullNode.INSTANCE);
                continue;
            }

            if (ttype.get() != TokenType.KEY) { pos++; continue; }

            String key = buffer.get(pos).stringValue();
            pos++;

            boolean colonOk = false;
            if (pos < n && buffer.get(pos).type() == TokenType.COLON) {
                colonOk = true;
                pos++;
            }

            if (!colonOk) {
                mapping.put(key, NullNode.INSTANCE);
                pos = consumeNewline(pos);
                continue;
            }

            var nxt = peekTypeAt(pos);

            if (nxt.isPresent() && VALUE_TOKENS.contains(nxt.get())) {
                var valResult = consumeValue(pos);
                mapping.put(key, valResult.node);
                pos = consumeNewline(valResult.newPos);

            } else if (nxt.isPresent() && nxt.get() == TokenType.NEWLINE) {
                int nlPos = pos + 1;
                var nxt2 = peekTypeAt(nlPos);

                if (nxt2.isEmpty()) {
                    return new ParseMapResult(mapping.entries(), pos);
                } else if (nxt2.get() == TokenType.INDENT) {
                    pos = nlPos + 1;
                    var nodeResult = parseNode(pos);
                    AstNode val = nodeResult.node();
                    int newP = nodeResult.newPos();
                    if (newP < n && buffer.get(newP).type() == TokenType.DEDENT) newP++;
                    pos = newP;

                    if (val.isList() && peekTypeAt(pos).isPresent() && peekTypeAt(pos).get() == TokenType.DASH) {
                        var seqResult = parseSequence(pos);
                        AstList combined = new AstList(new ArrayList<>(val.asList()));
                        combined.items().addAll(seqResult.list());
                        val = combined;
                        pos = seqResult.newPos();
                    }
                    mapping.put(key, val);

                } else if (nxt2.get() == TokenType.DASH) {
                    pos = nlPos;
                    var seqResult = parseSequence(pos);
                    mapping.put(key, new AstList(seqResult.list()));
                    pos = seqResult.newPos();

                } else {
                    mapping.put(key, NullNode.INSTANCE);
                    pos = nlPos;
                }

            } else if (nxt.isPresent() && nxt.get() == TokenType.DASH) {
                var seqResult = parseSequence(pos);
                mapping.put(key, new AstList(seqResult.list()));
                pos = seqResult.newPos();

            } else if (nxt.isPresent() && (nxt.get() == TokenType.DEDENT
                    || nxt.get() == TokenType.END || nxt.get() == TokenType.DOC_SEPARATOR)) {
                mapping.put(key, NullNode.INSTANCE);
            } else if (nxt.isEmpty()) {
                return new ParseMapResult(mapping.entries(), pos);
            } else {
                addWarningAt(pos, "映射键 '" + key + "' 后出现意外的 token，设为 null");
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
        boolean complexMode = isComplexSequence(pos).orElse(false);
        List<AstNode> seq = new ArrayList<>();
        int internalDepth = 0;
        int n = buffer.size();

        while (pos < n) {
            var ttype = peekTypeAt(pos);

            if (ttype.isEmpty()) break;
            if (ttype.get() == TokenType.END || ttype.get() == TokenType.DOC_SEPARATOR) break;

            if (ttype.get() == TokenType.DEDENT) {
                if (internalDepth > 0) { internalDepth--; pos++; continue; }
                int nextP = pos + 1;
                while (nextP < n && buffer.get(nextP).type() == TokenType.NEWLINE) nextP++;
                if (peekTypeAt(nextP).isPresent() && peekTypeAt(nextP).get() == TokenType.INDENT) {
                    pos = nextP + 1;
                    continue;
                }
                break;
            }

            if (ttype.get() == TokenType.NEWLINE) { pos++; continue; }
            if (ttype.get() == TokenType.INDENT) { internalDepth++; pos++; continue; }

            if (ttype.get() != TokenType.DASH) {
                if (internalDepth > 0)
                    addErrorAt(pos, "块类型冲突：序列块中出现非 '-' 条目，终止当前序列");
                break;
            }

            var itemResult = parseSequenceItem(pos, complexMode);
            seq.add(itemResult.node());
            pos = itemResult.newPos();
        }

        return new ParseSeqResult(seq, pos);
    }

    private record ParseSeqResult(List<AstNode> list, int newPos) {}

    private ParseNodeResult parseSequenceItem(int pos, boolean complexMode) {
        int n = buffer.size();

        if (pos >= n || buffer.get(pos).type() != TokenType.DASH)
            return new ParseNodeResult(NullNode.INSTANCE, pos);
        pos++;

        var nxt = peekTypeAt(pos);

        // ---- empty entry ----
        if (nxt.isPresent() && nxt.get() == TokenType.NEWLINE) {
            int newlinePos = pos + 1;
            var nxt2 = peekTypeAt(newlinePos);

            if (nxt2.isEmpty()) {
                return new ParseNodeResult(NullNode.INSTANCE, pos);
            } else if (nxt2.get() == TokenType.INDENT) {
                pos = newlinePos + 1;
                var blockResult = parseNode(pos);
                AstNode blockVal = blockResult.node();
                int newP = blockResult.newPos();
                if (newP < n && buffer.get(newP).type() == TokenType.DEDENT) newP++;
                if (complexMode) {
                    AstMap m = new AstMap();
                    m.put("", blockVal);
                    return new ParseNodeResult(m, newP);
                }
                return new ParseNodeResult(blockVal, newP);
            }
            return new ParseNodeResult(NullNode.INSTANCE, newlinePos);
        }

        // ---- NULL ----
        if (nxt.isPresent() && nxt.get() == TokenType.NULL_) {
            var vr = consumeValue(pos);
            int newP = consumeNewline(vr.newPos);
            return new ParseNodeResult(NullNode.INSTANCE, newP);
        }

        // ---- INLINE_LIST ----
        if (nxt.isPresent() && nxt.get() == TokenType.INLINE_LIST) {
            var vr = consumeValue(pos);
            return new ParseNodeResult(vr.node, consumeNewline(vr.newPos));
        }

        // ---- KEY → inline mapping ----
        if (nxt.isPresent() && nxt.get() == TokenType.KEY) {
            String key = buffer.get(pos).stringValue();
            pos++;
            if (pos < n && buffer.get(pos).type() == TokenType.COLON) pos++;

            var after = peekTypeAt(pos);

            if (after.isPresent() && VALUE_TOKENS.contains(after.get())) {
                var vr = consumeValue(pos);
                int newP = consumeNewline(vr.newPos);
                AstMap m = new AstMap();
                m.put(key, vr.node);
                return new ParseNodeResult(m, newP);
            }

            if (after.isPresent() && after.get() == TokenType.NEWLINE) {
                int newlinePos = pos + 1;
                var after2 = peekTypeAt(newlinePos);
                if (after2.isEmpty()) {
                    AstMap m = new AstMap();
                    m.put(key, NullNode.INSTANCE);
                    return new ParseNodeResult(m, pos);
                } else if (after2.get() == TokenType.INDENT) {
                    pos = newlinePos + 1;
                    var blockResult = parseNode(pos);
                    AstNode blockVal = blockResult.node();
                    int newP = blockResult.newPos();
                    if (newP < n && buffer.get(newP).type() == TokenType.DEDENT) newP++;
                    AstMap m = new AstMap();
                    m.put(key, blockVal);
                    return new ParseNodeResult(m, newP);
                } else if (after2.get() == TokenType.DASH) {
                    pos = newlinePos;
                    var seqResult = parseSequence(pos);
                    AstMap m = new AstMap();
                    m.put(key, new AstList(seqResult.list()));
                    return new ParseNodeResult(m, seqResult.newPos());
                } else {
                    AstMap m = new AstMap();
                    m.put(key, NullNode.INSTANCE);
                    return new ParseNodeResult(m, newlinePos);
                }
            }

            if (after.isEmpty()) {
                AstMap m = new AstMap();
                m.put(key, NullNode.INSTANCE);
                return new ParseNodeResult(m, pos);
            }

            AstMap m = new AstMap();
            m.put(key, NullNode.INSTANCE);
            return new ParseNodeResult(m, pos);
        }

        // ---- SCALAR, RAW_STRING ----
        if (nxt.isPresent() && (nxt.get() == TokenType.SCALAR || nxt.get() == TokenType.RAW_STRING)) {
            var vr = consumeValue(pos);
            int newP = consumeNewline(vr.newPos);
            if (complexMode) {
                AstMap m = new AstMap();
                m.put(vr.node.asString(), NullNode.INSTANCE);
                return new ParseNodeResult(m, newP);
            }
            return new ParseNodeResult(vr.node, newP);
        }

        // ---- MULTILINE_STRING ----
        if (nxt.isPresent() && nxt.get() == TokenType.MULTILINE_STRING) {
            var vr = consumeValue(pos);
            int newP = consumeNewline(vr.newPos);
            if (complexMode) {
                AstMap m = new AstMap();
                m.put(vr.node.asString(), NullNode.INSTANCE);
                return new ParseNodeResult(m, newP);
            }
            return new ParseNodeResult(vr.node, newP);
        }

        return new ParseNodeResult(NullNode.INSTANCE, pos);
    }

    // =========================================================================
    // Complex-sequence pre-scan (incremental)
    // =========================================================================

    private Optional<Boolean> isComplexSequence(int startPos) {
        int scanPos = startPos;
        int depth = 0;
        int n = buffer.size();

        while (scanPos < n) {
            Token t = buffer.get(scanPos);

            if (t.type() == TokenType.INDENT) { depth++; scanPos++; continue; }

            if (t.type() == TokenType.DEDENT) {
                if (depth > 0) { depth--; scanPos++; continue; }
                break;
            }

            if (t.type() == TokenType.END || t.type() == TokenType.DOC_SEPARATOR) break;

            if (t.type() == TokenType.KEY || t.type() == TokenType.BARE_KEY) break;

            if (t.type() == TokenType.DASH) {
                int scan = scanPos + 1;
                while (scan < n) {
                    TokenType st = buffer.get(scan).type();
                    if (st == TokenType.NEWLINE) { scan++; continue; }
                    if (st == TokenType.KEY) return Optional.of(true);
                    if (st == TokenType.INDENT) return Optional.of(true);
                    break;
                }
                if (scan >= n) return Optional.empty();
                scanPos = scan;
                continue;
            }

            scanPos++;
        }

        return Optional.of(false);
    }

    // =========================================================================
    // Buffer-incomplete detection
    // =========================================================================

    private boolean bufferIncomplete() {
        int n = buffer.size();
        if (n == 0) return false;

        for (int i = pos; i < n; i++) {
            if (buffer.get(i).type() == TokenType.END) return false;
        }
        return true; // No END → defer
    }

    // =========================================================================
    // Helpers
    // =========================================================================

    private Optional<TokenType> peekTypeAt(int p) {
        if (p >= buffer.size() || p < 0) return Optional.empty();
        return Optional.of(buffer.get(p).type());
    }

    private int skipNewlinesFrom(int p) {
        int n = buffer.size();
        while (p < n && buffer.get(p).type() == TokenType.NEWLINE) p++;
        return p;
    }

    private int consumeNewline(int p) {
        int n = buffer.size();
        if (p < n && buffer.get(p).type() == TokenType.NEWLINE) return p + 1;
        return p;
    }

    private record ConsumeValueResult(AstNode node, int newPos) {}

    private ConsumeValueResult consumeValue(int p) {
        int n = buffer.size();
        if (p >= n) return new ConsumeValueResult(NullNode.INSTANCE, p);

        Token t = buffer.get(p);
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

    private void addWarningAt(int p, String message) {
        int n = buffer.size();
        if (p < n) {
            Token t = buffer.get(p);
            warnings.add(new Warning(t.line(), t.column(), message));
        } else {
            warnings.add(new Warning(0, 0, message));
        }
    }

    private void addErrorAt(int p, String message) {
        int n = buffer.size();
        if (p < n) {
            Token t = buffer.get(p);
            throw new ParseError(t.line(), t.column(), message);
        } else {
            throw new ParseError(0, 0, message);
        }
    }
}
