# Yuan 编译器文档索引

本目录记录编译器各子系统的实现细节，面向项目开发者。

## 阅读顺序建议

1. `../README.md`：项目概览、构建与使用
2. `Driver/README.md`：驱动层入口与编译动作
3. `Lexer/README.md` + `Parser/README.md`：前端语法链路
4. `Sema/README.md`：语义分析与类型系统落地
5. `CodeGen/README.md`：LLVM IR 生成、ABI 与运行时桥接
6. `../doc/spec/Yuan_Language_Spec.md`：语言规范（用户视角 + 关键实现约束）

## 子目录说明

- `AST/`：AST 节点结构、打印与调试
- `Basic/`：诊断、源码管理、Token 基础设施
- `Builtin/`：`@` 内置函数实现
- `CodeGen/`：后端 IR 生成与命名修饰
- `Driver/`：命令行编译驱动
- `GUI/`：GUI 运行时桥接说明
- `Lexer/`：词法分析
- `Parser/`：语法分析
- `Sema/`：语义分析、符号与类型检查

## 相关文档

- 语言规范：`../doc/spec/Yuan_Language_Spec.md`
- 标准库总览：`../docs/stdlib.md`
- 标准库实现摘要：`../docs/stdlib-implementation-summary.md`
- 迭代器协议：`../docs/iterator_protocol.md`

## 文档维护要求

- 修改编译行为（语义规则、类型规则、IR 降低规则）时必须同步更新对应模块文档。
- 新增语法特性时，至少同时更新：`Parser`、`Sema`、`CodeGen` 文档和语言规范。
- 文档描述以当前代码实现为准，不确定处应标注“当前实现/计划行为”。
