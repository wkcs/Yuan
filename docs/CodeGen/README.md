# CodeGen 模块文档

## 1. 模块定位

CodeGen（代码生成器）是编译器的后端前端，负责将经过 Sema 验证并附带了语义类型（`SemanticType`）的抽象语法树（AST）降低为 LLVM IR。它覆盖了以下核心职责：

- 将 Yuan 的语义类型映射为 LLVM 的原生类型体系（`llvm::Type`）。
- 发射各种声明、语句和表达式的 LLVM IR 指令。
- 处理泛型函数，根据调用点的实参类型进行单态化（Specialization / Monomorphization）。
- 实现特定语言特性的底层语义，例如带错误的返回类型 `!T`、`defer` 延迟执行栈、控制流（包含多层 `break/continue`）以及模式匹配（`match`）。
- （最终由 Driver 调用）将生成的 LLVM Module 转换为目标文件（`.o`）或汇编（`.s`），并链接可执行文件。

核心实现文件：
- `include/yuan/CodeGen/CodeGen.h`
- `src/CodeGen/CodeGen.cpp` (整体管线与类型转换)
- `src/CodeGen/CGDecl.cpp` (声明级 IR 生成)
- `src/CodeGen/CGStmt.cpp` (语句级 IR 生成)
- `src/CodeGen/CGExpr.cpp` (表达式级 IR 生成)

## 2. 核心状态与数据结构

`CodeGen` 实例在运行期间持有并维护以下关键状态：

- **LLVM 核心对象**：`llvm::LLVMContext`、`llvm::Module`、`llvm::IRBuilder<>`。
- **TypeCache**：记录 `Type* -> llvm::Type*` 的映射，避免重复的类型转换计算。
- **ValueMap**：记录 `Decl* -> llvm::Value*` 的映射，用于在处理变量、函数、参数时快速获取其内存地址（`alloca` 产生的指针）或直接值。
- **当前函数上下文**：`CurrentFunction` (`llvm::Function*`) 和 `CurrentFuncDecl` (`FuncDecl*`)。
- **LoopStack (循环栈)**：管理当前所处的各层循环。记录 `continue` 和 `break` 对应的目标基本块（BasicBlock）以及循环标签，用于支持带标签的循环跳转。
- **DeferStack (延迟执行栈)**：记录当前函数在执行路径上已经注册的 `defer` 语句块。当发生函数返回或跨块跳转时，CodeGen 会根据此栈正确“回滚”并展开（Unroll）延迟代码。
- **GenericSubstStack (泛型替换栈)**：在进行泛型函数特化时，压入具体的类型替换映射表（泛型参数 -> 具体 `Type*`）。

## 3. 代码生成入口与总体流程

在 Sema 分析成功通过后，Driver 会对每个顶层声明调用 `CodeGen::generateDecl`。

总体生成流程如下：
1. **声明分派**：`generateDecl` 根据 AST 节点的类型（使用 `node->getKind()` 分支）将任务分派给具体的 `generateVarDecl`, `generateConstDecl`, `generateFuncDecl`, `generateStructDecl` 等方法。
2. **函数体生成**：进入 `generateFuncDecl` 时，创建 `llvm::Function`，开辟 Entry 基本块。对函数体内的语句通过 `generateStmt` 进行递归下降生成。
3. **表达式生成**：所有的计算和右值获取通过 `generateExpr` 递归发射 LLVM 指令。左值（变量地址等）则通过专门的 `generateLValueAddress` 获取。
4. **IR 验证**：在整个 Module 生成结束后，Driver 会调用 LLVM 提供的 `verifyModule` 检查生成的 IR 是否符合静态单赋值（SSA）形式和类型规则。

**注意**：CodeGen 假定传入的 AST 是在语义上绝对正确的。如果 IR 发射阶段遇到了未定义的符号或类型不一致的情况，CodeGen 会通过断言失败或直接返回 `nullptr`，不会再进行软性的错误恢复。

## 4. 符号命名修饰与 ABI (Name Mangling)

为了避免符号冲突并支持泛型特化，CodeGen 必须对符号进行统一的命名修饰（Name Mangling）：

- **普通符号**：结合模块路径、声明位置以及类型签名。
- **泛型特化**：根据传入的实际类型实参，在函数名后缀追加特化标识。
- **外部链接**：通过 `@link_name` 注解支持与 C ABI 或其他外部库的符号对齐。

（详细命名规则参见未来的 `docs/CodeGen/NameMangling.md`）

## 5. 类型降低 (Type -> LLVM Type)

`getLLVMType` 是类型降低的入口，调度一系列 `convert*Type` 方法：

- **标量类型**：`bool` -> `i1`，`char` -> `i32` (或特定大小)，`int` / `float` -> 对应位宽的 LLVM 整型/浮点型，`void` -> `llvm::Type::getVoidTy()`。
- **复合类型**：`array` / `struct` / `tuple` 映射为 `llvm::StructType` 或 `llvm::ArrayType`。
- **引用/指针**：统一降低为 `llvm::PointerType` (在不透明指针 Opaque Pointers 时代通常表现为 `ptr`)。
- **函数类型**：降低为 `llvm::FunctionType`。当函数作为一等公民（First-class value）传递时，退化为函数指针。
- **特殊结构**：Optional (`?T`)、Range、VarArgs 等在后端都有对应的标准化结构体内存布局。

### 5.1 错误类型 (`!T`) 的内存布局

对于可能失败的类型 `!T`，Yuan 在底层的内存布局表现为一个结构体（Struct）：
- 字段 0 (`tag`)：一个布尔标志，`0` 表示成功 (Ok)，`1` 表示错误 (Err)。
- 字段 1 (`ok_value`)：成功时的值负载槽位（如果 `T` 是 `void`，则此槽位可能为空或被优化）。
- 字段 2 (`err_ptr`)：指向实际错误信息（如 `SysError`）的指针。

这个布局是 `generateReturnStmt`、`generateErrorPropagateExpr` (`expr!`) 和 `generateErrorHandleExpr` (`expr -> err { ... }`) 协同工作的基础。

## 6. 声明生成细节

### 6.1 变量与常量
- **局部变量**：在函数入口处的 Entry 块发射 `alloca` 指令分配栈内存，接着发射 `store` 将初始值存入，并将得到的 `AllocaInst` 存入 `ValueMap` 中。
- **全局变量/常量**：映射为 `llvm::GlobalVariable`。Sema 阶段通常已保证常量的初始化表达式可以折叠为常量。

### 6.2 函数定义与 `main` 的特殊包装
在 `generateFuncDecl` 中：
- 构建 LLVM 函数签名。如果原函数是 `canError`，返回类型会被自动改写为上述的 `!T` 结构体类型。
- 创建形参对应的 `alloca` 并 `store` 形参值，以统一后续的按地址访问。
- **隐式尾返回支持**：如果函数的最后一条语句是表达式语句，且当前所在分支没有显式的返回指令，CodeGen 会自动插入一个 `ret` 指令将其作为返回值返回。
- 对于 `void` 函数，若缺少 `return`，在末尾自动补全 `ret void`。

**`main` 函数的特殊处理**：
- 用户编写的 `func main()` 实际上在 LLVM 中会被重命名为 `yuan_main`。
- CodeGen 会自动生成一个标准的 C ABI 入口点 `int main(int argc, char** argv)`，该包装函数负责初始化运行时环境、调用 `yuan_main`，并将返回值适配为 `i32` 给操作系统。
- 若 `main` 是 `async` 的，包装函数会通过运行时入口 `yuan_async_run(yuan_main)` 启动事件循环。

### 6.3 泛型函数特化 (Specialization)
- 在定义阶段，带有泛型参数且未被实例化的函数模板**不会**生成 LLVM IR。
- 当在表达式中遇到对该泛型函数的调用时，CodeGen 根据实参推导出具体的类型，调用 `getOrCreateSpecializedFunction`。
- 该过程压入 `GenericSubstStack`，随后重新走一遍 `generateFuncDecl`，在生成过程中自动将所有类型参数替换为对应的真实类型。

## 7. 语句生成细节

### 7.1 Block 与 `defer` 机制
- 每进入一个局部块（Block），记录当前的 `scopeDeferDepth`（延迟栈深度）。
- 块正常结束时，按 LIFO（后进先出）顺序弹出并生成该深度范围内的 defer 语句的执行代码。
- 若在块内发生提前终止（如 `return`, `break`, `continue`），在发射跳转指令前，必须先将 defer 栈安全“展开”，确保资源释放正确执行。

### 7.2 控制流：`return`, `break`, `continue`
- `while`, `loop`, `for` 语句会入栈 `LoopStack`，记录 `ContinueBlock`（下一轮迭代的目标）和 `BreakBlock`（跳出循环的目标）。
- 当遇到 `break` 或 `continue`，且带有标签时，CodeGen 会从内向外查找对应的循环层级。在实际跳转（`br` 指令）之前，会插入所需展开的 defer 代码逻辑。

### 7.3 `match` 模式匹配
- 发射被匹配的表达式的值（Scrutinee）。
- 针对每个分支（Arm），串联出一系列的条件判断（如果是枚举则判断 Tag，如果是字面量则进行 `icmp`/`fcmp`），形成 BasicBlock 链。
- 若条件满足，生成模式变量的绑定（如从 Enum 负载中提取数据并 `alloca`），随后执行 Body。最终所有成功分支汇聚到一个公共的 EndBlock。

## 8. 表达式生成细节

### 8.1 左值地址生成 (`generateLValueAddress`)
负责定位赋值或借用操作的目标内存地址。支持：
- 标识符（从 `ValueMap` 中查询指针）。
- 结构体/元组的成员访问（通过 `getelementptr` 指令）。
- 数组或切片的索引访问（同样使用 `getelementptr`，并可能在此处插入越界检查）。
- 指针解引用（直接返回指针值自身作为地址）。

### 8.2 调用表达式 (`generateCallExpr`)
支持多种调用模式：
- 普通直接调用和外部符号（C ABI）调用。
- 隐式 `self` 的成员方法调用（在生成参数列表时，把左值地址或按值对象作为第一个参数压入）。
- 模块级别的成员函数调用。
- **内置魔法方法**：针对 `len()`、`iter()` 或是 `SysError.message()` 等标准库基础设施，CodeGen 会直接硬编码展开为特定的指令流（例如提取切片的长度字段），而不是作为真正的函数调用处理。

### 8.3 错误处理表达式 (`expr!` 与 `-> err`)
- **`expr!` (错误传播)**：
  - 提取 `!T` 结构体中的 `tag`。
  - 生成 `if` 控制流。若为 `Ok`，提取 `ok_value` 供外层表达式使用。
  - 若为 `Err`，并且当前函数本身也是 `canError`，则直接构造外层的 `!T` 结构体并通过 `ret` 指令向上返回；否则（例如在 `main` 内部但未使用 `try`），可能插入 `trap` 直接崩溃。
- **`expr -> err { ... }` (错误捕获)**：
  - 若为 `Err`，提取 `err_ptr`，在局部作用域内为 `err` 变量分配内存并赋值，随后跳转并执行错误处理闭包。

## 9. 运行时耦合点 (Runtime Bridge)

CodeGen 严重依赖 `runtime/` 目录下的 C++ 实现提供核心能力。在生成 IR 时，它会主动声明并调用以下外部运行时符号：
- **异步调度**：`yuan_async_run`, `yuan_await`。
- **堆内存分配**：`malloc`, `free`（供内置类型如动态字符串或装箱对象使用）。
- **字符串与 I/O**：特定的 `std` 级别内置函数底层实现。

这些外部引用在链接阶段（Driver 层面）会通过将 `yuan_runtime.a`（或对应共享库）送入链接器来得到满足。

## 10. 排错与调试建议

如果编译在 CodeGen 阶段崩溃或产生非法 IR，常见原因包括：
- `verifyModule` 失败：这意味着生成的指令违反了 LLVM 强类型系统的规定（比如 `store` 的值类型和指针类型不匹配），或是基本块（BasicBlock）缺少了终结指令（如 `br` 或 `ret`）。
- **调试步骤**：
  1. 使用 `./yuanc -fsyntax-only` 确保 Sema 没有隐藏的级联错误。
  2. 使用 `./yuanc -S` 尝试输出 LLVM IR文本。即使生成失败，LLVM 往往也会输出引发崩溃前的部分 IR 函数，通过检查最后一个生成的函数即可锁定问题所在。
  3. 检查特定 AST 节点的 `SemanticType` 与 CodeGen 调用 `getLLVMType` 所产生的结果是否完全对应。

