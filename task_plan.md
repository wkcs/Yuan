# Task Plan: Yuan 语言规范审查与修正

## Goal
修复 Yuan 语言规范 `docs/spec/Yuan_Language_Spec.md` 中的 20 条设计问题，包括规范冲突、类型系统缺陷、内存模型歧义、语法问题和示例错误。

## Current Phase
Phase 1

## Phases

### Phase 1: P0 — 规范与实现的严重冲突（#1, #2, #3）
- [ ] #1: 明确 auto-borrow 规则 — 统一 §3.4.3 的声明与实际行为
- [ ] #2: 补充枚举多字段变体的语义说明，为实现层修复提供规范依据
- [ ] #3: 完善 Drop trait 签名定义，覆盖 `-> !void` 情况
- **Status:** pending

### Phase 2: P0/P1 — 内存模型与安全性设计（#4, #5, #6）
- [ ] #4: 明确"无借用检查"下的引用安全责任模型
- [ ] #5: 消除 `&mut T` 语义歧义，补充示例
- [ ] #6: 说明 Container 元素 Copy 约束的定位（临时限制 vs 永久设计）
- **Status:** pending

### Phase 3: P1 — 类型系统补全（#7, #8, #9, #10）
- [ ] #7: 定义 `str` 的内存模型（DST 还是内建值类型）
- [ ] #8: 补充整数类型提升与混合运算规则
- [ ] #9: 明确 Optional 类型转换的边界
- [ ] #10: 补全 `Ordering` 类型定义，厘清运算符重载与 trait 的关系
- **Status:** pending

### Phase 4: P2 — 语法与表达力（#11, #12, #13, #14）
- [ ] #11: 定义 match 表达式各分支的类型一致性规则
- [ ] #12: 明确 for 循环迭代语义（按值/按引用、non-Copy 元素处理）
- [ ] #13: 消除 `!` 在不同上下文中的二义性，补充解析规则
- [ ] #14: 补充模式匹配中守卫条件绑定变量的作用域说明
- **Status:** pending

### Phase 5: P2 — 语言设计方向性（#15, #16, #17, #18）
- [ ] #15: 将并发实验性 API 标注或分离
- [ ] #16: 讨论 `const` 用于导入的语义过载，考虑 `import` 方案
- [ ] #17: 明确枚举变体在 match 中是否可省略命名空间前缀
- [ ] #18: 统一标准库模块组织层级
- **Status:** pending

### Phase 6: P2 — 示例修正与缺失规范补充（#19, #20）
- [ ] #19: 修正示例代码中与 API 定义不一致的 `io.println` 用法
- [ ] #20: 补充缺失的关键规范内容（整数溢出、浮点 NaN、trait 对象、闭包捕获语义等）
- **Status:** pending

### Phase 7: 验证与交付
- [ ] 通读修改后的规范，检查一致性
- [ ] 确认所有 20 条问题均已处理
- [ ] 输出修改摘要
- **Status:** pending

## Key Questions
1. `str` 应定义为 DST（只能通过 `&str` 使用）还是内建固定类型？
2. Container 元素 Copy 限制是临时约束还是永久设计？—— 需要明确
3. `const` 导入是否引入 `import` 关键字？—— 影响语法变更范围

## Decisions Made
| Decision | Rationale |
|----------|-----------|
| 先修规范文档，不改实现 | 规范是语言设计的契约，应先定清楚再改代码 |
| 按优先级分阶段处理 | P0 问题影响最大，优先修复 |

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
|       |         |            |

## Notes
- 本次只修改规范文档 `docs/spec/Yuan_Language_Spec.md`，不动代码
- 每个 issue 在规范中找到对应位置修改，并记录修改内容
- 修改完成后同步检查 `docs/Sema/TypeSystem.md` 是否需要联动更新
