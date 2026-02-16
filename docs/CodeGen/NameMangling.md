# Yuan 命名修饰（Name Mangling）规范 v1

本文定义 Yuan 编译器统一符号修饰规则（ABI v1），用于函数、方法、全局变量、全局常量等可链接符号。

## 设计目标

- 统一：所有可链接符号使用同一套编码框架。
- 可判定：同一声明在同一输入下始终得到同一符号名。
- 可扩展：支持泛型实例化、方法接收者、错误返回、可变参数等语言特性。
- 可兼容：规则借鉴 C++ Itanium ABI（结构化编码）和 Rust（模块/crate 消歧思想）。

## 总体格式

符号以 ABI 前缀开头：

- `_Y1`：Yuan ABI v1

保留入口：

- 源码 `func main()` 的内部函数名固定为 `yuan_main`，并由编译器生成 C ABI `main` 包装。

随后是符号类别：

- `F`：普通函数
- `M`：方法（首参数为 `self`/`&self`/`&mut self`）
- `V`：全局变量
- `C`：全局常量

基础结构（函数/方法）：

`_Y1{F|M}M{Module}N{Name}P{Params}R{Return+Flags}G{GenericParams}_{Discriminator}`

基础结构（全局变量/常量）：

`_Y1{V|C}M{Module}N{Name}T_{Type}_{Discriminator}`

## 关键字段

- `Module`：声明来源模块（优先源码文件路径，降级到 LLVM module name）
- `Name`：源级标识符
- `Discriminator`：声明消歧码（可逆编码）
  - `DL{file_id}_{offset}`：源码位置可用时
  - `DP{hex_ptr}`：源码位置不可用时（如某些测试构造 AST）
  - `Dnone`：空声明占位

### 标识符编码

标识符统一编码为：

`I{byte_len}_{hex(utf8_bytes)}`

示例：

- `add` -> `I3_616464`
- `Point` -> `I5_506f696e74`

这保证符号仅含 ASCII 字符，且支持 Unicode 标识符。

## 类型编码（Type Mangling）

基础类型：

- `Tv`：void
- `Tb`：bool
- `Tc`：char
- `Tstr`：str
- `Tval`：Value
- `Ti{bits}`：有符号整数，如 `Ti32`
- `Tu{bits}`：无符号整数，如 `Tu64`
- `Tf{bits}`：浮点，如 `Tf64`

复合类型：

- `Ta{n}_{Elem}_E`：数组 `[T; n]`
- `Ts{m|i}_{Elem}_E`：切片 `&[T] / &mut [T]`
- `Tt{k}_{E1}_{E2}..._E`：元组
- `Tvargs_{Elem}_E`：可变参数
- `To_{Inner}_E`：Optional
- `Tr{m|i}_{Pointee}_E`：引用
- `Tp{m|i}_{Pointee}_E`：指针
- `Tfn{n}_{P1}_{P2}..._R_{Ret}_Er{0|1}_Vr{0|1}_E`：函数类型
- `Terr_{Success}_E`：错误类型 `!T`
- `Tra{0|1}_{Elem}_E`：范围类型（是否闭区间）

命名类型：

- `Tst_{Ident}`：struct
- `Ten_{Ident}`：enum
- `Ttr_{Ident}`：trait
- `Tg_{Ident}`：泛型参数
- `Tgi_{Base}_N{k}_{A1}_{A2}..._E`：泛型实例
- `Ttv{id}`：类型变量
- `Tal_{Alias}_{Aliased}_E`：类型别名
- `Tmo_{Ident}`：模块类型

## 函数附加信息

函数段包含：

- 参数列表：`P{count}_..._E`
- 返回与行为位：`R_{Ret}_Er{canError}_Vr{isVariadic}_Ar{isAsync}`
- 泛型参数列表：`G{count}_{param...}_E`

## 泛型实例化后缀

泛型函数/方法实例化时，在基础符号后追加：

`_S{k}_{ParamName}_{ConcreteType}..._E`

示例（概念）：

- 基础：`_Y1F...`
- 实例化 `<T=i32, U=str>` 后：
  `_Y1F..._S2_I1_54_Ti32_I1_55_Tstr_E`

## 与 C++/Rust 的对应关系

- 类似 C++：采用结构化、可组合的语法编码（而非简单拼接）。
- 类似 Rust：加入模块来源与消歧码，避免跨文件同名冲突。
- 与二者不同：Yuan v1 以“统一一套规则覆盖函数/方法/全局符号”为优先目标。

## 当前实现状态

- 已接入：函数、方法、全局变量、全局常量、泛型实例化后缀。
- 预留扩展：若后续引入 `extern "C"`/稳定公共 ABI，可增加“禁用 mangling”或“稳定导出名”策略层。

## 反向还原（yuanfilt）

仓库内提供 `yuanfilt`，可将 `_Y1...` 符号反解为可读格式（Yuan 风格 `func module.name(...)`）：

```bash
# 单个符号
yuanfilt _Y1...

# 管道过滤（类似 rustfilt / c++filt）
nm a.out | yuanfilt
```

说明：`yuanfilt` 以当前 v1 可逆格式为目标（`DL/DP/Dnone`）；旧哈希后缀格式不在支持范围内。
