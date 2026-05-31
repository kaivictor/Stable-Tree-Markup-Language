#!/bin/bash
# Build script for STML C++ library (MinGW / MSYS2)
# Usage: ./build.sh [release|debug]

set -e

BUILD_MODE="${1:-release}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
OBJ_DIR="$BUILD_DIR/obj"

CXX="${CXX:-g++}"
CXX_STD="-std=c++17"
if [ "$BUILD_MODE" = "debug" ]; then
    CXX_FLAGS="-g -O0 -DDEBUG"
else
    CXX_FLAGS="-O2 -DNDEBUG"
fi
INCLUDES="-I$SCRIPT_DIR"

LIB_SOURCES=(
    lexer/lexer.cpp
<<<<<<< Updated upstream
    parser/parser.cpp
    parser/line_tree_builder.cpp
    parser/ast_builder.cpp
=======
<<<<<<< Updated upstream
    parser/parser.cpp
    parser/line_tree_builder.cpp
    parser/ast_builder.cpp
=======
    parser/line_tree_builder.cpp
    parser/ast_builder.cpp
    parser/parser.cpp
>>>>>>> Stashed changes
>>>>>>> Stashed changes
    ast/ast.cpp
    serializer/to_stml.cpp
    serializer/to_json.cpp
    stml.cpp
    stml_stream.cpp
)

TEST_SOURCES=(
    tests/test_lexer.cpp
    tests/test_parser.cpp
    tests/test_serializer.cpp
    tests/test_regression.cpp
    tests/test_streaming.cpp
    tests/test_full_roundtrip.cpp
)

<<<<<<< Updated upstream
=======
<<<<<<< Updated upstream
>>>>>>> Stashed changes
# Tests that need test_json.cpp linked
NEED_JSON="test_regression test_streaming test_full_roundtrip"

=======
>>>>>>> Stashed changes
echo "======================================"
echo "  STML C++ Library Build"
echo "  Mode: $BUILD_MODE"
echo "======================================"

mkdir -p "$OBJ_DIR"

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

ar rcs "$BUILD_DIR/libstml.a" "${OBJECTS[@]}"
echo "  -> $BUILD_DIR/libstml.a"

<<<<<<< Updated upstream
# --- Build tests ---
echo ""
echo "Building tests..."

# Compile test_json helper first (if it exists)
<<<<<<< Updated upstream
=======
=======
# ---- Build test_json helper ----
>>>>>>> Stashed changes
>>>>>>> Stashed changes
JSON_OBJ="$OBJ_DIR/test_json.o"
if [ -f "$SCRIPT_DIR/tests/test_json.cpp" ]; then
    $CXX $CXX_STD $CXX_FLAGS $INCLUDES -c "$SCRIPT_DIR/tests/test_json.cpp" -o "$JSON_OBJ"
    echo "  Compiled test_json.cpp"
else
    # Create an empty object for linking
    echo "  test_json.cpp not found, creating stub"
    echo "namespace stml { namespace test_json { } }" > "$OBJ_DIR/stub_test_json.cpp"
    $CXX $CXX_STD $CXX_FLAGS $INCLUDES -c "$OBJ_DIR/stub_test_json.cpp" -o "$JSON_OBJ"
fi

<<<<<<< Updated upstream
=======
<<<<<<< Updated upstream
>>>>>>> Stashed changes
for test_src in "${TEST_SOURCES[@]}"; do
    test_name="$(basename "$test_src" .cpp)"
    test_exe="$BUILD_DIR/$test_name.exe"

    if [ ! -f "$SCRIPT_DIR/$test_src" ]; then
        echo "  Skipping $test_name (source not found)"
        continue
<<<<<<< Updated upstream
    fi

    echo "  Building $test_name"

    if echo "$NEED_JSON" | grep -qw "$test_name"; then
        $CXX $CXX_STD $CXX_FLAGS $INCLUDES \
            "${OBJECTS[@]}" "$JSON_OBJ" \
            "$SCRIPT_DIR/$test_src" -o "$test_exe"
    else
        $CXX $CXX_STD $CXX_FLAGS $INCLUDES \
            "${OBJECTS[@]}" \
            "$SCRIPT_DIR/$test_src" -o "$test_exe"
=======
>>>>>>> Stashed changes
    fi

=======
echo ""
echo "Building tests..."
for test_src in "${TEST_SOURCES[@]}"; do
    test_name="$(basename "$test_src" .cpp)"
    test_exe="$BUILD_DIR/$test_name.exe"
>>>>>>> Stashed changes
    echo "  Building $test_name"
    $CXX $CXX_STD $CXX_FLAGS $INCLUDES \
        "${OBJECTS[@]}" "$JSON_OBJ" \
        "$SCRIPT_DIR/$test_src" -o "$test_exe"
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
