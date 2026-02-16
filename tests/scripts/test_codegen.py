#!/usr/bin/env python3
"""
Yuan CodeGen Tests Runner

测试 tests/yuan/codegen/ 目录下的所有代码生成测试用例。
验证编译器是否能够正确生成 LLVM IR 和目标文件。
"""

import os
import sys
import subprocess
from pathlib import Path
from typing import List, Tuple

# 颜色代码
GREEN = "\033[92m"
RED = "\033[91m"
YELLOW = "\033[93m"
BLUE = "\033[94m"
RESET = "\033[0m"


class TestResult:
    def __init__(self, name: str, passed: bool, message: str = ""):
        self.name = name
        self.passed = passed
        self.message = message


def find_yuan_files(test_dir: Path) -> List[Path]:
    """查找所有 .yu 测试文件"""
    return sorted(test_dir.rglob("*.yu"))


def test_ir_generation(yuanc: Path, test_file: Path, verbose: bool = False) -> TestResult:
    """测试 LLVM IR 生成"""
    test_name = f"{test_file.parent.name}/{test_file.name}"
    output_file = f"/tmp/yuan_test_{test_file.stem}.ll"

    try:
        cmd = [str(yuanc), "--emit=ir", str(test_file), "-o", output_file]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)

        if result.returncode != 0:
            return TestResult(test_name, False, f"IR generation failed: {result.stderr}")

        # 检查输出文件是否存在且非空
        if not os.path.exists(output_file):
            return TestResult(test_name, False, "IR file not created")

        if os.path.getsize(output_file) == 0:
            return TestResult(test_name, False, "IR file is empty")

        # 清理
        os.remove(output_file)

        return TestResult(test_name, True, "IR generated successfully")

    except subprocess.TimeoutExpired:
        return TestResult(test_name, False, "Timeout during IR generation")
    except Exception as e:
        return TestResult(test_name, False, f"Exception: {str(e)}")


def test_object_generation(yuanc: Path, test_file: Path, verbose: bool = False) -> TestResult:
    """测试目标文件生成"""
    test_name = f"{test_file.parent.name}/{test_file.name}"
    output_file = f"/tmp/yuan_test_{test_file.stem}.o"

    try:
        cmd = [str(yuanc), "--emit=obj", str(test_file), "-o", output_file]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)

        if result.returncode != 0:
            return TestResult(test_name, False, f"Object generation failed: {result.stderr}")

        # 检查输出文件是否存在且非空
        if not os.path.exists(output_file):
            return TestResult(test_name, False, "Object file not created")

        if os.path.getsize(output_file) == 0:
            return TestResult(test_name, False, "Object file is empty")

        # 清理
        os.remove(output_file)

        return TestResult(test_name, True, "Object file generated successfully")

    except subprocess.TimeoutExpired:
        return TestResult(test_name, False, "Timeout during object generation")
    except Exception as e:
        return TestResult(test_name, False, f"Exception: {str(e)}")


def print_results(results: List[TestResult], test_type: str):
    """打印测试结果"""
    passed = sum(1 for r in results if r.passed)
    failed = len(results) - passed

    print(f"\n{BLUE}=== {test_type} 测试结果 ==={RESET}")
    print(f"总计: {len(results)} | 通过: {GREEN}{passed}{RESET} | 失败: {RED}{failed}{RESET}\n")

    if failed > 0:
        print(f"{RED}失败的测试:{RESET}")
        for result in results:
            if not result.passed:
                print(f"  {RED}✗{RESET} {result.name}: {result.message}")


def main():
    # 查找项目根目录
    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent

    # 查找 yuanc 编译器
    yuanc = project_root / "build" / "tools" / "yuanc" / "yuanc"
    if not yuanc.exists():
        print(f"{RED}错误: 找不到 yuanc 编译器: {yuanc}{RESET}")
        print(f"请先运行: cmake --build {project_root / 'build'}")
        return 1

    # 查找测试目录
    test_dir = project_root / "tests" / "yuan" / "codegen"
    if not test_dir.exists():
        print(f"{RED}错误: 找不到测试目录: {test_dir}{RESET}")
        return 1

    # 查找所有测试文件
    test_files = find_yuan_files(test_dir)
    if not test_files:
        print(f"{YELLOW}警告: 在 {test_dir} 中没有找到测试文件{RESET}")
        return 0

    print(f"{BLUE}Yuan CodeGen 测试运行器{RESET}")
    print(f"测试目录: {test_dir}")
    print(f"找到 {len(test_files)} 个测试文件\n")

    # 运行 IR 生成测试
    print(f"{BLUE}运行 IR 生成测试...{RESET}")
    ir_results = []
    for test_file in test_files:
        result = test_ir_generation(yuanc, test_file)
        ir_results.append(result)
        status = f"{GREEN}✓{RESET}" if result.passed else f"{RED}✗{RESET}"
        print(f"  {status} {test_file.parent.name}/{test_file.name}")

    print_results(ir_results, "IR 生成")

    # 运行目标文件生成测试
    print(f"\n{BLUE}运行目标文件生成测试...{RESET}")
    obj_results = []
    for test_file in test_files:
        result = test_object_generation(yuanc, test_file)
        obj_results.append(result)
        status = f"{GREEN}✓{RESET}" if result.passed else f"{RED}✗{RESET}"
        print(f"  {status} {test_file.parent.name}/{test_file.name}")

    print_results(obj_results, "目标文件生成")

    # 总结
    total_tests = len(ir_results) + len(obj_results)
    total_passed = sum(1 for r in ir_results if r.passed) + sum(1 for r in obj_results if r.passed)
    total_failed = total_tests - total_passed

    print(f"\n{BLUE}=== 总结 ==={RESET}")
    print(f"总测试数: {total_tests}")
    print(f"通过: {GREEN}{total_passed}{RESET}")
    print(f"失败: {RED}{total_failed}{RESET}")

    if total_failed > 0:
        percentage = (total_passed / total_tests) * 100
        print(f"通过率: {YELLOW}{percentage:.1f}%{RESET}")
        return 1
    else:
        print(f"{GREEN}所有测试通过! ✓{RESET}")
        return 0


if __name__ == "__main__":
    sys.exit(main())
