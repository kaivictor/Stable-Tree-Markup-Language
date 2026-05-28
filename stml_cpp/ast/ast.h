#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace stml {

// Forward declarations
struct AstNode;
using AstList = std::vector<AstNode>;
using AstMap  = std::vector<std::pair<std::string, AstNode>>;

// =========================================================================
// AstValue — the recursive variant that holds the actual data.
//   nullptr_t  → null
//   string     → scalar string
//   AstList    → sequence (ordered list)
//   AstMap     → mapping (ordered key-value pairs, insertion order)
// =========================================================================
using AstValue = std::variant<
    std::nullptr_t,
    std::string,
    AstList,
    AstMap
>;

// =========================================================================
// AstNode — a node in the STML abstract syntax tree.
// =========================================================================
struct AstNode {
    AstValue value;

    // Constructors
    AstNode() : value(nullptr) {}
    AstNode(std::nullptr_t) : value(nullptr) {}
    AstNode(const char* s) : value(std::string(s)) {}
    AstNode(std::string s) : value(std::move(s)) {}
    AstNode(AstList lst) : value(std::move(lst)) {}
    AstNode(AstMap map) : value(std::move(map)) {}
    AstNode(AstValue val) : value(std::move(val)) {}

    // Type queries
    bool is_null()   const { return std::holds_alternative<std::nullptr_t>(value); }
    bool is_string() const { return std::holds_alternative<std::string>(value); }
    bool is_list()   const { return std::holds_alternative<AstList>(value); }
    bool is_map()    const { return std::holds_alternative<AstMap>(value); }

    // Convenience accessors (returns nullptr on type mismatch)
    const std::string* as_string() const;
    const AstList*     as_list()   const;
    const AstMap*      as_map()    const;

    // Mutable accessors
    std::string* as_string_mut();
    AstList*     as_list_mut();
    AstMap*      as_map_mut();
};

// =========================================================================
// AstMap helpers — linear search (AstMap is a vector of pairs)
// =========================================================================

/// Find a key in an AstMap. Returns nullptr if not found.
inline const AstNode* map_find(const AstMap& map, const std::string& key) {
    for (const auto& [k, v] : map) {
        if (k == key) return &v;
    }
    return nullptr;
}

/// Mutable version of map_find.
inline AstNode* map_find_mut(AstMap& map, const std::string& key) {
    for (auto& [k, v] : map) {
        if (k == key) return &v;
    }
    return nullptr;
}

// =========================================================================
// Comparison, cloning, traversal
// =========================================================================

bool operator==(const AstNode& a, const AstNode& b);
bool operator!=(const AstNode& a, const AstNode& b);

bool operator==(const AstList& a, const AstList& b);
bool operator!=(const AstList& a, const AstList& b);

bool operator==(const AstMap& a, const AstMap& b);
bool operator!=(const AstMap& a, const AstMap& b);

/// Deep copy of an AstNode.
AstNode clone(const AstNode& node);

/// Walk all leaf values in the AST, calling *visitor* for each string/null node.
/// The visitor receives the path (list of string keys / int indices) and the value.
using AstVisitor = void (*)(const std::vector<std::string>& path, const AstNode& node);
void walk(const AstNode& node, AstVisitor visitor);

} // namespace stml
