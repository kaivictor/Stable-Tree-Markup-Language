/// pybind11 bindings for the STML C++ library.
#include <string>
#include <utility>
#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "stml.h"

namespace py = pybind11;
using namespace stml;

// =========================================================================
// Python ↔ AstNode conversion
// =========================================================================

static AstNode py_to_ast(py::object obj) {
    if (obj.is_none()) {
        return AstNode();
    }
    if (py::isinstance<py::str>(obj)) {
        return AstNode(obj.cast<std::string>());
    }
    if (py::isinstance<py::list>(obj)) {
        AstList list;
        for (auto item : obj.cast<py::list>()) {
            list.push_back(py_to_ast(py::reinterpret_borrow<py::object>(item)));
        }
        return AstNode(std::move(list));
    }
    if (py::isinstance<py::dict>(obj)) {
        AstMap map;
        for (auto [k, v] : obj.cast<py::dict>()) {
            std::string key = k.cast<std::string>();
            map.emplace_back(std::move(key), py_to_ast(py::reinterpret_borrow<py::object>(v)));
        }
        return AstNode(std::move(map));
    }
    // Fallback: convert via string representation
    return AstNode(obj.cast<std::string>());
}

static py::object ast_to_py(const AstNode& node) {
    if (node.is_null()) {
        return py::none();
    }
    if (node.is_string()) {
        return py::str(*node.as_string());
    }
    if (node.is_list()) {
        py::list lst;
        for (const auto& item : *node.as_list()) {
            lst.append(ast_to_py(item));
        }
        return std::move(lst);
    }
    if (node.is_map()) {
        py::dict d;
        // Use list-of-tuples for insertion order (Python 3.7+ dict is ordered)
        for (const auto& [k, v] : *node.as_map()) {
            d[py::str(k)] = ast_to_py(v);
        }
        return std::move(d);
    }
    return py::none();
}

// =========================================================================
// Warning conversion
// =========================================================================

static const char* token_type_name(TokenType t) {
    switch (t) {
        case TokenType::NEWLINE:          return "NEWLINE";
        case TokenType::INDENT:           return "INDENT";
        case TokenType::DEDENT:           return "DEDENT";
        case TokenType::DOC_SEPARATOR:    return "DOC_SEPARATOR";
        case TokenType::END:              return "END";
        case TokenType::KEY:              return "KEY";
        case TokenType::BARE_KEY:         return "BARE_KEY";
        case TokenType::COLON:            return "COLON";
        case TokenType::SCALAR:           return "SCALAR";
        case TokenType::NULL_:            return "NULL_";
        case TokenType::RAW_STRING:       return "RAW_STRING";
        case TokenType::DASH:             return "DASH";
        case TokenType::INLINE_LIST:      return "INLINE_LIST";
        case TokenType::MULTILINE_STRING: return "MULTILINE_STRING";
    }
    return "UNKNOWN";
}

static py::dict warning_to_py(const Warning& w) {
    py::dict d;
    d["line"] = w.line;
    d["column"] = w.column;
    d["message"] = w.message;
    return d;
}

// =========================================================================
// Module definition
// =========================================================================

PYBIND11_MODULE(_core, m) {
    m.doc() = "STML (Stable Tree Markup Language) parser — C++ extension";

    // ---- loads ----
    m.def("loads", [](const std::string& text) -> py::object {
        try {
            auto result = stml::loads(text);
            py::list warnings;
            for (const auto& w : result.warnings) {
                warnings.append(warning_to_py(w));
            }
            return py::make_tuple(ast_to_py(result.ast), std::move(warnings));
        } catch (const ParseError& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
            throw py::error_already_set();
        }
    }, py::arg("text"),
       "Parse STML text → (ast, warnings).\n"
       "AST is {\"docs\": [{...}, ...]}.");

    // ---- load ----
    m.def("load", [](const std::string& filename) -> py::object {
        try {
            auto result = stml::load(filename);
            py::list warnings;
            for (const auto& w : result.warnings) {
                warnings.append(warning_to_py(w));
            }
            return py::make_tuple(ast_to_py(result.ast), std::move(warnings));
        } catch (const ParseError& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
            throw py::error_already_set();
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_IOError, e.what());
            throw py::error_already_set();
        }
    }, py::arg("filename"),
       "Read and parse an STML file → (ast, warnings).");

    // ---- dumps ----
    m.def("dumps", [](py::object ast) -> std::string {
        return stml::dumps(py_to_ast(ast));
    }, py::arg("ast"),
       "Convert AST → canonical STML text.");

    // ---- to_json ----
    m.def("to_json", [](py::object ast) -> std::string {
        return stml::to_json(py_to_ast(ast));
    }, py::arg("ast"),
       "Convert AST → JSON string.");

    // ---- tokenize (debug) ----
    m.def("tokenize", [](const std::string& text) -> py::object {
        auto [tokens, warnings] = stml::tokenize(text);
        py::list py_tokens;
        for (const auto& t : tokens) {
            py::dict td;
            td["type"] = static_cast<int>(t.type); // token type code
            td["type_name"] = token_type_name(t.type);
            td["line"] = t.line;
            td["column"] = t.column;
            if (std::holds_alternative<std::string>(t.value)) {
                td["value"] = std::get<std::string>(t.value);
            } else if (std::holds_alternative<std::vector<InlineElem>>(t.value)) {
                py::list elems;
                for (const auto& e : std::get<std::vector<InlineElem>>(t.value)) {
                    if (e.has_value()) {
                        elems.append(py::str(*e));
                    } else {
                        elems.append(py::none());
                    }
                }
                td["value"] = std::move(elems);
            } else {
                td["value"] = py::none();
            }
            py_tokens.append(std::move(td));
        }
        py::list py_warnings;
        for (const auto& w : warnings) {
            py_warnings.append(warning_to_py(w));
        }
        return py::make_tuple(std::move(py_tokens), std::move(py_warnings));
    }, py::arg("text"),
       "Lex only → (tokens, warnings). Debug utility.");

    // ---- stream_parse ----
    m.def("stream_parse", [](const std::string& text) -> py::object {
        auto result = stml::stream_parse(text);
        py::list warnings;
        for (const auto& w : result.warnings) {
            warnings.append(warning_to_py(w));
        }
        return py::make_tuple(ast_to_py(result.ast), std::move(warnings));
    }, py::arg("text"),
       "Streaming parse of STML text → (ast, warnings).");
}
