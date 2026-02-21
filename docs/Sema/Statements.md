# Sema 模块文档：控制流、语句与模式匹配

本文档讲述 Sema 阶段对于非声明、非纯表达式的控制结构与语句（`Stmt`）的检查细节。

入口为 `Sema::analyzeStmt(Stmt* stmt)`。

## 1. 作用域语句与分支

### 1.1 块语句 (`BlockStmt`)
- 每进入一个花括号 `{}` 块，Sema 会创建一个 `Block` 级别的新作用域（压栈）。
- 依次调用 `analyzeStmt` 或 `analyzeExpr` 处理块内各条语句。
- 处理结束后退栈。**这一行为也是为什么全局词法分析无法查询局部变量的原因**，任何针对局部范围的数据抓取，必须像 LSP 那样拦截运行时的作用域，或者依靠写回 AST 的信息。

### 1.2 If 分支 (`IfStmt`)
- `if` 关键字后的条件（`Condition`）必须是可以被推断出并且完全匹配 `bool` 类型的表达式。不允许诸如 `int` 到 `bool` 的 C 风格隐式转换。
- 对于 `if ... else if ... else` 的多重分支，分别进入不同的局部作用域进行隔离分析。

## 2. 循环与跳转控制

### 2.1 While 与 Loop (`WhileStmt`, `LoopStmt`)
- 在进入这类循环体之前，Sema 会在符号表/作用域链中开辟一种特殊属性的作用域（标志其为 `Scope::Loop`）。
- **作用域标签 (`Label`)**: 如果 `for 'a` 或 `loop 'outer` 带有了标签，该标签会被注入当前循环的作用域中，为后续的具名跳转做准备。
- `while` 的条件强制检查是否为 `bool`。

### 2.2 For 迭代 (`ForStmt`)
`for x in iterable` 是一个高度重载的语法糖，Sema 对其有一套复杂的探测协议：
1. 分析 `iterable` 表达式的类型。
2. **内建迭代协议检查**: 如果类型是数组 `[T; N]`、切片 `[T]`、字符串 `str`、特定 `Range<T>` 或者元组变长数组，Sema 会内置提取出其元素类型 `T`。
3. **Trait 迭代协议检查**: 如果是非内建类型，Sema 会去查 `ImplTraitMap` 看其是否实现了 `Iterator` Trait（也就是检查是否具备合法的 `iter()` 关联方法返回一个包含 `next()` 方法的对象）。
4. 解析出 `T` 后，将循环变量（如上述的 `x`）以类型 `T` 注入到 For 语句的内部作用域中。如果 `iterable` 推断失败，则产生 `err_not_iterable`。

### 2.3 Break 与 Continue (`BreakStmt`, `ContinueStmt`)
这两条语句主要进行拓扑上下文的合法性验证：
- Sema 会从当前作用域开始向上溯源，寻找最近的 `Loop` 作用域。
- 如果找不到，说明 `break` 暴露在循环外部（比如直接写在了顶级函数体或者普通的 if 块里），产生 `err_break_outside_loop`。
- 如果语句带有特定的跳转标签（如 `break 'label`），则必须查找到被此名字标注的合法层级的作用域。

### 2.4 延迟执行 (`DeferStmt`)
- `defer` 用于资源的后入先出清理（类似 Go）。
- 它的内部可以接任意的合法语句。
- 语义阶段的检查仅限于确认该语句位于函数体内的合法位置，不负责实际的执行展开，后续 LIFO 机制的代码生成由 CodeGen 利用 `DeferStack` 来实现。

## 3. 模式匹配 (`MatchStmt` / `MatchExpr`)

`match` 是 Yuan 中极其核心的动态分发解构语法。Sema 需要对 `match` 进行极其严格的**穷尽性检查**（Exhaustiveness Checking）。

### 3.1 结构与作用域
- `match` 包含一个审查对象（Scrutinee）和若干个分支（Arms）。
- 每个分支包含一个模式（`Pattern`）以及其执行体（`Body`）。
- 对每一个分支，Sema 开辟新的 `Block` 作用域。

### 3.2 模式绑定与验证
- `analyzePattern(Pattern* pat, Type* expectedType)`：Sema 会把被匹配的对象的类型（`expectedType`）从上到下推给 `Pattern`。
- 如果 `Pattern` 是一个标识符，它将被看作是一个新的局部变量（Variable Binding）注入作用域，其类型等于 `expectedType` 的某个子集或本体。
- 如果 `Pattern` 是针对 `Enum` 变体的解构，Sema 将验证：
  1. 这个变体确实属于被审查类型（`expectedType`）的范围内。
  2. 解构的参数个数和具体类型，与枚举定义时的负载（Payload）类型一一对应。若匹配，则把解构出来的内部变量注入到当前分支的作用域内。
- （可选的）如果有 guard 子句 `if cond`，则随后在这个已经完成绑定的作用域里检查该条件必须是 `bool`。

### 3.3 穷尽性检查 (Exhaustiveness)
分析完所有分支后，Sema 会触发内部的 `checkExhaustive` 逻辑来保证代码的完备安全性：
- **布尔型 (`bool`)**: 必须显式覆盖 `true` 和 `false`（或者使用 `_` 兜底）。
- **可选型 (`?T`)**: 必须覆盖 `Some(...)` 和 `None`。
- **枚举型 (`Enum`)**: 会收集所有的已匹配分支标记，和目标枚举所有的变体成员做一个减法集合对比。如果存在未被提及的枚举成员，并且没有使用 `_` (Wildcard) 或者标识符进行兜底拦截，Sema 必报 `err_match_not_exhaustive`，并友好地在诊断信息里列出漏掉的变体名字。
- **其他开放类型 (如 `int`, `str`)**: 这类值域近似无限的类型，强制要求开发者必须写出 `_ => ...` 或者绑定到一个无关变量作为兜底处理，否则无法通过编译。

## 4. 返回值验证 (`ReturnStmt`)
- 根据当前所处的函数签名（从当前层层退栈查找到 `Function` 作用域，并取其声明时的 `ReturnType`），验证 `return expr` 的 `expr` 类型。
- 调用通用的 `checkTypeCompatible` 判断返回对象的兼容性，如果不兼容直接报错。
- 如果函数的签名带 `!`（具有 `canError` 属性），返回语句还必须与 `!T` 进行相容性打通。