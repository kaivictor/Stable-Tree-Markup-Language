"""STML – Stable Tree Markup Language parser.

Public API
----------
loads(text)        → (AST, warnings)       # parse STML string
load(filename)     → (AST, warnings)       # parse STML file
dumps(ast)         → str                   # AST → canonical STML
json_to_stml(json) → str                   # JSON string → STML

Types
-----
TokenType, Token, Warning, ParseError
STMLLexer, STMLParser
"""

from .token_types import Token, TokenType, Warning, ParseError
from .lexer import STMLLexer
from .parser import STMLParser
from .serializer import dumps, json_to_stml


def loads(text: str) -> tuple:
    """Parse STML text → (AST, warnings).

    AST is always {"docs": {"doc1": ..., ...}} even for single-document files.
    """
    lexer = STMLLexer(text)
    tokens, lex_warnings = lexer.tokenize()
    ast, parse_warnings = STMLParser(tokens).parse()
    return ast, lex_warnings + parse_warnings


def load(filename: str) -> tuple:
    """Read and parse an STML file → (AST, warnings)."""
    with open(filename, 'r', encoding='utf-8') as f:
        return loads(f.read())


__all__ = [
    'loads',
    'load',
    'dumps',
    'json_to_stml',
    'Token',
    'TokenType',
    'Warning',
    'ParseError',
    'STMLLexer',
    'STMLParser',
]
