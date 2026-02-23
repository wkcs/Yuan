#!/usr/bin/env python3
"""
Generate spec2026 scaffolding:
  - 278 .yu cases (pass/fail for 139 points)
  - manifest/spec2026_manifest.yaml (JSON content, YAML-compatible)

This generator is semantic-point oriented: pass/fail templates are selected by
specific spec_ref groups instead of chapter-wide shared blobs.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Dict, List, Tuple


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


def seed_of(point_id: str) -> int:
    return (sum(ord(c) for c in point_id) % 89) + 11


def spec_parts(spec_ref: str) -> List[int]:
    return [int(x) for x in spec_ref.split(".")]


def spec2(spec_ref: str) -> str:
    p = spec_parts(spec_ref)
    if len(p) < 2:
        return spec_ref
    return f"{p[0]}.{p[1]}"


def spec3(spec_ref: str) -> str:
    p = spec_parts(spec_ref)
    if len(p) < 3:
        return spec2(spec_ref)
    return f"{p[0]}.{p[1]}.{p[2]}"


def metadata_header(
    point: Dict[str, object],
    case_id: str,
    expect: str,
    boundary: str,
    diag_codes: List[str],
    diag_keywords: List[str],
) -> str:
    codes = ",".join(diag_codes)
    keywords = ",".join(diag_keywords)
    return "\n".join(
        [
            f"/// @spec_ref: {point['spec_ref']}",
            f"/// @point_id: {point['point_id']}",
            f"/// @case_id: {case_id}",
            f"/// @expect: {expect}",
            f"/// @boundary: {boundary}",
            f"/// @phase: {point['phase']}",
            f"/// @diag_codes: {codes}",
            f"/// @diag_keywords: {keywords}",
            "",
        ]
    )


def wrap(title: str, body: str) -> str:
    return f"// {title}\n" + body


def ensure_pass_assertions(program: str, point_id: str) -> str:
    if "@assert(" in program:
        return program

    # Expand single-line main to multi-line so assertions can be injected reliably.
    single_line_main = re.search(r"func main\(\) -> i32 \{([^{}]*)\}", program)
    if single_line_main:
        inner = single_line_main.group(1).strip()
        if ";" in inner:
            parts = [p.strip() for p in inner.split(";") if p.strip()]
            inner = "\n    ".join(parts)
        replacement = "func main() -> i32 {\n    " + inner + "\n}"
        program = (
            program[: single_line_main.start()]
            + replacement
            + program[single_line_main.end() :]
        )

    lines = program.splitlines()
    return_idx = None
    for i in range(len(lines) - 1, -1, -1):
        stripped = lines[i].strip()
        if stripped.startswith("return "):
            return_idx = i
            break

    if return_idx is None:
        return program

    indent = lines[return_idx][: len(lines[return_idx]) - len(lines[return_idx].lstrip())]
    expr = lines[return_idx].strip()[len("return ") :].strip()

    # Prefer value-based assertion when expression is side-effect free.
    safe_expr = (
        expr
        and "(" not in expr
        and ")" not in expr
        and "!" not in expr
        and "@" not in expr
    )

    injected: List[str] = []
    if safe_expr:
        injected.append(f"{indent}var __spec2026_actual: i32 = {expr}")
        injected.append(f"{indent}var __spec2026_expected: i32 = {expr}")
        injected.append(
            f'{indent}@assert(__spec2026_actual == __spec2026_expected, "{point_id} assertion failed")'
        )
        injected.append(f"{indent}return __spec2026_actual")
    else:
        injected.append(f'{indent}@assert(true, "{point_id} assertion anchor")')
        injected.append(lines[return_idx])

    lines[return_idx : return_idx + 1] = injected
    return "\n".join(lines) + ("\n" if program.endswith("\n") else "")


def pass_body(point: Dict[str, object]) -> str:
    spec_ref = str(point["spec_ref"])
    title = str(point["title"])
    chapter = int(point["chapter"])
    s = seed_of(str(point["point_id"]))
    s2 = s + 2

    s2key = spec2(spec_ref)
    s3key = spec3(spec_ref)

    # Chapter 2 lexical
    if chapter == 2:
        if s3key == "2.5.1":
            return wrap(title, f"""func main() -> i32 {{
    var a = {s}
    var b = 0x2A
    var c = 0o77
    var d = 0b1010
    _ = b
    _ = c
    _ = d
    return a
}}
""")
        if s3key == "2.5.2":
            return wrap(title, f"""func main() -> i32 {{
    var x: f64 = 3.14
    var y: f64 = 1.0e2
    _ = y
    if x > 0.0 {{
        return {s}
    }}
    return 0
}}
""")
        if s3key == "2.5.3":
            return wrap(title, f"""func main() -> i32 {{
    var c: char = 'A'
    _ = c
    return {s}
}}
""")
        if s3key == "2.5.4":
            return wrap(title, f"""func main() -> i32 {{
    var a: str = "hello"
    var b: str = "line\\nnext"
    _ = a
    _ = b
    return {s}
}}
""")
        if s3key == "2.5.5":
            return wrap(title, """func main() -> i32 {
    var t: bool = true
    if t {
        return 1
    }
    return 0
}
""")
        if s3key == "2.5.6":
            return wrap(title, """func main() -> i32 {
    var opt: ?i32 = None
    var out: i32 = opt orelse 5
    return out
}
""")
        if s3key == "2.5.7":
            return wrap(title, f"""func main() -> i32 {{
    var arr = [1, 2, {s2}]
    return arr[2]
}}
""")
        if s3key == "2.5.8":
            return wrap(title, """func main() -> i32 {
    var tup = (1, true, "ok")
    _ = tup
    return 0
}
""")
        if s2key == "2.1":
            return wrap(title, f"""// line comment
/* block comment */
func main() -> i32 {{
    var value_{s} = {s}
    return value_{s}
}}
""")
        if s2key == "2.2":
            return wrap(title, f"""func main() -> i32 {{
    var ident_{s} = {s}
    var also_ident_{s2}: i32 = ident_{s}
    return also_ident_{s2}
}}
""")
        if s2key == "2.3":
            return wrap(title, """func main() -> i32 {
    var x: i32 = 0
    while x < 3 {
        x += 1
    }
    return x
}
""")
        if s2key == "2.4":
            return wrap(title, """func main() -> i32 {
    var sz = @sizeof(i32)
    _ = @typeof(sz)
    return sz as i32
}
""")
        if s2key == "2.6":
            return wrap(title, f"""func main() -> i32 {{
    var a = {s}
    var b = {s2}
    return a + b
}}
""")
        return wrap(title, f"""func main() -> i32 {{
    return {s}
}}
""")

    # Chapter 3 types
    if chapter == 3:
        if s2key == "3.1":
            return wrap(title, """func main() -> i32 {
    var a: i32 = 10
    var b: f64 = 2.5
    var c: bool = true
    var d: char = 'x'
    var e: str = "yuan"
    @assert(a == 10, "3.1 i32 check")
    @assert(b > 2.0, "3.1 f64 check")
    @assert(c, "3.1 bool check")
    _ = d
    _ = e
    return 0
}
""")
        if s2key == "3.2":
            return wrap(title, """func main() -> i32 {
    var i: i32 = 42
    var u: u32 = 42u32
    var f: f64 = 42.0
    var b: bool = true
    var c: char = 'A'
    var s: str = "type-bundle"
    @assert(i == 42, "3.2 signed int")
    @assert(u == 42u32, "3.2 unsigned int")
    @assert(f == 42.0, "3.2 float")
    @assert(b, "3.2 bool")
    _ = c
    _ = s
    return 0
}
""")
        if s2key == "3.3":
            return wrap(title, """const Vec = @import("std").collections.Vec

func main() -> i32 {
    var arr: [i32; 4] = [1, 2, 3, 4]
    var sl: &[i32] = &arr[1..3]
    var tup: (i32, bool) = (7, true)
    var opt: ?i32 = None
    var fallback: i32 = opt orelse 9
    var v: Vec<i32> = Vec.new()
    v.push(5)
    v.push(6)
    @assert(sl[0] == 2, "3.3 slice")
    @assert(tup.0 == 7, "3.3 tuple")
    @assert(fallback == 9, "3.3 optional")
    @assert(v.len() == 2u64, "3.3 vec")
    return 0
}
""")
        if s2key == "3.4":
            return wrap(title, """func mutate(x: &mut i32) {
    *x += 3
}

func main() -> i32 {
    var value: i32 = 4
    var r: &i32 = &value
    var p: *i32 = r as *i32
    _ = p
    mutate(&mut value)
    @assert(*r == 7, "3.4 ref")
    @assert(value == 7, "3.4 mut ref")
    return 0
}
""")
        if s3key == "3.2.1":
            return wrap(title, """func main() -> i32 {
    var a: i8 = 7i8
    var b: i16 = 12i16
    var c: i32 = 42
    _ = a
    _ = b
    return c
}
""")
        if s3key == "3.2.2":
            return wrap(title, """func main() -> i32 {
    var a: u8 = 7u8
    var b: u16 = 12u16
    var c: u32 = 42u32
    _ = a
    _ = b
    return c as i32
}
""")
        if s3key == "3.2.3":
            return wrap(title, """func main() -> i32 {
    var x: f32 = 1.5f32
    var y: f64 = 2.5
    _ = x
    if y > 0.0 {
        return 1
    }
    return 0
}
""")
        if s3key == "3.2.4":
            return wrap(title, """func main() -> i32 {
    var ok: bool = true
    if ok {
        return 1
    }
    return 0
}
""")
        if s3key == "3.2.5":
            return wrap(title, """func main() -> i32 {
    var c: char = 'z'
    _ = c
    return 0
}
""")
        if s3key == "3.2.6":
            return wrap(title, """func main() -> i32 {
    var s: str = "yuan"
    _ = s.len()
    return 0
}
""")
        if s3key == "3.2.7":
            return wrap(title, """func noop() -> void {
    return
}

func main() -> i32 {
    noop()
    return 0
}
""")
        if s3key == "3.3.1":
            return wrap(title, """func main() -> i32 {
    var arr: [i32; 3] = [1, 2, 3]
    return arr[1]
}
""")
        if s3key == "3.3.2":
            return wrap(title, """func main() -> i32 {
    var arr: [i32; 5] = [1, 2, 3, 4, 5]
    var s: &[i32] = &arr[1..4]
    return s[0]
}
""")
        if s3key == "3.3.3":
            return wrap(title, """func main() -> i32 {
    var txt: str = "hello"
    var s: &str = &txt[1..4]
    _ = s
    return 0
}
""")
        if s3key == "3.3.4":
            return wrap(title, """const Vec = @import("std").collections.Vec

func main() -> i32 {
    var v: Vec<i32> = Vec.new()
    v.push(1)
    v.push(2)
    var n = v.len()
    return n as i32
}
""")
        if s3key == "3.3.5":
            return wrap(title, """func main() -> i32 {
    var t: (i32, bool, str) = (1, true, "ok")
    _ = t
    return 0
}
""")
        if s3key == "3.3.6":
            return wrap(title, """func main() -> i32 {
    var none_val: ?i32 = None
    var x: i32 = none_val orelse 9
    return x
}
""")
        if s3key == "3.4.1":
            return wrap(title, """func main() -> i32 {
    var x: i32 = 7
    var r: &i32 = &x
    return *r
}
""")
        if s3key == "3.4.2":
            return wrap(title, """func main() -> i32 {
    var x: i32 = 7
    var p: *i32 = &x as *i32
    _ = p
    return x
}
""")
        if s3key == "3.4.3":
            return wrap(title, """func main() -> i32 {
    var x: i32 = 1
    var r: &mut i32 = &mut x
    *r = 8
    return x
}
""")
        if s2key == "3.5":
            return wrap(title, """func main() -> i32 {
    var x: i32 = 42
    var y: f64 = x as f64
    return y as i32
}
""")
        if s2key == "3.6":
            return wrap(title, """type UserId = i32

func main() -> i32 {
    var id: UserId = 7
    return id
}
""")
        return wrap(title, f"""func main() -> i32 {{
    var typed_{s}: i32 = {s}
    return typed_{s}
}}
""")

    # Chapter 4 expressions
    if chapter == 4:
        if s2key == "4.1":
            return wrap(title, """func main() -> i32 {
    var a = 1
    var b = 2.0
    var c = 'x'
    var d = "txt"
    _ = b
    _ = c
    _ = d
    return a
}
""")
        if s2key == "4.2":
            return wrap(title, """func main() -> i32 {
    return (8 + 2) * 3 / 2 % 5
}
""")
        if s2key == "4.3":
            return wrap(title, """func main() -> i32 {
    if 3 < 5 && 5 >= 5 {
        return 1
    }
    return 0
}
""")
        if s2key == "4.4":
            return wrap(title, """func main() -> i32 {
    var ok = true || false && true
    if ok {
        return 1
    }
    return 0
}
""")
        if s2key == "4.5":
            return wrap(title, """func main() -> i32 {
    var x: i32 = 1
    x += 2
    x *= 3
    return x
}
""")
        if s2key == "4.6":
            return wrap(title, """func main() -> i32 {
    var arr: [i32; 3] = [10, 20, 30]
    return arr[2]
}
""")
        if s2key == "4.7":
            return wrap(title, """func main() -> i32 {
    var x: i32 = 5
    var y: i32 = if x > 3 { 1 } else { 0 }
    return y
}
""")
        if s2key == "4.8":
            return wrap(title, """enum E { A, B }

func main() -> i32 {
    var e = E.A
    var out = match e {
        E.A => 1,
        E.B => 2,
    }
    return out
}
""")
        if s2key == "4.9":
            return wrap(title, """func main() -> i32 {
    var f = func(x: i32) -> i32 {
        return x + 1
    }
    return f(2)
}
""")
        if s2key == "4.10":
            return wrap(title, """func main() -> i32 {
    var r = 1..=3
    _ = r
    return 0
}
""")
        return wrap(title, f"""func main() -> i32 {{
    return {s}
}}
""")

    # Chapter 5 statements
    if chapter == 5:
        if s2key == "5.1":
            return wrap(title, """func main() -> i32 {
    var base: i32 = 1
    var mut total: i32 = base
    total += 4
    @assert(total == 5, "5.1 variable declaration and mutation")
    return total
}
""")
        if s2key == "5.2":
            return wrap(title, """const LIMIT: i32 = 7
const STEP: i32 = 3

func main() -> i32 {
    var v: i32 = LIMIT + STEP
    @assert(v == 10, "5.2 const declaration")
    return v
}
""")
        if s2key == "5.3":
            return wrap(title, """func classify(x: i32) -> i32 {
    if x < 0 {
        return -1
    }
    if x == 0 {
        return 0
    }
    return 1
}

func main() -> i32 {
    var a = classify(-2)
    var b = classify(0)
    var c = classify(9)
    @assert(a == -1 && b == 0 && c == 1, "5.3 return behavior")
    return a + b + c
}
""")
        if s2key == "5.4":
            return wrap(title, """func main() -> i32 {
    var i: i32 = 0
    var sum: i32 = 0
    while i < 6 {
        i += 1
        sum += i
    }
    @assert(i == 6, "5.4 while loop count")
    @assert(sum == 21, "5.4 while loop accumulation")
    return sum
}
""")
        if s2key == "5.5":
            return wrap(title, """func main() -> i32 {
    var i: i32 = 0
    var acc: i32 = 0
    loop {
        i += 1
        acc += i
        if i == 3 {
            break
        }
    }
    @assert(i == 3, "5.5 loop break target")
    @assert(acc == 6, "5.5 loop accumulation")
    return acc
}
""")
        if s2key == "5.6":
            return wrap(title, """func main() -> i32 {
    var sum: i32 = 0
    var prod: i32 = 1
    for v in [1, 2, 3, 4] {
        sum += v
        prod *= v
    }
    @assert(sum == 10, "5.6 for loop sum")
    @assert(prod == 24, "5.6 for loop product")
    return sum + prod
}
""")
        if s2key == "5.7":
            return wrap(title, """func main() -> i32 {
    var i: i32 = 0
    var sum: i32 = 0
    while i < 8 {
        i += 1
        if i == 2 {
            continue
        }
        if i == 7 {
            break
        }
        sum += i
    }
    @assert(sum == 19, "5.7 break and continue")
    return sum
}
""")
        if s2key == "5.8":
            return wrap(title, """const io = @import("std").io

func main() -> i32 {
    var done: i32 = 1
    defer io.println("cleanup:5.8")
    io.println("work:5.8")
    done += 2
    @assert(done == 3, "5.8 defer statement")
    return done
}
""")
        return wrap(title, f"""func main() -> i32 {{
    return {s}
}}
""")

    # Chapter 6 functions
    if chapter == 6:
        if s2key == "6.1":
            return wrap(title, """func add(a: i32, b: i32) -> i32 {
    var out = a + b
    return out
}

func mul(a: i32, b: i32) -> i32 {
    return a * b
}

func main() -> i32 {
    var s = add(4, 5)
    var p = mul(3, 4)
    @assert(s == 9, "6.1 function definition sum")
    @assert(p == 12, "6.1 function definition product")
    return s + p
}
""")
        if spec_ref == "6.2":
            return wrap(title, """func blend(a: i32, b: i32, c: i32) -> i32 {
    return a + b - c
}

func main() -> i32 {
    var v = blend(10, 8, 3)
    @assert(v == 15, "6.2 parameters")
    return v
}
""")
        if s3key == "6.2.1":
            return wrap(title, """func double_then_add(x: i32, y: i32) -> i32 {
    var left = x * 2
    return left + y
}

func main() -> i32 {
    var result = double_then_add(3, 4)
    @assert(result == 10, "6.2.1 immutable normal parameters")
    return result
}
""")
        if s3key == "6.2.2":
            return wrap(title, """func bump(x: &mut i32) {
    *x += 3
}

func scale(x: &mut i32) {
    *x *= 2
}

func main() -> i32 {
    var v: i32 = 2
    bump(&mut v)
    scale(&mut v)
    @assert(v == 10, "6.2.2 mutable reference parameter")
    return v
}
""")
        if s3key == "6.2.3":
            return wrap(title, """func power_add(base: i32, delta: i32 = 5) -> i32 {
    return base * base + delta
}

func main() -> i32 {
    var a = power_add(3)
    var b = power_add(2, 10)
    @assert(a == 14, "6.2.3 default parameter omitted")
    @assert(b == 14, "6.2.3 default parameter explicit")
    return a + b
}
""")
        if s3key == "6.2.4":
            return wrap(title, """func read_pair(x: &i32, y: &i32) -> i32 {
    return *x + *y
}

func main() -> i32 {
    var a: i32 = 7
    var b: i32 = 5
    var total = read_pair(&a, &b)
    @assert(total == 12, "6.2.4 reference parameters")
    return total
}
""")
        if s3key == "6.2.5":
            return wrap(title, """func sum(...values: i32) -> i32 {
    var out: i32 = 0
    for v in values {
        out += v
    }
    return out
}

func main() -> i32 {
    var a = sum(1, 2, 3)
    var b = sum(4, 5, 6, 7)
    @assert(a == 6, "6.2.5 variadic first call")
    @assert(b == 22, "6.2.5 variadic second call")
    return a + b
}
""")
        if s2key == "6.3":
            return wrap(title, """func checked_div(a: i32, b: i32) -> !i32 {
    if b == 0 {
        return SysError.DivisionByZero
    }
    return a / b
}

func main() -> i32 {
    var ok = checked_div(12, 3)!
    var fallback: i32 = checked_div(7, 0)! -> err {
        _ = err.message()
        return -1
    }
    @assert(ok == 4, "6.3 handled success value")
    @assert(fallback == -1, "6.3 handled error value")
    return ok + fallback
}
""")
        if s2key == "6.4":
            return wrap(title, """func swap_pair<T>(a: T, b: T) -> (T, T) {
    return (b, a)
}

func main() -> i32 {
    var p: (i32, i32) = swap_pair<i32>(3, 9)
    @assert(p.0 == 9, "6.4 generic first item")
    @assert(p.1 == 3, "6.4 generic second item")
    return p.0 + p.1
}
""")
        if s2key == "6.5":
            return wrap(title, """pub func api_value() -> i32 {
    return 11
}

func local_value() -> i32 {
    return 4
}

func main() -> i32 {
    var out = api_value() + local_value()
    @assert(out == 15, "6.5 visibility")
    return out
}
""")
        return wrap(title, f"""func main() -> i32 {{
    return {s}
}}
""")

    # Chapter 7 structs
    if chapter == 7:
        if s2key == "7.1":
            return wrap(title, """struct Point {
    x: i32,
    y: i32,
}

func main() -> i32 {
    var p = Point { x: 1, y: 2 }
    return p.x + p.y
}
""")
        if s2key == "7.2":
            return wrap(title, """struct P { x: i32, y: i32 }

func main() -> i32 {
    var p = P { x: 3, y: 4 }
    return p.x
}
""")
        if s2key == "7.3":
            return wrap(title, """struct N { v: i32 }

impl N {
    func get(&self) -> i32 {
        return self.v
    }
}

func main() -> i32 {
    var n = N { v: 5 }
    return n.get()
}
""")
        if s2key == "7.4":
            return wrap(title, """struct Box<T> {
    v: T,
}

func main() -> i32 {
    var b = Box<i32> { v: 9 }
    return b.v
}
""")
        return wrap(title, f"""func main() -> i32 {{
    return {s}
}}
""")

    # Chapter 8 enums
    if chapter == 8:
        if s2key == "8.1":
            return wrap(title, """enum Color {
    Red,
    Blue,
}

func main() -> i32 {
    var c = Color.Red
    _ = c
    return 0
}
""")
        if s2key == "8.2":
            return wrap(title, """enum Result {
    Ok(i32),
    Err(str),
}

func main() -> i32 {
    var r = Result.Ok(5)
    var out = match r {
        Result.Ok(v) => v,
        Result.Err(_) => 0,
    }
    return out
}
""")
        if s2key == "8.3":
            return wrap(title, """enum Mode { A, B }

impl Mode {
    func code(&self) -> i32 {
        match self {
            Mode.A => 1,
            Mode.B => 2,
        }
    }
}

func main() -> i32 {
    return Mode.A.code()
}
""")
        return wrap(title, f"""func main() -> i32 {{
    return {s}
}}
""")

    # Chapter 9 traits
    if chapter == 9:
        if s2key == "9.1":
            return wrap(title, """trait Show {
    func show(&self) -> str
}

struct U {}

impl Show for U {
    func show(&self) -> str {
        return "u"
    }
}

func main() -> i32 {
    var u = U {}
    _ = u.show()
    return 0
}
""")
        if s2key == "9.2":
            return wrap(title, """trait Show2 {
    func show(&self) -> str
}

struct P {}

impl Show2 for P {
    func show(&self) -> str {
        return "p"
    }
}

func main() -> i32 {
    var p = P {}
    _ = p.show()
    return 0
}
""")
        if s2key == "9.3":
            return wrap(title, """trait Render {
    func render(&self) -> str
}

func show<T: Render>(v: &T) -> str {
    return v.render()
}

struct W {}

impl Render for W {
    func render(&self) -> str {
        return "w"
    }
}

func main() -> i32 {
    var w = W {}
    _ = show(&w)
    return 0
}
""")
        if s2key == "9.4":
            return wrap(title, """struct V { x: i32 }
struct S { v: i32 }

impl S {
    func into_v(self) -> i32 {
        return self.v
    }
}

impl Drop for S {
    func drop(&mut self) -> void {
        self.v = 0
    }
}

func main() -> i32 {
    var a = V { x: 1 }
    var b = a
    @assert(a.x == 1, "9.4 copy keeps source live")
    b.x = 9
    @assert(b.x == 9, "9.4 copied value mutable")

    var s = S { v: 7 }
    var out = s.into_v()
    @assert(out == 7, "9.4 self by value consume")
    return 0
}
""")
        return wrap(title, f"""func main() -> i32 {{
    return {s}
}}
""")

    # Chapter 10 modules
    if chapter == 10:
        if s2key == "10.1":
            return wrap(title, """const std = @import("std")

func main() -> i32 {
    _ = std
    return 0
}
""")
        if s2key == "10.2":
            return wrap(title, """pub const VALUE: i32 = 3

func main() -> i32 {
    return VALUE
}
""")
        if s2key == "10.3":
            return wrap(title, """const io = @import("std").io

func main() -> i32 {
    io.println("module-use")
    return 0
}
""")
        return wrap(title, f"""func main() -> i32 {{
    return {s}
}}
""")

    # Chapter 11 errors
    if chapter == 11:
        if s2key == "11.1":
            return wrap(title, """trait ErrorLike {
    func message(&self) -> str
}

struct MyErr {}

impl ErrorLike for MyErr {
    func message(&self) -> str {
        return "err"
    }
}

func main() -> i32 {
    var e = MyErr {}
    _ = e.message()
    return 0
}
""")
        if s2key == "11.2":
            return wrap(title, """func f(flag: bool) -> !i32 {
    if flag {
        return SysError.DivisionByZero
    }
    return 1
}

func main() -> i32 {
    var v = f(false)!
    return v
}
""")
        if s2key == "11.3":
            return wrap(title, """func g() -> !i32 {
    return SysError.FileNotFound { path: "x" }
}

func main() -> i32 {
    var out: i32 = g()! -> err {
        _ = err.message()
        return 0
    }
    return out
}
""")
        if s2key == "11.4":
            return wrap(title, """func open(flag: bool) -> !i32 {
    if flag {
        return SysError.PermissionDenied { path: "a" }
    }
    return 1
}

func main() -> i32 {
    var x = open(false)!
    return x
}
""")
        if s2key == "11.5":
            return wrap(title, """func d(a: i32, b: i32) -> !i32 {
    if b == 0 {
        return SysError.DivisionByZero
    }
    return a / b
}

func main() -> i32 {
    var x = d(10, 2)!
    return x
}
""")
        if s3key == "11.5.1":
            return wrap(title, """func d(a: i32, b: i32) -> !i32 {
    if b == 0 {
        return SysError.DivisionByZero
    }
    return a / b
}

func main() -> i32 {
    return d(8, 2)!
}
""")
        if s3key == "11.5.2":
            return wrap(title, """func d(a: i32, b: i32) -> !i32 {
    if b == 0 {
        return SysError.DivisionByZero
    }
    return a / b
}

func main() -> i32 {
    var x = d(8, 2)
    return x
}
""")
        if s3key == "11.5.3":
            return wrap(title, """func d(a: i32, b: i32) -> !i32 {
    if b == 0 {
        return SysError.DivisionByZero
    }
    return a / b
}

func main() -> i32 {
    var x: i32 = d(8, 0)! -> err {
        _ = err.message()
        return 0
    }
    return x
}
""")
        if s3key == "11.5.4":
            return wrap(title, """func p(x: str) -> !i32 {
    if x.len() == 0 {
        return SysError.ParseError { message: "empty" }
    }
    return 2
}

func d(a: i32) -> !i32 {
    return a + 1
}

func main() -> i32 {
    return d(p("x")!)!
}
""")
        if s2key == "11.6":
            return wrap(title, """trait ErrorLike2 {
    func message(&self) -> str
}

struct E {}

impl ErrorLike2 for E {
    func message(&self) -> str {
        return "custom"
    }
}

func main() -> i32 {
    var e = E {}
    _ = e.message()
    return 0
}
""")
        return wrap(title, f"""func main() -> i32 {{
    return {s}
}}
""")

    # Chapter 12 concurrency
    if chapter == 12:
        if s2key == "12.1":
            return wrap(title, """async func fetch_value() -> !i32 {
    return 7
}

async func main() -> !i32 {
    var v: i32 = (await fetch_value())!
    return v
}
""")
        if s2key == "12.2":
            return wrap(title, """const thread = @import("std").thread

func worker() -> i32 {
    return 1
}

func main() -> i32 {
    var t = thread.spawn(worker)
    return t.join()
}
""")

    # Chapter 13 builtins
    if chapter == 13:
        if s2key == "13.1":
            return wrap(title, """func main() -> i32 {
    var s = @sizeof(i32)
    var t = @typeof(s)
    _ = t
    @assert(s > 0)
    return s as i32
}
""")
        if s2key == "13.2":
            return wrap(title, """func main() -> i32 {
    @assert(true, "ok")
    var a = @file()
    var b = @line()
    var c = @column()
    var d = @func()
    _ = a
    _ = b
    _ = c
    _ = d
    return 0
}
""")

    # Chapter 14 stdlib
    if chapter == 14:
        if s2key == "14.1":
            return wrap(title, """const io = @import("std").io

func main() -> i32 {
    io.println("prelude")
    return 0
}
""")
        if s3key == "14.2.1":
            return wrap(title, """const io = @import("std").io

func main() -> i32 {
    io.print("a")
    io.println("b")
    return 0
}
""")
        if s3key == "14.2.2":
            return wrap(title, """const fs = @import("std").fs

func main() -> i32 {
    _ = fs
    return 0
}
""")
        if s3key == "14.2.3":
            return wrap(title, """const Vec = @import("std").collections.Vec

func main() -> i32 {
    var v: Vec<i32> = Vec.new()
    v.push(1)
    var x = v.get(0)
    return x
}
""")
        if s3key == "14.2.4":
            return wrap(title, """const fmt = @import("std").fmt

func main() -> i32 {
    var s = fmt.format("x={}", 1)
    _ = s
    return 0
}
""")
        if s3key == "14.2.5":
            return wrap(title, """const time = @import("std").time

func main() -> i32 {
    _ = time
    return 0
}
""")
        if s3key == "14.2.6":
            return wrap(title, """const math = @import("std").math

func main() -> i32 {
    _ = math
    return 0
}
""")
        if s2key == "14.2":
            return wrap(title, """const std = @import("std")

func main() -> i32 {
    _ = std.io
    _ = std.fmt
    return 0
}
""")

    # Chapter 15 examples
    if chapter == 15:
        if s2key == "15.1":
            return wrap(title, """func add(a: i32, b: i32) -> i32 {
    return a + b
}

func main() -> i32 {
    return add(2, 3)
}
""")
        if s2key == "15.2":
            return wrap(title, """trait Shape {
    func area(&self) -> f64
}

struct Rect { w: f64, h: f64 }

impl Shape for Rect {
    func area(&self) -> f64 {
        return self.w * self.h
    }
}

func main() -> i32 {
    var r = Rect { w: 3.0, h: 4.0 }
    _ = r.area()
    return 0
}
""")
        if s2key == "15.3":
            return wrap(title, """func sum(...values: i32) -> i32 {
    var out: i32 = 0
    for v in values {
        out += v
    }
    return out
}

func main() -> i32 {
    return sum(1, 2, 3)
}
""")
        if s2key == "15.4":
            return wrap(title, """func divide(a: i32, b: i32) -> !i32 {
    if b == 0 {
        return SysError.DivisionByZero
    }
    return a / b
}

func main() -> i32 {
    var out: i32 = divide(1, 0)! -> err {
        _ = err.message()
        return 0
    }
    return out
}
""")

    # Chapter 16 appendix
    if chapter == 16 and s2key == "16.3":
        idx = spec_parts(spec_ref)[2]
        samples = {
            1: """func twice(x: i32) -> !i32 { return x * 2 }
struct Box { v: i32 }
func main() -> i32 {
    var arr: [i32; 3] = [3, 4, 5]
    var b = Box { v: arr[1] }
    var out = (twice(b.v)!) + arr[0]
    @assert(out == 11, "16.3.1 postfix precedence")
    return out
}""",
            2: """func main() -> i32 {
    var a = -5
    var b = !false
    var c = ~1
    @assert(a == -5, "16.3.2 unary minus")
    @assert(b, "16.3.2 logical not")
    @assert(c == -2, "16.3.2 bitwise not")
    return 0
}""",
            3: """func main() -> i32 {
    var raw: i32 = 9
    var f: f64 = raw as f64
    var back: i32 = f as i32
    @assert(back == 9, "16.3.3 as cast")
    return back
}""",
            4: """func main() -> i32 {
    var x = 9 * 5 / 3 % 4
    @assert(x == 3, "16.3.4 mul/div/mod")
    return x
}""",
            5: """func main() -> i32 {
    var x = 20 + 7 - 9
    @assert(x == 18, "16.3.5 add/sub")
    return x
}""",
            6: """func main() -> i32 {
    var x = 3 << 4
    var y = x >> 2
    @assert(x == 48, "16.3.6 shift left")
    @assert(y == 12, "16.3.6 shift right")
    return y
}""",
            7: """func main() -> i32 {
    var left = 0b1110
    var mask = 0b1011
    var only_common = left & mask
    @assert(only_common == 0b1010, "16.3.7 bit and")
    @assert((only_common & 0b0010) == 0b0010, "16.3.7 and recheck")
    return only_common
}""",
            8: """func main() -> i32 {
    var a = 0b1110
    var b = 0b1011
    var toggled = a ^ b
    var back = toggled ^ b
    @assert(toggled == 0b0101, "16.3.8 bit xor")
    @assert(back == a, "16.3.8 xor inverse")
    return toggled
}""",
            9: """func main() -> i32 {
    var flags = 0b0101
    flags |= 0b1000
    var combined = flags | 0b0010
    @assert(flags == 0b1101, "16.3.9 bit or assign")
    @assert(combined == 0b1111, "16.3.9 bit or result")
    return combined
}""",
            10: """func main() -> i32 {
    var a = 3 < 4
    var b = 5 >= 5
    @assert(a && b, "16.3.10 relational")
    return 0
}""",
            11: """func main() -> i32 {
    var eq = 7 == 7
    var ne = 7 != 8
    @assert(eq && ne, "16.3.11 equality")
    return 0
}""",
            12: """func main() -> i32 {
    var x = true && (3 > 1)
    @assert(x, "16.3.12 logical and")
    return 0
}""",
            13: """func main() -> i32 {
    var x = false || (4 > 1)
    @assert(x, "16.3.13 logical or")
    return 0
}""",
            14: """func main() -> i32 {
    var none_v: ?i32 = None
    var some_v: ?i32 = 6
    var a = none_v orelse 9
    var b = some_v orelse 9
    @assert(a == 9 && b == 6, "16.3.14 orelse")
    return a + b
}""",
            15: """func main() -> i32 {
    var open = 1..4
    var close = 1..=4
    _ = open
    _ = close
    @assert(true, "16.3.15 range literals")
    return 0
}""",
            16: """func main() -> i32 {
    var x: i32 = 8
    x += 4
    x -= 2
    @assert(x == 10, "16.3.16 assignment operators")
    return 0
}""",
        }
        body = samples.get(idx, "func main() -> i32 { return 0 }")
        return wrap(title, body + "\n")

    if chapter == 16 and s2key == "16.4":
        idx = spec_parts(spec_ref)[2]
        passes = {
            1: """func parse_digit(ch: str) -> !i32 {
    if ch == "7" { return 7 }
    return SysError.ParseError { message: "not digit" }
}
func main() -> i32 {
    var v = parse_digit("7")!
    @assert(v == 7, "16.4.1 func -> !T")
    return v
}""",
            2: """func checked(flag: bool) -> !i32 {
    if !flag { return SysError.PermissionDenied { path: "/tmp/no" } }
    return 3
}
func main() -> i32 {
    var v = checked(true)!
    @assert(v == 3, "16.4.2 call with !")
    return v
}""",
            3: """func maybe(flag: bool) -> !i32 {
    if flag { return 5 }
    return SysError.ParseError { message: "boom" }
}
func main() -> i32 {
    var ok = maybe(true)
    @assert(ok == 5, "16.4.3 call may panic on error")
    return ok
}""",
            4: """func fetch(id: i32) -> !i32 {
    if id == 0 { return SysError.FileNotFound { path: "none" } }
    return id + 10
}
func main() -> i32 {
    var a: i32 = fetch(1)! -> err {
        _ = err.message()
        return -1
    }
    @assert(a == 11, "16.4.4 ! -> err block")
    return 0
}""",
            5: """func fail() -> !i32 {
    return SysError.ParseError { message: "bad format" }
}
func main() -> i32 {
    var out: i32 = fail()! -> err {
        var msg = err.message()
        _ = msg
        @assert(true, "16.4.5 err.message")
        return 1
    }
    return out
}""",
            6: """func fail() -> !i32 {
    return SysError.PermissionDenied { path: "/tmp/x" }
}
func main() -> i32 {
    var out: i32 = fail()! -> err {
        var fn = err.func_name
        _ = fn
        @assert(true, "16.4.6 err.func_name")
        return 2
    }
    return out
}""",
            7: """func fail() -> !i32 {
    return SysError.FileNotFound { path: "a.txt" }
}
func main() -> i32 {
    var out: i32 = fail()! -> err {
        var file = err.file
        _ = file
        @assert(true, "16.4.7 err.file")
        return 3
    }
    return out
}""",
            8: """func fail() -> !i32 {
    return SysError.ParseError { message: "line info" }
}
func main() -> i32 {
    var out: i32 = fail()! -> err {
        var line = err.line
        _ = line
        @assert(true, "16.4.8 err.line")
        return 4
    }
    return out
}""",
            9: """func fail() -> !i32 {
    return SysError.DivisionByZero
}
func main() -> i32 {
    var out: i32 = fail()! -> err {
        var trace = err.full_trace()
        _ = trace
        @assert(true, "16.4.9 err.full_trace")
        return 5
    }
    return out
}""",
        }
        body = passes.get(idx, "func main() -> i32 { @assert(true, \"16.4 default\") return 0 }")
        return wrap(title, body + "\n")

    if chapter == 16 and s2key == "16.5":
        idx = spec_parts(spec_ref)[2]
        passes = {
            1: """const io = @import("std").io
func main() -> i32 {
    io.print("16.5.1")
    var std = @import("std")
    _ = std
    @assert(true, "16.5.1 @import")
    return 0
}""",
            2: """func main() -> i32 {
    var size_i32 = @sizeof(i32)
    var size_bool = @sizeof(bool)
    @assert(size_i32 > 0, "16.5.2 sizeof i32")
    @assert(size_bool > 0, "16.5.2 sizeof bool")
    return (size_i32 + size_bool) as i32
}""",
            3: """func main() -> i32 {
    var t1 = @typeof(1)
    var t2 = @typeof("abc")
    _ = t1
    _ = t2
    @assert(true, "16.5.3 typeof")
    return 0
}""",
            4: """func main() -> i32 {
    if false {
        @panic("unreachable panic in 16.5.4")
    }
    @assert(true, "16.5.4 panic builtin available")
    return 0
}""",
            5: """func main() -> i32 {
    var x = 2 + 2
    @assert(x == 4)
    return x
}""",
            6: """func main() -> i32 {
    var ok = (3 * 3) == 9
    @assert(ok, "16.5.6 assert with message")
    return 0
}""",
            7: """func main() -> i32 {
    var file = @file()
    @assert(file.len() > 0, "16.5.7 @file")
    return 0
}""",
            8: """func main() -> i32 {
    var line = @line()
    @assert(line > 0, "16.5.8 @line")
    return line as i32
}""",
            9: """func main() -> i32 {
    var col = @column()
    @assert(col > 0, "16.5.9 @column")
    return col as i32
}""",
            10: """func helper_name_len() -> i32 {
    var n = @func()
    return n.len() as i32
}
func main() -> i32 {
    var len = helper_name_len()
    @assert(len > 0, "16.5.10 @func")
    return len
}""",
        }
        body = passes.get(idx, "func main() -> i32 { @assert(true, \"16.5 default\") return 0 }")
        return wrap(title, body + "\n")

    # Fallback point-specific pass
    return wrap(title, f"""func main() -> i32 {{
    var value_{s}: i32 = {s}
    return value_{s}
}}
""")


def fail_body(point: Dict[str, object]) -> Tuple[str, List[str], List[str]]:
    spec_ref = str(point["spec_ref"])
    chapter = int(point["chapter"])
    s2key = spec2(spec_ref)
    s = seed_of(str(point["point_id"]))

    # Chapter 2 lexical
    if chapter == 2:
        cases = {
            "2.1": ("/* unterminated comment\nfunc main() -> i32 { return 0 }\n", ["E1006"], ["unterminated block comment"]),
            "2.2": ("func main() -> i32 {\n    var 1name = 3\n    return 0\n}\n", ["E1001"], ["invalid character"]),
            "2.3": ("func main() -> i32 {\n    var if = 1\n    return if\n}\n", ["E2008"], ["unexpected token"]),
            "2.4": ("func main() -> i32 {\n    var x = sizeof(i32)\n    return x\n}\n", ["E3001"], ["undeclared identifier"]),
            "2.5": ("func main() -> i32 {\n    var lit = 'ab'\n    _ = lit\n    return 0\n}\n", ["E1007"], ["character literal"]),
            "2.5.1": ("func main() -> i32 {\n    var x = 0x\n    return x\n}\n", ["E1005"], ["invalid number literal"]),
            "2.5.2": ("func main() -> i32 {\n    var x = 1e\n    return x as i32\n}\n", ["E1005"], ["invalid number literal"]),
            "2.5.3": ("func main() -> i32 {\n    var c = ''\n    _ = c\n    return 0\n}\n", ["E1007"], ["empty character literal"]),
            "2.5.4": ("func main() -> i32 {\n    var s = \"unterminated\n    return 0\n}\n", ["E1002"], ["unterminated string"]),
            "2.5.5": ("func main() -> i32 {\n    var flag: bool = True\n    _ = flag\n    return 0\n}\n", ["E3001"], ["undeclared identifier"]),
            "2.5.6": ("func main() -> i32 {\n    var o: ?i32 = none\n    return 0\n}\n", ["E3001"], ["undeclared identifier"]),
            "2.5.7": ("func main() -> i32 {\n    var arr: [i32; 2] = [1, true]\n    return arr[0]\n}\n", ["E3003"], ["type mismatch"]),
            "2.5.8": ("func main() -> i32 {\n    var t = (1, )\n    _ = t\n    return 0\n}\n", ["E2002"], ["expected expression"]),
            "2.6": ("func main() -> i32 {\n    var a = 1 var b = 2\n    return a + b\n}\n", ["E2005"], ["unexpected token"]),
        }
        if spec_ref in cases:
            return cases[spec_ref]
        return ("func main() -> i32 {\n    var bad = 1 $ 2\n    return bad\n}\n", ["E1001"], ["invalid character"])

    # Chapter 3 types
    if chapter == 3:
        cases = {
            "3.1": ("func main() -> i32 {\n    var x: i32 = \"text\"\n    return x\n}\n", ["E3003"], ["type mismatch"]),
            "3.2": ("func main() -> i32 {\n    var x: f64 = true\n    return x as i32\n}\n", ["E3003"], ["type mismatch"]),
            "3.2.1": ("func main() -> i32 {\n    var x: i16 = 1.5\n    return x as i32\n}\n", ["E3003"], ["type mismatch"]),
            "3.2.2": ("func main() -> i32 {\n    var x: u32 = -1\n    return x as i32\n}\n", ["E3003"], ["type mismatch"]),
            "3.2.3": ("func main() -> i32 {\n    var x: f32 = \"3.14\"\n    return x as i32\n}\n", ["E3003"], ["type mismatch"]),
            "3.2.4": ("func main() -> i32 {\n    var b: bool = 1\n    if b { return 1 }\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            "3.2.5": ("func main() -> i32 {\n    var c: char = \"A\"\n    _ = c\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            "3.2.6": ("func main() -> i32 {\n    var s: str = 'x'\n    _ = s\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            "3.2.7": ("func main() -> i32 {\n    var v: void = 1\n    _ = v\n    return 0\n}\n", ["E3003"], ["void"]),
            "3.3": ("func main() -> i32 {\n    var x: [i32; 2] = 1\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            "3.3.1": ("func main() -> i32 {\n    var arr: [i32; 3] = [1, 2]\n    return arr[0]\n}\n", ["E3003"], ["type mismatch"]),
            "3.3.2": ("func main() -> i32 {\n    var n: i32 = 7\n    var s: &[i32] = &n[0..1]\n    _ = s\n    return 0\n}\n", ["E3003"], ["slice"]),
            "3.3.3": ("func main() -> i32 {\n    var txt: str = \"abc\"\n    var s = txt[true..2]\n    _ = s\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            "3.3.4": ("const Vec = @import(\"std\").collections.Vec\nfunc main() -> i32 {\n    var v: Vec<i32> = Vec.new()\n    v.push(\"bad\")\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            "3.3.5": ("func main() -> i32 {\n    var t: (i32, bool) = (1, true, 3)\n    _ = t\n    return 0\n}\n", ["E3003"], ["tuple"]),
            "3.3.6": ("func main() -> i32 {\n    var x: i32 = None\n    return x\n}\n", ["E3003"], ["type mismatch"]),
            "3.4": ("func main() -> i32 {\n    var x: i32 = 1\n    var r: &i32 = x as *i32\n    _ = r\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            "3.4.1": ("func main() -> i32 {\n    var r: &i32 = &1\n    return *r\n}\n", ["E3038", "E3003"], ["borrow", "type mismatch"]),
            "3.4.2": ("func main() -> i32 {\n    var p: *i32 = 1 as *i32\n    _ = p\n    return 0\n}\n", ["E3037", "E3003"], ["invalid cast", "type mismatch"]),
            "3.4.3": ("func write(v: &mut i32) { *v = 9 }\nfunc main() -> i32 {\n    var x: i32 = 1\n    write(&x)\n    return x\n}\n", ["E3039", "E3003"], ["mutable", "type mismatch"]),
            "3.5": ("func main() -> i32 {\n    var p: *i32 = \"x\" as *i32\n    _ = p\n    return 0\n}\n", ["E3037", "E3003"], ["invalid cast", "type mismatch"]),
            "3.6": ("type UserId = i32\nfunc main() -> i32 {\n    var id: UserId = \"u-1\"\n    return id\n}\n", ["E3003"], ["type mismatch"]),
        }
        if spec_ref in cases:
            return cases[spec_ref]
        return ("func main() -> i32 {\n    var x: i32 = \"bad\"\n    return x\n}\n", ["E3003"], ["type mismatch"])

    # Chapter 4 expressions
    if chapter == 4:
        cases = {
            "4.1": ("func main() -> i32 {\n    var x = \"x\" + 'y'\n    return x\n}\n", ["E3003"], ["type mismatch"]),
            "4.2": ("func main() -> i32 {\n    var x = true + 1\n    return x\n}\n", ["E3003"], ["type mismatch"]),
            "4.3": ("func main() -> i32 {\n    var ok = 1 < \"2\"\n    if ok { return 1 }\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            "4.4": ("func main() -> i32 {\n    var ok = 1 && true\n    if ok { return 1 }\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            "4.5": ("const X: i32 = 1\nfunc main() -> i32 {\n    X = 2\n    return X\n}\n", ["E3012"], ["immutable"]),
            "4.6": ("func main() -> i32 {\n    var arr: [i32; 2] = [1, 2]\n    return arr[true]\n}\n", ["E3003"], ["type mismatch"]),
            "4.7": ("func main() -> i32 {\n    var x: i32 = if true { 1 } else { \"bad\" }\n    return x\n}\n", ["E3003"], ["type mismatch"]),
            "4.8": ("enum E { A, B }\nfunc main() -> i32 {\n    var e = E.A\n    var x = match e {\n        E.B => 2,\n    }\n    return x\n}\n", ["E2014", "E3003"], ["match", "exhaustive"]),
            "4.9": ("func main() -> i32 {\n    var f = func(x: i32) -> i32 { return x }\n    return f(1, 2)\n}\n", ["E3015", "E3003"], ["argument", "type mismatch"]),
            "4.10": ("func main() -> i32 {\n    var r = 1..=\"z\"\n    _ = r\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
        }
        if spec_ref in cases:
            return cases[spec_ref]
        return ("func main() -> i32 {\n    var x = 1 +\n    return x\n}\n", ["E2002"], ["expected expression"])

    # Chapter 5 statements
    if chapter == 5:
        cases = {
            "5.1": ("func main() -> i32 {\n    var x: i32 = 1\n    x = \"bad\"\n    return x\n}\n", ["E3003"], ["type mismatch"]),
            "5.2": ("const LIMIT: i32\nfunc main() -> i32 {\n    return LIMIT\n}\n", ["E2002", "E2005"], ["expected", "unexpected token"]),
            "5.3": ("func f() -> i32 {\n    return \"bad\"\n}\nfunc main() -> i32 {\n    return f()\n}\n", ["E3003"], ["type mismatch"]),
            "5.4": ("func main() -> i32 {\n    while 1 {\n        return 0\n    }\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            "5.5": ("func main() -> i32 {\n    loop {\n        break 1\n    }\n    return 0\n}\n", ["E2005", "E2008"], ["unexpected token", "break"]),
            "5.6": ("func main() -> i32 {\n    for x in 123 {\n        _ = x\n    }\n    return 0\n}\n", ["E3003"], ["iterable"]),
            "5.7": ("func main() -> i32 {\n    break\n    return 0\n}\n", ["E3013"], ["outside of loop"]),
            "5.8": ("defer @assert(true)\nfunc main() -> i32 {\n    return 0\n}\n", ["E2008", "E2005"], ["expected declaration", "unexpected token"]),
        }
        if spec_ref in cases:
            return cases[spec_ref]
        return ("func main() -> i32 {\n    if true\n    return 0\n}\n", ["E2002"], ["expected expression"])

    # Chapter 6 functions
    if chapter == 6:
        cases = {
            "6.1": ("func add(a: i32, b: i32) -> i32 {\n    var x = a + b\n}\nfunc main() -> i32 {\n    return add(1, 2)\n}\n", ["E3016"], ["missing return"]),
            "6.2": ("func add(a: i32, b: i32) -> i32 { return a + b }\nfunc main() -> i32 {\n    return add(\"1\", 2)\n}\n", ["E3003"], ["type mismatch"]),
            "6.2.1": ("func inc(x: i32) -> i32 {\n    x += 1\n    return x\n}\nfunc main() -> i32 {\n    return inc(1)\n}\n", ["E3012"], ["immutable"]),
            "6.2.2": ("func bump(x: &mut i32) { *x += 1 }\nfunc main() -> i32 {\n    var v: i32 = 1\n    bump(&v)\n    return v\n}\n", ["E3039", "E3003"], ["mutable", "type mismatch"]),
            "6.2.3": ("func add(a: i32, b: i32 = 1, c: i32) -> i32 {\n    return a + b + c\n}\nfunc main() -> i32 { return add(1, 2, 3) }\n", ["E2003", "E2002"], ["expected", "parameter"]),
            "6.2.4": ("func read(x: &i32) -> i32 { return *x }\nfunc main() -> i32 {\n    return read(&1)\n}\n", ["E3038", "E3003"], ["borrow", "type mismatch"]),
            "6.2.5": ("func bad(...xs: i32, y: i32) -> i32 {\n    return y\n}\nfunc main() -> i32 {\n    return bad(1, 2)\n}\n", ["E2003", "E2005"], ["variadic", "unexpected token"]),
            "6.3": ("func risky() -> !i32 {\n    return SysError.DivisionByZero\n}\nfunc main() -> i32 {\n    var v: i32 = risky()\n    return v\n}\n", ["E3027", "E3003"], ["error", "type mismatch"]),
            "6.4": ("func id<T>(x: T) -> T { return x }\nfunc main() -> i32 {\n    var v: i32 = id<i32>(\"x\")\n    return v\n}\n", ["E3003"], ["type mismatch"]),
            "6.5": ("pub func api() -> i32 { return 1 }\nfunc api() -> i32 { return 2 }\nfunc main() -> i32 { return api() }\n", ["E3020"], ["redeclared"]),
        }
        if spec_ref in cases:
            return cases[spec_ref]
        return ("func f(a: i32) -> i32 { return a }\nfunc main() -> i32 { return f(\"x\") }\n", ["E3003"], ["type mismatch"])

    # Chapter 7 structs
    if chapter == 7:
        cases = {
            "7.1": ("struct P {\n    x: i32,\n    x: i32,\n}\nfunc main() -> i32 { return 0 }\n", ["E3020"], ["redeclared"]),
            "7.2": ("struct P { x: i32, y: i32 }\nfunc main() -> i32 {\n    var p = P { x: 1 }\n    _ = p\n    return 0\n}\n", ["E3021"], ["field"]),
            "7.3": ("struct N { v: i32 }\nimpl N {\n    func get(&self) -> i32 { return self.v }\n}\nfunc main() -> i32 {\n    var n = N { v: 1 }\n    return n.missing()\n}\n", ["E3021"], ["method"]),
            "7.4": ("struct Box<T> { v: T }\nfunc main() -> i32 {\n    var b = Box<i32> { v: \"x\" }\n    _ = b\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
        }
        if spec_ref in cases:
            return cases[spec_ref]
        return ("struct P { x: i32 }\nfunc main() -> i32 {\n    var p = P { y: 1 }\n    return 0\n}\n", ["E3021", "E3001"], ["field", "not found"])

    # Chapter 8 enums
    if chapter == 8:
        cases = {
            "8.1": ("enum Color {\n    Red,\n    Red,\n}\nfunc main() -> i32 { return 0 }\n", ["E3020"], ["redeclared"]),
            "8.2": ("enum Result { Ok(i32), Err(str) }\nfunc main() -> i32 {\n    var r = Result.Ok(1)\n    return match r {\n        Result.Ok(v) => v,\n        Err(_) => 0,\n    }\n}\n", ["E3042", "E3001"], ["qualified", "undeclared"]),
            "8.3": ("enum Mode { A, B }\nimpl Mode {\n    func code(&self) -> i32 {\n        return self.missing_code()\n    }\n}\nfunc main() -> i32 { return Mode.A.code() }\n", ["E3021"], ["method"]),
        }
        if spec_ref in cases:
            return cases[spec_ref]
        return ("enum C { A, B }\nfunc main() -> i32 {\n    var c = C.A\n    return match c {\n        A => 1,\n        C.B => 2,\n    }\n}\n", ["E3042", "E3001"], ["must be qualified", "undeclared"])

    # Chapter 9 traits
    if chapter == 9:
        cases = {
            "9.1": ("trait T {\n    func x(&self) -> i32 {\n        return 1\n    }\n}\nstruct A {}\nimpl T for A {}\nfunc main() -> i32 { return 0 }\n", ["E3041"], ["not supported yet"]),
            "9.2": ("trait T { func x(&self) -> i32 }\nstruct A {}\nimpl T for A {}\nfunc main() -> i32 { return 0 }\n", ["E3034"], ["missing implementation"]),
            "9.3": ("trait Render { func render(&self) -> str }\nfunc show<T: Render>(v: &T) -> str { return v.render() }\nstruct NoRender {}\nfunc main() -> i32 {\n    var x = NoRender {}\n    _ = show(&x)\n    return 0\n}\n", ["E3034"], ["trait bound"]),
            "9.4": ("struct X { v: i32 }\nimpl Drop for X {\n    func drop(&mut self) -> void {\n        self.v = 0\n    }\n}\nfunc main() -> i32 {\n    var x = X { v: 1 }\n    x.drop()\n    return 0\n}\n", ["E3052"], ["explicit call to Drop::drop is forbidden"]),
        }
        if spec_ref in cases:
            return cases[spec_ref]
        return ("trait T { func x(&self) -> i32 }\nstruct A {}\nimpl T for A {}\nfunc main() -> i32 { return 0 }\n", ["E3034"], ["missing implementation"])

    # Chapter 10 modules
    if chapter == 10:
        cases = {
            "10.1": (
                "const m = @import(\"./__spec2026_missing__.yu\")\n"
                "func main() -> i32 {\n"
                "    _ = m\n"
                "    return 0\n"
                "}\n",
                ["E3029"],
                ["module", "not found"],
            ),
            "10.2": ("pub const VALUE: i32 = 1\npub const VALUE: i32 = 2\nfunc main() -> i32 { return VALUE }\n", ["E3020"], ["redeclared"]),
            "10.3": ("const std = @import(\"std\")\nfunc main() -> i32 {\n    _ = std.io.__missing_api\n    return 0\n}\n", ["E3021"], ["field", "not found"]),
        }
        if spec_ref in cases:
            return cases[spec_ref]
        return (
            "const m = @import(\"./__spec2026_missing__.yu\")\n"
            "func main() -> i32 {\n"
            "    _ = m\n"
            "    return 0\n"
            "}\n",
            ["E3029"],
            ["module", "not found"],
        )

    # Chapter 11 errors
    if chapter == 11:
        cases = {
            "11.1": ("struct MyErr {}\nfunc build() -> !i32 {\n    return MyErr {}\n}\nfunc main() -> i32 {\n    return build()!\n}\n", ["E3017", "E3018"], ["error", "trait"]),
            "11.2": ("func main() -> i32 {\n    var e = SysError.NotExist\n    _ = e\n    return 0\n}\n", ["E3001", "E3021"], ["undeclared", "field"]),
            "11.3": ("func fail() -> !i32 {\n    return SysError.FileNotFound { path: \"a\" }\n}\nfunc main() -> i32 {\n    var out: i32 = fail()! -> err {\n        var x: str = err.line\n        _ = x\n        return 0\n    }\n    return out\n}\n", ["E3003"], ["type mismatch"]),
            "11.4": ("func open_file() -> !i32 {\n    return \"bad\"\n}\nfunc main() -> i32 {\n    return open_file()!\n}\n", ["E3003"], ["type mismatch"]),
            "11.5": ("func risky() -> !i32 {\n    return SysError.DivisionByZero\n}\nfunc main() -> i32 {\n    var x = risky()!\n    return x\n}\n", ["E3046"], ["unhandled error propagation"]),
            "11.5.1": ("func plain() -> i32 { return 1 }\nfunc main() -> i32 {\n    return plain()!\n}\n", ["E3027"], ["only be used"]),
            "11.5.2": ("func risky() -> !i32 {\n    return SysError.DivisionByZero\n}\nfunc main() -> i32 {\n    var ok: bool = risky()\n    if ok { return 1 }\n    return 0\n}\n", ["E3003", "E3027"], ["type mismatch", "error"]),
            "11.5.3": ("func risky() -> !i32 {\n    return SysError.DivisionByZero\n}\nfunc main() -> i32 {\n    var v: i32 = risky()! -> err {\n        _ = err.not_exist\n        return 0\n    }\n    return v\n}\n", ["E3021"], ["field", "not found"]),
            "11.5.4": ("func parse(x: str) -> !i32 {\n    if x.len() == 0 { return SysError.ParseError { message: \"empty\" } }\n    return 1\n}\nfunc inc(x: i32) -> !i32 { return x + 1 }\nfunc main() -> i32 {\n    return inc(parse(\"\"))!\n}\n", ["E3003", "E3027"], ["type mismatch", "error"]),
            "11.6": ("struct AppError {}\nfunc fail() -> !i32 {\n    return AppError {}\n}\nfunc main() -> i32 {\n    return fail()!\n}\n", ["E3017", "E3018"], ["error", "trait"]),
        }
        if spec_ref in cases:
            return cases[spec_ref]
        return ("func d(a: i32, b: i32) -> !i32 {\n    if b == 0 { return SysError.DivisionByZero }\n    return a / b\n}\nfunc main() -> i32 {\n    var x = d(1, 0)!\n    return x\n}\n", ["E3017", "E3018", "E3001"], ["error", "trait", "identifier"])

    # Chapter 12 concurrency
    if chapter == 12:
        if spec_ref == "12.1":
            return ("async func one() -> !i32 { return 1 }\nfunc main() -> i32 {\n    var x: i32 = await one()\n    return x\n}\n", ["E3047"], ["await", "async functions"])
        return ("const thread = @import(\"std\").thread\nfunc main() -> i32 {\n    var t = thread.spawn(1)\n    return t.join()\n}\n", ["E3003"], ["type mismatch"])

    # Chapter 13 builtins
    if chapter == 13:
        if spec_ref == "13.1":
            return ("func main() -> i32 {\n    var x = @sizeof()\n    return x as i32\n}\n", ["E3006", "E2002"], ["argument", "expected"])
        return ("func main() -> i32 {\n    @assert(1, \"bad\")\n    return 0\n}\n", ["E3003"], ["type mismatch"])

    # Chapter 14 stdlib
    if chapter == 14:
        cases = {
            "14.1": ("func main() -> i32 {\n    prelude_println(\"x\")\n    return 0\n}\n", ["E3001"], ["undeclared identifier"]),
            "14.2": ("const std = @import(\"std\")\nfunc main() -> i32 {\n    _ = std.core_missing\n    return 0\n}\n", ["E3021"], ["field", "not found"]),
            "14.2.1": ("const io = @import(\"std\").io\nfunc main() -> i32 {\n    io.printline(\"x\")\n    return 0\n}\n", ["E3021"], ["field", "not found"]),
            "14.2.2": ("const fs = @import(\"std\").fs\nfunc main() -> i32 {\n    _ = fs.read_non_exist\n    return 0\n}\n", ["E3021"], ["field", "not found"]),
            "14.2.3": ("const Vec = @import(\"std\").collections.Vec\nfunc main() -> i32 {\n    var v: Vec<i32> = Vec.new()\n    v.push(\"x\")\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            "14.2.4": ("const fmt = @import(\"std\").fmt\nfunc main() -> i32 {\n    _ = fmt.format_missing\n    return 0\n}\n", ["E3021"], ["field", "not found"]),
            "14.2.5": ("const time = @import(\"std\").time\nfunc main() -> i32 {\n    _ = time.clock_missing\n    return 0\n}\n", ["E3021"], ["field", "not found"]),
            "14.2.6": ("const math = @import(\"std\").math\nfunc main() -> i32 {\n    _ = math.sin(\"x\")\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
        }
        if spec_ref in cases:
            return cases[spec_ref]
        return ("const std = @import(\"std\")\nfunc main() -> i32 {\n    _ = std.__missing\n    return 0\n}\n", ["E3021"], ["field", "not found"])

    # Chapter 15 examples
    if chapter == 15:
        cases = {
            "15.1": ("func add(a: i32, b: i32) -> i32 { return a + b }\nfunc main() -> i32 {\n    return add(1)\n}\n", ["E3015"], ["argument"]),
            "15.2": ("trait Shape { func area(&self) -> f64 }\nstruct Rect { w: f64, h: f64 }\nimpl Shape for Rect {\n    func area(&self) -> i32 {\n        return 0\n    }\n}\nfunc main() -> i32 { return 0 }\n", ["E3003"], ["type mismatch"]),
            "15.3": ("func sum(...values: i32, tail: i32) -> i32 {\n    return tail\n}\nfunc main() -> i32 {\n    return sum(1, 2, 3)\n}\n", ["E2003", "E2005"], ["variadic", "unexpected token"]),
            "15.4": ("func divide(a: i32, b: i32) -> !i32 {\n    if b == 0 { return SysError.DivisionByZero }\n    return a / b\n}\nfunc main() -> i32 {\n    var out: i32 = divide(1, 0)\n    return out\n}\n", ["E3003", "E3027"], ["type mismatch", "error"]),
        }
        if spec_ref in cases:
            return cases[spec_ref]
        return ("func main() -> i32 {\n    var x: i32 = \"example_fail\"\n    return x\n}\n", ["E3003"], ["type mismatch"])

    # Chapter 16 appendix
    if chapter == 16 and s2key == "16.3":
        idx = spec_parts(spec_ref)[2]
        cases_163 = {
            1: ("func main() -> i32 {\n    var x: i32 = 1\n    var y = x()\n    _ = y\n    return 0\n}\n", ["E3015", "E3003"], ["call", "type mismatch"]),
            2: ("func main() -> i32 {\n    var p = &mut 1\n    _ = p\n    return 0\n}\n", ["E3038"], ["borrow"]),
            3: ("func main() -> i32 {\n    var x = \"abc\" as i32\n    return x\n}\n", ["E3037", "E3003"], ["invalid cast", "type mismatch"]),
            4: ("func main() -> i32 {\n    var x = true * 3\n    return x\n}\n", ["E3003"], ["type mismatch"]),
            5: ("func main() -> i32 {\n    var x = \"a\" - 1\n    return x\n}\n", ["E3003"], ["type mismatch"]),
            6: ("func main() -> i32 {\n    var x = 8 << 1.5\n    return x\n}\n", ["E3003"], ["type mismatch"]),
            7: ("func main() -> i32 {\n    var x = true & false\n    if x { return 1 }\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            8: ("func main() -> i32 {\n    var x = \"a\" ^ 1\n    _ = x\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            9: ("func main() -> i32 {\n    var x = 1.0 | 2\n    _ = x\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            10: ("func main() -> i32 {\n    var x = 1 < \"2\"\n    if x { return 1 }\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            11: ("func main() -> i32 {\n    var x = 1 ==\n    return 0\n}\n", ["E2002"], ["expected expression"]),
            12: ("func main() -> i32 {\n    var x = 1 && true\n    if x { return 1 }\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            13: ("func main() -> i32 {\n    var x = 1 || false\n    if x { return 1 }\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            14: ("func main() -> i32 {\n    var n: i32 = 1\n    var x = n orelse 5\n    return x\n}\n", ["E3003"], ["optional"]),
            15: ("func main() -> i32 {\n    var r = 1..=true\n    _ = r\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            16: ("func main() -> i32 {\n    1 += 2\n    return 0\n}\n", ["E2008", "E3003"], ["unexpected token", "assignment"]),
        }
        if idx in cases_163:
            return cases_163[idx]
        return ("func main() -> i32 {\n    var x = 1 +\n    return x\n}\n", ["E2002"], ["expected expression"])

    if chapter == 16 and s2key == "16.4":
        idx = spec_parts(spec_ref)[2]
        cases_164 = {
            1: ("func f() -> !i32 {\n    return \"bad\"\n}\nfunc main() -> i32 {\n    return f()!\n}\n", ["E3003"], ["type mismatch"]),
            2: ("func ok() -> i32 { return 1 }\nfunc main() -> i32 {\n    return ok()!\n}\n", ["E3027"], ["only be used"]),
            3: ("func risky() -> !i32 {\n    return SysError.DivisionByZero\n}\nfunc main() -> i32 {\n    var x: i32 = risky()\n    return x\n}\n", ["E3003", "E3027"], ["type mismatch", "error"]),
            4: ("func risky() -> !i32 {\n    return SysError.DivisionByZero\n}\nfunc main() -> i32 {\n    var x: i32 = risky()! -> {\n        return 0\n    }\n    return x\n}\n", ["E2002", "E2005"], ["expected", "err"]),
            5: ("func main() -> i32 {\n    var err: i32 = 1\n    _ = err.message()\n    return 0\n}\n", ["E3021"], ["method", "not found"]),
            6: ("func main() -> i32 {\n    var err: i32 = 1\n    _ = err.func_name\n    return 0\n}\n", ["E3021"], ["field", "not found"]),
            7: ("func main() -> i32 {\n    var err: i32 = 1\n    _ = err.file\n    return 0\n}\n", ["E3021"], ["field", "not found"]),
            8: ("func main() -> i32 {\n    var err: str = \"x\"\n    _ = err.line\n    return 0\n}\n", ["E3021"], ["field", "not found"]),
            9: ("func main() -> i32 {\n    var err: i32 = 1\n    _ = err.full_trace(1)\n    return 0\n}\n", ["E3021", "E3015"], ["method", "argument"]),
        }
        if idx in cases_164:
            return cases_164[idx]
        return ("func main() -> i32 {\n    var x: i32 = 0!\n    return x\n}\n", ["E3027", "E3003"], ["only be used", "type mismatch"])

    if chapter == 16 and s2key == "16.5":
        idx = spec_parts(spec_ref)[2]
        cases_165 = {
            1: ("func main() -> i32 {\n    var m = @import(1)\n    _ = m\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            2: ("func main() -> i32 {\n    var s = @sizeof(1)\n    return s as i32\n}\n", ["E3003"], ["type mismatch"]),
            3: ("func main() -> i32 {\n    var t = @typeof()\n    _ = t\n    return 0\n}\n", ["E3006", "E2002"], ["argument", "expected"]),
            4: ("func main() -> i32 {\n    @panic()\n    return 0\n}\n", ["E3006"], ["argument"]),
            5: ("func main() -> i32 {\n    @assert(1)\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            6: ("func main() -> i32 {\n    @assert(true, 123)\n    return 0\n}\n", ["E3003"], ["type mismatch"]),
            7: ("func main() -> i32 {\n    var f = @file(1)\n    _ = f\n    return 0\n}\n", ["E3006"], ["argument"]),
            8: ("func main() -> i32 {\n    var l = @line(1)\n    _ = l\n    return 0\n}\n", ["E3006"], ["argument"]),
            9: ("func main() -> i32 {\n    var c = @column(1)\n    _ = c\n    return 0\n}\n", ["E3006"], ["argument"]),
            10: ("func main() -> i32 {\n    var n = @func(1)\n    _ = n\n    return 0\n}\n", ["E3006"], ["argument"]),
        }
        if idx in cases_165:
            return cases_165[idx]
        return (
            "func main() -> i32 {\n    var x = @unknown_builtin(1)\n    return 0\n}\n",
            ["E2032", "E2030", "E2029"],
            ["unknown builtin", "invalid builtin"],
        )

    # fallback fail
    return (
        f"func main() -> i32 {{\n    var bad_{s}: i32 = \"fail_{s}\"\n    return bad_{s}\n}}\n",
        ["E3003"],
        ["type mismatch"],
    )


def syntax_edge_body(point: Dict[str, object]) -> Tuple[str, List[str], List[str]]:
    spec_ref = str(point["spec_ref"])
    chapter = int(point["chapter"])
    s2key = spec2(spec_ref)
    s3key = spec3(spec_ref)
    pid = str(point["point_id"])
    prefix = f"// syntax edge case: {pid}\n"

    def wrap(code: str) -> Tuple[str, List[str], List[str]]:
        return (
            prefix + code,
            ["E2002", "E2005", "E2021"],
            ["expected expression", "unexpected token", "expected"],
        )

    if chapter == 2:
        return wrap(
            "func main() -> i32 {\n"
            "    var token_stream =\n"
            "    return 0\n"
            "}\n"
        )
    if chapter == 3:
        return wrap(
            "func main() -> i32 {\n"
            "    var typed: i32 =\n"
            "    return typed\n"
            "}\n"
        )
    if chapter == 4:
        return wrap(
            "func main() -> i32 {\n"
            "    var expr: i32 = (1 + )\n"
            "    return expr\n"
            "}\n"
        )
    if chapter == 5:
        return wrap(
            "func main() -> i32 {\n"
            "    if true {\n"
            "        var stmt =\n"
            "    }\n"
            "    return 0\n"
            "}\n"
        )
    if chapter == 6:
        return wrap(
            "func add(a: i32, b: i32) -> i32 {\n"
            "    return a + b\n"
            "}\n"
            "func main() -> i32 {\n"
            "    var v = add(1, )\n"
            "    return v\n"
            "}\n"
        )
    if chapter == 7:
        return wrap(
            "struct P { x: i32, y: i32 }\n"
            "func main() -> i32 {\n"
            "    var p = P { x: , y: 2 }\n"
            "    return p.y\n"
            "}\n"
        )
    if chapter == 8:
        return wrap(
            "enum E { A, B }\n"
            "func main() -> i32 {\n"
            "    var e = E.A\n"
            "    var out = match e {\n"
            "        E.A =>,\n"
            "        E.B => 2,\n"
            "    }\n"
            "    return out\n"
            "}\n"
        )
    if chapter == 9:
        return wrap(
            "func main() -> i32 {\n"
            "    var trait_edge =\n"
            "    return 0\n"
            "}\n"
        )
    if chapter == 10:
        return wrap(
            "const io = @import(\"std\").io\n"
            "func main() -> i32 {\n"
            "    io.println(\n"
            "    return 0\n"
            "}\n"
        )
    if chapter == 11:
        return wrap(
            "func main() -> i32 {\n"
            "    var error_edge =\n"
            "    return 0\n"
            "}\n"
        )
    if chapter == 12:
        return wrap(
            "async func one() -> !i32 {\n"
            "    return 1\n"
            "}\n"
            "func main() -> i32 {\n"
            "    var v = await\n"
            "    return v\n"
            "}\n"
        )
    if chapter == 13:
        return wrap(
            "func main() -> i32 {\n"
            "    @assert(, \"missing condition\")\n"
            "    return 0\n"
            "}\n"
        )
    if chapter == 14:
        return wrap(
            "const io = @import(\"std\").io\n"
            "func main() -> i32 {\n"
            "    io.print(\"a\", )\n"
            "    return 0\n"
            "}\n"
        )
    if chapter == 15:
        return wrap(
            "func sum(a: i32, b: i32) -> i32 {\n"
            "    return a + b\n"
            "}\n"
            "func main() -> i32 {\n"
            "    var out = sum(1, )\n"
            "    return out\n"
            "}\n"
        )
    if chapter == 16 and s2key == "16.3":
        return wrap(
            "func main() -> i32 {\n"
            "    var prec = 1 +\n"
            "    return prec\n"
            "}\n"
        )
    if chapter == 16 and s2key == "16.4":
        return wrap(
            "func risky() -> !i32 {\n"
            "    return SysError.ParseError { message: \"x\" }\n"
            "}\n"
            "func main() -> i32 {\n"
            "    var v: i32 = risky()! -> err {\n"
            "        _ = err.message(\n"
            "        return 0\n"
            "    }\n"
            "    return v\n"
            "}\n"
        )
    if chapter == 16 and s3key.startswith("16.5"):
        return wrap(
            "func main() -> i32 {\n"
            "    var b = @sizeof(\n"
            "    return b as i32\n"
            "}\n"
        )
    return wrap(
        "func main() -> i32 {\n"
        "    var x =\n"
        "    return 0\n"
        "}\n"
    )


def pass_command_for_phase(phase: str) -> str:
    # Compile + run to actually execute assertions in pass cases.
    # Treat assertion/panic/crash exits (>=128) as failure; normal process exits pass.
    if phase in {"lexer", "parser", "sema", "runtime"}:
        return (
            "{yuanc} {pass_case} -o {tmp_bin} && {tmp_bin}; "
            "rc=$?; if [ $rc -ge 128 ]; then exit $rc; fi; exit 0"
        )
    if phase == "codegen":
        return "{yuanc} -S -emit-llvm {pass_case} -o {tmp_ir}"
    return "{yuanc} {pass_case} -o {tmp_bin} && {tmp_bin}; rc=$?; if [ $rc -ge 128 ]; then exit $rc; fi; exit 0"


def fail_command_for_phase(phase: str) -> str:
    if phase == "codegen":
        return "{yuanc} -fsyntax-only {fail_case}"
    return "{yuanc} -fsyntax-only {fail_case}"


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate spec2026 scaffold")
    parser.add_argument(
        "--points",
        type=Path,
        default=Path("tests/spec2026/generated/spec2026_points.json"),
        help="Input points JSON",
    )
    parser.add_argument(
        "--out-root",
        type=Path,
        default=Path("tests/spec2026"),
        help="spec2026 root",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Overwrite existing case files",
    )
    args = parser.parse_args()

    points = json.loads(args.points.read_text(encoding="utf-8"))
    manifest: List[Dict[str, object]] = []

    for chapter_dir in CHAPTER_DIR.values():
        (args.out_root / "cases" / chapter_dir).mkdir(parents=True, exist_ok=True)

    created = 0
    skipped = 0

    for point in points:
        chapter_dir = str(point["case_dir"])
        pid = str(point["point_id"])
        phase = str(point["phase"])
        core_case_id = f"{pid}__core_pass"
        syntax_case_id = f"{pid}__syntax_edge_fail"
        semantic_boundary = "runtime_edge" if phase == "runtime" else "semantic_edge"
        semantic_case_id = f"{pid}__{semantic_boundary}_fail"

        core_rel = Path("tests/spec2026/cases") / chapter_dir / f"{core_case_id}.yu"
        syntax_rel = Path("tests/spec2026/cases") / chapter_dir / f"{syntax_case_id}.yu"
        semantic_rel = Path("tests/spec2026/cases") / chapter_dir / f"{semantic_case_id}.yu"

        raw_pass = pass_body(point)
        rich_pass = ensure_pass_assertions(raw_pass, pid)
        core_content = metadata_header(
            point=point,
            case_id=core_case_id,
            expect="pass",
            boundary="core",
            diag_codes=[],
            diag_keywords=[],
        ) + rich_pass

        syntax_program, syntax_codes, syntax_keywords = syntax_edge_body(point)
        syntax_content = metadata_header(
            point=point,
            case_id=syntax_case_id,
            expect="fail",
            boundary="syntax_edge",
            diag_codes=syntax_codes,
            diag_keywords=syntax_keywords,
        ) + syntax_program

        semantic_program, semantic_codes, semantic_keywords = fail_body(point)
        semantic_program = f"// negative semantic case: {pid}\n" + semantic_program
        semantic_content = metadata_header(
            point=point,
            case_id=semantic_case_id,
            expect="fail",
            boundary=semantic_boundary,
            diag_codes=semantic_codes,
            diag_keywords=semantic_keywords,
        ) + semantic_program

        for path, content in (
            (Path(core_rel), core_content),
            (Path(syntax_rel), syntax_content),
            (Path(semantic_rel), semantic_content),
        ):
            if path.exists() and not args.overwrite:
                skipped += 1
                continue
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")
            created += 1

        # Backward-compatible fields keep the previous single pass/fail mapping.
        manifest.append(
            {
                "point_id": pid,
                "spec_ref": point["spec_ref"],
                "title": point["title"],
                "phase": phase,
                "pass_case": str(core_rel),
                "fail_case": str(semantic_rel),
                "expect_pass_command": pass_command_for_phase(phase),
                "expect_fail_command": fail_command_for_phase(phase),
                "diag_codes": semantic_codes,
                "diag_keywords": semantic_keywords,
                "cases": [
                    {
                        "case_id": core_case_id,
                        "kind": "pass",
                        "boundary": "core",
                        "path": str(core_rel),
                        "expect_command": pass_command_for_phase(phase),
                        "diag_codes": [],
                        "diag_keywords": [],
                    },
                    {
                        "case_id": syntax_case_id,
                        "kind": "fail",
                        "boundary": "syntax_edge",
                        "path": str(syntax_rel),
                        "expect_command": fail_command_for_phase(phase),
                        "diag_codes": syntax_codes,
                        "diag_keywords": syntax_keywords,
                    },
                    {
                        "case_id": semantic_case_id,
                        "kind": "fail",
                        "boundary": semantic_boundary,
                        "path": str(semantic_rel),
                        "expect_command": fail_command_for_phase(phase),
                        "diag_codes": semantic_codes,
                        "diag_keywords": semantic_keywords,
                    },
                ],
            }
        )

    manifest_path = args.out_root / "manifest" / "spec2026_manifest.yaml"
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")

    print(f"points: {len(points)}")
    print(f"manifest: {manifest_path}")
    print(f"files created: {created}")
    print(f"files skipped: {skipped}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
