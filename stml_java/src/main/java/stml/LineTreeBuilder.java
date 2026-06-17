package stml;

import java.util.ArrayList;
import java.util.List;

public class LineTreeBuilder {
    private List<List<Line>> docs;
    private List<List<Line>> blocks;
    private List<Integer> indentStack;
    private Line pending;
    private boolean hasPending;
    private boolean pendingHasDash;
    private boolean pendingHasColon;
    private boolean pendingHasKey;
    private boolean hasPushedDoc;
    private boolean hasContent;
    private List<Warning> warnings;

    public LineTreeBuilder() {
        reset();
    }

    public void reset() {
        docs = new ArrayList<>();
        blocks = new ArrayList<>();
        blocks.add(new ArrayList<>());
        indentStack = new ArrayList<>();
        indentStack.add(0);
        pending = new Line();
        hasPending = false;
        pendingHasDash = false;
        pendingHasColon = false;
        pendingHasKey = false;
        hasPushedDoc = false;
        hasContent = false;
        warnings = new ArrayList<>();
    }

    public void process(List<Token> tokens) {
        for (Token t : tokens) processToken(t);
    }

    public void processToken(Token token) {
        switch (token.type) {
            case INDENT: {
                finalizeLine();
                int delta = Integer.parseInt(token.asString());
                int absoluteIndent = indentStack.get(indentStack.size() - 1) + delta;
                indentStack.add(absoluteIndent);
                blocks.add(new ArrayList<>());
                break;
            }
            case DEDENT:
                finalizeLine();
                closeIndentLevel();
                if (indentStack.size() > 1) indentStack.remove(indentStack.size() - 1);
                break;

            case DASH:
                finalizeLine();
                startNewLine(Line.Kind.DASH_EMPTY);
                pendingHasDash = true;
                pendingHasColon = false;
                pendingHasKey = false;
                pending.indent = indentStack.get(indentStack.size() - 1);
                pending.lineNo = token.line;
                break;

            case KEY:
                if (pendingHasDash && pending.kind == Line.Kind.DASH_EMPTY) {
                    pending.kind = Line.Kind.DASH_KEY_VAL;
                    pending.key = token.asString();
                    pendingHasKey = true;
                    pendingHasColon = false;
                } else {
                    finalizeLine();
                    startNewLine(Line.Kind.KEY_VAL);
                    pending.key = token.asString();
                    pendingHasKey = true;
                    pendingHasColon = false;
                    pendingHasDash = false;
                    pending.indent = indentStack.get(indentStack.size() - 1);
                    pending.lineNo = token.line;
                }
                break;

            case COLON:
                pendingHasColon = true;
                break;

            case SCALAR:
            case NULL_:
            case INLINE_LIST:
            case MULTILINE_STRING:
            case RAW_STRING:
                if (pendingHasDash && !pendingHasColon && !pendingHasKey) {
                    pending.kind = Line.Kind.DASH_SCALAR;
                    pending.inlineValue = tokenToAstValue(token);
                } else if (pendingHasColon) {
                    pending.inlineValue = tokenToAstValue(token);
                }
                break;

            case BARE_KEY:
                if (pendingHasDash && pending.kind == Line.Kind.DASH_EMPTY) {
                    pending.kind = Line.Kind.DASH_SCALAR;
                    pending.inlineValue = new AstNode(token.asString());
                } else {
                    finalizeLine();
                    startNewLine(Line.Kind.BARE_KEY);
                    pending.key = token.asString();
                    pendingHasKey = true;
                    pendingHasDash = false;
                    pendingHasColon = false;
                    pending.indent = indentStack.get(indentStack.size() - 1);
                    pending.lineNo = token.line;
                }
                break;

            case NEWLINE:
                finalizeLine();
                break;

            case DOC_SEPARATOR:
                finalizeLine();
                while (blocks.size() > 1) closeIndentLevel();
                if (!blocks.isEmpty()) {
                    docs.add(new ArrayList<>(blocks.get(0)));
                    hasPushedDoc = true;
                    blocks.get(0).clear();
                    hasContent = false;
                    indentStack = new ArrayList<>();
                    indentStack.add(0);
                }
                break;

            case END:
                finalizeLine();
                while (blocks.size() > 1) closeIndentLevel();
                if (!blocks.isEmpty()) {
                    if (hasContent || hasPushedDoc) {
                        docs.add(new ArrayList<>(blocks.get(0)));
                    }
                }
                break;
        }
    }

    private void finalizeLine() {
        if (!hasPending) return;

        if (pending.indent == 0 && blocks.size() > 1) {
            pending.indent = indentStack.get(indentStack.size() - 1);
        }

        // Same-level absorption: irregular indent, non-dash BARE_KEY should absorb into last dash entry
        if (!pending.isDash() && pending.kind == Line.Kind.BARE_KEY && !blocks.get(blocks.size() - 1).isEmpty()) {
            boolean blockHasDash = false;
            for (Line l : blocks.get(blocks.size() - 1)) {
                if (l.isDash()) { blockHasDash = true; break; }
            }
            if (blockHasDash) {
                List<Line> block = blocks.get(blocks.size() - 1);
                Line lastDash = null;
                for (int i = block.size() - 1; i >= 0; i--) {
                    if (block.get(i).isDash()) { lastDash = block.get(i); break; }
                }
                if (lastDash != null) {
                    lastDash.children.add(new Line(pending.kind, pending.key, pending.inlineValue, pending.indent, pending.lineNo));
                    pending = new Line();
                    hasPending = false;
                    pendingHasDash = false;
                    pendingHasColon = false;
                    pendingHasKey = false;
                    return;
                }
            }
        }

        blocks.get(blocks.size() - 1).add(new Line(pending.kind, pending.key, pending.inlineValue, pending.indent, pending.lineNo));

        if (blocks.size() <= 1) {
            hasContent = true;
        }

        pending = new Line();
        hasPending = false;
        pendingHasDash = false;
        pendingHasColon = false;
        pendingHasKey = false;
    }

    private void startNewLine(Line.Kind kind) {
        pending = new Line();
        pending.kind = kind;
        hasPending = true;
    }

    private void closeIndentLevel() {
        if (blocks.size() <= 1) return;

        List<Line> child = new ArrayList<>(blocks.remove(blocks.size() - 1));
        if (child.isEmpty()) return;

        List<Line> parent = blocks.get(blocks.size() - 1);
        if (parent.isEmpty()) {
            blocks.set(blocks.size() - 1, child);
            return;
        }

        boolean parentHasDash = false;
        for (Line line : parent) {
            if (line.isDash()) { parentHasDash = true; break; }
        }

        Line parentLast = parent.get(parent.size() - 1);

        boolean childAllDash = true;
        boolean childHasKey = false;
        for (Line line : child) {
            if (!line.isDash()) childAllDash = false;
            if (line.kind == Line.Kind.KEY_VAL || line.kind == Line.Kind.BARE_KEY) childHasKey = true;
        }

        // Rule 1: parent last is open key and child type compatible
        if (isOpen(parentLast)) {
            boolean parentChildrenAllDash = true;
            for (Line c : parentLast.children) {
                if (!c.isDash()) { parentChildrenAllDash = false; break; }
            }
            boolean compatible = parentLast.children.isEmpty()
                    || (parentChildrenAllDash && childAllDash)
                    || (!parentChildrenAllDash && childHasKey);
            if (compatible) {
                for (Line line : child) parentLast.children.add(Line.shallowCopy(line));
                return;
            }
        }

        // Rule 2: parent has dash, child has key → absorb into last dash
        if (parentHasDash && childHasKey) {
            Line lastDash = null;
            for (int i = parent.size() - 1; i >= 0; i--) {
                if (parent.get(i).isDash()) { lastDash = parent.get(i); break; }
            }
            if (lastDash != null) {
                for (Line line : child) lastDash.children.add(Line.shallowCopy(line));
            }
            return;
        }

        // Rule 3: child all dash → merge as siblings
        if (childAllDash) {
            for (Line line : child) parent.add(Line.shallowCopy(line));
            return;
        }

        // Rule 4: non-dash parent + non-dash child → merge as siblings
        if (!childAllDash && !parentHasDash) {
            for (Line line : child) parent.add(Line.shallowCopy(line));
            return;
        }

        // Default: true nesting
        parentLast.children = child;
    }

    private boolean isOpen(Line l) {
        if (l.kind != Line.Kind.KEY_VAL && l.kind != Line.Kind.DASH_KEY_VAL && l.kind != Line.Kind.BARE_KEY)
            return false;
        if (l.hasInlineValue()) return false;
        boolean hasKey = false, hasDash = false;
        for (Line c : l.children) {
            if (c.isDash()) hasDash = true;
            else hasKey = true;
        }
        return !(hasKey && hasDash);
    }

    private AstNode tokenToAstValue(Token t) {
        switch (t.type) {
            case NULL_:
                return new AstNode();
            case SCALAR:
            case MULTILINE_STRING:
            case RAW_STRING:
                return new AstNode(t.asString());
            case INLINE_LIST: {
                List<AstNode> list = new ArrayList<>();
                for (InlineElem elem : t.asInlineList()) {
                    if (!elem.isNull()) list.add(new AstNode(elem.get()));
                    else list.add(new AstNode());
                }
                return new AstNode(list);
            }
            default:
                return new AstNode();
        }
    }

    public List<List<Line>> documents() { return docs; }
    public List<Warning> warnings() { return warnings; }
}