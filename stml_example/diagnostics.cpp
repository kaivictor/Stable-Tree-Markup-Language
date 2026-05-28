/// diagnostics — 演示 STML 的警告系统和错误恢复。
/// 解析包含各种不规范格式的文本并展示诊断信息。

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include "../stml_cpp/stml.h"

using namespace stml;

int main() {
    // 构造一个包含多种不规范格式的 STML 文本
    // 注意：以下所有"问题"都会被确定性恢复，不会崩溃
    std::string messy_stml = R"STML(
# 正常部分
"名称": "正确的值"
"版本": 1.0
"启用": true

# 问题 1：引号未闭合
"描述": "这是一个没有闭合引号的字符串

# 问题 2：非法转义
"路径": "C:\Users\张三\文档"

# 问题 3：内联列表格式问题
"标签": [标签1, 标签2,

# 问题 4：空值写法
"空配置": null
"另一空": ~

# 问题 5：不规范的缩进
"父项":
    "子项A": 值A
   "子项B": 值B      # 注意缩进变化
  "子项C": 值C        # 继续变化

# 问题 6：裸键（无冒号）
"完整键": 有值
仅键名                # 无冒号，值为 null
)STML";

    std::cout << "=== STML 诊断演示 ===" << std::endl;
    std::cout << "解析包含不规范格式的文本..." << std::endl;
    std::cout << std::endl;

    // 解析 — 不会崩溃
    auto [ast, warnings] = stml::loads(messy_stml);

    // 展示所有警告
    std::cout << "诊断结果: " << warnings.size() << " 条警告" << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    if (warnings.empty()) {
        std::cout << "  无警告" << std::endl;
    } else {
        for (size_t i = 0; i < warnings.size(); ++i) {
            const auto& w = warnings[i];
            std::cout << "  [" << (i + 1) << "] "
                      << "行 " << std::setw(3) << w.line
                      << ", 列 " << std::setw(3) << w.column
                      << ": " << w.message << std::endl;
        }
    }

    std::cout << std::string(50, '-') << std::endl;
    std::cout << std::endl;

    // 展示恢复后的 AST（仍然完整）
    std::cout << "恢复后的 AST（JSON 格式）:" << std::endl;
    std::cout << stml::to_json(ast);

    // 列出恢复后的所有叶节点
    std::cout << std::endl << "叶节点清单:" << std::endl;
    std::cout << std::string(40, '-') << std::endl;
    walk(ast, [](const std::vector<std::string>& path, const AstNode& node) {
        for (size_t i = 0; i < path.size(); ++i) {
            if (i > 0 && path[i - 1] == "docs") continue;
            if (path[i] == "docs") continue;
            std::cout << (i > 1 ? "." : "") << path[i];
        }
        if (node.is_string()) {
            std::string val = *node.as_string();
            if (val.size() > 40) val = val.substr(0, 40) + "...";
            std::cout << " → \"" << val << "\"";
        } else if (node.is_null()) {
            std::cout << " → null";
        }
        std::cout << std::endl;
    });

    return 0;
}
