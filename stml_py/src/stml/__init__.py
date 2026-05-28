"""STML — Stable Tree Markup Language

A visually YAML-like plain-text data format.  This package provides
a Python interface to the C++ STML parser and serializer.

Public API
----------
  loads(text)         → (ast, warnings)       parse STML string
  load(filename)      → (ast, warnings)       parse STML file
  dumps(ast)          → str                   AST → canonical STML text
  to_json(ast)        → str                   AST → JSON text
  tokenize(text)      → (tokens, warnings)    lex only (debug)
  parse(tokens)       → (ast, warnings)       parse tokens (debug)

Types
-----
  Warning:     dataclass with .message, .line, .column
  ParseError:  exception with .line, .column, .message (inherits RuntimeError)
"""

from stml._core import (
    loads as _loads,
    load as _load,
    dumps as _dumps,
    to_json as _to_json,
    tokenize as _tokenize,
    parse as _parse,
    ParseError as _ParseError,
)

from dataclasses import dataclass
from typing import Any, Dict, List, Tuple, Union


# =========================================================================
# ParseError — Python-side wrapper with structured fields
# =========================================================================

class ParseError(RuntimeError):
    """Fatal structural conflict detected during STML parsing.

    Attributes:
        line:   1-based line number where the error occurred.
        column: 1-based column number.
        message: Human-readable error description.
    """

    def __init__(self, line: int, column: int, message: str):
        super().__init__(f"{line}:{column}: {message}")
        self.line = line
        self.column = column
        self.message = message


def _parse_cpp_error(raw_error: _ParseError) -> ParseError:
    """Convert C++ ParseError (with "line:col: msg" string) to Python ParseError."""
    msg = str(raw_error)
    col1 = msg.find(":")
    col2 = msg.find(":", col1 + 1)
    if col1 != -1 and col2 != -1:
        line = int(msg[:col1])
        column = int(msg[col1 + 1:col2])
        message = msg[col2 + 1:].lstrip()
        return ParseError(line, column, message)
    # Fallback: couldn't parse structured fields
    return ParseError(0, 0, msg)


# =========================================================================
# Public API wrappers
# =========================================================================

@dataclass(frozen=True)
class Warning:
    """Non-fatal diagnostic produced during parsing.

    The AST is always well-formed; warnings describe potential user
    mistakes such as unclosed quotes, malformed inline lists, or
    ambiguous indentation.
    """
    message: str
    line: int        # 1-based
    column: int      # 1-based


def _wrap_warnings(raw_warnings):
    """Convert (message, line, column) tuples from C++ into Warning objects."""
    return [Warning(msg, line, col) for msg, line, col in raw_warnings]


def loads(text: str) -> Tuple[Dict[str, Any], List[Warning]]:
    """Parse STML text → (ast, warnings).

    Args:
        text: STML source string.

    Returns:
        (ast, warnings) tuple.
        ast is a dict with a ``"docs"`` key containing a list of document(s).
        warnings is a list of :class:`Warning` objects.

    Raises:
        ParseError: If the text contains a fatal structural conflict.
    """
    try:
        ast, raw = _loads(text)
    except _ParseError as e:
        raise _parse_cpp_error(e) from None
    return ast, _wrap_warnings(raw)


def load(path: str) -> Tuple[Dict[str, Any], List[Warning]]:
    """Parse an STML file → (ast, warnings).

    Args:
        path: Path to an .stml file.

    Returns:
        (ast, warnings) tuple.  See :func:`loads` for details.

    Raises:
        RuntimeError: If the file cannot be opened.
        ParseError: If the file contains a fatal structural conflict.
    """
    try:
        ast, raw = _load(path)
    except _ParseError as e:
        raise _parse_cpp_error(e) from None
    return ast, _wrap_warnings(raw)


def dumps(ast):
    """Serialize an AST to canonical STML text.

    Args:
        ast: A dict/list/str/None tree.  Typically wrapped as
            ``{"docs": [{...}, ...]}``.

    Returns:
        Canonical STML string (all keys double-quoted, 2-space indent).
    """
    return _dumps(ast)


def to_json(ast):
    """Serialize an AST to JSON text.

    Args:
        ast: A dict/list/str/None tree.

    Returns:
        JSON string with 2-space indent.
    """
    return _to_json(ast)


def tokenize(text: str) -> Tuple[List[Dict], List[Warning]]:
    """Lex STML text → (tokens, warnings).  Debug utility.

    Args:
        text: STML source string.

    Returns:
        (tokens, warnings) tuple.
        Each token is a dict ``{type, value, line, column}``.
    """
    tokens, raw = _tokenize(text)
    return tokens, _wrap_warnings(raw)


def parse(tokens):
    """Parse a token list → (ast, warnings).  Debug utility.

    The inverse of :func:`tokenize`.

    Args:
        tokens: A list of token dicts as returned by :func:`tokenize`.

    Returns:
        (ast, warnings) tuple.
    """
    try:
        ast, raw = _parse(tokens)
    except _ParseError as e:
        raise _parse_cpp_error(e) from None
    return ast, _wrap_warnings(raw)
