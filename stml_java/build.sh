#!/bin/bash
# Build script for STML Java library
# Usage: ./build.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
SRC_DIR="$SCRIPT_DIR/src/main/java"
TEST_DIR="$SCRIPT_DIR/src/test/java"
TESTDATA_DIR="$SCRIPT_DIR/../TestData"

JAVA_HOME="${JAVA_HOME:-E:/SoftWares/DevKits/Java/jdk-17}"
JAVAC="$JAVA_HOME/bin/javac"
JAVA="$JAVA_HOME/bin/java"

echo "======================================"
echo "  STML Java Library Build"
echo "======================================"
echo "JAVA_HOME: $JAVA_HOME"

mkdir -p "$BUILD_DIR"

# Source files
LIBSOURCES=(
    TokenType.java
    Warning.java
    ParseError.java
    InlineElem.java
    Token.java
    AstNode.java
    Line.java
    STMLLexer.java
    LineTreeBuilder.java
    AstBuilder.java
    Serializer.java
    STML.java
    STMLStreamer.java
)

# Compile library sources
echo ""
echo "Compiling library sources..."
for src in "${LIBSOURCES[@]}"; do
    echo "  $src"
done

"$JAVAC" -d "$BUILD_DIR" \
    $(for s in "${LIBSOURCES[@]}"; do echo "$SRC_DIR/stml/$s"; done)

# Compile test sources
echo ""
echo "Compiling test sources..."

# TestJson first
"$JAVAC" -d "$BUILD_DIR" -cp "$BUILD_DIR" "$TEST_DIR/stml/TestJson.java"

# Test executables
TEST_SOURCES=(
    TestLexer.java
    TestParser.java
    TestSerializer.java
    TestRegression.java
    TestStreaming.java
    TestFullRoundtrip.java
)

for test_src in "${TEST_SOURCES[@]}"; do
    echo "  $test_src"
    "$JAVAC" -d "$BUILD_DIR" -cp "$BUILD_DIR" "$TEST_DIR/stml/$test_src"
done

echo ""
echo "======================================"
echo "  Build complete"
echo "======================================"
echo ""
echo "Library: $BUILD_DIR/stml/"
echo "Tests:"
for test_src in "${TEST_SOURCES[@]}"; do
    test_name="${test_src%.java}"
    echo "  - $test_name"
done

echo ""
echo "Run tests:"
echo "  cd $BUILD_DIR && $JAVA -cp . stml.TestLexer && $JAVA -cp . stml.TestParser && $JAVA -cp . stml.TestSerializer"
echo "  Regression tests need TestData path:"
echo "  cd $BUILD_DIR && $JAVA -cp . stml.TestRegression $TESTDATA_DIR"