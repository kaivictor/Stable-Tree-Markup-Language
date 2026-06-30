/// STML — Stable Tree Markup Language JavaScript Library
///
/// Public API
/// ----------
///   loads(text)         → (ast, warnings)       parse STML string
///   load(filename)      → (ast, warnings)       parse STML file
///   dumps(ast)          → str                   AST → canonical STML
///   to_json(ast)        → str                   AST → JSON string
///   tokenize(text)      → (tokens, warnings)    lex only (debug)
///   parse(tokens)       → (ast, warnings)       parse only (debug)

const fs = require('fs');
const { nullNode, stringNode, listNode, mapNode } = require('./lib/ast');
const { STMLLexer, TokenType, Token, Warning, ParseError } = require('./lib/lexer');
const { LineTreeBuilder, AstBuilder } = require('./lib/parser');
const { dumps, toJson } = require('./lib/serializer');

// =========================================================================
// loads — parse STML text
// =========================================================================

function loads(text) {
  const lexer = new STMLLexer(text);
  const { tokens, warnings: lexWarnings } = lexer.tokenize();

  const treeBuilder = new LineTreeBuilder();
  treeBuilder.process(tokens);

  const astBuilder = new AstBuilder();
  const docsList = treeBuilder.docs.map(docLines => astBuilder.build(docLines));

  // Merge warnings from all phases
  const allWarnings = [
    ...lexWarnings,
    ...treeBuilder.warnings,
    ...astBuilder.warnings_,
  ];

  // Wrap in {"docs": [{...}, ...]}
  const wrapper = [['docs', listNode(docsList)]];
  return { ast: mapNode(wrapper), warnings: allWarnings };
}

// =========================================================================
// load — parse an STML file
// =========================================================================

function load(filename) {
  const text = fs.readFileSync(filename, 'utf-8');
  return loads(text);
}

// =========================================================================
// tokenize — lex only (debug utility)
// =========================================================================

function tokenize(text) {
  const lexer = new STMLLexer(text);
  return lexer.tokenize();
}

// =========================================================================
// parse — parse only (debug utility)
// =========================================================================

function parse(tokenArray) {
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

// =========================================================================
// Exports
// =========================================================================

module.exports = {
  // Main API
  loads,
  load,
  dumps,
  to_json: toJson,
  tokenize,
  parse,

  // Types
  TokenType,
  Token,
  Warning,
  ParseError,
  AstNode: require('./lib/ast').AstNode,

  // Factory functions
  nullNode,
  stringNode,
  listNode,
  mapNode,
};