# AST 模块文档

## 1. 概述

AST (Abstract Syntax Tree) 模块定义了 Yuan 语言的抽象语法树结构。它是编译器的核心数据结构，作为前后端之间的标准媒介，用于表示源代码的层级语法结构。

AST 节点主要分为四大类（均继承自基类 `ASTNode`）：
1. **Decl (声明)**: 表示变量、函数、结构体、模块等声明构造。
2. **Stmt (语句)**: 表示控制流、执行动作及块级语句等。
3. **Expr (表达式)**: 表示会产生值的运算、函数调用、字面量、控制流表达式等。
4. **TypeNode (语法类型)**: 专门表示源代码中字面写出的类型注解部分。

## 2. 核心基础设施

### 2.1 ASTContext

`ASTContext` (`include/yuan/AST/ASTContext.h`) 是管理 AST 生命周期和全局状态的核心上下文类，承担以下重任：
- **内存分配竞技场 (Arena Allocation)**: 所有 AST 节点必须通过 `ASTContext::create<T>(...)` 分配。它底层使用 BumpPtrAllocator（或类似的内存池机制），因此 AST 节点无需单独释放。编译单元销毁时，竞技场统一释放所有内存，极大提升了编译器性能。
- **规范类型缓存 (Canonical Types)**: 提供并在内部缓存各种语义类型实例（如 `IntegerType`, `ArrayType`），确保同一签名在全局只有一份实例。
- **源码管理关联**: 持有并提供 `SourceManager`，便于节点进行坐标查询和诊断。

### 2.2 ASTNode 与 RTTI

`ASTNode` 是所有 AST 节点的基类，提供了：
- **源码范围 (`SourceRange`)**: 记录节点在源代码中的起始与结束坐标。
- **节点种类枚举 (`Kind`)**: 用于定制化的运行时类型信息（RTTI）。Yuan 不使用标准 C++ 的 `dynamic_cast`，而是依赖 `Kind` 和静态方法 `classof` 进行安全的 `static_cast`（或者直接使用 `node->getKind() == ASTNode::Kind::FuncDecl`）。

## 3. 声明体系 (Decl)

声明节点（`Decl` 家族，`include/yuan/AST/Decl.h`）表示绑定了标识符或引入新作用域的结构：

| 类名 | 描述 | 示例 |
|------|------|------|
| `VarDecl` | 变量声明 | `var x: i32 = 10` |
| `ConstDecl` | 常量声明 | `const PI = 3.14` |
| `FuncDecl` | 函数声明 | `func add(a: i32, b: i32) -> i32 { ... }` |
| `ParamDecl` | 参数声明 | 函数参数 `x: i32`, `mut self` |
| `StructDecl` | 结构体声明 | `struct Point { x: i32, y: i32 }` |
| `FieldDecl` | 字段声明 | 结构体内部字段 `x: i32` |
| `EnumDecl` | 枚举声明 | `enum Color { Red, Green }` |
| `EnumVariantDecl` | 枚举变体 | `Red`, 或带有负载的 `Some(T)` |
| `TraitDecl` | 接口/协议声明 | `trait Show { func show() }` |
| `ImplDecl` | 实现块 | `impl Point { ... }` 或 `impl Show for Point { ... }` |
| `TypeAliasDecl` | 类型别名 | `type IntList = Vec<i32>` |

## 4. 语句体系 (Stmt)

语句节点（`Stmt` 家族，`include/yuan/AST/Stmt.h`）表示不产生值的控制流和执行语句：

| 类名 | 描述 | 示例 |
|------|------|------|
| `BlockStmt` | 块语句 | `{ stmt1; stmt2; }` |
| `DeclStmt` | 声明语句包装 | 用于在块内包含一个 `Decl` |
| `ExprStmt` | 表达式语句包装 | `x + 1;` 或函数调用 |
| `ReturnStmt` | 返回语句 | `return val;` |
| `IfStmt` | 条件分支语句 | `if cond { ... } else { ... }` |
| `WhileStmt` | 循环语句 | `while cond { ... }` |
| `LoopStmt` | 无限循环 | `loop { ... }` |
| `ForStmt` | 迭代循环 | `for i in 0..10 { ... }` |
| `MatchStmt` | 模式匹配 | `match val { pattern => stmt, _ => ... }` |
| `BreakStmt` | 中断循环 | `break` 或 `break 'label` |
| `ContinueStmt` | 继续循环 | `continue` 或 `continue 'label` |
| `DeferStmt` | 延迟块执行 | `defer file.close()` |

## 5. 表达式体系 (Expr)

表达式节点（`Expr` 家族，`include/yuan/AST/Expr.h`）表示所有能求值的片段。Sema 阶段的核心任务之一就是为所有的 `Expr` 节点计算并填充其 `Type*`。

### 5.1 字面量
- `IntegerLiteralExpr`: `123`, `0xFF`, `100i64`
- `FloatLiteralExpr`: `3.14`, `1e-5f32`
- `BoolLiteralExpr`: `true`, `false`
- `CharLiteralExpr`: `'A'`, `'\n'`
- `StringLiteralExpr`: `"Hello"`, `r"raw"`
- `NoneLiteralExpr`: 表示空值的 `None`

### 5.2 运算与赋值
- `BinaryExpr`: `+`, `-`, `*`, `/`, `&&`, `||`, `==`, `..` (Range构建) 等
- `UnaryExpr`: `-`, `!`, `&` (借用), `*` (解引用), `&mut` (可变借用) 等
- `AssignExpr`: `=`, `+=`, `-=` 等赋值操作

### 5.3 访问与调用
- `IdentifierExpr`: 单纯的标识符引用，如变量名 `x`。
- `MemberExpr`: 成员访问，如 `obj.field` 或模块访问 `std.fmt`。
- `CallExpr`: 函数或方法调用，包含被调用的基底与参数列表。
- `IndexExpr`: 数组/切片等索引访问 `arr[0]`。
- `SliceExpr`: 范围切片 `arr[1..5]`。

### 5.4 复杂表达式
- `IfExpr`: 带有分支返回值的条件表达式 `let x = if cond { 1 } else { 0 }`。
- `MatchExpr`: 带有分支返回值的匹配表达式。
- `BuiltinCallExpr`: 编译器内部魔法方法调用，如 `@sizeof(T)`。
- `ErrorPropagateExpr`: 错误向上抛出 `expr!`。
- `ErrorHandleExpr`: 错误就地处理 `expr -> err { ... }`。

## 6. 类型注解 (TypeNode)

类型节点（`TypeNode` 家族，`include/yuan/AST/Type.h`）表示开发者在代码中书写的**字面类型签名**。
*注意：这不同于语义分析后附加到 AST 节点上的“语义类型”（`yuan::Type`）。*

| 类名 | 描述 | 示例 |
|------|------|------|
| `BuiltinTypeNode` | 内建类型关键字 | `i32`, `bool`, `str`, `void` |
| `IdentifierTypeNode` | 标识符类型引用 | `MyStruct`, `String` |
| `ArrayTypeNode` | 固定长度数组 | `[i32; 10]` |
| `SliceTypeNode` | 动态切片 | `[u8]` |
| `TupleTypeNode` | 元组 | `(i32, f64)` |
| `PointerTypeNode` | 裸指针 | `*mut i32`, `*i32` |
| `ReferenceTypeNode` | 引用 | `&i32`, `&mut i32` |
| `FunctionTypeNode` | 函数签名 | `func(i32) -> i32` |
| `GenericTypeNode` | 泛型实例化 | `Vec<T>` |
| `OptionalTypeNode` | 可选类型糖 | `?i32` |
| `ErrorTypeNode` | 错误传播类型糖 | `!Result` |

## 7. AST 遍历与访问者模式

Yuan 提供了基于 CRTP（Curiously Recurring Template Pattern）的静态多态访问者模式 `ASTVisitor` (`include/yuan/AST/ASTVisitor.h`)。
- **使用方法**: 开发者可以通过继承 `ASTVisitor<MyVisitor>` 并重写特定的 `visitXXX(XXX* node)` 方法来拦截对该类型节点的访问。
- **默认递归**: 基础的 `ASTVisitor` 内置了对子节点的向下递归分发。如果你重写了 `visitExpr` 并且希望继续遍历子节点，你需要显式调用 `ASTVisitor::visitExpr(expr)`。
- **应用场景**: 这被广泛应用于 LSP 的 Hover/Definition 定位（如 `HoverDefVisitor` 通过遍历坐标查找光标下的叶子节点）以及部分静态检查中。
