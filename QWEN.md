# Yuan 编译器项目 - QWEN.md

## 项目概述

Yuan 是一个基于 LLVM 的**静态类型编译型语言**实现。项目包含完整的编译器前端（词法、语法、语义分析）、LLVM IR 代码生成、运行时库、标准库以及测试工具链。

### 核心特性

- **静态类型系统**：编译时类型检查，支持 i8/i16/i32/i64/i128、u8/u16/u32/u64/u128、f32/f64、str 等类型
- **LLVM 后端**：生成 LLVM IR 并编译为机器码
- **模块系统**：支持 `@import` 模块导入和预编译包
- **Unicode 支持**：标识符支持完整 Unicode 字符集
- **错误处理**：基于 Error trait 的显式错误处理机制（无异常）
- **并发支持**：内置 async/await 协程语法
- **标准库**：IO、HTTP、线程、时间、GUI 等运行时能力

### 技术栈

- **语言**：C++17
- **构建系统**：CMake >= 3.16
- **后端**：LLVM
- **依赖**：libcurl（HTTP 功能）、GoogleTest（测试）

## 仓库结构

```
Yuan/
├── include/yuan/        # 公开头文件（AST、Sema、CodeGen、Driver 等）
├── src/                 # 编译器核心实现
│   ├── AST/             # 抽象语法树节点
│   ├── Basic/           # 诊断、源码管理、Token 基础设施
│   ├── Builtin/         # @ 内置函数实现
│   ├── CodeGen/         # LLVM IR 代码生成
│   ├── Driver/          # 命令行编译驱动
│   ├── Lexer/           # 词法分析
│   ├── Parser/          # 语法分析
│   └── Sema/            # 语义分析与类型检查
├── runtime/             # 运行时库（异步、FFI、格式化、OS、HTTP）
├── stdlib/              # 标准库 Yuan 源码
├── tools/
│   ├── yuanc/           # 编译器命令行入口
│   └── yuanfilt/        # 符号反修饰工具
├── tests/               # 单元测试、语言测试、stdlib 测试
├── docs/                # 模块级实现文档
│   ├── spec/            # 语言规范与约束
│   └── <模块>/          # 各子系统详细文档
├── examples/            # 示例代码
├── cmake/               # CMake 配置
└── third_party/         # 第三方依赖
```

## 构建与运行

### 依赖要求

- CMake >= 3.16
- C++17 编译器（Clang/GCC/MSVC）
- LLVM 开发库（由 `cmake/FindLLVM.cmake` 发现）
- libcurl（运行时 HTTP 能力）

### 构建命令

```bash
# Debug 构建
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j

# Release 构建
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### CMake 选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `YUAN_BUILD_TESTS` | ON | 构建测试 |
| `YUAN_BUILD_DOCS` | OFF | 构建文档 |

### 运行测试

```bash
ctest --test-dir build --output-on-failure
```

## 使用指南

### yuanc 编译器命令

```bash
# 语法/语义检查
./build/tools/yuanc/yuanc -fsyntax-only examples/snake_window.yu

# 输出 LLVM IR
./build/tools/yuanc/yuanc -S input.yu

# 生成目标文件
./build/tools/yuanc/yuanc -c input.yu

# 编译并链接可执行文件
./build/tools/yuanc/yuanc input.yu -o output

# 输出 Token / AST / Pretty
./build/tools/yuanc/yuanc --emit=tokens input.yu
./build/tools/yuanc/yuanc --emit=ast input.yu
./build/tools/yuanc/yuanc --emit=pretty input.yu
```

### 常用参数

| 参数 | 说明 |
|------|------|
| `-O0/-O1/-O2/-O3` | 优化等级 |
| `-I <path>` | 附加头文件搜索路径 |
| `--pkg-path <path>` | 预编译包搜索路径 |
| `--stdlib <path>` | 指定标准库根路径 |
| `--module-cache <path>` | 模块缓存目录（默认 `.yuan/cache`） |

## 编译流程

```
源文件 (.yu)
    ↓
Lexer (词法分析) → Token 流
    ↓
Parser (语法分析) → AST
    ↓
Sema (语义分析) → 类型检查 + 符号表
    ↓
CodeGen (代码生成) → LLVM IR
    ↓
LLVM 后端 → 目标文件 (.o)
    ↓
链接器 → 可执行文件
```

### 关键组件

1. **Driver**：读取输入并初始化 `SourceManager/DiagnosticEngine`
2. **Lexer**：产出 Token 流
3. **Parser**：构建 AST
4. **Sema**：建立符号表、解析类型并执行语义检查
5. **CodeGen**：将 AST 降低到 LLVM IR
6. **运行时**：提供文件系统、HTTP、线程等底层能力

## 开发规范

### 代码风格

- C++ 使用 **C++17** 标准
- 保持现有命名风格与目录组织
- 注释优先解释"为什么"，避免冗余描述
- 新增特性需同步更新对应文档

### 文档维护要求

- 修改语义规则、类型规则、IR 降低规则时必须同步更新对应模块文档
- 新增语法特性时，至少同时更新：Parser、Sema、CodeGen 文档和语言规范
- 文档描述以当前代码实现为准，不确定处应标注"当前实现/计划行为"

### 测试要求

- 行为变更请补充 `tests/yuan/` 或 `tests/unit/` 用例
- 新增运行时能力（I/O、网络）时，建议提供最小可运行示例

## 文档导航

| 文档 | 说明 |
|------|------|
| `README.md` | 项目概览、构建与使用 |
| `docs/README.md` | 文档索引 |
| `docs/spec/Yuan_Language_Spec.md` | 语言规范（2752 行完整规范） |
| `docs/stdlib.md` | 标准库实现文档 |
| `docs/<模块>/README.md` | 各子系统详细实现文档 |

## 版本信息

- **当前版本**：0.1.0
- **版本配置**：`cmake/Version.cmake`
- **版本头文件**：`build/include/yuan/Basic/Version.gen.h`（构建时生成）

## 沟通语言

- 文档与协作默认使用**中文**
- 代码、命令、路径等技术内容保持英文
