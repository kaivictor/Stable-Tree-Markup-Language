/// Debug: parse test2.stml and dump AST as JSON
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "stml.h"

using namespace stml;

std::string read_file(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main(int argc, char* argv[]) {
    if (argc < 2) { std::cerr << "Usage: debug_parse <file.stml>\n"; return 1; }
    auto text = read_file(argv[1]);
    auto [ast, warnings] = loads(text);
    std::cout << to_json(ast) << "\n";
    for (auto& w : warnings)
        std::cerr << "WARN L" << w.line << ":" << w.column << " " << w.message << "\n";
    return 0;
}
