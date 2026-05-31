#include "../stml_stream.h"
#include "../stml.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace stml;

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; std::cout << "  " << name << "... "; } while(0)
#define OK() do { passed++; std::cout << "OK\n"; } while(0)
#define FAIL(msg) do { std::cout << "FAIL: " << msg << "\n"; } while(0)

// 批量解析 vs 流式解析应该一致
static void test_batch_vs_stream(const std::string& input) {
    // 批量解析
    AstList batch_result = loads(input);

    // 流式解析
    STMLStreamer streamer;
    streamer.feed(input);
    AstList stream_result = streamer.finish();

    // 比较
    if (batch_result != stream_result) {
        std::string json1 = to_json(batch_result);
        std::string json2 = to_json(stream_result);
        FAIL("Batch vs stream mismatch\n    Batch:   " + json1 + "\n    Stream:  " + json2);
    } else {
        OK();
    }
}

// 逐字符流式 vs 批量应该一致
static void test_char_by_char(const std::string& input) {
    AstList batch_result = loads(input);

    STMLStreamer streamer;
    for (char c : input) {
        streamer.feed(std::string(1, c));
    }
    AstList stream_result = streamer.finish();

    if (batch_result != stream_result) {
        std::string json1 = to_json(batch_result);
        std::string json2 = to_json(stream_result);
        FAIL("Char-by-char mismatch\n    Batch:   " + json1 + "\n    Char:    " + json2);
    } else {
        OK();
    }
}

int main(int argc, char* argv[]) {
    std::cout << "Streaming tests\n\n";

    // 基本测试：批量 vs 流式
    std::cout << "=== Batch vs Stream ===\n";
    test_batch_vs_stream("name: STML");
    test_batch_vs_stream("items:\n  - a\n  - b");
    test_batch_vs_stream("parent:\n  child: val");
    test_batch_vs_stream("- key1: val1\n  extra: val2\n- key2: val3");
    test_batch_vs_stream("bare_key");
    test_batch_vs_stream("---\ndoc1: a\n---\ndoc2: b");

    // 逐字符测试
    std::cout << "\n=== Char-by-Char ===\n";
    test_char_by_char("name: STML");
    test_char_by_char("- a\n- b\n- c");
    test_char_by_char("parent:\n  child: val");

    // 分块测试
    std::cout << "\n=== Chunked ===\n";
    {
        std::string input = "a: 1\nb: 2\nc: 3";
        AstList batch = loads(input);

        // 分成2个字符的块
        STMLStreamer streamer;
        for (size_t i = 0; i < input.size(); i += 2) {
            streamer.feed(input.substr(i, 2));
        }
        AstList chunked = streamer.finish();

        TEST("chunked (2-char blocks)");
        if (batch == chunked) {
            OK();
        } else {
            FAIL("chunked mismatch");
        }
    }

    // TestData 文件流式测试
    std::string test_data_dir = ".";
    if (argc > 1) {
        test_data_dir = argv[1];
    }

    std::cout << "\n=== TestData Files ===\n";
    for (int i = 1; i <= 11; ++i) {
        std::string path = test_data_dir + "/test" + std::to_string(i) + ".stml";
        std::ifstream f(path);
        if (!f.is_open()) continue;
        f.close();

        std::ifstream in(path);
        std::ostringstream ss;
        ss << in.rdbuf();
        std::string content = ss.str();

        TEST("test" + std::to_string(i) + ".stml (batch vs stream)");
        test_batch_vs_stream(content);
    }

    std::cout << "\n" << passed << "/" << tests << " streaming tests passed\n";
    return passed == tests ? 0 : 1;
}
