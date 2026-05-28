package com.stml.diagnostics;

/** Non-fatal diagnostic during parsing. The AST is still well-formed. */
public record Warning(int line, int column, String message) {
    @Override
    public String toString() {
        return line + ":" + column + ": " + message;
    }
}
