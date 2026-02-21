# Driver 模块文档

## 1. 模块定位

Driver (编译驱动器) 是整个编译器的主控和调度中心。它的职责类似于 GCC 或 Clang 的驱动程序：解析用户的命令行参数，组装出合理的编译配置，初始化各个子系统（如内存管理、诊断引擎），并按照严格的顺序编排编译管线（Pipeline）。

核心实现文件：
- `include/yuan/Driver/Driver.h`
- `include/yuan/Driver/Options.h`
- `src/Driver/Driver.cpp`
- `tools/yuanc/main.cpp` (CLI 程序的入口点，构建 Driver 并调用 run)

## 2. 核心架构与设计

### 2.1 CompilerOptions (编译配置选项)

`CompilerOptions` 封装了编译器的所有外部输入配置。
- **输入/输出文件**: `InputFiles` (可能多个源文件), `OutputFile` (-o 指定的产物)。
- **输出模式 (DriverAction / EmitMode)**:
  - `-fsyntax-only`: 仅运行前端检查（词法、语法、语义），不进行代码生成。
  - `--emit=tokens`: 仅执行词法分析并打印 Token 流。
  - `--emit=ast` 或 `--emit=pretty`: 执行词法和语法分析，将 AST 打印到控制台。
  - `-S` 或 `--emit=llvm`: 走完代码生成阶段，输出 LLVM IR 文本（`.ll`）。
  - `-c`: 生成目标文件（Object File, `.o`）。
  - 默认：生成目标文件并调用外部链接器链接为可执行程序。
- **优化级别**: `OptLevel`（对应 `-O0`, `-O1`, `-O2`, `-O3`），用于指导 LLVM PassBuilder 的优化策略。
- **路径配置**: `-I` (`IncludePaths`) 用于头文件或模块搜索，`-L` (`LibraryPaths`) 用于链接时查找库。
- **标准库与包管理**: `--stdlib` 和 `--pkg-path` 设定标准库和第三方依赖的位置。

### 2.2 Driver (编译调度器)

`Driver` 类持有配置，并在 `run()` 方法中调度编译的各个阶段。

#### 主要职责：
1. **基础设施初始化**: 创建 `SourceManager` 用于管理源码文件映射，创建 `DiagnosticEngine` 用于统一收集和打印各阶段的错误/警告。
2. **多阶段串行执行**: 按照 `Lexer -> Parser -> Sema -> CodeGen` 的顺序调度。
3. **熔断与错误处理**: 在每一个阶段（如 Parser 结束、Sema 结束）之后，Driver 都会检查 `DiagnosticEngine` 的错误计数。一旦发现致命错误（Error 级别以上），编译管线会立即中断并返回非零错误码。
4. **子进程调用**: 当处于默认的生成可执行文件模式时，Driver 在 LLVM 生成 `.o` 文件后，会构造原生的 Shell 命令并调用系统的连接器（通常是 C 编译器如 `cc` 或 `clang`）来完成最终的链接。这通常会注入 `yuan_runtime.a`、`libcurl` 等运行时必须的静态库和动态库依赖。

#### 编译管线执行流 (`run()` 方法)

```mermaid
graph TD
    Start[解析命令行参数] --> Init[初始化基础组件 (SM, Diag)]
    Init --> Frontend[运行前端 runFrontend: Lexer -> Parser -> Sema]
    Frontend -.->|出错或仅检查模式| End
    Frontend --> CodeGen[运行后端 runCodeGeneration: CodeGen -> LLVM IR]
    CodeGen -.->|出错或输出 IR 模式| End
    CodeGen --> Compile[写入目标文件 Object File]
    Compile -.->|出错或只编译模式 -c| End
    Compile --> Link[调用外部链接器 linkObjects]
    Link --> End[结束并返回状态码]
```

## 3. 扩展指南

如果你需要为 Yuan 编译器添加新的全局阶段（例如，在 AST 上跑独立的静态借用检查器，或者在 LLVM IR 层面增加自定义的优化 Pass），你应该：
1. 在 `include/yuan/Driver/Options.h` 中添加控制该特性的命令行选项（Flag）。
2. 修改 `tools/yuanc/main.cpp` 中的参数解析逻辑。
3. 在 `src/Driver/Driver.cpp` 的 `run()` 或相应的子方法（如 `runFrontend`、`runCodeGeneration`）中适当的位置插入你的模块调用，并妥善处理它的错误返回。
