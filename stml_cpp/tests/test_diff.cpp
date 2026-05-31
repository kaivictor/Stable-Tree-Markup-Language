/// Quick diff checker for test1 — finds exact AST differences.
#include "stml.h"
#include "test_json.h"
#include <iostream>
#include <fstream>
#include <sstream>

using namespace stml;

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void diff_ast(const AstNode& a, const AstNode& e, const std::string& path, int depth = 0) {
    if (depth > 20) return;
    std::string pad(depth*2, ' ');

    if (a.is_null() != e.is_null()) { std::cout << pad << path << ": type mismatch (null vs non-null)\n"; return; }
    if (a.is_null()) return; // both null

    if (a.is_string() != e.is_string()) { std::cout << pad << path << ": type mismatch (string vs non-string)\n"; return; }
    if (a.is_string()) {
        std::string as = *a.as_string(), es = *e.as_string();
        if (as != es) std::cout << pad << path << ": '" << as << "' vs '" << es << "'\n";
        return;
    }

    if (a.is_list() != e.is_list()) { std::cout << pad << path << ": type mismatch (list vs non-list)\n"; return; }
    if (a.is_list()) {
        const auto& al = *a.as_list();
        const auto& el = *e.as_list();
        if (al.size() != el.size()) std::cout << pad << path << ".size: " << al.size() << " vs " << el.size() << "\n";
        for (size_t i = 0; i < std::min(al.size(), el.size()); ++i)
            diff_ast(al[i], el[i], path + "[" + std::to_string(i) + "]", depth+1);
        return;
    }

    if (a.is_map() != e.is_map()) { std::cout << pad << path << ": type mismatch (map vs non-map)\n"; return; }
    if (a.is_map()) {
        const auto& am = *a.as_map();
        const auto& em = *e.as_map();
        if (am.size() != em.size()) std::cout << pad << path << ".size: " << am.size() << " vs " << em.size() << "\n";
        for (size_t i = 0; i < am.size(); ++i) {
            if (i >= em.size()) {
                std::cout << pad << path << "[" << am[i].first << "]: extra in actual\n";
            } else {
                if (am[i].first != em[i].first)
                    std::cout << pad << path << ": key order diff at " << i << ": '" << am[i].first << "' vs '" << em[i].first << "'\n";
                diff_ast(am[i].second, em[i].second, path + ".\"" + am[i].first + "\"", depth+1);
            }
        }
        return;
    }
}

int main(int argc, char* argv[]) {
    std::string testdata = argc > 1 ? argv[1] : "../../TestData";

    for (int n = 1; n <= 12; ++n) {
        if (n == 6) continue; // test6 expected JSON has comments
        std::string stml_file = testdata + "/test" + std::to_string(n) + ".stml";
        std::string exp_file = testdata + "/test" + std::to_string(n) + "_expected.json";

        std::ifstream check(stml_file);
        if (!check.is_open()) { std::cout << "test" << n << ": SKIP (no stml)\n"; continue; }
        check.close();

        std::string stml_text = read_file(stml_file);

        // Parse STML
        auto result = loads(stml_text);

        // Parse expected JSON
        std::string exp_json_text = read_file(exp_file);
        AstNode expected_wrapped;
        try {
            expected_wrapped = test_json::parse(exp_json_text);
        } catch (const std::exception& e) {
            std::cout << "test" << n << ": SKIP (invalid expected JSON: " << e.what() << ")\n";
            continue;
        }

        const AstNode* exp_docs = map_find(*expected_wrapped.as_map(), "docs");
        const AstNode* act_docs = map_find(*result.ast.as_map(), "docs");

        if (!act_docs || !act_docs->is_list() || !exp_docs || !exp_docs->is_list()) {
            std::cout << "test" << n << ": SKIP (bad structure)\n";
            continue;
        }

        const auto& al = *act_docs->as_list();
        const auto& el = *exp_docs->as_list();

        if (al.size() != el.size()) {
            std::cout << "test" << n << ": doc count " << al.size() << " vs " << el.size() << "\n";
        }

        bool mismatch = false;
        for (size_t d = 0; d < std::min(al.size(), el.size()); ++d) {
            if (al[d] != el[d]) {
                mismatch = true;
                std::cout << "test" << n << " doc[" << d << "]: MISMATCH\n";
                diff_ast(al[d], el[d], "test" + std::to_string(n) + " doc[" + std::to_string(d) + "]", 0);
            }
        }

        if (mismatch) {
            // Print the actual JSON for reference
            std::cout << "\n--- Actual JSON for test" << n << " ---\n";
            std::cout << to_json(result.ast) << "\n";
        } else {
            std::cout << "test" << n << ": MATCH\n";
        }
    }
    return 0;
}
