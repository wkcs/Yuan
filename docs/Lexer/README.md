# Lexer 模块文档

## 概述

Lexer (词法分析器) 模块负责将通过 SourceManager 加载的源代码文本转换为 Token 流。它是编译器的第一个阶段，为 Parser 提供输入。

## 核心类

### Token

`Token` (`include/yuan/Lexer/Token.h`) 表示一个词法单元。

- **属性**:
    - `Kind`: Token 类型 (关键字, 标识符, 字面量, 符号等)。
    - `Loc`: 在源码中的起始位置 (`SourceLocation`)。
    - `Text`: 对应的文本内容。
- **功能**: 提供了判断 Token 类型的方法 (`is`, `isNot`, `isOneOf`)。

### Lexer

`Lexer` (`include/yuan/Lexer/Lexer.h`) 是词法分析的核心类。

- **主要接口**:
    - `lex()`: 消耗并返回下一个 Token。
    - `peek(n)`: 查看后续第 n 个 Token 而不消耗。
- **特性**:
    - **零拷贝**: 尽量直接使用源码缓冲区的指针，减少字符串拷贝。
    - **Unicode 支持**: 支持 Unicode 标识符和字符字面量。
    - **处理复杂字面量**:
        - 原生字符串 (`r"..."`)
        - 多行字符串 (`"""..."""`)
        - 数值后缀 (`100u8`, `3.14f32`)
    - **泛型解析辅助**: `splitGreaterGreater()` 方法用于将 `>>` 拆分为两个 `>`，以正确解析嵌套泛型 (如 `Vec<Vec<T>>`)。

### LiteralParser

`LiteralParser` (`include/yuan/Lexer/LiteralParser.h`) 是一个工具类，用于解析字面量的具体数值。

- **功能**:
    - `parseInteger`: 解析整数，支持不同进制 (0x, 0b, 0o) 和类型后缀。
    - `parseFloat`: 解析浮点数。
    - `parseString`: 解析字符串，处理转义序列。
    - `parseChar`: 解析字符字面量。

## 使用示例

```cpp
Lexer lexer(sourceManager, diagEngine, fileID);

while (true) {
    Token token = lexer.lex();
    if (token.isEOF()) break;
    
    // 处理 token...
}
```

## 词法规则

词法规则定义在 `Lexer::lexImpl()` 中。
- **空白**: 跳过空格、制表符、换行符。
- **注释**:
    - 单行注释: `// ...`
    - 多行注释: `/* ... */` (不支持嵌套)
- **标识符**: 以字母或下划线开头，后接字母、数字或下划线。
