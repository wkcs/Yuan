#!/bin/bash
# Yuan 词法分析器测试运行脚本

set -e

echo "Yuan 词法分析器测试"
echo "=================="

# 检查是否已构建项目
if [ ! -f "build/tools/yuanc/yuanc" ]; then
    echo "错误：找不到 yuanc 可执行文件"
    echo "请先构建项目："
    echo "  cmake -B build -DCMAKE_BUILD_TYPE=Debug"
    echo "  cmake --build build"
    exit 1
fi

# 创建输出目录
mkdir -p tests/output/lexer

# 运行 Python 测试脚本
echo "运行词法分析器测试..."
python3 tests/scripts/test_lexer.py

echo ""
echo "测试完成！"
echo "查看详细的 token 输出："
echo "  ls tests/output/lexer/"