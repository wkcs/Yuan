# CodeGen 测试用例

本目录包含 Yuan 编译器代码生成模块的测试用例。

## 目录结构

- **basic/** - 基本代码生成测试
  - `empty_function.yu` - 空函数生成 ✅
  - `main_only.yu` - 只有 main 函数 ✅
  - `simple_return.yu` - 简单返回语句 ⏳
  - `literals.yu` - 各种字面量 ⏳
  - `arithmetic.yu` - 算术运算 ⏳
  - `comparison.yu` - 比较运算 ⏳

- **control_flow/** - 控制流测试
  - `if_statement.yu` - if/elif/else 语句 ⏳
  - `while_loop.yu` - while 循环（含 break/continue）⏳
  - `loop_statement.yu` - loop 无限循环（含 break/continue）⏳

- **functions/** - 函数测试
  - `function_calls.yu` - 函数调用和递归 ⏳
  - `parameters.yu` - 函数参数 ⏳

- **structs/** - 结构体测试（待实现）

- **errors/** - 错误处理测试（待实现）

## 测试方法

### 使用编译器命令

生成 LLVM IR：
```bash
./build/tools/yuanc/yuanc --emit=ir tests/yuan/codegen/basic/empty_function.yu -o output.ll
```

生成目标文件：
```bash
./build/tools/yuanc/yuanc --emit=obj tests/yuan/codegen/basic/main_only.yu -o output.o
```

生成可执行文件：
```bash
./build/tools/yuanc/yuanc tests/yuan/codegen/basic/main_only.yu -o test_exe
```

### 使用测试脚本

运行所有代码生成测试：
```bash
python3 tests/scripts/test_codegen.py
```

## 测试验证

每个测试用例应该：

1. **正确生成 LLVM IR** - IR 应该通过 LLVM 验证器
2. **编译成目标文件** - 生成有效的 .o 文件
3. **可链接成可执行文件** - 对于包含 main 函数的测试
4. **执行结果正确** - 可执行文件应该正确执行

## 当前状态

### 已实现功能 ✅
- 空函数生成
- 简单的 main 函数
- void 返回类型
- 基本控制流结构（IR 级别）

### 待实现功能 ⏳
- 变量声明和使用（VarDecl 语义分析和代码生成）
- 函数返回值（非 void）
- 表达式求值（字面量、运算符）
- 函数参数传递
- 控制流语句内的变量
- 函数调用

## 测试统计

最后运行时间: 2026-01-14

| 类别 | 总数 | 通过 | 失败 | 通过率 |
|------|------|------|------|--------|
| IR生成 | 11 | 2 | 9 | 18% |
| 目标文件生成 | 11 | 2 | 9 | 18% |
| **总计** | **22** | **4** | **18** | **18%** |

注:目前通过的测试都是最简单的函数定义（无参数、无返回值、无变量）。大部分失败是因为变量声明和表达式求值还未实现。
