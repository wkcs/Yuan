/// \file ParseStmtTest.cpp
/// \brief 语句解析单元测试。
///
/// 本文件测试 Parser 类中所有语句解析相关的功能，
/// 包括基本语句、控制流语句、跳转语句和延迟语句的解析。

#include <gtest/gtest.h>
#include "yuan/Parser/Parser.h"
#include "yuan/Lexer/Lexer.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/TextDiagnosticPrinter.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Stmt.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Pattern.h"
#include <sstream>

using namespace yuan;

/// \brief 语句解析测试基类
class ParseStmtTest : public ::testing::Test {
protected:
    void SetUp() override {
        SM = std::make_unique<SourceManager>();
        
        // 创建诊断输出流
        DiagStream = std::make_unique<std::ostringstream>();
        
        // 创建诊断引擎和打印器
        Diag = std::make_unique<DiagnosticEngine>(*SM);
        DiagPrinter = std::make_unique<TextDiagnosticPrinter>(*DiagStream, *SM, false);
        Diag->setConsumer(std::unique_ptr<DiagnosticConsumer>(DiagPrinter.get()));
        
        // 创建 AST 上下文
        Ctx = std::make_unique<ASTContext>(*SM);
    }
    
    void TearDown() override {
        // 释放 DiagPrinter 的所有权，因为它已经被 Diag 管理
        DiagPrinter.release();
    }
    
    /// \brief 解析语句
    /// \param source 源代码
    /// \return 解析结果
    ParseResult<Stmt> parseStmt(const std::string& source) {
        auto fileID = SM->createBuffer(source, "<test>");
        Lexer lexer(*SM, *Diag, fileID);
        Parser parser(lexer, *Diag, *Ctx);
        return parser.parseStmt();
    }
    
    /// \brief 检查是否有错误
    bool hasErrors() const {
        return Diag->hasErrors();
    }
    
    /// \brief 获取错误信息
    std::string getErrors() const {
        return DiagStream->str();
    }
    
    std::unique_ptr<SourceManager> SM;
    std::unique_ptr<DiagnosticEngine> Diag;
    std::unique_ptr<TextDiagnosticPrinter> DiagPrinter;
    std::unique_ptr<std::ostringstream> DiagStream;
    std::unique_ptr<ASTContext> Ctx;
};

// ============================================================================
// 基本语句测试
// ============================================================================

TEST_F(ParseStmtTest, ParseExprStmt) {
    auto result = parseStmt("foo()");
    
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasErrors());
    
    auto* stmt = result.get();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->getKind(), ASTNode::Kind::ExprStmt);
    
    auto* exprStmt = static_cast<ExprStmt*>(stmt);
    EXPECT_NE(exprStmt->getExpr(), nullptr);
}

TEST_F(ParseStmtTest, ParseBlockStmt) {
    auto result = parseStmt("{ x = 1\ny = 2 }");
    
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasErrors());
    
    auto* stmt = result.get();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->getKind(), ASTNode::Kind::BlockStmt);
    
    auto* blockStmt = static_cast<BlockStmt*>(stmt);
    EXPECT_EQ(blockStmt->getStatementCount(), 2);
}

TEST_F(ParseStmtTest, ParseEmptyBlockStmt) {
    auto result = parseStmt("{}");
    
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasErrors());
    
    auto* stmt = result.get();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->getKind(), ASTNode::Kind::BlockStmt);
    
    auto* blockStmt = static_cast<BlockStmt*>(stmt);
    EXPECT_TRUE(blockStmt->isEmpty());
}

TEST_F(ParseStmtTest, ParseReturnStmt) {
    auto result = parseStmt("return 42");
    
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasErrors());
    
    auto* stmt = result.get();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->getKind(), ASTNode::Kind::ReturnStmt);
    
    auto* returnStmt = static_cast<ReturnStmt*>(stmt);
    EXPECT_TRUE(returnStmt->hasValue());
    EXPECT_NE(returnStmt->getValue(), nullptr);
}

TEST_F(ParseStmtTest, ParseReturnStmtNoValue) {
    auto result = parseStmt("return");
    
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasErrors());
    
    auto* stmt = result.get();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->getKind(), ASTNode::Kind::ReturnStmt);
    
    auto* returnStmt = static_cast<ReturnStmt*>(stmt);
    EXPECT_FALSE(returnStmt->hasValue());
    EXPECT_EQ(returnStmt->getValue(), nullptr);
}

// ============================================================================
// 控制流语句测试
// ============================================================================

TEST_F(ParseStmtTest, ParseIfStmt) {
    auto result = parseStmt("if x > 0 { print(x) }");
    
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasErrors());
    
    auto* stmt = result.get();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->getKind(), ASTNode::Kind::IfStmt);
    
    auto* ifStmt = static_cast<IfStmt*>(stmt);
    EXPECT_EQ(ifStmt->getBranches().size(), 1);
    EXPECT_FALSE(ifStmt->hasElse());
    EXPECT_NE(ifStmt->getCondition(), nullptr);
    EXPECT_NE(ifStmt->getThenBody(), nullptr);
}

TEST_F(ParseStmtTest, ParseIfElseStmt) {
    auto result = parseStmt("if x > 0 { print(x) } else { print(0) }");
    
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasErrors());
    
    auto* stmt = result.get();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->getKind(), ASTNode::Kind::IfStmt);
    
    auto* ifStmt = static_cast<IfStmt*>(stmt);
    EXPECT_EQ(ifStmt->getBranches().size(), 2);
    EXPECT_TRUE(ifStmt->hasElse());
    EXPECT_NE(ifStmt->getElseBody(), nullptr);
}

TEST_F(ParseStmtTest, ParseIfElifElseStmt) {
    auto result = parseStmt("if x > 0 { print(1) } elif x < 0 { print(-1) } else { print(0) }");
    
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasErrors());
    
    auto* stmt = result.get();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->getKind(), ASTNode::Kind::IfStmt);
    
    auto* ifStmt = static_cast<IfStmt*>(stmt);
    EXPECT_EQ(ifStmt->getBranches().size(), 3);
    EXPECT_TRUE(ifStmt->hasElse());
}

TEST_F(ParseStmtTest, ParseWhileStmt) {
    auto result = parseStmt("while x > 0 { x = x - 1 }");
    
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasErrors());
    
    auto* stmt = result.get();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->getKind(), ASTNode::Kind::WhileStmt);
    
    auto* whileStmt = static_cast<WhileStmt*>(stmt);
    EXPECT_NE(whileStmt->getCondition(), nullptr);
    EXPECT_NE(whileStmt->getBody(), nullptr);
}

TEST_F(ParseStmtTest, ParseLoopStmt) {
    auto result = parseStmt("loop { break }");
    
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasErrors());
    
    auto* stmt = result.get();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->getKind(), ASTNode::Kind::LoopStmt);
    
    auto* loopStmt = static_cast<LoopStmt*>(stmt);
    EXPECT_NE(loopStmt->getBody(), nullptr);
}

TEST_F(ParseStmtTest, ParseForStmt) {
    auto result = parseStmt("for i in 0..10 { print(i) }");
    
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasErrors());
    
    auto* stmt = result.get();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->getKind(), ASTNode::Kind::ForStmt);
    
    auto* forStmt = static_cast<ForStmt*>(stmt);
    EXPECT_NE(forStmt->getPattern(), nullptr);
    EXPECT_NE(forStmt->getIterable(), nullptr);
    EXPECT_NE(forStmt->getBody(), nullptr);
}

TEST_F(ParseStmtTest, ParseForStmtMissingIn) {
    auto result = parseStmt("for i 0..10 { print(i) }");
    
    EXPECT_TRUE(result.isError());
    EXPECT_TRUE(hasErrors());
}

TEST_F(ParseStmtTest, ParseMatchStmt) {
    auto result = parseStmt("match x { 1 => print(\"one\"), 2 => { print(\"two\") } }");
    
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasErrors());
    
    auto* stmt = result.get();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->getKind(), ASTNode::Kind::MatchStmt);
    
    auto* matchStmt = static_cast<MatchStmt*>(stmt);
    EXPECT_NE(matchStmt->getScrutinee(), nullptr);
    EXPECT_EQ(matchStmt->getArmCount(), 2);
}

// ============================================================================
// 跳转和延迟语句测试
// ============================================================================

TEST_F(ParseStmtTest, ParseBreakStmt) {
    auto result = parseStmt("break");
    
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasErrors());
    
    auto* stmt = result.get();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->getKind(), ASTNode::Kind::BreakStmt);
    
    auto* breakStmt = static_cast<BreakStmt*>(stmt);
    EXPECT_FALSE(breakStmt->hasLabel());
}

TEST_F(ParseStmtTest, ParseContinueStmt) {
    auto result = parseStmt("continue");
    
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasErrors());
    
    auto* stmt = result.get();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->getKind(), ASTNode::Kind::ContinueStmt);
    
    auto* continueStmt = static_cast<ContinueStmt*>(stmt);
    EXPECT_FALSE(continueStmt->hasLabel());
}

TEST_F(ParseStmtTest, ParseDeferStmt) {
    auto result = parseStmt("defer { cleanup() }");
    
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasErrors());
    
    auto* stmt = result.get();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->getKind(), ASTNode::Kind::DeferStmt);
    
    auto* deferStmt = static_cast<DeferStmt*>(stmt);
    EXPECT_NE(deferStmt->getBody(), nullptr);
}

TEST_F(ParseStmtTest, ParseDeferStmtSingleStatement) {
    auto result = parseStmt("defer cleanup()");
    
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasErrors());
    
    auto* stmt = result.get();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->getKind(), ASTNode::Kind::DeferStmt);
    
    auto* deferStmt = static_cast<DeferStmt*>(stmt);
    EXPECT_NE(deferStmt->getBody(), nullptr);
}

// ============================================================================
// 错误处理测试
// ============================================================================

TEST_F(ParseStmtTest, ParseInvalidStatement) {
    auto result = parseStmt("}");
    
    EXPECT_TRUE(result.isError());
    EXPECT_TRUE(hasErrors());
}

TEST_F(ParseStmtTest, ParseUselessExpressionStatement) {
    // 测试无意义的表达式语句应该被拒绝
    auto result = parseStmt("invalid_keyword");
    
    EXPECT_TRUE(result.isError());
    EXPECT_TRUE(hasErrors());
    
    // 检查错误信息包含 "no effect"
    std::string errors = getErrors();
    EXPECT_TRUE(errors.find("no effect") != std::string::npos);
}

TEST_F(ParseStmtTest, ParseUselessLiteralStatement) {
    // 测试字面量作为语句应该被拒绝
    auto result = parseStmt("42");
    
    EXPECT_TRUE(result.isError());
    EXPECT_TRUE(hasErrors());
    
    // 检查错误信息包含 "no effect"
    std::string errors = getErrors();
    EXPECT_TRUE(errors.find("no effect") != std::string::npos);
}

TEST_F(ParseStmtTest, ParseMeaningfulExpressionStatement) {
    // 测试有意义的表达式语句应该被接受
    auto result = parseStmt("foo()");
    
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasErrors());
    
    auto* stmt = result.get();
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->getKind(), ASTNode::Kind::ExprStmt);
}

TEST_F(ParseStmtTest, ParseIncompleteIfStmt) {
    auto result = parseStmt("if x > 0");
    
    EXPECT_TRUE(result.isError());
    EXPECT_TRUE(hasErrors());
}

TEST_F(ParseStmtTest, ParseIncompleteWhileStmt) {
    auto result = parseStmt("while x > 0");
    
    EXPECT_TRUE(result.isError());
    EXPECT_TRUE(hasErrors());
}

TEST_F(ParseStmtTest, ParseIncompleteMatchStmt) {
    auto result = parseStmt("match x {");
    
    EXPECT_TRUE(result.isError());
    EXPECT_TRUE(hasErrors());
}