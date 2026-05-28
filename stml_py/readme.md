# STML Python 绑定库

STML (Stable Tree Markup Language) 的 Python 接口。底层为 C++ 核心 (`stml_cpp`)，通过 pybind11 暴露为 Python 模块。零运行时外部依赖，.pyd 全静态链接。

## 快速开始

```python
import stml

# 解析 STML 文本
ast, warnings = stml.loads('"项目": "STML"\n')

# 查看 AST
print(ast["docs"]["doc1"]["项目"])   # → "STML"

# 输出为标准 STML
canonical = stml.dumps(ast)
# → '"项目": "STML"\n'

# 输出为 JSON
json_str = stml.to_json(ast)
# → '{\n  "docs": {\n    "doc1": {\n      "项目": "STML"\n    }\n  }\n}\n'
```

## 安装

### 方式 1：直接拷贝目录（零安装，推荐开发试用）

`stml/` 目录是自包含的——Python 包 + 编译好的 `.pyd`。把它放到你的项目目录下就能 `import stml`：

```
my_project/
├── stml/                        # 拷贝整个目录过来
│   ├── __init__.py
│   ├── _core.cp313-win_amd64.pyd  # 全静态链接，无外部 DLL 依赖
│   └── py.typed
└── your_script.py               # import stml 直接用
```

```python
# your_script.py
import stml
ast, _ = stml.loads('"key": "value"\n')
print(ast)
```

约束：`.pyd` 文件是 Python 版本 + 平台绑定的（`cp313` = CPython 3.13，`win_amd64` = 64-bit Windows）。切换 Python 版本需重新编译。

### 方式 2：wheel 安装（开发环境）

```bash
cd stml_py

# 一次性编译（需 MinGW-w64 GCC）：
export PATH="/d/SoftWares/DevKits/mingw64_16.1.0/bin:$PATH"
export CXX="g++" CC="gcc"
python setup.py build_ext --compiler=mingw32 --inplace

# 运行（设置 PYTHONPATH）：
PYTHONPATH="$PWD/src" python -c "import stml; ..."

# 测试：
PYTHONPATH="$PWD/src" python -m pytest tests/ -v
```

> **注意**：当前 `pip install -e .` 不可用，原因是 C++ 源文件在 `../stml_cpp/` 被 setuptools 判定为绝对路径。详见 `package.md`。

### 方式 3：wheel 安装（分发）

```bash
pip install stml-1.3.0-cp313-cp313-win_amd64.whl
# 装完后，任意目录 import stml 可用
```

### 正式发布后（PyPI）

```bash
pip install stml
# 无需 CMake、MinGW、pybind11
# .pyd 全静态链接，只有 KERNEL32.dll / msvcrt.dll 系统依赖
```

## API

所有接口位于 `stml` 包根命名空间。

### 解析

```python
# 从文本解析 → (ast, warnings)
ast, warnings = stml.loads(text: str) -> tuple[dict, list[Warning]]

# 从文件解析
ast, warnings = stml.load(path: str) -> tuple[dict, list[Warning]]
```

**AST 结构**：总是 `{"docs": {"doc1": ..., "doc2": ...}}`，即使单文档也如此包装。

### 序列化

```python
# AST → 标准 STML 文本
text = stml.dumps(ast) -> str

# AST → JSON 文本
json_str = stml.to_json(ast) -> str
```

`dumps()` 输出符合 STML 规范的 canonical 格式：键始终双引号、2 空格缩进、null 无引号。

### 调试/低级接口

```python
# 仅词法分析 → Token 流
tokens, warnings = stml.tokenize(text: str) -> tuple[list[dict], list[Warning]]
# 每个 token: {"type": str, "value": str|list|None, "line": int, "column": int}

# 仅语法分析（从 Token 流）
ast, warnings = stml.parse(tokens: list[dict]) -> tuple[dict, list[Warning]]
```

## 数据类型

### Warning

```python
from stml import Warning

@dataclass(frozen=True)
class Warning:
    message: str    # 人类可读描述
    line: int       # 行号（1-based）
    column: int     # 列号（1-based）
```

警告是非致命诊断。AST 始终合法，警告仅提示用户可能的输入问题（未闭合引号、格式歧义等）。

### AST 节点

Python AST 使用原生类型，无需额外包装类：

| STML 类型 | Python 类型 |
|-----------|-------------|
| null      | `None`      |
| scalar    | `str`       |
| sequence  | `list`      |
| mapping   | `dict`      |

```python
# 构建 AST
ast = {
    "docs": {
        "doc1": {
            "name": "Alice",
            "age": None,           # null
            "tags": ["a", "b"],    # sequence
            "meta": {              # nested mapping
                "ver": "1.0"
            }
        }
    }
}

# 序列化
print(stml.dumps(ast))
```

### Token（调试用）

```python
tokens, _ = stml.tokenize('"key": "value"\n')
# tokens[0] → {"type": "KEY", "value": "key", "line": 1, "column": 1}
# tokens[1] → {"type": "COLON", "value": None, "line": 1, "column": 6}
# tokens[2] → {"type": "SCALAR", "value": "value", "line": 1, "column": 8}
```

Token 类型共计 14 种：`NEWLINE`, `INDENT`, `DEDENT`, `DOC_SEPARATOR`, `END`, `KEY`, `BARE_KEY`, `COLON`, `SCALAR`, `NULL_`, `RAW_STRING`, `DASH`, `INLINE_LIST`, `MULTILINE_STRING`。

## 异常

```python
# 文件不存在
stml.load("nonexistent.stml")  # → RuntimeError

# 致命解析错误（理论上不应发生）
# → RuntimeError，携带行列号和错误信息
```

## 标准 STML 输出规则

`dumps()` 每次输出一致：

| 规则 | 示例 |
|------|------|
| 缩进 2 空格 | `  ` |
| 键始终双引号 | `"key"` |
| null 无引号 | `null` |
| 标量值双引号 | `"value"` |
| 序列前缀 | `- ` |
| 内联列表 | `[a, b, c]` |
| 文档分隔 | `---` |
| 转义 | `\"` `\\` `\n` `\t` |

## 项目文件结构

```
stml_py/
├── pyproject.toml         # PEP 621 元数据、setuptools 构建声明
├── setup.py               # Pybind11Extension 编译配置（~60 行）
├── readme.md              # 本文件
├── dev.md                 # 开发日志
├── package.md             # 打包分发说明
├── src/stml/
│   ├── __init__.py        # 公有 API + Warning dataclass + 类型提示
│   ├── _core.cpp          # pybind11 C++ 绑定（~220 行）
│   ├── _core.*.pyd        # 编译产物（全静态链接）
│   └── py.typed           # PEP 561 类型标记
└── tests/
    ├── test_full_roundtrip.py   # 7 组 TestData 全链回归
    ├── test_lexer.py            # 词法分析单元测试
    ├── test_parser.py           # 语法分析单元测试
    └── test_serializer.py       # 序列化/往返单元测试
```

## 测试

```bash
cd stml_py
PYTHONPATH="$PWD/src" python -m pytest tests/ -v
```

共 65 项测试：

| 测试文件 | 项数 | 内容 |
|----------|------|------|
| test_full_roundtrip.py | 21 | 7 组 TestData × 3 链（STML→JSON、JSON→STML→JSON、STML 往返幂等） |
| test_lexer.py | 12 | 缩进、引号、空值、注释、内联列表、多行字符串 |
| test_parser.py | 14 | 嵌套映射、序列、多文档、裸键、多行字符串 |
| test_serializer.py | 18 | null/string/map/list 序列化、JSON 输出、5 项往返 |

## 许可证

MIT
