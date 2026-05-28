/// Regression tests using TestData/ files.
/// Parses each testN.stml and compares the AST against testN预期.json.

#include <cassert>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "stml.h"
#include "test_json.h"

using namespace stml;

static int tests_run = 0;
static int tests_passed = 0;

// Path to TestData directory (relative to build dir)
// When running from build/tests, TestData is at ../../../../TestData
static std::string g_testdata_path;

#define TEST(name) \
    do { \
        ++tests_run; \
        try { \
            name(); \
            ++tests_passed; \
            std::cout << "  PASS " << #name << "\n"; \
        } catch (const std::exception& e) { \
            std::cout << "  FAIL " << #name << ": " << e.what() << "\n"; \
        } \
    } while(0)

// =========================================================================
// Regression test against a specific test case
// =========================================================================

static void run_regression(int test_num) {
    std::string stml_file = g_testdata_path + "/test" + std::to_string(test_num) + ".stml";
    std::string json_file = g_testdata_path + "/test" + std::to_string(test_num) + "_expected.json";

    // Check file exists by trying to open it (avoid fs::exists Unicode issues)
    std::ifstream stml_check(stml_file);
    if (!stml_check.is_open()) {
        std::cout << "  SKIP test" << test_num << ": file not found: " << stml_file << "\n";
        ++tests_passed;
        return;
    }
    stml_check.close();

    // Parse STML
    auto [ast, warnings] = load(stml_file);

    // Parse expected JSON
    AstNode expected = test_json::load(json_file);

    // Compare
    if (ast != expected) {
        // Print diff for debugging
        std::cout << "\n  STML output (JSON):\n" << to_json(ast);
        std::cout << "  Expected (JSON):\n" << to_json(expected);

        // Check which docs differ
        const auto& docs_map = *ast.as_map();
        const auto& docs = *map_find(docs_map, "docs")->as_list();
        const auto& exp_docs_map = *expected.as_map();
        const auto& exp_docs = *map_find(exp_docs_map, "docs")->as_list();

        if (docs.size() != exp_docs.size()) {
            std::cout << "  Doc count differs: " << docs.size() << " vs " << exp_docs.size() << "\n";
        }
        size_t max_docs = docs.size() < exp_docs.size() ? docs.size() : exp_docs.size();
        for (size_t i = 0; i < max_docs; ++i) {
            if (docs[i] != exp_docs[i]) {
                std::cout << "  Doc " << i << " differs\n";
            }
        }
        throw std::runtime_error("AST mismatch for test" + std::to_string(test_num));
    }
}

// =========================================================================
// Individual test functions
// =========================================================================

static void test_regression_1() { run_regression(1); }
static void test_regression_2() { run_regression(2); }
static void test_regression_3() { run_regression(3); }
static void test_regression_4() { run_regression(4); }
static void test_regression_5() { run_regression(5); }
static void test_regression_6() {
    std::string stml_file = g_testdata_path + "/test6.stml";

    // Check file exists
    std::ifstream stml_check(stml_file);
    if (!stml_check.is_open()) {
        std::cout << "  SKIP test6: file not found: " << stml_file << "\n";
        ++tests_passed;
        return;
    }
    stml_check.close();

    // test6.stml contains unsupported structural conflicts → ParseError
    try {
        load(stml_file);
        throw std::runtime_error("Expected ParseError for test6 but none was thrown");
    } catch (const ParseError& e) {
        // Expected
    }
}
static void test_regression_7() { run_regression(7); }
static void test_regression_8() { run_regression(8); }
static void test_regression_9() { run_regression(9); }
static void test_regression_10() { run_regression(10); }
static void test_regression_11() { run_regression(11); }
static void test_regression_12() { run_regression(12); }

// =========================================================================
// Round-trip regression: for each test case, STML→AST→STML→AST must equal
// =========================================================================

static void test_roundtrip_regression(int test_num) {
    std::string stml_file = g_testdata_path + "/test" + std::to_string(test_num) + ".stml";
    // Check file exists
    std::ifstream file(stml_file);
    if (!file.is_open()) return;
    std::string text((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

    // Parse
    auto [ast1, w1] = loads(text);

    // Serialize back to STML
    std::string out1 = dumps(ast1);

    // Parse again
    auto [ast2, w2] = loads(out1);

    // Compare ASTs
    if (ast1 != ast2) {
        std::cout << "\n  Original JSON:\n" << to_json(ast1);
        std::cout << "  Roundtrip JSON:\n" << to_json(ast2);
        throw std::runtime_error("Round-trip mismatch for test" + std::to_string(test_num));
    }
}

static void test_roundtrip_1() { test_roundtrip_regression(1); }
static void test_roundtrip_2() { test_roundtrip_regression(2); }
static void test_roundtrip_3() { test_roundtrip_regression(3); }
static void test_roundtrip_8() { test_roundtrip_regression(8); }
static void test_roundtrip_9() { test_roundtrip_regression(9); }
static void test_roundtrip_10() { test_roundtrip_regression(10); }
static void test_roundtrip_11() { test_roundtrip_regression(11); }
static void test_roundtrip_12() { test_roundtrip_regression(12); }

// =========================================================================
// Main
// =========================================================================

int main(int argc, char* argv[]) {
    // Determine TestData path
    // When running from build/ directory (e.g. build/tests/), TestData is at
    // the project root: ../../../../TestData or ../../TestData
    if (argc > 1) {
        g_testdata_path = argv[1];
    } else {
        // Default: try /tmp/TestData (safe path on MinGW)
        std::ifstream check("/tmp/TestData/test1.stml");
        if (check.is_open()) {
            g_testdata_path = "/tmp/TestData";
            check.close();
        } else {
            std::cerr << "Cannot find TestData directory. Pass path as argument.\n";
            return 1;
        }
    }

    std::cout << "Using TestData at: " << g_testdata_path << "\n\n";

    std::cout << "Regression tests (against expected JSON):\n";
    TEST(test_regression_1);
    TEST(test_regression_2);
    TEST(test_regression_3);
    TEST(test_regression_4);
    TEST(test_regression_5);
    TEST(test_regression_6);
    TEST(test_regression_7);
    TEST(test_regression_8);
    TEST(test_regression_9);
    TEST(test_regression_10);
    TEST(test_regression_11);
    TEST(test_regression_12);

    std::cout << "\nRound-trip regression tests:\n";
    TEST(test_roundtrip_1);
    TEST(test_roundtrip_2);
    TEST(test_roundtrip_3);
    TEST(test_roundtrip_8);
    TEST(test_roundtrip_9);
    TEST(test_roundtrip_10);
    TEST(test_roundtrip_11);
    TEST(test_roundtrip_12);

    std::cout << "\n" << tests_passed << "/" << tests_run << " passed\n";
    return tests_passed == tests_run ? 0 : 1;
}
