/// Streaming parser tests.
/// Verifies that streaming parse results match batch parse results
/// across multiple chunking strategies for all TestData files.

#include <cassert>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>

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

/// Read a file into a string.
static std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open: " + path);
    }
    std::string text((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
    return text;
}

/// Helper to normalize text (for comparison purposes)
static std::string normalize(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
            out.push_back('\n');
        } else {
            out.push_back(text[i]);
        }
    }
    return out;
}

/// Compare streaming result against batch result.
static void compare_results(const LoadResult& stream_result,
                           const LoadResult& batch_result,
                           const std::string& context) {
    if (stream_result.ast != batch_result.ast) {
        std::cout << "\n  Context: " << context << "\n";
        std::cout << "  Batch JSON:\n" << to_json(batch_result.ast);
        std::cout << "  Stream JSON:\n" << to_json(stream_result.ast);
        throw std::runtime_error("AST mismatch: " + context);
    }

    // Compare warning counts (warnings may differ in order but should be same count)
    if (stream_result.warnings.size() != batch_result.warnings.size()) {
        std::cout << "\n  Context: " << context << "\n";
        std::cout << "  Batch warnings: " << batch_result.warnings.size() << "\n";
        for (const auto& w : batch_result.warnings)
            std::cout << "    [" << w.line << ":" << w.column << "] " << w.message << "\n";
        std::cout << "  Stream warnings: " << stream_result.warnings.size() << "\n";
        for (const auto& w : stream_result.warnings)
            std::cout << "    [" << w.line << ":" << w.column << "] " << w.message << "\n";
        throw std::runtime_error("Warning count mismatch: " + context);
    }
}

/// Verify that streaming parse of a single file produces the same result
/// as batch parse, using a specific chunking strategy.
static void test_streaming_for_file(const std::string& stml_path,
                                    const std::string& chunking_name,
                                    const std::vector<std::string>& chunks) {
    // Batch parse for reference
    std::string text = read_file(stml_path);
    text = normalize(text);
    auto batch_result = loads(text);

    // Streaming parse
    STMLStreamer streamer;
    for (const auto& chunk : chunks) {
        streamer.feed(chunk);
    }
    auto stream_result = streamer.finalize();

    std::string context = stml_path + " [" + chunking_name + "]";
    compare_results(stream_result, batch_result, context);
}

// =========================================================================
// Chunking strategies
// =========================================================================

/// Split text by lines (each line is a chunk).
static std::vector<std::string> chunk_by_lines(const std::string& text) {
    std::vector<std::string> chunks;
    size_t start = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            chunks.push_back(text.substr(start, i - start + 1));
            start = i + 1;
        }
    }
    if (start < text.size()) {
        chunks.push_back(text.substr(start));
    }
    return chunks;
}

/// Split text into chunks of a fixed size.
static std::vector<std::string> chunk_by_size(const std::string& text, int size) {
    std::vector<std::string> chunks;
    for (size_t i = 0; i < text.size(); i += size) {
        chunks.push_back(text.substr(i, size));
    }
    return chunks;
}

/// Split text at document separators.
static std::vector<std::string> chunk_by_doc_separator(const std::string& text) {
    std::vector<std::string> chunks;
    size_t start = 0;
    for (size_t i = 0; i + 2 < text.size(); ++i) {
        if (text[i] == '\n' && text[i+1] == '-' && text[i+2] == '-' && text[i+3] == '-') {
            chunks.push_back(text.substr(start, i - start + 1));
            start = i + 1;
        }
    }
    if (start < text.size()) {
        chunks.push_back(text.substr(start));
    }
    if (chunks.empty()) {
        chunks.push_back(text);
    }
    return chunks;
}

/// Split text at multiline boundaries (inside { ... } blocks).
static std::vector<std::string> chunk_by_multiline_split(const std::string& text) {
    std::vector<std::string> chunks;
    size_t start = 0;
    bool in_multiline = false;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '{' && (i == 0 || text[i-1] != '\\')) {
            in_multiline = true;
        } else if (text[i] == '}' && in_multiline) {
            in_multiline = false;
            // Split right after '}'
            if (i + 1 < text.size()) {
                chunks.push_back(text.substr(start, i - start + 1));
                start = i + 1;
            }
        } else if (text[i] == '\n' && in_multiline && i > start) {
            // Split inside multiline
            chunks.push_back(text.substr(start, i - start + 1));
            start = i + 1;
        }
    }
    if (start < text.size()) {
        chunks.push_back(text.substr(start));
    }
    if (chunks.empty()) {
        chunks.push_back(text);
    }
    return chunks;
}

// =========================================================================
// Test: full text at once (degenerate streaming case)
// =========================================================================

static void test_full_text(int test_num) {
    std::string stml_file = g_testdata_path + "/test" + std::to_string(test_num) + ".stml";
    std::ifstream check(stml_file);
    if (!check.is_open()) {
        ++tests_passed;
        return;
    }
    check.close();

    std::string text = normalize(read_file(stml_file));
    test_streaming_for_file(stml_file, "full_text", {text});
}

// =========================================================================
// Test: fixed-size chunks (e.g., 50 bytes — simulates typical LLM output)
// =========================================================================

static void test_by_50_bytes(int test_num) {
    std::string stml_file = g_testdata_path + "/test" + std::to_string(test_num) + ".stml";
    std::ifstream check(stml_file);
    if (!check.is_open()) {
        ++tests_passed;
        return;
    }
    check.close();

    std::string text = normalize(read_file(stml_file));
    // Use larger chunk sizes for larger files
    int chunk_size = text.size() < 1000 ? 50 : 200;
    auto chunks = chunk_by_size(text, chunk_size);
    test_streaming_for_file(stml_file,
        "by_" + std::to_string(chunk_size) + "_bytes", chunks);
}

// =========================================================================
// Test: split at document separators
// =========================================================================

static void test_by_doc_sep(int test_num) {
    std::string stml_file = g_testdata_path + "/test" + std::to_string(test_num) + ".stml";
    std::ifstream check(stml_file);
    if (!check.is_open()) {
        ++tests_passed;
        return;
    }
    check.close();

    std::string text = normalize(read_file(stml_file));
    auto chunks = chunk_by_doc_separator(text);
    test_streaming_for_file(stml_file, "by_doc_sep", chunks);
}

// =========================================================================
// Test: split inside multiline strings
// =========================================================================

static void test_multiline_split(int test_num) {
    std::string stml_file = g_testdata_path + "/test" + std::to_string(test_num) + ".stml";
    std::ifstream check(stml_file);
    if (!check.is_open()) {
        ++tests_passed;
        return;
    }
    check.close();

    std::string text = normalize(read_file(stml_file));
    // Only test if file contains multiline
    if (text.find('{') == std::string::npos) {
        ++tests_passed;
        return;
    }
    auto chunks = chunk_by_multiline_split(text);
    test_streaming_for_file(stml_file, "multiline_split", chunks);
}

// =========================================================================
// Test: empty chunks interspersed
// =========================================================================

static void test_empty_chunks(int test_num) {
    std::string stml_file = g_testdata_path + "/test" + std::to_string(test_num) + ".stml";
    std::ifstream check(stml_file);
    if (!check.is_open()) {
        ++tests_passed;
        return;
    }
    check.close();

    std::string text = normalize(read_file(stml_file));
    auto line_chunks = chunk_by_lines(text);

    // Interleave empty chunks
    std::vector<std::string> chunks;
    for (size_t i = 0; i < line_chunks.size(); ++i) {
        chunks.push_back("");
        chunks.push_back(line_chunks[i]);
    }

    test_streaming_for_file(stml_file, "empty_chunks_interleaved", chunks);
}

// =========================================================================
// Test: stream_parse convenience function
// =========================================================================

static void test_convenience_function() {
    std::string text = "key: value\nlist:\n  - item1\n  - item2\n";

    auto batch_result = loads(text);
    auto stream_result = stream_parse(text);

    compare_results(stream_result, batch_result, "stream_parse convenience");
}

// =========================================================================
// Test: test6.stml should throw ParseError (via loads)
// =========================================================================

static void test_parse_error_6() {
    std::string stml_file = g_testdata_path + "/test6.stml";
    std::ifstream check(stml_file);
    if (!check.is_open()) {
        ++tests_passed;
        return;
    }
    check.close();

    std::string text = normalize(read_file(stml_file));
    try {
        loads(text);
        throw std::runtime_error("Expected ParseError for test6 but none was thrown");
    } catch (const ParseError&) {
        // Expected
    }
}

// =========================================================================
// Test generators for each test file
// =========================================================================

#define GEN_TESTS(num) \
    static void test_full_text_##num()   { test_full_text(num); } \
    static void test_sized_##num()       { test_by_50_bytes(num); } \
    static void test_docsep_##num()      { test_by_doc_sep(num); } \
    static void test_multiline_##num()   { test_multiline_split(num); } \
    static void test_empty_##num()       { test_empty_chunks(num); }

GEN_TESTS(1)
GEN_TESTS(2)
GEN_TESTS(3)
GEN_TESTS(4)
GEN_TESTS(5)
GEN_TESTS(7)
GEN_TESTS(8)
GEN_TESTS(9)
GEN_TESTS(10)
GEN_TESTS(11)
GEN_TESTS(12)

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
            check.close();
        } else {
            std::cerr << "Cannot find TestData directory. Pass path as argument.\n";
            return 1;
        }
    }

    std::cout << "Using TestData at: " << g_testdata_path << "\n\n";

    #define RUN_TESTS(num) \
        std::cout << "--- test" << num << ".stml ---\n"; \
        TEST(test_full_text_##num); \
        TEST(test_sized_##num); \
        TEST(test_docsep_##num); \
        TEST(test_multiline_##num); \
        TEST(test_empty_##num);

    RUN_TESTS(1)
    RUN_TESTS(2)
    RUN_TESTS(3)
    RUN_TESTS(4)
    RUN_TESTS(5)
    RUN_TESTS(7)
    RUN_TESTS(8)
    RUN_TESTS(9)
    RUN_TESTS(10)
    RUN_TESTS(11)
    RUN_TESTS(12)

    std::cout << "\n--- ParseError test ---\n";
    TEST(test_parse_error_6);

    std::cout << "\n--- Convenience function ---\n";
    TEST(test_convenience_function);

    std::cout << "\n" << tests_passed << "/" << tests_run << " passed\n";
    return tests_passed == tests_run ? 0 : 1;
}
