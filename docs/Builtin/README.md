# Builtin 模块文档

## 概述

Builtin 模块负责处理 Yuan 语言中的内置函数（以 `@` 开头的函数，如 `@sizeof`, `@import`, `@panic` 等）。这些函数不是普通的库函数，而是由编译器直接支持的特殊操作，通常对应特定的 CPU 指令或编译器内部逻辑。

## 架构设计

Builtin 模块采用**策略模式**和**注册表模式**相结合的设计。

### BuiltinHandler

`BuiltinHandler` (`include/yuan/Builtin/BuiltinHandler.h`) 是所有内置函数处理器的抽象基类。每个具体的内置函数都需要继承此类并实现以下接口：

- `getName()`: 返回内置函数名称（不含 `@` 前缀）。
- `getKind()`: 返回内置函数类型枚举值。
- `analyze(expr, sema)`: **语义分析阶段**的钩子。负责检查参数数量和类型，并计算返回值的类型。
- `generate(expr, codegen)`: **代码生成阶段**的钩子。负责生成对应的 LLVM IR。

这种设计使得新增内置函数非常容易，只需添加一个新的 Handler 类，而无需修改编译器主逻辑。

### BuiltinRegistry

`BuiltinRegistry` (`include/yuan/Builtin/BuiltinRegistry.h`) 是一个单例类，负责管理所有的 `BuiltinHandler`。

- **自动注册**: 在编译器启动时，所有内置函数会自动注册到 Registry 中。
- **查找**: 提供通过名称或枚举值查找 Handler 的功能。
- **统一管理**: 编译器前端在解析到以 `@` 开头的调用时，会查询 Registry 获取对应的处理器。

## 扩展指南

要添加一个新的内置函数（例如 `@my_builtin`），需要执行以下步骤：

1. 在 `BuiltinKind` 枚举中添加新类型。
2. 创建一个继承自 `BuiltinHandler` 的类 `MyBuiltinHandler`。
3. 实现 `analyze` 方法进行参数检查和类型推导。
4. 实现 `generate` 方法生成 LLVM IR。
5. 在 `BuiltinRegistry::registerAllBuiltins` 中注册该 Handler。

## OS Builtin（运行时桥接）

`OSBuiltin`（`src/Builtin/OSBuiltin.cpp`）负责将标准库中的 `@os_*` 内置函数桥接到 runtime C API。当前已覆盖文件系统、线程、时间、HTTP，以及新增的进程环境能力。

### 新增：命令行参数与环境变量内置函数

- `@os_args_len() -> usize`
  - 返回当前进程的命令行参数数量（包含 `argv[0]`）。
- `@os_arg_at(index: usize) -> str`
  - 返回指定索引的命令行参数；越界时返回空字符串。
- `@os_env_has(name: str) -> bool`
  - 判断环境变量是否存在。
- `@os_env_get(name: str) -> str`
  - 获取环境变量值；不存在时返回空字符串。

对应 runtime 符号：
- `yuan_os_args_len`
- `yuan_os_arg_at`
- `yuan_os_env_has`
- `yuan_os_env_get`

扩展这些能力时，仍遵循同一流程：
1. 在 `BuiltinKind` 添加枚举；
2. 在 `Expr.cpp` 映射名字与 kind；
3. 在 `OSBuiltin.cpp` 增加 `analyze/generate` 分支；
4. 在 `BuiltinRegistry.cpp` 注册处理器；
5. 在 runtime 实现对应 `extern "C"` API。
