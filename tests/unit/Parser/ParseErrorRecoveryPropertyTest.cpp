/// \file ParseErrorRecoveryPropertyTest.cpp
/// \brief Parser 错误恢复属性测试。
///
/// 本文件测试 Parser 在遇到语法错误时的恢复能力，
/// 验证错误恢复机制能够正确同步到安全点并继续解析。

#include <gtest/gtest.h>
#include "yuan/Parser/Parser.h"
#include "yuan/Lexer/Lexer.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/TextDiagnosticPrinter.h"
#include "yuan/AST/ASTContext.h"
#include <random>
#include <sstream>

namespace yuan {

/// \brief Parser 错误恢复属性测试夹具
class ParseErrorRecoveryPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        SM = std::make_unique<SourceManager>();
        
        // 创建诊断引擎，输出到字符串流
        DiagStream = std::make_unique<std::ostringstream>();
        auto printer = std::make_unique<TextDiagnosticPrinter>(*DiagStream, *SM, false);
        Diag = std::make_unique<DiagnosticEngine>(*SM);
        Diag->setConsumer(std::move(printer));
        
        Ctx = std::make_unique<ASTContext>(*SM);
        
        // 初始化随机数生成器
        std::random_device rd;
        Gen.seed(rd());
    }
    
    /// \brief 创建包含语法错误的源码并解析
    /// \param source 源码字符串
    /// \return 解析是否成功恢复（没有崩溃）
    bool testErrorRecovery(const std::string& source) {
        try {
            // 重置诊断状态
            DiagStream->str("");
            DiagStream->clear();
            
            // 创建源码缓冲区
            auto fileID = SM->createBuffer(source, "<test>");
            
            // 创建词法分析器和语法分析器
            Lexer lexer(*SM, *Diag, fileID);
            Parser parser(lexer, *Diag, *Ctx);
            
            // 尝试解析整个编译单元
            auto decls = parser.parseCompilationUnit();
            
            // 如果没有崩溃，说明错误恢复成功
            return true;
        } catch (...) {
            // 如果抛出异常，说明错误恢复失败
            return false;
        }
    }
    
    /// \brief 生成随机的无效标识符
    std::string generateInvalidIdentifier() {
        std::uniform_int_distribution<> dis(1, 10);
        int len = dis(Gen);
        std::string result;
        
        // 生成包含无效字符的标识符
        std::uniform_int_distribution<> charDis(33, 126);  // 可打印 ASCII 字符
        for (int i = 0; i < len; ++i) {
            char c = static_cast<char>(charDis(Gen));
            // 排除有效的标识符字符
            if (!std::isalnum(c) && c != '_') {
                result += c;
            }
        }
        
        return result.empty() ? "@#$" : result;
    }
    
    /// \brief 生成随机的有效标识符
    std::string generateValidIdentifier() {
        std::uniform_int_distribution<> dis(1, 10);
        int len = dis(Gen);
        std::string result;
        
        // 第一个字符必须是字母或下划线
        std::uniform_int_distribution<> firstCharDis(0, 1);
        if (firstCharDis(Gen) == 0) {
            result += static_cast<char>('a' + (Gen() % 26));
        } else {
            result += '_';
        }
        
        // 后续字符可以是字母、数字或下划线
        std::uniform_int_distribution<> charTypeDis(0, 2);
        for (int i = 1; i < len; ++i) {
            int type = charTypeDis(Gen);
            if (type == 0) {
                result += static_cast<char>('a' + (Gen() % 26));
            } else if (type == 1) {
                result += static_cast<char>('A' + (Gen() % 26));
            } else {
                result += static_cast<char>('0' + (Gen() % 10));
            }
        }
        
        return result;
    }
    
    /// \brief 生成随机的语法错误源码
    std::string generateErrorSource() {
        std::uniform_int_distribution<> typeDis(0, 4);
        int errorType = typeDis(Gen);
        
        switch (errorType) {
            case 0: // 缺少标识符的变量声明
                return "var = 42";
            
            case 1: // 缺少等号的常量声明
                return "const x 42";
            
            case 2: // 不匹配的括号
                return "func test() { var x = (1 + 2; }";
            
            case 3: // 无效的表达式
                return "var x = + * 42";
            
            case 4: // 混合错误
                return "var " + generateInvalidIdentifier() + " = func() { return + }";
            
            default:
                return "var x = ";
        }
    }
    
protected:
    std::unique_ptr<SourceManager> SM;
    std::unique_ptr<DiagnosticEngine> Diag;
    std::unique_ptr<ASTContext> Ctx;
    std::unique_ptr<std::ostringstream> DiagStream;
    std::mt19937 Gen;
};

/// \brief 属性测试：Parser 错误恢复
/// **Feature: yuan-compiler, Property 10: Parser 错误恢复**
/// **Validates: Requirements 3.11**
///
/// 对于任何包含语法错误的源码，Parser 应该能够：
/// 1. 检测并报告错误
/// 2. 恢复到安全点继续解析
/// 3. 不会崩溃或进入无限循环
TEST_F(ParseErrorRecoveryPropertyTest, ErrorRecoveryProperty) {
    const int NUM_ITERATIONS = 100;
    int successCount = 0;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        std::string errorSource = generateErrorSource();
        
        bool recovered = testErrorRecovery(errorSource);
        if (recovered) {
            successCount++;
        } else {
            // 记录失败的测试用例
            FAIL() << "Error recovery failed for source: " << errorSource;
        }
    }
    
    // 所有测试都应该成功恢复
    EXPECT_EQ(successCount, NUM_ITERATIONS) 
        << "Error recovery failed in " << (NUM_ITERATIONS - successCount) 
        << " out of " << NUM_ITERATIONS << " cases";
}

/// \brief 测试声明级别的错误恢复
TEST_F(ParseErrorRecoveryPropertyTest, DeclarationErrorRecovery) {
    const int NUM_ITERATIONS = 50;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        // 生成包含多个声明的源码，其中一些有错误
        std::ostringstream source;
        source << "var valid1 = 42\n";
        source << "var " << generateInvalidIdentifier() << " = 123\n";  // 错误
        source << "const valid2 = \"hello\"\n";
        source << "const invalid_const\n";  // 错误：缺少初始化
        source << "func validFunc() { return 0 }\n";
        
        bool recovered = testErrorRecovery(source.str());
        EXPECT_TRUE(recovered) << "Declaration error recovery failed for iteration " << i;
        
        // 验证有错误被报告
        EXPECT_TRUE(Diag->hasErrors()) << "Expected errors to be reported";
    }
}

/// \brief 测试表达式级别的错误恢复
TEST_F(ParseErrorRecoveryPropertyTest, ExpressionErrorRecovery) {
    const int NUM_ITERATIONS = 50;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        // 生成包含表达式错误的源码
        std::ostringstream source;
        source << "func test() {\n";
        source << "  var x = 1 + 2\n";  // 正确
        source << "  var y = + * 3\n";  // 错误：无效表达式
        source << "  var z = \"valid\"\n";  // 正确
        source << "  return x + z\n";
        source << "}\n";
        
        bool recovered = testErrorRecovery(source.str());
        EXPECT_TRUE(recovered) << "Expression error recovery failed for iteration " << i;
        
        // 验证有错误被报告
        EXPECT_TRUE(Diag->hasErrors()) << "Expected errors to be reported";
    }
}

/// \brief 测试嵌套结构的错误恢复
TEST_F(ParseErrorRecoveryPropertyTest, NestedStructureErrorRecovery) {
    const int NUM_ITERATIONS = 30;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        // 生成包含嵌套结构错误的源码
        std::ostringstream source;
        source << "func outer() {\n";
        source << "  if true {\n";
        source << "    var x = (1 + 2\n";  // 错误：缺少右括号
        source << "    var y = 42\n";
        source << "  }\n";
        source << "  while false {\n";
        source << "    break\n";
        source << "  }\n";
        source << "}\n";
        
        bool recovered = testErrorRecovery(source.str());
        EXPECT_TRUE(recovered) << "Nested structure error recovery failed for iteration " << i;
    }
}

/// \brief 测试多个连续错误的恢复
TEST_F(ParseErrorRecoveryPropertyTest, MultipleConsecutiveErrors) {
    const int NUM_ITERATIONS = 30;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        // 生成包含多个连续错误的源码
        std::ostringstream source;
        source << "var = \n";           // 错误1：缺少标识符
        source << "const x\n";          // 错误2：缺少初始化
        source << "func () {}\n";       // 错误3：缺少函数名
        source << "var valid = 42\n";   // 正确的声明
        
        bool recovered = testErrorRecovery(source.str());
        EXPECT_TRUE(recovered) << "Multiple consecutive errors recovery failed for iteration " << i;
        
        // 验证报告了多个错误
        EXPECT_TRUE(Diag->hasErrors()) << "Expected multiple errors to be reported";
    }
}

} // namespace yuan