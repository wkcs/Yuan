/// \file ParseExprOperatorPropertyTest.cpp
/// \brief 表达式解析运算符优先级属性测试。

#include <gtest/gtest.h>
#include "yuan/Parser/Parser.h"
#include "yuan/Lexer/Lexer.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Expr.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/TextDiagnosticPrinter.h"
#include <sstream>
#include <random>
#include <vector>

using namespace yuan;

/// \brief 运算符优先级属性测试基类
class ParseExprOperatorPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        SM = std::make_unique<SourceManager>();
        
        // 创建诊断引擎和输出流
        DiagStream = std::make_unique<std::ostringstream>();
        auto printer = std::make_unique<TextDiagnosticPrinter>(*DiagStream, *SM, false);
        Diag = std::make_unique<DiagnosticEngine>(*SM);
        Diag->setConsumer(std::move(printer));
        
        Ctx = std::make_unique<ASTContext>(*SM);
        
        // 初始化随机数生成器
        rng.seed(std::random_device{}());
    }
    
    /// \brief 解析表达式
    /// \param source 源代码
    /// \return 解析结果
    Expr* parseExpr(const std::string& source) {
        auto fileID = SM->createBuffer(source, "<test>");
        Lexer lexer(*SM, *Diag, fileID);
        Parser parser(lexer, *Diag, *Ctx);
        
        auto result = parser.parseExpr();
        if (result.isError()) {
            return nullptr;
        }
        return result.get();
    }
    
    /// \brief 检查是否有错误
    bool hasError() const {
        return Diag->hasErrors();
    }
    
    /// \brief 生成随机整数字面量
    std::string generateRandomInt() {
        std::uniform_int_distribution<int> dist(1, 100);
        return std::to_string(dist(rng));
    }
    
    /// \brief 生成随机标识符
    std::string generateRandomIdentifier() {
        static const std::vector<std::string> identifiers = {
            "a", "b", "c", "x", "y", "z", "foo", "bar", "value"
        };
        std::uniform_int_distribution<size_t> dist(0, identifiers.size() - 1);
        return identifiers[dist(rng)];
    }
    
    std::unique_ptr<SourceManager> SM;
    std::unique_ptr<DiagnosticEngine> Diag;
    std::unique_ptr<ASTContext> Ctx;
    std::unique_ptr<std::ostringstream> DiagStream;
    std::mt19937 rng;
};

/// \brief Property 9: Parser 运算符优先级
/// \brief **Validates: Requirements 3.8**
///
/// 验证运算符优先级的正确性：
/// - 乘法优先级高于加法
/// - 比较运算符优先级低于算术运算符
/// - 逻辑运算符优先级最低
TEST_F(ParseExprOperatorPropertyTest, OperatorPrecedenceProperty) {
    // **Feature: yuan-compiler, Property 9: Parser 运算符优先级**
    
    // 运行多次测试以验证属性
    for (int i = 0; i < 100; ++i) {
        // 生成随机操作数
        std::string a = generateRandomInt();
        std::string b = generateRandomInt();
        std::string c = generateRandomInt();
        
        // 测试乘法优先级高于加法：a + b * c 应该解析为 a + (b * c)
        std::string expr1 = a + " + " + b + " * " + c;
        auto* parsed1 = parseExpr(expr1);
        ASSERT_NE(parsed1, nullptr) << "Failed to parse: " << expr1;
        EXPECT_FALSE(hasError()) << "Parse error for: " << expr1;
        
        // 验证结构：应该是 BinaryExpr(+, a, BinaryExpr(*, b, c))
        auto* addExpr = dynamic_cast<BinaryExpr*>(parsed1);
        ASSERT_NE(addExpr, nullptr) << "Expected BinaryExpr for: " << expr1;
        EXPECT_EQ(addExpr->getOp(), BinaryExpr::Op::Add) << "Expected Add operator for: " << expr1;
        
        // 右操作数应该是乘法表达式
        auto* mulExpr = dynamic_cast<BinaryExpr*>(addExpr->getRHS());
        ASSERT_NE(mulExpr, nullptr) << "Expected multiplication on RHS for: " << expr1;
        EXPECT_EQ(mulExpr->getOp(), BinaryExpr::Op::Mul) << "Expected Mul operator for: " << expr1;
        
        // 测试除法优先级高于减法：a - b / c 应该解析为 a - (b / c)
        std::string expr2 = a + " - " + b + " / " + c;
        auto* parsed2 = parseExpr(expr2);
        ASSERT_NE(parsed2, nullptr) << "Failed to parse: " << expr2;
        EXPECT_FALSE(hasError()) << "Parse error for: " << expr2;
        
        // 验证结构：应该是 BinaryExpr(-, a, BinaryExpr(/, b, c))
        auto* subExpr = dynamic_cast<BinaryExpr*>(parsed2);
        ASSERT_NE(subExpr, nullptr) << "Expected BinaryExpr for: " << expr2;
        EXPECT_EQ(subExpr->getOp(), BinaryExpr::Op::Sub) << "Expected Sub operator for: " << expr2;
        
        // 右操作数应该是除法表达式
        auto* divExpr = dynamic_cast<BinaryExpr*>(subExpr->getRHS());
        ASSERT_NE(divExpr, nullptr) << "Expected division on RHS for: " << expr2;
        EXPECT_EQ(divExpr->getOp(), BinaryExpr::Op::Div) << "Expected Div operator for: " << expr2;
    }
}

/// \brief 测试比较运算符优先级
TEST_F(ParseExprOperatorPropertyTest, ComparisonPrecedenceProperty) {
    // **Feature: yuan-compiler, Property 9: Parser 运算符优先级**
    
    // 运行多次测试
    for (int i = 0; i < 50; ++i) {
        std::string a = generateRandomInt();
        std::string b = generateRandomInt();
        std::string c = generateRandomInt();
        
        // 测试比较运算符优先级低于算术运算符：a + b == c 应该解析为 (a + b) == c
        std::string expr = a + " + " + b + " == " + c;
        auto* parsed = parseExpr(expr);
        ASSERT_NE(parsed, nullptr) << "Failed to parse: " << expr;
        EXPECT_FALSE(hasError()) << "Parse error for: " << expr;
        
        // 验证结构：应该是 BinaryExpr(==, BinaryExpr(+, a, b), c)
        auto* eqExpr = dynamic_cast<BinaryExpr*>(parsed);
        ASSERT_NE(eqExpr, nullptr) << "Expected BinaryExpr for: " << expr;
        EXPECT_EQ(eqExpr->getOp(), BinaryExpr::Op::Eq) << "Expected Eq operator for: " << expr;
        
        // 左操作数应该是加法表达式
        auto* addExpr = dynamic_cast<BinaryExpr*>(eqExpr->getLHS());
        ASSERT_NE(addExpr, nullptr) << "Expected addition on LHS for: " << expr;
        EXPECT_EQ(addExpr->getOp(), BinaryExpr::Op::Add) << "Expected Add operator for: " << expr;
    }
}

/// \brief 测试逻辑运算符优先级
TEST_F(ParseExprOperatorPropertyTest, LogicalPrecedenceProperty) {
    // **Feature: yuan-compiler, Property 9: Parser 运算符优先级**
    
    // 运行多次测试
    for (int i = 0; i < 50; ++i) {
        std::string a = generateRandomInt();
        std::string b = generateRandomInt();
        std::string c = generateRandomInt();
        std::string d = generateRandomInt();
        
        // 测试逻辑与优先级高于逻辑或：a == b || c == d 应该解析为 (a == b) || (c == d)
        // 但是 a == b && c == d 应该解析为 (a == b) && (c == d)
        std::string expr = a + " == " + b + " && " + c + " == " + d;
        auto* parsed = parseExpr(expr);
        ASSERT_NE(parsed, nullptr) << "Failed to parse: " << expr;
        EXPECT_FALSE(hasError()) << "Parse error for: " << expr;
        
        // 验证结构：应该是 BinaryExpr(&&, BinaryExpr(==, a, b), BinaryExpr(==, c, d))
        auto* andExpr = dynamic_cast<BinaryExpr*>(parsed);
        ASSERT_NE(andExpr, nullptr) << "Expected BinaryExpr for: " << expr;
        EXPECT_EQ(andExpr->getOp(), BinaryExpr::Op::And) << "Expected And operator for: " << expr;
        
        // 左右操作数都应该是比较表达式
        auto* leftEq = dynamic_cast<BinaryExpr*>(andExpr->getLHS());
        auto* rightEq = dynamic_cast<BinaryExpr*>(andExpr->getRHS());
        ASSERT_NE(leftEq, nullptr) << "Expected comparison on LHS for: " << expr;
        ASSERT_NE(rightEq, nullptr) << "Expected comparison on RHS for: " << expr;
        EXPECT_EQ(leftEq->getOp(), BinaryExpr::Op::Eq) << "Expected Eq operator on LHS for: " << expr;
        EXPECT_EQ(rightEq->getOp(), BinaryExpr::Op::Eq) << "Expected Eq operator on RHS for: " << expr;
    }
}

/// \brief 测试左结合性
TEST_F(ParseExprOperatorPropertyTest, LeftAssociativityProperty) {
    // **Feature: yuan-compiler, Property 9: Parser 运算符优先级**
    
    // 运行多次测试
    for (int i = 0; i < 50; ++i) {
        std::string a = generateRandomInt();
        std::string b = generateRandomInt();
        std::string c = generateRandomInt();
        
        // 测试减法左结合：a - b - c 应该解析为 (a - b) - c
        std::string expr = a + " - " + b + " - " + c;
        auto* parsed = parseExpr(expr);
        ASSERT_NE(parsed, nullptr) << "Failed to parse: " << expr;
        EXPECT_FALSE(hasError()) << "Parse error for: " << expr;
        
        // 验证结构：应该是 BinaryExpr(-, BinaryExpr(-, a, b), c)
        auto* outerSub = dynamic_cast<BinaryExpr*>(parsed);
        ASSERT_NE(outerSub, nullptr) << "Expected BinaryExpr for: " << expr;
        EXPECT_EQ(outerSub->getOp(), BinaryExpr::Op::Sub) << "Expected Sub operator for: " << expr;
        
        // 左操作数应该是减法表达式
        auto* innerSub = dynamic_cast<BinaryExpr*>(outerSub->getLHS());
        ASSERT_NE(innerSub, nullptr) << "Expected subtraction on LHS for: " << expr;
        EXPECT_EQ(innerSub->getOp(), BinaryExpr::Op::Sub) << "Expected Sub operator on LHS for: " << expr;
    }
}