package stml;

public class Warning {
    public final int line;
    public final int column;
    public final String message;

    public Warning(int line, int column, String message) {
        this.line = line;
        this.column = column;
        this.message = message;
    }
}