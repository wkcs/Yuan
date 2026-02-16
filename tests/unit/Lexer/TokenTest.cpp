#include <gtest/gtest.h>
#include "yuan/Lexer/Token.h"
#include "yuan/Basic/TokenKinds.h"
#include "yuan/Basic/SourceLocation.h"

using namespace yuan;

class TokenTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 设置测试环境
    }
    
    void TearDown() override {
        // 清理测试环境
    }
};

// 测试 Token 的基本构造和访问
TEST_F(TokenTest, BasicConstruction) {
    SourceLocation loc(100);
    Token token(TokenKind::Identifier, loc, "test_var");
    
    EXPECT_EQ(token.getKind(), TokenKind::Identifier);
    EXPECT_EQ(token.getLocation().getOffset(), 100u);
    EXPECT_EQ(token.getText(), "test_var");
    EXPECT_TRUE(token.isValid());
    EXPECT_FALSE(token.isEOF());
}

// 测试默认构造函数
TEST_F(TokenTest, DefaultConstruction) {
    Token token;
    
    EXPECT_EQ(token.getKind(), TokenKind::Invalid);
    EXPECT_FALSE(token.isValid());
    EXPECT_FALSE(token.isEOF());
    EXPECT_EQ(token.getText(), "");
}

// 测试 Token 类型检查方法
TEST_F(TokenTest, TypeChecking) {
    SourceLocation loc(0);
    
    // 测试 is() 方法
    Token identifierToken(TokenKind::Identifier, loc, "var_name");
    EXPECT_TRUE(identifierToken.is(TokenKind::Identifier));
    EXPECT_FALSE(identifierToken.is(TokenKind::IntegerLiteral));
    
    // 测试 isNot() 方法
    EXPECT_FALSE(identifierToken.isNot(TokenKind::Identifier));
    EXPECT_TRUE(identifierToken.isNot(TokenKind::IntegerLiteral));
    
    // 测试 isOneOf() 方法（两个参数）
    EXPECT_TRUE(identifierToken.isOneOf(TokenKind::Identifier, TokenKind::IntegerLiteral));
    EXPECT_TRUE(identifierToken.isOneOf(TokenKind::IntegerLiteral, TokenKind::Identifier));
    EXPECT_FALSE(identifierToken.isOneOf(TokenKind::IntegerLiteral, TokenKind::FloatLiteral));
    
    // 测试 isOneOf() 方法（多个参数）
    EXPECT_TRUE(identifierToken.isOneOf(TokenKind::IntegerLiteral, TokenKind::FloatLiteral, TokenKind::Identifier));
    EXPECT_FALSE(identifierToken.isOneOf(TokenKind::IntegerLiteral, TokenKind::FloatLiteral, TokenKind::CharLiteral));
}

// 测试关键字识别
TEST_F(TokenTest, KeywordRecognition) {
    SourceLocation loc(0);
    
    // 测试关键字
    Token varToken(TokenKind::KW_var, loc, "var");
    EXPECT_TRUE(varToken.isKeyword());
    EXPECT_FALSE(varToken.isLiteral());
    EXPECT_FALSE(varToken.isOperator());
    
    Token constToken(TokenKind::KW_const, loc, "const");
    EXPECT_TRUE(constToken.isKeyword());
    
    Token funcToken(TokenKind::KW_func, loc, "func");
    EXPECT_TRUE(funcToken.isKeyword());
    
    // 测试类型关键字
    Token i32Token(TokenKind::KW_i32, loc, "i32");
    EXPECT_TRUE(i32Token.isKeyword());
    
    Token boolToken(TokenKind::KW_bool, loc, "bool");
    EXPECT_TRUE(boolToken.isKeyword());
    
    // 测试非关键字
    Token identifierToken(TokenKind::Identifier, loc, "my_var");
    EXPECT_FALSE(identifierToken.isKeyword());
}

// 测试字面量识别
TEST_F(TokenTest, LiteralRecognition) {
    SourceLocation loc(0);
    
    // 测试各种字面量
    Token intToken(TokenKind::IntegerLiteral, loc, "42");
    EXPECT_TRUE(intToken.isLiteral());
    EXPECT_FALSE(intToken.isKeyword());
    EXPECT_FALSE(intToken.isOperator());
    
    Token floatToken(TokenKind::FloatLiteral, loc, "3.14");
    EXPECT_TRUE(floatToken.isLiteral());
    
    Token charToken(TokenKind::CharLiteral, loc, "'a'");
    EXPECT_TRUE(charToken.isLiteral());
    
    Token stringToken(TokenKind::StringLiteral, loc, "\"hello\"");
    EXPECT_TRUE(stringToken.isLiteral());
    
    Token trueToken(TokenKind::KW_true, loc, "true");
    EXPECT_TRUE(trueToken.isLiteral());
    EXPECT_TRUE(trueToken.isKeyword()); // true 既是关键字也是字面量
    
    Token falseToken(TokenKind::KW_false, loc, "false");
    EXPECT_TRUE(falseToken.isLiteral());
    EXPECT_TRUE(falseToken.isKeyword());
    
    Token noneToken(TokenKind::KW_None, loc, "None");
    EXPECT_TRUE(noneToken.isLiteral());
    EXPECT_TRUE(noneToken.isKeyword());
    
    // 测试非字面量
    Token identifierToken(TokenKind::Identifier, loc, "my_var");
    EXPECT_FALSE(identifierToken.isLiteral());
}

// 测试运算符识别
TEST_F(TokenTest, OperatorRecognition) {
    SourceLocation loc(0);
    
    // 测试算术运算符
    Token plusToken(TokenKind::Plus, loc, "+");
    EXPECT_TRUE(plusToken.isOperator());
    EXPECT_FALSE(plusToken.isKeyword());
    EXPECT_FALSE(plusToken.isLiteral());
    
    Token minusToken(TokenKind::Minus, loc, "-");
    EXPECT_TRUE(minusToken.isOperator());
    
    Token starToken(TokenKind::Star, loc, "*");
    EXPECT_TRUE(starToken.isOperator());
    
    // 测试比较运算符
    Token equalToken(TokenKind::EqualEqual, loc, "==");
    EXPECT_TRUE(equalToken.isOperator());
    
    Token lessToken(TokenKind::Less, loc, "<");
    EXPECT_TRUE(lessToken.isOperator());
    
    // 测试赋值运算符
    Token assignToken(TokenKind::Equal, loc, "=");
    EXPECT_TRUE(assignToken.isOperator());
    
    Token plusAssignToken(TokenKind::PlusEqual, loc, "+=");
    EXPECT_TRUE(plusAssignToken.isOperator());
    
    // 测试逻辑运算符
    Token andToken(TokenKind::AmpAmp, loc, "&&");
    EXPECT_TRUE(andToken.isOperator());
    
    Token orToken(TokenKind::PipePipe, loc, "||");
    EXPECT_TRUE(orToken.isOperator());
    
    // 测试非运算符
    Token identifierToken(TokenKind::Identifier, loc, "my_var");
    EXPECT_FALSE(identifierToken.isOperator());
    
    Token lparenToken(TokenKind::LParen, loc, "(");
    EXPECT_FALSE(lparenToken.isOperator());
}

// 测试 Token 范围计算
TEST_F(TokenTest, TokenRange) {
    SourceLocation loc(100);
    Token token(TokenKind::Identifier, loc, "test_var");
    
    SourceRange range = token.getRange();
    EXPECT_EQ(range.getBegin().getOffset(), 100u);
    EXPECT_EQ(range.getEnd().getOffset(), 108u); // 100 + 8 ("test_var" 长度)
}

// 测试 EOF Token
TEST_F(TokenTest, EOFToken) {
    SourceLocation loc(1000);
    Token eofToken(TokenKind::EndOfFile, loc, "");
    
    EXPECT_TRUE(eofToken.isEOF());
    EXPECT_TRUE(eofToken.isValid());
    EXPECT_FALSE(eofToken.isKeyword());
    EXPECT_FALSE(eofToken.isLiteral());
    EXPECT_FALSE(eofToken.isOperator());
}

// 测试 Token 名称和拼写
TEST_F(TokenTest, TokenNameAndSpelling) {
    SourceLocation loc(0);
    
    Token varToken(TokenKind::KW_var, loc, "var");
    EXPECT_STREQ(varToken.getKindName(), "var");
    EXPECT_STREQ(varToken.getSpelling(), "var");
    
    Token plusToken(TokenKind::Plus, loc, "+");
    EXPECT_STREQ(plusToken.getKindName(), "+");
    EXPECT_STREQ(plusToken.getSpelling(), "+");
    
    Token identifierToken(TokenKind::Identifier, loc, "my_var");
    EXPECT_STREQ(identifierToken.getKindName(), "Identifier");
    EXPECT_STREQ(identifierToken.getSpelling(), ""); // 标识符没有固定拼写
}

// 测试内置标识符
TEST_F(TokenTest, BuiltinIdentifier) {
    SourceLocation loc(0);
    Token builtinToken(TokenKind::BuiltinIdentifier, loc, "@print");
    
    EXPECT_EQ(builtinToken.getKind(), TokenKind::BuiltinIdentifier);
    EXPECT_EQ(builtinToken.getText(), "@print");
    EXPECT_FALSE(builtinToken.isKeyword());
    EXPECT_FALSE(builtinToken.isLiteral());
    EXPECT_FALSE(builtinToken.isOperator());
}

// 测试各种字符串字面量类型
TEST_F(TokenTest, StringLiteralTypes) {
    SourceLocation loc(0);
    
    // 普通字符串
    Token stringToken(TokenKind::StringLiteral, loc, "\"hello world\"");
    EXPECT_TRUE(stringToken.isLiteral());
    
    // 原始字符串
    Token rawStringToken(TokenKind::RawStringLiteral, loc, "r\"hello\\nworld\"");
    EXPECT_TRUE(rawStringToken.isLiteral());
    
    // 多行字符串
    Token multilineStringToken(TokenKind::MultilineStringLiteral, loc, "\"\"\"hello\nworld\"\"\"");
    EXPECT_TRUE(multilineStringToken.isLiteral());
}