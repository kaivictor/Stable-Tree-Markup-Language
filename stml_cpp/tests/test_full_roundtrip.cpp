#include "../stml.h"
#include "../serializer/serializer.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace stml;

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; std::cout << "  " << name << "... "; } while(0)
#define OK() do { passed++; std::cout << "OK\n"; } while(0)
#define FAIL(msg) do { std::cout << "FAIL: " << msg << "\n"; } while(0)

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// 测试 STML → AST → STML → AST → 两次 AST 应该一致
static void test_roundtrip(const std::string& input) {
    // 第一次解析
    AstList docs1 = loads(input);

    // 序列化回 STML
    std::string stml1 = dumps(docs1);

    // 第二次解析
    AstList docs2 = loads(stml1);

    // 比较两次 AST
    bool equal = true;
    if (docs1.items.size() != docs2.items.size()) {
        equal = false;
    } else {
        for (size_t i = 0; i < docs1.items.size(); ++i) {
            if (docs1.items[i] != docs2.items[i]) {
                equal = false;
                break;
            }
        }
    }

    if (!equal) {
        std::string json1 = to_json(docs1);
        std::string json2 = to_json(docs2);
        FAIL("Roundtrip mismatch\n    First:  " + json1 + "\n    Second: " + json2);
    } else {
        OK();
    }
}

int main(int argc, char* argv[]) {
    std::cout << "Full roundtrip tests\n\n";

    // 内联测试用例
    test_roundtrip("name: STML");
    test_roundtrip("items:\n  - a\n  - b\n  - c");
    test_roundtrip("person:\n  name: John\n  age: 30");
    test_roundtrip("empty_key:");
    test_roundtrip("- key1: val1\n  extra: val2\n- key3: val3");
    test_roundtrip("bare_key");
    test_roundtrip("---\ndoc1: true\n---\ndoc2: true");
    test_roundtrip("list:\n  - item1\n  - item2\nmap:\n  a: 1\n  b: 2");

    // 从 TestData 目录读取标准 STML 文件进行往返测试
    std::string test_data_dir = ".";
    if (argc > 1) {
        test_data_dir = argv[1];
    }

    for (int i = 1; i <= 11; ++i) {
        std::string path = test_data_dir + "/test" + std::to_string(i) + ".stml";
        std::ifstream f(path);
        if (!f.is_open()) continue;
        f.close();

        TEST("test" + std::to_string(i) + ".stml roundtrip");
        std::string content = read_file(path);
        test_roundtrip(content);
    }

    std::cout << "\n" << passed << "/" << tests << " roundtrip tests passed\n";
    return passed == tests ? 0 : 1;
}
