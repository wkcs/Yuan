#!/usr/bin/env python3
"""
Extract executable semantic points for spec2026 from Yuan language spec.

Output:
  tests/spec2026/generated/spec2026_points.json
"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import List, Optional


H2_RE = re.compile(r"^##\s+(\d+)\.\s+(.+?)\s*$")
H3_RE = re.compile(r"^###\s+(\d+\.\d+)\s+(.+?)\s*$")
H4_RE = re.compile(r"^####\s+(\d+\.\d+\.\d+)\s+(.+?)\s*$")


CHAPTER_DIR = {
    2: "ch02_lexical",
    3: "ch03_types",
    4: "ch04_expr",
    5: "ch05_stmt",
    6: "ch06_func",
    7: "ch07_struct",
    8: "ch08_enum",
    9: "ch09_trait",
    10: "ch10_module",
    11: "ch11_error",
    12: "ch12_concurrency",
    13: "ch13_builtin",
    14: "ch14_stdlib",
    15: "ch15_examples",
    16: "ch16_appendix",
}


def slugify(text: str) -> str:
    text = text.lower()
    text = text.replace("`", "")
    text = text.replace("/", "_")
    text = re.sub(r"[^a-z0-9]+", "_", text)
    text = re.sub(r"_+", "_", text).strip("_")
    return text or "item"


def normalize_spec_ref(spec_ref: str) -> str:
    parts = spec_ref.split(".")
    return "_".join(parts)


def infer_phase(chapter: int) -> str:
    if chapter == 2:
        return "lexer"
    if chapter in {14, 15}:
        return "runtime"
    return "sema"


@dataclass
class Point:
    point_id: str
    spec_ref: str
    title: str
    chapter: int
    level: int
    source: str
    phase: str
    case_dir: str

    def sort_key(self) -> tuple:
        nums = []
        for token in self.spec_ref.split("."):
            try:
                nums.append(int(token))
            except ValueError:
                nums.append(0)
        return (nums, self.point_id)


def make_point(spec_ref: str, title: str, level: int, source: str) -> Point:
    chapter = int(spec_ref.split(".")[0])
    prefix = normalize_spec_ref(spec_ref)
    pid = f"{prefix}_{slugify(title)}"
    return Point(
        point_id=pid,
        spec_ref=spec_ref,
        title=title.strip(),
        chapter=chapter,
        level=level,
        source=source,
        phase=infer_phase(chapter),
        case_dir=CHAPTER_DIR[chapter],
    )


def parse_markdown_table_row(line: str) -> Optional[List[str]]:
    line = line.strip()
    if not line.startswith("|") or line.count("|") < 2:
        return None
    cells = [c.strip() for c in line.strip("|").split("|")]
    if not cells:
        return None
    if all(set(c) <= {"-", ":"} for c in cells):
        return None
    return cells


def extract_points(spec_path: Path) -> List[Point]:
    lines = spec_path.read_text(encoding="utf-8").splitlines()
    points: List[Point] = []

    current_h2: Optional[int] = None
    current_h3: Optional[str] = None

    appendix_163_count = 0
    appendix_164_count = 0
    appendix_165_count = 0

    for line in lines:
        h2 = H2_RE.match(line)
        if h2:
            current_h2 = int(h2.group(1))
            continue

        h3 = H3_RE.match(line)
        if h3:
            current_h3 = h3.group(1)
            if current_h2 is not None and 2 <= current_h2 <= 15:
                points.append(make_point(h3.group(1), h3.group(2), level=3, source="heading"))
            continue

        h4 = H4_RE.match(line)
        if h4:
            if current_h2 is not None and 2 <= current_h2 <= 15:
                points.append(make_point(h4.group(1), h4.group(2), level=4, source="heading"))
            continue

        row = parse_markdown_table_row(line)
        if row is None or current_h3 is None:
            continue

        if current_h3 == "16.3":
            if row[0] == "优先级":
                continue
            appendix_163_count += 1
            priority = row[0]
            operators = row[1] if len(row) > 1 else ""
            desc = row[2] if len(row) > 2 else ""
            title = f"运算符优先级{priority}_{operators}_{desc}"
            spec_ref = f"16.3.{appendix_163_count}"
            points.append(make_point(spec_ref, title, level=4, source="appendix_16_3"))
            continue

        if current_h3 == "16.4":
            if row[0] == "语法":
                continue
            appendix_164_count += 1
            syntax = row[0]
            desc = row[1] if len(row) > 1 else ""
            title = f"错误语法_{syntax}_{desc}"
            spec_ref = f"16.4.{appendix_164_count}"
            points.append(make_point(spec_ref, title, level=4, source="appendix_16_4"))
            continue

        if current_h3 == "16.5":
            if row[0] == "函数":
                continue
            appendix_165_count += 1
            fn_name = row[0]
            desc = row[1] if len(row) > 1 else ""
            title = f"内置函数_{fn_name}_{desc}"
            spec_ref = f"16.5.{appendix_165_count}"
            points.append(make_point(spec_ref, title, level=4, source="appendix_16_5"))

    points.sort(key=lambda p: p.sort_key())

    return points


def ensure_counts(points: List[Point]) -> None:
    heading_points = [p for p in points if p.source == "heading"]
    appendix_points = [p for p in points if p.source.startswith("appendix_")]
    if len(heading_points) != 104:
        raise RuntimeError(f"expected 104 heading points, got {len(heading_points)}")
    if len(appendix_points) != 35:
        raise RuntimeError(f"expected 35 appendix points, got {len(appendix_points)}")
    if len(points) != 139:
        raise RuntimeError(f"expected 139 total points, got {len(points)}")

    ids = [p.point_id for p in points]
    if len(ids) != len(set(ids)):
        raise RuntimeError("duplicate point_id detected")


def main() -> int:
    parser = argparse.ArgumentParser(description="Extract spec2026 semantic points")
    parser.add_argument(
        "--spec",
        type=Path,
        default=Path("docs/spec/Yuan_Language_Spec.md"),
        help="Path to Yuan spec markdown",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("tests/spec2026/generated/spec2026_points.json"),
        help="Output JSON path",
    )
    args = parser.parse_args()

    points = extract_points(args.spec)
    ensure_counts(points)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    payload = [asdict(p) for p in points]
    args.output.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")

    print(f"wrote {len(payload)} points to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
