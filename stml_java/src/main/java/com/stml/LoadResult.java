package com.stml;

import com.stml.ast.AstNode;
import com.stml.diagnostics.Warning;

import java.util.List;

/** Result of parsing: the docs-wrapped AST plus all warnings. */
public record LoadResult(AstNode ast, List<Warning> warnings) {}
