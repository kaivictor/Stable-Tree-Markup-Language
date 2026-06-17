@echo off
REM Build script for STML Java library (Windows)
REM Usage: build.bat

set "JAVA_HOME=E:/SoftWares/DevKits/Java/jdk-17"
set "JAVAC=%JAVA_HOME%/bin/javac.exe"
set "JAVA=%JAVA_HOME%/bin/java.exe"
set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%build"
set "SRC_DIR=%SCRIPT_DIR%src/main/java/stml"
set "TEST_DIR=%SCRIPT_DIR%src/test/java/stml"
set "TESTDATA=%SCRIPT_DIR%../TestData"

echo ======================================
echo   STML Java Library Build
echo ======================================
echo JAVA_HOME: %JAVA_HOME%

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo.
echo Compiling library sources...
"%JAVAC%" -encoding UTF-8 -d "%BUILD_DIR%" "%SRC_DIR%/TokenType.java" "%SRC_DIR%/Warning.java" "%SRC_DIR%/ParseError.java" "%SRC_DIR%/InlineElem.java" "%SRC_DIR%/Token.java" "%SRC_DIR%/AstNode.java" "%SRC_DIR%/Line.java" "%SRC_DIR%/STMLLexer.java" "%SRC_DIR%/LineTreeBuilder.java" "%SRC_DIR%/AstBuilder.java" "%SRC_DIR%/Serializer.java" "%SRC_DIR%/STML.java" "%SRC_DIR%/STMLStreamer.java"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo Compiling test sources...
"%JAVAC%" -encoding UTF-8 -d "%BUILD_DIR%" -cp "%BUILD_DIR%" "%TEST_DIR%/TestJson.java" "%TEST_DIR%/TestLexer.java" "%TEST_DIR%/TestParser.java" "%TEST_DIR%/TestSerializer.java" "%TEST_DIR%/TestRegression.java" "%TEST_DIR%/TestStreaming.java" "%TEST_DIR%/TestFullRoundtrip.java"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo ======================================
echo   Build complete
echo ======================================

echo.
echo === Running Lexer Tests ===
"%JAVA%" -cp "%BUILD_DIR%" stml.TestLexer
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo === Running Parser Tests ===
"%JAVA%" -cp "%BUILD_DIR%" stml.TestParser
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo === Running Serializer Tests ===
"%JAVA%" -cp "%BUILD_DIR%" stml.TestSerializer
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo === Running Regression Tests ===
"%JAVA%" -cp "%BUILD_DIR%" stml.TestRegression "%TESTDATA%"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo === Running Full Roundtrip Tests ===
"%JAVA%" -cp "%BUILD_DIR%" stml.TestFullRoundtrip "%TESTDATA%"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo === Running Streaming Tests ===
"%JAVA%" -cp "%BUILD_DIR%" stml.TestStreaming "%TESTDATA%"

echo.
echo ======================================
echo   All tests completed
echo ======================================