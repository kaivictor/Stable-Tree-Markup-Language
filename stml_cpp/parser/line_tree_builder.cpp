#include "line_tree_builder.h"

namespace stml {

// ============================================================
// 重置
// ============================================================
void LineTreeBuilder::reset() {
    docs_.clear();
    root_lines_.clear();
    blocks_.clear();
    blocks_.push_back({}); // 哨兵：当前文档的根级行
    pending_ = Line{};
    has_pending_ = false;
    pending_has_dash_ = false;
    pending_has_colon_ = false;
    pending_has_key_ = false;
    has_pushed_doc_ = false;
    has_content_ = false;
    warnings_.clear();
}

// ============================================================
// 批量处理
// ============================================================
void LineTreeBuilder::process(const std::vector<Token>& tokens) {
    for (const auto& t : tokens) {
        process_token(t);
    }
}

// ============================================================
// 单 Token 处理（流式 + 批量共用）
// O(1) per token
// ============================================================
void LineTreeBuilder::process_token(const Token& token) {
    switch (token.type) {
    case TokenType::INDENT:
        // 下一级缩进 → 新块（后续行将添加到这个块）
        // 如果有待定行，先完成它，子行将附加到此行
        finalize_line();
        blocks_.push_back({});
        break;

    case TokenType::DEDENT:
        // 缩进级别结束 → 当前块闭合
        // 先完成待定行
        finalize_line();
        // 弹出当前块，将其作为父块最后一行的 children
        close_indent_level();
        break;

    case TokenType::DASH:
        finalize_line();
        start_new_line(Line::Kind::DASH_EMPTY);
        pending_has_dash_ = true;
        pending_has_colon_ = false;
        pending_has_key_ = false;
        pending_.indent = token.indent > 0 ? token.indent : (blocks_.size() - 1) * 2;
        pending_.line_no = token.line;
        break;

    case TokenType::KEY:
        if (pending_has_dash_ && pending_.kind == Line::Kind::DASH_EMPTY) {
            // "- key:" → DASH_KEY_VAL
            pending_.kind = Line::Kind::DASH_KEY_VAL;
            pending_.key = token.value;
            pending_has_key_ = true;
            pending_has_colon_ = false;
        } else {
            finalize_line();
            start_new_line(Line::Kind::KEY_VAL);
            pending_.key = token.value;
            pending_has_key_ = true;
            pending_has_colon_ = false;
            pending_has_dash_ = false;
            pending_.indent = token.indent > 0 ? token.indent : (blocks_.size() - 1) * 2;
            pending_.line_no = token.line;
        }
        break;

    case TokenType::COLON:
        pending_has_colon_ = true;
        break;

    case TokenType::SCALAR:
    case TokenType::NULL_:
    case TokenType::INLINE_LIST:
    case TokenType::MULTILINE_STRING:
    case TokenType::RAW_STRING:
        if (pending_has_dash_ && !pending_has_colon_ && !pending_has_key_) {
            // "- value" → DASH_SCALAR
            pending_.kind = Line::Kind::DASH_SCALAR;
            pending_.inline_value = token_to_ast_value(token);
        } else if (pending_has_colon_) {
            // "key: value" or "- key: value" → inline_value
            pending_.inline_value = token_to_ast_value(token);
        }
        // else: value without context, ignore or warn
        break;

    case TokenType::BARE_KEY:
        if (pending_has_dash_ && pending_.kind == Line::Kind::DASH_EMPTY) {
            // "- bare_key" → DASH_SCALAR (将 bare_key 作为值)
            pending_.kind = Line::Kind::DASH_SCALAR;
            pending_.inline_value = AstNode(AstScalar{token.value});
        } else {
            finalize_line();
            start_new_line(Line::Kind::BARE_KEY);
            pending_.key = token.value;
            pending_has_key_ = true;
            pending_has_dash_ = false;
            pending_has_colon_ = false;
            pending_.indent = token.indent > 0 ? token.indent : (blocks_.size() - 1) * 2;
            pending_.line_no = token.line;
        }
        break;

    case TokenType::NEWLINE:
        finalize_line();
        break;

    case TokenType::DOC_SEPARATOR:
        // 完成一切待定，保存当前文档，开始新文档
        finalize_line();
        // 关闭所有 indent 级别回到根
        while (blocks_.size() > 1) {
            close_indent_level();
        }
        // 保存当前文档
        if (!blocks_.empty()) {
            if (has_content_ || has_pushed_doc_) {
                // 当前文档有内容，或已经保存过文档 → 保存
                docs_.push_back(std::move(blocks_[0]));
                has_pushed_doc_ = true;
            } else {
                // 开头的 --- 且之前无内容无文档 → 保存空文档（null）
                docs_.push_back(std::move(blocks_[0]));
                has_pushed_doc_ = true;
            }
            blocks_[0].clear();
            has_content_ = false;
        }
        break;

    case TokenType::END:
        // 输入结束
        finalize_line();
        while (blocks_.size() > 1) {
            close_indent_level();
        }
        // 保存最后文档（仅当有内容或保存过文档）
        if (!blocks_.empty()) {
            if (has_content_ || has_pushed_doc_) {
                docs_.push_back(std::move(blocks_[0]));
            }
        }
        break;

    default:
        break;
    }
}

// ============================================================
// 完成当前待定行
// ============================================================
void LineTreeBuilder::finalize_line() {
    if (!has_pending_) return;

    // 修正 DASH_EMPTY 的 indent
    // indent 基于 blocks_ 栈深度
    if (pending_.indent == 0 && blocks_.size() > 1) {
        pending_.indent = (int)(blocks_.size() - 1) * 2;
    }

    blocks_.back().push_back(std::move(pending_));

    // 根级块中（blocks_ 只有第0层），添加的是文档内容
    if (blocks_.size() <= 1) {
        has_content_ = true;
    }

    pending_ = Line{};
    has_pending_ = false;
    pending_has_dash_ = false;
    pending_has_colon_ = false;
    pending_has_key_ = false;
}

// ============================================================
// 开始新行
// ============================================================
void LineTreeBuilder::start_new_line(Line::Kind kind) {
    pending_ = Line{};
    pending_.kind = kind;
    has_pending_ = true;
}

// ============================================================
// 关闭缩进级别：弹出当前块，附加到父块最后一行
// ============================================================
void LineTreeBuilder::close_indent_level() {
    if (blocks_.size() <= 1) return;

    auto children = std::move(blocks_.back());
    blocks_.pop_back();

    if (!children.empty() && !blocks_.back().empty()) {
        blocks_.back().back().children = std::move(children);
    } else if (!children.empty()) {
        // 父块为空（不应该发生），将 children 作为父块内容
        blocks_.back() = std::move(children);
    }
}

// ============================================================
// Token 值 → AST 值
// ============================================================
AstNode LineTreeBuilder::token_to_ast_value(const Token& t) {
    switch (t.type) {
    case TokenType::NULL_:
        return AstNode(NullNode{});
    case TokenType::SCALAR:
    case TokenType::MULTILINE_STRING:
    case TokenType::RAW_STRING:
        return AstNode(AstScalar{t.value});
    case TokenType::INLINE_LIST: {
        AstList list;
        for (const auto& elem : t.inline_list_items) {
            switch (elem.kind) {
            case InlineElem::Kind::SCALAR:
                list.items.push_back(AstNode(AstScalar{elem.value}));
                break;
            case InlineElem::Kind::NULL_VAL:
                list.items.push_back(AstNode(NullNode{}));
                break;
            case InlineElem::Kind::INLINE_LIST:
                // 嵌套列表暂作为 scalar 保留原文本
                list.items.push_back(AstNode(AstScalar{elem.value}));
                break;
            }
        }
        return AstNode(std::move(list));
    }
    default:
        return AstNode(NullNode{});
    }
}

} // namespace stml
