# STML Python 绑定库 — 打包与分发

## 三种使用方式

| 方式 | 适用场景 | 是否需要编译 |
|------|----------|-------------|
| 直接拷贝目录 | 开发试用、嵌入项目 | 否 |
| wheel 安装 | 正式使用、分发给他人 | 否 |
| 源码编译 | 开发贡献、切换 Python 版本 | 是（需 MinGW/MSVC） |

---

## 方式 1：直接拷贝目录（零安装）

`stml/` 目录是自包含的 Python 包，无需 pip、无需安装：

```
my_project/
├── stml/                        # 拷贝整个目录
│   ├── __init__.py
│   ├── _core.cp313-win_amd64.pyd  # 全静态链接，无外部 DLL 依赖
│   └── py.typed
└── your_script.py               # import stml 直接用
```

```python
# your_script.py
import stml
ast, warnings = stml.loads('"key": "value"\n')
```

**原理**：Python 将当前工作目录加入 `sys.path`，`stml/` 内的 `__init__.py` 使其成为包。`.pyd` 全静态链接，仅在系统 DLL 之外依赖 `python313.dll`。

**已验证**：拷贝到 `%TEMP%/test_stml_project/` 后，`import stml` 正常，`loads()`/`dumps()`/`to_json()` 全部可用。

**约束**：`.pyd` 绑定特定 Python 版本和平台。

| 标签 | 含义 | 当前值 |
|------|------|--------|
| `cp313` (python) | CPython 3.13 | 当前开发环境 |
| `cp313` (abi) | 含 C 扩展 | 有 .pyd |
| `win_amd64` (platform) | 64-bit Windows | MinGW 编译 |

换 Python 版本或操作系统需重新编译。

---

## 方式 2：wheel 安装

### 2.1 什么是 .whl

`.whl` 是 Python 标准分发包格式，本质是 ZIP 压缩包，按规定结构放置代码和元数据：

```
stml-1.3.0-cp313-cp313-win_amd64.whl     ← 就是一个 .zip
├── stml/
│   ├── __init__.py              (3.5 KB)
│   ├── _core.cp313-win_amd64.pyd (1.5 MB)
│   └── py.typed                 (0 B)
└── stml-1.3.0.dist-info/
    ├── METADATA                 # 包名、版本、许可证
    ├── WHEEL                    # 平台标签信息
    ├── RECORD                   # 各文件 SHA 校验（pip 装完后自动补）
    └── top_level.txt            # 顶层包名
```

#### 命名规则

```
stml  -  1.3.0  -  cp313  -  cp313  -  win_amd64  .whl
 包名     版本     python   abi       platform   扩展名
```

| 段 | 含义 | 本品值 | 纯 Python 包 |
|----|------|--------|-------------|
| python tag | 解释器版本 | `cp313` | `py3` |
| abi tag | 二进制接口 | `cp313`（含 C 扩展） | `none` |
| platform tag | OS+架构 | `win_amd64` | `any` |

纯 Python 包的 wheel 名为 `xxx-1.0-py3-none-any.whl`，跨版本跨平台。

### 2.2 构建 wheel

#### 标准方式（需构建环境完整）

```bash
# 方式 A：PEP 517 构建
python -m build --wheel

# 方式 B：setuptools 直接
python setup.py bdist_wheel
```

#### stml 当前方式（手动）

`python -m build` 创建独立 venv，丢失 MinGW 编译器。当前可手动打包已编译产物：

```python
import zipfile

wheel_name = "stml-1.3.0-cp313-cp313-win_amd64.whl"

with zipfile.ZipFile(f"dist/{wheel_name}", "w", zipfile.ZIP_DEFLATED) as zf:
    # 包文件
    for f in ["__init__.py", "py.typed", "_core.cp313-win_amd64.pyd"]:
        zf.write(f"src/stml/{f}", f"stml/{f}")

    # 元数据
    di = "stml-1.3.0.dist-info"
    zf.writestr(f"{di}/METADATA", "Name: stml\nVersion: 1.3.0\n...")
    zf.writestr(f"{di}/WHEEL", "Wheel-Version: 1.0\n...")
    zf.writestr(f"{di}/top_level.txt", "stml\n")
    zf.writestr(f"{di}/RECORD", "")

print(f"Wheel created: {wheel_name}")
# 产物 ~567 KB
```

**已验证**：手动构建的 wheel 用 `pip install` 安装成功，`pip show stml` 显示正常信息。

### 2.3 安装和使用

```bash
pip install dist/stml-1.3.0-cp313-cp313-win_amd64.whl
# Processing .../stml-1.3.0-cp313-cp313-win_amd64.whl
# Successfully installed stml-1.3.0
```

安装后，任意目录 `import stml` 可用：

```bash
cd $HOME
python -c "import stml; print(stml.loads('\"key\": \"value\"'))"
# ✓ 成功

pip show stml
# Name: stml   Version: 1.3.0   Location: .../site-packages
```

### 2.4 卸载

```bash
pip uninstall stml
```

### 2.5 分发给他人

把 `.whl` 文件发给对方，对方直接：
```bash
pip install stml-1.3.0-cp313-cp313-win_amd64.whl
```
前提：对方 Python 版本和平台匹配文件名标签。

### 2.6 上传 PyPI（目标）

```bash
twine upload dist/stml-1.3.0-*.whl
# 用户即可 pip install stml
```

---

## 方式 3：源码编译

#### 当前限制

`pip install .` 不可用。原因：setuptools 的 `build_py` 阶段对扩展源文件执行 `assert_relative()` 检查，使用 `os.path.isabs()` 验证。`../stml_cpp/` 下的 C++ 源文件被解析为绝对路径而拒绝。

**受影响命令**：`pip install .` / `pip install -e .` / `python setup.py install` / `python setup.py develop`

**不受影响命令**：`python setup.py build_ext`（不触发 build_py 路径验证）

### 3.1 前置条件

- Python ≥ 3.9（D:\SoftWares\DevKits\Miniconda3）
- MinGW-w64 GCC（D:\SoftWares\DevKits\mingw64_16.1.0）
- pybind11（`pip install pybind11`，已安装 3.0.4）

### 3.2 编译

```bash
cd stml_py

# 设置 MinGW 环境
export PATH="/d/SoftWares/DevKits/mingw64_16.1.0/bin:$PATH"
export CXX="g++"
export CC="gcc"

# 编译到 src/stml/ (--inplace)
python setup.py build_ext --compiler=mingw32 --inplace
```

产物：`src/stml/_core.cp313-win_amd64.pyd`

### 3.3 运行

```bash
# PYTHONPATH 方式
PYTHONPATH="$PWD/src" python -c "import stml; ..."

# 或编译后，直接拷贝 stml/ 目录到项目中（见方式 1）
```

### 3.4 测试

```bash
PYTHONPATH="$PWD/src" python -m pytest tests/ -v
# 65 项全部通过
```

---

## 构建参数说明

`setup.py` 中的关键配置：

```python
_ext = Pybind11Extension(
    "stml._core",
    [
        "../stml_cpp/lexer/lexer.cpp",        # 直接编译 C++ 源文件
        "../stml_cpp/parser/parser.cpp",      # （不链接 libstml.a）
        # ... 共 8 个 .cpp 文件
    ],
    include_dirs=["../stml_cpp"],             # stml.h 搜索路径
    cxx_std=17,
    extra_compile_args=["-std=c++17", "-O2", "-Wall"],   # GCC 标志
    extra_link_args=["-static-libgcc", "-static-libstdc++", "-static"],  # 全静态
)
```

### MinGW 特殊处理

1. **MSVC 标志剥离**：Pybind11Extension 在 Windows 自动添加 `/EHsc` `/bigobj`，需删除所有以 `/` 开头的编译参数
2. **编译器检测**：通过 `CXX` 环境变量判断当前使用 MinGW 还是 MSVC
3. **MCF 线程模型**：`-static` 消除 `libmcfgthread-2.dll` 运行时依赖

### 依赖验证

```bash
# 检查 .pyd 的 DLL 依赖
objdump -p src/stml/_core.cp313-win_amd64.pyd | grep "DLL Name"
# 输出：
#   DLL Name: KERNEL32.dll
#   DLL Name: msvcrt.dll
#   DLL Name: ntdll.dll
#   DLL Name: python313.dll
```

---

## 正式打包方案（待实现）

### 方案 A：修复 setuptools 路径（推荐）

将 C++ 源文件复制或符号链接到 `stml_py/cpp_src/` 下，避免 `../` 路径：

```
stml_py/
├── cpp_src/          # 复制自 ../stml_cpp/
│   ├── lexer/
│   ├── parser/
│   └── ...
├── setup.py
└── src/stml/
```

```python
# 构建脚本自动复制
import shutil
shutil.copytree("../stml_cpp", "cpp_src", dirs_exist_ok=True)
```

优点：`pip install -e .` 可用；缺点：需要维护拷贝逻辑。

### 方案 B：CMake + scikit-build-core

使用 scikit-build-core 替代 setuptools，直接链接 stml_cpp 构建产物：

```toml
# pyproject.toml
[build-system]
requires = ["scikit-build-core>=0.10", "pybind11>=2.12"]
build-backend = "scikit_build_core.build"
```

优点：与 C++ 项目构建一致；缺点：需要 CMake，增加复杂度。

### 方案 C：预编译 wheel（最实用）

CI 构建各平台 .pyd/.so，打包为 wheel 上传 PyPI：

```bash
# CI 流程
cd stml_cpp && cmake --build . --target stml    # 编译静态库
cd ../stml_py
python setup.py build_ext --compiler=mingw32     # Windows
python setup.py build_ext                        # Linux/macOS
python -m build --wheel                           # 打包
```

用户只需 `pip install stml`，无需编译器。

---

## wheel 结构（已验证）

```
stml-1.3.0-cp313-cp313-win_amd64.whl    567 KB
├── stml/
│   ├── __init__.py                 3.5 KB
│   ├── _core.cp313-win_amd64.pyd   1.5 MB（全静态链接）
│   └── py.typed                       0 B
└── stml-1.3.0.dist-info/
    ├── METADATA                      196 B
    ├── WHEEL                          87 B
    ├── RECORD                    （pip 装后自动生成）
    └── top_level.txt                   5 B
```

## 多平台支持

| 平台 | 编译器 | .pyd/.so | 注意事项 |
|------|--------|----------|----------|
| Windows (MinGW) | GCC 16.1.0 | .pyd | 需 `-static` 标志 |
| Windows (MSVC) | VS 2022+ | .pyd | Pybind11Extension 默认支持 |
| Linux | GCC 11+ | .so | 标准编译，无特殊处理 |
| macOS | Clang 15+ | .so | 需 `-stdlib=libc++` |

---

## 版本兼容性

| Python 版本 | ABI 标记 | 备注 |
|-------------|----------|------|
| 3.9 | cp39 | |
| 3.10 | cp310 | |
| 3.11 | cp311 | |
| 3.12 | cp312 | |
| 3.13 | cp313 | 当前开发环境 |
| 3.9-3.13 (abi3) | abi3 | 稳定 ABI，一个 .pyd 覆盖所有版本（可选） |

---

## 常见问题

### Q: 为什么不用 `pip install -e .`？

A: setuptools 对 `../` 路径验证过严。替代方案：`python setup.py build_ext --inplace` 编译，然后直接拷贝 `stml/` 目录到项目（方式 1）。

### Q: 怎么在没有编译器的电脑上用 stml？

A: 两种方式：
1. 拷贝 `stml/` 目录（含 .pyd）到项目目录，直接 `import stml`（Python 版本需匹配）
2. 安装预编译的 .whl：`pip install stml-1.3.0-cp313-cp313-win_amd64.whl`

### Q: .whl 文件能直接解压用吗？

A: 可以（就是 ZIP），但不推荐。`pip install` 会校验 RECORD 并放到 site-packages，卸载也方便。直接解压需手动处理路径和依赖。

### Q: 纯 Python 包和带 C 扩展的包，wheel 有什么区别？

A: 纯 Python 包 wheel 标签为 `py3-none-any.whl`，跨版本跨平台。stml 含 `.pyd`（C 扩展），标签含平台信息（`cp313-cp313-win_amd64`），一个 wheel 只适配一种 Python+OS+架构。

### Q: .pyd 体积为什么 1.5 MB？

A: C++ 模板展开 + 全静态链接（包含 libstdc++/libgcc 代码）。Release 版本 + LTO 可缩减到 ~500 KB。

### Q: 如何切换 MSVC 编译？

A: 不设置 `CXX=g++`，不加 `--compiler=mingw32`：

```bash
python setup.py build_ext --inplace
# Pybind11Extension 自动检测 MSVC
```
