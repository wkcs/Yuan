#!/usr/bin/env python3
"""
Yuan 编译器语法分析测试脚本

此脚本用于测试 Yuan 编译器的语法分析功能，验证 tests/yuan/parser/ 目录下的所有测试用例。
"""

import os
import sys
import subprocess
import glob
from pathlib import Path

def find_yuanc_executable():
    """查找 yuanc 可执行文件"""
    # 首先尝试在构建目录中查找
    build_paths = [
        "build/tools/yuanc/yuanc",
        "build/Debug/tools/yuanc/yuanc",
        "build/Release/tools/yuanc/yuanc",
        "cmake-build-debug/tools/yuanc/yuanc",
        "cmake-build-release/tools/yuanc/yuanc"
    ]
    
    for path in build_paths:
        if os.path.exists(path):
            return path
    
    # 尝试在 PATH 中查找
    try:
        result = subprocess.run(["which", "yuanc"], capture_output=True, text=True)
        if result.returncode == 0:
            return result.stdout.strip()
    except:
        pass
    
    return None

def run_parser_test(yuanc_path, test_file):
    """运行单个语法分析测试"""
    print(f"测试文件: {test_file}")
    
    try:
        # 运行语法分析
        result = subprocess.run([
            yuanc_path, 
            "-ast-dump", 
            "--verbose",
            test_file
        ], capture_output=True, text=True, timeout=30)
        
        if result.returncode == 0:
            print(f"  ✅ 成功")
            return True
        else:
            print(f"  ❌ 失败")
            if result.stderr:
                print(f"  错误输出: {result.stderr}")
            return False
            
    except subprocess.TimeoutExpired:
        print(f"  ⏰ 超时")
        return False
    except Exception as e:
        print(f"  💥 异常: {e}")
        return False

def run_error_test(yuanc_path, test_file):
    """运行错误测试用例（应该产生错误）"""
    print(f"错误测试文件: {test_file}")
    
    try:
        # 运行语法分析，期望失败
        result = subprocess.run([
            yuanc_path, 
            "-ast-dump", 
            test_file
        ], capture_output=True, text=True, timeout=30)
        
        if result.returncode != 0:
            print(f"  ✅ 正确产生错误")
            return True
        else:
            print(f"  ❌ 应该产生错误但成功了")
            return False
            
    except subprocess.TimeoutExpired:
        print(f"  ⏰ 超时")
        return False
    except Exception as e:
        print(f"  💥 异常: {e}")
        return False

def main():
    """主函数"""
    print("Yuan 编译器语法分析测试")
    print("=" * 50)
    
    # 查找编译器可执行文件
    yuanc_path = find_yuanc_executable()
    if not yuanc_path:
        print("❌ 找不到 yuanc 可执行文件")
        print("请确保已经构建了项目，或者 yuanc 在 PATH 中")
        return 1
    
    print(f"使用编译器: {yuanc_path}")
    print()
    
    # 获取测试文件
    test_root = Path("tests/yuan/parser")
    if not test_root.exists():
        print(f"❌ 测试目录不存在: {test_root}")
        return 1
    
    # 收集所有测试文件
    test_files = []
    error_files = []
    
    for category in ["declarations", "expressions", "statements", "types", "patterns"]:
        category_path = test_root / category
        if category_path.exists():
            test_files.extend(glob.glob(str(category_path / "*.yu")))
    
    # 收集错误测试文件
    error_path = test_root / "errors"
    if error_path.exists():
        error_files.extend(glob.glob(str(error_path / "*.yu")))
    
    if not test_files and not error_files:
        print("❌ 没有找到测试文件")
        return 1
    
    print(f"找到 {len(test_files)} 个正常测试文件")
    print(f"找到 {len(error_files)} 个错误测试文件")
    print()
    
    # 运行正常测试
    success_count = 0
    total_count = len(test_files)
    
    if test_files:
        print("运行正常测试用例:")
        print("-" * 30)
        
        for test_file in sorted(test_files):
            if run_parser_test(yuanc_path, test_file):
                success_count += 1
        
        print()
        print(f"正常测试结果: {success_count}/{total_count} 通过")
        print()
    
    # 运行错误测试
    error_success_count = 0
    error_total_count = len(error_files)
    
    if error_files:
        print("运行错误测试用例:")
        print("-" * 30)
        
        for test_file in sorted(error_files):
            if run_error_test(yuanc_path, test_file):
                error_success_count += 1
        
        print()
        print(f"错误测试结果: {error_success_count}/{error_total_count} 通过")
        print()
    
    # 总结
    total_success = success_count + error_success_count
    total_tests = total_count + error_total_count
    
    print("=" * 50)
    print(f"总体结果: {total_success}/{total_tests} 通过")
    
    if total_success == total_tests:
        print("🎉 所有测试通过！")
        return 0
    else:
        print("❌ 部分测试失败")
        return 1

if __name__ == "__main__":
    sys.exit(main())