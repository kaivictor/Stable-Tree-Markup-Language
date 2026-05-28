# STML C++ Library — 开发日志

## 项目总览

将 Python 验证版 `stml_py_` 移植到 C++17，作为后续 TypeScript/Java/Python/Rust 绑定的原生基础。

- **语言标准**：C++17（`std::optional`, `std::variant`, `std::string_view`, 结构化绑定）
- **外部依赖**：零（纯标准库）
- **代码量**：~2300 行（不含测试）
- **测试覆盖**：51 项单元/回归/往返测试
- **平台**：Windows (MinGW-w64)、Linux、macOS

---

## 2026-05-16 — 项目启动与核心实现

### 阶段 1：基础类型

创建 `diagnostics/error.h`、`lexer/token.h`、`ast/ast.h`：

- **TokenType** 枚举 14 种 token，`TokenValue` 使用 `variant<monostate, string, vector<InlineElem>>`
- **AstNode** 递归 `variant` 定义，`AstMap` 选用 `std::map` 保持插入顺序（对标 Python 的 `dict`）
- **Warning** 和 **ParseError** 携带行列号

### 阶段 2：词法分析器（lexer）

最复杂模块，~900 行。照搬 Python 逐函数直译：

| 算法 | 说明 |
|------|------|
| 缩进栈 | `_process_indent()` 仅空格计缩进；`_emit_dedents_to()` 弹出多级 |
| 延迟 DEDENT | 多行 `}` 闭合后由主循环发射 DEDENT（`_deferred_dedent_to`） |
| 结构冒号扫描 | `_find_unquoted_colon()` 状态机：`"` 翻转、`\` 跳 2 字节、`:` 命中 |
| 引号匹配 | 贪婪（值）：`_find_last_unescaped_quote`；非贪婪（键）：`_find_first_unescaped_quote` |
| 逃逸处理 | `_unescape()` 支持 `\" \\ \n \t`，非法逃逸保留原样 + warning |
| 内联列表 | `_parse_inline_list()` 逐字符扫描嵌套 `[]`、引号内逗号、`null/~` |
| 多行字符串 | `_read_multiline()` 逐行读到独立 `}` 或 EOF 自动闭合 |

**Python→C++ 翻译要点**：
- `None` → `std::monostate{}`
- `Optional[T]` → `std::optional<T>`
- `Tuple[A, B, C]` 返回 → `std::tuple<A, B, C>`
- `str.rstrip()` / `str.strip()` → 内联指针扫描
- `str.find()` / `str.rfind()` → `std::string` 成员方法

### 阶段 3：语法分析器（parser）

~750 行，从 Token 流构建 AST。关键设计：

- **索引指针消费**：`m_pos` 推进，不拷贝，不回溯
- **映射解析**：跟踪 INDENT/DEDENT 嵌套计数，DASH 出现 = 块类型冲突 → warning + 终止
- **序列解析**：双模式（`nested=true/false`），`_is_complex_sequence()` 预扫描判断
- **重新缩进**：DEDENT+INDENT 连续视为同级块
- **文档包装**：`parse()` 始终返回 `{"doc1": ..., ...}`，`loads()` 再加 `{"docs": ...}` 外层

### 阶段 4：序列化器（serializer）

- **to_stml.cpp**（~250 行）：递归 `serialize_node()`，2 空格缩进，键始终双引号，null 无引号
- **to_json.cpp**（~80 行）：递归 `to_json_impl()`，支持 Unicode 逃逸
- `dumps()` 识别 `docs` 包装并正确输出 `---` 分隔符，`docs` 按 `doc1`/`doc2` 排序

### 阶段 5：测试

4 个测试文件：

| 测试文件 | 项数 | 内容 |
|----------|------|------|
| test_lexer.cpp | 14 | 缩进、键值、空值、裸键、序列、注释、引号键、文档分隔、内联列表、多行字符串、逃逸、空行 |
| test_parser.cpp | 15 | 简单/嵌套映射、null/~、序列/嵌套序列/混合序列、内联列表、多行字符串、多文档 |
| test_serializer.cpp | 12 | 标量/引号转义/嵌套/列表/内联列表 序列化 + 5 项往返 |
| test_regression.cpp | 10 | 7 个 TestData 回归 + 3 项往返回归 |

### 4. `std::filesystem` 中文路径崩溃

MinGW 的 `std::filesystem::exists()` 无法处理中文路径。修复：改用 `std::ifstream` 尝试直接打开文件判断存在性。

---

## 与 Python 版的差异

| 差异点 | Python | C++ |
|--------|--------|-----|
| Token 存储 | INDENT 值可为 int | INDENT 值为 string（`to_string`） |
| 错误处理 | 全部 recovery + warning | 同，额外保留 `ParseError` 异常类 |
| AST Map | `dict`（3.7+ 保证插入序） | `std::map`（天然有序） |
| 字符串处理 | 天然 UTF-8 | `std::string` 按字节处理 UTF-8 |
| 泛型 | 鸭子类型 | 显式模板 + variant |

---

## 待完成

1. **CMake 构建修复**：需要支持含非 ASCII 字符的路径
2. **VS Code 插件**：语法高亮、格式化、行内警告
3. **C++20 优化**：`std::string::starts_with/ends_with` 替换手动指针扫描
4. **Allocator-aware**：允许自定义分配器
5. **其他语言绑定**：TypeScript、Java、Python、Rust

---

## 当前状态

```
版本: 1.3.0（C++ 移植版）
测试: 51/51 通过 ✅
往返幂等性: 已验证 ✅
回归: 与 Python 版 TestData 完全一致 ✅
编译: MinGW-w64 GCC 16.1.0，零警告
```
