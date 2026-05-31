#include "ast_builder.h"

namespace stml {

thread_local std::vector<Warning> g_build_warnings;

// ============================================================
// 主入口
// ============================================================
AstNode build_ast(const std::vector<Line>& lines) {
    if (lines.empty()) return AstNode(NullNode{});

    // 检查是否全部是 DASH_* 行
    bool all_dash = true;
    for (const auto& line : lines) {
        if (!line.is_dash()) {
            all_dash = false;
            break;
        }
    }

    if (all_dash) {
        return build_sequence(lines);
    } else {
        return AstNode(build_mapping(lines));
    }
}

// ============================================================
// build_mapping
// ============================================================
AstMap build_mapping(const std::vector<Line>& lines) {
    AstMap map;
    size_t i = 0;

    while (i < lines.size()) {
        const Line& line = lines[i];
        if (!(line.kind == Line::Kind::KEY_VAL || line.kind == Line::Kind::BARE_KEY)) {
            ++i;
            continue;
        }

        std::string key = line.key;
        AstNode val = AstNode(NullNode{});

        if (line.has_inline_value()) {
            // 完整条目：有行内值
            val = clone_ast(line.inline_value);
            // 先添加当前条目，再吸收子内容为兄弟键（保持顺序）
            map.emplace_back(key, std::move(val));
            if (line.has_children()) {
                // 不规则缩进 → 子内容吸收为兄弟键
                absorb_children_as_siblings(map, line.children);
            }
            ++i;
            continue;
        } else if (line.has_children()) {
            // 不完整条目：有冒号无值，子内容形成值
            val = build_ast(line.children);
            ++i;
            // 检查后续同级行是否为 dash 序列（不规则缩进导致分离的序列条目）
            if (val.is_list()) {
                size_t j = i;
                while (j < lines.size() && lines[j].is_dash()) {
                    ++j;
                }
                if (j > i) {
                    std::vector<Line> dash_lines(lines.begin() + i, lines.begin() + j);
                    AstNode extra = build_ast(dash_lines);
                    if (extra.is_list()) {
                        for (auto& item : extra.as_list_mut()->items) {
                            val.as_list_mut()->items.push_back(std::move(item));
                        }
                    } else {
                        val.as_list_mut()->items.push_back(std::move(extra));
                    }
                    i = j;
                }
            }
        } else {
            // 无值无子 → 检查后续同级行是否为 dash 序列（不规则缩进）
            size_t j = i + 1;
            while (j < lines.size() && lines[j].is_dash()) {
                ++j;
            }
            if (j > i + 1) {
                // 后续连续 dash 行构成序列值
                std::vector<Line> dash_lines(lines.begin() + i + 1, lines.begin() + j);
                val = build_ast(dash_lines);
                i = j;
            } else {
                // val = null
                ++i;
            }
        }

        map.emplace_back(key, std::move(val));
    }

    return map;
}

// ============================================================
// build_sequence — 先预扫描再分发
// ============================================================
AstNode build_sequence(const std::vector<Line>& lines) {
    // 预扫描：是否有 DASH_KEY_VAL？
    bool is_complex = false;
    for (const auto& line : lines) {
        if (line.kind == Line::Kind::DASH_KEY_VAL) {
            is_complex = true;
            break;
        }
    }

    if (is_complex) {
        return AstNode(build_complex_sequence(lines));
    } else {
        return AstNode(build_simple_sequence(lines));
    }
}

// ============================================================
// build_simple_sequence
// ============================================================
AstList build_simple_sequence(const std::vector<Line>& lines) {
    AstList seq;

    for (const auto& line : lines) {
        if (!line.is_dash()) continue;

        if (line.kind == Line::Kind::DASH_EMPTY) {
            if (line.has_children()) {
                // 先推入 null 表示空条目自身
                seq.items.push_back(AstNode(NullNode{}));
                // 然后提升子内容到当前序列
                AstNode child = build_ast(line.children);
                if (child.is_list()) {
                    // 子列表 → 展开合并
                    for (auto& item : child.as_list_mut()->items) {
                        seq.items.push_back(std::move(item));
                    }
                } else {
                    // Map 或 Scalar → 作为一个元素
                    seq.items.push_back(std::move(child));
                }
            } else {
                // "-" 单独 → null
                seq.items.push_back(AstNode(NullNode{}));
            }
        } else if (line.kind == Line::Kind::DASH_SCALAR) {
            if (line.has_children()) {
                // 不规则：标量条目有子内容 → 分离 dash 和非 dash 子行
                std::vector<Line> dash_kids, non_dash_kids;
                for (const auto& c : line.children) {
                    if (c.is_dash()) dash_kids.push_back(c);
                    else non_dash_kids.push_back(c);
                }
                if (!non_dash_kids.empty()) {
                    g_build_warnings.emplace_back(line.line_no, 1,
                        "Dash scalar entry has unexpected child content, ignoring children");
                }
                seq.items.push_back(clone_ast(line.inline_value));
                if (!dash_kids.empty()) {
                    AstNode extra = build_sequence(dash_kids);
                    if (extra.is_list()) {
                        for (auto& item : extra.as_list_mut()->items) {
                            seq.items.push_back(std::move(item));
                        }
                    } else {
                        seq.items.push_back(std::move(extra));
                    }
                }
            } else {
                seq.items.push_back(clone_ast(line.inline_value));
            }
        }
    }

    return seq;
}

// ============================================================
// build_complex_sequence
// ============================================================
AstList build_complex_sequence(const std::vector<Line>& lines) {
    AstList seq;
    size_t i = 0;

    while (i < lines.size()) {
        if (!lines[i].is_dash()) {
            ++i;
            continue;
        }

        AstMap entry;
        const Line& dash_line = lines[i];

        // ── 处理 DASH 行本身 ──
        if (dash_line.kind == Line::Kind::DASH_KEY_VAL) {
            AstNode val = dash_line.has_inline_value()
                ? clone_ast(dash_line.inline_value)
                : AstNode(NullNode{});

            entry.emplace_back(dash_line.key, std::move(val));

            if (!dash_line.has_inline_value() && dash_line.has_children()) {
                // 不完整: "- key:" + 子内容 → 子内容是该 key 的值
                entry.back().second = build_ast(dash_line.children);
            } else if (dash_line.has_inline_value() && dash_line.has_children()) {
                // 完整条目有子内容 → 分离 dash 和非 dash 子行
                std::vector<Line> non_dash_kids, dash_kids;
                for (const auto& c : dash_line.children) {
                    if (c.is_dash()) dash_kids.push_back(c);
                    else non_dash_kids.push_back(c);
                }
                if (!non_dash_kids.empty()) {
                    absorb_children_as_siblings(entry, non_dash_kids);
                }
                // 先推入当前条目
                seq.items.push_back(AstNode(std::move(entry)));
                // dash 子行作为附加序列条目
                if (!dash_kids.empty()) {
                    AstNode extra = build_sequence(dash_kids);
                    if (extra.is_list()) {
                        for (auto& item : extra.as_list_mut()->items) {
                            seq.items.push_back(std::move(item));
                        }
                    } else {
                        seq.items.push_back(std::move(extra));
                    }
                }
                // 找到下一个 dash 行（跳过兄弟键收集逻辑）
                ++i;
                continue;
            }
        } else if (dash_line.kind == Line::Kind::DASH_SCALAR) {
            // 复杂模式: "- text" → 将 text 作为键，值为 null
            const AstScalar* sc = dash_line.inline_value.as_scalar();
            std::string key = sc ? sc->value : "";
            entry.emplace_back(key, AstNode(NullNode{}));

            if (dash_line.has_children()) {
                // 分离 dash 和非 dash 子行
                std::vector<Line> non_dash_kids, dash_kids;
                for (const auto& c : dash_line.children) {
                    if (c.is_dash()) dash_kids.push_back(c);
                    else non_dash_kids.push_back(c);
                }
                if (!non_dash_kids.empty()) {
                    absorb_children_as_siblings(entry, non_dash_kids);
                }
                // 先推入当前条目
                seq.items.push_back(AstNode(std::move(entry)));
                // dash 子行作为附加序列条目
                if (!dash_kids.empty()) {
                    AstNode extra = build_sequence(dash_kids);
                    if (extra.is_list()) {
                        for (auto& item : extra.as_list_mut()->items) {
                            seq.items.push_back(std::move(item));
                        }
                    } else {
                        seq.items.push_back(std::move(extra));
                    }
                }
                ++i;
                continue; // 跳过兄弟键收集
            }
        } else if (dash_line.kind == Line::Kind::DASH_EMPTY) {
            // 复杂模式: "-" 单独
            if (dash_line.has_children()) {
                AstNode child = build_ast(dash_line.children);
                if (child.is_map()) {
                    // 合并子 map 到 entry
                    for (auto& [k, v] : *child.as_map_mut()) {
                        entry.emplace_back(std::move(k), std::move(v));
                    }
                } else if (child.is_list()) {
                    // 子列表形成隐式键 "" 的值
                    entry.emplace_back("", std::move(child));
                } else {
                    entry.emplace_back("", std::move(child));
                }
            } else {
                // "-" 单独无子 → null
                seq.items.push_back(AstNode(NullNode{}));
                ++i;
                continue; // 无需收集兄弟键
            }
        }

        // ── 收集兄弟键：后面的非 DASH 行 ──
        size_t j = i + 1;
        while (j < lines.size() && !lines[j].is_dash()) {
            const Line& sibling = lines[j];

            if (sibling.kind == Line::Kind::KEY_VAL || sibling.kind == Line::Kind::BARE_KEY) {
                std::string key = sibling.key;
                AstNode val = AstNode(NullNode{});

                if (sibling.has_inline_value()) {
                    val = clone_ast(sibling.inline_value);
                    if (sibling.has_children()) {
                        absorb_children_as_siblings(entry, sibling.children);
                    }
                } else if (sibling.has_children()) {
                    val = build_ast(sibling.children);
                }

                entry.emplace_back(key, std::move(val));
            }
            ++j;
        }

        seq.items.push_back(AstNode(std::move(entry)));
        i = j;
    }

    return seq;
}

// ============================================================
// absorb_children_as_siblings
// ============================================================
void absorb_children_as_siblings(AstMap& map, const std::vector<Line>& children) {
    for (const auto& child : children) {
        if (child.is_dash()) {
            g_build_warnings.emplace_back(child.line_no, 1,
                "Dash entry found in irregular indent context, skipping");
            continue;
        }

        if (child.kind == Line::Kind::KEY_VAL || child.kind == Line::Kind::BARE_KEY) {
            std::string key = child.key;
            AstNode val = AstNode(NullNode{});

            if (child.has_inline_value()) {
                val = clone_ast(child.inline_value);
            } else if (child.has_children()) {
                val = build_ast(child.children);
            }

            map.emplace_back(key, std::move(val));
        }
    }
}

} // namespace stml
