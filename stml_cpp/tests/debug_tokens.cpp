/// Debug tool: tokenize an STML file and dump all tokens
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "stml.h"

using namespace stml;

const char* type_name(TokenType t) {
    switch (t) {
        case TokenType::END: return "END";
        case TokenType::NEWLINE: return "NEWLINE";
        case TokenType::INDENT: return "INDENT";
        case TokenType::DEDENT: return "DEDENT";
        case TokenType::DOC_SEPARATOR: return "DOC_SEP";
        case TokenType::KEY: return "KEY";
        case TokenType::BARE_KEY: return "BARE_KEY";
        case TokenType::COLON: return "COLON";
        case TokenType::DASH: return "DASH";
        case TokenType::SCALAR: return "SCALAR";
        case TokenType::NULL_: return "NULL";
        case TokenType::RAW_STRING: return "RAW_STR";
        case TokenType::INLINE_LIST: return "INLINE_LIST";
        case TokenType::MULTILINE_STRING: return "MULTILINE";
        default: return "???";
    }
}

std::string read_file(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main(int argc, char* argv[]) {
    if (argc < 2) { std::cerr << "Usage: debug_tokens <file.stml>\n"; return 1; }
    auto text = read_file(argv[1]);
    auto [tokens, warnings] = tokenize(text);

    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& t = tokens[i];
        std::string val_str;
        if (auto* s = std::get_if<std::string>(&t.value)) {
            val_str = *s;
            if (val_str.size() > 40) val_str = val_str.substr(0, 40) + "...";
        }
        std::cout << "[" << i << "] L" << t.line << ":" << t.column
                  << " " << type_name(t.type);
        if (!val_str.empty()) std::cout << " \"" << val_str << "\"";
        std::cout << "\n";
    }
    std::cout << "Total: " << tokens.size() << " tokens\n";
    return 0;
}
