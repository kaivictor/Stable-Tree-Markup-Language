#pragma once

#include "parser/line.h"
#include "lexer/token.h"
#include <vector>

namespace stml {

// =========================================================================
// LineTreeBuilder — converts a flat token stream into a tree of Line nodes.
// =========================================================================
class LineTreeBuilder {
public:
    LineTreeBuilder() = default;

    void process(const std::vector<Token>& tokens);
    const std::vector<std::vector<Line>>& documents() const { return docs_; }
    void reset();

private:
    std::vector<std::vector<Line>> docs_;
    std::vector<Line> current_doc_lines_;
    std::vector<Line*> line_stack_;
    int current_indent_ = 0;

    // Token stream iteration helpers
    using Iter = std::vector<Token>::const_iterator;

    void handle_entry(Iter& it, Iter end);
    AstNode build_value(Iter& it, Iter end);

    void add_line(Line line);
    void commit_document();
};

} // namespace stml
