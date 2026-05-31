#ifndef STML_PARSER_PARSER_H
#define STML_PARSER_PARSER_H

<<<<<<< Updated upstream
#include "ast/ast.h"
#include "lexer/token.h"
#include "diagnostics/error.h"
=======
<<<<<<< Updated upstream
#include "ast/ast.h"
#include "lexer/token.h"
#include "diagnostics/error.h"
=======
#include "line_tree_builder.h"
#include "ast_builder.h"
#include "../ast/ast.h"
#include "../diagnostics/error.h"
>>>>>>> Stashed changes
>>>>>>> Stashed changes
#include <vector>

namespace stml {

<<<<<<< Updated upstream
// =========================================================================
// Parser — converts a token stream into an AST.
//
// Internally delegates to LineTreeBuilder (tokens → line tree) and
// AstBuilder (line tree → AST).
// =========================================================================
class Parser {
public:
    Parser() = default;

    /// Parse a complete token stream into an AST node.
    /// The result is an AstMap {"docs": [doc1, doc2, ...]}.
    AstNode parse(const std::vector<Token>& tokens);

    /// Get warnings accumulated during parsing.
    const std::vector<Warning>& warnings() const { return warnings_; }

private:
    std::vector<Warning> warnings_;
<<<<<<< Updated upstream
=======
=======
// ============================================================
// Parser — 统一批量+流式解析器
//
// 批量用法:
//   Lexer lexer;
//   auto tokens = lexer.tokenize(input);
//   Parser parser;
//   AstList docs = parser.parse(tokens);
//
// 流式用法:
//   Parser parser;
//   parser.feed_token(token1);
//   parser.feed_token(token2);
//   ...
//   AstList docs = parser.finish();
//
// 批量使用同一套逻辑，O(n)
// ============================================================
class Parser {
public:
    Parser() { reset(); }

    // ── 批量接口 ──
    AstList parse(const std::vector<Token>& tokens);

    // ── 流式接口（同一套逻辑） ──
    void feed_token(const Token& token);
    AstList finish();

    // ── 警告 ──
    const std::vector<Warning>& warnings() const { return warnings_; }

    // ── 重置 ──
    void reset();

private:
    LineTreeBuilder line_tree_;
    std::vector<Warning> warnings_;
    bool finished_ = false;
    bool end_seen_ = false;
>>>>>>> Stashed changes
>>>>>>> Stashed changes
};

} // namespace stml

#endif // STML_PARSER_PARSER_H
