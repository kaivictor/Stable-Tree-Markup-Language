#pragma once

/// STML — Stable Tree Markup Language C++ Library
///
/// Public API
/// ----------
///   loads(text)         → (ast, warnings)       parse STML string
///   load(filename)      → (ast, warnings)       parse STML file
///   dumps(ast)          → str                   AST → canonical STML
///   to_json(ast)        → str                   AST → JSON string
///   tokenize(text)      → (tokens, warnings)    lex only (debug)
///   parse(tokens)       → (ast, warnings)       parse only (debug)
///
/// Types
/// -----
///   TokenType, Token, Warning, ParseError
///   AstNode, AstList, AstMap, AstValue
///   STMLLexer, STMLParser

#include "diagnostics/error.h"
#include "lexer/token.h"
#include "lexer/lexer.h"
#include "ast/ast.h"
#include "parser/parser.h"
#include "serializer/serializer.h"

#include <string>
#include <utility>
#include <vector>

namespace stml {

// =========================================================================
// Main entry points
// =========================================================================

/// Load result: a parsed AST plus all warnings.
struct LoadResult {
    AstNode ast;
    std::vector<Warning> warnings;
};

/// Parse STML text → (AST, warnings).
/// AST is always {"docs": [{...}, ...]}, a list of documents.
/// Single-document input → single-element list.
/// Empty input → empty list.
/// @throws ParseError on unsupported structural conflicts.
LoadResult loads(const std::string& text);

/// Read and parse an STML file → (AST, warnings).
/// @throws ParseError on unsupported structural conflicts.
LoadResult load(const std::string& filename);

/// Lex only — returns (tokens, warnings). Useful for debugging.
std::pair<std::vector<Token>, std::vector<Warning>>
tokenize(const std::string& text);

/// Parse only — returns (ast, warnings) from a pre-lexed token list.
/// Returns a list of document AST nodes (unwrapped).
std::pair<AstList, std::vector<Warning>>
parse(const std::vector<Token>& tokens);

} // namespace stml

#include "stml_stream.h"
