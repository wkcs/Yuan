# spec2026

`spec2026` 是基于 `docs/spec/Yuan_Language_Spec.md` 的全量规范语义测试集。

## 范围
- 覆盖第 2-15 章语义小节。
- 覆盖附录 16.3/16.4/16.5 表格语义项。
- 每个语义点至少 3 个用例：`__core_pass.yu`、`__syntax_edge_fail.yu`、`__semantic_edge_fail.yu`（runtime 章节可用 `__runtime_edge_fail.yu`）。

## 结构
- `generated/spec2026_points.json`: 139 个语义点快照。
- `manifest/spec2026_manifest.yaml`: 运行清单（JSON 格式，YAML 兼容）。
- `cases/chXX_*/*.yu`: 每点至少 3 个规范语义与边界用例。
- `helpers/*.yu`: 通用 helper 模块。

## 生成与校验
```bash
python3 tests/scripts/spec2026_extract_points.py
python3 tests/scripts/spec2026_generate_scaffold.py --overwrite
python3 tests/scripts/spec2026_validate_manifest.py --min-cases-per-point 3
python3 tests/scripts/spec2026_coverage_report.py
```

## 运行
```bash
python3 tests/scripts/test_spec2026.py --profile all
```

可选参数：
- `--filter <regex>`
- `--phase <phase>`
- `--junit <path>`
- `--failure-report <path>`（输出完整失败详情 JSON）
- `--max-fail-details <n>`（控制终端打印多少个失败详情）
- `--output-snippet-chars <n>`（控制 stderr/stdout 摘要长度）
- `--no-progress`（关闭进度输出）
- `--stop-on-first-fail`
- `--verbose`

默认会显示进度行（当前点/总点、通过/失败累计、耗时、ETA）。

可选 xfail：
- `tests/spec2026/manifest/spec2026_xfail.yaml`
- 失败且命中 xfail 记为 `XFAIL`
- 通过但命中 xfail 记为 `XPASS`（默认会让任务失败，提示清理 xfail）

执行模型：
- `pass_case` 默认使用“编译+运行”执行，优先通过 `@assert(...)` 验证语义结果。
- 运行返回码 `>=128`（如 assert/panic/crash）会判定为失败。
- 普通业务返回码（`0-127`）不直接判失败，语义正确性主要由断言保障。

## 用例元数据
每个 `.yu` 文件头部包含：
- `@spec_ref`
- `@point_id`
- `@case_id`
- `@expect`
- `@boundary`
- `@phase`
- `@diag_codes`
- `@diag_keywords`
