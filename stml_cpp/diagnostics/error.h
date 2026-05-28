#pragma once

#include <stdexcept>
#include <string>
#include <vector>

namespace stml {

// =========================================================================
// Warning — non-fatal diagnostic during parsing.
// The AST is still well-formed; warnings describe potential user mistakes.
// =========================================================================
struct Warning {
    int line;       // 1-based
    int column;     // 1-based
    std::string message;

    Warning(int line, int column, std::string message)
        : line(line), column(column), message(std::move(message)) {}
};

// =========================================================================
// ParseError — fatal error during parsing. Cannot recover an AST.
// =========================================================================
class ParseError : public std::runtime_error {
public:
    int line;
    int column;
    std::string message;

    ParseError(int line, int column, std::string message)
        : std::runtime_error(std::to_string(line) + ":" + std::to_string(column) + ": " + message)
        , line(line)
        , column(column)
        , message(std::move(message)) {}
};

} // namespace stml
