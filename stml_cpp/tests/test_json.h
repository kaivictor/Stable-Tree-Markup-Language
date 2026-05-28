#pragma once

/// Minimal JSON parser for test verification only.
/// Parses a JSON string into AstNode for comparison with parser output.

#include "ast/ast.h"
#include <fstream>
#include <string>

namespace test_json {

/// Parse a JSON string into an AstNode.
stml::AstNode parse(const std::string& json_text);

/// Load and parse a JSON file.
stml::AstNode load(const std::string& filepath);

} // namespace test_json
