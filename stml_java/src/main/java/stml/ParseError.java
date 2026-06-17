package stml;

public class ParseError extends RuntimeException {
    public final int line;
    public final int column;

    public ParseError(int line, int column, String message) {
        super(line + ":" + column + ": " + message);
        this.line = line;
        this.column = column;
    }
}