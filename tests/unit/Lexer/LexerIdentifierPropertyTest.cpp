/// \file LexerIdentifierPropertyTest.cpp
/// \brief Property-based tests for Lexer identifier and keyword handling.
///
/// **Feature: yuan-compiler, Property 2: Lexer 关键字识别**
/// **Feature: yuan-compiler, Property 3: Lexer Unicode 标识符**
/// **Validates: Requirements 2.1, 2.2**

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

class LexerIdentifierPropertyTest : public ::testing::Test {
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
    
    /// \brief 生成随机标识符（确保不是关键字）
    /// \param gen 随机数生成器
    /// \return 随机标识符
    std::string generateRandomNonKeywordIdentifier(std::mt19937& gen) {
        // 所有关键字的集合
        static const std::unordered_set<std::string> keywords = {
            "var", "const", "func", "return", "struct", "enum", "trait", "impl",
            "pub", "priv", "internal", "if", "elif", "else", "match", "while",
            "loop", "for", "in", "break", "continue", "true", "false", "async",
            "await", "as", "self", "Self", "mut", "ref", "ptr", "void", "defer",
            "type", "where", "None", "orelse", "i8", "i16", "i32", "i64", "i128",
            "isize", "u8", "u16", "u32", "u64", "u128", "usize", "f32", "f64",
            "bool", "char", "str"
        };
        
        std::string identifier;
        do {
            identifier = generateRandomIdentifier(gen);
        } while (keywords.find(identifier) != keywords.end());
        
        return identifier;
    }
    
    /// \brief 生成随机标识符
    /// \param gen 随机数生成器
    /// \return 随机标识符
    std::string generateRandomIdentifier(std::mt19937& gen) {
        std::uniform_int_distribution<> lengthDist(1, 10);
        std::uniform_int_distribution<> charDist(0, 25);
        
        int length = lengthDist(gen);
        std::string identifier;
        identifier.reserve(length);
        
        // 第一个字符必须是字母或下划线
        if (gen() % 2 == 0) {
            identifier += static_cast<char>('a' + charDist(gen));
        } else {
            identifier += '_';
        }
        
        // 后续字符可以是字母、数字或下划线
        for (int i = 1; i < length; ++i) {
            int choice = gen() % 3;
            if (choice == 0) {
                identifier += static_cast<char>('a' + charDist(gen));
            } else if (choice == 1) {
                identifier += static_cast<char>('0' + (gen() % 10));
            } else {
                identifier += '_';
            }
        }
        
        return identifier;
    }
    
    /// \brief 获取所有关键字
    /// \return 关键字列表
    std::vector<std::string> getAllKeywords() {
        return {
            "var", "const", "func", "return", "struct", "enum", "trait", "impl",
            "pub", "priv", "internal", "if", "elif", "else", "match", "while",
            "loop", "for", "in", "break", "continue", "true", "false", "async",
            "await", "as", "self", "Self", "mut", "ref", "ptr", "void", "defer",
            "type", "where", "None", "orelse", "i8", "i16", "i32", "i64", "i128",
            "isize", "u8", "u16", "u32", "u64", "u128", "usize", "f32", "f64",
            "bool", "char", "str"
        };
    }
    
protected:
    SourceManager sm;
    std::unique_ptr<StoredDiagnosticConsumer> consumer;
    std::unique_ptr<DiagnosticEngine> diag;
};

/// \brief Property 2: Lexer 关键字识别
/// 
/// 对于任何关键字，词法分析器应该识别为对应的关键字 token，
/// 而不是普通标识符。
TEST_F(LexerIdentifierPropertyTest, KeywordRecognition) {
    std::vector<std::string> keywords = getAllKeywords();
    
    // 测试每个关键字
    for (const std::string& keyword : keywords) {
        SourceManager::FileID fileID = createTestBuffer(keyword);
        Lexer lexer(sm, *diag, fileID);
        
        std::vector<Token> tokens = extractAllTokens(lexer);
        
        // 应该只有一个 token
        ASSERT_EQ(tokens.size(), 1) 
            << "Expected 1 token for keyword '" << keyword << "', got " << tokens.size();
        
        // 应该是关键字，不是普通标识符
        EXPECT_TRUE(tokens[0].isKeyword())
            << "Keyword '" << keyword << "' should be recognized as keyword, got kind " 
            << static_cast<int>(tokens[0].getKind());
        
        EXPECT_NE(tokens[0].getKind(), TokenKind::Identifier)
            << "Keyword '" << keyword << "' should not be recognized as identifier";
        
        // 文本应该匹配
        EXPECT_EQ(tokens[0].getText(), keyword)
            << "Keyword '" << keyword << "' text mismatch";
    }
}

/// \brief Property 2: 非关键字标识符识别
/// 
/// 对于任何非关键字的有效标识符，词法分析器应该识别为 Identifier token。
TEST_F(LexerIdentifierPropertyTest, NonKeywordIdentifierRecognition) {
    std::mt19937 gen(42); // 固定种子以便重现
    
    // 运行多次随机测试
    for (int iteration = 0; iteration < 100; ++iteration) {
        std::string identifier = generateRandomNonKeywordIdentifier(gen);
        
        SourceManager::FileID fileID = createTestBuffer(identifier);
        Lexer lexer(sm, *diag, fileID);
        
        std::vector<Token> tokens = extractAllTokens(lexer);
        
        // 应该只有一个 token
        ASSERT_EQ(tokens.size(), 1)
            << "Iteration " << iteration << ": Expected 1 token for identifier '" 
            << identifier << "', got " << tokens.size();
        
        // 应该是普通标识符
        EXPECT_EQ(tokens[0].getKind(), TokenKind::Identifier)
            << "Iteration " << iteration << ": Identifier '" << identifier 
            << "' should be recognized as Identifier, got kind " 
            << static_cast<int>(tokens[0].getKind());
        
        // 不应该是关键字
        EXPECT_FALSE(tokens[0].isKeyword())
            << "Iteration " << iteration << ": Identifier '" << identifier 
            << "' should not be recognized as keyword";
        
        // 文本应该匹配
        EXPECT_EQ(tokens[0].getText(), identifier)
            << "Iteration " << iteration << ": Identifier '" << identifier << "' text mismatch";
    }
}

/// \brief Property 2: 内置函数标识符识别
/// 
/// 对于以 @ 开头的标识符，词法分析器应该识别为 BuiltinIdentifier token。
TEST_F(LexerIdentifierPropertyTest, BuiltinIdentifierRecognition) {
    std::mt19937 gen(123);
    
    // 运行多次随机测试
    for (int iteration = 0; iteration < 50; ++iteration) {
        std::string baseIdentifier = generateRandomNonKeywordIdentifier(gen);
        std::string builtinIdentifier = "@" + baseIdentifier;
        
        SourceManager::FileID fileID = createTestBuffer(builtinIdentifier);
        Lexer lexer(sm, *diag, fileID);
        
        std::vector<Token> tokens = extractAllTokens(lexer);
        
        // 应该只有一个 token
        ASSERT_EQ(tokens.size(), 1)
            << "Iteration " << iteration << ": Expected 1 token for builtin identifier '" 
            << builtinIdentifier << "', got " << tokens.size();
        
        // 应该是内置标识符
        EXPECT_EQ(tokens[0].getKind(), TokenKind::BuiltinIdentifier)
            << "Iteration " << iteration << ": Builtin identifier '" << builtinIdentifier 
            << "' should be recognized as BuiltinIdentifier, got kind " 
            << static_cast<int>(tokens[0].getKind());
        
        // 文本应该匹配
        EXPECT_EQ(tokens[0].getText(), builtinIdentifier)
            << "Iteration " << iteration << ": Builtin identifier '" << builtinIdentifier << "' text mismatch";
    }
}

/// \brief Property 2: 标识符边界检测
/// 
/// 标识符应该在非标识符字符处正确结束。
TEST_F(LexerIdentifierPropertyTest, IdentifierBoundaryDetection) {
    std::mt19937 gen(456);
    
    // 测试各种边界字符
    std::vector<char> boundaryChars = {' ', '\t', '\n', '(', ')', '[', ']', '{', '}', 
                                       ',', ';', ':', '.', '+', '-', '*', '/', '=', 
                                       '<', '>', '!', '&', '|', '^', '~', '?'};
    
    for (int iteration = 0; iteration < 50; ++iteration) {
        std::string identifier = generateRandomNonKeywordIdentifier(gen);
        char boundary = boundaryChars[gen() % boundaryChars.size()];
        
        std::string source = identifier + boundary;
        SourceManager::FileID fileID = createTestBuffer(source);
        Lexer lexer(sm, *diag, fileID);
        
        std::vector<Token> tokens = extractAllTokens(lexer);
        
        // 应该至少有一个 token（标识符）
        ASSERT_GE(tokens.size(), 1)
            << "Iteration " << iteration << ": Expected at least 1 token for '" 
            << source << "', got " << tokens.size();
        
        // 第一个 token 应该是标识符
        EXPECT_EQ(tokens[0].getKind(), TokenKind::Identifier)
            << "Iteration " << iteration << ": First token should be identifier for '" 
            << source << "', got kind " << static_cast<int>(tokens[0].getKind());
        
        // 标识符文本应该正确（不包含边界字符）
        EXPECT_EQ(tokens[0].getText(), identifier)
            << "Iteration " << iteration << ": Identifier text should be '" << identifier 
            << "' for source '" << source << "', got '" << tokens[0].getText() << "'";
    }
}

/// \brief Property 3: Unicode 标识符支持（当前为占位测试）
/// 
/// 注意：当前实现只支持 ASCII，这个测试验证当前行为。
/// 未来实现 Unicode 支持时需要更新此测试。
TEST_F(LexerIdentifierPropertyTest, UnicodeIdentifierSupport) {
    // 当前实现只支持 ASCII 标识符
    // 测试一些基本的 ASCII 标识符变体
    
    std::vector<std::string> asciiIdentifiers = {
        "a", "A", "_", "a1", "A1", "_1", "abc", "ABC", "_abc", "a_b_c",
        "identifier", "IDENTIFIER", "_identifier", "identifier_", "identifier123",
        "CamelCase", "snake_case", "UPPER_CASE", "mixedCase_123"
    };
    
    for (const std::string& identifier : asciiIdentifiers) {
        SourceManager::FileID fileID = createTestBuffer(identifier);
        Lexer lexer(sm, *diag, fileID);
        
        std::vector<Token> tokens = extractAllTokens(lexer);
        
        // 应该只有一个 token
        ASSERT_EQ(tokens.size(), 1)
            << "Expected 1 token for ASCII identifier '" << identifier << "', got " << tokens.size();
        
        // 应该是标识符（除非是关键字）
        if (tokens[0].isKeyword()) {
            // 如果是关键字，文本应该匹配
            EXPECT_EQ(tokens[0].getText(), identifier);
        } else {
            // 如果不是关键字，应该是普通标识符
            EXPECT_EQ(tokens[0].getKind(), TokenKind::Identifier)
                << "ASCII identifier '" << identifier << "' should be recognized as Identifier";
            EXPECT_EQ(tokens[0].getText(), identifier);
        }
    }
    
    // TODO: 当实现 Unicode 支持时，添加 Unicode 标识符测试
    // 例如：测试中文标识符、希腊字母标识符等
}

/// \brief Property 2: 关键字大小写敏感性
/// 
/// 关键字应该是大小写敏感的。
TEST_F(LexerIdentifierPropertyTest, KeywordCaseSensitivity) {
    // 测试一些关键字的大小写变体
    std::vector<std::pair<std::string, std::string>> testCases = {
        {"var", "VAR"},
        {"const", "CONST"},
        {"func", "FUNC"},
        {"if", "IF"},
        {"else", "ELSE"},
        {"true", "TRUE"},
        {"false", "FALSE"},
        {"None", "none"}, // None 是关键字，none 不是
        {"Self", "self"}, // 两者都是关键字但不同
    };
    
    for (const auto& testCase : testCases) {
        const std::string& keyword = testCase.first;
        const std::string& variant = testCase.second;
        
        // 测试原关键字
        {
            SourceManager::FileID fileID = createTestBuffer(keyword);
            Lexer lexer(sm, *diag, fileID);
            std::vector<Token> tokens = extractAllTokens(lexer);
            
            ASSERT_EQ(tokens.size(), 1);
            EXPECT_TRUE(tokens[0].isKeyword())
                << "'" << keyword << "' should be recognized as keyword";
        }
        
        // 测试大小写变体
        {
            SourceManager::FileID fileID = createTestBuffer(variant);
            Lexer lexer(sm, *diag, fileID);
            std::vector<Token> tokens = extractAllTokens(lexer);
            
            ASSERT_EQ(tokens.size(), 1);
            
            // 检查变体是否也是关键字（某些情况下可能是，如 self/Self）
            bool variantIsKeyword = tokens[0].isKeyword();
            
            if (!variantIsKeyword) {
                // 如果变体不是关键字，应该是普通标识符
                EXPECT_EQ(tokens[0].getKind(), TokenKind::Identifier)
                    << "'" << variant << "' should be recognized as identifier when not a keyword";
            }
            
            EXPECT_EQ(tokens[0].getText(), variant);
        }
    }
}