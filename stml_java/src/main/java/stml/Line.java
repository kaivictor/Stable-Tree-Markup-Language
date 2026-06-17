package stml;

import java.util.ArrayList;
import java.util.List;

public class Line {
    public enum Kind {
        DASH_EMPTY,
        DASH_SCALAR,
        DASH_KEY_VAL,
        KEY_VAL,
        BARE_KEY,
    }

    public Kind kind;
    public String key = "";
    public AstNode inlineValue = new AstNode();
    public int indent;
    public int lineNo;
    public List<Line> children = new ArrayList<>();

    public Line() {
        this.kind = Kind.BARE_KEY;
        this.indent = 0;
        this.lineNo = 0;
    }

    public Line(Kind kind, String key, AstNode inlineValue, int indent, int lineNo) {
        this.kind = kind;
        this.key = key;
        this.inlineValue = inlineValue;
        this.indent = indent;
        this.lineNo = lineNo;
    }

    public static Line shallowCopy(Line other) {
        Line copy = new Line(other.kind, other.key, AstNode.deepClone(other.inlineValue), other.indent, other.lineNo);
        copy.children = new ArrayList<>(other.children);
        return copy;
    }

    public boolean isDash() {
        return kind == Kind.DASH_EMPTY || kind == Kind.DASH_SCALAR || kind == Kind.DASH_KEY_VAL;
    }

    public boolean hasInlineValue() {
        return !inlineValue.isNull();
    }

    public boolean hasChildren() {
        return !children.isEmpty();
    }
}