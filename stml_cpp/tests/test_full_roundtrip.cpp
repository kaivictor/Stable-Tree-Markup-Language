/// Full round-trip chain tests.
///
/// Verifies the complete data transformation pipelines:
///
///   Chain A:  STML → AST → JSON  ≟  expected JSON      (parse correctness)
///   Chain B:  JSON → AST → STML → AST → JSON  ≟  expected JSON  (full roundtrip)
///
/// JSON comparison is semantic (via JSON→AST) to ignore key-order / whitespace diffs.

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
// Helpers
// =========================================================================

/// Read the whole file into a string.
static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) throw std::runtime_error("Cannot open: " + path);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

/// Strip a trailing newline for clean output.
static std::string chomp(const std::string& s) {
    if (!s.empty() && s.back() == '\n') return s.substr(0, s.size() - 1);
    return s;
}

/// Compare two ASTs obtained via JSON parsing (semantic equivalence).
static bool json_asts_equal(const AstNode& a, const AstNode& b) {
    return a == b;
}

// =========================================================================
// Per-test-case round-trip
// =========================================================================

static void run_full_roundtrip(int test_num) {
    std::string stml_path  = g_testdata_path + "/test" + std::to_string(test_num) + ".stml";
    std::string json_path  = g_testdata_path + "/test" + std::to_string(test_num) + "_expected.json";

    // Check files exist
    {
        std::ifstream chk(stml_path);
        if (!chk.is_open()) { std::cout << "  SKIP: " << stml_path << " not found\n"; return; }
    }
    {
        std::ifstream chk(json_path);
        if (!chk.is_open()) { std::cout << "  SKIP: " << json_path << " not found\n"; return; }
    }

    std::string stml_text = read_file(stml_path);
    std::string expected_json_text = read_file(json_path);

    // Empty expected JSON → it implies an empty docs list.
    // The file-based read trims nothing, so we check for empty/whitespace-only.
    {
        bool only_ws = true;
        for (char ch : expected_json_text) {
            if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t') { only_ws = false; break; }
        }
        if (only_ws) expected_json_text = R"({"docs": []})";
    }

    // Parse expected JSON into AST (our "ground truth")
    AstNode expected_ast = test_json::parse(expected_json_text);

    // ── Chain A: STML → AST → JSON ≟ expected JSON ────────
    auto [ast_from_stml, w1] = loads(stml_text);
    std::string json_from_stml = to_json(ast_from_stml);

    AstNode json_from_stml_ast = test_json::parse(json_from_stml);
    if (!json_asts_equal(json_from_stml_ast, expected_ast)) {
        std::cout << "\n  Chain A FAIL for test" << test_num << "\n";
        std::cout << "  STML→JSON:\n  " << chomp(json_from_stml) << "\n";
        std::cout << "  Expected:\n  " << chomp(expected_json_text) << "\n";
        throw std::runtime_error("Chain A: STML→JSON != expected JSON");
    }

    // ── Chain B: JSON → AST → STML → AST → JSON ≟ expected JSON ──
    std::string stml_from_json = dumps(expected_ast);
    auto [ast_rt, w2] = loads(stml_from_json);
    std::string json_rt = to_json(ast_rt);

    AstNode json_rt_ast = test_json::parse(json_rt);
    if (!json_asts_equal(json_rt_ast, expected_ast)) {
        std::cout << "\n  Chain B FAIL for test" << test_num << "\n";
        std::cout << "  JSON→STML→JSON:\n  " << chomp(json_rt) << "\n";
        std::cout << "  Expected:\n  " << chomp(expected_json_text) << "\n";
        throw std::runtime_error("Chain B: JSON→STML→JSON != expected JSON");
    }
}

// =========================================================================
// Individual test functions
// =========================================================================

static void test_chain_ab_1() { run_full_roundtrip(1); }
static void test_chain_ab_2() { run_full_roundtrip(2); }
static void test_chain_ab_3() { run_full_roundtrip(3); }
static void test_chain_ab_4() { run_full_roundtrip(4); }
static void test_chain_ab_5() { run_full_roundtrip(5); }
static void test_chain_ab_6() { run_full_roundtrip(6); }
static void test_chain_ab_7() { run_full_roundtrip(7); }
static void test_chain_ab_8() { run_full_roundtrip(8); }
static void test_chain_ab_9() { run_full_roundtrip(9); }
static void test_chain_ab_10() { run_full_roundtrip(10); }
static void test_chain_ab_11() { run_full_roundtrip(11); }
static void test_chain_ab_12() { run_full_roundtrip(12); }
static void test_chain_ab_13() { run_full_roundtrip(13); }

// =========================================================================
// Main
// =========================================================================

int main(int argc, char* argv[]) {
    if (argc > 1) {
        g_testdata_path = argv[1];
    } else {
        std::ifstream check("/tmp/TestData/test1.stml");
        if (check.is_open()) {
            g_testdata_path = "/tmp/TestData";
        } else {
            std::cerr << "Usage: test_full_roundtrip <TestData_dir>\n";
            return 1;
        }
    }

    std::cout << "Full round-trip tests (using TestData: " << g_testdata_path << ")\n\n";

    std::cout << "Chain A: STML → AST → JSON ≟ expected JSON\n";
    std::cout << "Chain B: JSON → AST → STML → AST → JSON ≟ expected JSON\n\n";

    TEST(test_chain_ab_1);
    TEST(test_chain_ab_2);
    TEST(test_chain_ab_3);
    TEST(test_chain_ab_4);
    TEST(test_chain_ab_5);
    TEST(test_chain_ab_6);
    TEST(test_chain_ab_7);
    TEST(test_chain_ab_8);
    TEST(test_chain_ab_9);
    TEST(test_chain_ab_10);
    TEST(test_chain_ab_11);
    TEST(test_chain_ab_12);
    TEST(test_chain_ab_13);

    std::cout << "\n" << tests_passed << "/" << tests_run << " passed\n";
    return tests_passed == tests_run ? 0 : 1;
}
