"""STML Serializer: AST → canonical STML text.

Directly migrated from JSON2STML.py to guarantee character-level
compatibility with `output.stml`.  All output rules follow §15.

C++ compatible: no generators, explicit types, list accumulation.
"""

import json
from typing import Any, Dict, List


def escape_string(s: str) -> str:
    """Escape double-quote, backslash, newline, tab."""
    s = s.replace('\\', '\\\\')
    s = s.replace('"', '\\"')
    s = s.replace('\n', '\\n')
    s = s.replace('\t', '\\t')
    return s


def quote_key(key: str) -> str:
    """Keys are always double-quoted."""
    return '"' + escape_string(key) + '"'


def format_scalar(value: Any) -> str:
    """Scalar value → STML representation.

    null → unquoted 'null'
    bool / int / float → double-quoted string
    str → double-quoted with escapes
    """
    if value is None:
        return 'null'
    if isinstance(value, bool):
        return '"' + ('true' if value else 'false') + '"'
    if isinstance(value, (int, float)):
        return '"' + str(value) + '"'
    if isinstance(value, str):
        return '"' + escape_string(value) + '"'
    raise TypeError(f"Unsupported type: {type(value)}")


def serialize_node(node: Any, indent_level: int = 0) -> str:
    """Convert an AST node to a (possibly multi-line) STML fragment.
    Indentation: 2 spaces per level.
    """
    indent = '  ' * indent_level
    next_indent = '  ' * (indent_level + 1)

    # 1. Null / scalar
    if node is None or isinstance(node, (bool, int, float, str)):
        return indent + format_scalar(node)

    def _values_are_all_scalars(lst: List[Any]) -> bool:
        """Check if every element in *lst* is a simple scalar (str, None, bool, int, float)."""
        for v in lst:
            if isinstance(v, (dict, list)):
                return False
        return True

    def _format_inline_list(lst: List[Any]) -> str:
        """Format a list of scalars as an inline list: ["v1", "v2", "v3"]."""
        parts: List[str] = []
        for v in lst:
            parts.append(format_scalar(v))
        return '[' + ', '.join(parts) + ']'

    # 2. List (sequence)
    if isinstance(node, list):
        if not node:
            return indent + '- null'
        lines: List[str] = []
        for item in node:
            if isinstance(item, (type(None), bool, int, float, str)):
                # Scalar entry: - "value"
                lines.append(indent + '- ' + format_scalar(item))
            elif isinstance(item, list):
                # Nested list item
                if not item:
                    lines.append(indent + '- null')
                elif _values_are_all_scalars(item):
                    # Inline list: - [v1, v2, v3]
                    lines.append(indent + '- ' + _format_inline_list(item))
                else:
                    # Complex nested list: sub-block
                    lines.append(indent + '- ')
                    lines.append(serialize_node(item, indent_level + 1))
            elif isinstance(item, dict):
                if not item:
                    lines.append(indent + '- null')
                    continue
                if len(item) > 1:
                    raise ValueError(
                        "SYML list entries do not support multi-key dicts. "
                        "Use single-key mapping wrapper: " + str(item)
                    )
                key, val = next(iter(item.items()))
                if isinstance(val, (dict, list)):
                    # Compound value: - "key":
                    lines.append(indent + '- ' + quote_key(key) + ':')
                    sub = serialize_node(val, indent_level + 1)
                    lines.append(sub)
                else:
                    # Scalar value: - "key": "value"
                    lines.append(
                        indent + '- ' + quote_key(key) + ': ' + format_scalar(val)
                    )
            else:
                raise TypeError(f"List contains unsupported type: {type(item)}")
        return '\n'.join(lines)

    # 3. Dict (mapping)
    if isinstance(node, dict):
        if not node:
            return indent + 'null'
        lines: List[str] = []
        for key, val in node.items():
            k_str = quote_key(key)
            if isinstance(val, (dict, list)):
                # Compound value: newline + indent
                sub = serialize_node(val, indent_level + 1)
                if sub.strip() == '':
                    lines.append(indent + k_str + ': null')
                else:
                    lines.append(indent + k_str + ':')
                    lines.append(sub)
            else:
                # Scalar value: same line
                lines.append(indent + k_str + ': ' + format_scalar(val))
        return '\n'.join(lines)

    raise TypeError(f"Unsupported node type: {type(node)}")


def dumps(obj: Dict[str, Any]) -> str:
    """Convert a docs-wrapped AST to canonical STML text.

    Format: {"docs": {"doc1": ..., "doc2": ...}}

    Multiple documents are separated by '---' on its own line.
    """
    if isinstance(obj, dict) and 'docs' in obj and isinstance(obj['docs'], dict):
        docs = obj['docs']
        # Sort keys numerically (doc1, doc2, ...)
        sorted_keys = sorted(docs.keys(), key=lambda x: int(x.replace('doc', '')))
        parts: List[str] = []
        for k in sorted_keys:
            parts.append(serialize_node(docs[k], indent_level=0))
        return '\n---\n'.join(parts) + '\n'
    else:
        # Unwrapped single document
        return serialize_node(obj, indent_level=0) + '\n'


def json_to_stml(json_str: str) -> str:
    """JSON string → STML text."""
    obj = json.loads(json_str)
    return dumps(obj)
