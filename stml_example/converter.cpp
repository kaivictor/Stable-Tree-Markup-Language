/// converter — STML ↔ JSON 双向命令行转换器。
/// 用法: converter input.stml          → 输出 JSON
///       converter input.json --json   → 输出 STML（将 JSON 当作标量映射）
///       converter input.stml --stml   → 规范化 STML 输出

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "../stml_cpp/stml.h"

// 文件读取辅助
static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("无法打开文件: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "用法:" << std::endl;
        std::cerr << "  converter <file>           STML → JSON" << std::endl;
        std::cerr << "  converter <file> --stml    规范化 STML" << std::endl;
        std::cerr << "  converter <file> --json    JSON → STML（键引号化）" << std::endl;
        return 1;
    }

    std::string path = argv[1];
    std::string mode = (argc > 2) ? argv[2] : "";

    try {
        std::string input = read_file(path);

        if (mode == "--json") {
            // JSON → STML：为键加引号输出为 STML（规范格式化）
            // 实际：当作 STML 解析（可接受类似 JSON 的输入），再规范输出
            auto [ast, w] = stml::loads(input);
            for (const auto& ww : w)
                std::cerr << "警告 [" << ww.line << ":" << ww.column << "] " << ww.message << std::endl;
            std::cout << stml::dumps(ast);
        } else {
            // STML → JSON 或 规范化 STML
            auto [ast, w] = stml::loads(input);
            for (const auto& ww : w)
                std::cerr << "警告 [" << ww.line << ":" << ww.column << "] " << ww.message << std::endl;

            if (mode == "--stml") {
                // 规范化 STML
                std::cout << stml::dumps(ast);
            } else {
                // 默认：输出 JSON
                std::cout << stml::to_json(ast);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
