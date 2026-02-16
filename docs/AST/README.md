# AST 模块文档

## 概述

AST (Abstract Syntax Tree) 模块定义了 Yuan 语言的抽象语法树结构。它是编译器的核心数据结构，用于表示源代码的语法结构。

AST 节点主要分为四大类：
1. **Decl (声明)**: 表示变量、函数、结构体等声明。
2. **Stmt (语句)**: 表示控制流、表达式语句等。
3. **Expr (表达式)**: 表示运算、调用、字面量等。
4. **TypeNode (类型)**: 表示源代码中的类型注解。

## 核心类

### ASTContext

`ASTContext` (`include/yuan/AST/ASTContext.h`) 是 AST 的上下文类，负责：
- **内存管理**: 所有 AST 节点都通过 `ASTContext` 分配，并由其统一管理生命周期。
- **类型工厂**: 提供并在内部缓存各种类型实例（如 `IntegerType`, `ArrayType` 等），确保同一类型在全局只有一份实例（Canonical Types）。
- **源文件管理**: 持有 `SourceManager` 的引用。

### ASTNode

`ASTNode` 是所有 AST 节点的基类，提供了：
- **源码范围 (`SourceRange`)**: 记录节点在源代码中的位置。
- **节点类型 (`Kind`)**:  用于 RTTI (Run-Time Type Information) 的类型标识。

## 声明 (Decl)

声明节点继承自 `Decl` 基类，定义在 `include/yuan/AST/Decl.h` 中。

| 类名 | 描述 | 示例 |
|------|------|------|
| `VarDecl` | 变量声明 | `var x: i32 = 10` |
| `ConstDecl` | 常量声明 | `const PI = 3.14` |
| `FuncDecl` | 函数声明 | `func add(a: i32, b: i32) -> i32 { ... }` |
| `ParamDecl` | 参数声明 | 函数参数 `x: i32` |
| `StructDecl` | 结构体声明 | `struct Point { x: i32, y: i32 }` |
| `FieldDecl` | 字段声明 | 结构体字段 `x: i32` |
| `EnumDecl` | 枚举声明 | `enum Color { Red, Green }` |
| `EnumVariantDecl` | 枚举变体 | `Red`, `Some(T)` |
| `TraitDecl` | Trait 声明 | `trait Show { func show() }` |
| `ImplDecl` | 实现块 | `impl Point { ... }` 或 `impl Show for Point { ... }` |
| `TypeAliasDecl` | 类型别名 | `type IntList = Vec<i32>` |

## 语句 (Stmt)

语句节点继承自 `Stmt` 基类，定义在 `include/yuan/AST/Stmt.h` 中。

| 类名 | 描述 | 示例 |
|------|------|------|
| `BlockStmt` | 块语句 | `{ stmt1; stmt2; }` |
| `DeclStmt` | 声明语句 | 包装 `Decl` 使其作为语句出现 |
| `ExprStmt` | 表达式语句 | `x + 1;` |
| `ReturnStmt` | 返回语句 | `return 0;` |
| `IfStmt` | 条件语句 | `if x > 0 { ... } else { ... }` |
| `WhileStmt` | While 循环 | `while x < 10 { ... }` |
| `LoopStmt` | 无限循环 | `loop { ... }` |
| `ForStmt` | For 循环 | `for i in 0..10 { ... }` |
| `MatchStmt` | 模式匹配 | `match x { 1 => ..., _ => ... }` |
| `BreakStmt` | 跳出循环 | `break` |
| `ContinueStmt` | 继续循环 | `continue` |
| `DeferStmt` | 延迟执行 | `defer file.close()` |

## 表达式 (Expr)

表达式节点继承自 `Expr` 基类，定义在 `include/yuan/AST/Expr.h` 中。

### 字面量
- `IntegerLiteralExpr`: 整数 (`123`)
- `FloatLiteralExpr`: 浮点数 (`3.14`)
- `BoolLiteralExpr`: 布尔值 (`true`, `false`)
- `CharLiteralExpr`: 字符 (`'a'`)
- `StringLiteralExpr`: 字符串 (`"hello"`)
- `NoneLiteralExpr`: 空值 (`None`)

### 运算与操作
- `BinaryExpr`: 二元运算 (`+`, `-`, `*`, `/`, `&&`, `||`, `==`, `..` 等)
- `UnaryExpr`: 一元运算 (`-`, `!`, `&`, `*` 等)
- `AssignExpr`: 赋值 (`=`, `+=` 等)

### 调用与访问
- `IdentifierExpr`: 标识符引用 (`x`, `MyClass`)
- `MemberExpr`: 成员访问 (`obj.field`)
- `CallExpr`: 函数调用 (`func(arg)`)
- `IndexExpr`: 索引访问 (`arr[0]`)
- `SliceExpr`: 切片操作 (`arr[1..5]`)
- `OptionalChainingExpr`: 可选链 (`obj?.prop`)

### 其他
- `IfExpr`: If 表达式 (`if cond { val1 } else { val2 }`)
- `BuiltinCallExpr`: 内置函数调用 (`@sizeof(T)`, `@import("mod")`)

## 类型注解 (TypeNode)

类型节点继承自 `TypeNode` 基类，定义在 `include/yuan/AST/Type.h` 中。这些节点表示**源代码中出现的类型语法**，不同于语义分析后的语义类型 (`yuan::Type`)。

| 类名 | 描述 | 示例 |
|------|------|------|
| `BuiltinTypeNode` | 内置类型 | `i32`, `bool`, `str`, `void` |
| `IdentifierTypeNode` | 命名类型 | `MyStruct`, `String` |
| `ArrayTypeNode` | 数组类型 | `[i32; 10]` |
| `SliceTypeNode` | 切片类型 | `&[u8]` |
| `TupleTypeNode` | 元组类型 | `(i32, f64)` |
| `PointerTypeNode` | 指针类型 | `*mut i32` |
| `ReferenceTypeNode` | 引用类型 | `&i32` |
| `FunctionTypeNode` | 函数类型 | `func(i32) -> i32` |
| `GenericTypeNode` | 泛型类型 | `Vec<T>` |
| `OptionalTypeNode` | 可选类型 | `?i32` |
| `ErrorTypeNode` | 错误类型 | `!Result` |

## AST 遍历

AST 模块提供了 `ASTVisitor` 模式用于遍历语法树。用户可以通过继承 `ASTVisitor` 类并重写相应的 `visit*` 方法来实现对特定节点的处理。
