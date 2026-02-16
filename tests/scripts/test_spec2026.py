#!/usr/bin/env python3
"""
Run spec2026 semantic suite (multi-case manifest + xfail support).
"""

from __future__ import annotations

import argparse
import json
import re
import shlex
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple


@dataclass
class CaseResult:
    name: str
    chapter: str
    point_id: str
    case_id: str
    spec_ref: str
    title: str
    phase: str
    kind: str  # pass|fail
    boundary: str
    ok: bool
    duration: float
    command: str
    returncode: int
    message: str
    stdout: str
    stderr: str
    expected_diag_codes: List[str]
    expected_diag_keywords: List[str]
    expected_xfail: bool
    status: str  # PASS|FAIL|XFAIL|XPASS


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def load_manifest(path: Path) -> List[Dict[str, object]]:
    data = load_json(path)
    if not isinstance(data, list):
        raise RuntimeError("manifest must be a list")
    return data


def load_xfail(path: Optional[Path]) -> Dict[str, Dict[str, object]]:
    if path is None or not path.exists():
        return {}
    data = load_json(path)
    if not isinstance(data, list):
        raise RuntimeError("xfail file must be a list")
    out: Dict[str, Dict[str, object]] = {}
    for row in data:
        if not isinstance(row, dict) or "case_id" not in row:
            continue
        out[str(row["case_id"])] = row
    return out


def find_yuanc(root: Path) -> Path:
    candidates = [
        root / "build/tools/yuanc/yuanc",
        root / "build/Debug/tools/yuanc/yuanc",
        root / "build/Release/tools/yuanc/yuanc",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    raise RuntimeError("yuanc not found; build compiler first")


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


def build_command(template: str, yuanc: Path, root: Path, case_path: Path, point_id: str, temp_dir: Path) -> str:
    values = {
        "yuanc": shlex.quote(str(yuanc)),
        "case": shlex.quote(str(case_path)),
        "pass_case": shlex.quote(str(case_path)),
        "fail_case": shlex.quote(str(case_path)),
        "tmp_bin": shlex.quote(str(temp_dir / f"{point_id}.bin")),
        "tmp_ir": shlex.quote(str(temp_dir / f"{point_id}.ll")),
        "tmp_obj": shlex.quote(str(temp_dir / f"{point_id}.o")),
    }
    return template.format(**values)


def run_command(command: str, timeout: int = 30) -> subprocess.CompletedProcess:
    return subprocess.run(
        command,
        shell=True,
        executable="/bin/bash",
        capture_output=True,
        text=True,
        timeout=timeout,
    )


def run_binary(path: Path, timeout: int = 30) -> subprocess.CompletedProcess:
    return subprocess.run([str(path)], capture_output=True, text=True, timeout=timeout)


def validate_fail_diagnostics(diag_codes: List[str], diag_keywords: List[str], output: str) -> Tuple[bool, str]:
    if not diag_codes:
        return False, "diag_codes is empty"
    if not diag_keywords:
        return False, "diag_keywords is empty"

    matched_codes = [code for code in diag_codes if code in output]
    matched_keywords = [kw for kw in diag_keywords if kw.lower() in output.lower()]

    if not matched_codes:
        return False, f"missing diagnostic code, expected={diag_codes}, matched={matched_codes}"
    if not matched_keywords:
        return False, f"missing diagnostic keyword, expected={diag_keywords}, matched={matched_keywords}"
    return True, f"diag matched codes={matched_codes}, keywords={matched_keywords}"


def run_pass_case(
    case_cmd: str,
    case_path: Path,
    point_id: str,
    yuanc: Path,
    temp_dir: Path,
    strict: bool,
) -> Tuple[str, subprocess.CompletedProcess, str]:
    if not strict:
        cp = run_command(case_cmd)
        return case_cmd, cp, "legacy"

    tmp_bin = temp_dir / f"{point_id}.bin"
    compile_cmd = f"{shlex.quote(str(yuanc))} {shlex.quote(str(case_path))} -o {shlex.quote(str(tmp_bin))}"
    compile_cp = run_command(compile_cmd)
    if compile_cp.returncode != 0:
        merged = subprocess.CompletedProcess(
            args=compile_cmd,
            returncode=compile_cp.returncode,
            stdout=compile_cp.stdout,
            stderr=compile_cp.stderr,
        )
        return compile_cmd, merged, "strict-compile"

    run_cmd = shlex.quote(str(tmp_bin))
    run_cp = run_binary(tmp_bin)
    combined = subprocess.CompletedProcess(
        args=f"{compile_cmd} && {run_cmd}",
        returncode=run_cp.returncode,
        stdout=(compile_cp.stdout or "") + (run_cp.stdout or ""),
        stderr=(compile_cp.stderr or "") + (run_cp.stderr or ""),
    )
    return f"{compile_cmd} && {run_cmd}", combined, "strict-run"


def classify_xfail(ok: bool, expected_xfail: bool) -> Tuple[bool, str]:
    if expected_xfail and ok:
        return False, "XPASS"
    if expected_xfail and not ok:
        return True, "XFAIL"
    return ok, "PASS" if ok else "FAIL"


def infer_modules(result: CaseResult) -> List[str]:
    phase_map = {
        "lexer": ["src/Lexer", "include/yuan/Lexer"],
        "parser": ["src/Parser", "include/yuan/Parser", "include/yuan/AST"],
        "sema": ["src/Sema", "include/yuan/Sema"],
        "codegen": ["src/CodeGen", "src/Builtin"],
        "runtime": ["src/CodeGen", "stdlib", "runtime"],
    }
    return phase_map.get(result.phase, [])


def run_entry(
    entry: Dict[str, object],
    yuanc: Path,
    root: Path,
    verbose: bool,
    strict: bool,
    xfail_map: Dict[str, Dict[str, object]],
) -> List[CaseResult]:
    chapter = str(entry["spec_ref"]).split(".")[0]
    spec_ref = str(entry["spec_ref"])
    point_id = str(entry["point_id"])
    title = str(entry["title"])
    phase = str(entry["phase"])
    results: List[CaseResult] = []

    cases = normalize_cases(entry)

    with tempfile.TemporaryDirectory(prefix="spec2026_") as td:
        temp_dir = Path(td)

        for case in cases:
            case_id = str(case["case_id"])
            kind = str(case["kind"])
            boundary = str(case["boundary"])
            case_path = root / str(case["path"])
            cmd_tpl = str(case["expect_command"])
            diag_codes = [str(x) for x in case.get("diag_codes", [])]
            diag_keywords = [str(x) for x in case.get("diag_keywords", [])]
            expected_xfail = case_id in xfail_map

            case_cmd = build_command(cmd_tpl, yuanc, root, case_path, point_id, temp_dir)
            t0 = time.time()

            if kind == "pass":
                cmd, cp, mode = run_pass_case(
                    case_cmd=case_cmd,
                    case_path=case_path,
                    point_id=point_id,
                    yuanc=yuanc,
                    temp_dir=temp_dir,
                    strict=strict,
                )
                if mode == "strict-compile":
                    ok = False
                    msg = f"strict: compile failed rc={cp.returncode}"
                elif strict:
                    ok = cp.returncode >= 0
                    msg = "ok" if ok else f"strict: runtime crashed by signal (rc={cp.returncode})"
                else:
                    ok = cp.returncode == 0
                    msg = "ok" if ok else f"expected rc=0, got {cp.returncode}"
            else:
                cmd = case_cmd
                cp = run_command(case_cmd)
                merged = (cp.stdout or "") + "\n" + (cp.stderr or "")
                if cp.returncode == 0:
                    ok = False
                    msg = "expected rc!=0, got rc=0"
                else:
                    ok, msg = validate_fail_diagnostics(diag_codes, diag_keywords, merged)

            dt = time.time() - t0
            visible_ok, status = classify_xfail(ok, expected_xfail)
            if status == "XPASS":
                msg = "xfail case unexpectedly passed"

            result = CaseResult(
                name=f"{point_id}::{case_id}",
                chapter=chapter,
                point_id=point_id,
                case_id=case_id,
                spec_ref=spec_ref,
                title=title,
                phase=phase,
                kind=kind,
                boundary=boundary,
                ok=visible_ok,
                duration=dt,
                command=cmd,
                returncode=cp.returncode,
                message=msg,
                stdout=cp.stdout,
                stderr=cp.stderr,
                expected_diag_codes=diag_codes,
                expected_diag_keywords=diag_keywords,
                expected_xfail=expected_xfail,
                status=status,
            )
            results.append(result)

            if verbose:
                print(f"[{status}] {point_id}::{case_id} ({dt:.3f}s)")

    return results


def shorten(text: str, max_chars: int) -> str:
    if len(text) <= max_chars:
        return text
    return text[:max_chars] + f"\n... <truncated {len(text) - max_chars} chars>"


def print_progress(
    idx: int,
    total: int,
    point_id: str,
    phase: str,
    passed: int,
    failed: int,
    xfailed: int,
    start_ts: float,
) -> None:
    percent = (idx / total) * 100 if total else 100.0
    elapsed = time.time() - start_ts
    avg = elapsed / idx if idx else 0.0
    eta = avg * (total - idx)
    print(
        f"[progress] {idx}/{total} ({percent:.1f}%) point={point_id} phase={phase} "
        f"cases_passed={passed} cases_failed={failed} cases_xfailed={xfailed} elapsed={elapsed:.1f}s eta={eta:.1f}s",
        flush=True,
    )


def write_junit(path: Path, results: List[CaseResult]) -> None:
    tests = len(results)
    failures = sum(1 for r in results if r.status in {"FAIL", "XPASS"})
    total_time = sum(r.duration for r in results)

    suite = ET.Element(
        "testsuite",
        name="spec2026",
        tests=str(tests),
        failures=str(failures),
        errors="0",
        time=f"{total_time:.6f}",
    )
    for r in results:
        case = ET.SubElement(
            suite,
            "testcase",
            classname=f"spec2026.chapter{r.chapter}",
            name=r.name,
            time=f"{r.duration:.6f}",
        )
        if r.status in {"FAIL", "XPASS"}:
            fail = ET.SubElement(case, "failure", message=r.message)
            fail.text = (
                f"status: {r.status}\n"
                f"command: {r.command}\n"
                f"returncode: {r.returncode}\n"
                f"stdout:\n{r.stdout}\n"
                f"stderr:\n{r.stderr}\n"
            )

    tree = ET.ElementTree(ET.Element("testsuites"))
    tree.getroot().append(suite)
    path.parent.mkdir(parents=True, exist_ok=True)
    tree.write(path, encoding="utf-8", xml_declaration=True)


def write_failure_report(path: Path, failed: List[CaseResult]) -> None:
    payload = []
    for r in failed:
        payload.append(
            {
                "name": r.name,
                "case_id": r.case_id,
                "point_id": r.point_id,
                "spec_ref": r.spec_ref,
                "title": r.title,
                "phase": r.phase,
                "kind": r.kind,
                "boundary": r.boundary,
                "status": r.status,
                "returncode": r.returncode,
                "message": r.message,
                "command": r.command,
                "expected_diag_codes": r.expected_diag_codes,
                "expected_diag_keywords": r.expected_diag_keywords,
                "stdout": r.stdout,
                "stderr": r.stderr,
                "duration": r.duration,
            }
        )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")


def write_failure_map(path: Path, failed: List[CaseResult]) -> None:
    by_chapter: Dict[str, Dict[str, object]] = {}
    for r in failed:
        chapter = by_chapter.setdefault(r.chapter, {"count": 0, "points": {}})
        chapter["count"] = int(chapter["count"]) + 1
        points = chapter["points"]  # type: ignore[assignment]
        point = points.setdefault(
            r.point_id,
            {
                "spec_ref": r.spec_ref,
                "title": r.title,
                "phase": r.phase,
                "fails": [],
            },
        )
        point["fails"].append(
            {
                "name": r.name,
                "case_id": r.case_id,
                "kind": r.kind,
                "boundary": r.boundary,
                "status": r.status,
                "returncode": r.returncode,
                "message": r.message,
                "expected_diag_codes": r.expected_diag_codes,
                "expected_diag_keywords": r.expected_diag_keywords,
                "suggested_modules": infer_modules(r),
            }
        )

    payload = {"failed_total": len(failed), "by_chapter": by_chapter}
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")


def summarize(results: List[CaseResult], max_fail_details: int, output_snippet_chars: int) -> int:
    total = len(results)
    failed = [r for r in results if r.status in {"FAIL", "XPASS"}]
    xfailed = [r for r in results if r.status == "XFAIL"]
    passed = total - len(failed) - len(xfailed)

    chapter_total: Dict[str, int] = {}
    chapter_failed: Dict[str, int] = {}
    for r in results:
        chapter_total[r.chapter] = chapter_total.get(r.chapter, 0) + 1
        if r.status in {"FAIL", "XPASS"}:
            chapter_failed[r.chapter] = chapter_failed.get(r.chapter, 0) + 1

    print("=" * 72)
    print(
        f"spec2026 results: total={total} passed={passed} xfailed={len(xfailed)} "
        f"failed={len(failed)}"
    )
    print("chapter breakdown:")
    for chapter in sorted(chapter_total, key=lambda x: int(x)):
        t = chapter_total[chapter]
        f = chapter_failed.get(chapter, 0)
        p = t - f
        print(f"  ch{chapter}: total={t} passed_or_xfailed={p} failed={f}")

    if xfailed:
        print("\nxfail cases:")
        for r in xfailed[:20]:
            print(f"  - {r.name} ({r.message})")
        if len(xfailed) > 20:
            print(f"  ... and {len(xfailed) - 20} more")

    if failed:
        print("\ndetailed failing cases:")
        for i, r in enumerate(failed[:max_fail_details], start=1):
            print(f"--- fail #{i}: {r.name}")
            print(f"  status: {r.status}")
            print(f"  spec_ref: {r.spec_ref}")
            print(f"  title: {r.title}")
            print(f"  phase: {r.phase}")
            print(f"  boundary: {r.boundary}")
            print(f"  returncode: {r.returncode}")
            print(f"  reason: {r.message}")
            if r.expected_diag_codes:
                print(f"  expected diag_codes: {r.expected_diag_codes}")
            if r.expected_diag_keywords:
                print(f"  expected diag_keywords: {r.expected_diag_keywords}")
            print(f"  command: {r.command}")
            if r.stdout.strip():
                print("  stdout:")
                print(shorten(r.stdout.rstrip(), output_snippet_chars))
            if r.stderr.strip():
                print("  stderr:")
                print(shorten(r.stderr.rstrip(), output_snippet_chars))
        if len(failed) > max_fail_details:
            print(f"... and {len(failed) - max_fail_details} more failing cases")
        return 1

    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Run spec2026 suite")
    parser.add_argument("--profile", default="all", choices=["all"], help="execution profile")
    parser.add_argument("--filter", default="", help="regex over point_id/case_id")
    parser.add_argument("--phase", default="", help="filter by phase")
    parser.add_argument(
        "--manifest",
        type=Path,
        default=Path("tests/spec2026/manifest/spec2026_manifest.yaml"),
        help="manifest path",
    )
    parser.add_argument(
        "--xfail",
        type=Path,
        default=Path("tests/spec2026/manifest/spec2026_xfail.yaml"),
        help="xfail file path (JSON content)",
    )
    parser.add_argument("--junit", type=Path, default=None, help="junit output path")
    parser.add_argument("--failure-report", type=Path, default=None, help="failed-case details JSON")
    parser.add_argument("--max-fail-details", type=int, default=20)
    parser.add_argument("--output-snippet-chars", type=int, default=1200)
    parser.add_argument("--no-progress", action="store_true")
    parser.add_argument("--stop-on-first-fail", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--strict", action="store_true")
    parser.add_argument("--failure-map", type=Path, default=None)
    args = parser.parse_args()

    root = Path.cwd()
    manifest = load_manifest(args.manifest)
    xfail_map = load_xfail(args.xfail)
    yuanc = find_yuanc(root)

    pattern = re.compile(args.filter) if args.filter else None
    selected: List[Dict[str, object]] = []
    for entry in manifest:
        pid = str(entry["point_id"])
        phase = str(entry["phase"])
        if args.phase and phase != args.phase:
            continue
        if pattern and not pattern.search(pid):
            cases = normalize_cases(entry)
            if not any(pattern.search(str(c.get("case_id", ""))) for c in cases):
                continue
        selected.append(entry)

    print(f"yuanc: {yuanc}")
    print(f"manifest points: {len(manifest)}")
    print(f"selected points: {len(selected)}")
    print(f"xfail entries: {len(xfail_map)}")

    results: List[CaseResult] = []
    start_ts = time.time()
    passed_so_far = 0
    failed_so_far = 0
    xfailed_so_far = 0

    for idx, entry in enumerate(selected, start=1):
        if not args.no_progress:
            print_progress(
                idx=idx,
                total=len(selected),
                point_id=str(entry["point_id"]),
                phase=str(entry["phase"]),
                passed=passed_so_far,
                failed=failed_so_far,
                xfailed=xfailed_so_far,
                start_ts=start_ts,
            )

        entry_results = run_entry(
            entry=entry,
            yuanc=yuanc,
            root=root,
            verbose=args.verbose,
            strict=args.strict,
            xfail_map=xfail_map,
        )
        results.extend(entry_results)

        for r in entry_results:
            if r.status in {"FAIL", "XPASS"}:
                failed_so_far += 1
            elif r.status == "XFAIL":
                xfailed_so_far += 1
            else:
                passed_so_far += 1

        if args.stop_on_first_fail and any(r.status in {"FAIL", "XPASS"} for r in entry_results):
            print("stop-on-first-fail triggered")
            break

    if args.junit:
        write_junit(args.junit, results)
        print(f"wrote junit: {args.junit}")

    failed = [r for r in results if r.status in {"FAIL", "XPASS"}]
    if args.failure_report:
        write_failure_report(args.failure_report, failed)
        print(f"wrote failure report: {args.failure_report}")
    if args.failure_map:
        write_failure_map(args.failure_map, failed)
        print(f"wrote failure map: {args.failure_map}")

    return summarize(results, args.max_fail_details, args.output_snippet_chars)


if __name__ == "__main__":
    raise SystemExit(main())
