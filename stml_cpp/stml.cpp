#include "stml.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
<<<<<<< Updated upstream
=======
<<<<<<< Updated upstream
>>>>>>> Stashed changes
#include <fstream>
#include <sstream>
#include <utility>

namespace stml {

// =========================================================================
// High-level API
// =========================================================================

LoadResult loads(const std::string& text) {
    Lexer lexer;
    std::vector<Token> tokens = lexer.tokenize(text);
    std::vector<Warning> warnings = lexer.warnings();

    Parser parser;
    AstNode ast = parser.parse(tokens);

    // Merge lexer warnings with parser warnings
    const auto& p_warnings = parser.warnings();
    warnings.insert(warnings.end(), p_warnings.begin(), p_warnings.end());

    return LoadResult(std::move(ast), std::move(warnings));
}

LoadResult load(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        throw ParseError(0, 0, "Cannot open file: " + filepath);
    }
<<<<<<< Updated upstream
    std::ostringstream ss;
    ss << file.rdbuf();
    return loads(ss.str());
=======
=======
#include "serializer/serializer.h"
#include <fstream>
#include <sstream>

namespace stml {

AstList loads(const std::string& input) {
    Lexer lexer;
    auto tokens = lexer.tokenize(input);

    Parser parser;
    return parser.parse(tokens);
>>>>>>> Stashed changes
}

AstList load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw ParseError(0, 0, "Cannot open file: " + filepath);
    }

>>>>>>> Stashed changes
    std::ostringstream ss;
    ss << file.rdbuf();
    return loads(ss.str());
}

<<<<<<< Updated upstream
// =========================================================================
// Low-level / debugging API
// =========================================================================

std::pair<std::vector<Token>, std::vector<Warning>>
tokenize(const std::string& text) {
    Lexer lexer;
    std::vector<Token> tokens = lexer.tokenize(text);
    std::vector<Warning> warnings = lexer.warnings();
    return {std::move(tokens), std::move(warnings)};
}

std::pair<AstNode, std::vector<Warning>>
parse(const std::vector<Token>& tokens) {
    Parser parser;
    AstNode ast = parser.parse(tokens);
    std::vector<Warning> warnings = parser.warnings();
    return {std::move(ast), std::move(warnings)};
<<<<<<< Updated upstream
=======
=======
std::string dumps(const AstList& docs, int indent) {
    return docs_to_stml(docs, indent);
}

std::string to_json(const AstList& docs, int indent) {
    return docs_to_json(docs, indent);
>>>>>>> Stashed changes
>>>>>>> Stashed changes
}

} // namespace stml
