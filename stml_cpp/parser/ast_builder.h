#pragma once

#include "ast/ast.h"
#include "parser/line.h"
#include "diagnostics/error.h"
#include <vector>

namespace stml {

// =========================================================================
// AstBuilder — Phase 2: Line 树 → AST
//
// 将 LineTreeBuilder 产生的 Line 树转换为标准 AST 节点。
// 处理语义分发（mapping vs sequence）和兄弟键收集。
// =========================================================================
class AstBuilder {
public:
    AstBuilder() = default;

    /// Build an AST from a single document's line tree.
    AstNode build(const std::vector<Line>& lines);

    /// Build ASTs from multiple documents' line trees.
    std::vector<AstNode> build_all(const std::vector<std::vector<Line>>& doc_lines);

    /// Accumulated warnings from the build phase.
    std::vector<Warning>& warnings() { return warnings_; }

private:
    AstMap build_mapping(const std::vector<Line>& lines);
    AstNode build_sequence(const std::vector<Line>& lines);
    AstList build_simple_sequence(const std::vector<Line>& lines);
    AstList build_complex_sequence(const std::vector<Line>& lines);
    void absorb_children_as_siblings(AstMap& map, const std::vector<Line>& children);

    std::vector<Warning> warnings_;
};

} // namespace stml
