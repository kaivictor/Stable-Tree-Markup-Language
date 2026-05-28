# STML C++ 示例说明

## 目录结构

```
stml_example/
├── basic_usage.cpp         # 入门：解析/序列化/往返验证/按字段读取
├── config_reader.cpp       # 实战：读取配置文件、嵌套查询、遍历
├── converter.cpp           # 工具：STML ↔ JSON 命令行转换器
├── diagnostics.cpp         # 诊断：不规范格式的确定性恢复 + 警告展示
├── ast_manipulation.cpp    # 进阶：编程构建 AST、克隆、修改、往返
├── config.stml             # 示例配置文件
├── CMakeLists.txt          # CMake 构建
├── build.sh                # Shell 构建脚本
└── example_cpp.md          # 本文件
```

---

## 1. basic_usage — 入门示例

**演示**：`loads` / `dumps` / `to_json` 三个核心 API，往返幂等性验证，按字段读取 AST 值。

```cpp
// 从字符串解析
auto [ast, warnings] = stml::loads(stml_text);

// 输出 JSON
std::cout << stml::to_json(ast);

// 输出规范 STML
std::string canonical = stml::dumps(ast);

// 验证往返幂等性
auto [ast2, w2] = stml::loads(canonical);
assert(ast == ast2);

// 按字段读取
const auto& doc1 = *ast.as_map()->at("docs").as_map()->at("doc1").as_map();
std::string title = *doc1.at("标题").as_string();
```

运行输出：
```
=== 解析结果 ===
JSON 格式:
{ "docs": { "doc1": { "标题": "STML 入门", ... } } }

标准 STML 格式:
"标题": "STML 入门"
"作者": "张三"
...

✓ 往返幂等性验证通过

=== 按字段读取 ===
标题: STML 入门
作者: 张三
标签: C++, 解析器, 配置
```

---

## 2. config_reader — 配置文件读取

**演示**：从文件加载、嵌套路径查询、`walk()` 遍历所有叶节点。

核心技巧：`get_by_path()` 安全获取深层嵌套值。

```cpp
auto result = stml::load("config.stml");

// 按路径读取
auto* val = get_by_path(doc1, {"服务端", "端口"});
std::cout << *val->as_string();  // → 8080

// 读取列表
val = get_by_path(doc1, {"功能开关"});
for (const auto& item : *val->as_list()) {
    std::cout << *item.as_string();
}
```

运行输出（摘录）：
```
=== 读取配置文件 ===
--- 基本配置 ---
  应用名称: STML演示
  版本号:   1.0.0
--- 服务端 ---
  主机:      127.0.0.1
  端口:      8080
  启用HTTPS: true
--- 功能开关 ---
  [0] 日志
  [1] 缓存
  [2] 通知
--- 全部叶节点 ---
docs.doc1.嵌套数据.用户.管理员.权限.0 = 读取
docs.doc1.数据库.主机 = db.example.com
...
```

---

## 3. converter — 命令行转换器

**演示**：封装为命令行工具。

```bash
converter input.stml           # STML → JSON
converter input.stml --stml    # 规范化 STML 输出
```

```cpp
auto [ast, w] = stml::loads(input);
if (mode == "--stml")
    std::cout << stml::dumps(ast);  // 规范化
else
    std::cout << stml::to_json(ast);  // JSON
```

运行示例：
```bash
$ ./converter ../config.stml
{ "docs": { "doc1": { ... } } }

$ ./converter ../config.stml --stml
"服务端":
  "主机": "127.0.0.1"
  "端口": "8080"
...
```

---

## 4. diagnostics — 诊断与恢复

**演示**：输入故意包含不规范格式，解析器**不崩溃**，产出可用的 AST + 警告列表。

输入的不规范格式：

| 问题类型 | 示例 | 恢复方式 |
|----------|------|----------|
| 引号未闭合 | `"描述": "没有闭合...` | 降级为 RAW_STRING |
| 非法转义 | `C:\Users\张三` | 保留原样 |
| 内联列表缺 `]` | `[标签1, 标签2,` | 降级为 RAW_STRING |
| 不规则缩进 | 同块内缩进变化 | 按 DEDENT/INDENT 自动调整 |
| 裸键 | `仅键名`（无冒号） | 视为 BARE_KEY，值 null |

运行输出（摘录）：
```
诊断结果: 5 条警告
--------------------------------------------------
  [1] 行 8, 列 1: 引号未闭合，降级为原始字符串
  [2] 行 11, 列 4: 非法转义序列 '\U'，保留原样
  [3] 行 11, 列 10: 非法转义序列 '\张'，保留原样
  [4] 行 11, 列 17: 非法转义序列 '\文'，保留原样
  [5] 行 14, 列 1: 内联列表缺少结尾 ']'，降级为原始字符串

恢复后的 AST（JSON 格式）:
{ "docs": { "doc1": { ... } } }   ← 结构完整
```

---

## 5. ast_manipulation — 编程构建 AST

**演示**：完全不解析文本，纯代码构造 AST 并序列化、克隆、修改。

```cpp
// 构建映射
AstMap doc;
doc["书名"] = AstNode("深入理解 STML");
doc["作者"] = AstNode("张三");

// 嵌套映射
AstMap pub;
pub["名称"] = AstNode("技术出版社");
doc["出版信息"] = AstNode(std::move(pub));

// 序列
AstList ch;
ch.push_back(AstNode("第一章"));
doc["目录"] = AstNode(std::move(ch));

// 包装为 docs 格式
AstMap root;
root["docs"] = AstNode(AstMap{{"doc1", AstNode(std::move(doc))}});
AstNode ast(std::move(root));

// 克隆并修改
AstNode copy = clone(ast);
```

运行输出（摘录）：
```
--- STML 输出 ---
"书名": "深入理解 STML"
"作者": "张三"
"目录":
  - "第一章：引言"
  - "第二章：词法分析"
...

--- 验证 ---
✓ 编程构建的 AST 经 STML 往返后完全一致

--- AST 查询 ---
修改后的 STML:            ← 页数改为 400，已发布改为 false，备注更新
"已发布": "false"
"页数": "400"
"备注": "需要复审"
```

---

## 构建和运行

```bash
# 1. 先构建 STML 库
cd ../stml_cpp
bash build.sh

# 2. 构建示例
cd ../stml_example
bash build.sh

# 3. 运行
cd build
./basic_usage.exe
./config_reader.exe ../config.stml
./converter.exe ../config.stml
./converter.exe ../config.stml --stml
./diagnostics.exe
./ast_manipulation.exe
```

## API 速查

| 函数 | 作用 |
|------|------|
| `loads(text)` | 文本 → `(AstNode, warnings)` |
| `load(path)` | 文件 → `(AstNode, warnings)` |
| `dumps(node)` | AST → 标准 STML 文本 |
| `to_json(node)` | AST → JSON 文本 |
| `tokenize(text)` | 仅词法分析 |
| `parse(tokens)` | 从 Token 流解析 |
| `clone(node)` | 深拷贝 |
| `walk(node, fn)` | 遍历所有叶节点 |
| `node.is_null()` / `is_string()` / `is_list()` / `is_map()` | 类型查询 |
| `node.as_string()` / `as_list()` / `as_map()` | 值访问 |
