# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概览

Yuan 是基于 LLVM 的静态类型编译型语言实现，主线是：

- 前端：`Lexer -> Parser -> Sema`
- 后端：`CodeGen -> LLVM IR/Object -> Link`
- 运行时与标准库：`runtime/`（C++）+ `stdlib/`（`.yu`）
- 工具：`yuanc`、`yuan-lsp`、`yuan-format`、`yuan-analyze`

核心目录（只列最关键的）：

- `include/yuan/`：对外头文件（AST/Sema/CodeGen/Driver/Frontend/Tooling 等）
- `src/`：编译器核心实现
- `runtime/`：运行时（core/net/gui）
- `tools/`：编译器与配套工具
- `tests/`：单元测试与规范语义测试
- `docs/` 与 `docs/spec/`：实现文档与语言规范

## 常用命令

### 1) 配置与构建

```bash
# Debug 构建
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# 构建所有目标
cmake --build build -j

# 按目标增量构建（日常更常用）
cmake --build build --target yuanc
cmake --build build --target yuan-format
cmake --build build --target yuan-analyze
```

### 2) 编译器（yuanc）

可执行文件：`./build/tools/yuanc/yuanc`

```bash
# 仅语法/语义检查
./build/tools/yuanc/yuanc -fsyntax-only path/to/file.yu

# 输出词法 / AST / Pretty
./build/tools/yuanc/yuanc -dump-tokens path/to/file.yu
./build/tools/yuanc/yuanc -ast-dump path/to/file.yu
./build/tools/yuanc/yuanc -ast-print path/to/file.yu

# 输出 LLVM IR（必须同时指定 -S 与 -emit-llvm）
./build/tools/yuanc/yuanc -S -emit-llvm path/to/file.yu

# 生成目标文件 / 可执行文件
./build/tools/yuanc/yuanc -c path/to/file.yu
./build/tools/yuanc/yuanc path/to/file.yu -o app
```

常用路径与链接参数：

- `--project <path>`：指定项目配置 `project.yaml`
- `--stdlib <path>`：覆盖标准库根目录
- `--module-cache <path>`：模块缓存目录（默认 `.yuan/cache`）
- `--pkg-path <path>`、`-I <path>`：模块/包搜索路径
- `-L <path>`、`-l <name>`：额外链接路径与库
- `-fruntime-net/-fno-runtime-net`、`-fruntime-gui/-fno-runtime-gui`：运行时链接开关

> 兼容性注意：旧参数 `--emit=*` 已移除，使用 `-dump-tokens/-ast-dump/-ast-print/-S -emit-llvm/-c`。

### 3) 格式化与静态检查（lint）

```bash
# 格式检查（有差异时返回非 0）
./build/tools/yuan-format/yuan-format --check path/to/file.yu

# 就地格式化
./build/tools/yuan-format/yuan-format -i path/to/file.yu

# 语义 + 风格检查
./build/tools/yuan-analyze/yuan-analyze path/to/file.yu

# 查看可用检查项
./build/tools/yuan-analyze/yuan-analyze --list-checks
```

### 4) 测试

```bash
# 全量测试
ctest --test-dir build --output-on-failure

# 列出测试
ctest --test-dir build -N

# 按名称过滤（适合快速回归）
ctest --test-dir build -R SemaTests --output-on-failure
ctest --test-dir build -R parser_tests --output-on-failure
ctest --test-dir build -R runtime_tests --output-on-failure
```

运行单个 gtest 用例（直接跑测试二进制）：

```bash
./build/tests/unit/Parser/parser_tests --gtest_filter=ParseExprTest.*
```

### 5) spec2026 规范语义套件

```bash
# CTest 入口
ctest --test-dir build -R spec2026_all --output-on-failure
ctest --test-dir build -R spec2026_validate --output-on-failure
ctest --test-dir build -R spec2026_report --output-on-failure

# 脚本入口（支持 filter/phase）
python3 tests/scripts/test_spec2026.py --profile all
python3 tests/scripts/test_spec2026.py --profile all --filter "ch10_.*"
```

### 6) VSCode 插件

```bash
cd tools/vscode-yuan
npm install
npm run compile
npx vsce package
```

## 架构总览（Big Picture）

### Driver：命令行到编译流水线的编排层

关键文件：

- `tools/yuanc/main.cpp`
- `include/yuan/Driver/Options.h`
- `src/Driver/Options.cpp`
- `src/Driver/Driver.cpp`

要点：

- `Options.cpp` 负责参数解析、动作互斥校验、`project.yaml` 自动发现与合并。
- `Driver.cpp` 用 `Compilation -> Command` 组织流程，核心命令是 `FrontendCommand` 和 `LinkCommand`。
- 链接模式下会先生成主输入 `.o`，再收集/重建模块依赖 `.o`，最后统一链接可执行文件。

### Frontend：可复用的编译动作框架

关键文件：

- `include/yuan/Frontend/CompilerInvocation.h`
- `include/yuan/Frontend/CompilerInstance.h`
- `include/yuan/Frontend/FrontendAction.h`
- `src/Frontend/CompilerInstance.cpp`
- `src/Frontend/FrontendAction.cpp`

要点：

- `CompilerInstance` 统一管理输入、源码映射、诊断、AST/Sema 生命周期。
- `ensureParsed()` 执行 Lexer/Parser；`ensureAnalyzed()` 执行 Sema。
- `FrontendAction` 统一封装 `DumpTokens / ASTDump / ASTPrint / SyntaxOnly / EmitLLVM / EmitObj`。
- `yuanc`、`yuan-format`、`yuan-analyze` 共享该前端框架。

### Sema 与模块系统：语义真相来源

关键文件：

- `src/Sema/`
- `include/yuan/Sema/ModuleManager.h`
- `docs/Sema/README.md`

要点：

- Sema 负责符号、类型、Trait/Impl 约束与错误恢复。
- Sema 会把语义信息写回 AST（如表达式类型、解析到的声明），供 CodeGen 与 LSP 直接消费。
- `ModuleManager` 负责模块路径解析、`.ymi/.o` 缓存、依赖装载与循环导入处理。

### CodeGen 与 Runtime：IR 降低与最终链接

关键文件：

- `src/CodeGen/CodeGen.cpp`
- `src/CodeGen/CGDecl.cpp`
- `src/CodeGen/CGStmt.cpp`
- `src/CodeGen/CGExpr.cpp`
- `runtime/CMakeLists.txt`
- `docs/CodeGen/README.md`

要点：

- CodeGen 将通过 Sema 的 AST 降低为 LLVM IR，并可产出 `.ll` / `.o`。
- Driver 链接阶段注入 `yuan_runtime`，并按开关选择 `yuan_runtime_net` / GUI runtime。

### Tooling：项目配置与编辑器能力

关键文件：

- `include/yuan/Tooling/ProjectConfig.h`
- `src/Tooling/ProjectConfig.cpp`
- `tools/yuan-format/main.cpp`
- `tools/yuan-analyze/main.cpp`
- `tools/yuan-lsp/`

要点：

- `ProjectConfigLoader::discover()` 会向上查找 `project.yaml`。
- 工具侧会合并项目配置与 CLI 选项：显式 CLI 优先，路径项可叠加。
- `yuan-lsp` 复用编译器语义结果提供语言服务。

### 测试分层

- `tests/unit/`：GoogleTest 单元测试（Basic/Lexer/AST/Builtin/Parser/Sema/CodeGen/Runtime）
- `tests/spec2026/`：基于 `docs/spec/Yuan_Language_Spec.md` 的规范语义测试
- `tests/scripts/`：spec2026 与词法测试脚本

## 项目约定

- 默认协作语言：中文。
- C++ 标准：C++17。
- AST 节点判断使用内部 RTTI（`getKind()` + `static_cast` / `Decl::classof()`）；不要对 AST 节点使用 `dynamic_cast` 或 `llvm::dyn_cast`。
- 修改语义规则、类型规则或 IR 降低规则时，必须同步更新 `docs/` 与 `docs/spec/`。
- 行为变更必须补充/更新测试（`tests/yuan/`、`tests/unit/`、`tests/spec2026/`）。

## 规则文件检查结果

- 未发现 `.cursorrules` 或 `.cursor/rules/` 额外规则。
- 未发现 `.github/copilot-instructions.md`。
