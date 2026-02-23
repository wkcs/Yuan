# Yuan 编译器语法分析测试

本目录包含 Yuan 编译器语法分析器的测试用例，用于验证编译器能够正确解析各种 Yuan 语言构造。

## 目录结构

```
tests/yuan/parser/
├── declarations/           # 声明测试
│   ├── functions.yu       # 函数声明
│   ├── structs.yu         # 结构体声明
│   ├── enums.yu           # 枚举声明
│   ├── traits.yu          # Trait 和 Impl 声明
│   ├── variables.yu       # 变量和常量声明
│   ├── variadic_functions.yu  # 可变参数函数
│   └── visibility.yu      # 可见性修饰符
├── expressions/            # 表达式测试
│   ├── arithmetic.yu      # 算术表达式
│   ├── logical.yu         # 逻辑表达式
│   ├── calls.yu           # 函数调用和方法调用
│   ├── collections.yu     # 集合表达式（数组、元组、闭包等）
│   ├── control_flow.yu    # 控制流表达式（if、match）
│   ├── error_propagation.yu   # 错误传递表达式
│   └── builtin_functions.yu  # 内置函数调用
├── statements/             # 语句测试
│   ├── assignments.yu     # 赋值语句
│   ├── control_flow.yu    # 控制流语句（while、for、loop）
│   └── declarations.yu    # 声明语句
├── types/                  # 类型测试
│   ├── basic.yu           # 基本类型
│   ├── complex.yu         # 复杂类型（泛型、函数类型等）
│   └── references.yu      # 引用和指针类型
├── patterns/               # 模式测试
│   ├── basic.yu           # 基本模式
│   └── complex.yu         # 复杂模式（解构、守卫等）
└── errors/                 # 错误恢复测试
    ├── syntax_errors.yu   # 语法错误
    └── recovery_test.yu   # 错误恢复
```

## 测试内容

### 声明测试 (declarations/)

- **functions.yu**: 函数声明的各种形式，包括参数、返回值、泛型函数等
- **structs.yu**: 结构体定义，包括字段、泛型结构体、元组结构体等
- **enums.yu**: 枚举定义，包括简单枚举、带数据的枚举、泛型枚举等
- **traits.yu**: Trait 定义和实现，包括关联类型、默认实现、继承等
- **variables.yu**: 变量和常量声明，包括类型推断、模式匹配等
- **variadic_functions.yu**: 可变参数函数的定义和调用
- **visibility.yu**: 公开、私有、包内可见等可见性修饰符

### 表达式测试 (expressions/)

- **arithmetic.yu**: 算术运算符、位运算符等
- **logical.yu**: 逻辑运算符、比较运算符等
- **calls.yu**: 函数调用、方法调用、泛型函数调用等
- **collections.yu**: 数组字面量、元组、结构体构造、闭包表达式等
- **control_flow.yu**: if 表达式、match 表达式等
- **error_propagation.yu**: 错误传递操作符 `!` 和错误处理语法
- **builtin_functions.yu**: 内置函数调用（@import、@sizeof、@typeof 等）

### 语句测试 (statements/)

- **assignments.yu**: 各种赋值语句和复合赋值
- **control_flow.yu**: while、for、loop 循环，break、continue 语句
- **declarations.yu**: 语句级别的声明

### 类型测试 (types/)

- **basic.yu**: 基本类型的使用和转换
- **complex.yu**: 泛型类型、函数类型、复杂嵌套类型等
- **references.yu**: 引用类型、可变引用、指针类型等

### 模式测试 (patterns/)

- **basic.yu**: 基本模式匹配，包括字面量、变量、通配符等
- **complex.yu**: 复杂模式，包括结构体解构、数组模式、守卫条件等

### 错误测试 (errors/)

- **syntax_errors.yu**: 各种语法错误情况
- **recovery_test.yu**: 错误恢复机制测试

## Yuan 语言特性

本测试套件覆盖了 Yuan 语言的以下特性：

### 语法特点
- 使用 `.` 而不是 `::` 来访问关联函数
- 使用 `var` 声明变量，`const` 声明常量
- 使用 `func` 关键字定义函数和闭包
- 不使用分号作为语句分隔符
- 支持 Unicode 标识符

### 错误处理
- 使用 `!` 操作符进行错误传播
- 使用 `-> err {}` 语法处理错误
- 基于 `Error` trait 的错误系统

### 可变参数
- 使用 `...args` 语法定义可变参数
- 编译器将可变参数转换为元组

### 内置函数
- `@import` - 模块导入
- `@sizeof` - 获取类型大小
- `@typeof` - 获取类型名称
- `@panic` - 程序终止
- `@assert` - 断言检查
- `@file`、`@line`、`@column`、`@func` - 调试信息

### 可见性
- `pub` - 公开
- `priv` - 私有
- `internal` - 包内可见
- 默认为私有

## 运行测试

使用提供的 Python 脚本运行所有测试：

```bash
# 从项目根目录运行
python3 tests/scripts/test_parser.py
```

或者手动测试单个文件：

```bash
# 构建编译器
cmake -B build
cmake --build build

# 测试单个文件
./build/tools/yuanc/yuanc -ast-dump tests/yuan/parser/declarations/functions.yu
```

## 测试要求

- 所有正常测试用例应该能够成功解析并生成 AST
- 错误测试用例应该产生适当的错误信息
- 测试覆盖 Yuan 语言规范中定义的所有语法构造
- 测试用例应该使用正确的 Yuan 语法，符合语言规范

## 注意事项

- 测试文件使用 `.yu` 扩展名
- 所有测试用例都应该是有效的 Yuan 代码
- 错误测试用例放在 `errors/` 目录下
- 测试用例应该涵盖边界情况和复杂嵌套结构
