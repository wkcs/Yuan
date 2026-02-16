# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概览

Yuan 是一门基于 LLVM 的静态类型编译型语言，当前仓库包含：

- 编译器前端：Lexer / Parser / Sema
- 后端代码生成：CodeGen -> LLVM IR -> 目标文件/可执行文件
- 运行时：文件系统、线程、时间、HTTP、流式输出等
- 标准库与测试：`stdlib/`、`tests/`

## 常用命令

```bash
# Debug 构建
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# 编译
cmake --build build -j

# 运行全部测试
ctest --test-dir build --output-on-failure

# 运行单个语法/语义检查
./build/tools/yuanc/yuanc -fsyntax-only examples/snake_demo.yu

# 输出 Token / AST / Pretty
./build/tools/yuanc/yuanc --emit=tokens examples/snake_demo.yu
./build/tools/yuanc/yuanc --emit=ast examples/snake_demo.yu
./build/tools/yuanc/yuanc --emit=pretty examples/snake_demo.yu
```

## CMake 选项

- `YUAN_BUILD_TESTS`：默认 `ON`，构建测试
- `YUAN_BUILD_DOCS`：默认 `OFF`，构建文档

## 编译流程

`源文件(.yu) -> Lexer -> Parser -> Sema -> CodeGen -> LLVM IR -> Object/Link`

## 目录约定

- 头文件：`include/yuan/<模块>/`
- 实现：`src/<模块>/`
- 运行时：`runtime/`
- 标准库：`stdlib/`
- 测试：`tests/`

## 近期实现重点（与文档同步）

- `runtime/yuan_os.cpp`：基于 libcurl 的 HTTP GET/POST 扩展接口，支持 headers、timeout、stream。
- OpenAI 兼容流式输出：SSE `data:` 行解析，支持 `delta.content` / `message.content` / `text` 的提取与回退。
- `tests/yuan/stdlib/test_std_net_openai_chat.yu`：标准库 HTTP 的 OpenAI 兼容聊天示例。

## 代码风格

- C++ 使用 C++17。
- 保持现有命名风格与目录组织。
- 注释优先解释“为什么”，避免冗余描述。

## 语言与沟通

- 文档与协作默认使用中文。
