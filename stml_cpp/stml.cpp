/// STML — public API implementations.
/// Non-inline functions for the main entry points.

#include "stml.h"

#include <fstream>
#include <stdexcept>

namespace stml {

// =========================================================================
// loads — parse STML text
// =========================================================================

LoadResult loads(const std::string& text)
{
    STMLLexer lexer(text);
    auto [tokens, lex_warnings] = lexer.tokenize();

    LineTreeBuilder tree_builder;
    tree_builder.process(tokens);

    AstBuilder ast_builder;
    AstList docs_list;
    for (const auto& doc_lines : tree_builder.documents()) {
        docs_list.push_back(ast_builder.build(doc_lines));
    }

    // Merge warnings from all phases
    std::vector<Warning> all_warnings;
    all_warnings.reserve(lex_warnings.size()
                         + tree_builder.warnings().size()
                         + ast_builder.warnings().size());
    all_warnings.insert(all_warnings.end(),
                         std::make_move_iterator(lex_warnings.begin()),
                         std::make_move_iterator(lex_warnings.end()));
    all_warnings.insert(all_warnings.end(),
                         std::make_move_iterator(tree_builder.warnings().begin()),
                         std::make_move_iterator(tree_builder.warnings().end()));
    all_warnings.insert(all_warnings.end(),
                         std::make_move_iterator(ast_builder.warnings().begin()),
                         std::make_move_iterator(ast_builder.warnings().end()));

    // Wrap in {"docs": [{...}, ...]} per the spec
    AstMap wrapper;
    wrapper.emplace_back("docs", AstNode(std::move(docs_list)));
    return {AstNode(std::move(wrapper)), std::move(all_warnings)};
}

// =========================================================================
// load — parse an STML file
// =========================================================================

LoadResult load(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    std::string text((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
    return loads(text);
}

// =========================================================================
// tokenize — lex only (debug utility)
// =========================================================================

std::pair<std::vector<Token>, std::vector<Warning>>
tokenize(const std::string& text)
{
    STMLLexer lexer(text);
    return lexer.tokenize();
}

// =========================================================================
// parse — parse only (debug utility)
// =========================================================================

std::pair<AstList, std::vector<Warning>>
parse(const std::vector<Token>& tokens)
{
    LineTreeBuilder tree_builder;
    tree_builder.process(tokens);

    AstBuilder ast_builder;
    AstList docs_list;
    for (const auto& doc_lines : tree_builder.documents()) {
        docs_list.push_back(ast_builder.build(doc_lines));
    }

    std::vector<Warning> all_warnings;
    all_warnings.reserve(tree_builder.warnings().size()
                         + ast_builder.warnings().size());
    all_warnings.insert(all_warnings.end(),
                         std::make_move_iterator(tree_builder.warnings().begin()),
                         std::make_move_iterator(tree_builder.warnings().end()));
    all_warnings.insert(all_warnings.end(),
                         std::make_move_iterator(ast_builder.warnings().begin()),
                         std::make_move_iterator(ast_builder.warnings().end()));

    return {std::move(docs_list), std::move(all_warnings)};
}

} // namespace stml
