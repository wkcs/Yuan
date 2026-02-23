# Yuan 编译器测试脚本

本目录包含用于测试 Yuan 编译器各个阶段的脚本。

## 词法分析器测试

### 快速运行

```bash
# 运行所有词法分析器测试
./tests/scripts/run_lexer_tests.sh
```

### 手动测试

```bash
# 测试单个文件
./build/tools/yuanc/yuanc -dump-tokens tests/yuan/lexer/keywords.yu

# 输出到文件
./build/tools/yuanc/yuanc -dump-tokens -o output.tokens tests/yuan/lexer/keywords.yu

# 详细输出
./build/tools/yuanc/yuanc -dump-tokens -v tests/yuan/lexer/keywords.yu
```

### Python 测试脚本

```bash
# 运行完整的测试套件
python3 tests/scripts/test_lexer.py
```

该脚本会：
- 自动查找 yuanc 可执行文件
- 测试所有正常情况的 .yu 文件
- 测试所有错误情况的 .yu 文件（应该失败）
- 生成详细的测试报告
- 将 token 输出保存到 `tests/output/lexer/`

## 测试文件结构

```
tests/yuan/lexer/
├── README.md              # 测试文件说明
├── keywords.yu            # 关键字测试
├── identifiers.yu         # 标识符测试
├── integers.yu            # 整数字面量测试
├── floats.yu              # 浮点数字面量测试
├── strings.yu             # 字符串字面量测试
├── operators.yu           # 运算符测试
├── comments.yu            # 注释测试
└── errors/                # 错误情况测试
    ├── invalid_characters.yu
    ├── invalid_escapes.yu
    ├── invalid_numbers.yu
    ├── unterminated_comments.yu
    └── unterminated_strings.yu
```

## 输出格式

Token 输出格式：
```
Token[序号]: 类型名 "文本内容" @行号:列号
```

示例：
```
Token[0]: var "var" @1:1
Token[1]: Identifier "x" @1:5
Token[2]: = "=" @1:7
Token[3]: IntegerLiteral "10" @1:9
```

## 已知问题

1. **Unicode 支持**: 当前词法分析器不完全支持 Unicode 标识符
2. **嵌套注释**: 可能不支持嵌套块注释

这些问题将在后续的词法分析器改进任务中解决。

## 添加新测试

1. 在 `tests/yuan/lexer/` 目录下创建新的 `.yu` 文件
2. 对于错误情况，放在 `tests/yuan/lexer/errors/` 目录下
3. 运行测试脚本验证新测试用例
4. 更新测试文档