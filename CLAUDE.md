# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概览与架构

Yuan 是一门基于 LLVM 的静态类型编译型语言。其架构遵循传统的编译器管线：

1. **前端 (Frontend)**:
   - `Lexer`: 将源码解析为 Token 流。
   - `Parser`: 构建抽象语法树 (AST)。
   - `Sema` (语义分析): 构建符号表、解析类型、执行类型检查以及处理 Trait/Impl 验证。这是前端最复杂的部分。
2. **后端 (Backend)**:
   - `CodeGen`: 将经过验证的 AST 降低 (Lower) 为 LLVM IR。
   - **Object/Link**: 将 IR 编译为目标文件并链接为可执行程序。
3. **运行时与标准库**:
   - `runtime/`: C++ 实现的底层接口，包括操作系统交互、文件 I/O、线程、时间、HTTP (基于 libcurl) 以及 GUI 适配器。
   - `stdlib/`: 使用 Yuan 语言编写的标准库 (`.yu` 文件)。

### 关键代码库结构

- `include/yuan/<Module>/`: 公开的 C++ 头文件 (如 `AST`, `Sema`, `CodeGen`, `Driver`)。
- `src/<Module>/`: 编译器的 C++ 核心实现。
- `tools/yuanc/`: 命令行编译器的主入口 (`main.cpp` 负责编排编译管线)。
- `tools/yuan-lsp/`: 语言服务器协议 (LSP) 的实现，用于提供 IDE 支持。
- `tools/vscode-yuan/`: Yuan 的 VSCode 插件客户端。
- `tests/`: 广泛的测试套件，使用 GoogleTest (C++ 单元测试) 和 lit 风格的语言行为测试。

## 开发工作流

### 构建命令

```bash
# 配置 Debug 构建环境
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# 编译项目 (包含编译器、运行时、工具和测试)
cmake --build build -j
```

### 运行编译器

编译器可执行文件生成在 `build/tools/yuanc/yuanc`。以下命令对调试语言特性非常有用：

```bash
# 仅运行语法和语义检查 (在 CodeGen 之前停止)
./build/tools/yuanc/yuanc -fsyntax-only examples/snake_demo.yu

# 检查编译的各个中间阶段
./build/tools/yuanc/yuanc --emit=tokens examples/snake_demo.yu
./build/tools/yuanc/yuanc --emit=ast examples/snake_demo.yu
./build/tools/yuanc/yuanc --emit=pretty examples/snake_demo.yu

# 输出 LLVM IR
./build/tools/yuanc/yuanc -S examples/snake_demo.yu
```

### 测试

测试由 CTest 管理。

```bash
# 运行完整的测试套件
ctest --test-dir build --output-on-failure

# 运行特定的测试套件或测试用例 (使用正则表达式过滤)
ctest --test-dir build -R "SemaTest" --output-on-failure
```

### VSCode 插件开发

```bash
cd tools/vscode-yuan
npm install
npm run compile
npx vsce package # 用于打包生成 .vsix 安装文件
```

## 贡献指南与约定

- **语言**: 文档、代码注释以及项目协作默认使用 **中文**。
- **C++ 标准**: 采用 C++17。
- **AST 节点**: 使用内部的 RTTI 系统 (`node->getKind()` 结合 `static_cast`，或 `Decl::classof()`)，不要使用 `dynamic_cast` 或 `llvm::dyn_cast` (后者仅保留给 LLVM IR 类型使用)。
- **文档维护**: 若修改了语义规则、类型规则或 IR 降低规则，必须同步更新 `docs/` 和 `docs/spec/` 中的相应文档。
- **测试要求**: 任何行为变更必须附带对 `tests/yuan/` 或 `tests/unit/` 中测试用例的更新。
