#include <iostream>
#include <fstream>
#include "stml.h"
#include "tests/test_json.h"
using namespace stml;

int main() {
    // Simulate run_full_roundtrip(10)
    std::string stml_path = "/f/Studio/Project/my_graduation_project2/Language/TestData/test10.stml";
    std::string json_path = "/f/Studio/Project/my_graduation_project2/Language/TestData/test10_expected.json";
    
    std::ifstream f1(stml_path);
    std::string stml_text((std::istreambuf_iterator<char>(f1)), std::istreambuf_iterator<char>());
    std::cout << "stml_text length: " << stml_text.size() << "\n";
    
    std::ifstream f2(json_path);
    std::string expected_json_text((std::istreambuf_iterator<char>(f2)), std::istreambuf_iterator<char>());
    std::cout << "expected_json_text: [" << expected_json_text << "]\n";
    
    try {
        AstNode expected_ast = test_json::parse(expected_json_text);
        std::cout << "Expected JSON parsed OK\n";
        
        auto [ast_from_stml, w1] = loads(stml_text);
        std::string json_from_stml = to_json(ast_from_stml);
        std::cout << "json_from_stml: [" << json_from_stml << "]\n";
        
        AstNode json_from_stml_ast = test_json::parse(json_from_stml);
        std::cout << "json_from_stml parsed OK\n";
        
        bool match = json_from_stml_ast == expected_ast;
        std::cout << "Match: " << (match ? "YES" : "NO") << "\n";
    } catch (const std::exception& e) {
        std::cout << "ERROR: " << e.what() << "\n";
    }
    return 0;
}
