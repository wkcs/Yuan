# spec2026 Manifest Schema

文件路径：`tests/spec2026/manifest/spec2026_manifest.yaml`

格式说明：使用 JSON 文本存储，YAML 兼容。

每个条目字段：
- `point_id` string
- `spec_ref` string
- `title` string
- `phase` enum: `lexer|parser|sema|codegen|runtime`
- `pass_case` string (兼容字段，可选)
- `fail_case` string (兼容字段，可选)
- `expect_pass_command` string (兼容字段，可选)
- `expect_fail_command` string (兼容字段，可选)
- `diag_codes` string[] (兼容字段，可选)
- `diag_keywords` string[] (兼容字段，可选)
- `cases` case[]（推荐字段，至少 3 个）

`case` 字段：
- `case_id` string（全局唯一）
- `kind` enum: `pass|fail`
- `boundary` enum: `core|syntax_edge|semantic_edge|runtime_edge`
- `path` string (repo-relative path)
- `expect_command` string
- `diag_codes` string[]（`kind=fail` 时必须非空）
- `diag_keywords` string[]（`kind=fail` 时必须非空）

约束：
- `point_id` 全局唯一。
- 每个语义点必须存在 `cases`，且用例数 `>= 3`。
- 每个语义点必须至少包含一个 `core` + 两个 edge（`syntax_edge|semantic_edge|runtime_edge`）。
- 每个语义点必须至少存在一个 `pass` 用例。
- 每个 `fail` 用例必须配置 `diag_codes` 和 `diag_keywords`。
- 用例文件头元数据（`@point_id/@case_id/@expect/@boundary`）必须与 manifest 一致。
