# Parser 模块文档

## 概述

Parser (语法分析器) 模块负责将 Lexer 产生的 Token 流转换为 AST (抽象语法树)。这是编译器的核心前端组件之一。

## 核心类

### Parser

`Parser` (`include/yuan/Parser/Parser.h`) 是语法分析的主类。

#### 解析策略
Yuan 语言的解析器采用了混合策略：
1. **递归下降解析 (Recursive Descent)**: 用于处理声明 (Decl)、语句 (Stmt) 和类型 (Type)。这种方法直观且易于手工实现。
2. **Pratt 解析 (Pratt Parsing)**: 专门用于处理表达式 (Expr)。Pratt 解析器（也称为 "Top-Down Operator Precedence Parsing"）能非常优雅且高效地处理复杂的运算符优先级和结合性问题。

#### 主要接口
- `parseCompilationUnit()`: 解析整个编译单元，这是解析的入口点。
- `parseDecl()`: 解析声明（如 `var`, `func`, `struct`）。
- `parseStmt()`: 解析语句（如 `if`, `while`, `return`）。
- `parseExpr()`: 解析表达式。
- `parseType()`: 解析类型注解。

### ParseResult

`ParseResult<T>` (`include/yuan/Parser/ParseResult.h`) 是一个简单的结果包装器（类似于 Rust 的 `Result<T, ()>` 或 `Option<T>`），用于表示解析操作的成功或失败。
- 如果解析成功，它包含指向生成的 AST 节点的指针。
- 如果解析失败（遇到语法错误），它不包含值，并标志着错误状态。

## 错误恢复 (Error Recovery)

为了在遇到语法错误时能够继续发现后续的错误，Parser 实现了错误恢复机制（Synchronization）。

当 `expect()` 或其他断言失败时，Parser 会尝试“同步”到已知的安全点。
- `synchronize()`: 跳过 Token 直到遇到语句结束符 (`;`) 或块边界。
- `synchronizeTo(TokenKind)`: 跳过直到遇到特定 Token。

这防止了“级联错误”，即一个语法错误导致后续大量无关的报错。

## 表达式解析细节

表达式解析使用 `parseExprWithPrecedence(int minPrec)` 方法。它根据当前运算符的优先级（`Binding Power`）决定是继续吞噬 Token 还是结束当前表达式的解析。相关的优先级定义在 `Parser::getOperatorPrecedence` 中。
