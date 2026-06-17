package stml;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

public class STMLLexer {
    private List<String> lines;
    private int lineCount;
    private List<Token> tokens;
    private List<Warning> warnings;
    private List<Integer> indentStack;
    private int lineIdx;
    private Optional<Integer> deferredDedentTo;

    public STMLLexer(String text) {
        // Normalize line endings
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < text.length(); i++) {
            char ch = text.charAt(i);
            if (ch == '\r') {
                if (i + 1 < text.length() && text.charAt(i + 1) == '\n') i++;
                sb.append('\n');
            } else {
                sb.append(ch);
            }
        }
        String normalized = sb.toString();
        lines = new ArrayList<>();
        int start = 0;
        for (int i = 0; i < normalized.length(); i++) {
            if (normalized.charAt(i) == '\n') {
                lines.add(normalized.substring(start, i));
                start = i + 1;
            }
        }
        if (start <= normalized.length()) {
            lines.add(normalized.substring(start));
        }
        lineCount = lines.size();
        indentStack = new ArrayList<>();
        indentStack.add(0);
        lineIdx = 0;
        deferredDedentTo = Optional.empty();
    }

    public static class TokenizeResult {
        public final List<Token> tokens;
        public final List<Warning> warnings;
        public TokenizeResult(List<Token> tokens, List<Warning> warnings) {
            this.tokens = tokens;
            this.warnings = warnings;
        }
    }

    public TokenizeResult tokenize() {
        tokens = new ArrayList<>();
        warnings = new ArrayList<>();
        indentStack = new ArrayList<>();
        indentStack.add(0);
        lineIdx = 0;
        deferredDedentTo = Optional.empty();

        while (lineIdx < lineCount) {
            String line = lines.get(lineIdx);
            int indent = calcIndent(line);
            String content = line.substring(indent);

            if (isBlankOrComment(content)) {
                lineIdx++;
                continue;
            }

            // Document separator (indent must be 0)
            if (indent == 0 && content.strip().equals("---")) {
                emitDedentsTo(0);
                addToken(TokenType.DOC_SEPARATOR, "---", lineIdx + 1, indent + 1);
                lineIdx++;
                continue;
            }

            processIndent(indent);

            // Dispatch on line type
            String contentStripped = content.strip();
            if (contentStripped.equals("---") && indent > 0) {
                lexMappingLine(content, indent);
            } else if (!content.isEmpty() && content.charAt(0) == '-') {
                lexSequenceLine(content, indent);
            } else {
                lexMappingLine(content, indent);
            }

            lineIdx++;

            if (deferredDedentTo.isPresent()) {
                int target = deferredDedentTo.get();
                deferredDedentTo = Optional.empty();
                emitDedentsTo(target);
            }
        }

        emitDedentsTo(0);
        addToken(TokenType.END, null, lineCount, 1);

        return new TokenizeResult(tokens, warnings);
    }

    // ---- Indentation helpers ----

    private int calcIndent(String line) {
        int n = 0;
        for (int i = 0; i < line.length(); i++) {
            if (line.charAt(i) == ' ') n++;
            else break;
        }
        return n;
    }

    private boolean isBlankOrComment(String content) {
        String s = content.strip();
        return s.isEmpty() || s.charAt(0) == '#';
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

    private void addToken(TokenType type, Object value, int line, int column) {
        tokens.add(new Token(type, value, line, column));
    }

    private void addWarning(int line, int column, String message) {
        warnings.add(new Warning(line, column, message));
    }

    private int col(int indent, int offsetInContent) {
        return indent + offsetInContent + 1;
    }

    // ---- Line dispatch ----

    private void lexSequenceLine(String content, int indent) {
        int lineNo = lineIdx + 1;
        addToken(TokenType.DASH, null, lineNo, col(indent, 0));

        String rest = content.substring(1);
        if (!rest.isEmpty() && rest.charAt(0) == ' ') rest = rest.substring(1);

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
            if (result.ok) {
                addToken(TokenType.KEY, result.key, lineNo, col(indent, 0));
                addToken(TokenType.COLON, null, lineNo, col(indent, result.colonPos));
                lexRemainderAfterColon(result.rest, indent, lineNo);
                addToken(TokenType.NEWLINE, null, lineNo, col(indent, content.length()));
                return;
            }
        }

        // (2) Scan for structural colon
        int colon = findUnquotedColon(content);

        if (colon == -1) {
            // No structural colon → bare-key or null-scalar
            String stripped = content.strip();
            String allStripped = content.replace(" ", "").replace("\t", "");
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

        // (3) Colon at position 0 → empty key
        if (colon == 0) {
            String key = reconstructEmptyKey(content);
            addToken(TokenType.BARE_KEY, key, lineNo, col(indent, 0));
            addToken(TokenType.NEWLINE, null, lineNo, col(indent, content.length()));
            return;
        }

        // (4) Normal key
        String key = content.substring(0, colon);
        String rest = content.substring(colon + 1);

        addToken(TokenType.KEY, key, lineNo, col(indent, 0));
        addToken(TokenType.COLON, null, lineNo, col(indent, colon));
        lexRemainderAfterColon(rest, indent, lineNo);
        addToken(TokenType.NEWLINE, null, lineNo, col(indent, content.length()));
    }

    // ---- Value parsing ----

    private void lexRemainderAfterColon(String rest, int indent, int lineNo) {
        if (rest.isEmpty()) return;
        if (rest.charAt(0) == ' ' || rest.charAt(0) == '\t') rest = rest.substring(1);
        if (rest.isEmpty()) return;
        lexValueOrKeyOnLine(rest, indent, false);
    }

    private void lexValueOrKeyOnLine(String text, int indent, boolean scanColon) {
        int lineNo = lineIdx + 1;
        String stripped = text.strip();

        // Multiline string trigger
        if (stripped.equals("{") && lineIdx + 1 < lineCount) {
            var result = readMultiline(indent);
            addToken(TokenType.MULTILINE_STRING, result.value, lineNo, col(indent, indent));
            lineIdx = result.consumed - 1;
            deferredDedentTo = Optional.of(result.closeIndent);
            return;
        }

        // Inline list
        if (!text.isEmpty() && text.charAt(0) == '[') {
            int endBracket = text.lastIndexOf(']');
            if (endBracket > 0) {
                // Check only whitespace after ]
                boolean onlyWS = true;
                for (int i = endBracket + 1; i < text.length(); i++) {
                    if (text.charAt(i) != ' ') { onlyWS = false; break; }
                }
                if (onlyWS) {
                    var result = parseInlineList(text, indent);
                    if (result.ok) {
                        addToken(TokenType.INLINE_LIST, result.elements, lineNo, col(indent, 0));
                        return;
                    }
                    addWarning(lineNo, result.col > 0 ? result.col : col(indent, 0),
                            "内联列表缺少结尾 ']'，降级为原始字符串");
                    addToken(TokenType.RAW_STRING, text, lineNo, col(indent, 0));
                    return;
                }
            }
            if (text.indexOf(']') == -1) {
                addWarning(lineNo, col(indent, 0), "内联列表缺少结尾 ']'，降级为原始字符串");
                addToken(TokenType.RAW_STRING, text, lineNo, col(indent, 0));
                return;
            }
        }

        // Quoted content
        if (!text.isEmpty() && text.charAt(0) == '"') {
            if (scanColon) {
                var result = tryQuotedKey(text, indent);
                if (result.ok) {
                    addToken(TokenType.KEY, result.key, lineNo, col(indent, 0));
                    addToken(TokenType.COLON, null, lineNo, col(indent, result.colonPos));
                    String rest = result.rest;
                    if (!rest.isEmpty() && rest.charAt(0) == ' ') rest = rest.substring(1);
                    lexValueOrKeyOnLine(rest, indent + text.length() - rest.length(), true);
                    return;
                }
            }
            var result = parseQuotedValue(text, 0, indent);
            if (result.ok) {
                addToken(TokenType.SCALAR, result.value, lineNo, col(indent, 0));
            } else {
                addWarning(lineNo, col(indent, 0), "引号未闭合，降级为原始字符串");
                addToken(TokenType.RAW_STRING, text, lineNo, col(indent, 0));
            }
            return;
        }

        // Unquoted value → check for inline mapping key:value
        if (scanColon) {
            int c = findUnquotedColon(text);
            if (c >= 0) {
                String inlineKey = text.substring(0, c);
                String inlineRest = text.substring(c + 1);
                if (!inlineRest.isEmpty() && inlineRest.charAt(0) == ' ') inlineRest = inlineRest.substring(1);
                addToken(TokenType.KEY, inlineKey, lineNo, col(indent, 0));
                addToken(TokenType.COLON, null, lineNo, col(indent, inlineKey.length()));
                lexValueOrKeyOnLine(inlineRest, indent + inlineKey.length() + 1, true);
                return;
            }
        }

        lexScalarOrNull(text, indent);
    }

    private void lexScalarOrNull(String text, int indent) {
        int lineNo = lineIdx + 1;
        String stripped = stripSpaceOnly(text);
        if (stripped.isEmpty()) return;

        String allStripped = text.replace(" ", "").replace("\t", "");
        if (allStripped.equals("null") || allStripped.equals("~")) {
            addToken(TokenType.NULL_, null, lineNo, col(indent, 0));
        } else {
            addToken(TokenType.SCALAR, stripped, lineNo, col(indent, 0));
        }
    }

    private static String stripSpaceOnly(String s) {
        int start = 0, end = s.length();
        while (start < end && s.charAt(start) == ' ') start++;
        while (end > start && s.charAt(end - 1) == ' ') end--;
        return s.substring(start, end);
    }

    // ---- Quoted key ----

    private static class QuotedKeyResult {
        final String key, rest;
        final boolean ok;
        final int colonPos;
        QuotedKeyResult(String key, String rest, boolean ok, int colonPos) {
            this.key = key; this.rest = rest; this.ok = ok; this.colonPos = colonPos;
        }
    }

    private QuotedKeyResult tryQuotedKey(String content, int indent) {
        int end = findFirstUnescapedQuote(content, 1);
        if (end == -1) return new QuotedKeyResult("", "", false, -1);
        if (end + 1 >= content.length() || content.charAt(end + 1) != ':')
            return new QuotedKeyResult("", "", false, -1);
        String rawKey = content.substring(1, end);
        String key = unescape(rawKey, indent + 1);
        String rest = content.substring(end + 2);
        return new QuotedKeyResult(key, rest, true, end + 1);
    }

    // ---- Colon scan ----

    private int findUnquotedColon(String s) {
        boolean inQuote = false;
        boolean hadClose = false;
        for (int i = 0; i < s.length(); i++) {
            char ch = s.charAt(i);
            if (ch == '\\') { i++; continue; }
            if (ch == '"') {
                if (inQuote) hadClose = true;
                inQuote = !inQuote;
            } else if (ch == ':' && !inQuote) {
                return i;
            }
        }
        if (inQuote && !hadClose) {
            int pos = s.indexOf(':');
            return pos;
        }
        return -1;
    }

    // ---- Quote matching ----

    private int findFirstUnescapedQuote(String s, int start) {
        for (int i = start; i < s.length(); i++) {
            if (s.charAt(i) == '\\') { i++; continue; }
            if (s.charAt(i) == '"') return i;
        }
        return -1;
    }

    private int findLastUnescapedQuote(String s, int start) {
        int last = -1;
        for (int i = start; i < s.length(); i++) {
            if (s.charAt(i) == '\\') { i++; continue; }
            if (s.charAt(i) == '"') last = i;
        }
        return last;
    }

    // ---- Escape processing ----

    private String unescape(String s, int colOffset) {
        StringBuilder result = new StringBuilder();
        for (int i = 0; i < s.length(); i++) {
            if (s.charAt(i) == '\\' && i + 1 < s.length()) {
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
                                "非法转义序列 '\\" + nxt + "', 保留原样");
                }
                i++;
            } else {
                result.append(s.charAt(i));
            }
        }
        return result.toString();
    }

    // ---- Quoted value ----

    private static class ParseQuotedResult {
        final String value;
        final int end;
        final boolean ok;
        ParseQuotedResult(String value, int end, boolean ok) {
            this.value = value; this.end = end; this.ok = ok;
        }
    }

    private ParseQuotedResult parseQuotedValue(String s, int start, int indent) {
        int end = findLastUnescapedQuote(s, start + 1);
        if (end == -1) return new ParseQuotedResult("", start, false);
        String raw = s.substring(start + 1, end);
        String val = unescape(raw, indent + start + 1);
        return new ParseQuotedResult(val, end + 1, true);
    }

    // ---- Inline list ----

    private static class ParseInlineResult {
        final List<InlineElem> elements;
        final boolean ok;
        final int col;
        ParseInlineResult(List<InlineElem> elements, boolean ok, int col) {
            this.elements = elements; this.ok = ok; this.col = col;
        }
    }

    private ParseInlineResult parseInlineList(String s, int indent) {
        int closePos = s.lastIndexOf(']');
        if (closePos == -1) return new ParseInlineResult(null, false, indent + s.length());

        for (int i = closePos + 1; i < s.length(); i++) {
            if (s.charAt(i) != ' ') return new ParseInlineResult(null, false, indent + i);
        }

        String inner = s.substring(1, closePos);
        List<InlineElem> elements = new ArrayList<>();
        int i = 0, n = inner.length();

        while (i < n) {
            while (i < n && inner.charAt(i) == ' ') i++;
            if (i >= n) break;

            if (inner.charAt(i) == '"') {
                int eq = findFirstUnescapedQuote(inner, i + 1);
                if (eq != -1) {
                    String rawElem = inner.substring(i + 1, eq);
                    elements.add(InlineElem.of(unescape(rawElem, indent + i + 2)));
                    i = eq + 1;
                } else {
                    elements.add(InlineElem.of(inner.substring(i)));
                    i = n;
                }
                while (i < n && (inner.charAt(i) == ',' || inner.charAt(i) == ' ')) i++;
            } else {
                int startI = i;
                while (i < n && inner.charAt(i) != ',') i++;
                String elem = inner.substring(startI, i);
                String elemStripped = elem.strip();
                if (elemStripped.equals("null") || elemStripped.equals("~") || elemStripped.isEmpty()) {
                    elements.add(InlineElem.nullElem());
                } else {
                    elements.add(InlineElem.of(elemStripped));
                }
                while (i < n && (inner.charAt(i) == ',' || inner.charAt(i) == ' ')) i++;
            }
        }

        // Trailing comma → extra null
        String innerStripped = inner.stripTrailing();
        if (!innerStripped.isEmpty() && innerStripped.charAt(innerStripped.length() - 1) == ',') {
            elements.add(InlineElem.nullElem());
        }

        return new ParseInlineResult(elements, true, 0);
    }

    // ---- Multiline ----

    private static class MultilineResult {
        final String value;
        final int consumed;
        final int closeIndent;
        MultilineResult(String value, int consumed, int closeIndent) {
            this.value = value; this.consumed = consumed; this.closeIndent = closeIndent;
        }
    }

    private MultilineResult readMultiline(int keyIndent) {
        List<String> parts = new ArrayList<>();
        int i = lineIdx + 1;
        int closeIndent = keyIndent;

        while (i < lineCount) {
            String line = lines.get(i);
            int lineIndent = 0;
            for (int j = 0; j < line.length(); j++) {
                if (line.charAt(j) == ' ') lineIndent++;
                else break;
            }
            String content = line.substring(lineIndent);
            String stripped = content.strip();

            if (stripped.equals("}") && lineIndent <= keyIndent) {
                i++;
                break;
            }

            parts.add(line);
            i++;
        }

        if (i >= lineCount) {
            addWarning(i, 1, "多行字符串未找到闭合 '}', 已由 EOF 自动闭合");
        }

        return new MultilineResult(String.join("\n", parts), i, closeIndent);
    }

    // ---- Bare key helpers ----

    private String bareKeyFromContent(String content) {
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
            var result = parseQuotedValue(rest, 0, 0);
            processed = result.ok ? result.value : rest;
        } else {
            processed = rest.strip();
        }

        if (hadSpace) return ":" + " " + processed;
        return ":" + processed;
    }
}