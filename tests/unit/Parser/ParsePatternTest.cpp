/// \file ParsePatternTest.cpp
/// \brief 模式解析单元测试。

#include <gtest/gtest.h>
#include "yuan/Parser/Parser.h"
#include "yuan/Lexer/Lexer.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Pattern.h"
#include "yuan/AST/Expr.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/TextDiagnosticPrinter.h"
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>

namespace yuan {

class ParsePatternTest : public ::testing::Test {
protected:
    void SetUp() override {
        SM = std::make_unique<SourceManager>();
        Diag = std::make_unique<DiagnosticEngine>(*SM);
        Ctx = std::make_unique<ASTContext>(*SM);
        
        // 设置诊断消费者（用于测试时不输出错误）
        auto printer = std::make_unique<TextDiagnosticPrinter>(std::cerr, *SM, false);
        Diag->setConsumer(std::move(printer));
    }
    
    /// \brief 解析模式字符串
    /// \param source 源代码字符串
    /// \return 解析结果
    ParseResult<Pattern> parsePattern(const std::string& source) {
        auto fileID = SM->createBuffer(source, "<test>");
        Lexer lexer(*SM, *Diag, fileID);
        Parser parser(lexer, *Diag, *Ctx);
        return parser.parsePattern();
    }
    
    /// \brief 解析模式字符串并返回第一个Token
    /// \param source 源代码字符串
    /// \return 第一个Token
    Token parseFirstToken(const std::string& source) {
        auto fileID = SM->createBuffer(source, "<test>");
        Lexer lexer(*SM, *Diag, fileID);
        return lexer.lex();
    }
    
    std::unique_ptr<SourceManager> SM;
    std::unique_ptr<DiagnosticEngine> Diag;
    std::unique_ptr<ASTContext> Ctx;
};

// ============================================================================
// 基本模式测试
// ============================================================================

TEST_F(ParsePatternTest, WildcardPattern) {
    auto result = parsePattern("_");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = result.get();
    ASSERT_NE(pattern, nullptr);
    EXPECT_TRUE(llvm::isa<WildcardPattern>(pattern));
}

TEST_F(ParsePatternTest, IdentifierPattern) {
    auto result = parsePattern("x");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<IdentifierPattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_EQ(pattern->getName(), "x");
    EXPECT_FALSE(pattern->isMutable());
    EXPECT_FALSE(pattern->hasType());
}

TEST_F(ParsePatternTest, MutableIdentifierPattern) {
    auto result = parsePattern("mut x");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<IdentifierPattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_EQ(pattern->getName(), "x");
    EXPECT_TRUE(pattern->isMutable());
    EXPECT_FALSE(pattern->hasType());
}

TEST_F(ParsePatternTest, IdentifierPatternWithType) {
    auto result = parsePattern("x: i32");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<IdentifierPattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_EQ(pattern->getName(), "x");
    EXPECT_FALSE(pattern->isMutable());
    EXPECT_TRUE(pattern->hasType());
}

TEST_F(ParsePatternTest, MutableIdentifierPatternWithType) {
    auto result = parsePattern("mut x: i32");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<IdentifierPattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_EQ(pattern->getName(), "x");
    EXPECT_TRUE(pattern->isMutable());
    EXPECT_TRUE(pattern->hasType());
}

// ============================================================================
// 字面量模式测试
// ============================================================================

TEST_F(ParsePatternTest, IntegerLiteralPattern) {
    auto result = parsePattern("42");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<LiteralPattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_TRUE(llvm::isa<IntegerLiteralExpr>(pattern->getLiteral()));
}

TEST_F(ParsePatternTest, StringLiteralPattern) {
    auto result = parsePattern("\"hello\"");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<LiteralPattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_TRUE(llvm::isa<StringLiteralExpr>(pattern->getLiteral()));
}

TEST_F(ParsePatternTest, BooleanLiteralPattern) {
    auto result = parsePattern("true");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<LiteralPattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_TRUE(llvm::isa<BoolLiteralExpr>(pattern->getLiteral()));
}

// ============================================================================
// 元组模式测试
// ============================================================================

TEST_F(ParsePatternTest, EmptyTuplePattern) {
    auto result = parsePattern("()");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<TuplePattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_TRUE(pattern->isEmpty());
    EXPECT_EQ(pattern->getElementCount(), 0);
}

TEST_F(ParsePatternTest, SingleElementParentheses) {
    // (x) 应该被解析为括号包围的标识符模式，而不是元组
    auto result = parsePattern("(x)");
    ASSERT_TRUE(result.isSuccess());
    
    // 应该是标识符模式，不是元组模式
    auto* pattern = result.get();
    EXPECT_TRUE(llvm::isa<IdentifierPattern>(pattern));
}

TEST_F(ParsePatternTest, TwoElementTuplePattern) {
    auto result = parsePattern("(x, y)");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<TuplePattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_EQ(pattern->getElementCount(), 2);
    
    const auto& elements = pattern->getElements();
    EXPECT_TRUE(llvm::isa<IdentifierPattern>(elements[0]));
    EXPECT_TRUE(llvm::isa<IdentifierPattern>(elements[1]));
}

TEST_F(ParsePatternTest, NestedTuplePattern) {
    auto result = parsePattern("(x, (y, z))");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<TuplePattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_EQ(pattern->getElementCount(), 2);
    
    const auto& elements = pattern->getElements();
    EXPECT_TRUE(llvm::isa<IdentifierPattern>(elements[0]));
    EXPECT_TRUE(llvm::isa<TuplePattern>(elements[1]));
}

TEST_F(ParsePatternTest, TuplePatternWithTrailingComma) {
    auto result = parsePattern("(x, y,)");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<TuplePattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_EQ(pattern->getElementCount(), 2);
}

// ============================================================================
// 结构体模式测试
// ============================================================================

TEST_F(ParsePatternTest, EmptyStructPattern) {
    auto result = parsePattern("Point {}");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<StructPattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_EQ(pattern->getTypeName(), "Point");
    EXPECT_EQ(pattern->getFieldCount(), 0);
    EXPECT_FALSE(pattern->hasRest());
}

TEST_F(ParsePatternTest, StructPatternWithFields) {
    auto result = parsePattern("Point { x, y }");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<StructPattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_EQ(pattern->getTypeName(), "Point");
    EXPECT_EQ(pattern->getFieldCount(), 2);
    
    const auto& fields = pattern->getFields();
    EXPECT_EQ(fields[0].Name, "x");
    EXPECT_EQ(fields[1].Name, "y");
    EXPECT_TRUE(llvm::isa<IdentifierPattern>(fields[0].Pat));
    EXPECT_TRUE(llvm::isa<IdentifierPattern>(fields[1].Pat));
}

TEST_F(ParsePatternTest, StructPatternWithExplicitFields) {
    auto result = parsePattern("Point { x: a, y: b }");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<StructPattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_EQ(pattern->getTypeName(), "Point");
    EXPECT_EQ(pattern->getFieldCount(), 2);
    
    const auto& fields = pattern->getFields();
    EXPECT_EQ(fields[0].Name, "x");
    EXPECT_EQ(fields[1].Name, "y");
    
    auto* field0Pat = llvm::cast<IdentifierPattern>(fields[0].Pat);
    auto* field1Pat = llvm::cast<IdentifierPattern>(fields[1].Pat);
    EXPECT_EQ(field0Pat->getName(), "a");
    EXPECT_EQ(field1Pat->getName(), "b");
}

TEST_F(ParsePatternTest, StructPatternWithRest) {
    auto result = parsePattern("Point { x, .. }");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<StructPattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_EQ(pattern->getTypeName(), "Point");
    EXPECT_EQ(pattern->getFieldCount(), 1);
    EXPECT_TRUE(pattern->hasRest());
}

// ============================================================================
// 枚举模式测试
// ============================================================================

TEST_F(ParsePatternTest, SimpleEnumPattern) {
    auto result = parsePattern("Color::Red");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<EnumPattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_EQ(pattern->getEnumName(), "Color");
    EXPECT_EQ(pattern->getVariantName(), "Red");
    EXPECT_FALSE(pattern->hasPayload());
}

TEST_F(ParsePatternTest, EnumPatternWithDotSyntax) {
    auto result = parsePattern("Color.Red");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<EnumPattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_EQ(pattern->getEnumName(), "Color");
    EXPECT_EQ(pattern->getVariantName(), "Red");
    EXPECT_FALSE(pattern->hasPayload());
}

TEST_F(ParsePatternTest, EnumPatternWithPayload) {
    auto result = parsePattern("Result::Ok(value)");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<EnumPattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_EQ(pattern->getEnumName(), "Result");
    EXPECT_EQ(pattern->getVariantName(), "Ok");
    EXPECT_TRUE(pattern->hasPayload());
    EXPECT_EQ(pattern->getPayloadCount(), 1);
    
    const auto& payload = pattern->getPayload();
    EXPECT_TRUE(llvm::isa<IdentifierPattern>(payload[0]));
}

TEST_F(ParsePatternTest, EnumPatternWithMultiplePayload) {
    auto result = parsePattern("Tuple::Pair(x, y)");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<EnumPattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_EQ(pattern->getEnumName(), "Tuple");
    EXPECT_EQ(pattern->getVariantName(), "Pair");
    EXPECT_TRUE(pattern->hasPayload());
    EXPECT_EQ(pattern->getPayloadCount(), 2);
}

// ============================================================================
// 范围模式测试
// ============================================================================

TEST_F(ParsePatternTest, InclusiveRangePattern) {
    auto result = parsePattern("1..=10");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<RangePattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_TRUE(pattern->isInclusive());
    EXPECT_TRUE(llvm::isa<IntegerLiteralExpr>(pattern->getStart()));
    EXPECT_TRUE(llvm::isa<IntegerLiteralExpr>(pattern->getEnd()));
}

TEST_F(ParsePatternTest, ExclusiveRangePattern) {
    auto result = parsePattern("1..10");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<RangePattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_FALSE(pattern->isInclusive());
    EXPECT_TRUE(llvm::isa<IntegerLiteralExpr>(pattern->getStart()));
    EXPECT_TRUE(llvm::isa<IntegerLiteralExpr>(pattern->getEnd()));
}

TEST_F(ParsePatternTest, CharRangePattern) {
    auto result = parsePattern("'a'..='z'");
    ASSERT_TRUE(result.isSuccess());
    
    auto* pattern = llvm::cast<RangePattern>(result.get());
    ASSERT_NE(pattern, nullptr);
    EXPECT_TRUE(pattern->isInclusive());
    EXPECT_TRUE(llvm::isa<CharLiteralExpr>(pattern->getStart()));
    EXPECT_TRUE(llvm::isa<CharLiteralExpr>(pattern->getEnd()));
}

// ============================================================================
// 错误情况测试
// ============================================================================

TEST_F(ParsePatternTest, InvalidPattern) {
    auto result = parsePattern("123abc");
    EXPECT_TRUE(result.isError());
    EXPECT_TRUE(Diag->hasErrors());
}

TEST_F(ParsePatternTest, UnterminatedTuplePattern) {
    auto result = parsePattern("(x, y");
    EXPECT_TRUE(result.isError());
    EXPECT_TRUE(Diag->hasErrors());
}

TEST_F(ParsePatternTest, UnterminatedStructPattern) {
    auto result = parsePattern("Point { x, y");
    EXPECT_TRUE(result.isError());
    EXPECT_TRUE(Diag->hasErrors());
}

TEST_F(ParsePatternTest, InvalidStructField) {
    auto result = parsePattern("Point { 123 }");
    EXPECT_TRUE(result.isError());
    EXPECT_TRUE(Diag->hasErrors());
}

} // namespace yuan