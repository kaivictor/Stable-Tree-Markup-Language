package com.stml.lexer;

import com.stml.diagnostics.Warning;

import java.util.ArrayList;
import java.util.List;

/**
 * StreamingLexer — incremental STML lexer for streaming input.
 * Buffers incomplete lines and multiline strings across chunks.
 */
public class StreamingLexer {

    private String buffer = "";    // un-terminated text after last \n
    private final List<String> lines = new ArrayList<>();
    private int lineIdx = 0;
    private final List<Integer> indentStack = new ArrayList<>();
    private boolean firstChunk = true;

    // Multiline string state
    private boolean inMultiline = false;
    private final List<String> multilineParts = new ArrayList<>();
    private int multilineKeyIndent = 0;
    private int multilineStartLine = 0;
    private int multilineNewlineLine = 0;
    private int multilineNewlineCol = 0;

    // Token emission
    private final List<Token> tokens = new ArrayList<>();
    private int lastEmitted = 0;

    // Warnings
    private final List<Warning> warnings = new ArrayList<>();

    // Deferred DEDENT (for multiline close between lines)
    private Integer deferredDedentTo = null;

    public StreamingLexer() {
        indentStack.add(0);
    }

    // =========================================================================
    // Public API
    // =========================================================================

    /** Feed a text chunk. Returns tokens emitted since last call. */
    public List<Token> feed(String chunk) {
        firstChunk = false;

        // Normalise line endings
        StringBuilder normalised = new StringBuilder(chunk.length());
        for (int i = 0; i < chunk.length(); i++) {
            char ch = chunk.charAt(i);
            if (ch == '\r') {
                if (i + 1 < chunk.length() && chunk.charAt(i + 1) == '\n') i++;
                normalised.append('\n');
            } else {
                normalised.append(ch);
            }
        }

        buffer += normalised.toString();

        // Extract complete lines from buffer
        int pos;
        while ((pos = buffer.indexOf('\n')) != -1) {
            lines.add(buffer.substring(0, pos));
            buffer = buffer.substring(pos + 1);
        }

        // Process new lines
        processLines();

        // Return tokens emitted since last call
        List<Token> newTokens = new ArrayList<>(tokens.subList(lastEmitted, tokens.size()));
        lastEmitted = tokens.size();
        return newTokens;
    }

    /** Signal end of input. Returns remaining tokens (DEDENTs + END). */
    public List<Token> finish() {
        // Process any remaining content in buffer as a final line
        if (!buffer.isEmpty()) {
            lines.add(buffer);
            buffer = "";
            processLines();
        }

        // Auto-close multiline if still open
        if (inMultiline) {
            addWarning(multilineStartLine, 1, "多行字符串未找到闭合 '}'，已由 EOF 自动闭合");

            String mlValue = String.join("\n", multilineParts);
            addToken(TokenType.MULTILINE_STRING, mlValue, multilineStartLine, col(multilineKeyIndent, 0));
            addToken(TokenType.NEWLINE, null, multilineNewlineLine, multilineNewlineCol);

            if (deferredDedentTo != null) {
                int target = deferredDedentTo;
                deferredDedentTo = null;
                emitDedentsTo(target);
            }

            inMultiline = false;
            multilineParts.clear();
        }

        // EOF: pop remaining indentation → DEDENTs
        emitDedentsTo(0);
        int lastLine = lines.size();
        addToken(TokenType.END, null, lastLine, 1);

        // Return remaining tokens
        List<Token> newTokens = new ArrayList<>(tokens.subList(lastEmitted, tokens.size()));
        lastEmitted = tokens.size();
        return newTokens;
    }

    public List<Warning> getWarnings() {
        return warnings;
    }

    // =========================================================================
    // Internal line processing
    // =========================================================================

    private void processLines() {
        while (lineIdx < lines.size()) {
            // ---- multiline accumulation mode ----
            if (inMultiline) {
                String line = lines.get(lineIdx);
                int lineIndent = calcIndent(line);
                String content = line.substring(lineIndent);
                String stripped = stripSpaces(content);

                // Standalone '}' at or before key indent → close multiline
                if (stripped.equals("}") && lineIndent <= multilineKeyIndent) {
                    String mlValue = String.join("\n", multilineParts);
                    addToken(TokenType.MULTILINE_STRING, mlValue, multilineStartLine, col(multilineKeyIndent, 0));
                    addToken(TokenType.NEWLINE, null, multilineNewlineLine, multilineNewlineCol);

                    if (lineIndent < multilineKeyIndent) {
                        deferredDedentTo = lineIndent;
                    }

                    inMultiline = false;
                    multilineParts.clear();
                    lineIdx++; // consume '}' line

                    if (deferredDedentTo != null) {
                        int target = deferredDedentTo;
                        deferredDedentTo = null;
                        emitDedentsTo(target);
                    }
                    continue;
                }

                // Not a close — accumulate this line
                multilineParts.add(line);
                lineIdx++;
                continue;
            }

            // ---- normal line processing ----
            processOneLine();
            lineIdx++;

            // Emit deferred DEDENTs
            if (deferredDedentTo != null) {
                int target = deferredDedentTo;
                deferredDedentTo = null;
                emitDedentsTo(target);
            }
        }
    }

    private void processOneLine() {
        String line = lines.get(lineIdx);
        int indent = calcIndent(line);
        String content = line.substring(indent);

        // Skip empty lines and whole-line comments
        if (isBlankOrComment(content)) return;

        // Document separator (indent must be 0)
        String stripped = stripSpaces(content);
        if (indent == 0 && stripped.equals("---")) {
            emitDedentsTo(0);
            addToken(TokenType.DOC_SEPARATOR, "---", lineIdx + 1, indent + 1);
            return;
        }

        // Process indentation changes
        processIndent(indent);

        // Dispatch on line type
        if (stripped.equals("---") && indent > 0) {
            lexMappingLine(content, indent);
        } else if (!content.isEmpty() && content.charAt(0) == '-') {
            lexSequenceLine(content, indent);
        } else {
            lexMappingLine(content, indent);
        }
    }

    // =========================================================================
    // Token factory
    // =========================================================================

    private void addToken(TokenType type, Object value, int line, int column) {
        tokens.add(new Token(type, value, line, column));
    }

    private void addWarning(int line, int column, String message) {
        warnings.add(new Warning(line, column, message));
    }

    private int col(int indent, int offsetInContent) {
        return indent + offsetInContent + 1;
    }

    // =========================================================================
    // Indentation helpers
    // =========================================================================

    private int calcIndent(String line) {
        int n = 0;
        for (int i = 0; i < line.length(); i++) {
            if (line.charAt(i) == ' ') n++;
            else break;
        }
        return n;
    }

    private boolean isBlankOrComment(String content) {
        int i = 0;
        while (i < content.length() && content.charAt(i) == ' ') i++;
        return i >= content.length() || content.charAt(i) == '#';
    }

    private void processIndent(int indent) {
        int top = indentStack.get(indentStack.size() - 1);
        if (indent > top) {
            indentStack.add(indent);
            addToken(TokenType.INDENT, String.valueOf(indent - top), lineIdx + 1, 1);
        } else if (indent < top) {
            emitDedentsTo(indent);
            if (indent > indentStack.get(indentStack.size() - 1)) {
                int diff = indent - indentStack.get(indentStack.size() - 1);
                indentStack.add(indent);
                addToken(TokenType.INDENT, String.valueOf(diff), lineIdx + 1, 1);
            }
        }
    }

    private void emitDedentsTo(int target) {
        while (indentStack.get(indentStack.size() - 1) > target) {
            indentStack.remove(indentStack.size() - 1);
            addToken(TokenType.DEDENT, null, lineIdx + 1, 1);
        }
    }

    // =========================================================================
    // Line-level dispatch
    // =========================================================================

    private void lexSequenceLine(String content, int indent) {
        int lineNo = lineIdx + 1;
        int contentLen = content.length();
        int newlineCol = col(indent, contentLen);
        addToken(TokenType.DASH, null, lineNo, col(indent, 0));

        String rest = content.substring(1);
        if (!rest.isEmpty() && rest.charAt(0) == ' ')
            rest = rest.substring(1);

        if (rest.isEmpty()) {
            addToken(TokenType.NEWLINE, null, lineNo, newlineCol);
            return;
        }

        lexValueOrKeyOnLine(rest, indent, true);

        if (inMultiline) {
            multilineNewlineLine = lineNo;
            multilineNewlineCol = newlineCol;
        } else {
            addToken(TokenType.NEWLINE, null, lineNo, newlineCol);
        }
    }

    private void lexMappingLine(String content, int indent) {
        int lineNo = lineIdx + 1;
        int contentLen = content.length();
        int newlineCol = col(indent, contentLen);

        // (1) Try quoted-key mode
        if (!content.isEmpty() && content.charAt(0) == '"') {
            var result = tryQuotedKey(content, indent);
            if (result.ok()) {
                addToken(TokenType.KEY, result.key(), lineNo, col(indent, 0));
                addToken(TokenType.COLON, null, lineNo, col(indent, result.colonPos()));
                lexRemainderAfterColon(result.rest(), indent, lineNo);
                if (inMultiline) {
                    multilineNewlineLine = lineNo;
                    multilineNewlineCol = newlineCol;
                } else {
                    addToken(TokenType.NEWLINE, null, lineNo, newlineCol);
                }
                return;
            }
        }

        // (2) Scan for structural colon
        int colon = findUnquotedColon(content);

        if (colon == -1) {
            String stripped = stripSpaces(content);
            String allStripped = stripAllWhitespace(content);
            if (allStripped.equals("null") || allStripped.equals("~")) {
                addToken(TokenType.NULL_, null, lineNo, col(indent, 0));
                addToken(TokenType.NEWLINE, null, lineNo, newlineCol);
                return;
            }

            String key = bareKeyFromContent(content);
            addToken(TokenType.BARE_KEY, key, lineNo, col(indent, 0));
            addToken(TokenType.NEWLINE, null, lineNo, newlineCol);
            return;
        }

        // (3) Colon at position 0 → empty key → BARE_KEY
        if (colon == 0) {
            String key = reconstructEmptyKey(content);
            addToken(TokenType.BARE_KEY, key, lineNo, col(indent, 0));
            addToken(TokenType.NEWLINE, null, lineNo, newlineCol);
            return;
        }

        // (4) Normal key
        String key = content.substring(0, colon);
        String rest = content.substring(colon + 1);

        addToken(TokenType.KEY, key, lineNo, col(indent, 0));
        addToken(TokenType.COLON, null, lineNo, col(indent, colon));
        lexRemainderAfterColon(rest, indent, lineNo);

        if (inMultiline) {
            multilineNewlineLine = lineNo;
            multilineNewlineCol = newlineCol;
        } else {
            addToken(TokenType.NEWLINE, null, lineNo, newlineCol);
        }
    }

    // =========================================================================
    // Value parsing
    // =========================================================================

    private void lexRemainderAfterColon(String rest, int indent, int lineNo) {
        if (rest.isEmpty()) return;
        if (rest.charAt(0) == ' ' || rest.charAt(0) == '\t')
            rest = rest.substring(1);
        if (rest.isEmpty()) return;
        lexValueOrKeyOnLine(rest, indent, false);
    }

    private void lexValueOrKeyOnLine(String text, int indent, boolean scanColon) {
        int lineNo = lineIdx + 1;
        String stripped = stripSpaces(text);

        // Multiline string trigger
        if (stripped.equals("{")) {
            inMultiline = true;
            multilineKeyIndent = indent;
            multilineStartLine = lineNo;
            multilineParts.clear();
            return;
        }

        // Inline list
        if (!text.isEmpty() && text.charAt(0) == '[') {
            int ep = text.length() - 1;
            while (ep >= 0 && text.charAt(ep) == ' ') ep--;
            if (ep > 0 && text.charAt(ep) == ']') {
                var listResult = parseInlineList(text, indent);
                if (listResult.ok()) {
                    addToken(TokenType.INLINE_LIST, listResult.elements(), lineNo, col(indent, 0));
                    return;
                }
                addWarning(lineNo, listResult.errorCol() != 0 ? listResult.errorCol() : col(indent, 0),
                        "内联列表缺少结尾 ']'，降级为原始字符串");
                addToken(TokenType.RAW_STRING, text, lineNo, col(indent, 0));
                return;
            }
        }

        if (!text.isEmpty() && text.charAt(0) == '[') {
            if (!text.contains("]")) {
                addWarning(lineNo, col(indent, 0), "内联列表缺少结尾 ']'，降级为原始字符串");
                addToken(TokenType.RAW_STRING, text, lineNo, col(indent, 0));
                return;
            }
        }

        // Quoted content
        if (!text.isEmpty() && text.charAt(0) == '"') {
            if (scanColon) {
                var result = tryQuotedKey(text, indent);
                if (result.ok()) {
                    addToken(TokenType.KEY, result.key(), lineNo, col(indent, 0));
                    addToken(TokenType.COLON, null, lineNo, col(indent, result.colonPos()));
                    int consumed = text.length() - result.rest().length();
                    String r = result.rest();
                    if (!r.isEmpty() && r.charAt(0) == ' ') r = r.substring(1);
                    lexValueOrKeyOnLine(r, indent + consumed, true);
                    return;
                }
            }
            var qv = parseQuotedValue(text, 0, indent);
            if (qv.ok()) {
                addToken(TokenType.SCALAR, qv.value(), lineNo, col(indent, 0));
            } else {
                addWarning(lineNo, col(indent, 0), "引号未闭合，降级为原始字符串");
                addToken(TokenType.RAW_STRING, text, lineNo, col(indent, 0));
            }
            return;
        }

        // Unquoted → check for inline mapping
        if (scanColon) {
            int c = findUnquotedColon(text);
            if (c >= 0) {
                String inlineKey = text.substring(0, c);
                String inlineRest = text.substring(c + 1);
                if (!inlineRest.isEmpty() && inlineRest.charAt(0) == ' ')
                    inlineRest = inlineRest.substring(1);
                addToken(TokenType.KEY, inlineKey, lineNo, col(indent, 0));
                addToken(TokenType.COLON, null, lineNo, col(indent, inlineKey.length()));
                lexValueOrKeyOnLine(inlineRest, indent + inlineKey.length() + 1, true);
                return;
            }
        }

        // Plain scalar
        lexScalarOrNull(text, indent);
    }

    private void lexScalarOrNull(String text, int indent) {
        int lineNo = lineIdx + 1;
        String stripped = stripSpaces(text);
        if (stripped.isEmpty()) return;
        String allStripped = stripAllWhitespace(text);
        if (allStripped.equals("null") || allStripped.equals("~")) {
            addToken(TokenType.NULL_, null, lineNo, col(indent, 0));
        } else {
            addToken(TokenType.SCALAR, stripped, lineNo, col(indent, 0));
        }
    }

    // =========================================================================
    // Quoted-key attempt — non-greedy
    // =========================================================================

    private record QuotedKeyResult(String key, String rest, boolean ok, int colonPos) {
        static final QuotedKeyResult FAIL = new QuotedKeyResult("", "", false, -1);
    }

    private QuotedKeyResult tryQuotedKey(String content, int indent) {
        int end = findFirstUnescapedQuote(content, 1);
        if (end == -1) return QuotedKeyResult.FAIL;
        if (end + 1 >= content.length() || content.charAt(end + 1) != ':')
            return QuotedKeyResult.FAIL;
        String rawKey = content.substring(1, end);
        String key = unescape(rawKey, indent + 1);
        String rest = content.substring(end + 2);
        return new QuotedKeyResult(key, rest, true, end + 1);
    }

    // =========================================================================
    // Structural colon scan
    // =========================================================================

    private int findUnquotedColon(String s) {
        boolean inQuote = false;
        boolean hadClose = false;
        int n = s.length();
        for (int i = 0; i < n; i++) {
            char ch = s.charAt(i);
            if (ch == '\\') { i++; continue; }
            if (ch == '"') {
                if (inQuote) hadClose = true;
                inQuote = !inQuote;
            } else if (ch == ':' && !inQuote) {
                return i;
            }
        }
        if (inQuote && !hadClose) return s.indexOf(':');
        return -1;
    }

    // =========================================================================
    // Quote matching
    // =========================================================================

    private int findFirstUnescapedQuote(String s, int start) {
        int i = start;
        int n = s.length();
        while (i < n) {
            if (s.charAt(i) == '\\') { i += 2; continue; }
            if (s.charAt(i) == '"') return i;
            i++;
        }
        return -1;
    }

    private int findLastUnescapedQuote(String s, int start) {
        int last = -1;
        int i = start;
        int n = s.length();
        while (i < n) {
            if (s.charAt(i) == '\\') { i += 2; continue; }
            if (s.charAt(i) == '"') last = i;
            i++;
        }
        return last;
    }

    // =========================================================================
    // Escape processing
    // =========================================================================

    private String unescape(String s, int colOffset) {
        StringBuilder result = new StringBuilder(s.length());
        int i = 0;
        int n = s.length();
        while (i < n) {
            if (s.charAt(i) == '\\' && i + 1 < n) {
                char nxt = s.charAt(i + 1);
                switch (nxt) {
                    case '"': result.append('"'); break;
                    case '\\': result.append('\\'); break;
                    case 'n': result.append('\n'); break;
                    case 't': result.append('\t'); break;
                    default:
                        result.append('\\');
                        result.append(nxt);
                        addWarning(lineIdx + 1, colOffset + i + 1,
                                "非法转义序列 '\\" + nxt + "'，保留原样");
                        break;
                }
                i += 2;
            } else {
                result.append(s.charAt(i));
                i++;
            }
        }
        return result.toString();
    }

    // =========================================================================
    // Quoted value — greedy
    // =========================================================================

    private record QuotedValueResult(String value, int endPos, boolean ok) {}

    private QuotedValueResult parseQuotedValue(String s, int start, int indent) {
        int end = findLastUnescapedQuote(s, start + 1);
        if (end == -1) return new QuotedValueResult("", start, false);
        String raw = s.substring(start + 1, end);
        return new QuotedValueResult(unescape(raw, indent + start + 1), end + 1, true);
    }

    // =========================================================================
    // Inline list
    // =========================================================================

    private record InlineListResult(List<InlineElem> elements, boolean ok, int errorCol) {}

    private InlineListResult parseInlineList(String s, int indent) {
        int closePos = s.lastIndexOf(']');
        if (closePos == -1) return new InlineListResult(List.of(), false, indent + s.length());

        for (int i = closePos + 1; i < s.length(); i++) {
            if (s.charAt(i) != ' ') return new InlineListResult(List.of(), false, indent + i);
        }

        String inner = s.substring(1, closePos);
        List<InlineElem> elements = new ArrayList<>();
        int i = 0;
        int n = inner.length();

        while (i < n) {
            while (i < n && inner.charAt(i) == ' ') i++;
            if (i >= n) break;

            if (inner.charAt(i) == '"') {
                int eq = findFirstUnescapedQuote(inner, i + 1);
                if (eq != -1) {
                    String rawElem = inner.substring(i + 1, eq);
                    elements.add(new InlineElem.StringElem(unescape(rawElem, indent + i + 2)));
                    i = eq + 1;
                } else {
                    elements.add(new InlineElem.StringElem(inner.substring(i)));
                    i = n;
                }
                while (i < n && (inner.charAt(i) == ',' || inner.charAt(i) == ' ')) i++;
            } else {
                int startI = i;
                while (i < n && inner.charAt(i) != ',') i++;
                String elem = inner.substring(startI, i);
                String elemStripped = stripSpaces(elem);

                if (elemStripped.equals("null") || elemStripped.equals("~")) {
                    elements.add(InlineElem.NullElem.INSTANCE);
                } else if (elemStripped.isEmpty()) {
                    elements.add(InlineElem.NullElem.INSTANCE);
                } else {
                    elements.add(new InlineElem.StringElem(elemStripped));
                }
                while (i < n && (inner.charAt(i) == ',' || inner.charAt(i) == ' ')) i++;
            }
        }

        String rstrip = inner.stripTrailing();
        if (!rstrip.isEmpty() && rstrip.charAt(rstrip.length() - 1) == ',')
            elements.add(InlineElem.NullElem.INSTANCE);

        return new InlineListResult(elements, true, 0);
    }

    // =========================================================================
    // Bare-key helpers
    // =========================================================================

    private static String bareKeyFromContent(String content) {
        String stripped = content.stripTrailing();
        if (stripped.length() >= 2 && stripped.charAt(0) == '"' && stripped.charAt(stripped.length() - 1) == '"') {
            String inner = stripped.substring(1, stripped.length() - 1);
            return inner + content.substring(stripped.length());
        }
        return stripped;
    }

    private String reconstructEmptyKey(String content) {
        String rest = content.substring(1);
        boolean hadSpace = !rest.isEmpty() && rest.charAt(0) == ' ';
        if (hadSpace) rest = rest.substring(1);

        String processed;
        if (!rest.isEmpty() && rest.charAt(0) == '"') {
            var qv = parseQuotedValue(rest, 0, 0);
            processed = qv.ok() ? qv.value() : rest;
        } else {
            processed = stripSpaces(rest);
        }

        if (hadSpace) return ":" + " " + processed;
        return ":" + processed;
    }

    // =========================================================================
    // Utility
    // =========================================================================

    private static String stripSpaces(String s) {
        int start = 0, end = s.length();
        while (start < end && s.charAt(start) == ' ') start++;
        while (end > start && s.charAt(end - 1) == ' ') end--;
        return s.substring(start, end);
    }

    private static String stripAllWhitespace(String s) {
        int start = 0, end = s.length();
        while (start < end && (s.charAt(start) == ' ' || s.charAt(start) == '\t')) start++;
        while (end > start && (s.charAt(end - 1) == ' ' || s.charAt(end - 1) == '\t')) end--;
        return s.substring(start, end);
    }
}
