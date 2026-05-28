#include <iostream>
#include <fstream>
#include "stml.h"
#include "tests/test_json.h"
using namespace stml;

int main() {
    std::string stml_path = "/f/Studio/Project/my_graduation_project2/Language/TestData/test11.stml";
    std::string json_path = "/f/Studio/Project/my_graduation_project2/Language/TestData/test11_expected.json";
    
    std::ifstream f1(stml_path);
    std::string stml_text((std::istreambuf_iterator<char>(f1)), std::istreambuf_iterator<char>());
    std::cout << "stml_text length: " << stml_text.size() << " content: [" << stml_text << "]\n";
    
    std::ifstream f2(json_path);
    std::string expected_json_text((std::istreambuf_iterator<char>(f2)), std::istreambuf_iterator<char>());
    std::cout << "expected_json_text: [" << expected_json_text << "]\n";
    
    try {
        auto [ast, w] = loads(stml_text);
        std::string json_out = to_json(ast);
        std::cout << "STML->JSON: [" << json_out << "]\n";
        
        AstNode expected_ast = test_json::parse(expected_json_text);
        AstNode json_ast = test_json::parse(json_out);
        std::cout << "Match: " << (json_ast == expected_ast ? "YES" : "NO") << "\n";
        
        if (json_ast != expected_ast) {
            std::cout << "Expected JSON AST:\n" << to_json(expected_ast) << "\n";
        }
    } catch (const std::exception& e) {
        std::cout << "ERROR: " << e.what() << "\n";
    }
    return 0;
}
