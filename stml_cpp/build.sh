#!/bin/bash
# Build script for STML C++ library (MinGW / MSYS2)
# Usage: ./build.sh [release|debug]
# The library has ZERO external dependencies.

set -e

BUILD_MODE="${1:-release}"

# Paths
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
OBJ_DIR="$BUILD_DIR/obj"

# Compiler flags
CXX="${CXX:-g++}"
CXX_STD="-std=c++17"
if [ "$BUILD_MODE" = "debug" ]; then
    CXX_FLAGS="-g -O0 -DDEBUG"
else
    CXX_FLAGS="-O2 -DNDEBUG"
fi
INCLUDES="-I$SCRIPT_DIR"

# Source files for the library (relative to SCRIPT_DIR)
LIB_SOURCES=(
    lexer/lexer.cpp
    parser/parser.cpp
    ast/ast.cpp
    serializer/to_stml.cpp
    serializer/to_json.cpp
    stml.cpp
    stml_stream.cpp
)

# Test source files
TEST_SOURCES=(
    tests/test_lexer.cpp
    tests/test_parser.cpp
    tests/test_serializer.cpp
    tests/test_regression.cpp
    tests/test_streaming.cpp
    tests/test_full_roundtrip.cpp
)

# Supported tests that need test_json.cpp
NEED_JSON="test_regression test_streaming test_full_roundtrip"

echo "======================================"
echo "  STML C++ Library Build"
echo "  Mode: $BUILD_MODE"
echo "======================================"

mkdir -p "$OBJ_DIR"

# --- Build library objects ---
echo ""
echo "Building library objects..."
OBJECTS=()
for src in "${LIB_SOURCES[@]}"; do
    obj_name="$(basename "$src" .cpp).o"
    obj_path="$OBJ_DIR/$obj_name"
    echo "  Compiling $src"
    $CXX $CXX_STD $CXX_FLAGS $INCLUDES -c "$SCRIPT_DIR/$src" -o "$obj_path"
    OBJECTS+=("$obj_path")
done

# Generate a static library (optional)
echo ""
echo "Creating static library..."
ar rcs "$BUILD_DIR/libstml.a" "${OBJECTS[@]}"
echo "  -> $BUILD_DIR/libstml.a"

# --- Build tests ---
echo ""
echo "Building tests..."

# Compile test_json helper first
JSON_OBJ="$OBJ_DIR/test_json.o"
$CXX $CXX_STD $CXX_FLAGS $INCLUDES -c "$SCRIPT_DIR/tests/test_json.cpp" -o "$JSON_OBJ"

ALL_TESTS_PASSED=0
TOTAL_TESTS=0
for test_src in "${TEST_SOURCES[@]}"; do
    test_name="$(basename "$test_src" .cpp)"
    test_exe="$BUILD_DIR/$test_name.exe"

    echo "  Building $test_name"

    if echo "$NEED_JSON" | grep -qw "$test_name"; then
        $CXX $CXX_STD $CXX_FLAGS $INCLUDES \
            "${OBJECTS[@]}" "$JSON_OBJ" \
            "$SCRIPT_DIR/$test_src" -o "$test_exe"
    else
        $CXX $CXX_STD $CXX_FLAGS $INCLUDES \
            "${OBJECTS[@]}" \
            "$SCRIPT_DIR/$test_src" -o "$test_exe"
    fi
done

echo ""
echo "======================================"
echo "  Build complete"
echo "======================================"
echo ""
echo "Library:  $BUILD_DIR/libstml.a"
echo "Tests:    $BUILD_DIR/"
for test_src in "${TEST_SOURCES[@]}"; do
    test_name="$(basename "$test_src" .cpp)"
    echo "  - $test_name.exe"
done

echo ""
echo "Run tests with: cd $BUILD_DIR && ./test_lexer.exe && ./test_parser.exe && ./test_serializer.exe"
echo "Regression tests need TestData path: ./test_regression.exe /path/to/TestData"
