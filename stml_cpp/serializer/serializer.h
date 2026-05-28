#pragma once

#include <string>

#include "ast/ast.h"

namespace stml {

// =========================================================================
// Serializer: AST → canonical STML text / JSON text
// =========================================================================

/// Escape special characters for double-quoted strings.
std::string escape_string(const std::string& s);

/// Format a key (always double-quoted).
std::string quote_key(const std::string& key);

/// Format a scalar value as STML representation.
/// null → "null", strings → "\"value\"", etc.
std::string format_scalar(const AstNode& node);

/// Convert an AST node to a (possibly multi-line) STML fragment.
/// Indentation: 2 spaces per level.
std::string serialize_node(const AstNode& node, int indent_level = 0);

/// Convert docs-wrapped AST to canonical STML text.
/// Format: {"docs": [{...}, {...}, ...]}
std::string dumps(const AstNode& node);

/// Convert an AST node to JSON text.
std::string to_json(const AstNode& node);

} // namespace stml
