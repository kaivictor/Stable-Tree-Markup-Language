# Stable Tree Markup Language

STML 是一种面向人类手写与机器生成的纯文本数据格式。它继承 YAML 的简洁语法，但移除所有隐式类型推断、锚点、别名、复杂多行指示符等特性，并通过严格的容错与确定性规则确保：任何符合本规范的解析器，对任意输入都会生成完全相同的抽象语法树（AST），且永远不会崩溃。

[Github]() | [Document](STML规范.md) | [Bilibili]() | [Web]()

## 特性
依靠换行、状态、相对缩进进行识别。支持流式解析、适用于LLM、有限兼容YAML。

## 示例

```stml
"STML":  "Stable Tree Markup Language"
"特色":
  - "解析确定性高"
  - "容错性强"
  - "输出严格"
---
"映射":
  "键":  "值"
"字符串":  "文本"
"空值":  null
"内联列表":  ["a", "b"]
# 注释
"多行文本":  {
  内容
}
```

## 支持类型
- 键值对
- 字符串
- 内联列表
- 多行文本
- 多行列表
- 多文档
- 注释

## 对YAML的兼容

|	|STML|YAML|
| - | - | - |
边界|基于缩进、冒号与换行|基于缩进、冒号与换行
Null支持|支持null、~为Null|支持null、~为Null
字典嵌套|支持|支持
内联列表|支持|支持
注释|独行注释|支持
多文档|支持|支持

## 快速上手

### C++

```cpp
#include "stml.h"

auto [ast, warnings] = stml::loads("键: 值");
std::string json = stml::to_json(ast);
std::string stml_text = stml::dumps(ast);
```

### Java

```java
import stml.STML;

var result = STML.loads("键: 值");
String json = STML.toJson(result.ast);
String stmlText = STML.dumps(result.ast);
```

### Python

```python
import stml

ast, warnings = stml.loads("键: 值")
json_str = stml.to_json(ast)
stml_text = stml.dumps(ast)
```

### JavaScript / TypeScript

```javascript
const { loads, dumps, toJson } = require('./lib/parser');

const { ast, warnings } = loads("键: 值");
const json = toJson(ast);
const stmlText = dumps(ast);
```

## 仓库内容

### 解析器实现

所有解析器共享同一套测试用例（[TestData](TestData)），跨语言解析结果完全一致。解析管线统一为：**Lexer → LineTreeBuilder → AstBuilder → AST → Serializer**，均支持 STML 解析、AST 序列化回 STML、AST 转 JSON、流式解析（按 `---` 文档分隔符增量处理）。

| 目录 | 语言 | 依赖 | 说明 |
| - | - | - | - |
| [stml_cpp](stml_cpp) | C++17 | 无外部依赖 | 核心库，Python 绑定基础。CMake + MinGW 构建 |
| [stml_java](stml_java) | Java 17 | 无外部依赖 | 原生实现，javac 编译 |
| [stml_py](stml_py) | Python 3.10+ | pybind11 | 基于 stml_cpp 的 C++ 扩展模块 |
| [stml_js](stml_js) | JavaScript | 无外部依赖 | Node.js 原生实现，零依赖 |
| [stml_ts](stml_ts) | TypeScript | 无外部依赖 | Node.js 原生实现，npm run build 编译 |
| [stml_vsix](stml_vsix) | TypeScript | VSCode | 语法高亮、代码片段、括号配对 |

### 其他目录

| 目录 | 说明 |
| - | - |
| [EffectsInLargeModels2](EffectsInLargeModels2) | 让 LLM 使用 STML 的效果测试，[测试报告](EffectsInLargeModels2/报告.md) |
| IdeaValidationDraft | 手动编写的验证代码草稿 |
| IdeaValidationProject | LLM 编写的验证代码 |
| IdeaValidationProjectTest | LLM 编写的验证代码测试 |




