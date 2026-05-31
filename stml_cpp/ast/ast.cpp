<<<<<<< Updated upstream
#include "ast/ast.h"

namespace stml {

// =========================================================================
// Deep equality comparison of AstNodes.
// =========================================================================
bool operator==(const AstNode& a, const AstNode& b) {
    // Different variant indices → not equal
    if (a.value.index() != b.value.index()) return false;

    if (a.is_null()) return true; // both null

    if (a.is_scalar()) {
        return a.as_scalar()->value == b.as_scalar()->value;
    }

    if (a.is_list()) {
        const auto& la = *a.as_list();
        const auto& lb = *b.as_list();
        if (la.size() != lb.size()) return false;
        for (size_t i = 0; i < la.size(); ++i) {
            if (la[i] != lb[i]) return false;
        }
        return true;
    }

    if (a.is_map()) {
        const auto& ma = *a.as_map();
        const auto& mb = *b.as_map();
        if (ma.size() != mb.size()) return false;
        for (size_t i = 0; i < ma.size(); ++i) {
            if (ma[i].first != mb[i].first) return false;
            if (ma[i].second != mb[i].second) return false;
        }
        return true;
    }

    return false;
}

// =========================================================================
// Deep clone of an AstNode.
// =========================================================================
AstNode clone_ast(const AstNode& node) {
    if (node.is_null()) {
        return AstNode();
    }
    if (node.is_scalar()) {
        return AstNode(AstScalar(node.as_scalar()->value));
    }
    if (node.is_list()) {
        AstList list;
        for (const auto& child : *node.as_list()) {
            list.push_back(clone_ast(child));
        }
        return AstNode(std::move(list));
    }
    if (node.is_map()) {
        AstMap map;
        for (const auto& [k, v] : *node.as_map()) {
            map.emplace_back(k, clone_ast(v));
        }
        return AstNode(std::move(map));
    }
    return AstNode();
=======
#include "ast.h"

<<<<<<< Updated upstream
=======
namespace stml {

// ============================================================
// AstNode 深拷贝
// ============================================================
AstNode clone_ast(const AstNode& node) {
    switch (node.kind) {
        case AstNode::Kind::Null:
            return AstNode(NullNode{});
        case AstNode::Kind::Scalar:
            return AstNode(AstScalar{node.as_scalar()->value});
        case AstNode::Kind::List: {
            AstList new_list;
            for (const auto& item : node.as_list()->items) {
                new_list.items.push_back(clone_ast(item));
            }
            return AstNode(std::move(new_list));
        }
        case AstNode::Kind::Map: {
            AstMap new_map;
            for (const auto& [k, v] : *node.as_map()) {
                new_map.emplace_back(k, clone_ast(v));
            }
            return AstNode(std::move(new_map));
        }
    }
    return AstNode(NullNode{});
}

// ============================================================
// AstNode 比较
// ============================================================
bool operator==(const AstNode& a, const AstNode& b) {
    if (a.kind != b.kind) return false;
    switch (a.kind) {
        case AstNode::Kind::Null:
            return true;
        case AstNode::Kind::Scalar:
            return a.as_scalar()->value == b.as_scalar()->value;
        case AstNode::Kind::List: {
            const auto& la = a.as_list()->items;
            const auto& lb = b.as_list()->items;
            if (la.size() != lb.size()) return false;
            for (size_t i = 0; i < la.size(); ++i) {
                if (la[i] != lb[i]) return false;
            }
            return true;
        }
        case AstNode::Kind::Map: {
            const auto& ma = *a.as_map();
            const auto& mb = *b.as_map();
            if (ma.size() != mb.size()) return false;
            for (size_t i = 0; i < ma.size(); ++i) {
                if (ma[i].first != mb[i].first) return false;
                if (ma[i].second != mb[i].second) return false;
            }
            return true;
        }
    }
    return false;
>>>>>>> Stashed changes
}

>>>>>>> Stashed changes
} // namespace stml
