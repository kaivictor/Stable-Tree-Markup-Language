/// STML Python bindings — pybind11 bridge between C++ core and Python.
///
/// Exposes:
///   loads(text)      → (ast, warnings)       may throw ParseError
///   load(filename)   → (ast, warnings)       may throw ParseError
///   dumps(ast)       → str
///   to_json(ast)     → str
///   tokenize(text)   → (tokens, warnings)   [debug]
///   parse(tokens)    → (ast, warnings)       [debug]  may throw ParseError
///
/// Exceptions:
///   ParseError — fatal parse error (line, column, message). Inherits RuntimeError.
///
/// AstNode (C++ variant) ↔ Python native types:
///   nullptr_t → None    string → str    AstList → list    AstMap → dict

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "stml.h"

namespace py = pybind11;

// =========================================================================
// AstNode ↔ Python recursive conversion
// =========================================================================

/// C++ AstNode → Python object (None / str / list / dict)
static py::object ast_to_python(const stml::AstNode& node)
{
    if (node.is_null()) {
        return py::none();
    }
    if (node.is_string()) {
        return py::str(*node.as_string());
    }
    if (node.is_list()) {
        py::list result;
        for (const auto& item : *node.as_list()) {
            result.append(ast_to_python(item));
        }
        return std::move(result);
    }
    if (node.is_map()) {
        py::dict result;
        for (const auto& [key, val] : *node.as_map()) {
            result[py::str(key)] = ast_to_python(val);
        }
        return std::move(result);
    }
    return py::none(); // unreachable
}

/// Python object → C++ AstNode.
/// None→null, str→string, list→AstList, dict→AstMap.
/// Numeric/bool types are converted to their string representation
/// (STML has no numeric type).
static stml::AstNode python_to_ast(const py::handle& obj)
{
    if (obj.is_none()) {
        return stml::AstNode(nullptr);
    }
    if (py::isinstance<py::str>(obj)) {
        return stml::AstNode(obj.cast<std::string>());
    }
    if (py::isinstance<py::list>(obj)) {
        stml::AstList lst;
        for (auto item : obj.cast<py::list>()) {
            lst.push_back(python_to_ast(item));
        }
        return stml::AstNode(std::move(lst));
    }
    if (py::isinstance<py::dict>(obj)) {
        stml::AstMap map;
        for (auto [key, val] : obj.cast<py::dict>()) {
            map.emplace_back(py::str(key).cast<std::string>(), python_to_ast(val));
        }
        return stml::AstNode(std::move(map));
    }
    // int, float, bool, etc. — stringify
    if (py::isinstance<py::int_>(obj) || py::isinstance<py::float_>(obj)
        || py::isinstance<py::bool_>(obj)) {
        return stml::AstNode(py::str(obj).cast<std::string>());
    }
    // Last resort: convert to string
    return stml::AstNode(py::str(obj).cast<std::string>());
}

// =========================================================================
// Warning list conversion
// =========================================================================

static py::list warnings_to_python(const std::vector<stml::Warning>& warnings)
{
    py::list result;
    for (const auto& w : warnings) {
        result.append(py::make_tuple(w.message, w.line, w.column));
    }
    return result;
}

// =========================================================================
// Token ↔ Python conversion (for the debug tokenize() function)
// =========================================================================

static const char* token_type_name(stml::TokenType type)
{
    switch (type) {
        case stml::TokenType::NEWLINE:          return "NEWLINE";
        case stml::TokenType::INDENT:           return "INDENT";
        case stml::TokenType::DEDENT:           return "DEDENT";
        case stml::TokenType::DOC_SEPARATOR:    return "DOC_SEPARATOR";
        case stml::TokenType::END:              return "END";
        case stml::TokenType::KEY:              return "KEY";
        case stml::TokenType::BARE_KEY:         return "BARE_KEY";
        case stml::TokenType::COLON:            return "COLON";
        case stml::TokenType::SCALAR:           return "SCALAR";
        case stml::TokenType::NULL_:            return "NULL_";
        case stml::TokenType::RAW_STRING:       return "RAW_STRING";
        case stml::TokenType::DASH:             return "DASH";
        case stml::TokenType::INLINE_LIST:      return "INLINE_LIST";
        case stml::TokenType::MULTILINE_STRING: return "MULTILINE_STRING";
    }
    return "UNKNOWN";
}

static py::object token_value_to_python(const stml::TokenValue& val)
{
    if (std::holds_alternative<std::monostate>(val)) {
        return py::none();
    }
    if (std::holds_alternative<std::string>(val)) {
        return py::str(std::get<std::string>(val));
    }
    // INLINE_LIST — vector<InlineElem> where InlineElem = optional<string>
    py::list result;
    for (const auto& elem : std::get<std::vector<stml::InlineElem>>(val)) {
        if (elem.has_value()) {
            result.append(py::str(elem.value()));
        } else {
            result.append(py::none());
        }
    }
    return std::move(result);
}

static stml::TokenType token_type_from_name(const std::string& name)
{
    if (name == "NEWLINE")          return stml::TokenType::NEWLINE;
    if (name == "INDENT")           return stml::TokenType::INDENT;
    if (name == "DEDENT")           return stml::TokenType::DEDENT;
    if (name == "DOC_SEPARATOR")    return stml::TokenType::DOC_SEPARATOR;
    if (name == "END")              return stml::TokenType::END;
    if (name == "KEY")              return stml::TokenType::KEY;
    if (name == "BARE_KEY")         return stml::TokenType::BARE_KEY;
    if (name == "COLON")            return stml::TokenType::COLON;
    if (name == "SCALAR")           return stml::TokenType::SCALAR;
    if (name == "NULL_")            return stml::TokenType::NULL_;
    if (name == "RAW_STRING")       return stml::TokenType::RAW_STRING;
    if (name == "DASH")             return stml::TokenType::DASH;
    if (name == "INLINE_LIST")      return stml::TokenType::INLINE_LIST;
    if (name == "MULTILINE_STRING") return stml::TokenType::MULTILINE_STRING;
    throw std::runtime_error("Unknown token type: " + name);
}

// =========================================================================
// Module definition
// =========================================================================

PYBIND11_MODULE(_core, m) {
    m.doc() = "STML parser/serializer C++ core";

    // ---- ParseError exception ----
    // Register a Python ParseError (inherits RuntimeError).
    // C++ stml::ParseError is automatically translated with e.what()
    // formatted as "line:column: message".
    py::register_exception<stml::ParseError>(m, "ParseError", PyExc_RuntimeError);

    // ---- loads ----
    m.def("loads",
          [](const std::string& text) -> py::tuple {
              auto result = stml::loads(text);
              return py::make_tuple(
                  ast_to_python(result.ast),
                  warnings_to_python(result.warnings));
          },
          py::arg("text"),
          "Parse STML text -> (ast, warnings).\n\n"
          "Args:\n"
          "    text: STML source string.\n\n"
          "Returns:\n"
          "    (ast, warnings) tuple. ast is a dict with a 'docs' key.\n"
          "    warnings is a list of (message, line, column) tuples.");

    // ---- load ----
    m.def("load",
          [](const std::string& filename) -> py::tuple {
              auto result = stml::load(filename);
              return py::make_tuple(
                  ast_to_python(result.ast),
                  warnings_to_python(result.warnings));
          },
          py::arg("filename"),
          "Parse STML file -> (ast, warnings).\n\n"
          "Args:\n"
          "    filename: Path to an STML file.\n\n"
          "Returns:\n"
          "    (ast, warnings) tuple.");

    // ---- dumps ----
    m.def("dumps",
          [](const py::object& ast) -> std::string {
              return stml::dumps(python_to_ast(ast));
          },
          py::arg("ast"),
          "Serialize AST -> canonical STML text.\n\n"
          "Args:\n"
          "    ast: A dict/list/str/None tree representing an STML AST.\n"
          "    Should be wrapped as {'docs': [{...}, ...]}.\n\n"
          "Returns:\n"
          "    Canonical STML string (all keys double-quoted, 2-space indent).");

    // ---- to_json ----
    m.def("to_json",
          [](const py::object& ast) -> std::string {
              return stml::to_json(python_to_ast(ast));
          },
          py::arg("ast"),
          "Serialize AST -> JSON text.\n\n"
          "Args:\n"
          "    ast: A dict/list/str/None tree.\n\n"
          "Returns:\n"
          "    JSON string with 2-space indent.");

    // ---- tokenize (debug) ----
    m.def("tokenize",
          [](const std::string& text) -> py::tuple {
              auto [tokens, warn] = stml::tokenize(text);
              py::list token_list;
              for (const auto& t : tokens) {
                  py::dict td;
                  td["type"]   = token_type_name(t.type);
                  td["value"]  = token_value_to_python(t.value);
                  td["line"]   = t.line;
                  td["column"] = t.column;
                  token_list.append(std::move(td));
              }
              return py::make_tuple(std::move(token_list),
                                    warnings_to_python(warn));
          },
          py::arg("text"),
          "Lex STML text -> (tokens, warnings). Debug utility.\n\n"
          "Each token is a dict: {type: str, value: str|list|None, line: int, column: int}.");

    // ---- parse (debug) ----
    m.def("parse",
          [](const py::list& tokens) -> py::tuple {
              std::vector<stml::Token> cpp_tokens;
              for (auto item : tokens) {
                  py::dict td = item.cast<py::dict>();

                  stml::TokenType tt =
                      token_type_from_name(td["type"].cast<std::string>());

                  stml::TokenValue tv;
                  py::object val = td["value"];
                  if (!val.is_none()) {
                      if (py::isinstance<py::list>(val)) {
                          std::vector<stml::InlineElem> elems;
                          for (auto elem : val.cast<py::list>()) {
                              if (elem.is_none()) {
                                  elems.push_back(std::nullopt);
                              } else {
                                  elems.push_back(elem.cast<std::string>());
                              }
                          }
                          tv = std::move(elems);
                      } else {
                          tv = val.cast<std::string>();
                      }
                  }

                  cpp_tokens.emplace_back(tt, std::move(tv),
                                          td["line"].cast<int>(),
                                          td["column"].cast<int>());
              }

              auto [ast_list, warn] = stml::parse(cpp_tokens);

              // Wrap in {"docs": [...]} for consistent return format
              stml::AstNode wrapper(
                  stml::AstMap{{std::pair{"docs", stml::AstNode(std::move(ast_list))}}});
              return py::make_tuple(ast_to_python(wrapper),
                                    warnings_to_python(warn));
          },
          py::arg("tokens"),
          "Parse a token list -> (ast, warnings). Debug utility.\n\n"
          "The inverse of tokenize().");
}
