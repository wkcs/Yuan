/// \file LexerNumberPropertyTest.cpp
/// \brief Property-based tests for Lexer number literal handling.
///
/// **Feature: yuan-compiler, Property 4: Lexer 整数字面量解析**
/// **Validates: Requirements 2.3, 2.4**

#include <gtest/gtest.h>
#include "yuan/Lexer/Lexer.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/TextDiagnosticPrinter.h"
#include <sstream>
#include <random>
#include <vector>
#include <string>
#include <unordered_set>

using namespace yuan;

class LexerNumberPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        consumer = std::make_unique<StoredDiagnosticConsumer>();
        diag = std::make_unique<DiagnosticEngine>(sm);
        diag->setConsumer(std::make_unique<StoredDiagnosticConsumer>());
    }
    
    void TearDown() override {
        // 清理
    }
    
    /// \brief 创建测试源码
    /// \param content 源码内容
    /// \return FileID
    SourceManager::FileID createTestBuffer(const std::string& content) {
        return sm.createBuffer(content, "<test>");
    }
    
    /// \brief 从 Lexer 中提取所有非 EOF token
    /// \param lexer 词法分析器
    /// \return Token 列表
    std::vector<Token> extractAllTokens(Lexer& lexer) {
        std::vector<Token> tokens;
        Token token;
        do {
            token = lexer.lex();
            if (!token.isEOF()) {
                tokens.push_back(token);
            }
        } while (!token.isEOF());
        return tokens;
    }
    
    /// \brief 生成随机十进制整数
    /// \param gen 随机数生成器
    /// \return 随机整数字符串
    std::string generateRandomDecimalInteger(std::mt19937& gen) {
        std::uniform_int_distribution<> lengthDist(1, 10);
        std::uniform_int_distribution<> digitDist(0, 9);
        
        int length = lengthDist(gen);
        std::string number;
        
        // 第一位不能是 0（除非整个数字就是 0）
        if (length == 1 || gen() % 10 != 0) {
            number += static_cast<char>('1' + (gen() % 9)); // 1-9
        } else {
            number += '0';
            return number;
        }
        
        // 后续数字
        for (int i = 1; i < length; ++i) {
            number += static_cast<char>('0' + digitDist(gen));
            
            // 随机添加下划线分隔符
            if (i < length - 1 && gen() % 4 == 0) {
                number += '_';
            }
        }
        
        return number;
    }
    
    /// \brief 生成随机十六进制整数
    /// \param gen 随机数生成器
    /// \return 随机十六进制字符串
    std::string generateRandomHexInteger(std::mt19937& gen) {
        std::uniform_int_distribution<> lengthDist(1, 8);
        std::uniform_int_distribution<> digitDist(0, 15);
        
        int length = lengthDist(gen);
        std::string number = "0x";
        
        for (int i = 0; i < length; ++i) {
            int digit = digitDist(gen);
            if (digit < 10) {
                number += static_cast<char>('0' + digit);
            } else {
                // 随机选择大小写
                if (gen() % 2 == 0) {
                    number += static_cast<char>('A' + digit - 10);
                } else {
                    number += static_cast<char>('a' + digit - 10);
                }
            }
            
            // 随机添加下划线分隔符
            if (i < length - 1 && gen() % 4 == 0) {
                number += '_';
            }
        }
        
        return number;
    }
    
    /// \brief 生成随机八进制整数
    /// \param gen 随机数生成器
    /// \return 随机八进制字符串
    std::string generateRandomOctalInteger(std::mt19937& gen) {
        std::uniform_int_distribution<> lengthDist(1, 8);
        std::uniform_int_distribution<> digitDist(0, 7);
        
        int length = lengthDist(gen);
        std::string number = "0o";
        
        for (int i = 0; i < length; ++i) {
            number += static_cast<char>('0' + digitDist(gen));
            
            // 随机添加下划线分隔符
            if (i < length - 1 && gen() % 4 == 0) {
                number += '_';
            }
        }
        
        return number;
    }
    
    /// \brief 生成随机二进制整数
    /// \param gen 随机数生成器
    /// \return 随机二进制字符串
    std::string generateRandomBinaryInteger(std::mt19937& gen) {
        std::uniform_int_distribution<> lengthDist(1, 16);
        
        int length = lengthDist(gen);
        std::string number = "0b";
        
        for (int i = 0; i < length; ++i) {
            number += (gen() % 2 == 0) ? '0' : '1';
            
            // 随机添加下划线分隔符
            if (i < length - 1 && gen() % 4 == 0) {
                number += '_';
            }
        }
        
        return number;
    }
    
    /// \brief 生成随机浮点数
    /// \param gen 随机数生成器
    /// \return 随机浮点数字符串
    std::string generateRandomFloat(std::mt19937& gen) {
        std::string intPart = generateRandomDecimalInteger(gen);
        std::string fracPart = generateRandomDecimalInteger(gen);
        
        std::string number = intPart + "." + fracPart;
        
        // 随机添加科学计数法
        if (gen() % 3 == 0) {
            number += (gen() % 2 == 0) ? "e" : "E";
            if (gen() % 2 == 0) {
                number += (gen() % 2 == 0) ? "+" : "-";
            }
            number += std::to_string(gen() % 100);
        }
        
        return number;
    }
    
    /// \brief 获取有效的整数类型后缀
    /// \return 整数后缀列表
    std::vector<std::string> getValidIntegerSuffixes() {
        return {"i8", "i16", "i32", "i64", "i128", "isize",
                "u8", "u16", "u32", "u64", "u128", "usize"};
    }
    
    /// \brief 获取有效的浮点数类型后缀
    /// \return 浮点数后缀列表
    std::vector<std::string> getValidFloatSuffixes() {
        return {"f32", "f64"};
    }
    
protected:
    SourceManager sm;
    std::unique_ptr<StoredDiagnosticConsumer> consumer;
    std::unique_ptr<DiagnosticEngine> diag;
};

/// \brief Property 4: 十进制整数字面量解析
/// 
/// 对于任何有效的十进制整数字面量，词法分析器应该正确识别为 IntegerLiteral。
TEST_F(LexerNumberPropertyTest, DecimalIntegerLiteralParsing) {
    std::mt19937 gen(42); // 固定种子以便重现
    
    // 运行多次随机测试
    for (int iteration = 0; iteration < 100; ++iteration) {
        std::string number = generateRandomDecimalInteger(gen);
        
        SourceManager::FileID fileID = createTestBuffer(number);
        Lexer lexer(sm, *diag, fileID);
        
        std::vector<Token> tokens = extractAllTokens(lexer);
        
        // 应该只有一个 token
        ASSERT_EQ(tokens.size(), 1)
            << "Iteration " << iteration << ": Expected 1 token for decimal '" 
            << number << "', got " << tokens.size();
        
        // 应该是整数字面量
        EXPECT_EQ(tokens[0].getKind(), TokenKind::IntegerLiteral)
            << "Iteration " << iteration << ": Decimal '" << number 
            << "' should be recognized as IntegerLiteral, got kind " 
            << static_cast<int>(tokens[0].getKind());
        
        // 文本应该匹配
        EXPECT_EQ(tokens[0].getText(), number)
            << "Iteration " << iteration << ": Decimal '" << number << "' text mismatch";
    }
}

/// \brief Property 4: 十六进制整数字面量解析
/// 
/// 对于任何有效的十六进制整数字面量，词法分析器应该正确识别为 IntegerLiteral。
TEST_F(LexerNumberPropertyTest, HexadecimalIntegerLiteralParsing) {
    std::mt19937 gen(123);
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        std::string number = generateRandomHexInteger(gen);
        
        SourceManager::FileID fileID = createTestBuffer(number);
        Lexer lexer(sm, *diag, fileID);
        
        std::vector<Token> tokens = extractAllTokens(lexer);
        
        ASSERT_EQ(tokens.size(), 1)
            << "Iteration " << iteration << ": Expected 1 token for hex '" 
            << number << "', got " << tokens.size();
        
        EXPECT_EQ(tokens[0].getKind(), TokenKind::IntegerLiteral)
            << "Iteration " << iteration << ": Hex '" << number 
            << "' should be recognized as IntegerLiteral";
        
        EXPECT_EQ(tokens[0].getText(), number)
            << "Iteration " << iteration << ": Hex '" << number << "' text mismatch";
    }
}

/// \brief Property 4: 八进制整数字面量解析
/// 
/// 对于任何有效的八进制整数字面量，词法分析器应该正确识别为 IntegerLiteral。
TEST_F(LexerNumberPropertyTest, OctalIntegerLiteralParsing) {
    std::mt19937 gen(456);
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        std::string number = generateRandomOctalInteger(gen);
        
        SourceManager::FileID fileID = createTestBuffer(number);
        Lexer lexer(sm, *diag, fileID);
        
        std::vector<Token> tokens = extractAllTokens(lexer);
        
        ASSERT_EQ(tokens.size(), 1)
            << "Iteration " << iteration << ": Expected 1 token for octal '" 
            << number << "', got " << tokens.size();
        
        EXPECT_EQ(tokens[0].getKind(), TokenKind::IntegerLiteral)
            << "Iteration " << iteration << ": Octal '" << number 
            << "' should be recognized as IntegerLiteral";
        
        EXPECT_EQ(tokens[0].getText(), number)
            << "Iteration " << iteration << ": Octal '" << number << "' text mismatch";
    }
}

/// \brief Property 4: 二进制整数字面量解析
/// 
/// 对于任何有效的二进制整数字面量，词法分析器应该正确识别为 IntegerLiteral。
TEST_F(LexerNumberPropertyTest, BinaryIntegerLiteralParsing) {
    std::mt19937 gen(789);
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        std::string number = generateRandomBinaryInteger(gen);
        
        SourceManager::FileID fileID = createTestBuffer(number);
        Lexer lexer(sm, *diag, fileID);
        
        std::vector<Token> tokens = extractAllTokens(lexer);
        
        ASSERT_EQ(tokens.size(), 1)
            << "Iteration " << iteration << ": Expected 1 token for binary '" 
            << number << "', got " << tokens.size();
        
        EXPECT_EQ(tokens[0].getKind(), TokenKind::IntegerLiteral)
            << "Iteration " << iteration << ": Binary '" << number 
            << "' should be recognized as IntegerLiteral";
        
        EXPECT_EQ(tokens[0].getText(), number)
            << "Iteration " << iteration << ": Binary '" << number << "' text mismatch";
    }
}

/// \brief Property 4: 浮点数字面量解析
/// 
/// 对于任何有效的浮点数字面量，词法分析器应该正确识别为 FloatLiteral。
TEST_F(LexerNumberPropertyTest, FloatLiteralParsing) {
    std::mt19937 gen(101112);
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        std::string number = generateRandomFloat(gen);
        
        SourceManager::FileID fileID = createTestBuffer(number);
        Lexer lexer(sm, *diag, fileID);
        
        std::vector<Token> tokens = extractAllTokens(lexer);
        
        ASSERT_EQ(tokens.size(), 1)
            << "Iteration " << iteration << ": Expected 1 token for float '" 
            << number << "', got " << tokens.size();
        
        EXPECT_EQ(tokens[0].getKind(), TokenKind::FloatLiteral)
            << "Iteration " << iteration << ": Float '" << number 
            << "' should be recognized as FloatLiteral";
        
        EXPECT_EQ(tokens[0].getText(), number)
            << "Iteration " << iteration << ": Float '" << number << "' text mismatch";
    }
}

/// \brief Property 4: 整数类型后缀解析
/// 
/// 带有有效类型后缀的整数字面量应该被正确解析。
TEST_F(LexerNumberPropertyTest, IntegerTypeSuffixParsing) {
    std::mt19937 gen(131415);
    std::vector<std::string> suffixes = getValidIntegerSuffixes();
    
    for (int iteration = 0; iteration < 50; ++iteration) {
        std::string baseNumber = generateRandomDecimalInteger(gen);
        std::string suffix = suffixes[gen() % suffixes.size()];
        std::string number = baseNumber + suffix;
        
        SourceManager::FileID fileID = createTestBuffer(number);
        Lexer lexer(sm, *diag, fileID);
        
        std::vector<Token> tokens = extractAllTokens(lexer);
        
        ASSERT_EQ(tokens.size(), 1)
            << "Iteration " << iteration << ": Expected 1 token for suffixed integer '" 
            << number << "', got " << tokens.size();
        
        EXPECT_EQ(tokens[0].getKind(), TokenKind::IntegerLiteral)
            << "Iteration " << iteration << ": Suffixed integer '" << number 
            << "' should be recognized as IntegerLiteral";
        
        EXPECT_EQ(tokens[0].getText(), number)
            << "Iteration " << iteration << ": Suffixed integer '" << number << "' text mismatch";
    }
}

/// \brief Property 4: 浮点数类型后缀解析
/// 
/// 带有有效类型后缀的浮点数字面量应该被正确解析。
TEST_F(LexerNumberPropertyTest, FloatTypeSuffixParsing) {
    std::mt19937 gen(161718);
    std::vector<std::string> suffixes = getValidFloatSuffixes();
    
    for (int iteration = 0; iteration < 50; ++iteration) {
        std::string baseNumber = generateRandomFloat(gen);
        std::string suffix = suffixes[gen() % suffixes.size()];
        std::string number = baseNumber + suffix;
        
        SourceManager::FileID fileID = createTestBuffer(number);
        Lexer lexer(sm, *diag, fileID);
        
        std::vector<Token> tokens = extractAllTokens(lexer);
        
        ASSERT_EQ(tokens.size(), 1)
            << "Iteration " << iteration << ": Expected 1 token for suffixed float '" 
            << number << "', got " << tokens.size();
        
        EXPECT_EQ(tokens[0].getKind(), TokenKind::FloatLiteral)
            << "Iteration " << iteration << ": Suffixed float '" << number 
            << "' should be recognized as FloatLiteral";
        
        EXPECT_EQ(tokens[0].getText(), number)
            << "Iteration " << iteration << ": Suffixed float '" << number << "' text mismatch";
    }
}

/// \brief Property 4: 数字边界检测
/// 
/// 数字字面量应该在非数字字符处正确结束。
TEST_F(LexerNumberPropertyTest, NumberBoundaryDetection) {
    std::mt19937 gen(192021);
    
    // 测试各种边界字符
    std::vector<char> boundaryChars = {' ', '\t', '\n', '(', ')', '[', ']', '{', '}', 
                                       ',', ';', ':', '+', '-', '*', '/', '=', 
                                       '<', '>', '!', '&', '|', '^', '~', '?'};
    
    for (int iteration = 0; iteration < 50; ++iteration) {
        std::string number = generateRandomDecimalInteger(gen);
        char boundary = boundaryChars[gen() % boundaryChars.size()];
        
        std::string source = number + boundary;
        SourceManager::FileID fileID = createTestBuffer(source);
        Lexer lexer(sm, *diag, fileID);
        
        std::vector<Token> tokens = extractAllTokens(lexer);
        
        // 应该至少有一个 token（数字）
        ASSERT_GE(tokens.size(), 1)
            << "Iteration " << iteration << ": Expected at least 1 token for '" 
            << source << "', got " << tokens.size();
        
        // 第一个 token 应该是数字字面量
        EXPECT_EQ(tokens[0].getKind(), TokenKind::IntegerLiteral)
            << "Iteration " << iteration << ": First token should be integer literal for '" 
            << source << "', got kind " << static_cast<int>(tokens[0].getKind());
        
        // 数字文本应该正确（不包含边界字符）
        EXPECT_EQ(tokens[0].getText(), number)
            << "Iteration " << iteration << ": Number text should be '" << number 
            << "' for source '" << source << "', got '" << tokens[0].getText() << "'";
    }
}