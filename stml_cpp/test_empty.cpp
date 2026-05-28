#include <iostream>
#include "stml.h"
using namespace stml;
int main() {
    auto [ast, w] = loads("");
    std::cout << to_json(ast) << "\n";
    return 0;
}
