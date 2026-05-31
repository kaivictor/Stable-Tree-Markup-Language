#include "../stml.h"
#include "test_json.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace stml;

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; std::cout << "  " << name << "... "; } while(0)
#define OK() do { passed++; std::cout << "OK\n"; } while(0)
#define FAIL(msg) do { std::cout << "FAIL: " << msg << "\n"; } while(0)

// 读取整个文件
static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) throw std::runtime_error("Cannot open: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// 测试一组 STML → 解析 → 与预期 JSON 比较
static void test_file(const std::string& test_data_dir, int test_num) {
    std::string stml_name = "test" + std::to_string(test_num);
    std::string json_name = "test" + std::to_string(test_num) + "_expected";

    std::string stml_path = test_data_dir + "/" + stml_name + ".stml";
    std::string json_path = test_data_dir + "/" + json_name + ".json";

    // 读取 STML
    std::string stml_text = read_file(stml_path);

    // 读取预期 JSON（可能不是合法 JSON，如 test6 的注释）
    std::string expected_json = read_file(json_path);

    TEST(stml_name + " (STML→JSON)");

    AstNode expected_wrapped;
    try {
        expected_wrapped = test_json::parse(expected_json);
    } catch (const std::exception& e) {
        FAIL("Cannot parse expected JSON: " + std::string(e.what()));
        return;
    }

    // 解析 STML
    AstList docs = loads(stml_text);

    // 转换为 JSON 并比较
    std::string actual_json = to_json(docs);

    // 解析实际 JSON 比较
    AstNode actual_wrapped;
    try {
        actual_wrapped = test_json::parse(actual_json);
    } catch (const std::exception& e) {
        FAIL("Cannot parse actual JSON: " + std::string(e.what()) +
             "\n    Actual:   " + actual_json);
        return;
    }

    if (actual_wrapped == expected_wrapped) {
        OK();
    } else {
        FAIL("JSON mismatch\n    Expected: " + expected_json + "\n    Actual:   " + actual_json);
    }
}

int main(int argc, char* argv[]) {
    std::string test_data_dir = ".";
    if (argc > 1) {
        test_data_dir = argv[1];
    }

    std::cout << "Regression tests (TestData dir: " << test_data_dir << ")\n\n";

    // 测试 test1 到 test11（如果存在）
    for (int i = 1; i <= 11; ++i) {
        std::string stml_path = test_data_dir + "/test" + std::to_string(i) + ".stml";
        std::ifstream f(stml_path);
        if (!f.is_open()) continue; // 跳过不存在的
        f.close();
        test_file(test_data_dir, i);
    }

    // 测试标准文档
    for (int i = 1; i <= 8; ++i) {
        std::string stml_path = test_data_dir + "/\xe6\xb5\x8b\xe8\xaf\x95" +
                                std::to_string(i) +
                                "\xe6\xa0\x87\xe5\x87\x86\xe6\x96\x87\xe6\xa1\xa3.stml";
        std::ifstream f(stml_path);
        if (!f.is_open()) continue;
        f.close();
        test_file(test_data_dir, i);
    }

    std::cout << "\n" << passed << "/" << tests << " regression tests passed\n";
    return passed == tests ? 0 : 1;
}
