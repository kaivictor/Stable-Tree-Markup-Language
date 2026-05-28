package com.stml.lexer;

import java.util.List;

/**
 * A single lexical token with source location.
 * value is:
 *   null         — structural tokens (NEWLINE, INDENT, DEDENT, COLON, DASH, END, NULL_, DOC_SEPARATOR)
 *   String       — SCALAR, KEY, BARE_KEY, RAW_STRING, MULTILINE_STRING
 *   List<InlineElem> — INLINE_LIST (already parsed elements)
 */
public record Token(TokenType type, Object value, int line, int column) {

    public String stringValue() {
        return (String) value;
    }

    @SuppressWarnings("unchecked")
    public List<InlineElem> inlineListValue() {
        return (List<InlineElem>) value;
    }
}
