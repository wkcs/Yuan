#!/usr/bin/env python3
"""
Validate spec2026 manifest and case files.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Dict, List, Tuple

LEGACY_REQUIRED_FIELDS = [
    "point_id",
    "spec_ref",
    "title",
    "phase",
    "pass_case",
    "fail_case",
    "expect_pass_command",
    "expect_fail_command",
    "diag_codes",
    "diag_keywords",
]

ALLOWED_BOUNDARIES = {"core", "syntax_edge", "semantic_edge", "runtime_edge"}
ALLOWED_KINDS = {"pass", "fail"}


def load_manifest(path: Path) -> List[Dict[str, object]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, list):
        raise RuntimeError("manifest top-level must be a list")
    return data


def read_meta(path: Path) -> Dict[str, str]:
    meta: Dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines()[:40]:
        if not line.startswith("/// @"):
            continue
        body = line[len("/// @") :]
        if ":" not in body:
            continue
        key, value = body.split(":", 1)
        meta[key.strip()] = value.strip()
    return meta


def normalize_cases(entry: Dict[str, object]) -> List[Dict[str, object]]:
    raw_cases = entry.get("cases")
    if isinstance(raw_cases, list) and raw_cases:
        return [dict(c) for c in raw_cases]

    # Legacy fallback
    missing = [k for k in LEGACY_REQUIRED_FIELDS if k not in entry]
    if missing:
        raise RuntimeError(f"entry {entry.get('point_id','<unknown>')}: missing fields: {missing}")

    pid = str(entry["point_id"])
    phase = str(entry["phase"])
    edge_boundary = "runtime_edge" if phase == "runtime" else "semantic_edge"
    return [
        {
            "case_id": f"{pid}__core_pass",
            "kind": "pass",
            "boundary": "core",
            "path": str(entry["pass_case"]),
            "expect_command": str(entry["expect_pass_command"]),
            "diag_codes": [],
            "diag_keywords": [],
        },
        {
            "case_id": f"{pid}__{edge_boundary}_fail",
            "kind": "fail",
            "boundary": edge_boundary,
            "path": str(entry["fail_case"]),
            "expect_command": str(entry["expect_fail_command"]),
            "diag_codes": [str(x) for x in entry.get("diag_codes", [])],
            "diag_keywords": [str(x) for x in entry.get("diag_keywords", [])],
        },
    ]


def validate_case(entry: Dict[str, object], case: Dict[str, object], root: Path) -> List[str]:
    errors: List[str] = []
    pid = str(entry["point_id"])

    for key in ["case_id", "kind", "boundary", "path", "expect_command", "diag_codes", "diag_keywords"]:
        if key not in case:
            errors.append(f"{pid}: case missing '{key}'")
    if errors:
        return errors

    case_id = str(case["case_id"])
    kind = str(case["kind"])
    boundary = str(case["boundary"])
    path = root / str(case["path"])

    if kind not in ALLOWED_KINDS:
        errors.append(f"{case_id}: invalid kind {kind}")
    if boundary not in ALLOWED_BOUNDARIES:
        errors.append(f"{case_id}: invalid boundary {boundary}")

    if not path.exists():
        errors.append(f"{case_id}: case file not found: {path}")
        return errors

    meta = read_meta(path)
    if meta.get("point_id") != pid:
        errors.append(f"{path}: @point_id mismatch")
    if meta.get("case_id") != case_id:
        errors.append(f"{path}: @case_id mismatch")
    if meta.get("expect") != kind:
        errors.append(f"{path}: @expect mismatch")
    if meta.get("boundary") != boundary:
        errors.append(f"{path}: @boundary mismatch")

    if kind == "fail":
        diag_codes = [str(x) for x in case.get("diag_codes", [])]
        diag_keywords = [str(x) for x in case.get("diag_keywords", [])]
        if not diag_codes:
            errors.append(f"{case_id}: diag_codes must be non-empty for fail case")
        if not diag_keywords:
            errors.append(f"{case_id}: diag_keywords must be non-empty for fail case")

    return errors


def validate_entry(entry: Dict[str, object], root: Path, min_cases_per_point: int) -> Tuple[List[str], List[str]]:
    errors: List[str] = []
    case_ids: List[str] = []

    for key in ["point_id", "spec_ref", "title", "phase"]:
        if key not in entry:
            errors.append(f"entry missing '{key}'")
    if errors:
        return errors, case_ids

    pid = str(entry["point_id"])
    if not re.match(r"^\d+_\d+(_\d+)?_[a-z0-9_]+$", pid):
        errors.append(f"invalid point_id format: {pid}")

    try:
        cases = normalize_cases(entry)
    except RuntimeError as ex:
        return [str(ex)], case_ids

    if len(cases) < min_cases_per_point:
        errors.append(f"{pid}: requires >= {min_cases_per_point} cases, found {len(cases)}")

    boundaries = {str(c.get("boundary", "")) for c in cases}
    if "core" not in boundaries:
        errors.append(f"{pid}: missing core boundary case")
    edge_count = sum(1 for b in boundaries if b in {"syntax_edge", "semantic_edge", "runtime_edge"})
    if edge_count < 2:
        errors.append(f"{pid}: requires >=2 edge boundaries, found {edge_count}")

    pass_count = sum(1 for c in cases if str(c.get("kind", "")) == "pass")
    if pass_count < 1:
        errors.append(f"{pid}: requires >=1 pass case")

    for case in cases:
        case_errors = validate_case(entry, case, root)
        errors.extend(case_errors)
        if "case_id" in case:
            case_ids.append(str(case["case_id"]))

    return errors, case_ids


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate spec2026 manifest")
    parser.add_argument(
        "--manifest",
        type=Path,
        default=Path("tests/spec2026/manifest/spec2026_manifest.yaml"),
        help="manifest path",
    )
    parser.add_argument(
        "--expected-points",
        type=int,
        default=139,
        help="expected number of semantic points",
    )
    parser.add_argument(
        "--min-cases-per-point",
        type=int,
        default=3,
        help="minimum number of cases per point",
    )
    args = parser.parse_args()

    root = Path.cwd()
    manifest = load_manifest(args.manifest)

    errors: List[str] = []
    point_ids = [str(x.get("point_id", "")) for x in manifest]
    if len(point_ids) != len(set(point_ids)):
        errors.append("duplicate point_id found in manifest")

    if len(manifest) != args.expected_points:
        errors.append(f"expected {args.expected_points} points, found {len(manifest)}")

    all_case_ids: List[str] = []
    existing_cases = 0
    for entry in manifest:
        entry_errors, case_ids = validate_entry(entry, root, args.min_cases_per_point)
        errors.extend(entry_errors)
        all_case_ids.extend(case_ids)
        try:
            cases = normalize_cases(entry)
            existing_cases += sum(1 for c in cases if (root / str(c["path"])).exists())
        except Exception:
            pass

    if len(all_case_ids) != len(set(all_case_ids)):
        errors.append("duplicate case_id found in manifest")

    print(f"points: {len(manifest)}")
    print(f"cases found: {existing_cases}")

    if errors:
        print("validation failed:")
        for err in errors[:300]:
            print(f"  - {err}")
        if len(errors) > 300:
            print(f"  ... and {len(errors) - 300} more")
        return 1

    print("validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
