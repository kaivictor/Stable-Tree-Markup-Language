#ifndef STML_DIAGNOSTICS_ERROR_H
#define STML_DIAGNOSTICS_ERROR_H

#include <string>
#include <vector>
#include <stdexcept>

namespace stml {

// ============================================================
// Warning — 解析警告（非错误，解析结果可能和用户预期不同）
// ============================================================
struct Warning {
    int line = 0;
    int col = 0;
    std::string message;

    Warning() = default;
    Warning(int l, int c, std::string msg)
        : line(l), col(c), message(std::move(msg)) {}
};

// ============================================================
// ParseError — 解析异常（程序级错误）
// ============================================================
class ParseError : public std::runtime_error {
public:
    int line = 0;
    int col = 0;

    ParseError(int l, int c, const std::string& msg)
        : std::runtime_error(msg), line(l), col(c) {}
};

} // namespace stml

#endif // STML_DIAGNOSTICS_ERROR_H
