#pragma once

#include "ast/ast.h"
#include "parser/line.h"
#include <vector>

namespace stml {

// =========================================================================
// AstBuilder — converts a Line tree into an AstNode.
//
// Usage:
//   AstBuilder builder;
//   AstNode doc_ast = builder.build(lines);
//
//   // Build all documents
//   std::vector<AstNode> docs = builder.build_all(doc_line_trees);
// =========================================================================
class AstBuilder {
public:
    AstBuilder() = default;

    /// Build an AST from a single document's line tree.
    AstNode build(const std::vector<Line>& lines);

    /// Build ASTs from multiple documents' line trees.
    std::vector<AstNode> build_all(const std::vector<std::vector<Line>>& doc_lines);

private:
    AstNode build_line(const Line& line);
    AstNode build_children(const std::vector<Line>& children);
};

} // namespace stml
