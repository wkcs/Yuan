/// \file ParseRoundTripPropertyTest.cpp
/// \brief Parser-Printer Round-Trip 属性测试。
///
/// 本文件测试 Parser 和 ASTPrinter 的 Round-Trip 属性：
/// 对于任何有效的 AST，解析然后打印然后再解析应该产生等价的 AST。

#include <gtest/gtest.h>
#include "yuan/Parser/Parser.h"
#include "yuan/Lexer/Lexer.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/TextDiagnosticPrinter.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/ASTPrinter.h"
#include <random>
#include <sstream>

namespace yuan {

/// \brief Parser-Printer Round-Trip 属性测试夹具
class ParseRoundTripPropertyTest : public ::testing::Test {
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
    
    /// \brief 解析源码并返回 AST
    /// \param source 源码字符串
    /// \return 解析结果的声明列表
    std::vector<Decl*> parseSource(const std::string& source) {
        // 重置诊断状态
        DiagStream->str("");
        DiagStream->clear();
        
        // 创建源码缓冲区
        auto fileID = SM->createBuffer(source, "<test>");
        
        // 创建词法分析器和语法分析器
        Lexer lexer(*SM, *Diag, fileID);
        Parser parser(lexer, *Diag, *Ctx);
        
        // 解析整个编译单元
        return parser.parseCompilationUnit();
    }
    
    /// \brief 将 AST 打印为源码
    /// \param decls 声明列表
    /// \return 打印的源码字符串
    std::string printAST(const std::vector<Decl*>& decls) {
        std::ostringstream output;
        ASTPrinter printer(output);
        
        for (Decl* decl : decls) {
            printer.print(decl);
            output << "\n";
        }
        
        return output.str();
    }
    
    /// \brief 比较两个 AST 是否结构等价
    /// \param decls1 第一个 AST
    /// \param decls2 第二个 AST
    /// \return 如果结构等价返回 true
    bool compareASTs(const std::vector<Decl*>& decls1, const std::vector<Decl*>& decls2) {
        if (decls1.size() != decls2.size()) {
            return false;
        }
        
        for (size_t i = 0; i < decls1.size(); ++i) {
            if (!compareDeclNodes(decls1[i], decls2[i])) {
                return false;
            }
        }
        
        return true;
    }
    
    /// \brief 比较两个声明节点是否等价
    bool compareDeclNodes(Decl* decl1, Decl* decl2) {
        if (decl1->getKind() != decl2->getKind()) {
            return false;
        }
        
        switch (decl1->getKind()) {
            case ASTNode::Kind::VarDecl: {
                auto* var1 = static_cast<VarDecl*>(decl1);
                auto* var2 = static_cast<VarDecl*>(decl2);
                return var1->getName() == var2->getName() &&
                       var1->isMutable() == var2->isMutable();
            }
            case ASTNode::Kind::ConstDecl: {
                auto* const1 = static_cast<ConstDecl*>(decl1);
                auto* const2 = static_cast<ConstDecl*>(decl2);
                return const1->getName() == const2->getName();
            }
            case ASTNode::Kind::FuncDecl: {
                auto* func1 = static_cast<FuncDecl*>(decl1);
                auto* func2 = static_cast<FuncDecl*>(decl2);
                return func1->getName() == func2->getName() &&
                       func1->isAsync() == func2->isAsync() &&
                       func1->canError() == func2->canError();
            }
            default:
                // 对于其他类型的声明，简单比较类型
                return true;
        }
    }
    
    /// \brief 生成随机的有效标识符
    std::string generateValidIdentifier() {
        std::uniform_int_distribution<> dis(3, 8);
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
    
    /// \brief 生成随机的整数字面量
    std::string generateIntegerLiteral() {
        std::uniform_int_distribution<> dis(0, 1000);
        return std::to_string(dis(Gen));
    }
    
    /// \brief 生成随机的字符串字面量
    std::string generateStringLiteral() {
        std::uniform_int_distribution<> dis(1, 10);
        int len = dis(Gen);
        std::string content;
        
        for (int i = 0; i < len; ++i) {
            content += static_cast<char>('a' + (Gen() % 26));
        }
        
        return "\"" + content + "\"";
    }
    
    /// \brief 生成随机的变量声明
    std::string generateVarDecl() {
        std::ostringstream source;
        source << "var " << generateValidIdentifier() << " = " << generateIntegerLiteral();
        return source.str();
    }
    
    /// \brief 生成随机的常量声明
    std::string generateConstDecl() {
        std::ostringstream source;
        source << "const " << generateValidIdentifier() << " = " << generateStringLiteral();
        return source.str();
    }
    
    /// \brief 生成随机的函数声明
    std::string generateFuncDecl() {
        std::ostringstream source;
        source << "func " << generateValidIdentifier() << "() { return " << generateIntegerLiteral() << " }";
        return source.str();
    }
    
    /// \brief 生成随机的有效源码
    std::string generateValidSource() {
        std::uniform_int_distribution<> typeDis(0, 2);
        int declType = typeDis(Gen);
        
        switch (declType) {
            case 0: return generateVarDecl();
            case 1: return generateConstDecl();
            case 2: return generateFuncDecl();
            default: return generateVarDecl();
        }
    }
    
protected:
    std::unique_ptr<SourceManager> SM;
    std::unique_ptr<DiagnosticEngine> Diag;
    std::unique_ptr<ASTContext> Ctx;
    std::unique_ptr<std::ostringstream> DiagStream;
    std::mt19937 Gen;
};

/// \brief 属性测试：Parser-Printer Round-Trip
/// **Feature: yuan-compiler, Property 8: Parser-Printer Round-Trip**
/// **Validates: Requirements 3.12, 3.13**
///
/// 对于任何有效的 AST，解析然后打印然后再解析应该产生等价的 AST。
/// 即：parse(print(parse(source))) 应该与 parse(source) 等价。
TEST_F(ParseRoundTripPropertyTest, RoundTripProperty) {
    const int NUM_ITERATIONS = 100;
    int successCount = 0;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        std::string originalSource = generateValidSource();
        
        // 第一次解析
        auto originalAST = parseSource(originalSource);
        if (Diag->hasErrors() || originalAST.empty()) {
            // 跳过有语法错误的源码
            continue;
        }
        
        // 打印 AST
        std::string printedSource = printAST(originalAST);
        
        // 第二次解析（解析打印的源码）
        auto reparsedAST = parseSource(printedSource);
        if (Diag->hasErrors() || reparsedAST.empty()) {
            FAIL() << "Round-trip failed: printed source has syntax errors\n"
                   << "Original: " << originalSource << "\n"
                   << "Printed: " << printedSource;
        }
        
        // 比较两个 AST 是否等价
        if (compareASTs(originalAST, reparsedAST)) {
            successCount++;
        } else {
            FAIL() << "Round-trip failed: ASTs are not equivalent\n"
                   << "Original: " << originalSource << "\n"
                   << "Printed: " << printedSource;
        }
    }
    
    // 至少应该有一些成功的测试用例
    EXPECT_GT(successCount, NUM_ITERATIONS / 2) 
        << "Round-trip succeeded in only " << successCount 
        << " out of " << NUM_ITERATIONS << " valid cases";
}

/// \brief 测试简单声明的 Round-Trip
TEST_F(ParseRoundTripPropertyTest, SimpleDeclarationRoundTrip) {
    std::vector<std::string> testCases = {
        "var x = 42",
        "const PI = 3.14",
        "var name = \"hello\"",
        "const flag = true",
        "var result = false"
    };
    
    for (const std::string& source : testCases) {
        // 第一次解析
        auto originalAST = parseSource(source);
        ASSERT_FALSE(Diag->hasErrors()) << "Original source has syntax errors: " << source;
        ASSERT_FALSE(originalAST.empty()) << "Original AST is empty: " << source;
        
        // 打印 AST
        std::string printedSource = printAST(originalAST);
        
        // 第二次解析
        auto reparsedAST = parseSource(printedSource);
        ASSERT_FALSE(Diag->hasErrors()) << "Printed source has syntax errors\n"
                                        << "Original: " << source << "\n"
                                        << "Printed: " << printedSource;
        ASSERT_FALSE(reparsedAST.empty()) << "Reparsed AST is empty";
        
        // 比较 AST
        EXPECT_TRUE(compareASTs(originalAST, reparsedAST))
            << "ASTs are not equivalent\n"
            << "Original: " << source << "\n"
            << "Printed: " << printedSource;
    }
}

/// \brief 测试函数声明的 Round-Trip
TEST_F(ParseRoundTripPropertyTest, FunctionDeclarationRoundTrip) {
    std::vector<std::string> testCases = {
        "func test() { return 0 }",
        "func add() { return 1 + 2 }",
        "func greet() { return \"hello\" }"
    };
    
    for (const std::string& source : testCases) {
        // 第一次解析
        auto originalAST = parseSource(source);
        ASSERT_FALSE(Diag->hasErrors()) << "Original source has syntax errors: " << source;
        ASSERT_FALSE(originalAST.empty()) << "Original AST is empty: " << source;
        
        // 打印 AST
        std::string printedSource = printAST(originalAST);
        
        // 第二次解析
        auto reparsedAST = parseSource(printedSource);
        ASSERT_FALSE(Diag->hasErrors()) << "Printed source has syntax errors\n"
                                        << "Original: " << source << "\n"
                                        << "Printed: " << printedSource;
        ASSERT_FALSE(reparsedAST.empty()) << "Reparsed AST is empty";
        
        // 比较 AST
        EXPECT_TRUE(compareASTs(originalAST, reparsedAST))
            << "ASTs are not equivalent\n"
            << "Original: " << source << "\n"
            << "Printed: " << printedSource;
    }
}

/// \brief 测试多个声明的 Round-Trip
TEST_F(ParseRoundTripPropertyTest, MultipleDeclarationsRoundTrip) {
    std::string source = 
        "var x = 42\n"
        "const name = \"test\"\n"
        "func getValue() { return x }";
    
    // 第一次解析
    auto originalAST = parseSource(source);
    ASSERT_FALSE(Diag->hasErrors()) << "Original source has syntax errors";
    ASSERT_EQ(originalAST.size(), 3) << "Expected 3 declarations";
    
    // 打印 AST
    std::string printedSource = printAST(originalAST);
    
    // 第二次解析
    auto reparsedAST = parseSource(printedSource);
    ASSERT_FALSE(Diag->hasErrors()) << "Printed source has syntax errors\n"
                                    << "Printed: " << printedSource;
    ASSERT_EQ(reparsedAST.size(), 3) << "Expected 3 declarations in reparsed AST";
    
    // 比较 AST
    EXPECT_TRUE(compareASTs(originalAST, reparsedAST))
        << "ASTs are not equivalent\n"
        << "Original: " << source << "\n"
        << "Printed: " << printedSource;
}

} // namespace yuan