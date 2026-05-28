/// config_reader — 读取 STML 配置文件并查询/修改值。
/// 演示从文件解析、多层嵌套读取、路径遍历。

#include <iostream>
#include <string>
#include "../stml_cpp/stml.h"

using namespace stml;

/// 安全获取嵌套映射中的值，路径如 {"服务端", "端口"}
static const AstNode* get_by_path(const AstMap& root,
                                   const std::vector<std::string>& path) {
    const AstNode* cur = nullptr;
    for (size_t i = 0; i < path.size(); ++i) {
        bool first = (i == 0);
        const auto& key = path[i];
        if (first) {
            auto it = root.find(key);
            if (it == root.end()) return nullptr;
            cur = &it->second;
        } else {
            if (!cur->is_map()) return nullptr;
            auto it = cur->as_map()->find(key);
            if (it == cur->as_map()->end()) return nullptr;
            cur = &it->second;
        }
    }
    return cur;
}

int main(int argc, char* argv[]) {
    std::string filepath = "config.stml";
    if (argc > 1) filepath = argv[1];

    std::cout << "=== 读取配置文件: " << filepath << " ===" << std::endl << std::endl;

    // 1. 从文件加载
    LoadResult result = load(filepath);

    if (!result.warnings.empty()) {
        std::cout << "警告 (" << result.warnings.size() << " 条):" << std::endl;
        for (const auto& w : result.warnings) {
            std::cout << "  [" << w.line << ":" << w.column << "] " << w.message << std::endl;
        }
        std::cout << std::endl;
    }

    // 2. 获取文档根映射
    const auto& doc1 = *result.ast.as_map()->at("docs").as_list()->at(0).as_map();

    // 3. 读取标量值
    std::cout << "--- 基本配置 ---" << std::endl;
    const AstNode* val;

    val = get_by_path(doc1, {"应用名称"});
    if (val) std::cout << "  应用名称: " << *val->as_string() << std::endl;

    val = get_by_path(doc1, {"版本号"});
    if (val) std::cout << "  版本号:   " << *val->as_string() << std::endl;

    // 4. 读取嵌套映射
    std::cout << std::endl << "--- 服务端 ---" << std::endl;
    val = get_by_path(doc1, {"服务端", "主机"});
    if (val) std::cout << "  主机:      " << *val->as_string() << std::endl;

    val = get_by_path(doc1, {"服务端", "端口"});
    if (val) std::cout << "  端口:      " << *val->as_string() << std::endl;

    val = get_by_path(doc1, {"服务端", "启用HTTPS"});
    if (val) std::cout << "  启用HTTPS: " << *val->as_string() << std::endl;

    // 5. 读取序列
    std::cout << std::endl << "--- 功能开关 ---" << std::endl;
    val = get_by_path(doc1, {"功能开关"});
    if (val && val->is_list()) {
        const auto& features = *val->as_list();
        for (size_t i = 0; i < features.size(); ++i) {
            std::cout << "  [" << i << "] " << *features[i].as_string() << std::endl;
        }
    }

    // 6. 遍历所有键（使用 walk）
    std::cout << std::endl << "--- 全部叶节点 ---" << std::endl;
    walk(result.ast, [](const std::vector<std::string>& path, const AstNode& node) {
        for (size_t i = 0; i < path.size(); ++i) {
            if (i > 0) std::cout << ".";
            std::cout << path[i];
        }
        if (node.is_string()) {
            std::cout << " = " << *node.as_string();
        } else if (node.is_null()) {
            std::cout << " = null";
        }
        std::cout << std::endl;
    });

    return 0;
}
