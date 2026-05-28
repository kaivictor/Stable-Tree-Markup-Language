package com.stml.ast;

import java.util.ArrayList;
import java.util.List;

/** Ordered sequence AST node. */
public record AstList(List<AstNode> items) implements AstNode {
    public AstList() { this(new ArrayList<>()); }
}
