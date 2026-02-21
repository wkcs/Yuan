# Sema 模块文档

## 1. 模块定位

Sema（Semantic Analysis）位于 Parser 与 CodeGen 之间，负责把“语法正确”的抽象语法树（AST）转化为“语义可执行”的 AST。它是整个前端最复杂、最核心的阶段。

由于其涵盖内容过多，详细的语义检查规则已拆分为以下几个子文档：

- [作用域、符号表与模块管理 (ScopeAndSymbol.md)](ScopeAndSymbol.md)
  详述：词法作用域管理、符号防止重定义、Shadowing、模块导入 (`ModuleManager`)。
- [语义类型系统与兼容性规则 (TypeSystem.md)](TypeSystem.md)
  详述：AST 类型与语义类型隔离、Canonical Types、兼容性适配、泛型参数推导。
- [声明、模块与宏观结构 (Declarations.md)](Declarations.md)
  详述：变量/常量解析、函数返回与尾调用保障、结构体/枚举结构解析、极其严格的 Trait 与 Impl 契约检验。
- [控制流、语句与模式匹配 (Statements.md)](Statements.md)
  详述：Block 作用域分配、If/While/Loop/For 逻辑及控制流验证、以及对 `match` 语法的核心级穷尽性检查 (Exhaustiveness)。
- [表达式类型推断与验证 (Expressions.md)](Expressions.md)
  详述：字面量类型推导、运算和借用符号 (`&mut`) 的验证、方法调用的签名解析以及错误处理机制 (`!` 与 `-> err`)。

## 2. 总体流程与契约

分析的入口点为：`Sema::analyzeDecl` / `Sema::analyzeStmt` / `Sema::analyzeExpr`。

一个典型的编译单元分析流程如下：
1. **重定义检查**：对顶层声明进行冲突检查。
2. **声明解析**：解析声明的类型签名，在全局符号表中建立符号记录。
3. **函数/Impl 深入分析**：针对函数体和 Impl 块，压入局部作用域，递归分析其内部的语句和表达式。
4. **错误恢复**：当分析失败时，通过 `DiagnosticEngine` 报告错误（如 `err_type_mismatch`），并尽可能继续分析后续节点，以发现更多潜在错误。

### 与 CodeGen 和 LSP 的契约

Sema 的核心输出结果不只用于报编译错误，还直接支撑后端和工具链。Sema 必须保证以下信息被正确、永久地写回到 AST 树上：

- `Expr::Type`：所有表达式（包括 `IdentifierExpr`, `MemberExpr` 等）必须拥有确定的静态类型。LSP 工具（如 Hover 悬停提示）依赖 ASTVisitor 直接读取此字段来呈现类型。
- `ResolvedDecl`：为标识符等关联其原始声明节点，支撑跳转到定义功能。
- `Decl::SemanticType`：声明节点自身的推导类型。
- **隐式变量声明**：模式绑定等产生的作用域隐式 `VarDecl`。

CodeGen 在生成 LLVM IR 时绝对信赖 Sema 写回的数据，不会再进行任何重复的语义推导。

## 3. 当前实现边界

- `TypeChecker.cpp` 作为一个独立文件的定位目前仍是占位符，绝大部分类型检查逻辑集中在 `Sema.cpp` 中。
- 一些高级静态分析功能（例如严密的控制流可达性分析、借用生命周期检查）尚未完全实现。
- 针对某些错误，Sema 会采用“记录错误但继续推断”（Error Recovery）的策略，这可能在复杂语法错误下引发级联诊断警告。

