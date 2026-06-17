package stml;

import java.util.List;

public class Token {
    public final TokenType type;
    public final Object value; // String, List<InlineElem>, or null
    public final int line;
    public final int column;

    public Token(TokenType type, Object value, int line, int column) {
        this.type = type;
        this.value = value;
        this.line = line;
        this.column = column;
    }

    @SuppressWarnings("unchecked")
    public List<InlineElem> asInlineList() {
        return (List<InlineElem>) value;
    }

    public String asString() {
        return (String) value;
    }
}