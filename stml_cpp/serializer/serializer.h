<<<<<<< Updated upstream
#pragma once

#include "ast/ast.h"
=======
#ifndef STML_SERIALIZER_SERIALIZER_H
#define STML_SERIALIZER_SERIALIZER_H

<<<<<<< Updated upstream
#include "ast/ast.h"
=======
#include "../ast/ast.h"
>>>>>>> Stashed changes
>>>>>>> Stashed changes
#include <string>

namespace stml {

<<<<<<< Updated upstream
// =========================================================================
// Serializer — converts AST to various text formats.
// =========================================================================

/// Serialize an AST node to canonical STML text.
/// Handles the top-level {"docs": [...]} wrapper by emitting `---`
/// separators between documents.
std::string to_stml(const AstNode& node);

/// Serialize an AST node to JSON text.
/// Output is always pretty-printed (2-space indent).
/// The top-level {"docs": [...]} wrapper is preserved.
std::string to_json(const AstNode& node);
=======
// ============================================================
// Serializer — AST → JSON / STML 文本
// ============================================================

// AST → JSON 字符串
std::string to_json(const AstNode& node, int indent = 2);

// AstList (docs) → wrapped JSON: {"docs": [...]}
std::string docs_to_json(const AstList& docs, int indent = 2);

// AST → STML 字符串
std::string to_stml(const AstNode& node, int base_indent = 0, int indent_step = 2);

// AstList (docs) → STML 多文档
std::string docs_to_stml(const AstList& docs, int indent_step = 2);
>>>>>>> Stashed changes

/// Convenience: to_stml is also known as dumps.
inline std::string dumps(const AstNode& node) {
    return to_stml(node);
}

/// Convenience: to_stml is also known as dumps.
inline std::string dumps(const AstNode& node) {
    return to_stml(node);
}

} // namespace stml

#endif // STML_SERIALIZER_SERIALIZER_H
