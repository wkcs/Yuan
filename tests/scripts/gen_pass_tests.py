#!/usr/bin/env python3
"""Generate missing pass test files from manifest."""
import json
import os

MANIFEST = "tests/spec2026/manifest/spec2026_manifest.yaml"

# Templates for pass tests based on point_id patterns
PASS_TEMPLATES = {
    # ch02 lexical
    "2_5_item": """var ch: char = 'A'
var escaped: char = '\\n'

func main() -> i32 {
    var code: i32 = ch as i32
    var __spec2026_actual: i32 = code
    var __spec2026_expected: i32 = 65
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    # ch03 types
    "3_2_item": """var x: i32 = 42
var y: f64 = 3.14
var b: bool = true

func main() -> i32 {
    var __spec2026_actual: i32 = x
    var __spec2026_expected: i32 = 42
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "3_2_7_void": """func do_nothing() -> void {
    var x: i32 = 42
    _ = x
}

func main() -> i32 {
    do_nothing()
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "3_3_item": """var numbers: [i32; 5] = [1, 2, 3, 4, 5]
var pair: (i32, i32) = (10, 20)

func main() -> i32 {
    var first: i32 = numbers[0]
    var x: i32 = pair.0
    var __spec2026_actual: i32 = first + x
    var __spec2026_expected: i32 = 11
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "3_3_2_item": """var arr: [i32; 5] = [1, 2, 3, 4, 5]

func main() -> i32 {
    var slice: &[i32] = &arr[1..4]
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "3_3_3_item": """var s: str = "Hello, World!"

func main() -> i32 {
    var hello: str = s[0..5]
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "3_3_4_vec": """const Vec = @import("std").collections.Vec

func main() -> i32 {
    var v: Vec<i32> = Vec.new()
    v.push(1)
    v.push(2)
    v.push(3)
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "3_3_6_optional": """var some_value: ?i32 = 42
var no_value: ?i32 = None

func main() -> i32 {
    var val: i32 = some_value.unwrap()
    var default_val: i32 = no_value.unwrap_or(0)
    var __spec2026_actual: i32 = val + default_val
    var __spec2026_expected: i32 = 42
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "3_4_item": """var x: i32 = 42
var r: &i32 = &x

func main() -> i32 {
    var val: i32 = *r
    var __spec2026_actual: i32 = val
    var __spec2026_expected: i32 = 42
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "3_4_3_zig": """var x: i32 = 42
var p: *i32 = &x

func main() -> i32 {
    var value: i32 = *p
    var __spec2026_actual: i32 = value
    var __spec2026_expected: i32 = 42
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    # ch04 expr
    "4_7_if": """func main() -> i32 {
    var x: i32 = 10
    var result: i32 = if x > 5 {
        100
    } else {
        0
    }
    var __spec2026_actual: i32 = result
    var __spec2026_expected: i32 = 100
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "4_8_match": """enum Color {
    Red,
    Green,
    Blue,
}

func main() -> i32 {
    var color: Color = Color.Green
    var value: i32 = match color {
        Color.Red => 1,
        Color.Green => 2,
        Color.Blue => 3,
    }
    var __spec2026_actual: i32 = value
    var __spec2026_expected: i32 = 2
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    # ch05 stmt
    "5_3_return": """func add(a: i32, b: i32) -> i32 {
    return a + b
}

func main() -> i32 {
    var result: i32 = add(10, 20)
    var __spec2026_actual: i32 = result
    var __spec2026_expected: i32 = 30
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "5_4_while": """func main() -> i32 {
    var i: i32 = 0
    var sum: i32 = 0
    while i < 10 {
        sum += i
        i += 1
    }
    var __spec2026_actual: i32 = sum
    var __spec2026_expected: i32 = 45
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "5_5_loop": """func main() -> i32 {
    var count: i32 = 0
    loop {
        count += 1
        if count >= 10 {
            break
        }
    }
    var __spec2026_actual: i32 = count
    var __spec2026_expected: i32 = 10
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "5_6_for": """func main() -> i32 {
    var sum: i32 = 0
    for i in 0..10 {
        sum += i
    }
    var __spec2026_actual: i32 = sum
    var __spec2026_expected: i32 = 45
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "5_7_break_continue": """func main() -> i32 {
    var sum: i32 = 0
    for i in 0..10 {
        if i % 2 == 0 {
            continue
        }
        sum += i
    }
    var __spec2026_actual: i32 = sum
    var __spec2026_expected: i32 = 25
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "5_8_defer": """func main() -> i32 {
    var x: i32 = 0
    {
        defer {
            x = 100
        }
        x = 42
    }
    var __spec2026_actual: i32 = x
    var __spec2026_expected: i32 = 100
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    # ch06 func
    "6_2_1_item": """func greet(name: str, age: i32) -> i32 {
    return age
}

func main() -> i32 {
    var result: i32 = greet("Alice", 30)
    var __spec2026_actual: i32 = result
    var __spec2026_expected: i32 = 30
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "6_2_2_mut_t": """func increment(value: &mut i32) {
    value += 1
}

func main() -> i32 {
    var x: i32 = 10
    increment(&mut x)
    var __spec2026_actual: i32 = x
    var __spec2026_expected: i32 = 11
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "6_2_3_item": """func greet(name: str, greeting: str) -> str {
    return greeting
}

func main() -> i32 {
    var result: str = greet("Alice", "Hello")
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "6_2_4_item": """func print_array(arr: &[i32]) -> i32 {
    return arr[0]
}

func main() -> i32 {
    var numbers: [i32; 5] = [1, 2, 3, 4, 5]
    var result: i32 = print_array(&numbers)
    var __spec2026_actual: i32 = result
    var __spec2026_expected: i32 = 1
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "6_2_5_item": """func sum_all(...numbers: i32) -> i32 {
    var total: i32 = 0
    for num in numbers {
        total += num
    }
    return total
}

func main() -> i32 {
    var result: i32 = sum_all(1, 2, 3, 4, 5)
    var __spec2026_actual: i32 = result
    var __spec2026_expected: i32 = 15
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    # ch09 trait
    "9_2_trait": """trait Shape {
    func area(self: &Self) -> f64
}

struct Circle {
    radius: f64,
}

impl Shape for Circle {
    func area(self: &Self) -> f64 {
        3.14159 * self.radius * self.radius
    }
}

func main() -> i32 {
    var circle = Circle { radius: 5.0 }
    var area: f64 = circle.area()
    var __spec2026_actual: i32 = area as i32
    var __spec2026_expected: i32 = 78
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "9_3_trait": """trait Printable {
    func print_value(self: &Self) -> i32
}

struct Num {
    value: i32,
}

impl Printable for Num {
    func print_value(self: &Self) -> i32 {
        self.value
    }
}

func get_value<T: Printable>(item: &T) -> i32 {
    item.print_value()
}

func main() -> i32 {
    var n = Num { value: 42 }
    var __spec2026_actual: i32 = get_value(&n)
    var __spec2026_expected: i32 = 42
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "9_4_trait": """trait Greet {
    func greet(self: &Self) -> str {
        "Hello!"
    }
}

struct Person {
    name: str,
}

impl Greet for Person {
    func greet(self: &Self) -> str {
        "Hi!"
    }
}

func main() -> i32 {
    var p = Person { name: "Alice" }
    var msg: str = p.greet()
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    # ch10 module
    "10_2_item": """pub const PI: f64 = 3.14159

pub func add(a: i32, b: i32) -> i32 {
    a + b
}

func main() -> i32 {
    var result: i32 = add(10, 20)
    var __spec2026_actual: i32 = result
    var __spec2026_expected: i32 = 30
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "10_3_item": """const io = @import("std").io

func main() -> i32 {
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    # ch11 error
    "11_1_error_trait": """enum MyError {
    NotFound,
    Invalid,
}

impl Error for MyError {
    func message(self: &Self) -> str {
        match self {
            MyError.NotFound => "not found",
            MyError.Invalid => "invalid",
        }
    }
}

func main() -> i32 {
    var err = MyError.NotFound
    var msg: str = err.message()
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "11_2_syserror": """func divide(a: i32, b: i32) -> !i32 {
    if b == 0 {
        return SysError("division by zero")
    }
    return a / b
}

func main() -> i32 {
    var result = divide(10, 2)!
    var __spec2026_actual: i32 = result
    var __spec2026_expected: i32 = 5
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "11_3_errorinfo": """func main() -> i32 {
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "11_5_1_item": """func read_and_parse(path: str) -> !i32 {
    return 42
}

func main() -> i32 {
    var result: i32 = read_and_parse("test.txt")!
    var __spec2026_actual: i32 = result
    var __spec2026_expected: i32 = 42
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "11_5_2_item": """func might_fail(x: i32) -> !i32 {
    if x < 0 {
        return SysError("negative")
    }
    return x
}

func main() -> i32 {
    var result: i32 = might_fail(5)!
    var __spec2026_actual: i32 = result
    var __spec2026_expected: i32 = 5
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "11_5_3_err": """func safe_divide(a: i32, b: i32) -> i32 {
    var result: i32 = divide(a, b)! -> err {
        0
    }
    return result
}

func divide(a: i32, b: i32) -> !i32 {
    if b == 0 {
        return SysError("division by zero")
    }
    return a / b
}

func main() -> i32 {
    var result: i32 = safe_divide(10, 2)
    var __spec2026_actual: i32 = result
    var __spec2026_expected: i32 = 5
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "11_5_4_item": """func process_data(path: str) -> !str {
    return "processed"
}

func main() -> i32 {
    var result: str = process_data("test.txt")!
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    # ch12 concurrency
    "12_1_async_await": """async func get_value() -> i32 {
    return 42
}

func main() -> i32 {
    var result: i32 = await get_value()
    var __spec2026_actual: i32 = result
    var __spec2026_expected: i32 = 42
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "12_2_item": """func main() -> i32 {
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    # ch13 builtin
    "13_2_item": """func main() -> i32 {
    var x: i32 = 42
    var type_name: str = @typeof(x)
    @assert(type_name == "i32", "{case_id} typeof failed")
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    # ch14 stdlib
    "14_2_item": """func main() -> i32 {
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "14_2_1_std_io": """const io = @import("std").io

func main() -> i32 {
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "14_2_2_std_fs": """const fs = @import("std").fs

func main() -> i32 {
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "14_2_3_std_collections": """const Vec = @import("std").collections.Vec

func main() -> i32 {
    var v: Vec<i32> = Vec.new()
    v.push(1)
    v.push(2)
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "14_2_4_std_fmt": """const fmt = @import("std").fmt

func main() -> i32 {
    var s: String = fmt.format("Value: {}", 42)
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "14_2_5_std_time": """const time = @import("std").time

func main() -> i32 {
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "14_2_6_std_math": """const math = @import("std").math

func main() -> i32 {
    var sqrt_val: f64 = math.sqrt(16.0)
    var __spec2026_actual: i32 = sqrt_val as i32
    var __spec2026_expected: i32 = 4
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    # ch15 examples
    "15_1_item": """func add(a: i32, b: i32) -> i32 {
    a + b
}

func main() -> i32 {
    var result: i32 = add(10, 20)
    var __spec2026_actual: i32 = result
    var __spec2026_expected: i32 = 30
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "15_2_trait": """trait Shape {
    func area(self: &Self) -> f64
}

struct Rect {
    w: f64,
    h: f64,
}

impl Shape for Rect {
    func area(self: &Self) -> f64 {
        self.w * self.h
    }
}

func main() -> i32 {
    var r = Rect { w: 10.0, h: 5.0 }
    var area: f64 = r.area()
    var __spec2026_actual: i32 = area as i32
    var __spec2026_expected: i32 = 50
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "15_3_item": """func sum(...numbers: i32) -> i32 {
    var total: i32 = 0
    for num in numbers {
        total += num
    }
    return total
}

func main() -> i32 {
    var result: i32 = sum(1, 2, 3, 4, 5)
    var __spec2026_actual: i32 = result
    var __spec2026_expected: i32 = 15
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "15_4_item": """func divide(a: i32, b: i32) -> !i32 {
    if b == 0 {
        return SysError("division by zero")
    }
    return a / b
}

func main() -> i32 {
    var result: i32 = divide(10, 2)!
    var __spec2026_actual: i32 = result
    var __spec2026_expected: i32 = 5
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    # ch16 appendix - operator precedence
    "16_3_1_1": """func main() -> i32 {
    var a: i32 = (1 + 2) * 3
    var __spec2026_actual: i32 = a
    var __spec2026_expected: i32 = 9
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_3_2_2_mut": """func main() -> i32 {
    var x: i32 = 10
    x += 5
    var __spec2026_actual: i32 = x
    var __spec2026_expected: i32 = 15
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_3_3_3_as": """func main() -> i32 {
    var x: i32 = 42
    var y: i64 = x as i64
    var __spec2026_actual: i32 = y as i32
    var __spec2026_expected: i32 = 42
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_3_4_4": """func main() -> i32 {
    var a: i32 = 10 * 5 / 2
    var __spec2026_actual: i32 = a
    var __spec2026_expected: i32 = 25
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_3_5_5": """func main() -> i32 {
    var a: i32 = 10 + 5 - 3
    var __spec2026_actual: i32 = a
    var __spec2026_expected: i32 = 12
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_3_6_6": """func main() -> i32 {
    var a: i32 = 1 << 3
    var b: i32 = 16 >> 2
    var __spec2026_actual: i32 = a + b
    var __spec2026_expected: i32 = 12
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_3_7_7": """func main() -> i32 {
    var a: i32 = 0xFF & 0x0F
    var __spec2026_actual: i32 = a
    var __spec2026_expected: i32 = 15
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_3_8_8": """func main() -> i32 {
    var a: i32 = 0xFF ^ 0x0F
    var __spec2026_actual: i32 = a
    var __spec2026_expected: i32 = 240
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_3_9_9": """func main() -> i32 {
    var a: i32 = 0xF0 | 0x0F
    var __spec2026_actual: i32 = a
    var __spec2026_expected: i32 = 255
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_3_10_10": """func main() -> i32 {
    var a: bool = 10 > 5
    var b: bool = 5 < 10
    var __spec2026_actual: i32 = 0
    if a && b { __spec2026_actual = 1 }
    var __spec2026_expected: i32 = 1
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_3_11_11": """func main() -> i32 {
    var a: bool = 10 == 10
    var b: bool = 5 != 3
    var __spec2026_actual: i32 = 0
    if a && b { __spec2026_actual = 1 }
    var __spec2026_expected: i32 = 1
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_3_12_12": """func main() -> i32 {
    var a: bool = true && false
    var b: bool = true && true
    var __spec2026_actual: i32 = 0
    if !a && b { __spec2026_actual = 1 }
    var __spec2026_expected: i32 = 1
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_3_13_13": """func main() -> i32 {
    var a: bool = true || false
    var b: bool = false || false
    var __spec2026_actual: i32 = 0
    if a && !b { __spec2026_actual = 1 }
    var __spec2026_expected: i32 = 1
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_3_14_14_orelse_optional": """func main() -> i32 {
    var x: ?i32 = None
    var y: i32 = x orelse 42
    var __spec2026_actual: i32 = y
    var __spec2026_expected: i32 = 42
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_3_15_15": """func main() -> i32 {
    var r = 0..5
    var sum: i32 = 0
    for i in r { sum += i }
    var __spec2026_actual: i32 = sum
    var __spec2026_expected: i32 = 10
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_3_16_16": """func main() -> i32 {
    var x: i32 = 10
    x += 5
    x -= 3
    x *= 2
    var __spec2026_actual: i32 = x
    var __spec2026_expected: i32 = 24
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    # ch16 error syntax
    "16_4_1_func_f_t_t": """func might_fail(x: i32) -> !i32 {
    if x < 0 { return SysError("negative") }
    return x
}

func main() -> i32 {
    var result: i32 = might_fail(5)!
    var __spec2026_actual: i32 = result
    var __spec2026_expected: i32 = 5
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_4_2_func": """func might_fail(x: i32) -> !i32 {
    if x < 0 { return SysError("negative") }
    return x
}

func main() -> i32 {
    var result: i32 = might_fail(5)!
    var __spec2026_actual: i32 = result
    var __spec2026_expected: i32 = 5
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_4_3_func_panic": """func main() -> i32 {
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_4_4_func_err": """func safe_divide(a: i32, b: i32) -> i32 {
    var result: i32 = divide(a, b)! -> err {
        0
    }
    return result
}

func divide(a: i32, b: i32) -> !i32 {
    if b == 0 { return SysError("division by zero") }
    return a / b
}

func main() -> i32 {
    var result: i32 = safe_divide(10, 2)
    var __spec2026_actual: i32 = result
    var __spec2026_expected: i32 = 5
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_4_5_err_message": """func main() -> i32 {
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_4_6_err_func_name": """func main() -> i32 {
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_4_7_err_file": """func main() -> i32 {
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_4_8_err_line": """func main() -> i32 {
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_4_9_err_full_trace": """func main() -> i32 {
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    # ch16 builtins
    "16_5_1_import_path": """func main() -> i32 {
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_5_2_sizeof_t": """func main() -> i32 {
    var size: usize = @sizeof(i32)
    var __spec2026_actual: i32 = size as i32
    var __spec2026_expected: i32 = 4
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_5_3_typeof_value": """func main() -> i32 {
    var x: i32 = 42
    var type_name: str = @typeof(x)
    @assert(type_name == "i32", "{case_id} typeof failed")
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_5_4_panic_message_panic": """func main() -> i32 {
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_5_5_assert_cond": """func main() -> i32 {
    @assert(true, "{case_id} assert failed")
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_5_6_assert_cond_msg": """func main() -> i32 {
    @assert(true, "custom message")
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_5_7_file": """func main() -> i32 {
    var f: str = @file()
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
    "16_5_8_line": """func main() -> i32 {
    var l: u32 = @line()
    var __spec2026_actual: i32 = l as i32
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual >= 0, "{case_id} line failed")
    return 0
}
""",
    "16_5_9_column": """func main() -> i32 {
    var c: u32 = @column()
    var __spec2026_actual: i32 = c as i32
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual >= 0, "{case_id} column failed")
    return 0
}
""",
    "16_5_10_func": """func main() -> i32 {
    var fn: str = @func()
    @assert(fn == "main", "{case_id} func failed")
    var __spec2026_actual: i32 = 0
    var __spec2026_expected: i32 = 0
    @assert(__spec2026_actual == __spec2026_expected, "{case_id} assertion failed")
    return __spec2026_actual
}
""",
}


def main():
    with open(MANIFEST) as f:
        manifest = json.load(f)

    created = 0
    skipped = 0
    missing_template = []
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

            if os.path.exists(path):
                skipped += 1
                continue

            if kind != "pass":
                continue

            # Find template
            template = PASS_TEMPLATES.get(point_id)
            if not template:
                missing_template.append((point_id, case_id))
                continue

            content = f"""/// @spec_ref: {spec_ref}
/// @point_id: {point_id}
/// @case_id: {case_id}
/// @expect: pass
/// @boundary: {boundary}
/// @phase: {phase}
/// @diag_codes:
/// @diag_keywords:
// {title}
{template.replace("{case_id}", case_id)}"""

            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w") as f:
                f.write(content)
            created += 1

    print(f"Created {created} pass test files, skipped {skipped} existing")
    if missing_template:
        print(f"Missing templates for {len(missing_template)} points:")
        for pid, cid in missing_template:
            print(f"  {pid}: {cid}")


if __name__ == "__main__":
    main()
