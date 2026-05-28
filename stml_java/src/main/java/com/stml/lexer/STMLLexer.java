package com.stml.lexer;

import com.stml.diagnostics.Warning;

import java.util.ArrayList;
import java.util.List;

/**
 * STMLLexer — converts STML text into a token stream.
 * Single entry point: tokenize().
 * All deterministic recovery rules live here so the resulting token stream
 * is fully determined for any input.
 */
public class STMLLexer {

    // Input
    private final List<String> lines;
    private final int lineCount;

    // Accumulators
    private final List<Token> tokens = new ArrayList<>();
    private final List<Warning> warnings = new ArrayList<>();

    // Indentation stack (starts with implicit root at indent 0)
    private final List<Integer> indentStack = new ArrayList<>();

    // Current position in lines
    private int lineIdx;

    // Deferred DEDENT target: set by multiline-string parsing.
    private Integer deferredDedentTo = null;

    public STMLLexer(String text) {
        // Normalise line endings: \r\n → \n, \r → \n
        StringBuilder normalised = new StringBuilder(text.length());
        for (int i = 0; i < text.length(); i++) {
            char ch = text.charAt(i);
            if (ch == '\r') {
                if (i + 1 < text.length() && text.charAt(i + 1) == '\n') {
                    i++; // skip \n of \r\n
                }
                normalised.append('\n');
            } else {
                normalised.append(ch);
            }
        }

        // Split into lines
        lines = new ArrayList<>();
        int start = 0;
        String s = normalised.toString();
        for (int i = 0; i < s.length(); i++) {
            if (s.charAt(i) == '\n') {
                lines.add(s.substring(start, i));
                start = i + 1;
            }
        }
        if (start <= s.length()) {
            lines.add(s.substring(start));
        }
        lineCount = lines.size();

        indentStack.add(0);
        lineIdx = 0;
    }

    // =========================================================================
    // Public API
    // =========================================================================

    public TokenizeResult tokenize() {
        tokens.clear();
        warnings.clear();
        indentStack.clear();
        indentStack.add(0);
        lineIdx = 0;
        deferredDedentTo = null;

        while (lineIdx < lineCount) {
            String line = lines.get(lineIdx);
            int indent = calcIndent(line);
            String content = line.substring(indent);

            // Skip empty lines and whole-line comments
            if (isBlankOrComment(content)) {
                lineIdx++;
                continue;
            }

            // Document separator (indent must be 0)
            String stripped = stripSpaces(content);
            if (indent == 0 && stripped.equals("---")) {
                emitDedentsTo(0);
                addToken(TokenType.DOC_SEPARATOR, "---", lineIdx + 1, indent + 1);
                lineIdx++;
                continue;
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

            lineIdx++;

            // Emit deferred DEDENTs from multiline-string closure
            if (deferredDedentTo != null) {
                int target = deferredDedentTo;
                deferredDedentTo = null;
                emitDedentsTo(target);
            }
        }

        // EOF: pop remaining indentation → DEDENTs
        emitDedentsTo(0);
        addToken(TokenType.END, null, lineCount, 1);

        return new TokenizeResult(new ArrayList<>(tokens), new ArrayList<>(warnings));
    }

    public List<Warning> getWarnings() {
        return warnings;
    }

    // =========================================================================
    // Result type
    // =========================================================================

    public record TokenizeResult(List<Token> tokens, List<Warning> warnings) {}

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
            // If indent > new top, emit INDENT to re-enter at new level
            if (indent > indentStack.get(indentStack.size() - 1)) {
                int diff = indent - indentStack.get(indentStack.size() - 1);
                indentStack.add(indent);
                addToken(TokenType.INDENT, String.valueOf(diff), lineIdx + 1, 1);
            }
        }
        // indent == top → nothing to do
    }

    private void emitDedentsTo(int target) {
        while (indentStack.get(indentStack.size() - 1) > target) {
            indentStack.remove(indentStack.size() - 1);
            addToken(TokenType.DEDENT, null, lineIdx + 1, 1);
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
    // Line-level dispatch
    // =========================================================================

    private void lexSequenceLine(String content, int indent) {
        int lineNo = lineIdx + 1;
        addToken(TokenType.DASH, null, lineNo, col(indent, 0));

        String rest = content.substring(1); // strip '-'
        if (!rest.isEmpty() && rest.charAt(0) == ' ')
            rest = rest.substring(1); // strip optional space

        if (rest.isEmpty()) {
            addToken(TokenType.NEWLINE, null, lineNo, col(indent, content.length()));
            return;
        }

        lexValueOrKeyOnLine(rest, indent, true);
        addToken(TokenType.NEWLINE, null, lineNo, col(indent, content.length()));
    }

    private void lexMappingLine(String content, int indent) {
        int lineNo = lineIdx + 1;

        // (1) Try quoted-key mode
        if (!content.isEmpty() && content.charAt(0) == '"') {
            var result = tryQuotedKey(content, indent);
            if (result.ok()) {
                addToken(TokenType.KEY, result.key(), lineNo, col(indent, 0));
                addToken(TokenType.COLON, null, lineNo, col(indent, result.colonPos()));
                lexRemainderAfterColon(result.rest(), indent, lineNo);
                addToken(TokenType.NEWLINE, null, lineNo, col(indent, content.length()));
                return;
            }
        }

        // (2) Scan for structural colon
        int colon = findUnquotedColon(content);

        if (colon == -1) {
            // No structural colon → bare-key or null-scalar
            String stripped = stripSpaces(content);
            String allStripped = stripAllWhitespace(content);

            if (allStripped.equals("null") || allStripped.equals("~")) {
                addToken(TokenType.NULL_, null, lineNo, col(indent, 0));
                addToken(TokenType.NEWLINE, null, lineNo, col(indent, content.length()));
                return;
            }

            String key = bareKeyFromContent(content);
            addToken(TokenType.BARE_KEY, key, lineNo, col(indent, 0));
            addToken(TokenType.NEWLINE, null, lineNo, col(indent, content.length()));
            return;
        }

        // (3) Colon at position 0 → empty key → treat as BARE_KEY
        if (colon == 0) {
            String key = reconstructEmptyKey(content);
            addToken(TokenType.BARE_KEY, key, lineNo, col(indent, 0));
            addToken(TokenType.NEWLINE, null, lineNo, col(indent, content.length()));
            return;
        }

        // (4) Normal key: content before colon
        String key = content.substring(0, colon); // preserve trailing spaces
        String rest = content.substring(colon + 1);

        addToken(TokenType.KEY, key, lineNo, col(indent, 0));
        addToken(TokenType.COLON, null, lineNo, col(indent, colon));
        lexRemainderAfterColon(rest, indent, lineNo);
        addToken(TokenType.NEWLINE, null, lineNo, col(indent, content.length()));
    }

    // =========================================================================
    // Value parsing (inline content after COLON or DASH)
    // =========================================================================

    private void lexRemainderAfterColon(String rest, int indent, int lineNo) {
        if (rest.isEmpty()) return;

        if (rest.charAt(0) == ' ' || rest.charAt(0) == '\t')
            rest = rest.substring(1); // consume optional single space or tab

        if (rest.isEmpty()) return;

        lexValueOrKeyOnLine(rest, indent, false);
    }

    private void lexValueOrKeyOnLine(String text, int indent, boolean scanColon) {
        int lineNo = lineIdx + 1;

        String stripped = stripSpaces(text);

        // Multiline string trigger: content is exactly '{'
        if (stripped.equals("{") && lineIdx + 1 < lineCount) {
            var mlResult = readMultiline(indent);
            addToken(TokenType.MULTILINE_STRING, mlResult.value(), lineNo, col(indent, indent));
            lineIdx = mlResult.consumed() - 1; // -1 because caller will ++
            deferredDedentTo = mlResult.closeIndent();
            return;
        }

        // Inline list: only triggers when both '[' and ']' are on the line
        if (!text.isEmpty() && text.charAt(0) == '[') {
            int ep = text.length() - 1;
            while (ep >= 0 && text.charAt(ep) == ' ') ep--;
            if (ep > 0 && text.charAt(ep) == ']') {
                var listResult = parseInlineList(text, indent);
                if (listResult.ok()) {
                    addToken(TokenType.INLINE_LIST, listResult.elements(), lineNo, col(indent, 0));
                    return;
                }
                // Malformed inline list → degrade to raw string
                addWarning(lineNo, listResult.errorCol() != 0 ? listResult.errorCol() : col(indent, 0),
                        "内联列表缺少结尾 ']'，降级为原始字符串");
                addToken(TokenType.RAW_STRING, text, lineNo, col(indent, 0));
                return;
            }
        }

        // Value starts with '[' but no matching ']' → malformed
        if (!text.isEmpty() && text.charAt(0) == '[') {
            if (!text.contains("]")) {
                addWarning(lineNo, col(indent, 0),
                        "内联列表缺少结尾 ']'，降级为原始字符串");
                addToken(TokenType.RAW_STRING, text, lineNo, col(indent, 0));
                return;
            }
        }

        // Quoted content
        if (!text.isEmpty() && text.charAt(0) == '"') {
            if (scanColon) {
                // Non-greedy: try "key": value pattern
                var result = tryQuotedKey(text, indent);
                if (result.ok()) {
                    addToken(TokenType.KEY, result.key(), lineNo, col(indent, 0));
                    addToken(TokenType.COLON, null, lineNo, col(indent, result.colonPos()));
                    String r = result.rest();
                    int consumed = text.length() - r.length();
                    if (!r.isEmpty() && r.charAt(0) == ' ')
                        r = r.substring(1);
                    lexValueOrKeyOnLine(r, indent + consumed, true);
                    return;
                }
            }
            // Greedy quoted value
            var qv = parseQuotedValue(text, 0, indent);
            if (qv.ok()) {
                addToken(TokenType.SCALAR, qv.value(), lineNo, col(indent, 0));
            } else {
                addWarning(lineNo, col(indent, 0), "引号未闭合，降级为原始字符串");
                addToken(TokenType.RAW_STRING, text, lineNo, col(indent, 0));
            }
            return;
        }

        // Unquoted value → for sequence entries, check for inline mapping key:value
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
        if (stripped.isEmpty()) return; // no token; parser handles

        // For null/~ check, strip all whitespace (including tabs)
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

    private QuotedKeyResult tryQuotedKey(String content, int indent) {
        int end = findFirstUnescapedQuote(content, 1);
        if (end == -1)
            return QuotedKeyResult.FAIL;
        // Must be immediately followed by ':'
        if (end + 1 >= content.length() || content.charAt(end + 1) != ':')
            return QuotedKeyResult.FAIL;
        String rawKey = content.substring(1, end);
        String key = unescape(rawKey, indent + 1);
        String rest = content.substring(end + 2);
        return new QuotedKeyResult(key, rest, true, end + 1);
    }

    private record QuotedKeyResult(String key, String rest, boolean ok, int colonPos) {
        static final QuotedKeyResult FAIL = new QuotedKeyResult("", "", false, -1);
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
            if (ch == '\\') {
                i++; // skip next
                continue;
            }
            if (ch == '"') {
                if (inQuote) hadClose = true;
                inQuote = !inQuote;
            } else if (ch == ':' && !inQuote) {
                return i;
            }
        }

        // Unclosed quote that was never closed → '"' is probably key-name char
        if (inQuote && !hadClose) {
            int pos = s.indexOf(':');
            return pos;
        }
        return -1;
    }

    // =========================================================================
    // Quote matching
    // =========================================================================

    private int findFirstUnescapedQuote(String s, int start) {
        int i = start;
        int n = s.length();
        while (i < n) {
            if (s.charAt(i) == '\\') {
                i += 2;
                continue;
            }
            if (s.charAt(i) == '"')
                return i;
            i++;
        }
        return -1;
    }

    private int findLastUnescapedQuote(String s, int start) {
        int last = -1;
        int i = start;
        int n = s.length();
        while (i < n) {
            if (s.charAt(i) == '\\') {
                i += 2;
                continue;
            }
            if (s.charAt(i) == '"')
                last = i;
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
                        // Illegal escape — keep as-is
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

    private QuotedValueResult parseQuotedValue(String s, int start, int indent) {
        int end = findLastUnescapedQuote(s, start + 1);
        if (end == -1)
            return new QuotedValueResult("", start, false);
        String raw = s.substring(start + 1, end);
        String val = unescape(raw, indent + start + 1);
        return new QuotedValueResult(val, end + 1, true);
    }

    private record QuotedValueResult(String value, int endPos, boolean ok) {}

    // =========================================================================
    // Inline list
    // =========================================================================

    private InlineListResult parseInlineList(String s, int indent) {
        int closePos = s.lastIndexOf(']');
        if (closePos == -1)
            return new InlineListResult(List.of(), false, indent + s.length());

        // Check content after ']' is just whitespace
        for (int i = closePos + 1; i < s.length(); i++) {
            if (s.charAt(i) != ' ') {
                return new InlineListResult(List.of(), false, indent + i);
            }
        }

        String inner = s.substring(1, closePos);
        List<InlineElem> elements = new ArrayList<>();
        int i = 0;
        int n = inner.length();

        while (i < n) {
            // Skip leading spaces
            while (i < n && inner.charAt(i) == ' ') i++;
            if (i >= n) break;

            if (inner.charAt(i) == '"') {
                int eq = findFirstUnescapedQuote(inner, i + 1);
                if (eq != -1) {
                    String rawElem = inner.substring(i + 1, eq);
                    elements.add(new InlineElem.StringElem(unescape(rawElem, indent + i + 2)));
                    i = eq + 1;
                } else {
                    // Unclosed quote — take rest as raw
                    elements.add(new InlineElem.StringElem(inner.substring(i)));
                    i = n;
                }
                // Skip comma and spaces
                while (i < n && (inner.charAt(i) == ',' || inner.charAt(i) == ' ')) i++;
            } else {
                // Unquoted element — stops at ','
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
                // Skip comma and spaces
                while (i < n && (inner.charAt(i) == ',' || inner.charAt(i) == ' ')) i++;
            }
        }

        // Trailing comma → extra null element
        String rstrip = inner.stripTrailing();
        if (!rstrip.isEmpty() && rstrip.charAt(rstrip.length() - 1) == ',')
            elements.add(InlineElem.NullElem.INSTANCE);

        return new InlineListResult(elements, true, 0);
    }

    private record InlineListResult(List<InlineElem> elements, boolean ok, int errorCol) {}

    // =========================================================================
    // Multiline string
    // =========================================================================

    private MultilineResult readMultiline(int keyIndent) {
        List<String> parts = new ArrayList<>();
        int i = lineIdx + 1; // start after '{' line
        int closeIndent = keyIndent; // default (EOF case)

        while (i < lineCount) {
            String line = lines.get(i);

            int lineIndent = 0;
            for (int j = 0; j < line.length(); j++) {
                if (line.charAt(j) == ' ') lineIndent++;
                else break;
            }
            String content = line.substring(lineIndent);
            String stripped = stripSpaces(content);

            // Standalone '}' at correct indentation → close
            if (stripped.equals("}") && lineIndent <= keyIndent) {
                closeIndent = lineIndent;
                i++; // consume closing '}'
                break;
            }

            // Preserve raw line
            parts.add(line);
            i++;
        }

        // i == lineCount → EOF auto-close
        if (i >= lineCount) {
            addWarning(i, 1, "多行字符串未找到闭合 '}'，已由 EOF 自动闭合");
        }

        // Join parts with newline
        String result = String.join("\n", parts);

        return new MultilineResult(result, i, closeIndent);
    }

    private record MultilineResult(String value, int consumed, int closeIndent) {}

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
        String rest = content.substring(1); // after ':'
        boolean hadSpace = !rest.isEmpty() && rest.charAt(0) == ' ';
        if (hadSpace)
            rest = rest.substring(1);

        String processed;
        if (!rest.isEmpty() && rest.charAt(0) == '"') {
            var qv = parseQuotedValue(rest, 0, 0);
            if (qv.ok())
                processed = qv.value();
            else
                processed = rest;
        } else {
            processed = stripSpaces(rest);
        }

        if (hadSpace)
            return ":" + " " + processed;
        return ":" + processed;
    }

    // =========================================================================
    // Utility
    // =========================================================================

    private static String stripSpaces(String s) {
        int start = 0;
        int end = s.length();
        while (start < end && s.charAt(start) == ' ') start++;
        while (end > start && s.charAt(end - 1) == ' ') end--;
        return s.substring(start, end);
    }

    private static String stripAllWhitespace(String s) {
        int start = 0;
        int end = s.length();
        while (start < end && (s.charAt(start) == ' ' || s.charAt(start) == '\t')) start++;
        while (end > start && (s.charAt(end - 1) == ' ' || s.charAt(end - 1) == '\t')) end--;
        return s.substring(start, end);
    }
}
