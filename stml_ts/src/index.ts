/// STML — Stable Tree Markup Language TypeScript Library
///
/// Public API
/// ----------
///   loads(text)         → { ast, warnings }  parse STML string
///   load(filename)      → { ast, warnings }  parse STML file
///   dumps(ast)          → string             AST → canonical STML
///   to_json(ast)        → string             AST → JSON string
///   tokenize(text)      → { tokens, warnings }  lex only (debug)
///   parse(tokens)       → { ast, warnings }     parse only (debug)

import * as fs from 'fs';
import { nullNode, stringNode, listNode, mapNode, AstNode } from './ast';
import { STMLLexer, TokenType, Token, Warning, ParseError, InlineElem } from './lexer';
import { LineTreeBuilder, AstBuilder, Line, LineKind } from './parser';
import { dumps, toJson, serializeNode, formatScalar } from './serializer';

// ---- Types ----
export interface LoadResult {
  ast: AstNode;
  warnings: Warning[];
}

export interface TokenizeResult {
  tokens: Token[];
  warnings: Warning[];
}

export interface ParseResult {
  ast: AstNode[];
  warnings: Warning[];
}

// =========================================================================
// loads — parse STML text
// =========================================================================

export function loads(text: string): LoadResult {
  const lexer = new STMLLexer(text);
  const { tokens, warnings: lexWarnings } = lexer.tokenize();

  const treeBuilder = new LineTreeBuilder();
  treeBuilder.process(tokens);

  const astBuilder = new AstBuilder();
  const docsList = treeBuilder.docs.map(docLines => astBuilder.build(docLines));

  const allWarnings = [
    ...lexWarnings,
    ...treeBuilder.warnings,
    ...astBuilder.warnings_,
  ];

  const wrapper: Array<[string, AstNode]> = [['docs', listNode(docsList)]];
  return { ast: mapNode(wrapper), warnings: allWarnings };
}

// =========================================================================
// load — parse an STML file
// =========================================================================

export function load(filename: string): LoadResult {
  const text = fs.readFileSync(filename, 'utf-8');
  return loads(text);
}

// =========================================================================
// tokenize — lex only (debug utility)
// =========================================================================

export function tokenize(text: string): TokenizeResult {
  const lexer = new STMLLexer(text);
  return lexer.tokenize();
}

// =========================================================================
// parse — parse only (debug utility)
// =========================================================================

export function parse(tokenArray: Token[]): ParseResult {
  const treeBuilder = new LineTreeBuilder();
  treeBuilder.process(tokenArray);

  const astBuilder = new AstBuilder();
  const docsList = treeBuilder.docs.map(docLines => astBuilder.build(docLines));

  const allWarnings = [
    ...treeBuilder.warnings,
    ...astBuilder.warnings_,
  ];

  return { ast: docsList, warnings: allWarnings };
}

// Re-export everything
export { dumps, toJson, toJson as to_json, serializeNode, formatScalar };
export { AstNode, nullNode, stringNode, listNode, mapNode, mapFind, mapSet, clone, astEqual, walk, AstMap, AstList, AstVisitor } from './ast';
export { STMLLexer, TokenType, Token, Warning, ParseError, InlineElem } from './lexer';
export { LineTreeBuilder, AstBuilder, Line, LineKind } from './parser';