# Sema 模块文档：语义类型系统与兼容性规则

本文档描述了 Yuan 编译器中最复杂的类型推导、表示和兼容性验证引擎。它是区分静态类型语言与动态脚本的核心屏障。

## 1. 语法类型 vs 语义类型

在编译过程前端，有两套并行的类型表示体系，不可混淆：
- **`TypeNode` (语法类型)**：存在于 `AST` 命名空间下。这些是开发者在代码中字面上写出来的东西，比如 `i32`, `Vec<T>`, `?str`。它仅仅保留了排版、拼写和位置。
- **`Type` (语义类型)**：存在于 `Sema` 命名空间下。这才是编译器内部真正理解并在各模块间传递的“具有运算能力的绝对实体”。

Sema 的一项基建任务就是执行 `Sema::resolveType(TypeNode* node)`，把死板的语法树节点，“升维”成唯一的、可校验的 `Type*`。

## 2. 规范类型 (Canonical Types) 管理

所有的语义类型（继承自基类 `Type`）都不是随时随地 `new` 出来的。
它们被注册且托管在 `ASTContext` 内部（依靠类型工厂池）。
- **指针相等性**: 如果代码里出现两个不同的 `i32`，它们经过 `resolveType` 返回的指针地址必定指向内存里的**同一个**对象。这种特性称为 Canonical Types。
- 只有结构体、枚举等用户自定义类型会在解析到其定义时开辟新的指针地址。基础类型和复合结构（如 `[i32; 10]` 或 `&str`）如果结构一致，会被通过某种 Hash 函数映射为同一个指针。因此，在多数情况下，判定两个类型绝对相等只需要简单的 `T1 == T2` 指针对比。

## 3. 核心语义类型分类

| 类型名 | 描述 |
|---|---|
| `IntegerType`, `FloatType`, `BoolType`, `CharType` | 语言内置的无状态基础标量。 |
| `VoidType` | 空单位类型。 |
| `StringType` | 系统级支持的 UTF-8 切片字面量类型。 |
| `StructType` / `EnumType` | 自定义代数数据类型。持有字段表或变体列表（Payloads）。 |
| `ReferenceType` / `PointerType` | `&T`, `&mut T`, `*T`, `*mut T`，包装了另一个底层类型 `T`。 |
| `ArrayType` | 固定长度数组 `[T; N]`，带有静态界定的编译时尺寸。 |
| `SliceType` | 切片 `[T]`，在运行时对应一个胖指针（指针 + 长度）。 |
| `TupleType` | 多元素异构聚合 `(A, B, C)`。 |
| `OptionalType` | `?T`，可能为空的容器，是 Yuan 消除 NullPointerException 的工具。 |
| `FunctionType` | 表示函数签名：参数列表，返回类型，以及是否携带 `canError` 的 `!` 标记。 |
| `ErrorType` | 专门处理 `!T` 返回值的特殊类型，表示一个可能附带 `SysError` 的执行结果。 |
| `GenericType` | 尚未被实例化的泛型占位符参数（如 `T`）。 |
| `GenericInstanceType` | 被应用了具体实参后的类（如 `Vec<i32>`）。 |

## 4. 类型兼容性判定 (`checkTypeCompatible`)

在赋值（Assign）、参数传递（Call）、或者返回（Return）等场景中，Sema 会调用 `bool checkTypeCompatible(Type* expected, Type* actual, ...)` 进行强制约束。

Yuan 支持以下兼容和隐式适配规则：

1. **绝对相等**: `expected == actual`，直接通过。
2. **泛型容差**: 如果 `expected` 内部是一个未绑定的泛型实参（在推导阶段），会认为其匹配并绑定。
3. **Optional (可选类型) 自动包容**:
   - `None` (或者说底层的 `?void`) 可以赋予任何的 `?T`。
   - 基础值 `T` 可以安全赋值给 `?T`（Sema 在后续 CodeGen 会自动装箱构建出 Some 结构）。
4. **引用退化 (Reference Downgrade)**:
   - 可变引用 `&mut T` 可以安全隐式降级并传入需要只读不可变引用 `&T` 的上下文中。
   - 但反过来（试图将 `&T` 传给 `&mut T`）会被严格拒绝。
5. **解引用与自动借用 (Auto Deref / Auto Borrow)**:
   - 在成员方法调用时（即 `obj.method()`），如果 `method` 期待的是 `&Self` 而你只提供了值 `Self`，或者期待的是 `Self` 而你提供了 `&Self`，在这个极其特定的调用受控上下文中，编译器允许这种不严谨的写法（这是向 Rust 靠拢体验的特例），并在 CodeGen 阶段自动插入提取地址或载入指令。但在普通的独立函数调用或变量赋值中，必须写出明确的 `&obj` 或 `*obj`。
6. **整数自动字面量适配**:
   - 如果一端是具体的整数类型（如 `i64`），而另一端是通过解析未经限定后缀的 `IntegerLiteralExpr` 产生的泛型基础整数，Sema 会允许其匹配，并在内部将其具体化为 `i64`。

如果不兼容，Sema 将直接返回 `false` 并通过系统 Diagnostic 发射一个标准化的 `err_type_mismatch` 提示框。

## 5. 泛型推导机制 (Generic Deduction)

位于 `unifyGenericTypes` 和相关体系。

当开发者调用一个如 `func identity<T>(x: T) -> T` 的函数时，若没有使用钻石括号 `<i32>` 手动指定：
- Sema 会启动匹配器，把 `expected`（即签名里的泛型 `T`）和 `actual`（传入实参的具体类型，如 `i32`）进行结构对比。
- 一旦匹配器发现 `expected` 是一个尚未固定的 `GenericType`，它就在内部的置换映射表（`std::unordered_map<std::string, Type*>`）中记录 `T -> i32`。
- 如果在参数列表中有多个 `T`（比如 `func add(a: T, b: T)`），如果推导出现了分歧（如传入 `i32` 和 `f64`），会导致推导破裂并报错。
- 完成推导后，Sema 使用这个映射表调用 `substituteType`，在脑内将函数内部签名中的所有 `T` 改写为 `i32`。最后生成一个带有特定签名的实体（即实例化），供 CodeGen 最终发射专用代码段。