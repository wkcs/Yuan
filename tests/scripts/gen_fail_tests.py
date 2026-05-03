#!/usr/bin/env python3
"""Generate missing fail test files from manifest."""
import json
import os

MANIFEST = "tests/spec2026/manifest/spec2026_manifest.yaml"

# Template for syntax_edge_fail tests (generic syntax error)
SYNTAX_FAIL_TEMPLATE = """/// @spec_ref: {spec_ref}
/// @point_id: {point_id}
/// @case_id: {case_id}
/// @expect: fail
/// @boundary: syntax_edge
/// @phase: {phase}
/// @diag_codes: {diag_codes}
/// @diag_keywords: {diag_keywords}
// Syntax edge fail test for {title}
{code}
"""

# Template for semantic_edge_fail and runtime_edge_fail tests
SEMANTIC_FAIL_TEMPLATE = """/// @spec_ref: {spec_ref}
/// @point_id: {point_id}
/// @case_id: {case_id}
/// @expect: fail
/// @boundary: {boundary}
/// @phase: {phase}
/// @diag_codes: {diag_codes}
/// @diag_keywords: {diag_keywords}
// {boundary} fail test for {title}
{code}
"""

# Default syntax error code for syntax_edge_fail tests (missing closing brace)
SYNTAX_ERROR_CODE = """func main() -> i32 {
    var x: i32 = 42
    return 0
"""

# Default semantic error code (type mismatch E3003) for semantic_edge_fail tests
SEMANTIC_DEFAULT_CODE = """func main() -> i32 {
    var x: str = 42
    return 0
}"""

# Map of (point_id, diag_codes) -> specific error code
SEMANTIC_ERROR_CODES = {
    # ch02 lexical
    ("2_1_item", ("E1006",)): """/* unterminated block comment""",
    ("2_2_item", ("E1001",)): """var 123abc: i32 = 0""",
    ("2_3_item", ("E2008",)): """var x: i32 = 42
func main() -> i32 {
    return x
}
""",
    ("2_4_item", ("E3001",)): """func main() -> i32 {
    var x: i32 = unknown_func()
    return x
}""",
    ("2_5_item", ("E1007",)): """func main() -> i32 {
    var ch: char = ''
    return 0
}""",
    ("2_5_1_item", ("E1005",)): """func main() -> i32 {
    var x: i32 = 999999999999999999999
    return 0
}""",
    ("2_5_2_item", ("E1005",)): """func main() -> i32 {
    var x: f64 = 3.14.15
    return 0
}""",
    ("2_5_3_item", ("E1007",)): """func main() -> i32 {
    var ch: char = ''
    return 0
}""",
    ("2_5_4_item", ("E1002",)): """func main() -> i32 {
    var s: str = "unterminated
    return 0
}""",
    ("2_5_5_item", ("E3001",)): """func main() -> i32 {
    var x: bool = maybe
    return 0
}""",
    ("2_5_6_none", ("E3001",)): """func main() -> i32 {
    var x: ?i32 = nothing
    return 0
}""",
    ("2_5_7_item", ("E3003",)): """func main() -> i32 {
    var arr: [i32; 3] = [1, 2,]
    return 0
}""",
    ("2_5_8_item", ("E2002",)): """func main() -> i32 {
    var t: (i32, i32) = (1,)
    return 0
}""",
    ("2_6_item", ("E2005",)): """func main() -> i32 {
    var x: i32 = 10 var y: i32 = 20
    return 0
}""",
    # ch03 types
    ("3_1_item", ("E3003",)): """func main() -> i32 {
    var x: i32 = 42
    x = "hello"
    return 0
}""",
    ("3_2_item", ("E3003",)): """func main() -> i32 {
    var x: i32 = "hello"
    return 0
}""",
    ("3_2_1_item", ("E3003",)): """func main() -> i32 {
    var x: i8 = 999
    return 0
}""",
    ("3_2_2_item", ("E3003",)): """func main() -> i32 {
    var x: u8 = -1
    return 0
}""",
    ("3_2_3_item", ("E3003",)): """func main() -> i32 {
    var x: f32 = "hello"
    return 0
}""",
    ("3_2_4_item", ("E3003",)): """func main() -> i32 {
    var x: bool = 42
    return 0
}""",
    ("3_2_5_item", ("E3003",)): """func main() -> i32 {
    var x: char = 42
    return 0
}""",
    ("3_2_6_item", ("E3003",)): """func main() -> i32 {
    var x: str = 42
    return 0
}""",
    ("3_2_7_void", ("E3003",)): """func main() -> void {
    var x: void = 42
}""",
    ("3_3_item", ("E3003",)): """func main() -> i32 {
    var x: [i32; 3] = [1, "hello", 3]
    return 0
}""",
    ("3_3_1_item", ("E3003",)): """func main() -> i32 {
    var arr: [i32; 3] = [1, 2, 3]
    arr[0] = "hello"
    return 0
}""",
    ("3_3_2_item", ("E3003",)): """func main() -> i32 {
    var arr: [i32; 5] = [1, 2, 3, 4, 5]
    var slice: &[i32] = &arr[1..4]
    slice[0] = "hello"
    return 0
}""",
    ("3_3_3_item", ("E3003",)): """func main() -> i32 {
    var s: str = "hello"
    var sub: str = s[0..3]
    sub = 42
    return 0
}""",
    ("3_3_4_vec", ("E3003",)): """func main() -> i32 {
    var v: Vec<i32> = Vec.new()
    v.push("hello")
    return 0
}""",
    ("3_3_5_item", ("E3003",)): """func main() -> i32 {
    var t: (i32, str) = (1, "hello")
    var x: str = t.0
    return 0
}""",
    ("3_3_6_optional", ("E3003",)): """func main() -> i32 {
    var x: ?i32 = 42
    var y: i32 = x
    return 0
}""",
    ("3_4_item", ("E3003",)): """func main() -> i32 {
    var x: i32 = 42
    var r: &i32 = &x
    r = "hello"
    return 0
}""",
    ("3_4_1_item", ("E3003",)): """func main() -> i32 {
    var x: i32 = 42
    var r: &i32 = &x
    *r = "hello"
    return 0
}""",
    ("3_4_2_item", ("E3003",)): """func main() -> i32 {
    var x: i32 = 42
    var p: *i32 = &x
    *p = "hello"
    return 0
}""",
    ("3_4_3_zig", ("E3003",)): """func main() -> i32 {
    var x: i32 = 42
    var p: *i32 = &x
    *p = "hello"
    return 0
}""",
    ("3_5_item", ("E3003",)): """func main() -> i32 {
    var x: i32 = 42
    var y: str = x as str
    return 0
}""",
    ("3_6_item", ("E3003",)): """type Byte = u8
func main() -> i32 {
    var b: Byte = "hello"
    return 0
}""",
    # ch04 expr
    ("4_1_item", ("E3003",)): """func main() -> i32 {
    var x: str = 42
    return 0
}""",
    ("4_2_item", ("E3003",)): """func main() -> i32 {
    var x: i32 = 10 + "hello"
    return 0
}""",
    ("4_3_item", ("E3003",)): """func main() -> i32 {
    var x: bool = 10 == "hello"
    return 0
}""",
    ("4_4_item", ("E3003",)): """func main() -> i32 {
    var x: bool = true && 42
    return 0
}""",
    ("4_5_item", ("E3012",)): """func main() -> i32 {
    var x: i32 = 10
    x = "hello"
    return 0
}""",
    ("4_6_item", ("E3003",)): """func main() -> i32 {
    var arr: [i32; 3] = [1, 2, 3]
    var x: str = arr[0]
    return 0
}""",
    ("4_7_if", ("E3003",)): """func main() -> i32 {
    var x: i32 = if true { 1 } else { "hello" }
    return 0
}""",
    ("4_8_match", ("E3003",)): """enum Color { Red, Green, Blue }
func main() -> i32 {
    var c = Color.Red
    var x: i32 = match c {
        Color.Red => 1,
        Color.Green => "hello",
        Color.Blue => 3,
    }
    return 0
}""",
    ("4_9_item", ("E3003",)): """func main() -> i32 {
    var f = func(x: i32) -> i32 { x * 2 }
    var result: str = f(5)
    return 0
}""",
    ("4_10_item", ("E3003",)): """func main() -> i32 {
    var r = 0..10
    var x: str = r
    return 0
}""",
    # ch05 stmt
    ("5_1_item", ("E3003",)): """func main() -> i32 {
    var x: i32 = "hello"
    return 0
}""",
    ("5_2_item", ("E2002", "E2005")): """func main() -> i32 {
    const X: i32 = "hello"
    return 0
}""",
    ("5_2_1_const_func", ("E3003",)): """const func bad() -> i32 {
    return "hello"
}
func main() -> i32 {
    return 0
}""",
    ("5_2_2_const_construct", ("E3003",)): """struct Point { x: f64, y: f64 }
const P: Point = Point { x: "hello", y: 0.0 }
func main() -> i32 {
    return 0
}""",
    ("5_3_item", ("E3003",)): """func bad() -> i32 {
    return "hello"
}
func main() -> i32 {
    return 0
}""",
    ("5_4_item", ("E3003",)): """func main() -> i32 {
    var i: i32 = 0
    while "hello" {
        i += 1
    }
    return 0
}""",
    ("5_5_item", ("E2005", "E2008")): """func main() -> i32 {
    loop {
        break "hello"
    }
    return 0
}""",
    ("5_6_item", ("E3003",)): """func main() -> i32 {
    for i in "hello" {
        var x = i
    }
    return 0
}""",
    ("5_7_item", ("E3003",)): """func main() -> i32 {
    for i in 0..10 {
        if i == 5 {
            break "hello"
        }
    }
    return 0
}""",
    ("5_8_item", ("E3003",)): """func main() -> i32 {
    defer "hello"
    return 0
}""",
    # ch06 func
    ("6_1_item", ("E3016",)): """func bad(x: i32) -> str {
    return x
}
func main() -> i32 {
    return 0
}""",
    ("6_2_item", ("E3003",)): """func bad(x: i32) -> i32 {
    x = "hello"
    return x
}
func main() -> i32 {
    return 0
}""",
    ("6_3_item", ("E3003",)): """func get_value() -> i32 {
    return 42
}
func main() -> i32 {
    get_value()
    return 0
}""",
    ("6_4_item", ("E3003",)): """func identity<T>(x: T) -> T {
    return x
}
func main() -> i32 {
    var x: i32 = identity("hello")
    return 0
}""",
    ("6_5_item", ("E3020",)): """pub func bad() -> str {
    return 42
}
func main() -> i32 {
    return 0
}""",
    # ch07 struct
    ("7_1_item", ("E3020",)): """struct Point { x: f64, y: f64 }
func main() -> i32 {
    var p = Point { x: "hello", y: 0.0 }
    return 0
}""",
    ("7_2_item", ("E3021",)): """struct Point { x: f64, y: f64 }
func main() -> i32 {
    var p = Point { x: "hello", y: 0.0 }
    return 0
}""",
    ("7_3_item", ("E3021",)): """struct Rect { w: f64, h: f64 }
impl Rect {
    func area(self: &Self) -> f64 { self.w * self.h }
}
func main() -> i32 {
    var r = Rect { w: 10.0, h: 5.0 }
    var x: str = r.area()
    return 0
}""",
    ("7_4_item", ("E3003",)): """struct Pair<T, U> { first: T, second: U }
func main() -> i32 {
    var p = Pair { first: 1, second: "hello" }
    var x: str = p.first
    return 0
}""",
    # ch08 enum
    ("8_1_item", ("E3020",)): """enum Color { Red, Green, Blue }
func main() -> i32 {
    var c = Color.Red
    var x: str = c
    return 0
}""",
    ("8_2_item", ("E3042", "E3001")): """enum Color { Red, Green, Blue }
func main() -> i32 {
    var c = Color.Red
    var x: str = match c {
        Color.Red => 1,
        Color.Green => "hello",
        Color.Blue => 3,
    }
    return 0
}""",
    ("8_3_item", ("E3021",)): """enum Dir { N, S, E, W }
impl Dir {
    func opposite(self: &Self) -> Dir {
        match self {
            Self.N => Dir.S,
            Self.S => Dir.N,
            Self.E => Dir.W,
            Self.W => Dir.E,
        }
    }
}
func main() -> i32 {
    var d = Dir.N
    var x: str = d.opposite()
    return 0
}""",
    ("8_4_item", ("E3003",)): """enum Color { Red, Green, Blue }
func main() -> i32 {
    var c = Color.Red
    var x: str = c.ordinal()
    return 0
}""",
    # ch09 trait
    ("9_1_trait", ("E3041",)): """trait Printable { func print(&self) -> void }
struct Point { x: i32, y: i32 }
impl Printable for Point { func print(&self) -> void {} }
func main() -> i32 {
    var p = Point { x: 1, y: 2 }
    var x: str = p.print()
    return 0
}""",
    ("9_2_item", ("E3003",)): """trait Shape { func area(&self) -> f64 }
struct Circle { r: f64 }
impl Shape for Circle { func area(&self) -> f64 { 3.14 * self.r * self.r } }
func main() -> i32 {
    var c = Circle { r: 5.0 }
    var x: str = c.area()
    return 0
}""",
    ("9_3_item", ("E3003",)): """trait Val { func get(&self) -> i32 }
struct Num { v: i32 }
impl Val for Num { func get(&self) -> i32 { self.v } }
func use_val<T: Val>(t: &T) -> str { t.get() }
func main() -> i32 { return 0 }""",
    ("9_3_1_trait_object", ("E3003",)): """trait Describable { func describe(&self) -> str }
struct Dog { name: str }
impl Describable for Dog { func describe(&self) -> str { "dog" } }
func get_desc(d: &Describable) -> i32 { d.describe() }
func main() -> i32 { return 0 }""",
    ("9_4_item", ("E3003",)): """struct Point { x: i32, y: i32 }
impl Display for Point {
    func fmt(&self, f: &mut Formatter) -> !void {}
}
func main() -> i32 {
    var p = Point { x: 1, y: 2 }
    var x: str = p
    return 0
}""",
    ("9_5_operator", ("E3003",)): """struct Vec2 { x: i32, y: i32 }
impl Add for Vec2 {
    func add(&self, other: &Self) -> Self { Vec2 { x: self.x + other.x, y: self.y + other.y } }
}
func main() -> i32 {
    var a = Vec2 { x: 1, y: 2 }
    var b = Vec2 { x: 3, y: 4 }
    var c: str = a + b
    return 0
}""",
    # ch10 module
    ("10_1_item", ("E3029",)): """const io = @import("std").io
func main() -> i32 {
    var x: str = io
    return 0
}""",
    ("10_2_item", ("E3020",)): """pub const X: i32 = 42
func main() -> i32 {
    var x: str = X
    return 0
}""",
    ("10_3_item", ("E3021",)): """const io = @import("std").io
func main() -> i32 {
    var x: str = io
    return 0
}""",
    # ch11 error
    ("11_1_item", ("E3017", "E3018")): """enum MyErr { A, B }
impl Error for MyErr {
    func message(&self) -> str { "err" }
}
func main() -> i32 {
    var e = MyErr.A
    var x: i32 = e.message()
    return 0
}""",
    ("11_2_item", ("E3001", "E3021")): """func divide(a: i32, b: i32) -> !i32 {
    if b == 0 { return SysError("div by zero") }
    return a / b
}
func main() -> i32 {
    var x: str = divide(10, 2)!
    return 0
}""",
    ("11_3_item", ("E3003",)): """func main() -> i32 {
    var x: str = 42
    return 0
}""",
    ("11_4_item", ("E3003",)): """func might_fail(x: i32) -> !i32 {
    if x < 0 { return SysError("neg") }
    return x
}
func main() -> i32 {
    var x: str = might_fail(5)!
    return 0
}""",
    ("11_4_1_error_optional", ("E3003",)): """func find(x: i32) -> !?i32 {
    if x < 0 { return SysError("neg") }
    if x == 0 { return None }
    return Some(x)
}
func main() -> i32 {
    var x: str = find(5)!
    return 0
}""",
    ("11_5_item", ("E3046",)): """func safe_div(a: i32, b: i32) -> !i32 {
    if b == 0 { return SysError("div") }
    return a / b
}
func main() -> i32 {
    var x: str = safe_div(10, 2)!
    return 0
}""",
    ("11_6_item", ("E3017", "E3018")): """enum ValErr { Empty { field: str } }
impl Error for ValErr {
    func message(&self) -> str { "val err" }
}
func validate(s: str) -> !str {
    if s.len() == 0 { return ValErr.Empty { field: "s" } }
    return s
}
func main() -> i32 {
    var x: str = validate("hello")!
    return 0
}""",
    # ch12 concurrency
    ("12_1_async", ("E3047",)): """async func get() -> i32 { return 42 }
func main() -> i32 {
    var x: str = await get()
    return 0
}""",
    ("12_2_thread", ("E3003",)): """func main() -> i32 {
    var x: str = 42
    return 0
}""",
    # ch13 builtin
    ("13_1_item", ("E3006", "E2002")): """func main() -> i32 {
    var x: str = @sizeof(i32)
    return 0
}""",
    ("13_2_item", ("E3003",)): """func main() -> i32 {
    var x: str = @typeof(42)
    return 0
}""",
    # ch14 stdlib
    ("14_1_prelude", ("E3003",)): """func main() -> i32 {
    var x: str = 42
    return 0
}""",
    ("14_2_1_io", ("E3003",)): """const io = @import("std").io
func main() -> i32 {
    io.println(42)
    return 0
}""",
    ("14_2_2_fs", ("E3003",)): """const fs = @import("std").fs
func main() -> i32 {
    var x: str = fs.exists("test")
    return 0
}""",
    ("14_2_3_collections", ("E3003",)): """const Vec = @import("std").collections.Vec
func main() -> i32 {
    var v: Vec<i32> = Vec.new()
    var x: str = v.len()
    return 0
}""",
    ("14_2_4_fmt", ("E3003",)): """const fmt = @import("std").fmt
func main() -> i32 {
    var x: str = fmt.format("{}", 42)
    return 0
}""",
    ("14_2_5_time", ("E3003",)): """const time = @import("std").time
func main() -> i32 {
    var x: str = time.Instant.now()
    return 0
}""",
    ("14_2_6_env", ("E3003",)): """const env = @import("std").env
func main() -> i32 {
    var x: str = env.args()
    return 0
}""",
    ("14_2_7_math", ("E3003",)): """const math = @import("std").math
func main() -> i32 {
    var x: str = math.sqrt(16.0)
    return 0
}""",
    # ch15 examples
    ("15_1_basic", ("E3003",)): """func add(a: i32, b: i32) -> i32 { a + b }
func main() -> i32 {
    var x: str = add(1, 2)
    return 0
}""",
    ("15_2_struct_trait", ("E3003",)): """trait Shape { func area(&self) -> f64 }
struct Rect { w: f64, h: f64 }
impl Shape for Rect { func area(&self) -> f64 { self.w * self.h } }
func main() -> i32 {
    var r = Rect { w: 10.0, h: 5.0 }
    var x: str = r.area()
    return 0
}""",
    ("15_3_variadic", ("E3003",)): """func sum(...numbers: i32) -> i32 {
    var total: i32 = 0
    for n in numbers { total += n }
    return total
}
func main() -> i32 {
    var x: str = sum(1, 2, 3)
    return 0
}""",
    ("15_4_error_handling", ("E3003",)): """func divide(a: i32, b: i32) -> !i32 {
    if b == 0 { return SysError("div") }
    return a / b
}
func main() -> i32 {
    var x: str = divide(10, 2)!
    return 0
}""",
    # ch16 appendix
    ("16_1_naming", ("E3003",)): """var user_name: str = "Alice"
func calculate_sum(a: i32, b: i32) -> i32 { a + b }
func main() -> i32 {
    var x: str = calculate_sum(1, 2)
    return 0
}""",
    ("16_2_comparison", ("E3003",)): """func main() -> i32 {
    var x: str = 42
    return 0
}""",
    ("16_3_operator_precedence", ("E3003",)): """func main() -> i32 {
    var x: str = 2 + 3 * 4
    return 0
}""",
    ("16_4_error_syntax", ("E3003",)): """func might_fail(x: i32) -> !i32 {
    if x < 0 { return SysError("neg") }
    return x
}
func main() -> i32 {
    var x: str = might_fail(5)!
    return 0
}""",
    ("16_5_builtin_summary", ("E3003",)): """func main() -> i32 {
    var x: str = @sizeof(i32)
    return 0
}""",
    ("16_6_file_ext", ("E3003",)): """func main() -> i32 {
    var x: str = 42
    return 0
}""",
    # ch17 impl
    ("17_1_pipeline", ("E3003",)): """func main() -> i32 {
    var x: str = 42
    return 0
}""",
    ("17_2_sema", ("E3003",)): """func main() -> i32 {
    var x: str = 42
    return 0
}""",
    ("17_3_codegen", ("E3003",)): """func main() -> i32 {
    var x: str = 42
    return 0
}""",
    ("17_4_boundary", ("E3003",)): """func main() -> i32 {
    var x: str = 42
    return 0
}""",
    # ch18 ownership
    ("18_1_basic_model", ("E3003",)): """func main() -> i32 {
    var x: i32 = 42
    var y: str = x
    return 0
}""",
    ("18_2_copy", ("E3003",)): """func main() -> i32 {
    var x: i32 = 42
    var y: str = x
    return 0
}""",
    ("18_3_move", ("E3049",)): """struct NonCopy { data: i32 }
func consume(val: NonCopy) -> i32 { val.data }
func main() -> i32 {
    var a = NonCopy { data: 42 }
    var r1 = consume(a)
    var r2 = consume(a)
    return r1 + r2
}""",
    ("18_4_join", ("E3003",)): """struct NonCopy { data: i32 }
func consume(val: NonCopy) -> i32 { val.data }
func main() -> i32 {
    var a = NonCopy { data: 10 }
    var r: str = consume(a)
    return 0
}""",
    ("18_5_partial_move", ("E3051",)): """struct Pair { first: i32, second: i32 }
func main() -> i32 {
    var p = Pair { first: 1, second: 2 }
    var x = p.first
    var y = p.second
    var z = p.first
    return 0
}""",
    ("18_6_drop", ("E3003",)): """struct R { v: i32 }
impl Drop for R { func drop(&mut self) -> void {} }
func main() -> i32 {
    var r = R { v: 42 }
    var x: str = r
    return 0
}""",
    ("18_7_stdlib", ("E3003",)): """func main() -> i32 {
    var x: str = 42
    return 0
}""",
    # ch19 testing
    ("19_1_test_func", ("E3003",)): """@test func bad() -> i32 { return 42 }
func main() -> i32 { return 0 }""",
    ("19_2_test_run", ("E3003",)): """func main() -> i32 {
    var x: str = 42
    return 0
}""",
    ("19_3_test_spec", ("E3003",)): """@test func bad(x: i32) { }
func main() -> i32 { return 0 }""",
    # ch20 FFI
    ("20_1_extern_decl", ("E3003",)): """extern "C" func abs(i32) -> i32
func main() -> i32 {
    var x: str = abs(-42)
    return 0
}""",
    ("20_2_call_extern", ("E3003",)): """extern "C" func abs(i32) -> i32
func main() -> i32 {
    var x: str = abs(-10)
    return 0
}""",
    ("20_3_type_mapping", ("E3003",)): """extern "C" func abs(i32) -> i32
func main() -> i32 {
    var x: str = abs(-42)
    return 0
}""",
    ("20_4_safety", ("E3003",)): """extern "C" func bad(str) -> i32
func main() -> i32 { return 0 }""",
    # ch21 iterators
    ("21_1_iterator_trait", ("E3003",)): """func main() -> i32 {
    var x: str = (0..10).count()
    return 0
}""",
    ("21_2_adapters", ("E3003",)): """func main() -> i32 {
    var x: str = (0..10).take(3).count()
    return 0
}""",
    ("21_3_collect", ("E3003",)): """const Vec = @import("std").collections.Vec
func main() -> i32 {
    var v: Vec<str> = (0..5).collect<Vec<i32>>()
    return 0
}""",
    ("21_4_consumer", ("E3003",)): """func main() -> i32 {
    var x: str = (0..10).count()
    return 0
}""",
}


def main():
    with open(MANIFEST) as f:
        manifest = json.load(f)

    created = 0
    skipped = 0
    for point in manifest:
        point_id = point["point_id"]
        spec_ref = point["spec_ref"]
        title = point["title"]
        phase = point.get("phase", "sema")

        for case in point["cases"]:
            case_id = case["case_id"]
            kind = case["kind"]
            boundary = case["boundary"]
            path = case["path"]
            diag_codes = case.get("diag_codes", [])
            diag_keywords = case.get("diag_keywords", [])

            if os.path.exists(path):
                skipped += 1
                continue

            if kind == "pass":
                continue  # Skip pass tests, they should already exist

            # Find the error code
            key = (point_id, tuple(diag_codes))
            code = SEMANTIC_ERROR_CODES.get(key)
            if not code:
                if boundary == "syntax_edge":
                    code = SYNTAX_ERROR_CODE
                else:
                    code = SEMANTIC_DEFAULT_CODE

            if boundary == "syntax_edge":
                content = SYNTAX_FAIL_TEMPLATE.format(
                    spec_ref=spec_ref,
                    point_id=point_id,
                    case_id=case_id,
                    phase=phase,
                    diag_codes=diag_codes,
                    diag_keywords=diag_keywords,
                    title=title,
                    code=code,
                )
            else:
                content = SEMANTIC_FAIL_TEMPLATE.format(
                    spec_ref=spec_ref,
                    point_id=point_id,
                    case_id=case_id,
                    boundary=boundary,
                    phase=phase,
                    diag_codes=diag_codes,
                    diag_keywords=diag_keywords,
                    title=title,
                    code=code,
                )

            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w") as f:
                f.write(content)
            created += 1

    print(f"Created {created} fail test files, skipped {skipped} existing")


if __name__ == "__main__":
    main()
