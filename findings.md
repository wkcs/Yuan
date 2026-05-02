# Findings & Decisions

## Requirements
- 修复 Yuan 语言规范中的 20 条设计问题
- 涵盖：规范冲突、类型系统缺陷、内存模型歧义、语法问题、示例错误
- 本次只修改规范文档，不改实现代码

## Research Findings

### 实现层发现的问题（来自代码审查）
- `TypeChecker::checkTypeCompatible` 静默执行 `T -> &T` auto-borrow，与规范 §3.4.3 矛盾
- `EnumType::get` 只保留变体第一个数据字段，多字段变体信息丢失
- `needsDropImpl` 只识别 `-> void` 签名的 drop，`-> !void` 不被识别
- 整数类型提升只支持同 signedness 窄到宽，规范未定义此规则

### 规范文档自身问题
- §5.6 和 §14.2.6 的示例使用了 `io.println("fmt {}", val)` 双参数形式，但 API 定义只有单参数
- `Ordering` 类型在 §9.4 被引用但从未定义
- `str` 的内存模型完全未说明
- 并发 API 放在正文中但标注为实验层，位置矛盾

## Technical Decisions
| Decision | Rationale |
|----------|-----------|
| 补充 auto-borrow 规则到 §3.4.3 | 方法调用的隐式借用是常见模式，应明确文档化 |
| 在 §8 中补充枚举多字段变体的完整语义 | 现有规范已展示多字段用法，需要补充底层规则 |
| 为 Container Copy 限制添加"临时约束"说明 | 避免用户误解为永久设计决定 |
| 引入 `import` 关键字建议（标记为讨论） | `const` 导入语义不自然，但关键字变更是 breaking change |

## Issues Encountered
| Issue | Resolution |
|-------|------------|
|       |            |

## Resources
- 规范文件：`docs/spec/Yuan_Language_Spec.md`
- 类型系统文档：`docs/Sema/TypeSystem.md`
- 实现参考：`src/Sema/TypeChecker.cpp`, `src/Sema/Type.cpp`
