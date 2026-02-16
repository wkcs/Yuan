#!/usr/bin/env python3
"""
Yuan 编译器语法分析测试脚本

运行所有 tests/yuan/parser/ 目录下的测试用例，验证语法分析器的正确性。
"""

import os
import sys
import subprocess
import glob
from pathlib import Path

def run_parser_test(yuanc_path, test_file):
    """运行单个语法分析测试"""
    try:
        # 运行 yuanc --emit=ast
        result = subprocess.run(
            [yuanc_path, '--emit=ast', test_file],
            capture_output=True,
            text=True,
            timeout=30
        )
        
        return {
            'file': test_file,
            'returncode': result.returncode,
            'stdout': result.stdout,
            'stderr': result.stderr
        }
    except subprocess.TimeoutExpired:
        return {
            'file': test_file,
            'returncode': -1,
            'stdout': '',
            'stderr': 'Timeout'
        }
    except Exception as e:
        return {
            'file': test_file,
            'returncode': -2,
            'stdout': '',
            'stderr': str(e)
        }

def main():
    # 获取项目根目录
    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent
    
    # yuanc 可执行文件路径
    yuanc_path = project_root / 'build' / 'tools' / 'yuanc' / 'yuanc'
    
    if not yuanc_path.exists():
        print(f"错误：找不到 yuanc 可执行文件: {yuanc_path}")
        print("请先编译项目：cd build && make")
        return 1
    
    # 查找所有测试文件
    test_dir = project_root / 'tests' / 'yuan' / 'parser'
    test_files = list(test_dir.glob('**/*.yu'))
    
    if not test_files:
        print(f"错误：在 {test_dir} 中找不到测试文件")
        return 1
    
    print(f"找到 {len(test_files)} 个测试文件")
    print(f"使用编译器: {yuanc_path}")
    print("=" * 60)
    
    # 运行测试
    passed = 0
    failed = 0
    error_files = []
    
    for test_file in sorted(test_files):
        relative_path = test_file.relative_to(project_root)
        print(f"测试: {relative_path}")
        
        result = run_parser_test(str(yuanc_path), str(test_file))
        
        if result['returncode'] == 0:
            print("  ✅ 通过")
            passed += 1
        else:
            print(f"  ❌ 失败 (退出码: {result['returncode']})")
            if result['stderr']:
                # 只显示前几行错误信息
                error_lines = result['stderr'].strip().split('\n')[:3]
                for line in error_lines:
                    print(f"     {line}")
                if len(result['stderr'].strip().split('\n')) > 3:
                    print("     ...")
            failed += 1
            error_files.append(str(relative_path))
        
        print()
    
    # 输出总结
    print("=" * 60)
    print(f"测试完成: {passed} 通过, {failed} 失败")
    
    if error_files:
        print("\n失败的测试文件:")
        for file in error_files:
            print(f"  - {file}")
    
    return 0 if failed == 0 else 1

if __name__ == '__main__':
    sys.exit(main())