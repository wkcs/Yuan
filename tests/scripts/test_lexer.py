#!/usr/bin/env python3
"""
Yuan 词法分析器测试脚本

该脚本用于批量测试 tests/yuan/lexer/ 目录下的所有测试用例，
验证词法分析器的正确性。
"""

import os
import sys
import subprocess
import glob
from pathlib import Path

def find_yuanc_executable():
    """查找 yuanc 可执行文件"""
    # 尝试几个可能的位置
    possible_paths = [
        "build/tools/yuanc/yuanc",
        "build/Debug/tools/yuanc/yuanc",
        "build/Release/tools/yuanc/yuanc",
        "tools/yuanc/yuanc"
    ]
    
    for path in possible_paths:
        if os.path.exists(path) and os.access(path, os.X_OK):
            return path
    
    # 尝试在 PATH 中查找
    try:
        result = subprocess.run(["which", "yuanc"], capture_output=True, text=True)
        if result.returncode == 0:
            return result.stdout.strip()
    except:
        pass
    
    return None

def run_lexer_test(yuanc_path, test_file, output_dir):
    """运行单个词法分析测试"""
    print(f"测试文件: {test_file}")
    
    # 生成输出文件名
    test_name = Path(test_file).stem
    output_file = os.path.join(output_dir, f"{test_name}.tokens")
    
    # 运行词法分析
    cmd = [yuanc_path, "-dump-tokens", "-o", output_file, test_file]
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        
        if result.returncode == 0:
            print(f"  ✅ 成功: {test_file}")
            return True, None
        else:
            print(f"  ❌ 失败: {test_file}")
            print(f"     错误输出: {result.stderr}")
            return False, result.stderr
            
    except subprocess.TimeoutExpired:
        print(f"  ⏰ 超时: {test_file}")
        return False, "测试超时"
    except Exception as e:
        print(f"  💥 异常: {test_file} - {str(e)}")
        return False, str(e)

def run_error_test(yuanc_path, test_file):
    """运行错误情况测试（应该失败）"""
    print(f"错误测试文件: {test_file}")
    
    # 运行词法分析，期望失败
    cmd = [yuanc_path, "-dump-tokens", test_file]
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        
        if result.returncode != 0:
            print(f"  ✅ 正确失败: {test_file}")
            # 显示错误信息的前几行，帮助验证错误类型
            if result.stderr:
                error_lines = result.stderr.strip().split('\n')
                for line in error_lines:
                    if 'error[' in line:
                        print(f"     错误: {line.strip()}")
                        break
            return True, None
        else:
            print(f"  ❌ 意外成功: {test_file} (应该失败)")
            return False, "测试应该失败但成功了"
            
    except subprocess.TimeoutExpired:
        print(f"  ⏰ 超时: {test_file}")
        return False, "测试超时"
    except Exception as e:
        print(f"  💥 异常: {test_file} - {str(e)}")
        return False, str(e)

def main():
    """主函数"""
    print("Yuan 词法分析器测试脚本")
    print("=" * 50)
    
    # 查找 yuanc 可执行文件
    yuanc_path = find_yuanc_executable()
    if not yuanc_path:
        print("❌ 错误：找不到 yuanc 可执行文件")
        print("请确保已经构建了项目，或者 yuanc 在 PATH 中")
        return 1
    
    print(f"使用编译器: {yuanc_path}")
    print()
    
    # 创建输出目录
    output_dir = "tests/output/lexer"
    os.makedirs(output_dir, exist_ok=True)
    
    # 查找所有测试文件
    test_dir = "tests/yuan/lexer"
    if not os.path.exists(test_dir):
        print(f"❌ 错误：测试目录不存在: {test_dir}")
        return 1
    
    # 正常测试文件
    normal_tests = []
    for pattern in ["*.yu"]:
        normal_tests.extend(glob.glob(os.path.join(test_dir, pattern)))
    
    # 排除错误目录中的文件
    normal_tests = [f for f in normal_tests if "/errors/" not in f]
    
    # 错误测试文件
    error_tests = glob.glob(os.path.join(test_dir, "errors", "*.yu"))
    
    print(f"找到 {len(normal_tests)} 个正常测试文件")
    print(f"找到 {len(error_tests)} 个错误测试文件")
    print()
    
    # 运行正常测试
    print("运行正常测试...")
    print("-" * 30)
    
    normal_passed = 0
    normal_failed = 0
    failed_tests = []
    
    for test_file in sorted(normal_tests):
        success, error = run_lexer_test(yuanc_path, test_file, output_dir)
        if success:
            normal_passed += 1
        else:
            normal_failed += 1
            failed_tests.append((test_file, error))
    
    print()
    
    # 运行错误测试
    print("运行错误测试...")
    print("-" * 30)
    
    error_passed = 0
    error_failed = 0
    
    for test_file in sorted(error_tests):
        success, error = run_error_test(yuanc_path, test_file)
        if success:
            error_passed += 1
        else:
            error_failed += 1
            failed_tests.append((test_file, error))
    
    print()
    
    # 输出测试结果
    print("测试结果汇总")
    print("=" * 50)
    print(f"正常测试: {normal_passed} 通过, {normal_failed} 失败")
    print(f"错误测试: {error_passed} 通过, {error_failed} 失败")
    print(f"总计: {normal_passed + error_passed} 通过, {normal_failed + error_failed} 失败")
    
    if failed_tests:
        print()
        print("失败的测试:")
        for test_file, error in failed_tests:
            print(f"  - {test_file}: {error}")
    
    print()
    print(f"Token 输出文件保存在: {output_dir}")
    
    # 返回适当的退出码
    return 0 if (normal_failed + error_failed) == 0 else 1

if __name__ == "__main__":
    sys.exit(main())