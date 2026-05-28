"""STML Token type definitions.

Maps to C++: enum class + struct Token, struct Warning, class ParseError.
"""

from enum import Enum, auto
from dataclasses import dataclass, field
from typing import Any


class TokenType(Enum):
    """Token types for the STML lexer."""
    # Structural
    NEWLINE = auto()
    INDENT = auto()
    DEDENT = auto()
    DOC_SEPARATOR = auto()  # indent=0 and content='---'
    EOF = auto()

    # Mapping
    KEY = auto()             # Normal key (already unescaped)
    BARE_KEY = auto()        # Line without structural colon; value defaults to null
    COLON = auto()

    # Values
    SCALAR = auto()          # Scalar string (already unescaped)
    NULL = auto()            # null / ~ / empty container
    RAW_STRING = auto()      # Degraded raw string (unclosed quote, etc.)

    # Sequence
    DASH = auto()            # Sequence entry prefix: '- '

    # Special
    INLINE_LIST = auto()     # Inline list (value = already parsed Python list)
    MULTILINE_STRING = auto()  # Multiline text (value = raw string content)


@dataclass
class Token:
    """A single lexical token with source location.

    Maps to C++: struct Token { TokenType type; std::string value; int line; int column; };
    """
    type: TokenType
    value: Any = None
    line: int = 0            # 1-based line number
    column: int = 0          # 1-based column (token start position on line)


@dataclass
class Warning:
    """Non-fatal warning during parsing. The AST is still well-formed."""
    line: int                # 1-based
    column: int              # 1-based
    message: str             # Chinese warning description


class ParseError(Exception):
    """Fatal error during parsing. Cannot recover AST."""
    def __init__(self, line: int, column: int, message: str):
        self.line = line
        self.column = column
        self.message = message
        super().__init__(f"{line}:{column}: {message}")
