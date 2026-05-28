"""STML Python bindings — build configuration.

Compiles the C++ stml_cpp library sources together with the pybind11
binding code (_core.cpp) into a single Python extension module (stml._core).
"""

from setuptools import setup, find_packages
from pybind11.setup_helpers import Pybind11Extension, build_ext

import os
import sys

# ---- Source files (relative to setup.py, forward slashes) ----
_stml_sources = [
    "../stml_cpp/lexer/lexer.cpp",
    "../stml_cpp/lexer/streaming_lexer.cpp",
    "../stml_cpp/parser/parser.cpp",
    "../stml_cpp/parser/streaming_parser.cpp",
    "../stml_cpp/ast/ast.cpp",
    "../stml_cpp/serializer/to_stml.cpp",
    "../stml_cpp/serializer/to_json.cpp",
    "../stml_cpp/diagnostics/recovery.cpp",
    "../stml_cpp/stml.cpp",
    "../stml_cpp/stml_stream.cpp",
    "src/stml/_core.cpp",
]

# ---- Detect compiler: MSVC vs MinGW ----
_using_mingw = False
_cxx = os.environ.get("CXX", "")
if "mingw" in _cxx.lower() or "g++" in _cxx or "gcc" in _cxx:
    _using_mingw = True

extra_compile_args = []
extra_link_args = []

if sys.platform == "win32":
    if _using_mingw:
        extra_compile_args = ["-std=c++17", "-O2", "-Wall"]
        extra_link_args = ["-static-libgcc", "-static-libstdc++", "-static"]

_cxx_std = None if _using_mingw else 17

_ext = Pybind11Extension(
    "stml._core",
    _stml_sources,
    include_dirs=["../stml_cpp"],
    cxx_std=_cxx_std,
    extra_compile_args=extra_compile_args,
    extra_link_args=extra_link_args,
)

# ---- Strip MSVC-style flags when using MinGW ----
if _using_mingw:
    _ext.extra_compile_args = [
        f for f in _ext.extra_compile_args
        if not f.startswith("/")
    ]
    # Also strip from Extension._extra_compile_args (pybind11 internal)
    # pybind11 sometimes adds /std:c++latest on Windows
    if hasattr(_ext, '_extra_compile_args'):
        _ext._extra_compile_args = [
            f for f in _ext._extra_compile_args
            if not f.startswith("/")
        ]

setup(
    ext_modules=[_ext],
    cmdclass={"build_ext": build_ext},
    packages=find_packages(where="src"),
    package_dir={"": "src"},
)
