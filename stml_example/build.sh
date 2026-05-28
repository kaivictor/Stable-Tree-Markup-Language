#!/bin/bash
# Build examples for STML C++ library.
# Requires: stml_cpp/ already built (run ../stml_cpp/build.sh first).

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
STML_DIR="$SCRIPT_DIR/../stml_cpp"
BUILD_DIR="$SCRIPT_DIR/build"
STML_OBJ_DIR="$STML_DIR/build/obj"

# Compiler
CXX="${CXX:-g++}"
CXX_STD="-std=c++17"
CXX_FLAGS="-O2"
INCLUDES="-I$STML_DIR"

# Library objects from stml_cpp build
LIB_OBJS=(
    "$STML_OBJ_DIR/lexer.o"
    "$STML_OBJ_DIR/parser.o"
    "$STML_OBJ_DIR/ast.o"
    "$STML_OBJ_DIR/to_stml.o"
    "$STML_OBJ_DIR/to_json.o"
    "$STML_OBJ_DIR/recovery.o"
    "$STML_OBJ_DIR/stml.o"
)

# Check if library was built
if [ ! -f "${LIB_OBJS[0]}" ]; then
    echo "Library not built yet. Run ../stml_cpp/build.sh first."
    exit 1
fi

# Example programs
EXAMPLES=(
    basic_usage
    config_reader
    converter
    diagnostics
    ast_manipulation
)

mkdir -p "$BUILD_DIR"

echo "======================================"
echo "  STML Examples Build"
echo "======================================"

for exe in "${EXAMPLES[@]}"; do
    echo "  Building $exe..."
    $CXX $CXX_STD $CXX_FLAGS $INCLUDES \
        "${LIB_OBJS[@]}" \
        "$SCRIPT_DIR/$exe.cpp" \
        -o "$BUILD_DIR/$exe.exe"
done

echo ""
echo "Build complete!"
echo "Run examples:"
echo ""
echo "  cd build"
echo "  ./basic_usage.exe"
echo "  ./config_reader.exe ../config.stml"
echo "  ./converter.exe ../config.stml"
echo "  ./converter.exe ../config.stml --stml"
echo "  ./diagnostics.exe"
echo "  ./ast_manipulation.exe"
