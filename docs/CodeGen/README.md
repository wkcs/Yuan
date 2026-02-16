# CodeGen 模块文档

## 1. 模块定位

CodeGen 负责把经过 Sema 标注的 AST 降低为 LLVM IR，覆盖：

- 类型到 LLVM 类型的映射
- 声明/语句/表达式的 IR 发射
- 泛型函数按实参单态化（specialization）
- 错误类型 `!T`、`defer`、控制流、模式匹配的后端语义
- 目标文件生成与可执行链接（经 Driver 调用）

核心实现文件：

- `include/yuan/CodeGen/CodeGen.h`
- `src/CodeGen/CodeGen.cpp`
- `src/CodeGen/CGDecl.cpp`
- `src/CodeGen/CGStmt.cpp`
- `src/CodeGen/CGExpr.cpp`

## 2. 核心状态与数据结构

`CodeGen` 持有以下关键状态：

- `LLVMContext` / `Module` / `IRBuilder`
- `TypeCache`：`Type* -> llvm::Type*`，避免重复构造类型
- `ValueMap`：`Decl* -> llvm::Value*`，用于变量/函数/参数取址与取值
- `CurrentFunction` / `CurrentFuncDecl`
- `LoopStack`：记录 `continue`、`break` 目标块和循环标签
- `DeferStack`：记录当前函数已注册的延迟执行语句
- `GenericSubstStack`：泛型实参替换上下文

## 3. 代码生成入口

Driver 在语义分析成功后，对每个顶层声明调用 `generateDecl`。

生成流程（简化）：

1. `generateDecl` 分派到 `generateVarDecl/generateConstDecl/generateFuncDecl/...`
2. 函数体内由 `generateStmt` 递归下降
3. 表达式由 `generateExpr` 递归发射
4. 生成结束后由 Driver 执行 `verifyModule`

注意：CodeGen 假设语义已通过；若 IR 发射遇到类型不一致，通常直接返回失败。

## 4. 符号命名与 ABI（实现级）

CodeGen 通过 `getFunctionSymbolName/getGlobalSymbolName` 做统一命名修饰，目标是：

- 避免同名冲突（模块路径 + 声明位置 + 类型签名）
- 支持泛型函数特化后缀
- 在链接时稳定定位符号

补充文档：`docs/CodeGen/NameMangling.md`。

## 5. 类型降低（Type -> LLVM Type）

`getLLVMType` 调度 `convert*Type` 系列函数：

- 标量：`bool/char/int/float/void`
- 复合：`array/slice/tuple/struct/enum`
- 引用/指针：统一降低为指针语义
- 函数类型：`llvm::FunctionType`（必要时作为一等值会退化为函数指针）
- 错误类型：`ErrorType(!T)` 降低为结构体布局
- Optional/Range/VarArgs/Value 等运行时结构类型

### 5.1 错误类型布局

当前 `!T` 使用结构体布局（逻辑上）：

- `tag`：是否错误
- `ok`：成功值槽位
- `err_ptr`：错误负载指针

`generateReturnStmt`、`generateErrorPropagateExpr`、`generateErrorHandleExpr` 都依赖这一布局。

## 6. 声明生成

### 6.1 变量与常量

- 局部变量：函数入口 `alloca`，`store` 初始化，`ValueMap` 记录地址
- 全局变量/常量：`llvm::GlobalVariable`
- 常量初始化要求可折叠为 LLVM 常量；否则回退零初始化（语义层通常已拦截）

### 6.2 函数定义与 `main` 包装

`generateFuncDecl` 关键行为：

- 按语义类型构造 LLVM 函数签名
- 若函数可报错（`canError`），返回类型自动包装为 `!T`
- 参数统一先 `alloca` 再 `store`，后续按地址访问
- 支持隐式尾返回：函数最后一条为表达式语句或可转换的 `match` 时自动 `return`
- 对 `void` 函数自动补 `ret void`

`main` 特殊规则：

- 用户 `func main()` 被重命名为 `yuan_main`
- 自动生成 C ABI `int main(int argc, char** argv)` 包装函数
- 包装函数调用 `yuan_main` 并将返回值折算为 `i32`
- `async main` 通过 `yuan_async_run` 运行时入口执行

### 6.3 泛型函数物化

- 非特化上下文下，含泛型参数的函数体可跳过生成
- 调用点通过实参推导映射后触发 `getOrCreateSpecializedFunction`
- 特化时压栈 `GenericSubstStack`，对参数/返回值/内部类型做替换

## 7. 语句生成

### 7.1 Block 与 `defer`

- 进入块时记录 `scopeDeferDepth`
- 块结束时按 LIFO 执行新增 defer
- 终止块（`return/break/...`）提前退出，且保持 defer 栈正确回滚

### 7.2 Return

- `return` 之前执行当前函数 defer 栈
- 普通函数：直接 `ret` 值/`ret void`
- 错误返回函数：构造 `!T` 返回结构（成功路径/错误路径）

### 7.3 循环与跳转

`while/loop/for` 都会入栈 `LoopStack`，记录：

- `ContinueBlock`
- `BreakBlock`
- 可选标签
- 进入循环时的 defer 深度

`break/continue`：

- 支持标签解析（从内向外匹配）
- 跳转前执行需要展开的 defer（从当前深度展开到目标循环入口深度）

### 7.4 Match 语句

- 先生成被匹配值
- 为每个 arm 构造条件与绑定
- 通过基本块链和终结块拼接控制流

## 8. 表达式生成

### 8.1 LValue 地址生成

`generateLValueAddress` 支持：

- 标识符
- 成员访问
- 索引
- 解引用

赋值时优先拿“原对象地址”，避免对临时值写入。

### 8.2 调用表达式（重点）

`generateCallExpr` 支持：

- 普通函数调用
- 成员方法调用（含隐式 `self` 注入）
- 模块成员函数调用
- 外部符号调用（`link_name` / 模块成员 `LinkName`）
- 枚举变体构造调用
- 可变参数与 spread 实参
- 泛型调用点映射推导 + 特化

并处理若干内建成员语义：

- `len()`：字符串/切片/数组
- `iter()`：可迭代对象
- `SysError.message()/full_trace()` 的专门分支

### 8.3 错误处理表达式

- `expr!`：
  - 解包 `!T`
  - `Ok` 分支取成功值
  - `Err` 分支在可传播函数中直接 `ret`，否则 `trap`
- `expr -> err { ... }`：
  - 分支化处理成功/失败路径
  - 错误分支绑定错误变量并执行 handler

### 8.4 其他表达式

包括：

- 二元/一元运算
- `as` 转换（整数/浮点/指针互转与聚合回退路径）
- `if`/`match` 表达式
- 闭包表达式（当前以函数对象形式生成）
- 数组/元组/结构体字面量
- 索引与切片
- 内置调用 `@...`

## 9. 与运行时的耦合点

CodeGen 依赖若干运行时符号：

- 异步：`yuan_async_run`
- 内存：必要时 `malloc`
- 以及 runtime 库提供的 ABI 支持

链接时由 Driver 将 `yuan_runtime` 注入链接命令。

## 10. 失败模式与排查建议

常见失败点：

- `verifyModule` 失败：通常是类型不一致或基本块终止不完整
- 调用生成失败：callee 未解析、泛型映射不完整、外部符号签名冲突
- 错误类型发射失败：`!T` 结构布局与使用不一致

建议：

1. 先使用 `-fsyntax-only` 确认 Sema 干净
2. 使用 `-S` 观察 `.ll`，定位失败前最后生成的函数
3. 对比 `Decl` 的 `SemanticType` 与 `getLLVMType` 结果

## 11. 当前实现边界

- TypeChecker 独立模块仍是占位，类型检查集中在 `Sema.cpp`
- 某些高级优化（如逃逸分析、借用分析）尚未引入
- 泛型策略为“按需单态化”，并非全程序预先实例化

