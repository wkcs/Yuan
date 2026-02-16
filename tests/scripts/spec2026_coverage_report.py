#!/usr/bin/env python3
"""Generate coverage report for spec2026 multi-case manifest."""

from __future__ import annotations

import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Dict, List


def load_manifest(path: Path) -> List[Dict[str, object]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, list):
        raise RuntimeError("manifest must be a list")
    return data


def normalize_cases(entry: Dict[str, object]) -> List[Dict[str, object]]:
    raw_cases = entry.get("cases")
    if isinstance(raw_cases, list) and raw_cases:
        return [dict(c) for c in raw_cases]

    pid = str(entry["point_id"])
    phase = str(entry["phase"])
    edge_boundary = "runtime_edge" if phase == "runtime" else "semantic_edge"
    return [
        {
            "case_id": f"{pid}__core_pass",
            "kind": "pass",
            "boundary": "core",
            "path": str(entry["pass_case"]),
        },
        {
            "case_id": f"{pid}__{edge_boundary}_fail",
            "kind": "fail",
            "boundary": edge_boundary,
            "path": str(entry["fail_case"]),
        },
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate spec2026 coverage report")
    parser.add_argument(
        "--manifest",
        type=Path,
        default=Path("tests/spec2026/manifest/spec2026_manifest.yaml"),
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("tests/spec2026/generated/spec2026_coverage_report.json"),
    )
    args = parser.parse_args()

    manifest = load_manifest(args.manifest)

    chapter_heatmap: Dict[str, Counter] = defaultdict(Counter)
    point_summaries = []
    missing = []

    for entry in manifest:
        pid = str(entry["point_id"])
        spec_ref = str(entry["spec_ref"])
        chapter = spec_ref.split(".")[0]
        phase = str(entry["phase"])
        cases = normalize_cases(entry)

        boundaries = Counter(str(c.get("boundary", "")) for c in cases)
        kinds = Counter(str(c.get("kind", "")) for c in cases)

        for c in cases:
            chapter_heatmap[chapter][str(c.get("boundary", ""))] += 1

        has_core = boundaries.get("core", 0) > 0
        edge_total = boundaries.get("syntax_edge", 0) + boundaries.get("semantic_edge", 0) + boundaries.get("runtime_edge", 0)

        if not has_core or edge_total < 2:
            missing.append(
                {
                    "point_id": pid,
                    "spec_ref": spec_ref,
                    "phase": phase,
                    "boundaries": dict(boundaries),
                    "reason": "missing core or <2 edges",
                }
            )

        point_summaries.append(
            {
                "point_id": pid,
                "spec_ref": spec_ref,
                "phase": phase,
                "case_count": len(cases),
                "boundary_counts": dict(boundaries),
                "kind_counts": dict(kinds),
            }
        )

    payload = {
        "points": len(manifest),
        "total_cases": sum(x["case_count"] for x in point_summaries),
        "chapter_heatmap": {k: dict(v) for k, v in sorted(chapter_heatmap.items(), key=lambda x: int(x[0]))},
        "missing_points": missing,
        "point_summaries": point_summaries,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")

    print(f"points: {payload['points']}")
    print(f"total_cases: {payload['total_cases']}")
    print(f"missing_points: {len(payload['missing_points'])}")
    print(f"report: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
