#include "ast/ast.h"

#include <functional> // for std::function in walk

namespace stml {

// -----------------------------------------------------------------------
// AstNode accessors
// -----------------------------------------------------------------------

const std::string* AstNode::as_string() const {
    if (auto* p = std::get_if<std::string>(&value)) return p;
    return nullptr;
}

const AstList* AstNode::as_list() const {
    if (auto* p = std::get_if<AstList>(&value)) return p;
    return nullptr;
}

const AstMap* AstNode::as_map() const {
    if (auto* p = std::get_if<AstMap>(&value)) return p;
    return nullptr;
}

std::string* AstNode::as_string_mut() {
    if (auto* p = std::get_if<std::string>(&value)) return p;
    return nullptr;
}

AstList* AstNode::as_list_mut() {
    if (auto* p = std::get_if<AstList>(&value)) return p;
    return nullptr;
}

AstMap* AstNode::as_map_mut() {
    if (auto* p = std::get_if<AstMap>(&value)) return p;
    return nullptr;
}

// -----------------------------------------------------------------------
// Comparison
// -----------------------------------------------------------------------

bool operator==(const AstNode& a, const AstNode& b) {
    return a.value == b.value;
}

bool operator!=(const AstNode& a, const AstNode& b) {
    return !(a == b);
}

bool operator==(const AstList& a, const AstList& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

bool operator!=(const AstList& a, const AstList& b) {
    return !(a == b);
}

bool operator==(const AstMap& a, const AstMap& b) {
    if (a.size() != b.size()) return false;
    auto ai = a.begin();
    auto bi = b.begin();
    while (ai != a.end()) {
        if (ai->first != bi->first) return false;
        if (ai->second != bi->second) return false;
        ++ai; ++bi;
    }
    return true;
}

bool operator!=(const AstMap& a, const AstMap& b) {
    return !(a == b);
}

// -----------------------------------------------------------------------
// clone — deep copy
// -----------------------------------------------------------------------

AstNode clone(const AstNode& node) {
    if (node.is_null()) {
        return AstNode();
    }
    if (node.is_string()) {
        return AstNode(*node.as_string());
    }
    if (node.is_list()) {
        AstList list;
        for (const auto& item : *node.as_list()) {
            list.push_back(clone(item));
        }
        return AstNode(std::move(list));
    }
    if (node.is_map()) {
        AstMap map;
        for (const auto& [k, v] : *node.as_map()) {
            map.emplace_back(k, clone(v));
        }
        return AstNode(std::move(map));
    }
    return AstNode();
}

// -----------------------------------------------------------------------
// walk — depth-first traversal of leaf values
// An internal helper uses std::function for recursion.
// -----------------------------------------------------------------------

namespace {

void walk_impl(const AstNode& node, std::vector<std::string>& path,
               const std::function<void(const std::vector<std::string>&, const AstNode&)>& visitor) {
    if (node.is_null() || node.is_string()) {
        visitor(path, node);
    } else if (node.is_list()) {
        const auto& list = *node.as_list();
        for (size_t i = 0; i < list.size(); ++i) {
            path.push_back(std::to_string(i));
            walk_impl(list[i], path, visitor);
            path.pop_back();
        }
    } else if (node.is_map()) {
        for (const auto& [k, v] : *node.as_map()) {
            path.push_back(k);
            walk_impl(v, path, visitor);
            path.pop_back();
        }
    }
}

} // anonymous namespace

void walk(const AstNode& node, AstVisitor visitor) {
    std::vector<std::string> path;
    // Wrap the raw function pointer in a lambda that calls it
    auto wrapped = [visitor](const std::vector<std::string>& p, const AstNode& n) {
        visitor(p, n);
    };
    walk_impl(node, path, std::function<void(const std::vector<std::string>&, const AstNode&)>(wrapped));
}

} // namespace stml
