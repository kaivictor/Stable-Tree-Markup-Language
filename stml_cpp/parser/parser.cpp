<<<<<<< Updated upstream
#include "parser/parser.h"
#include "parser/line_tree_builder.h"
#include "parser/ast_builder.h"

namespace stml {

AstNode Parser::parse(const std::vector<Token>& tokens) {
    warnings_.clear();

    // Phase 1: Tokens → Line tree
    LineTreeBuilder tree_builder;
    tree_builder.process(tokens);
    const auto& doc_line_trees = tree_builder.documents();

    // Phase 2: Line tree → AST
    AstBuilder ast_builder;
    std::vector<AstNode> doc_asts = ast_builder.build_all(doc_line_trees);
<<<<<<< Updated upstream

    // Wrap in {"docs": [...]}
    AstList docs_list;
    docs_list.reserve(doc_asts.size());
    for (auto& doc : doc_asts) {
        docs_list.push_back(std::move(doc));
    }

    AstMap top;
    top.emplace_back("docs", AstNode(std::move(docs_list)));
    return AstNode(std::move(top));
=======

    // Wrap in {"docs": [...]}
    AstList docs_list;
    docs_list.reserve(doc_asts.size());
    for (auto& doc : doc_asts) {
        docs_list.push_back(std::move(doc));
    }

    AstMap top;
    top.emplace_back("docs", AstNode(std::move(docs_list)));
    return AstNode(std::move(top));
=======
#include "parser.h"
#include "../diagnostics/error.h"
#include <stdexcept>

namespace stml {

// ============================================================
// 重置
// ============================================================
void Parser::reset() {
    line_tree_.reset();
    warnings_.clear();
    g_build_warnings.clear();
    finished_ = false;
    end_seen_ = false;
}

// ============================================================
// 批量接口
// ============================================================
AstList Parser::parse(const std::vector<Token>& tokens) {
    reset();

    // 喂入所有 token（跳过 END，由 finish() 处理）
    for (const auto& t : tokens) {
        if (t.type == TokenType::END) break;
        feed_token(t);
    }

    return finish();
}

// ============================================================
// 流式：喂入 Token
// ============================================================
void Parser::feed_token(const Token& token) {
    if (finished_) {
        throw ParseError(token.line, token.col,
            "Parser already finished, cannot feed more tokens");
    }
    if (token.type == TokenType::END) {
        end_seen_ = true;
    }
    line_tree_.process_token(token);
}

// ============================================================
// 流式：完成解析
// ============================================================
AstList Parser::finish() {
    if (finished_) {
        throw ParseError(0, 0, "Parser already finished");
    }
    finished_ = true;

    // 确保所有待定行已 flush
    // 流式：Lexer 已通过回调发送 END；批量：parse() 在 END 处 break
    if (!end_seen_) {
        line_tree_.process_token(Token::make(TokenType::END, 0, 1));
    }

    // 收集 LineTreeBuilder 的警告
    for (const auto& w : line_tree_.warnings()) {
        warnings_.push_back(w);
    }

    // Phase 2: 每个文档的行树 → AST
    g_build_warnings.clear();
    AstList docs;

    for (const auto& doc_lines : line_tree_.documents()) {
        AstNode doc = build_ast(doc_lines);
        docs.items.push_back(std::move(doc));
    }

    // 收集 Phase 2 的警告
    for (const auto& w : g_build_warnings) {
        warnings_.push_back(w);
    }

    return docs;
>>>>>>> Stashed changes
>>>>>>> Stashed changes
}

} // namespace stml
