# Parser 模块文档

## 1. 模块定位

Parser (语法分析器) 模块是编译器前端的核心组件，位于 Lexer 和 Sema 之间。它的职责是消费 Lexer 产生的 Token 流，根据 Yuan 语言的文法规则，构建出等价的树状数据结构——抽象语法树（AST）。

核心实现文件：
- `include/yuan/Parser/Parser.h`
- `include/yuan/Parser/ParseResult.h`
- `src/Parser/Parser.cpp` （整体框架与辅助函数）
- `src/Parser/ParseDecl.cpp` （声明解析）
- `src/Parser/ParseStmt.cpp` （语句解析）
- `src/Parser/ParseExpr.cpp` （表达式解析，基于 Pratt Parsing）
- `src/Parser/ParseType.cpp` （语法类型注解解析）

## 2. 核心架构与解析策略

`Parser` 采用了混合解析策略：

1. **递归下降解析 (Recursive Descent)**：
   - 绝大多数的语法构造（如模块顶层结构、声明 Decl、语句 Stmt 和语法类型 TypeNode）都采用传统的递归下降方式进行解析。
   - 每个非终结符（Non-terminal）都对应一个 `parseXXX()` 方法，方法内部通过不断检查当前 Token 的种类 (`peek().is(...)`) 来决定如何消费 Token (`consume()`) 并下降到对应的子结构。

2. **普拉特解析法 (Pratt Parsing / Top-Down Operator Precedence)**：
   - 专门用于处理表达式（Expr）。
   - 由于表达式充斥着各种左结合、右结合以及具有复杂优先级（Precedence / Binding Power）的中缀、前缀和后缀运算符，Pratt Parsing 能够极其优雅且高效地在单个遍历函数（`parseExprWithPrecedence(int minPrec)`）中解决所有运算符优先级冲突。
   - 中缀和前缀的关联逻辑分散在 `parsePrefix()` 和 `parseInfix()` 中，优先级常量定义在 `getOperatorPrecedence` 方法中。

## 3. 核心 API 接口

`Parser` 类的公共入口主要有：
- `parseCompilationUnit()`: 解析整个编译单元（通常对应一个源码文件）。这是启动解析过程的总入口，返回一个顶级声明的数组。
- `parseDecl()`: 尝试解析声明（如 `var`, `func`, `struct`, `trait`, `impl` 等）。
- `parseStmt()`: 尝试解析语句（如 `if`, `while`, `for`, `match`, `return`, `defer`，以及作为语句的表达式）。
- `parseExpr()`: 解析表达式，它是 `parseExprWithPrecedence(0)` 的门面包装。
- `parseType()`: 解析语法类型注解，返回 `TypeNode*` 体系的节点。

## 4. `ParseResult<T>` 结果包装器

为了支持平滑的错误传播和恢复，所有的 `parseXXX` 方法通常不直接返回原始指针，而是返回一个泛型的 `ParseResult<T>`。
- 这是一个简单的包装器，类似于 Rust 的 `Result<T, ()>` 或 `Option<T>`。
- **解析成功**：包含生成的 AST 节点指针。
- **解析失败**：内部为空，并标记当前状态为失败，同时向 `DiagnosticEngine` 发射错误诊断信息。
- 提供 `isError()`, `get()`, `getLoc()` 等方法。如果外部检测到失败，可以立即中止解析或者选择进入错误恢复流程。

## 5. 错误恢复机制 (Error Recovery)

为了防止语法错误引起级联报错（Cascade Errors），Parser 内部必须实现健壮的同步机制（Synchronization）。

当 `expect(TokenKind)` 断言失败，或者某个语法结构的格式明显错误时，Parser 将：
1. 发射包含准确位置和期望内容的错误诊断。
2. 调用同步方法：
   - `synchronize()`: 循环丢弃 Token，直到遇到语句结束符（如分号 `;`）或块的边界符号（如 `}`）。
   - `synchronizeTo(TokenKind)`: 跳过并丢弃 Token，直到匹配到指定的同步点 Token。
3. 恢复解析器的正常状态，然后尝试继续解析下一个声明或语句。

这一机制确保即使代码中存在语法错误，Parser 也能尽量扫描完整个文件，一次性报告出尽可能多的真实错误。

## 6. 与 ASTContext 的交互

Parser 在解析成功后需要分配 AST 节点。它直接依赖于 `ASTContext` 提供的内存竞技场（BumpPtrAllocator）。
例如：
```cpp
auto* varDecl = Ctx.create<VarDecl>(range, name, type, init, isMut, vis, pattern);
```
通过这种方式分配的 AST 节点不需要手动 `delete`，它们的生命周期与 `ASTContext` 绑定，在编译单元处理结束后统一释放，这极大地提升了 Parser 的性能。
