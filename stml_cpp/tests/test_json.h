#ifndef STML_TESTS_TEST_JSON_H
#define STML_TESTS_TEST_JSON_H

<<<<<<< Updated upstream
=======
<<<<<<< Updated upstream
>>>>>>> Stashed changes
/// Minimal JSON parser used for testing comparisons only.
/// Not a general-purpose JSON library.

#include "ast/ast.h"
<<<<<<< Updated upstream
=======
=======
#include "../ast/ast.h"
>>>>>>> Stashed changes
>>>>>>> Stashed changes
#include <string>

namespace stml {
namespace test_json {

<<<<<<< Updated upstream
/// Parse a JSON string into an AstNode.
/// Supports: objects, arrays, strings, numbers (as strings), null, true, false, boolean.
/// Throws ParseError on malformed input.
AstNode parse(const std::string& json);

} // namespace test_json
} // namespace stml
<<<<<<< Updated upstream
=======
=======
// Parse JSON string → AstNode (for test comparison)
stml::AstNode parse(const std::string& json);

} // namespace test_json

#endif
>>>>>>> Stashed changes
>>>>>>> Stashed changes
