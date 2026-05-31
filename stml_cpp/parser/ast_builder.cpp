#include "parser/ast_builder.h"

namespace stml {

// =========================================================================
// Public API
// =========================================================================

std::vector<AstNode> AstBuilder::build_all(const std::vector<std::vector<Line>>& doc_lines) {
    std::vector<AstNode> docs;
    docs.reserve(doc_lines.size());
    for (const auto& lines : doc_lines) {
        docs.push_back(build(lines));
    }
    return docs;
}

AstNode AstBuilder::build(const std::vector<Line>& lines) {
    return build_children(lines);
}

// =========================================================================
// Internal helpers
// =========================================================================

AstNode AstBuilder::build_line(const Line& line) {
    if (line.children.empty()) {
        // Leaf node — return whatever inline value we have
        if (line.kind == Line::Kind::BARE_KEY) {
            return AstNode(); // bare key → null
        }
        return line.inline_value;
    }

    // This line has children — build a compound value from them
    return build_children(line.children);
}

AstNode AstBuilder::build_children(const std::vector<Line>& children) {
    if (children.empty()) {
        return AstNode();
    }

    // Determine if all children are dash entries (sequence)
    bool all_dash = true;
    for (const auto& line : children) {
        if (!line.is_dash()) {
            all_dash = false;
            break;
        }
    }

    if (all_dash) {
        // Build as AstList
        AstList list;
        list.reserve(children.size());
        for (const auto& line : children) {
            switch (line.kind) {
                case Line::Kind::DASH_EMPTY:
                    // "- " → null entry
                    if (line.children.empty()) {
                        list.push_back(AstNode());
                    } else {
                        // Rare: dash with children but no key → list of children
                        list.push_back(build_children(line.children));
                    }
                    break;

                case Line::Kind::DASH_SCALAR:
                    if (line.children.empty()) {
                        list.push_back(line.inline_value);
                    } else {
                        // Sequence entry with both inline value and children
                        // Children win (they define the value)
                        list.push_back(build_children(line.children));
                    }
                    break;

                case Line::Kind::DASH_KEY_VAL: {
                    // "- key: value" — this is a map entry in a list
                    AstMap map;
                    if (!line.key.empty()) {
                        AstNode value = line.children.empty()
                            ? line.inline_value
                            : build_children(line.children);
                        map.emplace_back(line.key, std::move(value));
                    }
                    list.push_back(AstNode(std::move(map)));
                    break;
                }

                default:
                    list.push_back(AstNode());
                    break;
            }
        }
        return AstNode(std::move(list));
    }

    // Build as AstMap
    AstMap map;
    map.reserve(children.size());
    for (const auto& line : children) {
        if (line.is_dash()) {
            // A dash entry inside a mapping context (mixed type)
            // Treat as a sequence under the parent key
            AstList sub_list;
            sub_list.push_back(build_line(line));
            // Use empty key for this entry
            map.emplace_back("", AstNode(std::move(sub_list)));
        } else {
            std::string key = line.key;
            if (key.empty()) continue; // skip lines without a key

            AstNode value = line.children.empty()
                ? (line.kind == Line::Kind::BARE_KEY ? AstNode() : line.inline_value)
                : build_children(line.children);

            map.emplace_back(std::move(key), std::move(value));
        }
    }

    // If we have inline map entries from a parent, they are handled
    // by the caller (build_line) via children. No special merging needed.

    return AstNode(std::move(map));
}

} // namespace stml
