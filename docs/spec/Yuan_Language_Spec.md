# Yuan 语言规范文档 (编译型版本)

版本：1.0
最后更新：2026-02-15

## 目录

1. [概述](#1-概述)
2. [词法结构](#2-词法结构)
3. [类型系统](#3-类型系统)
4. [表达式](#4-表达式)
5. [语句](#5-语句)
6. [函数](#6-函数)
7. [结构体](#7-结构体)
8. [枚举](#8-枚举)
9. [Trait](#9-trait)
10. [模块系统](#10-模块系统)
11. [错误处理](#11-错误处理)
12. [并发](#12-并发)
13. [内置函数](#13-内置函数)
14. [标准库](#14-标准库)
15. [完整示例](#15-完整示例)
16. [附录](#16-附录)
17. [实现说明：语义分析与代码生成](#17-实现说明语义分析与代码生成)

---

## 1. 概述

### 1.1 语言特性

Yuan 是一门现代的、静态类型的编译型编程语言，具有以下核心特性：

- **静态类型**：编译时类型检查，类型安全
- **编译型**：编译为机器码或中间代码，高性能执行
- **Unicode 支持**：标识符支持完整的 Unicode 字符集
- **函数式特性**：一等公民函数、闭包、高阶函数
- **面向对象**：基于 Trait 的多态，结构体组合
- **并发支持**：内置协程和 async/await 语法
- **模式匹配**：强大的 match 表达式
- **精确数值类型**：i8/i16/i32/i64/i128、u8/u16/u32/u64/u128、f32/f64
- **内置数组和切片**：固定长度数组、动态切片、字符串切片
- **显式错误处理**：基于 Error trait 的错误处理机制，无异常
- **强制返回值处理**：函数返回值必须被使用
- **内置函数**：编译器提供的 `@` 开头的内置函数

### 1.2 设计理念

- **安全性**：静态类型提供编译时错误检查
- **性能**：编译型语言，零成本抽象
- **显式性**：错误处理显式化，无隐藏控制流
- **可控内存模型**：借鉴 Zig 的显式引用/指针设计，不使用 Rust 风格借用检查
- **简洁性**：语法简洁，无宏系统

### 1.3 文件扩展名

Yuan 源代码文件使用 `.yu` 扩展名。

---

## 2. 词法结构

### 2.1 注释

Yuan 支持三种注释形式：

```yuan
// 单行注释

/*
 * 块注释
 * 可以跨越多行
 */

/// 文档注释
/// 用于生成文档
func add(a: i32, b: i32) -> i32 {
    return a + b
}
```

### 2.2 标识符

标识符支持完整的 Unicode 字符集：

- 以 Unicode 字母或下划线开头
- 后续可包含 Unicode 字母、数字或下划线
- `@` 开头的标识符保留给内置函数

```yuan
var count: i32 = 0          // 普通标识符
var 计数器: i32 = 0         // Unicode 标识符
var _private: i32 = 1       // 下划线开头
```

### 2.3 关键字

以下是 Yuan 的保留关键字：

```
var       const     func      return    struct    enum      trait     impl
pub       priv      internal  if        elif      else      match     while
loop      for       in        break     continue  true      false     async
await     as        self      Self      mut       ref       ptr       void
defer     type      where     None
i8        i16       i32       i64       i128      isize
u8        u16       u32       u64       u128      usize
f32       f64       bool      char      str
```

**关键字说明**：
- `var`：声明可变变量
- `const`：声明不可变常量
- `self`：在方法中表示当前实例
- `Self`：表示类型自身，用于返回值类型和 Trait 定义
- `mut`：标记可变引用（如 `&mut T`、`*mut T`）
- `ref`：引用
- `ptr`：指针
- `defer`：延迟执行
- `None`：表示空值

### 2.4 内置函数

以 `@` 开头的标识符是内置函数，由编译器实现：

```yuan
@import     // 导入模块
@sizeof     // 获取类型大小
@typeof     // 获取类型信息
@panic      // 触发 panic
@assert     // 断言
@file       // 当前文件名
@line       // 当前行号
@column     // 当前列号
@func       // 当前函数名
```

### 2.5 字面量

#### 2.5.1 整数字面量

```yuan
var decimal: i32 = 123_456       // 十进制
var hex: i32 = 0xFF_00           // 十六进制
var octal: i32 = 0o755           // 八进制
var binary: i32 = 0b1010_1100    // 二进制
var negative: i32 = -42          // 负数

// 带类型后缀
var a: i8 = 127i8
var b: i64 = 9223372036854775807i64
var c: u32 = 4294967295u32
```

#### 2.5.2 浮点数字面量

```yuan
var pi: f64 = 3.14159
var scientific: f64 = 1.23e-4
var with_separator: f64 = 1_000.5

// 带类型后缀
var x: f32 = 3.14f32
var y: f64 = 3.14159265358979f64
```

#### 2.5.3 字符字面量

```yuan
var ch: char = 'A'
var unicode_char: char = '中'
var escaped: char = '\n'
var hex_char: char = '\x41'      // 'A'
var unicode_escape: char = '\u{4E2D}'  // '中'
```

#### 2.5.4 字符串字面量

```yuan
// 普通字符串
var s: str = "Hello, World!"

// 转义字符
var escaped: str = "Line 1\nLine 2\tTabbed"

// 原始字符串（不处理转义）
var raw: str = r"C:\path\to\file"
var raw_custom: str = r###"可以包含 " 和 ## 符号"###

// 多行字符串
var multiline: str = """
    第一行
    第二行
    第三行
"""

// 字符串拼接（使用 std.fmt）
const fmt = @import("std").fmt
var name: str = "Alice"
var age: i32 = 30
var message: String = fmt.format("My name is {}, I'm {} years old", name, age)
```

#### 2.5.5 布尔字面量

```yuan
var is_true: bool = true
var is_false: bool = false
```

#### 2.5.6 None 字面量

```yuan
var empty: ?i32 = None    // 表示空值
```

#### 2.5.7 数组字面量

```yuan
// 固定长度数组
var empty: [i32; 0] = []
var numbers: [i32; 5] = [1, 2, 3, 4, 5]
var zeros: [i32; 10] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]  // 10个0
var matrix: [[i32; 3]; 2] = [[1, 2, 3], [4, 5, 6]]

// 类型推断
var inferred = [1, 2, 3, 4, 5]           // [i32; 5]
```

#### 2.5.8 元组字面量

```yuan
var empty: () = ()
var pair: (i32, str) = (42, "hello")
var triple: (i32, f64, bool) = (1, 3.14, true)
```

### 2.6 语句分隔

Yuan 使用换行作为语句分隔符，不需要分号：

```yuan
var x: i32 = 10
var y: i32 = 20
var sum: i32 = x + y
```

---

## 3. 类型系统

### 3.1 静态类型

Yuan 是静态类型语言，所有变量的类型在编译时确定：

```yuan
var x: i32 = 42
// x = "hello"  // 编译错误：类型不匹配

// 类型推断
var y = 42              // 推断为 i32
var z = 3.14            // 推断为 f64
var s = "hello"         // 推断为 str
```

### 3.2 基本类型

#### 3.2.1 整数类型

| 类型 | 大小 | 范围 |
|------|------|------|
| `i8` | 8位 | -128 到 127 |
| `i16` | 16位 | -32,768 到 32,767 |
| `i32` | 32位 | -2,147,483,648 到 2,147,483,647 |
| `i64` | 64位 | -9,223,372,036,854,775,808 到 9,223,372,036,854,775,807 |
| `i128` | 128位 | -2^127 到 2^127-1 |
| `isize` | 平台相关 | 32位或64位有符号整数 |

```yuan
var a: i8 = 127
var b: i16 = 32767
var c: i32 = 2147483647
var d: i64 = 9223372036854775807
var e: i128 = 170141183460469231731687303715884105727
var f: isize = 100
```

#### 3.2.2 无符号整数类型

| 类型 | 大小 | 范围 |
|------|------|------|
| `u8` | 8位 | 0 到 255 |
| `u16` | 16位 | 0 到 65,535 |
| `u32` | 32位 | 0 到 4,294,967,295 |
| `u64` | 64位 | 0 到 18,446,744,073,709,551,615 |
| `u128` | 128位 | 0 到 2^128-1 |
| `usize` | 平台相关 | 32位或64位无符号整数 |

```yuan
var a: u8 = 255
var b: u16 = 65535
var c: u32 = 4294967295
var d: u64 = 18446744073709551615
var e: usize = 100
```

#### 3.2.3 浮点数类型

| 类型 | 大小 | 精度 |
|------|------|------|
| `f32` | 32位 | 单精度（约7位有效数字） |
| `f64` | 64位 | 双精度（约15位有效数字） |

```yuan
var x: f32 = 3.14f32
var y: f64 = 3.141592653589793
var z: f64 = 1.0e-10
```

#### 3.2.4 布尔类型

```yuan
var flag: bool = true
var is_valid: bool = false
```

#### 3.2.5 字符类型

`char` 类型表示一个 Unicode 标量值（4字节）：

```yuan
var ch: char = 'A'
var chinese: char = '中'
var emoji: char = '🎉'
```

#### 3.2.6 字符串类型

```yuan
// str - 不可变字符串（UTF-8编码）
var s: str = "Hello, World!"

// String - 可变字符串（堆分配）
var mut_s: String = String.from("Hello")
mut_s.push_str(", World!")
```

#### 3.2.7 void 类型

表示无返回值：

```yuan
func print_hello() -> void {
    const io = @import("std").io
    io.println("Hello")
}
```

### 3.3 复合类型

#### 3.3.1 数组类型

数组是固定长度的同类型元素序列：

```yuan
// 声明数组
var numbers: [i32; 5] = [1, 2, 3, 4, 5]
var zeros: [f64; 10] = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]

// 访问元素
var first: i32 = numbers[0]
var last: i32 = numbers[4]

// 修改元素
numbers[0] = 10

// 获取长度
var len: usize = numbers.len()

// 多维数组
var matrix: [[i32; 3]; 2] = [
    [1, 2, 3],
    [4, 5, 6]
]
var element: i32 = matrix[1][2]  // 6
```

#### 3.3.2 切片类型

切片是对数组或其他连续内存区域的动态视图：

```yuan
// 从数组创建切片
var arr: [i32; 5] = [1, 2, 3, 4, 5]
var slice: &[i32] = &arr[1..4]      // [2, 3, 4]

// 切片操作
var full: &[i32] = &arr[..]         // 完整切片
var from_start: &[i32] = &arr[..3]  // [1, 2, 3]
var to_end: &[i32] = &arr[2..]      // [3, 4, 5]

// 可变切片（mut 标注在类型前）
var mut_slice: &mut [i32] = &mut arr[1..4]
mut_slice[0] = 10

// 切片方法
var len: usize = slice.len()
var is_empty: bool = slice.is_empty()
```

#### 3.3.3 字符串切片类型

字符串切片是对字符串的视图：

```yuan
var s: str = "Hello, World!"

// 创建字符串切片
var hello: &str = &s[0..5]          // "Hello"
var world: &str = &s[7..12]         // "World"

// 字符串切片方法
var len: usize = hello.len()        // 字节长度
var chars: usize = hello.chars().count()  // 字符数量

// UTF-8 安全切片
var chinese: str = "你好世界"
var first_char: &str = &chinese[0..3]  // "你"（UTF-8中一个中文字符占3字节）

// 字符串切片操作
var trimmed: &str = s.trim()
var upper: String = s.to_uppercase()
var contains: bool = s.contains("World")
```

#### 3.3.4 Vec 动态数组

`Vec<T>` 是堆分配的动态数组：

```yuan
const Vec = @import("std").collections.Vec
const io = @import("std").io

// 创建 Vec
var v: Vec<i32> = Vec.new()
var v2: Vec<i32> = Vec.with_capacity(10)
var v3: Vec<i32> = Vec.from_slice(&[1, 2, 3, 4, 5])

// 添加元素
v.push(1)
v.push(2)
v.push(3)

// 访问元素
var first: i32 = v.get(0)
var last: i32 = v.last()

// 删除元素
var popped: i32 = v.pop()
v.remove(0)

// 切片
var slice: &[i32] = v.as_slice()

// 迭代
for item in v.iter() {
    io.println(item)
}

// 容量和长度
var len: usize = v.len()
var cap: usize = v.capacity()
```

#### 3.3.5 元组类型

元组是固定长度的异构集合：

```yuan
var point: (i32, i32) = (10, 20)
var person: (str, i32, bool) = ("Alice", 30, true)

// 访问元组元素
var x: i32 = point.0
var y: i32 = point.1

// 元组解构
var (name, age, is_student) = person

// 单元素元组
var single: (i32,) = (42,)

// 空元组（unit 类型）
var unit: () = ()
```

#### 3.3.6 Optional 类型

表示可能为空的值，使用 `?T` 语法：

```yuan
var some_value: ?i32 = 42
var no_value: ?i32 = None

// 模式匹配
const io = @import("std").io
match some_value {
    Some(value) => io.println(value),
    None => io.println("No value")
}

// 方法
var unwrapped: i32 = some_value.unwrap()           // 如果为 None 则 panic
var or_default: i32 = no_value.unwrap_or(0)        // 提供默认值
var is_some: bool = some_value.is_some()
var is_none: bool = no_value.is_none()

// orelse 操作符
var value: i32 = no_value orelse 0
var value2: i32 = no_value orelse return  // 如果为 None 则返回
```

### 3.4 指针和引用

Yuan 的引用/指针模型借鉴 Zig：不引入生命周期标注，也不做 Rust 风格借用检查。

#### 3.4.1 引用

```yuan
var x: i32 = 42

// 不可变引用
var r: &i32 = &x

// 可变引用（mut 标注在类型前）
var y: i32 = 42
var mr: &mut i32 = &mut y
mr = 100
```

#### 3.4.2 指针

```yuan
var x: i32 = 42

// 指针
var p: *i32 = &x
var value: i32 = *p

// 可变指针（mut 标注在类型前）
var y: i32 = 42
var mp: *mut i32 = &mut y
*mp = 100
```

#### 3.4.3 引用与并发规则（Zig 风格）

- `&T` 和 `&mut T` 都是非拥有引用，调用方负责保证被引用值在使用期间有效。
- `&mut T` 表示“允许修改”，但编译器不做 Rust 风格的全局唯一借用检查。
- 引用自动解引用：对 `&T` / `&mut T` 进行读取、赋值、运算、方法调用时，编译器自动作用到被引用值。
- 引用绑定不可重绑定；对 `&mut T` 执行 `ref = expr` 等价于写入被引用值。
- `*` 仅用于指针（`*T` / `*mut T`）的显式解引用；引用类型不需要写 `*`。
- 跨线程共享可变数据时，必须使用 `std.sync` 提供的同步原语（如互斥锁、通道）避免数据竞争。
- 调试构建默认开启边界/空指针等安全检查；发布构建可由编译选项控制检查强度。

### 3.5 类型转换

```yuan
// 数值类型转换
var x: i32 = 42
var y: i64 = x as i64
var z: f64 = x as f64

// 整数截断
var big: i64 = 1000
var small: i8 = big as i8  // 可能溢出

// 浮点转整数
var f: f64 = 3.7
var i: i32 = f as i32  // 3（截断）

// 字符和整数
var ch: char = 'A'
var code: u32 = ch as u32  // 65
```

### 3.6 类型别名

```yuan
type Byte = u8
type Word = u16
type Point = (f64, f64)
type Matrix3x3 = [[f64; 3]; 3]

var b: Byte = 255
var p: Point = (1.0, 2.0)
```

---

## 4. 表达式

### 4.1 字面量表达式

```yuan
42i32           // 整数
3.14f64         // 浮点数
"hello"         // 字符串
'A'             // 字符
true            // 布尔值
None            // 空值
[1, 2, 3]       // 数组
(1, "hello")    // 元组
```

### 4.2 算术表达式

```yuan
var a: i32 = 10 + 5      // 加法
var b: i32 = 10 - 5      // 减法
var c: i32 = 10 * 5      // 乘法
var d: i32 = 10 / 5      // 除法
var e: i32 = 10 % 3      // 取模
var f: i32 = -a          // 取负

// 位运算
var g: i32 = 0b1010 & 0b1100   // 按位与
var h: i32 = 0b1010 | 0b1100   // 按位或
var i: i32 = 0b1010 ^ 0b1100   // 按位异或
var j: i32 = ~0b1010           // 按位取反
var k: i32 = 1 << 4            // 左移
var l: i32 = 16 >> 2           // 右移
```

### 4.3 比较表达式

```yuan
var a: bool = 10 == 5     // 等于
var b: bool = 10 != 5     // 不等于
var c: bool = 10 > 5      // 大于
var d: bool = 10 < 5      // 小于
var e: bool = 10 >= 5     // 大于等于
var f: bool = 10 <= 5     // 小于等于
```

### 4.4 逻辑表达式

```yuan
var a: bool = true && false   // 逻辑与
var b: bool = true || false   // 逻辑或
var c: bool = !true           // 逻辑非

// 短路求值
var result: bool = x > 0 && y / x > 2
```

### 4.5 赋值表达式

```yuan
var x: i32 = 10
x = 20              // 简单赋值
x += 5              // 复合赋值
x -= 3
x *= 2
x /= 4
x %= 3
x &= 0xFF           // 位运算复合赋值
x |= 0x0F
x ^= 0xF0
x <<= 2
x >>= 1
```

### 4.6 索引表达式

```yuan
// 数组索引
var arr: [i32; 5] = [1, 2, 3, 4, 5]
var first: i32 = arr[0]
var last: i32 = arr[4]

// 切片
var slice: &[i32] = &arr[1..3]       // [2, 3]
var from_start: &[i32] = &arr[..3]   // [1, 2, 3]
var to_end: &[i32] = &arr[2..]       // [3, 4, 5]

// 字符串索引（返回字节）
var s: str = "hello"
var byte: u8 = s.as_bytes()[0]

// 字符串切片
var sub: &str = &s[1..4]  // "ell"
```

### 4.7 if 表达式

if 表达式可以返回值：

```yuan
var x: i32 = 10
var result: str = if x > 5 {
    "greater"
} elif x < 5 {
    "less"
} else {
    "equal"
}

// 单行 if
var max: i32 = if a > b { a } else { b }
```

### 4.8 match 表达式

match 表达式提供强大的模式匹配：

```yuan
enum Color {
    Red,
    Green,
    Blue,
    RGB(u8, u8, u8)
}

var color: Color = Color.RGB(255, 0, 0)
const fmt = @import("std").fmt
const io = @import("std").io

var description: str = match color {
    Color.Red => "红色",
    Color.Green => "绿色",
    Color.Blue => "蓝色",
    Color.RGB(r, g, b) => fmt.format("RGB({}, {}, {})", r, g, b)
}

// 字面量模式
match x {
    0 => "zero",
    1 => "one",
    _ => "other"  // 通配符
}

// 守卫条件
match number {
    n if n < 0 => "negative",
    n if n > 0 => "positive",
    _ => "zero"
}

// 多个模式
match value {
    1 | 2 | 3 => "small",
    4 | 5 | 6 => "medium",
    _ => "large"
}

// 范围模式
match age {
    0..=12 => "child",
    13..=19 => "teenager",
    20..=59 => "adult",
    _ => "senior"
}

// 解构模式
match point {
    (0, 0) => "origin",
    (x, 0) => "on x-axis",
    (0, y) => "on y-axis",
    (x, y) => "other"
}

// Optional 模式
match optional_value {
    Some(value) => io.println(value),
    None => io.println("empty")
}
```

### 4.9 闭包表达式

闭包是匿名函数，可以捕获环境变量：

```yuan
// 使用 func 定义闭包
var add = func(a: i32, b: i32) -> i32 {
    return a + b
}

// 简短语法（类型推断）
var double = func(x: i32) -> i32 { x * 2 }

// 捕获环境变量
var x: i32 = 10
var add_x = func(y: i32) -> i32 {
    return x + y
}
var result: i32 = add_x(5)  // 15

// 高阶函数
func apply(value: i32, f: func(i32) -> i32) -> i32 {
    return f(value)
}

var result: i32 = apply(5, func(x: i32) -> i32 { x * 2 })  // 10

// 返回闭包
func make_adder(x: i32) -> func(i32) -> i32 {
    return func(y: i32) -> i32 {
        return x + y
    }
}

var add_5 = make_adder(5)
var result: i32 = add_5(3)  // 8
```

### 4.10 范围表达式

```yuan
// 半开范围
var range = 0..10        // 0, 1, 2, ..., 9

// 闭合范围
var inclusive = 0..=10   // 0, 1, 2, ..., 10

// 用于循环
const io = @import("std").io
for i in 0..10 {
    io.println(i)
}

// 用于切片
var arr: [i32; 10] = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
var slice: &[i32] = &arr[2..5]  // [2, 3, 4]
```

---

## 5. 语句

### 5.1 变量声明

使用 `var` 声明可变变量，使用 `const` 声明不可变常量：

```yuan
// 可变变量（使用 var）
var x: i32 = 10
x = 20  // 可以重新赋值

// 不可变常量（使用 const）
const PI: f64 = 3.14159
// PI = 3.0  // 编译错误：常量不可修改

// 类型推断
var z = 42        // 推断为 i32
var s = "hello"   // 推断为 str

// 多重赋值
var (a, b, c) = (1, 2, 3)

// 解构赋值
var (x, y) = point
var [first, second, ...rest] = array
```

数组解构中的 `...rest` 最多出现一次，且必须位于最后一个位置。

### 5.2 常量声明

```yuan
// 常量必须初始化，编译时求值
const PI: f64 = 3.14159
const MAX_SIZE: usize = 1000
const GREETING: str = "Hello"

// 常量表达式
const DOUBLE_PI: f64 = PI * 2.0
const BUFFER_SIZE: usize = MAX_SIZE * 2

// 导入模块作为常量
const std = @import("std")
const io = @import("std").io
```

### 5.3 return 语句

```yuan
func add(a: i32, b: i32) -> i32 {
    return a + b
}

// 提前返回
func divide(a: i32, b: i32) -> !i32 {
    if b == 0 {
        return SysError.DivisionByZero
    }
    return a / b
}

// 最后一个表达式自动作为返回值
func multiply(a: i32, b: i32) -> i32 {
    a * b  // 自动返回
}
```

### 5.4 while 循环

```yuan
const io = @import("std").io

var i: i32 = 0
while i < 10 {
    io.println(i)
    i += 1
}

// 带条件的循环
var found: bool = false
while !found {
    // ...
    if condition {
        found = true
    }
}
```

### 5.5 loop 循环

无限循环，需要使用 break 退出：

```yuan
const io = @import("std").io

var count: i32 = 0
loop {
    io.println(count)
    count += 1

    if count >= 10 {
        break
    }
}

// loop 可以返回值
var result: i32 = loop {
    count += 1
    if count == 20 {
        break count * 2
    }
}
```

### 5.6 for 循环

```yuan
const io = @import("std").io
const fmt = @import("std").fmt

// 遍历范围
for i in 0..10 {
    io.println(i)
}

// 遍历数组
var numbers: [i32; 5] = [1, 2, 3, 4, 5]
for num in numbers {
    io.println(num)
}

// 遍历带索引
for (index, value) in numbers.iter().enumerate() {
    io.println(fmt.format("Index: {}, Value: {}", index, value))
}

// 遍历切片
var slice: &[i32] = &numbers[1..4]
for num in slice {
    io.println(num)
}

// 遍历字符串字符
for ch in "hello".chars() {
    io.println(ch)
}
```

### 5.7 break 和 continue

```yuan
const io = @import("std").io

// break：退出循环
for i in 0..10 {
    if i == 5 {
        break
    }
    io.println(i)
}

// continue：跳过当前迭代
for i in 0..10 {
    if i % 2 == 0 {
        continue
    }
    io.println(i)  // 只打印奇数
}

// 带标签的 break/continue
outer: for i in 0..10 {
    for j in 0..10 {
        if i * j > 50 {
            break outer
        }
    }
}
```

### 5.8 defer 语句

defer 语句用于延迟执行，在当前作用域结束时执行：

```yuan
func read_file(path: str) -> !String {
    const fs = @import("std").fs
    var file = fs.open(path, "r")!
    defer file.close()  // 函数返回前自动关闭文件

    var content = file.read_all()!
    return content
}

// 多个 defer 按 LIFO 顺序执行
func example() {
    const io = @import("std").io
    defer io.println("first")   // 最后执行
    defer io.println("second")  // 第二个执行
    defer io.println("third")   // 第一个执行
    // 输出顺序：third, second, first
}
```

---

## 6. 函数

### 6.1 函数定义

```yuan
// 基本函数
func add(a: i32, b: i32) -> i32 {
    return a + b
}

// 无返回值
func print_hello() -> void {
    const io = @import("std").io
    io.println("Hello")
}

// 省略 void 返回类型
func greet() {
    const io = @import("std").io
    io.println("Hello")
}

// 最后一个表达式自动作为返回值
func multiply(a: i32, b: i32) -> i32 {
    a * b
}

// 可能返回错误的函数（使用 ! 标记）
func divide(a: i32, b: i32) -> !i32 {
    if b == 0 {
        return SysError.DivisionByZero
    }
    return a / b
}
```

### 6.2 参数

#### 6.2.1 普通参数（默认不可变）

函数参数默认是不可变的：

```yuan
func greet(name: str, age: i32) {
    // name 和 age 在函数内不可修改
    const fmt = @import("std").fmt
    const io = @import("std").io
    io.println(fmt.format("Hello {}, you are {} years old", name, age))
}
```

#### 6.2.2 可变引用参数（统一使用 `&mut T`）

参数可变性统一通过 `&mut T` 表达，不再使用 `mut T` 按值参数：

```yuan
func increment(value: &mut i32) {
    value += 1
}

func swap(a: &mut i32, b: &mut i32) {
    var temp: i32 = a
    a = b
    b = temp
}

// 调用
var x: i32 = 10
increment(&mut x)  // x 现在是 11

var a: i32 = 1
var b: i32 = 2
swap(&mut a, &mut b)  // a=2, b=1
```

#### 6.2.3 默认参数

```yuan
func greet(name: str, greeting: str = "Hello") {
    const fmt = @import("std").fmt
    const io = @import("std").io
    io.println(fmt.format("{}, {}!", greeting, name))
}

greet("Alice")              // 使用默认值
greet("Bob", "Hi")          // 提供值
greet("Charlie", greeting="Hey")  // 关键字参数
```

#### 6.2.4 引用参数

```yuan
const io = @import("std").io

// 不可变引用
func print_array(arr: &[i32]) {
    for item in arr {
        io.println(item)
    }
}

// 可变引用（mut 标注在类型前）
func double_array(arr: &mut [i32]) {
    for i in 0..arr.len() {
        arr[i] *= 2
    }
}

var numbers: [i32; 5] = [1, 2, 3, 4, 5]
double_array(&mut numbers)
```

#### 6.2.5 可变参数函数

可变参数必须位于参数列表最后，支持两种形式：

- 同构可变参数：`...name: T`，同一次调用中的实参类型必须一致；在函数体内类型为只读切片 `&[T]`。
- 异构可变参数：`...name`，允许实参类型不一致；在函数体内表现为只读参数包（tuple pack）。

异构参数包支持：

- `args.len()`：获取参数个数（编译期常量）。
- 解构：`var (a, b, c) = args`（参数个数需匹配）。
- 转发：在调用处使用 `...args` 展开参数包并传给另一个可变参数函数。

```yuan
const io = @import("std").io
const fmt = @import("std").fmt

// 同构可变参数函数定义
func print_all<T>(prefix: str, ...args: T) {
    io.print(prefix)
    for arg in args {
        io.print(" ")
        io.print(arg)
    }
    io.println("")
}

// 调用
print_all("Words:", "hello", "yuan", "lang")

// 同构数值可变参数
func sum_all(...numbers: i32) -> i32 {
    var total: i32 = 0
    for num in numbers {
        total += num
    }
    return total
}

var result: i32 = sum_all(1, 2, 3, 4, 5)  // 15

// 异构可变参数：允许类型不一致
func logf(fmt_str: str, ...args) {
    io.println(fmt.format(fmt_str, ...args))
}

logf("Mixed: {}, {}, {}", "Alice", 30, true)
```

### 6.3 返回值必须处理

Yuan 强制要求处理函数返回值（除非返回 void）：

```yuan
func get_value() -> i32 {
    return 42
}

// 正确：使用返回值
var value: i32 = get_value()

// 正确：显式丢弃返回值
_ = get_value()

// 编译错误：未处理返回值
// get_value()  // Error: return value must be used
```

### 6.4 泛型函数

```yuan
func swap<T>(a: &mut T, b: &mut T) {
    var temp: T = a
    a = b
    b = temp
}

func max<T: Ord>(a: T, b: T) -> T {
    if a > b { a } else { b }
}

// 多个类型参数
func pair<T, U>(first: T, second: U) -> (T, U) {
    (first, second)
}
```

### 6.5 可见性

```yuan
// 公开函数（默认私有）
pub func public_function() {
    // 可以被其他模块访问
}

// 私有函数
priv func private_function() {
    // 只能在当前文件内访问
}

// 包内可见
internal func internal_function() {
    // 只能在当前包内访问
}
```

`internal` 的边界由构建系统定义的“包（package）”决定，通常对应同一个编译目标（库或可执行程序）。

---

## 7. 结构体

### 7.1 结构体定义

```yuan
// 基本结构体
struct Point {
    pub x: f64,
    pub y: f64
}

// 带私有字段
struct Person {
    pub name: String,
    pub age: u32,
    priv id: u64
}

// 带默认值
struct Config {
    pub host: String = String.from("localhost"),
    pub port: u16 = 8080,
    pub debug: bool = false
}

// 元组结构体
struct Color(u8, u8, u8)

// 单元结构体
struct Marker
```

### 7.2 结构体实例化

```yuan
// 使用字段名初始化
var p1: Point = Point { x: 10.0, y: 20.0 }

// 字段顺序可以任意
var p2: Point = Point { y: 30.0, x: 40.0 }

// 字段简写
var x: f64 = 10.0
var y: f64 = 20.0
var p3: Point = Point { x, y }

// 实例化带默认值的结构体
var config1: Config = Config { }
var config2: Config = Config { port: 3000 }

// 元组结构体
var red: Color = Color(255, 0, 0)
```

### 7.3 方法

```yuan
struct Rectangle {
    pub width: f64,
    pub height: f64
}

impl Rectangle {
    // 关联函数（静态方法）
    pub func new(width: f64, height: f64) -> Rectangle {
        Rectangle { width, height }
    }

    pub func square(size: f64) -> Rectangle {
        Rectangle { width: size, height: size }
    }

    // 实例方法（不可变 self）
    pub func area(self: &Self) -> f64 {
        self.width * self.height
    }

    pub func perimeter(self: &Self) -> f64 {
        2.0 * (self.width + self.height)
    }

    // 可变方法（mut 标注在类型前）
    pub func scale(self: &mut Self, factor: f64) {
        self.width *= factor
        self.height *= factor
    }

    // 消耗 self 的方法
    pub func into_tuple(self: Self) -> (f64, f64) {
        (self.width, self.height)
    }
}

// 使用
var rect: Rectangle = Rectangle.new(10.0, 20.0)
var area: f64 = rect.area()
var perimeter: f64 = rect.perimeter()

var rect2: Rectangle = Rectangle.square(10.0)
rect2.scale(2.0)
```

### 7.4 泛型结构体

```yuan
struct Pair<T, U> {
    pub first: T,
    pub second: U
}

impl<T, U> Pair<T, U> {
    pub func new(first: T, second: U) -> Pair<T, U> {
        Pair { first, second }
    }
}

var pair: Pair<i32, str> = Pair.new(42, "hello")
```

---

## 8. 枚举

### 8.1 枚举定义

```yuan
// 简单枚举
enum Color {
    Red,
    Green,
    Blue
}

// 带数据的枚举
enum Message {
    Quit,
    Move { x: i32, y: i32 },
    Write(String),
    ChangeColor(u8, u8, u8)
}

// 泛型枚举
enum Optional<T> {
    Some(T),
    None
}
```

### 8.2 枚举使用

```yuan
// 创建枚举值
var color: Color = Color.Red
var msg: Message = Message.Write(String.from("Hello"))
var response: Message = Message.Move { x: 10, y: 20 }

// 模式匹配
const fmt = @import("std").fmt
const io = @import("std").io

match msg {
    Message.Quit => io.println("Quit"),
    Message.Move { x, y } => io.println(fmt.format("Move to ({}, {})", x, y)),
    Message.Write(text) => io.println(fmt.format("Write: {}", text)),
    Message.ChangeColor(r, g, b) => io.println(fmt.format("Color: ({}, {}, {})", r, g, b))
}
```

### 8.3 枚举方法

```yuan
enum Direction {
    North,
    South,
    East,
    West
}

impl Direction {
    pub func opposite(self: &Self) -> Direction {
        match self {
            Direction.North => Direction.South,
            Direction.South => Direction.North,
            Direction.East => Direction.West,
            Direction.West => Direction.East
        }
    }

    pub func to_degrees(self: &Self) -> i32 {
        match self {
            Direction.North => 0,
            Direction.East => 90,
            Direction.South => 180,
            Direction.West => 270
        }
    }
}
```

---

## 9. Trait

### 9.1 Trait 定义

```yuan
// 基本 trait
trait Display {
    func display(self: &Self) -> String
}

// 带多个方法的 trait
trait Shape {
    func area(self: &Self) -> f64
    func perimeter(self: &Self) -> f64
}

// 带默认实现的 trait
trait Greet {
    func greet(self: &Self) -> String {
        String.from("Hello!")
    }

    func greet_with_name(self: &Self, name: &str) -> String
}

// 带关联类型的 trait
trait Iterator {
    type Item

    func next(self: &mut Self) -> ?Self.Item
}
```

### 9.2 Trait 实现

```yuan
struct Circle {
    pub radius: f64
}

impl Shape for Circle {
    func area(self: &Self) -> f64 {
        3.14159 * self.radius * self.radius
    }

    func perimeter(self: &Self) -> f64 {
        2.0 * 3.14159 * self.radius
    }
}

impl Display for Circle {
    func display(self: &Self) -> String {
        const fmt = @import("std").fmt
        fmt.format("Circle(radius={})", self.radius)
    }
}

// 使用
var circle: Circle = Circle { radius: 5.0 }
var area: f64 = circle.area()
var perimeter: f64 = circle.perimeter()
var display: String = circle.display()
```

### 9.3 Trait 约束

```yuan
const io = @import("std").io
const fmt = @import("std").fmt

// 泛型函数带 trait 约束
func print_area<T: Shape>(shape: &T) {
    io.println(fmt.format("Area: {}", shape.area()))
}

// 多个 trait 约束
func describe<T: Shape + Display>(shape: &T) {
    io.println(fmt.format("{}: area = {}", shape.display(), shape.area()))
}

// where 子句
func complex_function<T, U>(t: T, u: U) -> i32
where
    T: Shape + Clone,
    U: Display
{
    // ...
}
```

### 9.4 常用内置 Trait

```yuan
// Clone - 深拷贝
trait Clone {
    func clone(self: &Self) -> Self
}

// Copy - 按位复制（标记 trait）
trait Copy: Clone { }

// Eq - 相等性比较
trait Eq {
    func eq(self: &Self, other: &Self) -> bool
}

// Ord - 排序比较
trait Ord: Eq {
    func cmp(self: &Self, other: &Self) -> Ordering
}

// Default - 默认值
trait Default {
    func default() -> Self
}

// Drop - 析构
trait Drop {
    func drop(self: &mut Self)
}

// Error - 错误类型（详见错误处理章节）
trait Error {
    func message(self: &Self) -> str
    // &Error 表示 trait 对象引用
    func source(self: &Self) -> ?&Error
}
```

---

## 10. 模块系统

### 10.1 导入模块

Yuan 使用 `@import` 内置函数导入模块：

```yuan
// 导入标准库
const std = @import("std")
const io = @import("std").io
const fmt = @import("std").fmt

// 导入特定项
const Vec = @import("std").collections.Vec
const HashMap = @import("std").collections.HashMap

// 导入本地模块（相对路径，使用 .yu 扩展名）
const utils = @import("./utils/math.yu")
const models = @import("./models/user.yu")

// 导入并使用
const fs = @import("std").fs
var content = fs.read_to_string("file.txt")!
```

### 10.2 模块定义

```yuan
// 文件: math.yu
pub const PI: f64 = 3.14159

pub func add(a: i32, b: i32) -> i32 {
    a + b
}

priv func internal_helper() {
    // 私有函数
}

pub struct Point {
    pub x: f64,
    pub y: f64
}
```

### 10.3 使用导入的模块

```yuan
// main.yu
const math = @import("./math.yu")
const io = @import("std").io
const fmt = @import("std").fmt

func main() {
    var result: i32 = math.add(10, 20)
    io.println(fmt.format("PI = {}", math.PI))

    var p: math.Point = math.Point { x: 1.0, y: 2.0 }
}
```

---

## 11. 错误处理

Yuan 使用基于 `Error` trait 的显式错误处理机制，不支持异常。

### 11.1 Error Trait

```yuan
/// 所有错误类型必须实现此 trait
trait Error {
    /// 返回错误消息
    func message(self: &Self) -> str

    /// 返回导致此错误的源错误（如果有）
    func source(self: &Self) -> ?&Error {
        None
    }
}
```

`&Error` 表示 Error trait 对象引用（动态分发）。

### 11.2 SysError 系统错误枚举

Yuan 提供内置的 `SysError` 枚举，包含常见的系统错误：

```yuan
/// 系统内置错误类型
enum SysError {
    // I/O 错误
    FileNotFound { path: str },
    PermissionDenied { path: str },
    IoError { message: str },

    // 数值错误
    DivisionByZero,
    Overflow,
    Underflow,

    // 内存错误
    OutOfMemory,
    NullPointer,

    // 索引错误
    IndexOutOfBounds { index: usize, len: usize },

    // 解析错误
    ParseError { message: str },
    InvalidFormat { expected: str, found: str },

    // 网络错误
    ConnectionFailed { host: str, port: u16 },
    Timeout { duration_ms: u64 },

    // 通用错误
    InvalidArgument { name: str, message: str },
    NotImplemented { feature: str },
    Unknown { message: str }
}

impl Error for SysError {
    func message(self: &Self) -> str {
        match self {
            SysError.FileNotFound { path } => "File not found",
            SysError.DivisionByZero => "Division by zero",
            SysError.IndexOutOfBounds { index, len } => "Index out of bounds",
            // ... 其他错误消息
            _ => "Unknown error"
        }
    }
}
```

### 11.3 ErrorInfo 错误信息结构

为了支持获取出错的函数名和位置，Yuan 使用 `ErrorInfo` 结构包装错误：

```yuan
const Vec = @import("std").collections.Vec

/// 错误信息包装器，包含错误发生的上下文
struct ErrorInfo<E: Error> {
    /// 原始错误
    pub error: E,
    /// 发生错误的函数名
    pub func_name: str,
    /// 发生错误的文件名
    pub file: str,
    /// 发生错误的行号
    pub line: u32,
    /// 发生错误的列号
    pub column: u32,
    /// 错误链（链式调用中的错误传播路径）
    pub trace: Vec<ErrorLocation>
}

struct ErrorLocation {
    pub func_name: str,
    pub file: str,
    pub line: u32,
    pub column: u32
}

impl<E: Error> ErrorInfo<E> {
    /// 透传错误消息
    pub func message(self: &Self) -> str {
        self.error.message()
    }

    /// 获取完整的错误追踪信息
    pub func full_trace(self: &Self) -> String {
        // 返回格式化的错误追踪
    }

    /// 获取原始错误
    pub func unwrap_error(self: &Self) -> &E {
        &self.error
    }
}
```

### 11.4 可能返回错误的函数

使用 `!` 标记在返回类型前表示函数可能返回错误：

```yuan
// 可能返回错误的函数
func divide(a: i32, b: i32) -> !i32 {
    if b == 0 {
        return SysError.DivisionByZero
    }
    return a / b
}

// 可能返回自定义错误
func parse_number(s: str) -> !i32 {
    if !is_valid_number(s) {
        return SysError.ParseError { message: "Invalid number format" }
    }
    return do_parse(s)
}

// 返回 void 但可能出错
func write_file(path: str, content: str) -> !void {
    const fs = @import("std").fs
    fs.write(path, content)!
}
```

### 11.5 错误处理语法

#### 11.5.1 使用 `!` 传播错误

在函数调用后加 `!` 表示：如果函数返回错误，则将错误传播给调用者：

```yuan
func read_and_parse(path: str) -> !i32 {
    const fs = @import("std").fs

    // 使用 ! 传播错误（后缀操作符）
    // 如果 read_to_string 返回错误，read_and_parse 也返回该错误
    var content: String = fs.read_to_string(path)!

    // 继续传播
    var number: i32 = parse_number(content.as_str())!

    return number
}
```

#### 11.5.2 不使用 `!` 时的行为

如果调用可能出错的函数但不使用 `!`，且函数返回了错误，则触发 panic：

```yuan
func risky_call() {
    // 如果 divide 返回错误，程序 panic
    var result: i32 = divide(10, 0)  // panic: Division by zero
}
```

#### 11.5.3 使用 `-> err {}` 处理错误

在函数调用后使用 `!` 和 `-> err {}` 语法捕获并处理错误：

- `err` 是编译器注入的 `ErrorInfo` 值，包含 `error`、`func_name`、`file`、`line`、`column`、`trace` 字段。
- `err.message()` 等价于 `err.error.message()`。

```yuan
const io = @import("std").io

func safe_divide(a: i32, b: i32) -> i32 {
    var result: i32 = divide(a, b)! -> err {
        io.println(err.message())
        io.println(err.func_name)      // "divide"
        io.println(err.file)           // 源文件名
        io.println(err.line)           // 行号
        0  // 返回默认值，函数继续执行
    }
    return result
}

// 在错误处理块中直接返回
func try_read_file(path: str) -> String {
    const fs = @import("std").fs
    const io = @import("std").io

    var content: String = fs.read_to_string(path)! -> err {
        io.println(err.full_trace())
        return String.from("")  // 直接从 try_read_file 返回
    }

    return content
}
```

#### 11.5.4 链式调用的错误处理

对于链式调用，在每个可能出错的函数后加 `!`：

```yuan
func process_data(path: str) -> !String {
    const fs = @import("std").fs

    // 对可能出错的调用使用后缀 !
    var content: String = fs.read_to_string(path)!
    var result: String = content.trim().to_uppercase()

    return result
}

// 链式调用的错误处理
func safe_process(path: str) -> String {
    const fs = @import("std").fs
    const io = @import("std").io

    var content: String = fs.read_to_string(path)! -> err {
        io.println(err.full_trace())
        return String.from("default")
    }

    var result: String = content.trim().to_uppercase()

    return result
}
```

### 11.6 自定义错误类型

```yuan
// 定义自定义错误枚举
enum ValidationError {
    EmptyField { field_name: str },
    InvalidEmail { email: str },
    TooShort { field_name: str, min_length: usize, actual_length: usize },
    TooLong { field_name: str, max_length: usize, actual_length: usize }
}

impl Error for ValidationError {
    func message(self: &Self) -> str {
        match self {
            ValidationError.EmptyField { field_name } => "Field cannot be empty",
            ValidationError.InvalidEmail { email } => "Invalid email format",
            ValidationError.TooShort { .. } => "Value is too short",
            ValidationError.TooLong { .. } => "Value is too long"
        }
    }
}

// 使用自定义错误
func validate_email(email: str) -> !str {
    if email.len() == 0 {
        return ValidationError.EmptyField { field_name: "email" }
    }
    if !email.contains("@") {
        return ValidationError.InvalidEmail { email: email }
    }
    return email
}
```

---

## 12. 并发

### 12.1 async/await

```yuan
const io = @import("std").io
const fmt = @import("std").fmt

// 定义异步函数
async func fetch_data(url: &str) -> !String {
    const http = @import("std").net.http

    var response = (await http.get_async(url))!
    if response.status != 200 {
        return SysError.ConnectionFailed { host: url, port: 80 }
    }
    return response.body
}

// 调用异步函数
async func main() -> !void {
    var data = (await fetch_data("https://example.com"))! -> err {
        io.println(fmt.format("Error: {}", err.message()))
        return
    }

    io.println(fmt.format("Data: {}", data))
}
```

### 12.2 线程

```yuan
const thread = @import("std").thread
const io = @import("std").io
const channel = @import("std").sync.channel

// 创建线程
var handle = thread.spawn(func() {
    io.println("Hello from thread!")
})

// 等待线程完成
_ = handle.join()

// 带返回值的线程
var handle = thread.spawn(func() -> i32 {
    42
})
var result: i32 = handle.join()

// 线程间通信（通道）
var (tx, rx) = channel.create<i32>()

_ = thread.spawn(func() {
    tx.send(42)
})

var received: i32 = rx.recv()
```

---

## 13. 内置函数

内置函数以 `@` 开头，由编译器直接实现。

### 13.1 核心内置函数

```yuan
// 模块导入
const std = @import("std")
const math = @import("./math.yu")

// 类型信息
var size: usize = @sizeof(i32)           // 获取类型大小（字节）
var type_name: str = @typeof(value)      // 获取值的类型名称

// 程序控制
@panic("Something went wrong")           // 触发 panic，终止程序

// 断言
@assert(condition)                       // 断言，失败时 panic
@assert(condition, "Error message")      // 带消息的断言

// 编译时信息
const file: str = @file()                // 当前文件名
const line: u32 = @line()                // 当前行号
const column: u32 = @column()            // 当前列号
const func_name: str = @func()           // 当前函数名
```

### 13.2 内置函数详解

#### @import

导入模块：

```yuan
// 导入标准库模块
const std = @import("std")
const io = @import("std").io

// 导入本地模块
const utils = @import("./utils.yu")
const config = @import("../config.yu")
```

#### @sizeof

获取类型或表达式的大小（以字节为单位）：

```yuan
// 使用类型参数
var int_size: usize = @sizeof(i32)       // 4
var ptr_size: usize = @sizeof(*i32)      // 8 (64位系统)
var struct_size: usize = @sizeof(Point)  // 取决于结构体定义

// 使用表达式参数
var x: i32 = 42
var x_size: usize = @sizeof(x)           // 4，等同于 @sizeof(i32)

var arr = [1, 2, 3, 4, 5]
var arr_size: usize = @sizeof(arr)       // 20，等同于 @sizeof([i32; 5])
```

#### @typeof

获取表达式的类型名称：

```yuan
var x: i32 = 42
var type_name: str = @typeof(x)          // "i32"

var arr = [1, 2, 3]
var arr_type: str = @typeof(arr)         // "[i32; 3]"

// @typeof 只接受表达式参数，不接受类型参数
// var invalid = @typeof(i32)            // 编译错误
```

#### @panic

触发 panic，终止程序执行：

```yuan
if critical_error {
    @panic("Critical error occurred")
}
```

#### @assert

断言检查：

```yuan
@assert(x > 0)                           // 如果 x <= 0，panic
@assert(x > 0, "x must be positive")     // 带自定义消息
```

---

## 14. 标准库

### 14.1 预导入（Prelude）

以下类型自动可用，无需导入：

```yuan
// 基本类型
bool, char, str
i8, i16, i32, i64, i128, isize
u8, u16, u32, u64, u128, usize
f32, f64

// 常用类型
String
SysError
Error (trait)
```

### 14.2 核心模块

#### 14.2.1 std.io

```yuan
const io = @import("std").io

// 打印输出
io.print("Hello")                        // 打印（不换行）
io.println("Hello, World!")              // 打印并换行
io.println(42)                           // 打印任意类型
io.eprintln("Error message")             // 打印到标准错误

// 读取输入
var input: String = io.stdin().read_line()!

// 文件操作
var file = io.File.open("file.txt", "r")!
var content: String = file.read_to_string()!
file.close()
```

#### 14.2.2 std.fs

```yuan
const fs = @import("std").fs

// 读取文件
var content: String = fs.read_to_string("file.txt")!

// 写入文件
fs.write("file.txt", "Hello, World!")!

// 文件信息
var exists: bool = fs.exists("file.txt")
var is_file: bool = fs.is_file("file.txt")
var is_dir: bool = fs.is_dir("directory")

// 目录操作
fs.create_dir("new_dir")!
fs.create_dir_all("path/to/dir")!
fs.remove_dir("old_dir")!
var entries = fs.read_dir("directory")!
```

#### 14.2.3 std.collections

```yuan
const Vec = @import("std").collections.Vec
const HashMap = @import("std").collections.HashMap
const HashSet = @import("std").collections.HashSet
const io = @import("std").io

// Vec - 动态数组
var v: Vec<i32> = Vec.new()
v.push(1)
v.push(2)
var first: i32 = v.get(0)
var popped: i32 = v.pop()

// HashMap - 哈希映射
var map: HashMap<str, i32> = HashMap.new()
map.insert("one", 1)
map.insert("two", 2)
var value: ?i32 = map.get("one")

// HashSet - 哈希集合
var set: HashSet<i32> = HashSet.new()
set.insert(1)
set.insert(2)
var contains: bool = set.contains(1)
```

#### 14.2.4 std.fmt

```yuan
const fmt = @import("std").fmt

// 格式化字符串
var s: String = fmt.format("Value: {}", 42)
var hex: String = fmt.format("{:x}", 255)        // "ff"
var padded: String = fmt.format("{:05}", 42)     // "00042"
var float_fmt: String = fmt.format("{:.2}", 3.14159) // "3.14"

// 多个参数
var msg: String = fmt.format("Name: {}, Age: {}", "Alice", 30)
```

#### 14.2.5 std.time

```yuan
const time = @import("std").time
const thread = @import("std").thread
const io = @import("std").io
const fmt = @import("std").fmt

// 当前时间
var now = time.Instant.now()

// 持续时间
var duration = time.Duration.from_secs(5)
var millis = time.Duration.from_millis(100)

// 计时
var start = time.Instant.now()
// ... 操作 ...
var elapsed = start.elapsed()

io.println(fmt.format("Elapsed: {}s", elapsed.as_secs_f64()))

// 睡眠
thread.sleep(time.Duration.from_secs(1))
```

#### 14.2.6 std.math

```yuan
const math = @import("std").math

// 常量
const PI: f64 = math.PI
const E: f64 = math.E

// 基本函数
var sqrt_val: f64 = math.sqrt(16.0)
var pow_val: f64 = math.pow(2.0, 3.0)    // 幂运算使用函数
var log_val: f64 = math.log(10.0)
var sin_val: f64 = math.sin(math.PI / 2.0)
var cos_val: f64 = math.cos(0.0)

// 取整
var ceil_val: f64 = math.ceil(3.2)
var floor_val: f64 = math.floor(3.8)
var round_val: f64 = math.round(3.5)

// 最值
var max_val: i32 = math.max(10, 20)
var min_val: i32 = math.min(10, 20)
var abs_val: i32 = math.abs(-42)
```

---

## 15. 完整示例

### 15.1 基础示例

```yuan
// 简单的计算器程序
const fmt = @import("std").fmt
const io = @import("std").io

func add(a: i32, b: i32) -> i32 {
    a + b
}

func subtract(a: i32, b: i32) -> i32 {
    a - b
}

func multiply(a: i32, b: i32) -> i32 {
    a * b
}

func divide(a: f64, b: f64) -> !f64 {
    if b == 0.0 {
        return SysError.DivisionByZero
    }
    return a / b
}

func main() {
    var x: i32 = 10
    var y: i32 = 5

    io.println(fmt.format("Add: {}", add(x, y)))
    io.println(fmt.format("Subtract: {}", subtract(x, y)))
    io.println(fmt.format("Multiply: {}", multiply(x, y)))

    var div_result: f64 = divide(x as f64, y as f64)! -> err {
        io.println(fmt.format("Error: {}", err.message()))
        return
    }
    io.println(fmt.format("Divide: {}", div_result))

    // 测试除零错误
    _ = divide(x as f64, 0.0)! -> err {
        io.println(fmt.format("Error: {}", err.message()))
    }
}
```

### 15.2 结构体和 Trait 示例

```yuan
const fmt = @import("std").fmt
const io = @import("std").io

trait Shape {
    func area(self: &Self) -> f64
    func perimeter(self: &Self) -> f64
    func name(self: &Self) -> str
}

struct Rectangle {
    pub width: f64,
    pub height: f64
}

impl Rectangle {
    pub func new(width: f64, height: f64) -> Rectangle {
        Rectangle { width, height }
    }

    pub func square(size: f64) -> Rectangle {
        Rectangle { width: size, height: size }
    }
}

impl Shape for Rectangle {
    func area(self: &Self) -> f64 {
        self.width * self.height
    }

    func perimeter(self: &Self) -> f64 {
        2.0 * (self.width + self.height)
    }

    func name(self: &Self) -> str {
        "Rectangle"
    }
}

struct Circle {
    pub radius: f64
}

impl Circle {
    pub func new(radius: f64) -> Circle {
        Circle { radius }
    }
}

impl Shape for Circle {
    func area(self: &Self) -> f64 {
        3.14159 * self.radius * self.radius
    }

    func perimeter(self: &Self) -> f64 {
        2.0 * 3.14159 * self.radius
    }

    func name(self: &Self) -> str {
        "Circle"
    }
}

func print_shape_info<T: Shape>(shape: &T) {
    io.println(fmt.format("{}:", shape.name()))
    io.println(fmt.format("  Area: {:.2}", shape.area()))
    io.println(fmt.format("  Perimeter: {:.2}", shape.perimeter()))
}

func main() {
    var rect = Rectangle.new(10.0, 5.0)
    var circle = Circle.new(7.0)
    var square = Rectangle.square(4.0)

    print_shape_info(&rect)
    print_shape_info(&circle)
    print_shape_info(&square)
}
```

### 15.3 可变参数函数示例

```yuan
const fmt = @import("std").fmt
const io = @import("std").io

// 同构可变参数（参数类型一致）
func print_all<T>(prefix: str, ...args: T) {
    io.print(prefix)
    for arg in args {
        io.print(" ")
        io.print(arg)
    }
    io.println("")
}

// 求和函数
func sum(...numbers: i32) -> i32 {
    var total: i32 = 0
    for num in numbers {
        total += num
    }
    return total
}

// 查找最大值
func max_of(...values: i32) -> ?i32 {
    if values.len() == 0 {
        return None
    }
    var max_val: i32 = values[0]
    for val in values {
        if val > max_val {
            max_val = val
        }
    }
    return max_val
}

// 异构可变参数（参数类型可不一致）
func logf(fmt_str: str, ...args) {
    io.println(fmt.format(fmt_str, ...args))
}

func main() {
    print_all("Values:", 1, 2, 3, 4, 5)
    print_all("Words:", "hello", "yuan", "lang")
    logf("Mixed: {}, {}, {}", "hello", 42, true)

    var total: i32 = sum(1, 2, 3, 4, 5)
    io.println(fmt.format("Sum: {}", total))

    var max_val: i32 = max_of(3, 1, 4, 1, 5, 9, 2, 6) orelse 0
    io.println(fmt.format("Max: {}", max_val))
}
```

### 15.4 错误处理示例

```yuan
const fs = @import("std").fs
const fmt = @import("std").fmt
const io = @import("std").io

// 自定义错误类型
enum ConfigError {
    FileNotFound { path: str },
    ParseError { line: u32, message: str },
    MissingField { field: str }
}

impl Error for ConfigError {
    func message(self: &Self) -> str {
        match self {
            ConfigError.FileNotFound { .. } => "Configuration file not found",
            ConfigError.ParseError { .. } => "Failed to parse configuration",
            ConfigError.MissingField { .. } => "Required field is missing"
        }
    }
}

struct Config {
    pub host: String,
    pub port: u16,
    pub debug: bool
}

impl Config {
    pub func default() -> Config {
        Config {
            host: String.from("localhost"),
            port: 8080,
            debug: false
        }
    }
}

func load_config(path: str) -> !Config {
    var content: String = fs.read_to_string(path)!
    var config = parse_config(content.as_str())!
    return config
}

func parse_config(content: str) -> !Config {
    if content.len() == 0 {
        return ConfigError.ParseError { line: 1, message: "Empty file" }
    }
    return Config.default()
}

func main() {
    var config: Config = load_config("config.txt")! -> err {
        io.println(fmt.format("Warning: {}", err.message()))
        io.println(fmt.format("  at {}:{}", err.file, err.line))
        io.println("Using default configuration")
        return Config.default()
    }

    io.println(fmt.format("Host: {}", config.host))
    io.println(fmt.format("Port: {}", config.port))
    io.println(fmt.format("Debug: {}", config.debug))
}
```

---

## 16. 附录

### 16.1 命名约定

- **变量和函数**：使用 snake_case
  - `var user_name: str = "Alice"`
  - `func calculate_sum() { }`

- **常量**：使用 SCREAMING_SNAKE_CASE
  - `const MAX_SIZE: usize = 1000`
  - `const PI: f64 = 3.14159`

- **类型（结构体、枚举、Trait）**：使用 PascalCase
  - `struct UserProfile { }`
  - `enum HttpStatus { }`
  - `trait Serializable { }`

- **内置函数**：使用 @snake_case
  - `@import`, `@sizeof`, `@typeof`, `@panic`

### 16.2 与其他语言的对比

#### 与 Rust 的主要区别

1. **无生命周期**：Yuan 不需要生命周期标注
2. **无借用检查**：不做 Rust 风格借用检查，引用模型借鉴 Zig
3. **无 unsafe**：Yuan 不提供 unsafe 块（但指针与并发正确性由开发者负责）
4. **无宏**：Yuan 不支持宏系统
5. **变量声明**：使用 `var`/`const` 区分可变性
6. **可变引用参数**：统一使用 `&mut T`
7. **内置函数**：使用 `@` 前缀的内置函数
8. **模块导入**：使用 `@import` 而非 `use`

#### 与 Zig 的主要区别

1. **错误处理**：Yuan 使用 `!` 和 `-> err {}` 语法
2. **面向对象**：Yuan 支持 Trait 和方法
3. **语法风格**：更接近 Rust 的语法
4. **标准库组织**：以 Trait + 模块组合为核心

#### 与 YuanScript 的主要区别

1. **类型系统**：静态类型而非动态类型
2. **数据结构**：移除 `dict` 和 `list`，添加数组、切片、Vec、HashMap
3. **错误处理**：基于 Error trait，无 try-catch
4. **编译**：编译型而非解释型
5. **无装饰器和宏**
6. **无字符串插值**：使用 `fmt.format()` 替代
7. **无幂运算符**：使用 `math.pow()` 函数

### 16.3 运算符优先级

从高到低：

| 优先级 | 运算符 | 说明 |
|--------|--------|------|
| 1 | `()` `[]` `.` `!`(后缀) | 函数调用、索引、成员访问、错误传播 |
| 2 | `-` `!` `~` `*` `&` `&mut` | 一元运算符（含逻辑非） |
| 3 | `as` | 类型转换 |
| 4 | `*` `/` `%` | 乘法、除法、取模 |
| 5 | `+` `-` | 加法、减法 |
| 6 | `<<` `>>` | 位移 |
| 7 | `&` | 按位与 |
| 8 | `^` | 按位异或 |
| 9 | `\|` | 按位或 |
| 10 | `<` `>` `<=` `>=` | 关系运算 |
| 11 | `==` `!=` | 相等性 |
| 12 | `&&` | 逻辑与 |
| 13 | `\|\|` | 逻辑或 |
| 14 | `orelse` | Optional 默认值 |
| 15 | `..` `..=` | 范围 |
| 16 | `=` `+=` `-=` `*=` `/=` `%=` `&=` `\|=` `^=` `<<=` `>>=` | 赋值 |

### 16.4 错误处理语法总结

| 语法 | 说明 |
|------|------|
| `func f(...) -> !T` | 函数可能返回类型 T 或错误 |
| `func()!` | 调用函数，如果出错则传播错误给调用者 |
| `func()` | 调用可能出错的函数，如果出错则 panic |
| `func()! -> err { }` | 调用函数，如果出错则执行错误处理块 |
| `err.message()` | 获取错误消息 |
| `err.func_name` | 获取出错的函数名 |
| `err.file` | 获取出错的文件名 |
| `err.line` | 获取出错的行号 |
| `err.full_trace()` | 获取完整的错误追踪信息 |

### 16.5 内置函数总结

| 函数 | 说明 |
|------|------|
| `@import(path)` | 导入模块 |
| `@sizeof(T)` | 获取类型大小 |
| `@typeof(value)` | 获取值的类型名称 |
| `@panic(message)` | 触发 panic |
| `@assert(cond)` | 断言 |
| `@assert(cond, msg)` | 带消息的断言 |
| `@file()` | 当前文件名 |
| `@line()` | 当前行号 |
| `@column()` | 当前列号 |
| `@func()` | 当前函数名 |

### 16.6 文件扩展名

- `.yu` - Yuan 源代码文件

---

## 17. 实现说明：语义分析与代码生成

> 本章描述当前编译器实现的关键行为，用于解释“规范如何落地”。  
> 当本章与前文语法描述存在差异时，以编译器当前实现为准，并在后续版本继续收敛。

### 17.1 编译流水线（实现视角）

Yuan 当前编译流程为：

1. `Lexer`：词法切分，产生 Token 流  
2. `Parser`：构建 AST  
3. `Sema`：语义分析、类型检查、符号绑定  
4. `CodeGen`：AST 降低为 LLVM IR  
5. LLVM 目标文件生成与链接（含 runtime）

其中语义分析和代码生成是“语言行为最终落地”的核心阶段。

### 17.2 语义分析（Sema）关键规则

#### 17.2.1 作用域与符号

- Sema 使用层级作用域（全局、函数、块、循环、trait、impl 等）。
- `break/continue` 的合法性依赖循环作用域栈；带标签时向外逐层匹配。
- 标识符解析成功后，会把 `ResolvedDecl` 写回表达式节点，供后端直接使用。

#### 17.2.2 类型解析与类型兼容

- 所有 `TypeNode` 都先解析为语义类型 `Type`。
- 赋值/参数传递/返回检查统一通过类型兼容逻辑处理。
- 当前实现支持：
  - `None`（`?void`）到任意 `?T`；
  - `T` 到 `?T`；
  - 引用和值在特定上下文下自动借用/解引用；
  - `&mut T` 可用于期望 `&T` 的位置。

#### 17.2.3 函数与返回规则

- 非 `void` 函数必须保证返回值路径完整。
- 当前实现允许“隐式尾返回”：
  - 块尾为表达式语句时可视为返回该表达式；
  - 块尾 `match` 且每个分支为表达式语句时可作为尾返回。

#### 17.2.4 `for` 迭代推导

`for` 的元素类型推导优先顺序：

1. 内建可迭代类型（`range/array/slice/str/tuple/varargs`）
2. 迭代协议（`iter()` + `next()`，并检查 `Iterator` trait 约束）

若无法推导元素类型，语义阶段报错为“不可迭代”。

#### 17.2.5 模式匹配与穷尽性

- `match` 每个 arm 在独立作用域中做模式绑定。
- 穷尽性检查当前覆盖：
  - `bool`（`true/false`）
  - `enum`（全部变体）
  - `optional`（`Some/None`）
- 对无法静态穷尽的类型，要求有通配或可覆盖全部值的兜底分支。

#### 17.2.6 错误处理语义

- `expr!` 只允许用于 `!T`，且当前函数必须是可报错函数（`-> !R`）。
- `expr -> err { ... }` 会引入错误处理作用域，并绑定错误变量后分析 handler。
- 可报错函数可直接 `return` 成功值，也可返回错误值（需满足 `Error` trait 约束）。

#### 17.2.7 Trait/Impl 与泛型约束

- `impl Trait for Type` 会登记“类型实现了哪些 Trait”的映射。
- 泛型约束检查会读取该映射或泛型参数自身约束列表。
- trait 方法实现需与声明签名一致（参数个数、`self` 形式、返回类型、是否可报错）。

### 17.3 代码生成（CodeGen）关键规则

#### 17.3.1 入口与依赖

- CodeGen 依赖 Sema 已写回的语义信息：`SemanticType`、`ResolvedDecl`、表达式类型。
- 每个顶层声明递归生成 IR，结束后做 LLVM IR 校验。

#### 17.3.2 类型降低

- Yuan 语义类型会映射为 LLVM 类型（标量、结构、枚举、函数、引用/指针等）。
- 函数类型在一等值场景会按函数指针处理。
- 错误类型 `!T` 采用结构体表示（标签 + 成功值槽 + 错误指针槽）。

#### 17.3.3 函数生成与 `main` 包装

- 普通函数根据语义签名生成 LLVM 函数。
- 参数先 `alloca` 再 `store`，统一地址化访问。
- 用户 `main` 会被包装为 C ABI `main`，以便可执行程序入口兼容系统加载器。
- `async main` 通过运行时入口调度执行。

#### 17.3.4 `defer` 的后端语义

- `defer` 在 IR 层以栈式记录，按 LIFO 执行。
- 在块退出、`return`、`break`、`continue` 等路径上都会触发展开。
- 展开深度与循环/作用域深度关联，保证只执行应执行的 defer 项。

#### 17.3.5 调用生成与泛型单态化

- 调用点会解析 callee（普通函数、成员方法、模块成员、外部符号）。
- 对泛型函数，调用点实参先推导泛型映射，再按需实例化特化函数。
- 方法调用可注入隐式 `self`，并区分值接收与引用接收语义。

#### 17.3.6 错误表达式降低

- `expr!`：分支化为 `Ok`/`Err` 路径；`Err` 路径在可传播函数中直接返回错误值。
- `expr -> err { ... }`：构建成功/失败控制流并在失败分支执行处理块。
- 非可传播上下文下的错误强制解包失败会触发终止路径（trap）。

### 17.4 当前实现边界（摘要）

- 类型检查器逻辑主要集中在 `Sema.cpp`，独立 `TypeChecker` 文件仍为占位。
- 泛型采用“按需单态化”，并非全程序提前实例化。
- 更高级别的静态分析（如完整借用生命周期检查）尚未纳入当前版本。

---

**文档结束**
