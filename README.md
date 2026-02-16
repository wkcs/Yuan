# Yuan 编译器项目文档

Yuan 是一个基于 LLVM 的静态类型编译型语言实现。
仓库包含语言前端（词法、语法、语义）、LLVM IR 代码生成、运行时、标准库、测试与工具链。

## 当前状态

- 前端链路可用：`Lexer -> Parser -> Sema`
- 后端链路可用：`CodeGen -> LLVM IR -> Object -> Link`
- 模块导入可用：`@import` + `ModuleManager`
- 语言持续演进中，规范与实现会持续对齐

## 仓库结构

- `include/yuan/`：公开头文件（AST、Sema、CodeGen、Driver 等）
- `src/`：编译器核心实现
- `runtime/`：运行时（异步、FFI、格式化、OS/GUI 适配、HTTP）
- `stdlib/`：标准库 Yuan 源码
- `tools/yuanc/`：命令行编译器入口
- `tools/yuanfilt/`：符号反修饰工具
- `tests/`：单元测试、语言测试、stdlib 测试
- `docs/`：模块级实现文档
- `docs/spec/`：语言规范与约束

## 构建

### 依赖

- CMake >= 3.16
- C++17 编译器（Clang/GCC/MSVC）
- LLVM 开发库（由 `cmake/FindLLVM.cmake` 发现）
- libcurl（用于运行时 HTTP 能力）

### 编译

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

默认目标通常包括：

- `yuanc`（编译器）
- `yuan_runtime`（运行时库）
- 测试（当 `YUAN_BUILD_TESTS=ON`）

## 使用

`yuanc` 常用示例：

```bash
# 仅语法/语义检查
./build/tools/yuanc/yuanc -fsyntax-only examples/snake_demo.yu

# 输出 LLVM IR
./build/tools/yuanc/yuanc -S examples/snake_demo.yu

# 生成目标文件
./build/tools/yuanc/yuanc -c examples/snake_demo.yu

# 直接编译并链接可执行文件
./build/tools/yuanc/yuanc examples/snake_demo.yu -o snake_demo

# 输出 token / AST / pretty
./build/tools/yuanc/yuanc --emit=tokens examples/snake_demo.yu
./build/tools/yuanc/yuanc --emit=ast examples/snake_demo.yu
./build/tools/yuanc/yuanc --emit=pretty examples/snake_demo.yu
```

常用参数：

- `-O0/-O1/-O2/-O3`：优化等级
- `-I`：附加搜索路径（也用于模块包路径来源）
- `--pkg-path`：预编译包搜索路径
- `--stdlib`：指定标准库根路径
- `--module-cache`：模块缓存目录（默认 `.yuan/cache`）

## HTTP 与 OpenAI 兼容流式能力

运行时在 `runtime/yuan_os.cpp` 提供扩展 HTTP 接口，支持：

- GET/POST 自定义 headers
- timeout（毫秒）
- stream 模式

在 stream 模式下，运行时会解析 SSE `data:` 行并尝试提取：

- `delta.content`
- `message.content`
- `text`

相关示例：`tests/yuan/stdlib/test_std_net_openai_chat.yu`

该示例演示了：

- 构造 OpenAI 兼容 `chat/completions` 请求
- 设置 `Authorization`/`Content-Type`/`Accept` 头
- 使用 `stream=true` 持续打印模型输出

## 测试

```bash
ctest --test-dir build --output-on-failure
```

更多测试脚本见 `tests/scripts/README.md`。

## 编译流程总览

1. `Driver` 读取输入并初始化 `SourceManager/DiagnosticEngine`
2. `Lexer` 产出 Token
3. `Parser` 构建 AST
4. `Sema` 建立符号表、解析类型并执行语义检查
5. `CodeGen` 将 AST 降低到 LLVM IR
6. 生成 `.ll` / `.o` 并链接为可执行文件

## 文档导航

- 总索引：`docs/README.md`
- 语义分析：`docs/Sema/README.md`
- 代码生成：`docs/CodeGen/README.md`
- 语言规范：`docs/spec/Yuan_Language_Spec.md`

## 贡献建议

- 修改语义或后端行为时，同步更新对应文档。
- 行为变更请补充 `tests/yuan/` 或 `tests/unit/` 用例。
- 新增运行时能力（尤其 I/O、网络）时，建议提供最小可运行示例。
