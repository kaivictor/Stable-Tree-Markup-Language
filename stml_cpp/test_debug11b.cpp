#include <iostream>
#include "stml.h"
using namespace stml;
int main() {
    auto [ast, w] = loads("---\n");
    std::cout << "loads('---'): " << to_json(ast) << "\n";
    std::cout << "docs count: " << map_find(*ast.as_map(), "docs")->as_list()->size() << "\n";
    return 0;
}
