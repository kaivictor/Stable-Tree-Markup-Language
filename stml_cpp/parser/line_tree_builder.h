#ifndef STML_PARSER_LINE_TREE_BUILDER_H
#define STML_PARSER_LINE_TREE_BUILDER_H

#include "line.h"
#include "../lexer/token.h"
#include "../diagnostics/error.h"
#include <vector>

namespace stml {

// ============================================================
// LineTreeBuilder — Phase 1: Token 流 → Line 树
//
// 处理缩进嵌套，将 Token 流转换为多级 Line 树。
// 不关心语义（不区分 mapping/sequence），只做缩进分组。
// ============================================================
class LineTreeBuilder {
public:
    LineTreeBuilder() { reset(); }

    // 处理一批 Token，返回按文档分组的行树
    // 每个文档是一组根级 Line
    struct Document {
        std::vector<Line> lines;
    };

    void process(const std::vector<Token>& tokens);
    void process_token(const Token& token);

    // 获取结果：每个文档对应一组根级行
    const std::vector<std::vector<Line>>& documents() const { return docs_; }
    const std::vector<Warning>& warnings() const { return warnings_; }

    // 重置
    void reset();

private:
    // 文档列表（每个文档是一组根级行）
    std::vector<std::vector<Line>> docs_;

    // 当前文档的根级行
    std::vector<Line> root_lines_;

    // 缩进块栈：blocks_.back() 是当前缩进级别的行集合
    std::vector<std::vector<Line>> blocks_;

    // 当前正在构建的行
    Line pending_;
    bool has_pending_ = false;
    bool pending_has_dash_ = false;
    bool pending_has_colon_ = false;
    bool pending_has_key_ = false;

    // 文档追踪
    bool has_pushed_doc_ = false;   // 是否已保存过文档
    bool has_content_ = false;      // 当前文档是否有内容

    // 警告
    std::vector<Warning> warnings_;

    // 辅助
    void finalize_line();
    void start_new_line(Line::Kind kind);
    void close_indent_level();
    AstNode token_to_ast_value(const Token& t);
};

} // namespace stml

#endif // STML_PARSER_LINE_TREE_BUILDER_H
