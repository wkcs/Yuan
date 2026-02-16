/// \file LexerStringPropertyTest.cpp
/// \brief 字符串字面量词法分析属性测试
///
/// 这个文件包含了对 Lexer 字符串字面量解析功能的属性测试，
/// 验证各种字符串格式的正确识别和处理。

#include "yuan/Lexer/Lexer.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/TextDiagnosticPrinter.h"
#include <gtest/gtest.h>
#include <sstream>
#include <random>

using namespace yuan;

class LexerStringPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建源码管理器
        sm = std::make_unique<SourceManager>();
        
        // 创建诊断打印器
        diagPrinter = std::make_unique<TextDiagnosticPrinter>(diagStream, *sm);
        
        // 创建诊断引擎
        diag = std::make_unique<DiagnosticEngine>(*sm);
    }
    
    /// 辅助函数：从字符串创建 Lexer 并获取第一个 token
    Token lexFirstToken(const std::string& source) {
        auto fileID = sm->createBuffer(source, "<test>");
        Lexer lexer(*sm, *diag, fileID);
        return lexer.lex();
    }
    
    /// 辅助函数：生成随机字符串内容
    std::string generateRandomStringContent(size_t length) {
        static const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
        
        std::string result;
        result.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            result += chars[dis(gen)];
        }
        return result;
    }
    
    /// 辅助函数：生成随机分隔符
    std::string generateRandomDelimiter(size_t length) {
        std::string result;
        for (size_t i = 0; i < length; ++i) {
            result += '#';
        }
        return result;
    }
    
    std::unique_ptr<SourceManager> sm;
    std::unique_ptr<DiagnosticEngine> diag;
    std::unique_ptr<TextDiagnosticPrinter> diagPrinter;
    std::ostringstream diagStream;
};

/// 测试普通字符串字面量的解析
TEST_F(LexerStringPropertyTest, StringLiteralParsing) {
    // 测试基本字符串
    Token token = lexFirstToken("\"hello world\"");
    EXPECT_EQ(token.getKind(), TokenKind::StringLiteral);
    EXPECT_EQ(token.getText(), "\"hello world\"");
    
    // 测试空字符串
    token = lexFirstToken("\"\"");
    EXPECT_EQ(token.getKind(), TokenKind::StringLiteral);
    EXPECT_EQ(token.getText(), "\"\"");
    
    // 测试包含转义字符的字符串
    token = lexFirstToken("\"hello\\nworld\\t!\"");
    EXPECT_EQ(token.getKind(), TokenKind::StringLiteral);
    EXPECT_EQ(token.getText(), "\"hello\\nworld\\t!\"");
    
    // 测试包含 Unicode 转义的字符串
    token = lexFirstToken("\"\\u{1F600}\"");
    EXPECT_EQ(token.getKind(), TokenKind::StringLiteral);
    EXPECT_EQ(token.getText(), "\"\\u{1F600}\"");
    
    // 测试包含十六进制转义的字符串
    token = lexFirstToken("\"\\x41\\x42\\x43\"");
    EXPECT_EQ(token.getKind(), TokenKind::StringLiteral);
    EXPECT_EQ(token.getText(), "\"\\x41\\x42\\x43\"");
}

/// 测试字符字面量的解析
TEST_F(LexerStringPropertyTest, CharLiteralParsing) {
    // 测试基本字符
    Token token = lexFirstToken("'a'");
    EXPECT_EQ(token.getKind(), TokenKind::CharLiteral);
    EXPECT_EQ(token.getText(), "'a'");
    
    // 测试转义字符
    token = lexFirstToken("'\\n'");
    EXPECT_EQ(token.getKind(), TokenKind::CharLiteral);
    EXPECT_EQ(token.getText(), "'\\n'");
    
    // 测试十六进制转义字符
    token = lexFirstToken("'\\x41'");
    EXPECT_EQ(token.getKind(), TokenKind::CharLiteral);
    EXPECT_EQ(token.getText(), "'\\x41'");
    
    // 测试 Unicode 转义字符
    token = lexFirstToken("'\\u{41}'");
    EXPECT_EQ(token.getKind(), TokenKind::CharLiteral);
    EXPECT_EQ(token.getText(), "'\\u{41}'");
    
    // 测试单引号转义
    token = lexFirstToken("'\\''");
    EXPECT_EQ(token.getKind(), TokenKind::CharLiteral);
    EXPECT_EQ(token.getText(), "'\\''");
}

/// 测试原始字符串字面量的解析
TEST_F(LexerStringPropertyTest, RawStringLiteralParsing) {
    // 测试基本原始字符串
    Token token = lexFirstToken("r\"hello world\"");
    EXPECT_EQ(token.getKind(), TokenKind::RawStringLiteral);
    EXPECT_EQ(token.getText(), "r\"hello world\"");
    
    // 测试包含转义字符的原始字符串（不应被处理）
    token = lexFirstToken("r\"hello\\nworld\"");
    EXPECT_EQ(token.getKind(), TokenKind::RawStringLiteral);
    EXPECT_EQ(token.getText(), "r\"hello\\nworld\"");
    
    // 测试带分隔符的原始字符串
    token = lexFirstToken("r#\"hello \"world\" !\"#");
    EXPECT_EQ(token.getKind(), TokenKind::RawStringLiteral);
    EXPECT_EQ(token.getText(), "r#\"hello \"world\" !\"#");
    
    // 测试多个分隔符的原始字符串
    token = lexFirstToken("r###\"hello # ## world\"###");
    EXPECT_EQ(token.getKind(), TokenKind::RawStringLiteral);
    EXPECT_EQ(token.getText(), "r###\"hello # ## world\"###");
    
    // 测试空的原始字符串
    token = lexFirstToken("r\"\"");
    EXPECT_EQ(token.getKind(), TokenKind::RawStringLiteral);
    EXPECT_EQ(token.getText(), "r\"\"");
}

/// 测试多行字符串字面量的解析
TEST_F(LexerStringPropertyTest, MultilineStringLiteralParsing) {
    // 测试基本多行字符串
    Token token = lexFirstToken("\"\"\"hello\nworld\"\"\"");
    EXPECT_EQ(token.getKind(), TokenKind::MultilineStringLiteral);
    EXPECT_EQ(token.getText(), "\"\"\"hello\nworld\"\"\"");
    
    // 测试空的多行字符串
    token = lexFirstToken("\"\"\"\"\"\"");
    EXPECT_EQ(token.getKind(), TokenKind::MultilineStringLiteral);
    EXPECT_EQ(token.getText(), "\"\"\"\"\"\"");
    
    // 测试包含转义字符的多行字符串
    token = lexFirstToken("\"\"\"line1\\nline2\\tindented\"\"\"");
    EXPECT_EQ(token.getKind(), TokenKind::MultilineStringLiteral);
    EXPECT_EQ(token.getText(), "\"\"\"line1\\nline2\\tindented\"\"\"");
    
    // 测试包含双引号的多行字符串
    token = lexFirstToken("\"\"\"He said \"hello\" to me\"\"\"");
    EXPECT_EQ(token.getKind(), TokenKind::MultilineStringLiteral);
    EXPECT_EQ(token.getText(), "\"\"\"He said \"hello\" to me\"\"\"");
}

/// 测试字符串边界检测
TEST_F(LexerStringPropertyTest, StringBoundaryDetection) {
    // 测试字符串后跟标识符
    auto fileID = sm->createBuffer("\"hello\"world", "<test>");
    Lexer lexer(*sm, *diag, fileID);
    
    Token token1 = lexer.lex();
    EXPECT_EQ(token1.getKind(), TokenKind::StringLiteral);
    EXPECT_EQ(token1.getText(), "\"hello\"");
    
    Token token2 = lexer.lex();
    EXPECT_EQ(token2.getKind(), TokenKind::Identifier);
    EXPECT_EQ(token2.getText(), "world");
    
    // 测试字符串后跟数字
    fileID = sm->createBuffer("\"test\"123", "<test>");
    Lexer lexer2(*sm, *diag, fileID);
    
    token1 = lexer2.lex();
    EXPECT_EQ(token1.getKind(), TokenKind::StringLiteral);
    EXPECT_EQ(token1.getText(), "\"test\"");
    
    token2 = lexer2.lex();
    EXPECT_EQ(token2.getKind(), TokenKind::IntegerLiteral);
    EXPECT_EQ(token2.getText(), "123");
}

/// 测试转义序列的正确处理
TEST_F(LexerStringPropertyTest, EscapeSequenceHandling) {
    // 测试所有基本转义序列
    std::vector<std::pair<std::string, std::string>> escapeTests = {
        {"\"\\n\"", "\"\\n\""},      // 换行符
        {"\"\\t\"", "\"\\t\""},      // 制表符
        {"\"\\r\"", "\"\\r\""},      // 回车符
        {"\"\\\\\"", "\"\\\\\""},    // 反斜杠
        {"\"\\\"\"", "\"\\\"\""},    // 双引号
        {"\"\\0\"", "\"\\0\""},      // 空字符
    };
    
    for (const auto& test : escapeTests) {
        Token token = lexFirstToken(test.first);
        EXPECT_EQ(token.getKind(), TokenKind::StringLiteral);
        EXPECT_EQ(token.getText(), test.second);
    }
    
    // 测试十六进制转义序列
    Token token = lexFirstToken("\"\\x48\\x65\\x6C\\x6C\\x6F\"");
    EXPECT_EQ(token.getKind(), TokenKind::StringLiteral);
    EXPECT_EQ(token.getText(), "\"\\x48\\x65\\x6C\\x6C\\x6F\"");
    
    // 测试 Unicode 转义序列
    token = lexFirstToken("\"\\u{48}\\u{65}\\u{6C}\\u{6C}\\u{6F}\"");
    EXPECT_EQ(token.getKind(), TokenKind::StringLiteral);
    EXPECT_EQ(token.getText(), "\"\\u{48}\\u{65}\\u{6C}\\u{6C}\\u{6F}\"");
}

/// 测试随机生成的字符串内容
TEST_F(LexerStringPropertyTest, RandomStringContentParsing) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> lengthDis(1, 50);
    
    // 测试多个随机字符串
    for (int i = 0; i < 20; ++i) {
        size_t length = lengthDis(gen);
        std::string content = generateRandomStringContent(length);
        std::string source = "\"" + content + "\"";
        
        Token token = lexFirstToken(source);
        EXPECT_EQ(token.getKind(), TokenKind::StringLiteral);
        EXPECT_EQ(token.getText(), source);
    }
}

/// 测试随机生成的原始字符串分隔符
TEST_F(LexerStringPropertyTest, RandomRawStringDelimiterParsing) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> lengthDis(0, 10);
    
    // 测试多个随机分隔符长度
    for (int i = 0; i < 10; ++i) {
        size_t delimiterLength = lengthDis(gen);
        std::string delimiter = generateRandomDelimiter(delimiterLength);
        std::string content = generateRandomStringContent(20);
        std::string source = "r" + delimiter + "\"" + content + "\"" + delimiter;
        
        Token token = lexFirstToken(source);
        EXPECT_EQ(token.getKind(), TokenKind::RawStringLiteral);
        EXPECT_EQ(token.getText(), source);
    }
}