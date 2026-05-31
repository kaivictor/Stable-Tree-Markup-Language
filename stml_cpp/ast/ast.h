#ifndef STML_AST_H
#define STML_AST_H

<<<<<<< Updated upstream
=======
<<<<<<< Updated upstream
>>>>>>> Stashed changes
#include <cstddef>
#include <memory>
=======
>>>>>>> Stashed changes
#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <cstdint>

namespace stml {

<<<<<<< Updated upstream
=======
<<<<<<< Updated upstream
>>>>>>> Stashed changes
// =========================================================================
// Forward declarations for recursive types.
// =========================================================================
class AstNode;

/// An empty struct representing the null value.
struct NullNode {};

/// A scalar string value.
struct AstScalar {
    std::string value;

    AstScalar() = default;
    explicit AstScalar(std::string v) : value(std::move(v)) {}
};

/// AstList: an ordered sequence of AstNodes.
using AstList = std::vector<AstNode>;

/// AstMap: an ordered list of key-value pairs (insertion order preserved).
using AstMap = std::vector<std::pair<std::string, AstNode>>;

/// AstValue: the underlying variant for AstNode.
using AstValue = std::variant<std::nullptr_t, AstScalar, AstList, AstMap>;

// =========================================================================
// AstNode — the single runtime AST node type.
// =========================================================================
class AstNode {
public:
    AstValue value;

    // ---- Constructors ----
    AstNode() : value(nullptr) {}
    AstNode(std::nullptr_t) : value(nullptr) {}
    AstNode(NullNode) : value(nullptr) {}
    AstNode(const char* s) : value(AstScalar(std::string(s))) {}
    AstNode(std::string s) : value(AstScalar(std::move(s))) {}
    AstNode(AstScalar s) : value(std::move(s)) {}
    AstNode(AstList l) : value(std::move(l)) {}
    AstNode(AstMap m) : value(std::move(m)) {}

    // ---- Type queries ----
    bool is_null() const noexcept { return std::holds_alternative<std::nullptr_t>(value); }
    bool is_scalar() const noexcept { return std::holds_alternative<AstScalar>(value); }
    bool is_list() const noexcept { return std::holds_alternative<AstList>(value); }
    bool is_map() const noexcept { return std::holds_alternative<AstMap>(value); }

    // ---- Const accessors (return nullptr on type mismatch) ----
    const AstScalar* as_scalar() const noexcept {
        return std::get_if<AstScalar>(&value);
    }
    const AstList* as_list() const noexcept {
        return std::get_if<AstList>(&value);
    }
    const AstMap* as_map() const noexcept {
        return std::get_if<AstMap>(&value);
    }

    // ---- Mutable accessors (return nullptr on type mismatch) ----
    AstScalar* as_scalar_mut() noexcept {
        return std::get_if<AstScalar>(&value);
    }
    AstList* as_list_mut() noexcept {
        return std::get_if<AstList>(&value);
    }
    AstMap* as_map_mut() noexcept {
        return std::get_if<AstMap>(&value);
    }

    // ---- Convenience: get scalar string (returns "" if not scalar) ----
    const std::string& scalar_value() const noexcept {
        static const std::string empty;
        const auto* s = as_scalar();
        return s ? s->value : empty;
    }
};

// ---- Operators ----
bool operator==(const AstNode& a, const AstNode& b);
inline bool operator!=(const AstNode& a, const AstNode& b) { return !(a == b); }

// ---- Deep clone ----
AstNode clone_ast(const AstNode& node);

// ---- Map helpers (operate on AstMap = vector<pair<string, AstNode>>) ----

/// Find a key in an AstMap. Returns pointer to value, or nullptr if not found.
<<<<<<< Updated upstream
=======
=======
// 仅前向声明 AstNode（AstList/AstMap 需要引用它）
struct AstNode;

// ============================================================
// Null — 空值
// ============================================================
struct NullNode {};

inline bool operator==(const NullNode&, const NullNode&) { return true; }
inline bool operator!=(const NullNode&, const NullNode&) { return false; }

// ============================================================
// AstScalar — 标量字符串
// ============================================================
struct AstScalar {
    std::string value;
};

// ============================================================
// AstList — 有序列表
// ============================================================
struct AstList {
    std::vector<AstNode> items;
};

// ============================================================
// AstMap — 有序键值对（insertion order, allows duplicate keys）
// ============================================================
using AstMap = std::vector<std::pair<std::string, AstNode>>;

// ============================================================
// AstNode — 四种 AST 节点之和类型
// ============================================================
struct AstNode {
    enum class Kind : uint8_t { Null, Scalar, List, Map } kind;
    std::variant<NullNode, AstScalar, AstList, AstMap> data;

    // 构造 Null
    AstNode() : kind(Kind::Null), data(NullNode{}) {}
    AstNode(NullNode n) : kind(Kind::Null), data(std::move(n)) {}
    AstNode(AstScalar s) : kind(Kind::Scalar), data(std::move(s)) {}
    AstNode(AstList l) : kind(Kind::List), data(std::move(l)) {}
    AstNode(AstMap m) : kind(Kind::Map), data(std::move(m)) {}

    // 便捷判断
    bool is_null()   const { return kind == Kind::Null; }
    bool is_scalar() const { return kind == Kind::Scalar; }
    bool is_list()   const { return kind == Kind::List; }
    bool is_map()    const { return kind == Kind::Map; }

    // 便捷访问
    const AstScalar* as_scalar() const {
        return is_scalar() ? &std::get<AstScalar>(data) : nullptr;
    }
    const AstList* as_list() const {
        return is_list() ? &std::get<AstList>(data) : nullptr;
    }
    const AstMap* as_map() const {
        return is_map() ? &std::get<AstMap>(data) : nullptr;
    }
    AstScalar* as_scalar_mut() {
        return is_scalar() ? &std::get<AstScalar>(data) : nullptr;
    }
    AstList* as_list_mut() {
        return is_list() ? &std::get<AstList>(data) : nullptr;
    }
    AstMap* as_map_mut() {
        return is_map() ? &std::get<AstMap>(data) : nullptr;
    }
};

// ============================================================
// AstMap 辅助函数
// ============================================================
>>>>>>> Stashed changes
>>>>>>> Stashed changes
inline const AstNode* map_find(const AstMap& map, const std::string& key) {
    for (const auto& [k, v] : map) {
        if (k == key) return &v;
    }
    return nullptr;
}

<<<<<<< Updated upstream
/// Mutable find: returns pointer to value, or nullptr if not found.
=======
<<<<<<< Updated upstream
/// Mutable find: returns pointer to value, or nullptr if not found.
=======
>>>>>>> Stashed changes
>>>>>>> Stashed changes
inline AstNode* map_find_mut(AstMap& map, const std::string& key) {
    for (auto& [k, v] : map) {
        if (k == key) return &v;
    }
    return nullptr;
}

<<<<<<< Updated upstream
=======
<<<<<<< Updated upstream
=======
// ============================================================
// AstNode 深拷贝辅助
// ============================================================
AstNode clone_ast(const AstNode& node);

// ============================================================
// 比较
// ============================================================
bool operator==(const AstNode& a, const AstNode& b);
inline bool operator!=(const AstNode& a, const AstNode& b) { return !(a == b); }

>>>>>>> Stashed changes
>>>>>>> Stashed changes
} // namespace stml

#endif // STML_AST_H
