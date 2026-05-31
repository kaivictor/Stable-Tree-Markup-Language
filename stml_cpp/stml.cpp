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

    auto [docs_list, parse_warnings] = STMLParser(std::move(tokens)).parse();

    std::vector<Warning> all_warnings;
    all_warnings.reserve(lex_warnings.size() + parse_warnings.size());
    all_warnings.insert(all_warnings.end(),
                         std::make_move_iterator(lex_warnings.begin()),
                         std::make_move_iterator(lex_warnings.end()));
    all_warnings.insert(all_warnings.end(),
                         std::make_move_iterator(parse_warnings.begin()),
                         std::make_move_iterator(parse_warnings.end()));

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
    STMLParser parser(tokens);
    return parser.parse();
}

} // namespace stml
