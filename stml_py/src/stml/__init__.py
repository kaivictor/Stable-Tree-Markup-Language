"""STML — Stable Tree Markup Language Python API.

Usage:
    import stml
    ast, warnings = stml.loads(text)
    ast, warnings = stml.load(filename)
    text = stml.dumps(ast)
    json_str = stml.to_json(ast)
"""

import os
import sys

if sys.platform == 'win32':
    # Ensure MinGW runtime DLLs are discoverable.
    # Use the package directory (where DLLs are copied during build).
    _pkg_dir = os.path.dirname(os.path.abspath(__file__))
    if hasattr(os, 'add_dll_directory'):
        os.add_dll_directory(_pkg_dir)

from stml._core import loads, load, dumps, to_json, tokenize, stream_parse  # noqa: E402

__all__ = [
    'loads',
    'load',
    'dumps',
    'to_json',
    'tokenize',
    'stream_parse',
]
