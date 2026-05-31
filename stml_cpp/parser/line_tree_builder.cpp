#include "line_tree_builder.h"

namespace stml {

// ============================================================
// Reset
// ============================================================
void LineTreeBuilder::reset() {
    docs_.clear();
    blocks_.clear();
    blocks_.push_back({});
    indent_stack_ = {0};
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
// Batch processing
// ============================================================
void LineTreeBuilder::process(const std::vector<Token>& tokens) {
    for (const auto& t : tokens) {
        process_token(t);
    }
}

// ============================================================
// Single token processing (shared by batch + streaming)
// O(1) per token
// ============================================================
void LineTreeBuilder::process_token(const Token& token) {
    switch (token.type) {
    case TokenType::INDENT: {
        finalize_line();
        int delta = std::stoi(std::get<std::string>(token.value));
        int absolute_indent = indent_stack_.back() + delta;
        indent_stack_.push_back(absolute_indent);
        blocks_.push_back({});
        break;
    }
    case TokenType::DEDENT:
        finalize_line();
        close_indent_level();
        if (indent_stack_.size() > 1) {
            indent_stack_.pop_back();
        }
        break;

    case TokenType::DASH:
        finalize_line();
        start_new_line(Line::Kind::DASH_EMPTY);
        pending_has_dash_ = true;
        pending_has_colon_ = false;
        pending_has_key_ = false;
        pending_.indent = indent_stack_.back();
        pending_.line_no = token.line;
        break;

    case TokenType::KEY:
        if (pending_has_dash_ && pending_.kind == Line::Kind::DASH_EMPTY) {
            pending_.kind = Line::Kind::DASH_KEY_VAL;
            pending_.key = std::get<std::string>(token.value);
            pending_has_key_ = true;
            pending_has_colon_ = false;
        } else {
            finalize_line();
            start_new_line(Line::Kind::KEY_VAL);
            pending_.key = std::get<std::string>(token.value);
            pending_has_key_ = true;
            pending_has_colon_ = false;
            pending_has_dash_ = false;
            pending_.indent = indent_stack_.back();
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
            pending_.kind = Line::Kind::DASH_SCALAR;
            pending_.inline_value = token_to_ast_value(token);
        } else if (pending_has_colon_) {
            pending_.inline_value = token_to_ast_value(token);
        }
        break;

    case TokenType::BARE_KEY:
        if (pending_has_dash_ && pending_.kind == Line::Kind::DASH_EMPTY) {
            pending_.kind = Line::Kind::DASH_SCALAR;
            pending_.inline_value = AstNode(std::get<std::string>(token.value));
        } else {
            finalize_line();
            start_new_line(Line::Kind::BARE_KEY);
            pending_.key = std::get<std::string>(token.value);
            pending_has_key_ = true;
            pending_has_dash_ = false;
            pending_has_colon_ = false;
            pending_.indent = indent_stack_.back();
            pending_.line_no = token.line;
        }
        break;

    case TokenType::NEWLINE:
        finalize_line();
        break;

    case TokenType::DOC_SEPARATOR:
        finalize_line();
        while (blocks_.size() > 1) {
            close_indent_level();
        }
        if (!blocks_.empty()) {
            // 总是推入当前文档（可能是空文档 → null）
            docs_.push_back(std::move(blocks_[0]));
            has_pushed_doc_ = true;
            blocks_[0].clear();
            has_content_ = false;
            indent_stack_ = {0};
        }
        break;

    case TokenType::END:
        finalize_line();
        while (blocks_.size() > 1) {
            close_indent_level();
        }
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
// Finalize current pending line
// ============================================================
void LineTreeBuilder::finalize_line() {
    if (!has_pending_) return;

    if (pending_.indent == 0 && blocks_.size() > 1) {
        pending_.indent = indent_stack_.back();
    }

    blocks_.back().push_back(std::move(pending_));

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
// Start a new line
// ============================================================
void LineTreeBuilder::start_new_line(Line::Kind kind) {
    pending_ = Line{};
    pending_.kind = kind;
    has_pending_ = true;
}

// ============================================================
// Close indent level: pop current block, attach to parent
//
// 核心洞察：DEDENT 的语义取决于弹出块和父块的内容类型兼容性。
// 四个规则按优先级判断，确保不规则缩进也能正确解析。
// ============================================================
void LineTreeBuilder::close_indent_level() {
    if (blocks_.size() <= 1) return;

    auto child = std::move(blocks_.back());
    blocks_.pop_back();

    if (child.empty()) return;

    auto& parent = blocks_.back();
    if (parent.empty()) {
        parent = std::move(child);
        return;
    }

    // 检查父块是否包含 dash 行
    bool parent_has_dash = false;
    for (const auto& line : parent) {
        if (line.is_dash()) { parent_has_dash = true; break; }
    }

    Line& parent_last = parent.back();

    // 检查子块属性
    bool child_all_dash = true;
    bool child_has_key = false;
    for (const auto& line : child) {
        if (!line.is_dash()) child_all_dash = false;
        if (line.kind == Line::Kind::KEY_VAL || line.kind == Line::Kind::BARE_KEY)
            child_has_key = true;
    }

    // 判断父块最后一行是否为"开放键"（可吸收更多同类子行）
    // 开放键 = 无行内值 + children 不混合 dash 和 key
    auto is_open = [](const Line& l) -> bool {
        if (l.kind != Line::Kind::KEY_VAL
            && l.kind != Line::Kind::DASH_KEY_VAL
            && l.kind != Line::Kind::BARE_KEY)
            return false;
        if (l.has_inline_value()) return false;
        bool has_key = false, has_dash = false;
        for (const auto& c : l.children) {
            if (c.is_dash()) has_dash = true;
            else has_key = true;
        }
        return !(has_key && has_dash);  // 不能同时有 key 和 dash
    };

    // 规则 1：父块最后行为开放键，且子块类型兼容 → 合并到该键的值块
    //         类型兼容：已有 children 全 dash → 只吸收全 dash 子块
    //                   已有 children 全 key  → 只吸收含 key 子块
    //                   无 children          → 吸收任意类型
    if (is_open(parent_last)) {
        bool parent_children_all_dash = true;
        for (const auto& c : parent_last.children) {
            if (!c.is_dash()) { parent_children_all_dash = false; break; }
        }
        bool compatible = parent_last.children.empty()
            || (parent_children_all_dash && child_all_dash)
            || (!parent_children_all_dash && child_has_key);
        if (compatible) {
            for (auto& line : child) {
                parent_last.children.push_back(std::move(line));
            }
            return;
        }
        // 类型不兼容 → 继续检查后续规则
    }

    // 规则 2：父块为 dash 上下文，child 包含 key →
    //         child 吸收为父块最后 dash 条目的兄弟键
    if (parent_has_dash && child_has_key) {
        parent_last.children = std::move(child);
        return;
    }

    // 规则 3：child 全是 dash → 合并到父块作为兄弟
    //        （不规则缩进导致子块 dash 提升到父块）
    if (child_all_dash) {
        for (auto& line : child) {
            parent.push_back(std::move(line));
        }
        return;
    }

    // 规则 4：非 dash 父块 + 非全 dash 子块 → 合并为兄弟（不规则缩进）
    if (!child_all_dash && !parent_has_dash) {
        for (auto& line : child) {
            parent.push_back(std::move(line));
        }
        return;
    }

    // 默认：真正的嵌套
    parent_last.children = std::move(child);
}

// ============================================================
// Token value → AST value
// ============================================================
AstNode LineTreeBuilder::token_to_ast_value(const Token& t) {
    switch (t.type) {
    case TokenType::NULL_:
        return AstNode(nullptr);
    case TokenType::SCALAR:
    case TokenType::MULTILINE_STRING:
    case TokenType::RAW_STRING:
        return AstNode(std::get<std::string>(t.value));
    case TokenType::INLINE_LIST: {
        AstList list;
        for (const auto& elem : std::get<std::vector<InlineElem>>(t.value)) {
            if (elem.has_value()) {
                list.push_back(AstNode(*elem));
            } else {
                list.push_back(AstNode(nullptr));
            }
        }
        return AstNode(std::move(list));
    }
    default:
        return AstNode(nullptr);
    }
}

} // namespace stml
