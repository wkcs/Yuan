# Yuan 标准库使用指南

## 快速开始

### 导入标准库

```yuan
const std = @import("std")
```

### 使用 IO 模块

```yuan
const std = @import("std")

func main() {
    // 打印一行（自动换行）
    std.io.println("Hello, World!")

    // 打印不换行
    std.io.print("Hello, ")
    std.io.print("World!\n")
}
```

## 模块导入规则

### 1. 标准库模块

标准库模块位于 `stdlib/` 目录下。

```yuan
// 导入标准库主模块
const std = @import("std")

// 访问子模块
std.io.println("...")
```

### 2. 相对路径导入

在标准库内部，使用相对路径导入同目录的模块：

```yuan
// 在 stdlib/std.yu 中
pub const io = @import("io")  // 导入 stdlib/io.yu
```

### 3. 导出规则

**重要：** 只有使用 `pub` 关键字标记的声明才能被导出。

```yuan
// 可以被导出
pub func println(message: string) { ... }
pub const VERSION = "1.0.0"
pub struct Point { x: i32, y: i32 }

// 不能被导出（私有）
func internal_helper() { ... }
const PRIVATE_CONST = 42
```

## 可用模块

### std.io - 输入输出

#### `std.io.println(message: string)`

打印一行文本到标准输出（自动换行）。

```yuan
std.io.println("Hello, World!")
std.io.println("测试中文")
```

#### `std.io.print(message: string)`

打印文本到标准输出（不换行）。

```yuan
std.io.print("Hello, ")
std.io.print("World!")
std.io.print("\n")
```

## 内置函数

### @print(message: string)

底层打印函数，直接输出到标准输出（不换行）。

```yuan
@print("Hello, World!\n")
@print("测试中文\n")
```

**注意：** 通常应该使用 `std.io.println` 或 `std.io.print` 而不是直接使用 `@print`。

### @import(path: string)

导入模块。

```yuan
const std = @import("std")
const local = @import("./local_module")
```

## 示例程序

### Hello World

```yuan
const std = @import("std")

func main() {
    std.io.println("Hello, World!")
}
```

### 多行输出

```yuan
const std = @import("std")

func main() {
    std.io.println("第一行")
    std.io.println("第二行")
    std.io.println("第三行")
}
```

### 混合使用 print 和 println

```yuan
const std = @import("std")

func main() {
    std.io.print("姓名: ")
    std.io.println("张三")

    std.io.print("年龄: ")
    std.io.println("25")
}
```

## 标准库目录结构

```
stdlib/
├── std.yu          # 标准库主入口
└── io.yu           # IO 模块
```

## 编译和运行

### 词法分析

```bash
./build/tools/yuanc/yuanc --emit=tokens your_file.yu
```

### 语法分析

```bash
./build/tools/yuanc/yuanc --emit=ast your_file.yu
```

### 完整编译（待实现）

```bash
./build/tools/yuanc/yuanc your_file.yu -o output
```

## 注意事项

1. **导出声明必须使用 `pub` 关键字**
   - 函数：`pub func name() { ... }`
   - 常量：`pub const NAME = value`
   - 变量：`pub var name = value`
   - 结构体：`pub struct Name { ... }`

2. **模块导入路径**
   - 标准库：`@import("std")`
   - 相对路径：`@import("./module")` 或 `@import("../module")`
   - 同目录：`@import("module")`（在标准库内部）

3. **访问子模块**
   - 使用点号访问：`std.io.println(...)`
   - 不是双冒号：~~`std::io::println(...)`~~

4. **字符串字面量**
   - 支持转义字符：`"\n"`, `"\t"`, `"\\"`
   - 支持 UTF-8 中文：`"你好，世界！"`

## 未来计划

以下模块正在计划中：

- `std.collections` - 集合类型（Vector, HashMap 等）
- `std.fs` - 文件系统操作
- `std.net` - 网络功能
- `std.math` - 数学函数
- `std.string` - 字符串处理
- `std.time` - 时间和日期

## 贡献

欢迎为 Yuan 标准库贡献代码！请确保：

1. 所有导出的函数和类型都使用 `pub` 关键字
2. 添加完整的文档注释
3. 编写测试用例
4. 遵循项目的代码风格

## 相关文档

- [标准库实现文档](stdlib.md) - 技术实现细节
- [CLAUDE.md](../CLAUDE.md) - 项目开发指南
