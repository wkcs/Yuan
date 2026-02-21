# Sema 模块文档：声明、模块与宏观结构

本文档聚焦于 Sema 对代码基本结构单元——声明（Declarations）及其包含的结构（如模块、结构体、枚举、接口与实现）的语义分析逻辑。

## 1. 全局与局部声明

分析的入口为 `Sema::analyzeDecl(Decl* decl)`，其内部根据 `decl->getKind()` 分发到各个具体的 `analyzeXXXDecl`。

### 1.1 变量与常量 (`VarDecl`, `ConstDecl`)
- **符号查重**: 任何声明首先会在当前最近的作用域（`SymbolTable::getCurrentScope()`）内检查是否已经存在同名符号（避免 Redeclaration）。
- **类型显隐推导**:
  - 如果提供了显式的语法类型注解（`TypeNode`），Sema 会调用 `resolveType` 获得 `Type*`，并强制后续的初始化表达式必须满足 `checkTypeCompatible`。
  - 如果未提供显式类型，Sema 必须通过分析初始化表达式（`InitExpr`）来推导类型。这要求 `var` 必须有初始值才能推断。
- **可变性登记**: `var` 默认带有 `mut` 标记（如果是 `let` 则是只读）；`const` 具有全局或模块生命周期只读特性，该标志会在 `Symbol` 中被打上标记，后续赋值表达式（`AssignExpr`）会来查询此标志。
- **模式解构 (`Pattern`)**: 如果 `VarDecl` 左侧不是简单的标识符，而是 Tuple 或 Struct 模式，Sema 需要解构右侧的类型，为每个模式叶子节点在其局部作用域中隐式注入带有正确类型的内部符号。

### 1.2 函数声明 (`FuncDecl`)
`analyzeFuncDecl` 是驱动函数合法性验证的核心方法：
1. **参数解析**:
   - 对每一个 `ParamDecl` 解析其类型。
   - 对特殊参数如 `self`、`&self`、`&mut self`，Sema 会验证其所在的上下文是否是合法的 `Impl` 块。
   - 对于 `...`（变长参数），会被转换为内部特有的 `VarArgs<T>` 签名。
2. **返回类型解析**:
   - 没有箭头 `->` 的函数会被隐式赋予 `void` 返回类型。
   - 解析是否有 `!` 标记，判定此函数是否具有 `canError` 属性。
3. **函数作用域压栈**:
   - 在开始分析函数体（`BlockStmt`）之前，Sema 会创建一个新作用域（`Function` 级别），并将所有参数以局部变量形式注入其中。
4. **尾返回控制与错误回收**:
   - Sema 遍历函数体，分析每一条语句。
   - 如果函数声明返回非 `void`，Sema 必须证明函数的每条控制流路径最终都以 `return` 或者以隐式尾表达式（Block的最后一句）收尾，且类型兼容。

## 2. 复合类型声明

### 2.1 结构体 (`StructDecl`)
- 注册结构体名称。
- 遍历 `FieldDecl`，为每一个字段执行 `resolveType`。
- 探测并拒绝自引用的无限大小嵌套（如在 `Node` 中直接包含 `Node` 而非 `*Node`）。
- 检查任何携带默认初始值的字段的类型正确性。

### 2.2 枚举 (`EnumDecl`)
Yuan 具有代数数据类型（Algebraic Data Types）风格的枚举。
- 为 `EnumDecl` 生成一个封闭的作用域。
- 对每个 `EnumVariantDecl`（变体），解析其可选的负载负载（Payload，如 `Unit`、`Tuple` 或带有字段的 `Struct` 样式）。
- 将每一个变体名字作为独立符号（类型为 `SymbolKind::EnumVariant`）注入到当前作用域或外层作用域中，方便后续的模式匹配和变体构造调用。

## 3. 抽象与实现 (Trait / Impl)

Yuan 的面向对象/多态机制基于 Trait（特质/接口）与 Impl（实现块）。Sema 必须严格把关实现与契约的匹配度。

### 3.1 Trait 声明 (`TraitDecl`)
- 记录 Trait 的名字。
- 将所有要求实现的方法签名（参数个数、名称、类型、是否要求可变借用等）固化下来。
- （内部机制）在 Trait 定义的上下文中，会自动注入一个名为 `Self` 的虚拟类型，代表未来实现该 Trait 的某个具体类型。

### 3.2 Impl 块 (`ImplDecl`)
这是连接具体类型与方法的核心。分两种情况：
**情况 A: 为类型实现自身方法 (`impl MyStruct`)**
- 解析目标类型（`resolveType`）。
- 开辟 `Impl` 作用域，并将所有的成员方法注册进去。
- 当开发者调用 `my_struct.func()` 时，Sema 会回退查找这个目标类型关联的所有内置方法。

**情况 B: 为类型实现特定 Trait (`impl TraitName for MyStruct`)**
- 这一步要求极其严格的校验机制：
  1. 解析目标类型和 Trait 名字，查找原 Trait 声明。
  2. 将目标类型与 Trait 绑定，把这条记录插入到全局的 `ImplTraitMap`（这是 `checkTraitBound` 的真理数据源）。
  3. 对比方法：Impl 块中定义的每一个函数，其名称和签名必须与 Trait 中定义的签名 **完全兼容**。包括但不限于 `Self` 的引用级别、参数长度、返回值是否有 `!` 错误传播属性。
  4. 完整性：必须实现 Trait 要求的所有方法，缺少任何一个则直接产生 `err_missing_trait_impl`。

## 4. 类型别名 (`TypeAliasDecl`)
处理形如 `type ID = i32` 的语句。Sema 会将其右侧立刻 `resolveType`。在使用别名的上下文里，Sema 会采用“结构等价”（Structural Typing）或者进行显式的别名展开，将其视为它所指向的基础类型。