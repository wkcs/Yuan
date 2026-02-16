# Basic 模块文档

## 概述

Basic 模块提供了编译器所需的基础设施，主要包括源代码管理、诊断信息处理（错误/警告）、Token 定义等。这些组件被编译器的其他模块（如 Lexer, Parser, Sema）广泛使用。

## 源代码管理

### SourceLocation

`SourceLocation` (`include/yuan/Basic/SourceLocation.h`) 用于表示源代码中的位置。
- 使用 32 位整数 (`Offset`) 紧凑地表示位置。
- 该偏移量是相对于所有被加载源文件的起始位置的全局偏移。
- 提供了比较运算符和有效性检查方法。

### SourceRange

`SourceRange` 表示源代码中的一个连续范围，由起始位置 (`Begin`) 和结束位置 (`End`) 组成。

### SourceManager

`SourceManager` (`include/yuan/Basic/SourceManager.h`) 负责管理所有的源代码文件。
- **文件加载**: 从磁盘加载源文件或从内存创建缓冲区。
- **位置转换**: 将 `SourceLocation` 转换为文件名、行号和列号。
- **内容获取**: 获取特定位置或范围的源代码内容。
- **FileID**: 为每个加载的文件分配唯一的 ID。

## 诊断系统 (Diagnostics)

诊断系统用于报告编译过程中的错误、警告和提示信息。设计参考了 LLVM/Clang 的诊断系统。

### DiagnosticEngine

`DiagnosticEngine` (`include/yuan/Basic/DiagnosticEngine.h`) 是诊断系统的核心枢纽。
- **报告接口**: 提供 `report()` 方法用于创建一个 `DiagnosticBuilder`。
- **消费者管理**: 管理 `DiagnosticConsumer`，将构建好的诊断信息分发给它。
- **统计**: 跟踪错误和警告的数量。
- **配置**: 支持将警告视为错误 (`WarningsAsErrors`) 和设置错误上限 (`ErrorLimit`)。

### Diagnostic

`Diagnostic` 表示一条具体的诊断信息。
- **属性**: 包含 ID、级别 (Ignored, Note, Warning, Error, Fatal)、位置。
- **参数**: 支持字符串、整数等多种类型的参数注入。
- **高亮**: 支持附加 `SourceRange` 用于代码高亮。
- **Fix-It**: 支持附加修复提示 (`FixItHint`)，用于自动修复代码错误。

### DiagnosticConsumer

`DiagnosticConsumer` 是接收和处理诊断信息的抽象基类。
- **TextDiagnosticPrinter**: (在其他文件中定义) 将诊断信息格式化并打印到终端。
- **StoredDiagnosticConsumer**: 将诊断信息存储在内存中，以便后续处理（如测试）。
- **MultiplexDiagnosticConsumer**: 将诊断信息分发给多个消费者。

## Token 定义

`TokenKinds.h` 定义了词法分析器使用的所有 Token 类型，包括关键字、标点符号、字面量类型等。
