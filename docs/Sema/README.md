# Sema 模块文档

## 1. 模块定位

Sema（Semantic Analysis）位于 Parser 与 CodeGen 之间，负责把“语法正确”的 AST 变成“语义可执行”的 AST：

- 建立作用域与符号表
- 解析类型注解到语义类型
- 推断表达式类型并写回 AST
- 检查控制流、可变性、返回值、trait 约束、模式匹配穷尽性
- 为 CodeGen 提供 `ResolvedDecl`、`SemanticType`、模式绑定信息

核心实现：

- `include/yuan/Sema/Sema.h`
- `include/yuan/Sema/Scope.h`
- `include/yuan/Sema/Type.h`
- `src/Sema/Sema.cpp`
- `src/Sema/Scope.cpp`
- `src/Sema/ModuleManager.cpp`

## 2. 核心组件

### 2.1 `SymbolTable` / `Scope`

作用域类型包括：

- `Global` / `Module` / `Function` / `Block`
- `Struct` / `Enum` / `Trait` / `Impl`
- `Loop`（用于 `break/continue` 与标签检查）

符号表能力：

- 当前作用域插入 + 向父作用域链查找
- 内置类型预注册（`i32/f64/str/...`）
- 内置 `SysError` 枚举注入

### 2.2 `Type` 系统

语义类型与语法类型节点分离：

- 语法层：`TypeNode` 家族
- 语义层：`Type` 家族（`IntegerType/StructType/ErrorType/...`）

Sema 统一把 `TypeNode` 解析为 `Type*`，并在 AST 节点上记录。

### 2.3 `ModuleManager`

通过 `@import` 支持：

- 标准库解析（`std.*`）
- 相对路径/绝对路径模块
- 预编译包接口（`.ymi`）
- 模块缓存（`.yuan/cache`）

## 3. 入口与总体流程

入口：`Sema::analyzeDecl` / `Sema::analyzeStmt` / `Sema::analyzeExpr`。

典型流程：

1. 顶层声明预检查重定义
2. 解析声明类型，建立符号
3. 对函数/impl 进入局部作用域继续分析语句与表达式
4. 失败时通过 `DiagnosticEngine` 报告并尽量继续发现更多错误

## 4. 类型解析（`resolveType`）

支持解析：

- 内建类型
- 标识符类型（含别名）
- 数组 / 切片 / 元组 / optional
- 引用 / 指针
- 函数类型（含 `canError` / variadic）
- 错误类型 `!T`
- 泛型类型与泛型实例

解析结果用于：

- 声明 `SemanticType` 写回
- 表达式类型检查与推断
- 后续 CodeGen 类型降低

## 5. 声明分析细节

### 5.1 变量/常量

- `var` 可推断类型；`const` 需可确定初始化类型
- 赋值前检查类型兼容
- 常量/不可变符号禁止后续写入

### 5.2 函数声明

`analyzeFuncDecl` 主要做：

- 参数类型解析与默认值检查
- 可变参数收敛为 `VarArgs<T>`（无显式类型时为 `value`）
- 返回类型解析（默认 `void`）
- 构建函数语义类型并入符号表
- 分析函数体并检查“非 void 必须返回”

返回检查支持两类成功路径：

- 显式 `return`
- 隐式尾返回（块尾表达式或可转换 `match`）

### 5.3 结构体/枚举

- 字段/变体重名检查
- 字段默认值类型检查
- 枚举变体支持 unit/tuple/struct 负载
- 变体符号写入枚举作用域

### 5.4 Trait / Impl

Trait：

- 注册 trait 类型与方法签名
- 在 trait 作用域注入 `Self`

Impl：

- 解析目标类型
- 可从目标类型自动收集隐式泛型参数
- trait impl 时登记 `ImplTraitMap`（用于约束检查）
- 校验方法签名（参数个数、self 形式、返回类型、错误标记）
- 校验必需方法/关联类型是否全部实现
- 将 impl 方法注册到 `ASTContext`，供成员调用解析

## 6. 语句分析细节

### 6.1 作用域语句

- `BlockStmt`：进入/退出块作用域
- 分析失败后尽量继续后续语句

### 6.2 控制流语句

- `if/while` 条件必须为 `bool`
- `loop/while/for` 进入 `Loop` 作用域
- `break/continue` 检查是否在循环内
- 带标签 `break/continue` 向外匹配标签，遇函数边界终止

### 6.3 `for` 迭代规则

优先级：

1. 内建可迭代类型（range/array/slice/str/tuple/varargs）
2. 迭代器协议（`iter()/next()` + `Iterator` trait）

若无法推导元素类型，则报 `iterable` 类型不匹配。

### 6.4 `match` 与穷尽检查

- 每个 arm 进入独立块作用域
- 模式先绑定，再检查 guard，再分析 body
- 结束后执行穷尽检查：
  - `bool`：需覆盖 `true/false`
  - `enum`：需覆盖全部变体
  - `optional`：需覆盖 `Some/None`
  - 其他类型通常要求通配或标识符兜底

### 6.5 `defer`

`defer` 必须位于函数内；语义阶段只做合法性与内部语句检查，执行顺序由 CodeGen 保证。

## 7. 表达式分析细节

### 7.1 字面量与标识符

- 整数字面量默认 `i32`（有后缀按后缀）
- 浮点默认 `f64`
- `None` 初始视为 `?void`
- 标识符解析后写入 `ResolvedDecl`

### 7.2 运算表达式

- 算术/位运算/逻辑/比较分别检查操作数约束
- 处理无后缀整数字面量向对侧整数类型适配
- `orelse` 限定于 optional 语义

### 7.3 一元表达式与可变性

- `&` 需要可借用左值（特例支持切片借用）
- `&mut` 需要可赋值且可变
- `*` 仅可解引用引用/指针

### 7.4 赋值表达式

- 目标必须可赋值（lvalue）
- 目标必须可变（常量/不可变引用会报错）
- 复合赋值按运算符类型约束检查

### 7.5 调用表达式（重点）

`analyzeCallExpr` 处理：

- 普通函数调用
- 方法调用与隐式 `self` 语义
- 泛型函数参数推导
- 可变参数与 spread 参数约束
- 模块成员调用

如果调用目标是 trait 方法，在类型已知时可映射到具体 impl 方法。

### 7.6 错误相关表达式

- `expr!`：
  - 内部必须是 `!T`
  - 当前函数必须是 `canError`
  - 表达式类型为 `T`
- `expr -> err { ... }`：
  - 内部必须是 `!T`
  - 新建错误处理作用域并注入错误变量
  - 表达式类型为成功类型 `T`

### 7.7 结果未使用检查

Sema 对有返回值函数调用、部分内置调用可发出 `warn_unused_result`。

## 8. 类型兼容规则（`checkTypeCompatible`）

当前实现允许：

- 完全同型
- optional 接收 `None`（`?void`）
- `T` 赋给 `?T`
- 引用与值在特定场景的自动借用/自动解引用
- 泛型实例在未绑定参数上的宽松匹配
- `&mut T` 到 `&T` 的只读收敛

不兼容时统一报告 `err_type_mismatch`（包含期望/实际类型文本）。

## 9. Trait 约束检查

`checkTraitBound` 优先检查：

1. 泛型参数的约束列表
2. `ImplTraitMap` 中是否存在目标类型到 trait 的实现映射

该映射在 `analyzeImplDecl` 建立，是泛型约束、错误类型约束（如 `Error`）的基础。

## 10. 与 CodeGen 的契约

Sema 需要保证以下信息正确写回 AST：

- `Expr::Type`
- `IdentifierExpr/MemberExpr` 的 `ResolvedDecl`
- `Decl::SemanticType`
- 模式绑定产生的隐式 `VarDecl`

CodeGen 依赖这些信息直接发射 IR，不再重复语义推导。

## 11. 当前实现边界

- `TypeChecker` 独立文件仍为占位，核心逻辑集中在 `Sema.cpp`
- 一些高级静态分析（完整控制流可达性、借用生命周期）尚未实现
- 部分错误路径采用“继续分析”策略，可能出现级联诊断

