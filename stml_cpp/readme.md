# STML C++ Library

STML (Stable Tree Markup Language) 解析器的 C++17 参考实现。零外部依赖，纯标准库。

## 快速开始

```cpp
#include "stml.h"

int main() {
    // 解析 STML 文本
    auto [ast, warnings] = stml::loads("\"项目\": \"STML\"\n");

    // 输出为标准 STML
    std::string canonical = stml::dumps(ast);
    // → "\"项目\": \"STML\"\n"

    // 输出为 JSON
    std::string json = stml::to_json(ast);
    // → {"docs":{"doc1":{"项目":"STML"}}}
}
```

## 构建

**要求**：C++17 编译器，无外部依赖。

```bash
# 推荐方式（支持中文路径）
bash build.sh

# CMake（路径需纯 ASCII）
mkdir build && cd build
cmake .. -G "MinGW Makefiles" && cmake --build .
```

产物：
- `build/libstml.a` — 静态库
- `build/test_*.exe` — 测试可执行文件

## API

所有接口位于 `stml` 命名空间，引入 `stml.h` 即可。

### 解析

```cpp
// 从文本解析 → AST + 警告列表
stml::LoadResult loads(const std::string& text);

// 从文件解析
stml::LoadResult load(const std::string& filename);
```

`LoadResult` 结构体：

```cpp
struct LoadResult {
    AstNode ast;                     // AST 根节点
    std::vector<Warning> warnings;   // 诊断警告
};
```

**AST 结构**：总是 `{"docs": {"doc1": ..., "doc2": ...}}`，即使单文档。

### 序列化

```cpp
// AST → 标准 STML 文本
std::string dumps(const AstNode& node);

// AST → JSON 文本
std::string to_json(const AstNode& node);
```

### 调试/低级接口

```cpp
// 仅词法分析 → Token 流
auto [tokens, warnings] = tokenize(const std::string& text);

// 仅语法分析（从 Token 流）
auto [ast_map, warnings] = parse(const std::vector<Token>& tokens);
```

## 数据类型

### AstNode

四种节点类型，通过 `std::variant` 实现：

```cpp
AstNode node;

// 创建
node = AstNode();                         // null
node = AstNode("hello");                  // 字符串标量
node = AstNode(AstList{...});             // 列表
node = AstNode(AstMap{{"key", val}});     // 映射

// 查询
node.is_null();      // → bool
node.is_string();    // → bool
node.is_list();      // → bool
node.is_map();       // → bool

// 访问（类型不匹配返回 nullptr）
const std::string* s = node.as_string();
const AstList*     l = node.as_list();
const AstMap*      m = node.as_map();

// 可变访问
std::string* s = node.as_string_mut();
AstList*     l = node.as_list_mut();
AstMap*      m = node.as_map_mut();
```

- **AstMap** = `std::map<std::string, AstNode>` — 保持插入顺序的有序映射
- **AstList** = `std::vector<AstNode>` — 有序列表

### 比较、克隆、遍历

```cpp
if (ast1 == ast2) { /* 深度相等 */ }

AstNode copy = clone(original);          // 深拷贝

walk(node, [](auto& path, auto& n) {    // 遍历所有叶节点
    // path: {"doc1", "key", "0", ...}
});
```

### Token

```cpp
enum class TokenType : uint8_t {
    NEWLINE, INDENT, DEDENT, DOC_SEPARATOR, END,
    KEY, BARE_KEY, COLON,
    SCALAR, NULL_, RAW_STRING,
    DASH, INLINE_LIST, MULTILINE_STRING
};

struct Token {
    TokenType type;
    TokenValue value;   // variant<monostate, string, vector<optional<string>>>
    int line;           // 1-based
    int column;         // 1-based
};
```

### Warning

```cpp
struct Warning {
    int line;
    int column;
    std::string message;
};
```

## 标准 STML 输出规则

`dumps()` 序列化为标准 STML，每次输出一致：

| 规则 | 示例 |
|------|------|
| 缩进 2 空格 | `  ` |
| 键始终双引号 | `"key"` |
| null 无引号 | `null` |
| 标量值双引号 | `"value"` |
| 序列前缀 | `- ` |
| 内联列表 | `[a, b, c]` |
| 文档分隔 | `---` |
| 逃逸 | `\"` `\\` `\n` `\t` |

## 项目文件结构

```
stml_cpp/
├── lexer/              # 词法分析（~900 行，最复杂模块）
├── ast/                # AST 节点：递归 variant + 遍历
├── parser/             # 递归下降语法分析（~750 行）
├── serializer/         # AST → STML / JSON
├── diagnostics/        # ParseError 异常、Warning、恢复策略
├── tests/              # 单元测试 + 回归测试 + 往返测试
├── stml.h / stml.cpp   # 公共 API 胶水层
├── CMakeLists.txt      # CMake 构建
├── build.sh            # Shell 构建脚本
├── readme.md           # 本文件
└── dev.md              # 开发日志
```

## 测试

```bash
cd build

# 词法分析测试（14 项）
./test_lexer.exe

# 语法分析测试（15 项）
./test_parser.exe

# 序列化 + 往返测试（12 项）
./test_serializer.exe

# 回归测试（与 Python 版共用 TestData/，10 项）
./test_regression.exe /path/to/TestData
```

往返测试验证 `STML → AST → STML → AST` 两次解析结果完全一致。
回归测试将 C++ 解析结果与 Python 版预期 JSON 逐项比对。

## 许可证

待定。
