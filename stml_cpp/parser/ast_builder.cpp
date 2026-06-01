#include "ast_builder.h"

namespace stml {

// ============================================================
// build_all — build multiple documents
// ============================================================
std::vector<AstNode> AstBuilder::build_all(const std::vector<std::vector<Line>>& doc_lines) {
    std::vector<AstNode> docs;
    for (const auto& lines : doc_lines) {
        docs.push_back(build(lines));
    }
    return docs;
}

// ============================================================
// build — main entry
// ============================================================
AstNode AstBuilder::build(const std::vector<Line>& lines) {
    if (lines.empty()) return AstNode(nullptr);

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
    }

    // 混合块：如果首行是 dash → 路由到 complex sequence builder
    // （不规则缩进下，非 dash 行可能混在 dash 行之间，应被吸收为兄弟键）
    bool first_is_dash = !lines.empty() && lines[0].is_dash();
    if (first_is_dash) {
        return build_sequence(lines);
    }

    return AstNode(build_mapping(lines));
}

// ============================================================
// build_mapping
// ============================================================
AstMap AstBuilder::build_mapping(const std::vector<Line>& lines) {
    AstMap map;
    size_t i = 0;

    while (i < lines.size()) {
        const Line& line = lines[i];
        if (!(line.kind == Line::Kind::KEY_VAL || line.kind == Line::Kind::BARE_KEY)) {
            ++i;
            continue;
        }

        std::string key = line.key;
        AstNode val = AstNode(nullptr);

        if (line.has_inline_value()) {
            // 完整条目：有行内值
            val = clone(line.inline_value);
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
            val = build(line.children);
            ++i;
            // 检查后续同级行是否为 dash 序列（不规则缩进导致分离的序列条目）
            if (val.is_list()) {
                size_t j = i;
                while (j < lines.size() && lines[j].is_dash()) {
                    ++j;
                }
                if (j > i) {
                    std::vector<Line> dash_lines(lines.begin() + i, lines.begin() + j);
                    AstNode extra = build(dash_lines);
                    if (extra.is_list()) {
                        AstList& val_list = *val.as_list_mut();
                        AstList& extra_list = *extra.as_list_mut();
                        for (auto& item : extra_list) {
                            val_list.push_back(std::move(item));
                        }
                    } else {
                        val.as_list_mut()->push_back(std::move(extra));
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
                val = build(dash_lines);
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
AstNode AstBuilder::build_sequence(const std::vector<Line>& lines) {
    // 预扫描：是否有 DASH_KEY_VAL 或非 dash 行（混合块）？
    bool is_complex = false;
    bool has_non_dash = false;
    for (const auto& line : lines) {
        if (line.kind == Line::Kind::DASH_KEY_VAL) {
            is_complex = true;
        }
        if (!line.is_dash()) {
            has_non_dash = true;
        }
    }

    // 混合块（dash + 非 dash）必须走 complex 路径
    if (is_complex || has_non_dash) {
        return AstNode(build_complex_sequence(lines));
    } else {
        return AstNode(build_simple_sequence(lines));
    }
}

// ============================================================
// build_simple_sequence
// ============================================================
AstList AstBuilder::build_simple_sequence(const std::vector<Line>& lines) {
    AstList seq;

    for (const auto& line : lines) {
        if (!line.is_dash()) continue;

        if (line.kind == Line::Kind::DASH_EMPTY) {
            if (line.has_children()) {
                // 先推入 null 表示空条目自身
                seq.push_back(AstNode(nullptr));
                // 然后提升子内容到当前序列
                AstNode child = build(line.children);
                if (child.is_list()) {
                    // 子列表 → 展开合并
                    for (auto& item : *child.as_list_mut()) {
                        seq.push_back(std::move(item));
                    }
                } else {
                    // Map 或 Scalar → 作为一个元素
                    seq.push_back(std::move(child));
                }
            } else {
                // "-" 单独 → null
                seq.push_back(AstNode(nullptr));
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
                    warnings_.emplace_back(line.line_no, 1,
                        "Dash scalar entry has unexpected child content, ignoring children");
                }
                seq.push_back(clone(line.inline_value));
                if (!dash_kids.empty()) {
                    AstNode extra = build_sequence(dash_kids);
                    if (extra.is_list()) {
                        for (auto& item : *extra.as_list_mut()) {
                            seq.push_back(std::move(item));
                        }
                    } else {
                        seq.push_back(std::move(extra));
                    }
                }
            } else {
                seq.push_back(clone(line.inline_value));
            }
        }
    }

    return seq;
}

// ============================================================
// build_complex_sequence
// ============================================================
AstList AstBuilder::build_complex_sequence(const std::vector<Line>& lines) {
    AstList seq;
    size_t i = 0;

    while (i < lines.size()) {
        if (!lines[i].is_dash()) {
            // 检查是否还有后续 dash 行来收集这些非 dash 行作为兄弟键
            bool any_dash_left = false;
            for (size_t k = i + 1; k < lines.size(); ++k) {
                if (lines[k].is_dash()) { any_dash_left = true; break; }
            }
            if (!any_dash_left) break; // 尾部非 dash 行，由循环后代码处理
            ++i;
            continue;
        }

        AstMap entry;
        const Line& dash_line = lines[i];

        // ── 处理 DASH 行本身 ──
        if (dash_line.kind == Line::Kind::DASH_KEY_VAL) {
            AstNode val = dash_line.has_inline_value()
                ? clone(dash_line.inline_value)
                : AstNode(nullptr);

            entry.emplace_back(dash_line.key, std::move(val));

            if (!dash_line.has_inline_value() && dash_line.has_children()) {
                // 不完整: "- key:" + 子内容 → 子内容是该 key 的值
                entry.back().second = build(dash_line.children);
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
                seq.push_back(AstNode(std::move(entry)));
                // dash 子行作为附加序列条目
                if (!dash_kids.empty()) {
                    AstNode extra = build_sequence(dash_kids);
                    if (extra.is_list()) {
                        for (auto& item : *extra.as_list_mut()) {
                            seq.push_back(std::move(item));
                        }
                    } else {
                        seq.push_back(std::move(extra));
                    }
                }
                // 找到下一个 dash 行（跳过兄弟键收集逻辑）
                ++i;
                continue;
            }
        } else if (dash_line.kind == Line::Kind::DASH_SCALAR) {
            // 复杂模式: "- text" → 将 text 作为键，值为 null
            const std::string* sc = dash_line.inline_value.as_string();
            std::string key = sc ? *sc : "";
            entry.emplace_back(key, AstNode(nullptr));

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
                // dash 子行 → 推入当前条目及附加序列条目，跳过兄弟键收集
                if (!dash_kids.empty()) {
                    seq.push_back(AstNode(std::move(entry)));
                    AstNode extra = build_sequence(dash_kids);
                    if (extra.is_list()) {
                        for (auto& item : *extra.as_list_mut()) {
                            seq.push_back(std::move(item));
                        }
                    } else {
                        seq.push_back(std::move(extra));
                    }
                    ++i;
                    continue;
                }
                // 无 dash 子行 → 落入下方兄弟键收集（i 不变，j=i+1 开始收集）
            }
        } else if (dash_line.kind == Line::Kind::DASH_EMPTY) {
            // 复杂模式: "-" 单独
            if (dash_line.has_children()) {
                AstNode child = build(dash_line.children);
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
                seq.push_back(AstNode(nullptr));
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
                AstNode val = AstNode(nullptr);

                if (sibling.has_inline_value()) {
                    val = clone(sibling.inline_value);
                    if (sibling.has_children()) {
                        absorb_children_as_siblings(entry, sibling.children);
                    }
                } else if (sibling.has_children()) {
                    val = build(sibling.children);
                }

                entry.emplace_back(key, std::move(val));
            }
            ++j;
        }

        seq.push_back(AstNode(std::move(entry)));
        i = j;
    }

    // ── 处理尾部非 DASH 行（没有后续 dash 来触发兄弟键收集）──
    if (i < lines.size()) {
        std::vector<Line> trailing(lines.begin() + i, lines.end());
        AstNode extra = build(trailing);
        if (extra.is_map()) {
            seq.push_back(std::move(extra));
        } else if (extra.is_list()) {
            for (auto& item : *extra.as_list_mut()) {
                seq.push_back(std::move(item));
            }
        }
    }

    return seq;
}

// ============================================================
// absorb_children_as_siblings
// ============================================================
void AstBuilder::absorb_children_as_siblings(AstMap& map, const std::vector<Line>& children) {
    for (const auto& child : children) {
        if (child.is_dash()) {
            warnings_.emplace_back(child.line_no, 1,
                "Dash entry found in irregular indent context, skipping");
            continue;
        }

        if (child.kind == Line::Kind::KEY_VAL || child.kind == Line::Kind::BARE_KEY) {
            std::string key = child.key;
            AstNode val = AstNode(nullptr);

            if (child.has_inline_value()) {
                val = clone(child.inline_value);
            } else if (child.has_children()) {
                val = build(child.children);
            }

            map.emplace_back(key, std::move(val));
        }
    }
}

} // namespace stml
