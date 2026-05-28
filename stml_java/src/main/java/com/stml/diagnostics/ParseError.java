package com.stml.diagnostics;

/** Fatal error during parsing. Cannot recover an AST. */
public class ParseError extends RuntimeException {
    public final int line;
    public final int column;
    public final String message;

    public ParseError(int line, int column, String message) {
        super(line + ":" + column + ": " + message);
        this.line = line;
        this.column = column;
        this.message = message;
    }
}
