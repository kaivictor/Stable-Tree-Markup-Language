#include <iostream>
#include "stml.h"
#include "tests/test_json.h"
using namespace stml;
int main() {
    // test10: empty file
    auto [ast, w] = loads("");
    std::string json_out = to_json(ast);
    std::cout << "JSON from empty STML: " << json_out << "\n";
    
    // Roundtrip: dumps → loads → to_json
    std::string stml_out = dumps(ast);
    std::cout << "STML from AST: [" << stml_out << "]\n";
    auto [ast2, w2] = loads(stml_out);
    std::string json2 = to_json(ast2);
    std::cout << "JSON roundtrip: " << json2 << "\n";
    
    // Check expected JSON parse
    std::string expected = "{\"docs\": []}";
    auto exp_ast = test_json::parse(expected);
    std::cout << "Expected parsed OK\n";
    
    // Chain B
    std::string stml_from_json = dumps(exp_ast);
    std::cout << "STML from expected JSON: [" << stml_from_json << "]\n";
    auto [ast_rt, w3] = loads(stml_from_json);
    std::string json_rt = to_json(ast_rt);
    std::cout << "Chain B JSON: " << json_rt << "\n";
    std::cout << "Match: " << (json_rt == expected || json_rt == "{\n  \"docs\": []\n}" || json_rt == "{\"docs\": []}" ? "YES" : "NO") << "\n";
    return 0;
}
