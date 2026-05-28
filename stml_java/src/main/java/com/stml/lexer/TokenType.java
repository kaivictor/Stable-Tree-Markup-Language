package com.stml.lexer;

/** The 14 token types produced by the lexer. */
public enum TokenType {
    // Structural
    NEWLINE,
    INDENT,
    DEDENT,
    DOC_SEPARATOR,
    END,

    // Mapping
    KEY,
    BARE_KEY,
    COLON,

    // Values
    SCALAR,
    NULL_,
    RAW_STRING,

    // Sequence
    DASH,

    // Special
    INLINE_LIST,
    MULTILINE_STRING,
}
