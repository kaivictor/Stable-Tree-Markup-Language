#include "parser/line_tree_builder.h"

namespace stml {

// =========================================================================
// Public API
// =========================================================================

void LineTreeBuilder::process(const std::vector<Token>& tokens) {
    reset();

    auto it = tokens.begin();
    auto end = tokens.end();

    while (it != end) {
        switch (it->type) {
            case TokenType::NEWLINE:
                ++it;
                break;

            case TokenType::INDENT:
                current_indent_ = it->indent;
                ++it;
                break;

            case TokenType::DEDENT:
                current_indent_ = it->indent;
                while (!line_stack_.empty()
                       && line_stack_.back()->indent >= current_indent_) {
                    line_stack_.pop_back();
                }
                ++it;
                break;

            case TokenType::DOC_SEPARATOR:
                commit_document();
                ++it;
                break;

            case TokenType::END:
                commit_document();
                ++it;
                break;

            case TokenType::DASH:
            case TokenType::KEY:
            case TokenType::BARE_KEY:
                handle_entry(it, end);
                break;

            default:
                // Unexpected token at top level — skip
                ++it;
                break;
        }
    }

    commit_document();
}

void LineTreeBuilder::reset() {
    docs_.clear();
    current_doc_lines_.clear();
    line_stack_.clear();
    current_indent_ = 0;
}

void LineTreeBuilder::commit_document() {
    if (!current_doc_lines_.empty()) {
        docs_.push_back(std::move(current_doc_lines_));
        current_doc_lines_.clear();
    }
    line_stack_.clear();
    current_indent_ = 0;
}

void LineTreeBuilder::add_line(Line line) {
    while (!line_stack_.empty() && line_stack_.back()->indent >= line.indent) {
        line_stack_.pop_back();
    }

    if (line_stack_.empty()) {
        current_doc_lines_.push_back(std::move(line));
        line_stack_.push_back(&current_doc_lines_.back());
    } else {
        line_stack_.back()->children.push_back(std::move(line));
        line_stack_.push_back(&line_stack_.back()->children.back());
    }
}

// =========================================================================
// Build inline value from the next tokens
// =========================================================================

AstNode LineTreeBuilder::build_value(Iter& it, Iter end) {
    if (it == end) return AstNode();

    switch (it->type) {
        case TokenType::SCALAR: {
            std::string val;
            if (auto* s = std::get_if<std::string>(&it->value)) val = *s;
            ++it;
            return AstNode(AstScalar(std::move(val)));
        }
        case TokenType::NULL_: {
            ++it;
            return AstNode();
        }
        case TokenType::INLINE_LIST: {
            AstList list;
            if (auto* elems = std::get_if<std::vector<InlineElem>>(&it->value)) {
                for (const auto& e : *elems) {
                    if (e.is_null()) list.push_back(AstNode());
                    else list.push_back(AstNode(AstScalar(e.value)));
                }
            }
            ++it;
            return AstNode(std::move(list));
        }
        case TokenType::MULTILINE_STRING:
        case TokenType::RAW_STRING:
        case TokenType::BARE_KEY: {
            std::string val;
            if (auto* s = std::get_if<std::string>(&it->value)) val = *s;
            ++it;
            return AstNode(AstScalar(std::move(val)));
        }
        default:
            return AstNode();
    }
}

// =========================================================================
// Handle a line entry: DASH, KEY, or BARE_KEY token
// =========================================================================

void LineTreeBuilder::handle_entry(Iter& it, Iter end) {
    TokenType entry_type = it->type;
    int line_no = it->line;

    if (entry_type == TokenType::DASH) {
        ++it; // consume DASH

        if (it == end) {
            add_line(Line(Line::Kind::DASH_EMPTY, "", AstNode(),
                          current_indent_, line_no));
            return;
        }

        switch (it->type) {
            case TokenType::NEWLINE:
            case TokenType::END:
                add_line(Line(Line::Kind::DASH_EMPTY, "", AstNode(),
                              current_indent_, line_no));
                return;

            case TokenType::NULL_:
                ++it;
                add_line(Line(Line::Kind::DASH_EMPTY, "", AstNode(),
                              current_indent_, line_no));
                return;

            case TokenType::KEY: {
                std::string key;
                if (auto* s = std::get_if<std::string>(&it->value)) key = *s;
                ++it; // consume KEY
                if (it != end && it->type == TokenType::COLON) ++it; // consume COLON
                AstNode val = build_value(it, end);
                add_line(Line(Line::Kind::DASH_KEY_VAL, std::move(key),
                              std::move(val), current_indent_, line_no));
                return;
            }

            case TokenType::SCALAR: {
                std::string val;
                if (auto* s = std::get_if<std::string>(&it->value)) val = *s;
                ++it;
                add_line(Line(Line::Kind::DASH_SCALAR, "",
                              AstNode(AstScalar(std::move(val))),
                              current_indent_, line_no));
                return;
            }

            case TokenType::INLINE_LIST: {
                AstList list;
                if (auto* elems = std::get_if<std::vector<InlineElem>>(&it->value)) {
                    for (const auto& e : *elems) {
                        if (e.is_null()) list.push_back(AstNode());
                        else list.push_back(AstNode(AstScalar(e.value)));
                    }
                }
                ++it;
                add_line(Line(Line::Kind::DASH_SCALAR, "",
                              AstNode(std::move(list)),
                              current_indent_, line_no));
                return;
            }

            case TokenType::MULTILINE_STRING:
            case TokenType::RAW_STRING: {
                std::string val;
                if (auto* s = std::get_if<std::string>(&it->value)) val = *s;
                ++it;
                add_line(Line(Line::Kind::DASH_SCALAR, "",
                              AstNode(AstScalar(std::move(val))),
                              current_indent_, line_no));
                return;
            }

            case TokenType::BARE_KEY: {
                std::string val;
                if (auto* s = std::get_if<std::string>(&it->value)) val = *s;
                ++it;
                if (it != end && it->type == TokenType::COLON) {
                    ++it;
                    AstNode colon_val = build_value(it, end);
                    add_line(Line(Line::Kind::DASH_KEY_VAL, std::move(val),
                                  std::move(colon_val), current_indent_, line_no));
                } else {
                    add_line(Line(Line::Kind::DASH_SCALAR, "",
                                  AstNode(AstScalar(std::move(val))),
                                  current_indent_, line_no));
                }
                return;
            }

            default:
                add_line(Line(Line::Kind::DASH_EMPTY, "", AstNode(),
                              current_indent_, line_no));
                return;
        }
    }

    else if (entry_type == TokenType::KEY) {
        std::string key;
        if (auto* s = std::get_if<std::string>(&it->value)) key = *s;
        ++it; // consume KEY

        if (it != end && it->type == TokenType::COLON) {
            ++it; // consume COLON
        }

        AstNode val = build_value(it, end);
        add_line(Line(Line::Kind::KEY_VAL, std::move(key), std::move(val),
                      current_indent_, line_no));
    }

    else if (entry_type == TokenType::BARE_KEY) {
        std::string key;
        if (auto* s = std::get_if<std::string>(&it->value)) key = *s;
        ++it; // consume BARE_KEY

        if (it != end && it->type == TokenType::COLON) {
            ++it; // consume COLON
            AstNode val = build_value(it, end);
            add_line(Line(Line::Kind::KEY_VAL, std::move(key), std::move(val),
                          current_indent_, line_no));
        } else {
            add_line(Line(Line::Kind::BARE_KEY, std::move(key), AstNode(),
                          current_indent_, line_no));
        }
    }
}

} // namespace stml
