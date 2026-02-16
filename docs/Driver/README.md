# Driver 模块文档

## 概述

Driver 模块是编译器的入口和协调中心。它负责解析命令行参数、初始化各个子模块（如 SourceManager, DiagnosticEngine）、并按照预定的顺序执行编译管线 (`Pipeline`)。

## 核心类

### CompilerOptions

`CompilerOptions` (`include/yuan/Driver/Options.h`) 封装了编译器的所有配置选项。

- **输入/输出**: `InputFiles`, `OutputFile`
- **输出模式**: `EmitMode`
    - `Tokens`: 仅做词法分析，输出 Token 流。
    - `AST`: 仅做语法分析，输出抽象语法树。
    - `IR`: 仅做代码生成，输出 LLVM IR。
    - `Object`: 编译为目标文件 (.o)。
    - `Executable`: 编译并链接为可执行文件 (默认)。
- **优化级别**: `OptLevel` (O0, O1, O2, O3)。
- **路径配置**: `IncludePaths`, `LibraryPaths`。

### Driver

`Driver` (`include/yuan/Driver/Driver.h`) 是编译过程的控制器。

#### 主要职责
1. **环境初始化**: 创建 `SourceManager`, `DiagnosticEngine` 等基础设施。
2. **多阶段执行**: 根据 `CompilerOptions` 的设定，按顺序调用 Lexer, Parser, Sema, CodeGen。
3. **错误处理**: 如果任一阶段包含严重错误，立即终止后续阶段并返回错误码。

#### 编译管线 (`run()` 方法)

```mermaid
graph TD
    Start[开始] --> Lexer[词法分析 (Lexer)]
    Lexer --> Parser[语法分析 (Parser)]
    Parser --> Sema[语义分析 (Sema)]
    Sema --> CodeGen[代码生成 (CodeGen)]
    CodeGen --> Link[链接 (Linker)]
    Link --> End[结束]

    Lexer -.->|出错| End
    Parser -.->|出错| End
    Sema -.->|出错| End
    CodeGen -.->|出错| End
```

## 扩展指南

如果需要添加新的编译阶段（例如优化 pass），通常需要在 `Driver::run()` 方法中插入相应的调用，并在 `CompilerOptions` 中添加对应的开关。
