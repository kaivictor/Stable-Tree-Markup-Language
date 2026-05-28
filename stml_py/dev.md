# STML Python 绑定库 — 开发日志

## 项目总览

将 stml_cpp（C++ 核心库）通过 pybind11 绑定为 Python 模块 `stml`。
架构：**表层 Python、内核 C++**。Python 层仅负责类型提示、dataclass、文档。

- **绑定技术**：pybind11 3.0.4
- **构建系统**：setuptools + Pybind11Extension
- **C++ 标准**：C++17
- **运行时依赖**：零（.pyd 全静态链接，仅系统 DLL）
- **Python 版本**：≥ 3.9
- **测试覆盖**：65 项（词法/语法/序列化/回归/往返）

---

## 2026-05-17 — 项目创建与完整实现

### 阶段 1：设计决策

**构建系统选择**：`setuptools + Pybind11Extension`

| 方案 | 理由 |
|------|------|
| scikit-build-core | 需要 CMake，对新手不友好 |
| py-build-cmake | 过于复杂 |
| **setuptools + pybind11** | pybind11 官方推荐，资料最多，~60 行 setup.py |

**DLL 处理**：全静态链接。MinGW-w64 的 MCF 线程模型依赖 `libmcfgthread-2.dll`，需额外 `-static` 标志（`-static-libgcc -static-libstdc++` 不够）。

**路径问题**：C++ 源文件在 `../stml_cpp/`，`pip install .` 的 `build_py` 验证会将其视为绝对路径（即使实际是相对路径，setuptools 解析后判定为绝对）。最终选择 `python setup.py build_ext --inplace` + `PYTHONPATH` 的工作流。

### 阶段 2：pybind11 绑定 (_core.cpp)

~220 行，绑定 6 个函数：

```cpp
PYBIND11_MODULE(_core, m) {
    m.def("loads",  ...);   // C++ LoadResult → Python (dict, list[tuple])
    m.def("load",   ...);
    m.def("dumps",  ...);   // Python object → AstNode → string
    m.def("to_json",...);
    m.def("tokenize",...);  // debug
    m.def("parse",  ...);   // debug
}
```

**AstNode ← Python 转换**：

| C++ | Python |
|-----|--------|
| `nullptr_t` | `None` |
| `string` | `str` |
| `AstList` | `list` (递归) |
| `AstMap` | `dict` (递归) |
| — | `int`/`float`/`bool` → stringify |

转换函数 `ast_to_python()` / `python_to_ast()` 递归处理嵌套结构。

**Warning 转换**：C++ `Warning{msg, line, col}` → Python `(msg, line, col)` tuple，再在 `__init__.py` 转为 dataclass。

### 阶段 3：Python 公有 API (\_\_init\_\_.py)

- `Warning` dataclass：`message: str`, `line: int`, `column: int`（frozen）
- 6 个公有函数，完整类型提示
- `_wrap_warnings()` 将 tuple 列表转为 `Warning` 对象列表

### 阶段 4：测试

#### 4.1 回归测试 (test_full_roundtrip.py) — 21 项

对 TestData 的 7 组数据运行三条链：

- **Chain A**：`STML → loads() → to_json() → compare(expected JSON)`
- **Chain B**：`expected JSON → dumps() → loads() → to_json() → compare(expected JSON)`
- **Chain C**：`STML → loads() → dumps() → loads() → dumps()` 幂等性

全部 7 组测试两组 STML 输出字节完全一致。

#### 4.2 单元测试 — 44 项

| 文件 | 项数 | 覆盖 |
|------|------|------|
| test_lexer.py | 12 | 缩进、键值、null/~、序列、引号键非贪婪、内联列表、多行字符串、注释、空输入、未闭合引号 |
| test_parser.py | 14 | 简单/嵌套映射、null/~、序列/含映射项序列、内联列表/null元素、多文档、空文档/空输入、多行字符串、裸键、警告 |
| test_serializer.py | 18 | null/string/转义/map/list/内联列表 序列化、to_json、5 项往返、tokenize→parse 往返 |

### 阶段 5：构建验证

最终构建流程：

```bash
cd stml_py
export PATH="/d/SoftWares/DevKits/mingw64_16.1.0/bin:$PATH"
export CXX="g++" CC="gcc"
python setup.py build_ext --compiler=mingw32 --inplace
```

产物 `src/stml/_core.cp313-win_amd64.pyd`（~1.5 MB，全静态链接）。

DLL 依赖仅：`KERNEL32.dll`, `msvcrt.dll`, `ntdll.dll`, `python313.dll`。

### 阶段 6：Standalone 包验证

验证 `stml/` 目录作为独立包，无需 pip 安装即可使用：

```bash
# 拷贝到任意目录
cp -r stml_py/src/stml /tmp/my_project/

# 从该目录直接 import
cd /tmp/my_project
python -c "import stml; print(stml.loads('...'))"
# ✓ 成功，无需 PYTHONPATH，无需 pip install
```

原理：Python 自动将当前工作目录加入 `sys.path`，`stml/` 内的 `__init__.py` 使该目录成为一个包。`.pyd` 全静态链接，不需要任何外部 DLL。

约束：`.pyd` 绑定特定 Python 版本/平台（文件名含 `cp313-win_amd64` 标签）。

### 阶段 7：Wheel 打包验证

`.whl` 本质是 ZIP 文件，内含包文件 + 元数据目录。手动构造并安装验证：

**构建**（手动，因为 `python -m build` 的隔离环境缺少 MinGW）：
```bash
# 用 zipfile 手动打包已编译的 .pyd + __init__.py + 元数据
python -c "
import zipfile
zf = zipfile.ZipFile('dist/stml-1.3.0-cp313-cp313-win_amd64.whl', 'w')
# 添加 stml/__init__.py, _core.*.pyd, py.typed
# 添加 stml-1.3.0.dist-info/METADATA, WHEEL, RECORD, top_level.txt
"
# 产物：567 KB
```

**安装**：
```bash
pip install dist/stml-1.3.0-cp313-cp313-win_amd64.whl
# Successfully installed stml-1.3.0
```

**验证**：从任意目录 `import stml` 成功，`pip show stml` 显示包信息。

Wheel 内部结构：
```
stml-1.3.0-cp313-cp313-win_amd64.whl
├── stml/
│   ├── __init__.py              (3.5 KB)
│   ├── _core.cp313-win_amd64.pyd (1.5 MB)
│   └── py.typed                 (0 B)
└── stml-1.3.0.dist-info/
    ├── METADATA
    ├── WHEEL
    ├── RECORD
    └── top_level.txt
```

Wheel 命名解析：`{name}-{version}-{python_tag}-{abi_tag}-{platform_tag}.whl`
| 段 | 本品值 | 含义 |
|----|--------|------|
| python_tag | `cp313` | CPython 3.13 |
| abi_tag | `cp313` | 含 C 扩展（非 pure Python） |
| platform_tag | `win_amd64` | 64-bit Windows |

纯 Python 包标记为 `py3-none-any.whl`。

---

## 关键设计决策

### 1. 不使用 CMake

虽然 stml_cpp 使用 CMake，Python 绑定选择 setuptools 直接编译源文件（不链接 .a），原因：
- 省去找静态库路径的麻烦
- pybind11 官方推荐
- `pip install -e .` 支持（理论）
- 与其他 Python 生态一致

### 2. 静态链接

MinGW-w64 的 MCF 线程模型产生 `libmcfgthread-2.dll` 依赖。`-static-libgcc -static-libstdc++` 无法覆盖此库，必须加 `-static`。这会连带静态链接 msvcrt，.pyd 体积增大但无运行时 DLL 依赖。

### 3. Pybind11Extension MSVC 标志冲突

在 Windows 平台，`Pybind11Extension.__init__` 无条件添加 `/EHsc` 和 `/bigobj`（MSVC 专用标志）。使用 MinGW 时这些标志导致编译失败。解决方案：创建后遍历 `extra_compile_args`，删除所有以 `/` 开头的参数。

### 4. pip install 路径验证

`pip install .` 在 `build_py` 阶段调用 `egg_info`，其产生的 `SOURCES.txt` 包含扩展源文件路径。`build_py.analyze_manifest()` 对每个路径调用 `assert_relative()`，使用 `os.path.isabs()` 检查。即使传入相对路径 `"../stml_cpp/stml.cpp"`，setuptools 在内部处理后将其视为绝对路径并拒绝。

受影响的命令：`pip install .`, `pip install -e .`, `python setup.py install`, `python setup.py develop`（后者内部调用 pip）。

不受影响：`python setup.py build_ext`（不触发 `build_py` 的全路径验证）。

### 5. Python 层的 Warning 包装

`_core.cpp` 返回 `list[tuple]` 而非 pybind11 绑定的 C++ struct，原因是：
- 避免复杂的内存管理（pybind11 class 需要注册、生命周期管理）
- tuple 到 dataclass 转换由纯 Python 完成，灵活且易维护
- 性能无影响（warning 数量极少）

---

## 与 C++ 库的关系

```
stml_cpp/                        stml_py/
├── lexer/lexer.cpp      ──→     setup.py 直接编译
├── parser/parser.cpp    ──→     (不链接 libstml.a)
├── ast/ast.cpp          ──→
├── serializer/*.cpp     ──→
├── diagnostics/*.cpp    ──→
├── stml.cpp             ──→
│                         ──→     src/stml/_core.cpp  (pybind11 绑定)
│                         ──→     src/stml/__init__.py (Python API)
```

C++ 头和测试独立于 Python 绑定，TestData 共享使用。

---

## 当前状态

```
版本: 1.3.0（Python 绑定 beta）
测试: 65/65 通过 ✅
往返幂等性: 已验证 ✅
回归: 7 组 TestData 全部通过 ✅
Standalone 包: 可直接拷贝目录使用 ✅
Wheel 打包: 已验证，pip install 可用 ✅
编译: MinGW-w64 GCC 16.1.0，pybind11 3.0.4
DLL 依赖: KERNEL32 / msvcrt / ntdll / python313（无其他依赖）
```

## 待完成

1. **自动化 wheel 构建**：修复 setuptools 路径问题，使 `python -m build --wheel` 可用
2. **多平台 CI**：Windows (MSVC + MinGW)、Linux、macOS 自动构建
3. **PyPI 发布**：上传预编译 wheel（abi3 标签可覆盖 3.9-3.13）
4. **VSCode 插件**：语法高亮、格式化、行内警告

## 参考

- `../stml_cpp/readme.md` — C++ 核心库文档
- `../stml_cpp/dev.md` — C++ 开发日志
- `../STML规范.md` — STML 语义规范
- `package.md` — 打包分发指南
