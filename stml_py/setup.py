"""Build script for stml Python extension (pybind11 + MinGW)."""
import sys
import os
from setuptools import setup, find_packages, Extension

STML_CPP = os.path.join(os.path.dirname(__file__), '..', 'stml_cpp')
PYBIND11_INC = os.path.join(sys.prefix, 'Lib', 'site-packages', 'pybind11', 'include')

ext_modules = [
    Extension(
        'stml._core',
        ['src/bindings.cpp'],
        include_dirs=[
            os.path.join(STML_CPP),
            PYBIND11_INC,
        ],
        extra_objects=[
            os.path.join(STML_CPP, 'build', 'libstml.a'),
        ],
        extra_compile_args=['-std=c++17', '-O2', '-Wall'],
        extra_link_args=[
            '-static-libgcc',
            '-static-libstdc++',
            '-Wl,-Bstatic',
            '-lwinpthread',
            '-Wl,-Bdynamic',
        ],
        language='c++',
    ),
]

setup(
    name='stml',
    version='0.14.0',
    description='STML — Stable Tree Markup Language parser',
    packages=find_packages('src'),
    package_dir={'': 'src'},
    ext_modules=ext_modules,
    python_requires='>=3.10',
)
