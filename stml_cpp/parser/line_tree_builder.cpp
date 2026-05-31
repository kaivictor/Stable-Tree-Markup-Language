#include "line_tree_builder.h"

namespace stml {

// ============================================================
// Reset
// ============================================================
void LineTreeBuilder::reset() {
    docs_.clear();
    root_lines_.clear();
    blocks_.clear();
    blocks_.push_back({});
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
    int block_indent = (int)(blocks_.size() - 1) * 2;

    switch (token.type) {
    case TokenType::INDENT:
        finalize_line();
        blocks_.push_back({});
        break;

    case TokenType::DEDENT:
        finalize_line();
        close_indent_level();
        break;

    case TokenType::DASH:
        finalize_line();
        start_new_line(Line::Kind::DASH_EMPTY);
        pending_has_dash_ = true;
        pending_has_colon_ = false;
        pending_has_key_ = false;
        pending_.indent = block_indent;
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
            pending_.indent = block_indent;
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
            pending_.indent = block_indent;
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
            if (has_content_ || has_pushed_doc_) {
                docs_.push_back(std::move(blocks_[0]));
                has_pushed_doc_ = true;
            } else {
                docs_.push_back(std::move(blocks_[0]));
                has_pushed_doc_ = true;
            }
            blocks_[0].clear();
            has_content_ = false;
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
        pending_.indent = (int)(blocks_.size() - 1) * 2;
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
// ============================================================
void LineTreeBuilder::close_indent_level() {
    if (blocks_.size() <= 1) return;

    auto children = std::move(blocks_.back());
    blocks_.pop_back();

    if (!children.empty() && !blocks_.back().empty()) {
        blocks_.back().back().children = std::move(children);
    } else if (!children.empty()) {
        blocks_.back() = std::move(children);
    }
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
