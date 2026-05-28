/// Recovery strategy functions for error handling.
///
/// The STML lexer never produces fatal errors — all malformed input is
/// recovered deterministically with warnings. This module documents the
/// recovery strategies for maintainability.
///
/// Recovery strategies (embedded in lexer.cpp):
///   - Unclosed quote          → RAW_STRING token + warning
///   - Malformed inline list   → RAW_STRING token + warning
///   - Block type conflict     → terminate current block + warning
///   - Multiline string EOF    → auto-close + warning
///   - Illegal escape sequence → kept as-is + warning
///   - Unclosed quote in key   → treated as key-name character

#include "diagnostics/error.h"

// This file exists as a placeholder for centralized recovery documentation.
// The actual recovery is implemented inline in lexer.cpp and parser.cpp.
// Future: if recovery becomes more complex, extract helper functions here.

namespace stml {

// Reserved for future recovery strategy functions.

} // namespace stml
