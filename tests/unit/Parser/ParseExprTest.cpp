/// \file ParseExprTest.cpp
/// \brief 表达式解析单元测试。

#include <gtest/gtest.h>
#include "yuan/Parser/Parser.h"
#include "yuan/Lexer/Lexer.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Expr.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/TextDiagnosticPrinter.h"
#include <sstream>

using namespace yuan;

/// \brief 表达式解析测试基类
class ParseExprTest : public ::testing::Test {
protected:
    void SetUp() override {
        SM = std::make_unique<SourceManager>();
        
        // 创建诊断引擎和输出流（作为成员变量保持生命周期）
        DiagStream = std::make_unique<std::ostringstream>();
        auto printer = std::make_unique<TextDiagnosticPrinter>(*DiagStream, *SM, false);
        Diag = std::make_unique<DiagnosticEngine>(*SM);
        Diag->setConsumer(std::move(printer));
        
        Ctx = std::make_unique<ASTContext>(*SM);
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
    
    std::unique_ptr<SourceManager> SM;
    std::unique_ptr<DiagnosticEngine> Diag;
    std::unique_ptr<ASTContext> Ctx;
    std::unique_ptr<std::ostringstream> DiagStream;  // 保持诊断输出流的生命周期
};

// ============================================================================
// 字面量表达式测试
// ============================================================================

TEST_F(ParseExprTest, IntegerLiterals) {
    // 十进制整数
    auto* expr1 = parseExpr("42");
    ASSERT_NE(expr1, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* intLit1 = dynamic_cast<IntegerLiteralExpr*>(expr1);
    ASSERT_NE(intLit1, nullptr);
    EXPECT_EQ(intLit1->getValue(), 42u);
    
    // 十六进制整数
    auto* expr2 = parseExpr("0xFF");
    ASSERT_NE(expr2, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* intLit2 = dynamic_cast<IntegerLiteralExpr*>(expr2);
    ASSERT_NE(intLit2, nullptr);
    EXPECT_EQ(intLit2->getValue(), 255u);
}

TEST_F(ParseExprTest, FloatLiterals) {
    auto* expr = parseExpr("3.14");
    ASSERT_NE(expr, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* floatLit = dynamic_cast<FloatLiteralExpr*>(expr);
    ASSERT_NE(floatLit, nullptr);
    EXPECT_DOUBLE_EQ(floatLit->getValue(), 3.14);
}

TEST_F(ParseExprTest, BoolLiterals) {
    // true 字面量
    auto* expr1 = parseExpr("true");
    ASSERT_NE(expr1, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* boolLit1 = dynamic_cast<BoolLiteralExpr*>(expr1);
    ASSERT_NE(boolLit1, nullptr);
    EXPECT_TRUE(boolLit1->getValue());
    
    // false 字面量
    auto* expr2 = parseExpr("false");
    ASSERT_NE(expr2, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* boolLit2 = dynamic_cast<BoolLiteralExpr*>(expr2);
    ASSERT_NE(boolLit2, nullptr);
    EXPECT_FALSE(boolLit2->getValue());
}

TEST_F(ParseExprTest, StringLiterals) {
    auto* expr = parseExpr("\"hello world\"");
    ASSERT_NE(expr, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* strLit = dynamic_cast<StringLiteralExpr*>(expr);
    ASSERT_NE(strLit, nullptr);
    EXPECT_EQ(strLit->getValue(), "hello world");
    EXPECT_EQ(strLit->getStringKind(), StringLiteralExpr::StringKind::Normal);
}

TEST_F(ParseExprTest, NoneLiteral) {
    auto* expr = parseExpr("None");
    ASSERT_NE(expr, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* noneLit = dynamic_cast<NoneLiteralExpr*>(expr);
    ASSERT_NE(noneLit, nullptr);
}

// ============================================================================
// 标识符表达式测试
// ============================================================================

TEST_F(ParseExprTest, Identifier) {
    auto* expr = parseExpr("variable");
    ASSERT_NE(expr, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* idExpr = dynamic_cast<IdentifierExpr*>(expr);
    ASSERT_NE(idExpr, nullptr);
    EXPECT_EQ(idExpr->getName(), "variable");
}

// ============================================================================
// 二元表达式测试
// ============================================================================

TEST_F(ParseExprTest, BinaryExpressions) {
    // 加法
    auto* expr1 = parseExpr("1 + 2");
    ASSERT_NE(expr1, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* binExpr1 = dynamic_cast<BinaryExpr*>(expr1);
    ASSERT_NE(binExpr1, nullptr);
    EXPECT_EQ(binExpr1->getOp(), BinaryExpr::Op::Add);
    
    // 乘法（优先级测试）
    auto* expr2 = parseExpr("1 + 2 * 3");
    ASSERT_NE(expr2, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* binExpr2 = dynamic_cast<BinaryExpr*>(expr2);
    ASSERT_NE(binExpr2, nullptr);
    EXPECT_EQ(binExpr2->getOp(), BinaryExpr::Op::Add);
    
    // 右操作数应该是乘法表达式
    auto* rhsExpr = dynamic_cast<BinaryExpr*>(binExpr2->getRHS());
    ASSERT_NE(rhsExpr, nullptr);
    EXPECT_EQ(rhsExpr->getOp(), BinaryExpr::Op::Mul);
}

TEST_F(ParseExprTest, ComparisonExpressions) {
    auto* expr = parseExpr("x == y");
    ASSERT_NE(expr, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* binExpr = dynamic_cast<BinaryExpr*>(expr);
    ASSERT_NE(binExpr, nullptr);
    EXPECT_EQ(binExpr->getOp(), BinaryExpr::Op::Eq);
}

// ============================================================================
// 一元表达式测试
// ============================================================================

TEST_F(ParseExprTest, UnaryExpressions) {
    // 取负
    auto* expr1 = parseExpr("-x");
    ASSERT_NE(expr1, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* unaryExpr1 = dynamic_cast<UnaryExpr*>(expr1);
    ASSERT_NE(unaryExpr1, nullptr);
    EXPECT_EQ(unaryExpr1->getOp(), UnaryExpr::Op::Neg);
    
    // 逻辑非
    auto* expr2 = parseExpr("!flag");
    ASSERT_NE(expr2, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* unaryExpr2 = dynamic_cast<UnaryExpr*>(expr2);
    ASSERT_NE(unaryExpr2, nullptr);
    EXPECT_EQ(unaryExpr2->getOp(), UnaryExpr::Op::Not);
}

TEST_F(ParseExprTest, AwaitExpression) {
    auto* expr = parseExpr("await fetch()");
    ASSERT_NE(expr, nullptr);
    EXPECT_FALSE(hasError());

    auto* awaitExpr = dynamic_cast<AwaitExpr*>(expr);
    ASSERT_NE(awaitExpr, nullptr);

    auto* callExpr = dynamic_cast<CallExpr*>(awaitExpr->getInner());
    ASSERT_NE(callExpr, nullptr);
    EXPECT_EQ(callExpr->getArgCount(), 0u);
}

TEST_F(ParseExprTest, AwaitErrorPropagateExpression) {
    auto* expr = parseExpr("await fetch()!");
    ASSERT_NE(expr, nullptr);
    EXPECT_FALSE(hasError());

    auto* propagateExpr = dynamic_cast<ErrorPropagateExpr*>(expr);
    ASSERT_NE(propagateExpr, nullptr);

    auto* awaitExpr = dynamic_cast<AwaitExpr*>(propagateExpr->getInner());
    ASSERT_NE(awaitExpr, nullptr);

    auto* callExpr = dynamic_cast<CallExpr*>(awaitExpr->getInner());
    ASSERT_NE(callExpr, nullptr);
    EXPECT_EQ(callExpr->getArgCount(), 0u);
}

TEST_F(ParseExprTest, AwaitErrorHandleExpression) {
    auto* expr = parseExpr("await fetch()! -> err { return 1 }");
    ASSERT_NE(expr, nullptr);
    EXPECT_FALSE(hasError());

    auto* handleExpr = dynamic_cast<ErrorHandleExpr*>(expr);
    ASSERT_NE(handleExpr, nullptr);
    EXPECT_EQ(handleExpr->getErrorVar(), "err");

    auto* awaitExpr = dynamic_cast<AwaitExpr*>(handleExpr->getInner());
    ASSERT_NE(awaitExpr, nullptr);

    auto* callExpr = dynamic_cast<CallExpr*>(awaitExpr->getInner());
    ASSERT_NE(callExpr, nullptr);
    EXPECT_EQ(callExpr->getArgCount(), 0u);
}

// ============================================================================
// 函数调用表达式测试
// ============================================================================

TEST_F(ParseExprTest, CallExpressions) {
    // 无参数调用
    auto* expr1 = parseExpr("foo()");
    ASSERT_NE(expr1, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* callExpr1 = dynamic_cast<CallExpr*>(expr1);
    ASSERT_NE(callExpr1, nullptr);
    EXPECT_EQ(callExpr1->getArgCount(), 0u);
    
    // 有参数调用
    auto* expr2 = parseExpr("add(1, 2)");
    ASSERT_NE(expr2, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* callExpr2 = dynamic_cast<CallExpr*>(expr2);
    ASSERT_NE(callExpr2, nullptr);
    EXPECT_EQ(callExpr2->getArgCount(), 2u);
}

// ============================================================================
// 成员访问表达式测试
// ============================================================================

TEST_F(ParseExprTest, MemberExpressions) {
    auto* expr = parseExpr("obj.field");
    ASSERT_NE(expr, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* memberExpr = dynamic_cast<MemberExpr*>(expr);
    ASSERT_NE(memberExpr, nullptr);
    EXPECT_EQ(memberExpr->getMember(), "field");
}

// ============================================================================
// 索引表达式测试
// ============================================================================

TEST_F(ParseExprTest, IndexExpressions) {
    auto* expr = parseExpr("arr[0]");
    ASSERT_NE(expr, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* indexExpr = dynamic_cast<IndexExpr*>(expr);
    ASSERT_NE(indexExpr, nullptr);
}

// ============================================================================
// 数组表达式测试
// ============================================================================

TEST_F(ParseExprTest, ArrayExpressions) {
    // 空数组
    auto* expr1 = parseExpr("[]");
    ASSERT_NE(expr1, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* arrayExpr1 = dynamic_cast<ArrayExpr*>(expr1);
    ASSERT_NE(arrayExpr1, nullptr);
    EXPECT_EQ(arrayExpr1->getElements().size(), 0u);
    
    // 有元素的数组
    auto* expr2 = parseExpr("[1, 2, 3]");
    ASSERT_NE(expr2, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* arrayExpr2 = dynamic_cast<ArrayExpr*>(expr2);
    ASSERT_NE(arrayExpr2, nullptr);
    EXPECT_EQ(arrayExpr2->getElements().size(), 3u);
}

// ============================================================================
// 元组表达式测试
// ============================================================================

TEST_F(ParseExprTest, TupleExpressions) {
    // 空元组
    auto* expr1 = parseExpr("()");
    ASSERT_NE(expr1, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* tupleExpr1 = dynamic_cast<TupleExpr*>(expr1);
    ASSERT_NE(tupleExpr1, nullptr);
    EXPECT_TRUE(tupleExpr1->isEmpty());
    
    // 单元素括号表达式（不是元组）
    auto* expr2 = parseExpr("(42)");
    ASSERT_NE(expr2, nullptr);
    EXPECT_FALSE(hasError());
    
    // 应该是整数字面量，不是元组
    auto* intLit = dynamic_cast<IntegerLiteralExpr*>(expr2);
    ASSERT_NE(intLit, nullptr);
    
    // 多元素元组
    auto* expr3 = parseExpr("(1, 2, 3)");
    ASSERT_NE(expr3, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* tupleExpr3 = dynamic_cast<TupleExpr*>(expr3);
    ASSERT_NE(tupleExpr3, nullptr);
    EXPECT_EQ(tupleExpr3->getElements().size(), 3u);
}

// ============================================================================
// 范围表达式测试
// ============================================================================

TEST_F(ParseExprTest, RangeExpressions) {
    // 排他范围
    auto* expr1 = parseExpr("1..10");
    ASSERT_NE(expr1, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* rangeExpr1 = dynamic_cast<RangeExpr*>(expr1);
    ASSERT_NE(rangeExpr1, nullptr);
    EXPECT_FALSE(rangeExpr1->isInclusive());
    
    // 包含范围
    auto* expr2 = parseExpr("1..=10");
    ASSERT_NE(expr2, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* rangeExpr2 = dynamic_cast<RangeExpr*>(expr2);
    ASSERT_NE(rangeExpr2, nullptr);
    EXPECT_TRUE(rangeExpr2->isInclusive());
}

// ============================================================================
// 赋值表达式测试
// ============================================================================

TEST_F(ParseExprTest, AssignExpressions) {
    // 简单赋值
    auto* expr1 = parseExpr("x = 42");
    ASSERT_NE(expr1, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* assignExpr1 = dynamic_cast<AssignExpr*>(expr1);
    ASSERT_NE(assignExpr1, nullptr);
    EXPECT_EQ(assignExpr1->getOp(), AssignExpr::Op::Assign);
    
    // 复合赋值
    auto* expr2 = parseExpr("x += 1");
    ASSERT_NE(expr2, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* assignExpr2 = dynamic_cast<AssignExpr*>(expr2);
    ASSERT_NE(assignExpr2, nullptr);
    EXPECT_EQ(assignExpr2->getOp(), AssignExpr::Op::AddAssign);
    EXPECT_TRUE(assignExpr2->isCompound());
}

// ============================================================================
// 错误处理表达式测试
// ============================================================================

TEST_F(ParseExprTest, ErrorPropagateExpressions) {
    // 先测试最简单的标识符
    auto* identifier = parseExpr("myFunc");
    if (hasError()) {
        std::cerr << "Parse error occurred for 'myFunc'" << std::endl;
    }
    ASSERT_NE(identifier, nullptr);
    EXPECT_FALSE(hasError());
    
    // 再测试简单的函数调用
    auto* simpleCall = parseExpr("myFunc()");
    if (hasError()) {
        std::cerr << "Parse error occurred for 'myFunc()'" << std::endl;
    }
    ASSERT_NE(simpleCall, nullptr);
    EXPECT_FALSE(hasError());
    
    // 测试错误传播（后缀操作符）
    auto* expr = parseExpr("myFunc()!");
    if (hasError()) {
        std::cerr << "Parse error occurred for 'myFunc()!'" << std::endl;
    }
    ASSERT_NE(expr, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* errorExpr = dynamic_cast<ErrorPropagateExpr*>(expr);
    ASSERT_NE(errorExpr, nullptr);
    
    // 内部应该是函数调用
    auto* callExpr = dynamic_cast<CallExpr*>(errorExpr->getInner());
    ASSERT_NE(callExpr, nullptr);
}

// ============================================================================
// 内置函数调用测试
// ============================================================================

TEST_F(ParseExprTest, BuiltinCallExpressions) {
    // 使用类型作为参数（@sizeof 支持类型参数）
    auto* expr1 = parseExpr("@sizeof(i32)");
    if (hasError()) {
        std::cerr << "Parse error occurred for '@sizeof(i32)': " << DiagStream->str() << std::endl;
    }
    ASSERT_NE(expr1, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* builtinExpr1 = dynamic_cast<BuiltinCallExpr*>(expr1);
    ASSERT_NE(builtinExpr1, nullptr);
    EXPECT_EQ(builtinExpr1->getBuiltinKind(), BuiltinKind::Sizeof);
    EXPECT_EQ(builtinExpr1->getArgCount(), 1u);
    
    // 验证参数是类型参数
    const auto& args1 = builtinExpr1->getArgs();
    EXPECT_TRUE(args1[0].isType());
    EXPECT_FALSE(args1[0].isExpr());
    
    // 使用表达式作为参数（@sizeof 也支持表达式参数）
    auto* expr2 = parseExpr("@sizeof(42)");
    if (hasError()) {
        std::cerr << "Parse error occurred for '@sizeof(42)': " << DiagStream->str() << std::endl;
    }
    ASSERT_NE(expr2, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* builtinExpr2 = dynamic_cast<BuiltinCallExpr*>(expr2);
    ASSERT_NE(builtinExpr2, nullptr);
    EXPECT_EQ(builtinExpr2->getBuiltinKind(), BuiltinKind::Sizeof);
    EXPECT_EQ(builtinExpr2->getArgCount(), 1u);
    
    // 验证参数是表达式参数
    const auto& args2 = builtinExpr2->getArgs();
    EXPECT_TRUE(args2[0].isExpr());
    EXPECT_FALSE(args2[0].isType());
    
    // 测试其他内置函数（只支持表达式参数）
    auto* expr3 = parseExpr("@panic(\"error\")");
    if (hasError()) {
        std::cerr << "Parse error occurred for '@panic(\"error\")': " << DiagStream->str() << std::endl;
    }
    ASSERT_NE(expr3, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* builtinExpr3 = dynamic_cast<BuiltinCallExpr*>(expr3);
    ASSERT_NE(builtinExpr3, nullptr);
    EXPECT_EQ(builtinExpr3->getBuiltinKind(), BuiltinKind::Panic);
    EXPECT_EQ(builtinExpr3->getArgCount(), 1u);
}

// ============================================================================
// 运算符优先级测试
// ============================================================================

TEST_F(ParseExprTest, OperatorPrecedence) {
    // 测试 1 + 2 * 3 应该解析为 1 + (2 * 3)
    auto* expr = parseExpr("1 + 2 * 3");
    ASSERT_NE(expr, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* addExpr = dynamic_cast<BinaryExpr*>(expr);
    ASSERT_NE(addExpr, nullptr);
    EXPECT_EQ(addExpr->getOp(), BinaryExpr::Op::Add);
    
    // 右操作数应该是乘法
    auto* mulExpr = dynamic_cast<BinaryExpr*>(addExpr->getRHS());
    ASSERT_NE(mulExpr, nullptr);
    EXPECT_EQ(mulExpr->getOp(), BinaryExpr::Op::Mul);
}

TEST_F(ParseExprTest, AssociativityTest) {
    // 测试 1 - 2 - 3 应该解析为 (1 - 2) - 3（左结合）
    auto* expr = parseExpr("1 - 2 - 3");
    ASSERT_NE(expr, nullptr);
    EXPECT_FALSE(hasError());
    
    auto* outerSub = dynamic_cast<BinaryExpr*>(expr);
    ASSERT_NE(outerSub, nullptr);
    EXPECT_EQ(outerSub->getOp(), BinaryExpr::Op::Sub);
    
    // 左操作数应该是减法
    auto* innerSub = dynamic_cast<BinaryExpr*>(outerSub->getLHS());
    ASSERT_NE(innerSub, nullptr);
    EXPECT_EQ(innerSub->getOp(), BinaryExpr::Op::Sub);
}

// ============================================================================
// 错误情况测试
// ============================================================================

TEST_F(ParseExprTest, InvalidExpressions) {
    // 无效的表达式应该返回错误
    auto* expr1 = parseExpr("++");
    EXPECT_EQ(expr1, nullptr);
    EXPECT_TRUE(hasError());
}
