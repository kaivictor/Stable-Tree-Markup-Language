#ifndef STML_STML_H
#define STML_STML_H

<<<<<<< Updated upstream
=======
<<<<<<< Updated upstream
>>>>>>> Stashed changes
/// \file stml.h
/// Public API for the STML C++ library.
///
/// Include this single header to use all STML functionality.

#include "ast/ast.h"
#include "lexer/token.h"
#include "diagnostics/error.h"
#include "serializer/serializer.h"
<<<<<<< Updated upstream
=======
=======
#include "ast/ast.h"
#include "diagnostics/error.h"
>>>>>>> Stashed changes
>>>>>>> Stashed changes
#include <string>
#include <vector>
#include <utility>

namespace stml {

<<<<<<< Updated upstream
// =========================================================================
// LoadResult — returned by loads() and load().
// =========================================================================
struct LoadResult {
    AstNode ast;
    std::vector<Warning> warnings;

    LoadResult() = default;
    LoadResult(AstNode a, std::vector<Warning> w)
        : ast(std::move(a)), warnings(std::move(w)) {}

    /// Implicit conversion to AstNode for backward compatibility.
    operator AstNode() const { return ast; }
};

// =========================================================================
// High-level API
// =========================================================================

/// Parse STML text and return the AST with warnings.
LoadResult loads(const std::string& text);

/// Parse STML from a file and return the AST with warnings.
LoadResult load(const std::string& filepath);

/// Parse STML text from a C-string.
inline LoadResult loads(const char* text) {
    return loads(std::string(text));
}

// =========================================================================
// Low-level / debugging API
// =========================================================================

/// Tokenize STML text without building an AST.
/// Returns the token stream and any warnings.
std::pair<std::vector<Token>, std::vector<Warning>>
tokenize(const std::string& text);

/// Parse a pre-tokenized token stream into an AST.
/// Returns the AST and any warnings.
std::pair<AstNode, std::vector<Warning>>
parse(const std::vector<Token>& tokens);

} // namespace stml
<<<<<<< Updated upstream
=======
=======
// ============================================================
// Stml — 顶层 API
// ============================================================

// 从 STML 文本解析为 AST 文档列表
// 返回: AstList 包含所有文档
AstList loads(const std::string& input);

// 从 STML 文件解析
AstList load(const std::string& filepath);

// AST 文档列表 → STML 文本
std::string dumps(const AstList& docs, int indent = 2);

// AST 文档列表 → JSON 文本
std::string to_json(const AstList& docs, int indent = 2);

} // namespace stml

#endif // STML_STML_H
>>>>>>> Stashed changes
>>>>>>> Stashed changes
