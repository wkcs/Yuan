# Sema 模块文档：符号表、作用域与模块管理

本文档讲述 Yuan 编译器在语义分析阶段如何建立和使用基于词法的环境上下文，确保名字在被引用时总是能被正确找到或者被正确隐藏（Shadowing）。

## 1. 作用域层级设计 (`Scope`)

`Scope` (`include/yuan/Sema/Scope.h`) 表示一段独立受保护的代码空间。每一个 `Scope` 持有它所声明的局部符号字典（`std::unordered_map<std::string, Symbol*>`），并含有一个指向父作用域的指针，构成了“向外冒泡”查找的经典词法作用域链。

`Scope` 包含一个 `Kind` 属性，用于界定此作用域的特性和它能承载的逻辑限制：
- **`Global`**: 全局作用域。整个编译单元的根节点。这里通常放置顶层的 `func`、`struct`、`trait`，以及内建的模块别名（如 `std`）。
- **`Module`**: 对应于单独导入的文件或库，内部隔离了外部的视线，仅当作为成员访问时可穿透。
- **`Function`**: 函数体作用域。这层作用域持有函数所有的参数变量。当退出此作用域时，代表一个函数的语义检查全部结束。
- **`Block`**: 由花括号 `{}` 产生的常规代码块。主要用来约束 `var` 和 `let` 变量的存活期。
- **`Loop`**: 用于承载 `for`, `while`, `loop` 逻辑。它附带记录了当前循环的入口或标签。`break` 和 `continue` 语句在执行时，只能搜索到具有 `Loop` 特性的作用域并建立拓扑跳转。
- **`Struct` / `Enum`**: 定义阶段的作用域。对于枚举，这里存放 `Variant` 成员；对于结构体，主要约束字段可见性。
- **`Trait` / `Impl`**: 注入上下文对象（如 `Self` 泛指代词）和特质方法的区域。

## 2. 符号表管理 (`SymbolTable`)

`SymbolTable` 是整个 Sema 运转的信息存储总闸。

### 2.1 插入与遮蔽 (Shadowing)
调用 `insert(name, symbol)` 会将此符号挂在当前活动的最近一个 `Scope` 内。
- **作用域内防重**: 如果当前作用域（并且仅限当前作用域，不向父级查找）中已存在 `name`，则会抛出“重定义”错误。
- **合法的 Shadowing**: 如果外层（父作用域）存在同名变量 `x`，在当前的 `Block` 作用域内声明了新的 `let x`，因为上述防重规则只检查当前作用域，因此新的 `x` 被合法存入，并在后续向下引用的过程中优先被查找到，这就是合法的“变量遮蔽”。

### 2.2 查找机制 (`lookup`)
- 从 `getCurrentScope()` 开始，逐步向 `getParent()` 向上索溯。
- 第一个被找到的同名 `Symbol` 就是引用目标（返回该指针）。
- 这种机制自动解决了内外层作用域的就近屏蔽问题。
- 由于 `lookup()` 强依赖于当前树遍历的层级，一旦完成了对某个 `Block` 乃至 `Function` 的 `analyze()`，Sema 会触发 `exitScope()` 退栈销毁内部信息。这也是为什么全局的 `lookup` 无法在编译后直接提供 LSP 局部变量跳转的原因，LSP 必须通过前置缓存（存在 `ASTNode` 里）或者独立的 `ASTVisitor` 爬树重构来获取数据。

## 3. 符号模型 (`Symbol`)

每个 `Symbol` (`include/yuan/Sema/Symbol.h`) 被绑定在名称上，它不仅包含了名字，还包含：
- **`SymbolKind`**: 符号类别（`Variable`, `Constant`, `Function`, `Struct`, `EnumVariant`, `TypeAlias` 等等）。这让 LSP 能够很轻易地将结果转译为 IDE 中的图标。
- **`Type*`**: 这个符号被推断出或者显式标记的、全局唯一的语义类型。
- **`SourceLocation`**: 此符号当初是在哪一行、哪一列被定义出来的，极其重要，是 `Definition` 跳转的核心基石。
- **可变标志 (`IsMutable`)**: `true` 代表可通过赋值运算符重写该内存，`false` 代表它是只读的绑定。
- **可见性 (`Visibility`)**: `pub`（公开）或默认私有。

## 4. 模块系统与依赖解决 (`ModuleManager`)

位于 `src/Sema/ModuleManager.cpp`。

当 Sema 遇到一个 `@import("std.fmt")` 或者对外部库的引用时：
1. **防循环加载**: 会维护一个正在 `ImportChain`，若探测到循环引用，立即报出编译错误，防止解析死循环。
2. **包查找定位**:
   - 它先检查 `std`，这会引导它去读取项目路径里的 `stdlib/` 或编译期配置的系统库目录。
   - 然后通过 `.` 换 `/`，寻找对应的 `.yu` 源文件（如 `std/fmt.yu`）。
   - 如果启用了预编译缓存（`.ymi`，Yuan Module Interface，类似 C++20 的 BMI 或 Swift 的 rmeta），它将反序列化 `.ymi` 直接把里面包含的公有导出（`pub` 符号）塞进一个新的 `ModuleType` 里，而不需要重复跑一遍完整的 AST 和 Sema。
3. **注入符号**: 将得到的 `ModuleType` 返回，由于 `ModuleType` 自身含有 `getExportedSymbols`，当外界代码对模块执行成员访问（如 `m.func1`）时，便可以从此处去查。