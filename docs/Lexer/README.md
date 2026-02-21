# Lexer 模块文档

## 1. 概述

Lexer（词法分析器）模块负责将通过 `SourceManager` 加载的源代码文本字符流转换为 `Token`（词法单元）流。它是编译器前端的第一阶段，负责剥离空白字符和注释，并将其余文本聚合成对 Parser 有意义的基础构建块。

## 2. 核心数据结构

### 2.1 Token (`include/yuan/Lexer/Token.h`)

`Token` 是表示词法单元的轻量级对象，它自身不分配堆内存。
- **属性**:
  - `Kind`: Token 的种类，如关键字（`kw_func`, `kw_var`）、标识符（`identifier`）、字面量（`integer_literal`）、符号（`plus`, `lbrace`）等。
  - `Loc`: 该 Token 在源码中的起始位置 (`SourceLocation`)，用于后续的错误诊断。
  - `Length`: Token 文本的长度。
  - `Text`: 通过 `llvm::StringRef` 存储，直接指向 `SourceManager` 管理的源码缓冲区，实现零拷贝。
- **功能接口**:
  - 提供了一组高效的类型判定方法：`is(Kind)`, `isNot(Kind)`, `isOneOf(Kind...)`。

### 2.2 Lexer (`include/yuan/Lexer/Lexer.h`)

`Lexer` 是执行词法分析的核心类。
- **主要接口**:
  - `lex()`: 消耗当前缓冲区的字符，提取并返回下一个有效的 Token。
  - `peek(unsigned n = 1)`: 前瞻（Lookahead）后续第 n 个 Token 而不消耗它。这在 Parser 处理某些需要超前查看的语法（如泛型、闭包）时非常有用。
- **核心特性**:
  - **按需解析（Lazy Lexing）**: Token 是随着 Parser 的调用按需生成的，而不是一次性全部读入内存。
  - **Unicode 支持**: 支持 Unicode 标识符（如中文变量名/函数名）和宽字符字面量。
  - **字面量扩展支持**:
    - 原生字符串（Raw String）：`r"..."`，内部不处理转义。
    - 多行字符串：`"""..."""`。
    - 带有明确类型后缀的数值字面量：`100u8`, `3.14f32`。
  - **泛型解析辅助**: 提供 `splitGreaterGreater()` 等特殊接口。由于词法层面 `>>` 会被解析为右移操作符，当 Parser 在解析嵌套泛型如 `Vec<Vec<T>>` 遇到 `>>` 时，可调用该方法将其原位拆分为两个单独的 `>` Token。

### 2.3 LiteralParser (`include/yuan/Lexer/LiteralParser.h`)

`LiteralParser` 是一个从 `Lexer` 提取出的辅助工具类，专门用于在后续需要时提取和转换字面量内部的具体数值。
- **功能**:
  - `parseInteger`: 将整数 Token 的文本转换为实际的 `uint64_t` 值，并能正确处理各种进制前缀（`0x`, `0b`, `0o`）和类型后缀。
  - `parseFloat`: 解析浮点数文本。
  - `parseString`: 处理字符串内部的转义序列（如 `\n`, `\t`, `\uXXXX`），将其转换为实际的内存字节流。
  - `parseChar`: 解析字符字面量及其转义。

## 3. 词法规则与实现细节

具体的词法拆分规则定义在 `src/Lexer/Lexer.cpp` 的 `lexImpl()` 方法中：

- **空白符与注释**:
  - 自动跳过所有的空格、制表符、换行符（除非这些位于字符串字面量内部）。
  - 单行注释：以 `//` 开头，直到行尾。
  - 多行注释：被包裹在 `/*` 和 `*/` 之间。当前实现**不支持**嵌套多行注释。
- **标识符**:
  - 以字母或下划线或非 ASCII（`>= 0x80`）字符开头。
  - 紧跟零个或多个字母、数字、下划线或非 ASCII 字符。
  - 提取出标识符后，Lexer 会查一张静态哈希表，判断其是否属于语言保留关键字（Keyword），若是则转换其 `Kind`。
- **符号与操作符**:
  - 采用最大匹配（Maximal Munch）原则。例如 `==` 会被优先解析为相等操作符，而不是两个连续的 `=`。

## 4. 使用示例

在驱动层或工具链中通常这样使用 Lexer：

```cpp
#include "yuan/Lexer/Lexer.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Basic/Diagnostic.h"

// ... 初始化 sourceManager, diagEngine, fileID ...
Lexer lexer(sourceManager, diagEngine, fileID);

while (true) {
    Token token = lexer.lex();
    if (token.is(Token::Kind::eof)) {
        break;
    }

    // 打印或传递给 Parser 处理
    llvm::outs() << token.getName() << " '" << token.getText() << "'\n";
}
```
