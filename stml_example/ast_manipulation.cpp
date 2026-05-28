/// ast_manipulation — 编程方式构建 AST 并序列化。
/// 演示如何在不解析文本的情况下构造 STML 文档。

#include <iostream>
#include <string>
#include "../stml_cpp/stml.h"

using namespace stml;

int main() {
    std::cout << "=== 编程方式构建 AST ===" << std::endl << std::endl;

    // ============================================================
    // 构建一个复杂的嵌套 AST
    // ============================================================
    AstMap doc;

    // -- 标量字段 --
    doc["书名"]          = AstNode("深入理解 STML");
    doc["作者"]          = AstNode("张三");
    doc["页数"]          = AstNode("356");
    doc["已发布"]        = AstNode("true");

    // -- 嵌套映射 --
    AstMap publisher;
    publisher["名称"]    = AstNode("技术出版社");
    publisher["城市"]    = AstNode("北京");
    publisher["年份"]    = AstNode("2026");
    doc["出版信息"]       = AstNode(std::move(publisher));

    // -- 序列 --
    AstList chapters;
    chapters.push_back(AstNode("第一章：引言"));
    chapters.push_back(AstNode("第二章：词法分析"));
    chapters.push_back(AstNode("第三章：语法分析"));
    chapters.push_back(AstNode("第四章：序列化"));
    doc["目录"]           = AstNode(std::move(chapters));

    // -- 序列中包含映射（STML 列表单键条目，每项一个映射） --
    AstList authors;

    AstMap a1;
    a1["姓名"] = AstNode("张三");
    authors.push_back(AstNode(std::move(a1)));

    AstMap a2;
    a2["姓名"] = AstNode("李四");
    authors.push_back(AstNode(std::move(a2)));

    AstMap a3;
    a3["姓名"] = AstNode("王五");
    authors.push_back(AstNode(std::move(a3)));
    doc["作者团队"] = AstNode(std::move(authors));

    // -- null 值 --
    doc["备注"] = AstNode();

    // ============================================================
    // 构建 docs 包装（模拟 loads() 输出格式）
    // ============================================================
    AstList docs_list;
    docs_list.push_back(AstNode(std::move(doc)));

    AstMap root;
    root["docs"] = AstNode(std::move(docs_list));
    AstNode ast(std::move(root));

    // ============================================================
    // 序列化输出
    // ============================================================
    std::cout << "--- STML 输出 ---" << std::endl;
    std::cout << stml::dumps(ast) << std::endl;

    std::cout << "--- JSON 输出 ---" << std::endl;
    std::cout << stml::to_json(ast) << std::endl;

    // ============================================================
    // 验证：往返
    // ============================================================
    std::string stml_text = stml::dumps(ast);
    auto [ast2, w2] = stml::loads(stml_text);

    std::cout << "--- 验证 ---" << std::endl;
    if (ast == ast2) {
        std::cout << "✓ 编程构建的 AST 经 STML 往返后完全一致" << std::endl;
    } else {
        std::cout << "✗ 往返不一致!" << std::endl;
        std::cout << "原始:" << std::endl << stml::to_json(ast);
        std::cout << "往返后:" << std::endl << stml::to_json(ast2);
    }

    // ============================================================
    // AST 查询和修改
    // ============================================================
    std::cout << std::endl << "--- AST 查询 ---" << std::endl;

    // 深层克隆
    AstNode copy = clone(ast2);
    AstList& copy_docs = *copy.as_map_mut()->at("docs").as_list_mut();
    AstMap& copy_doc1 = *copy_docs[0].as_map_mut();

    // 修改字段
    copy_doc1["页数"] = AstNode("400");   // 更新页数
    copy_doc1["已发布"] = AstNode("false"); // 改为未发布
    copy_doc1["备注"] = AstNode("需要复审"); // 更新备注

    std::cout << "修改后的 STML:" << std::endl;
    std::cout << stml::dumps(copy);

    return 0;
}
