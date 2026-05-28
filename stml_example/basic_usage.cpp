/// basic_usage — STML 解析和序列化的最简示例。
/// 演示 loads / dumps / to_json 三个核心 API。

#include <iostream>
#include <string>
#include "../stml_cpp/stml.h"

int main() {
    // ============================================================
    // 1. 从字符串解析 STML
    // ============================================================
    std::string stml_text = R"STML(
"标题": "STML 入门"
"作者": "张三"
"标签":
  - C++
  - 解析器
  - 配置
"元信息":
  "创建日期": 2026-05-16
  "许可": MIT
)STML";

    auto [ast, warnings] = stml::loads(stml_text);

    std::cout << "=== 解析结果 ===" << std::endl << std::endl;

    // 2. 输出为 JSON
    std::cout << "JSON 格式:" << std::endl;
    std::cout << stml::to_json(ast) << std::endl;

    // 3. 输出为标准 STML（往返）
    std::cout << "标准 STML 格式:" << std::endl;
    std::string canonical = stml::dumps(ast);
    std::cout << canonical << std::endl;

    // 4. 验证往返幂等性
    auto [ast2, w2] = stml::loads(canonical);
    if (ast == ast2) {
        std::cout << "✓ 往返幂等性验证通过" << std::endl;
    } else {
        std::cout << "✗ 往返幂等性验证失败!" << std::endl;
    }

    // 5. 读取 AST 中的值
    const auto& doc1 = *ast.as_map()->at("docs").as_list()->at(0).as_map();

    std::string title = *doc1.at("标题").as_string();
    std::string author = *doc1.at("作者").as_string();
    const auto& tags = *doc1.at("标签").as_list();

    std::cout << std::endl << "=== 按字段读取 ===" << std::endl;
    std::cout << "标题: " << title << std::endl;
    std::cout << "作者: " << author << std::endl;
    std::cout << "标签: ";
    for (size_t i = 0; i < tags.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << *tags[i].as_string();
    }
    std::cout << std::endl;

    return 0;
}
