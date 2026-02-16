/// \file LexerCommentPropertyTest.cpp
/// \brief Property-based tests for Lexer comment handling.
///
/// **Feature: yuan-compiler, Property 6: Lexer 注释跳过**
/// **Validates: Requirements 2.8**

#include <gtest/gtest.h>
#include "yuan/Lexer/Lexer.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/TextDiagnosticPrinter.h"
#include <sstream>
#include <random>
#include <vector>
#include <string>

using namespace yuan;

class LexerCommentPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        consumer = std::make_unique<StoredDiagnosticConsumer>();
        diag = std::make_unique<DiagnosticEngine>(sm);
        diag->setConsumer(std::make_unique<StoredDiagnosticConsumer>());
    }
    
    void TearDown() override {
        // 清理
    }
    
    /// \brief 创建包含注释的测试源码
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
    
    /// \brief 生成随机注释内容
    /// \param gen 随机数生成器
    /// \return 随机注释内容
    std::string generateRandomCommentContent(std::mt19937& gen) {
        std::uniform_int_distribution<> lengthDist(0, 50);
        std::uniform_int_distribution<> charDist(32, 126); // 可打印 ASCII 字符
        
        int length = lengthDist(gen);
        std::string content;
        content.reserve(length);
        
        for (int i = 0; i < length; ++i) {
            char c = static_cast<char>(charDist(gen));
            // 避免生成会干扰注释结构的字符
            if (c == '*' || c == '/' || c == '\n' || c == '\r') {
                c = ' ';
            }
            content += c;
        }
        
        return content;
    }
    
protected:
    SourceManager sm;
    std::unique_ptr<StoredDiagnosticConsumer> consumer;
    std::unique_ptr<DiagnosticEngine> diag;
};

/// \brief Property 6: Lexer 注释跳过
/// 
/// 对于任何包含行注释的源码，词法分析器应该跳过注释内容，
/// 只返回注释前后的有效 token。
TEST_F(LexerCommentPropertyTest, LineCommentSkipping) {
    std::mt19937 gen(42); // 固定种子以便重现
    
    // 运行多次随机测试
    for (int iteration = 0; iteration < 100; ++iteration) {
        // 生成随机的标识符和注释内容
        std::string beforeComment = generateRandomIdentifier(gen);
        std::string commentContent = generateRandomCommentContent(gen);
        std::string afterComment = generateRandomIdentifier(gen);
        
        // 测试普通行注释 //
        {
            std::string source = beforeComment + " // " + commentContent + "\n" + afterComment;
            SourceManager::FileID fileID = createTestBuffer(source);
            Lexer lexer(sm, *diag, fileID);
            
            std::vector<Token> tokens = extractAllTokens(lexer);
            
            // 应该只有两个标识符 token，注释被跳过
            ASSERT_EQ(tokens.size(), 2) 
                << "Iteration " << iteration << ": Expected 2 tokens, got " << tokens.size()
                << " for source: " << source;
            
            EXPECT_TRUE(tokens[0].getKind() == TokenKind::Identifier || tokens[0].isKeyword());
            EXPECT_EQ(tokens[0].getText(), beforeComment);
            EXPECT_TRUE(tokens[1].getKind() == TokenKind::Identifier || tokens[1].isKeyword());
            EXPECT_EQ(tokens[1].getText(), afterComment);
        }
        
        // 测试文档注释 ///
        {
            std::string source = beforeComment + " /// " + commentContent + "\n" + afterComment;
            SourceManager::FileID fileID = createTestBuffer(source);
            Lexer lexer(sm, *diag, fileID);
            
            std::vector<Token> tokens = extractAllTokens(lexer);
            
            // 应该只有两个标识符 token，文档注释也被跳过
            ASSERT_EQ(tokens.size(), 2)
                << "Iteration " << iteration << ": Expected 2 tokens for doc comment, got " << tokens.size()
                << " for source: " << source;
            
            EXPECT_TRUE(tokens[0].getKind() == TokenKind::Identifier || tokens[0].isKeyword());
            EXPECT_EQ(tokens[0].getText(), beforeComment);
            EXPECT_TRUE(tokens[1].getKind() == TokenKind::Identifier || tokens[1].isKeyword());
            EXPECT_EQ(tokens[1].getText(), afterComment);
        }
    }
}

/// \brief Property 6: Lexer 块注释跳过
/// 
/// 对于任何包含块注释的源码，词法分析器应该跳过注释内容，
/// 只返回注释前后的有效 token。
TEST_F(LexerCommentPropertyTest, BlockCommentSkipping) {
    std::mt19937 gen(123); // 不同的种子
    
    // 运行多次随机测试
    for (int iteration = 0; iteration < 100; ++iteration) {
        // 生成随机的标识符和注释内容
        std::string beforeComment = generateRandomIdentifier(gen);
        std::string commentContent = generateRandomCommentContent(gen);
        std::string afterComment = generateRandomIdentifier(gen);
        
        // 测试块注释 /* ... */
        std::string source = beforeComment + " /* " + commentContent + " */ " + afterComment;
        SourceManager::FileID fileID = createTestBuffer(source);
        Lexer lexer(sm, *diag, fileID);
        
        std::vector<Token> tokens = extractAllTokens(lexer);
        
        // 应该只有两个标识符 token，块注释被跳过
        ASSERT_EQ(tokens.size(), 2)
            << "Iteration " << iteration << ": Expected 2 tokens, got " << tokens.size()
            << " for source: " << source;
        
        // 第一个 token 应该是标识符或关键字，文本应该匹配
        EXPECT_TRUE(tokens[0].getKind() == TokenKind::Identifier || tokens[0].isKeyword())
            << "Iteration " << iteration << ": First token should be identifier or keyword, got " 
            << static_cast<int>(tokens[0].getKind()) << " for source: " << source;
        EXPECT_EQ(tokens[0].getText(), beforeComment);
        
        // 第二个 token 应该是标识符或关键字，文本应该匹配
        EXPECT_TRUE(tokens[1].getKind() == TokenKind::Identifier || tokens[1].isKeyword())
            << "Iteration " << iteration << ": Second token should be identifier or keyword, got " 
            << static_cast<int>(tokens[1].getKind()) << " for source: " << source;
        EXPECT_EQ(tokens[1].getText(), afterComment);
    }
}

/// \brief Property 6: 多行块注释跳过
/// 
/// 块注释可以跨越多行，词法分析器应该正确跳过所有内容。
TEST_F(LexerCommentPropertyTest, MultilineBlockCommentSkipping) {
    std::mt19937 gen(456);
    
    for (int iteration = 0; iteration < 50; ++iteration) {
        std::string beforeComment = generateRandomIdentifier(gen);
        std::string afterComment = generateRandomIdentifier(gen);
        
        // 生成多行注释内容
        std::string commentContent;
        int numLines = 1 + (gen() % 5); // 1-5 行
        for (int line = 0; line < numLines; ++line) {
            if (line > 0) commentContent += "\n";
            commentContent += generateRandomCommentContent(gen);
        }
        
        std::string source = beforeComment + " /*\n" + commentContent + "\n*/ " + afterComment;
        SourceManager::FileID fileID = createTestBuffer(source);
        Lexer lexer(sm, *diag, fileID);
        
        std::vector<Token> tokens = extractAllTokens(lexer);
        
        // 应该只有两个标识符 token
        ASSERT_EQ(tokens.size(), 2)
            << "Iteration " << iteration << ": Expected 2 tokens for multiline comment, got " << tokens.size();
        
        EXPECT_TRUE(tokens[0].getKind() == TokenKind::Identifier || tokens[0].isKeyword());
        EXPECT_EQ(tokens[0].getText(), beforeComment);
        EXPECT_TRUE(tokens[1].getKind() == TokenKind::Identifier || tokens[1].isKeyword());
        EXPECT_EQ(tokens[1].getText(), afterComment);
    }
}

/// \brief Property 6: 嵌套和连续注释处理
/// 
/// 测试多个连续注释和不同类型注释的组合。
TEST_F(LexerCommentPropertyTest, ConsecutiveCommentsSkipping) {
    std::mt19937 gen(789);
    
    for (int iteration = 0; iteration < 50; ++iteration) {
        std::string beforeComments = generateRandomIdentifier(gen);
        std::string afterComments = generateRandomIdentifier(gen);
        
        // 生成多个连续注释
        std::string comments;
        int numComments = 1 + (gen() % 4); // 1-4 个注释
        
        for (int i = 0; i < numComments; ++i) {
            std::string content = generateRandomCommentContent(gen);
            
            if (gen() % 2 == 0) {
                // 行注释
                comments += " // " + content + "\n";
            } else {
                // 块注释
                comments += " /* " + content + " */";
            }
        }
        
        std::string source = beforeComments + comments + " " + afterComments;
        SourceManager::FileID fileID = createTestBuffer(source);
        Lexer lexer(sm, *diag, fileID);
        
        std::vector<Token> tokens = extractAllTokens(lexer);
        
        // 应该只有两个标识符 token，所有注释都被跳过
        ASSERT_EQ(tokens.size(), 2)
            << "Iteration " << iteration << ": Expected 2 tokens for consecutive comments, got " << tokens.size();
        
        EXPECT_TRUE(tokens[0].getKind() == TokenKind::Identifier || tokens[0].isKeyword());
        EXPECT_EQ(tokens[0].getText(), beforeComments);
        EXPECT_TRUE(tokens[1].getKind() == TokenKind::Identifier || tokens[1].isKeyword());
        EXPECT_EQ(tokens[1].getText(), afterComments);
    }
}

/// \brief Property 6: 空注释处理
/// 
/// 测试空注释（只有注释标记，没有内容）的处理。
TEST_F(LexerCommentPropertyTest, EmptyCommentsSkipping) {
    // 测试空行注释
    {
        std::string source = "before //\nafter";
        SourceManager::FileID fileID = createTestBuffer(source);
        Lexer lexer(sm, *diag, fileID);
        
        std::vector<Token> tokens = extractAllTokens(lexer);
        
        ASSERT_EQ(tokens.size(), 2);
        EXPECT_EQ(tokens[0].getText(), "before");
        EXPECT_EQ(tokens[1].getText(), "after");
    }
    
    // 测试空文档注释
    {
        std::string source = "before ///\nafter";
        SourceManager::FileID fileID = createTestBuffer(source);
        Lexer lexer(sm, *diag, fileID);
        
        std::vector<Token> tokens = extractAllTokens(lexer);
        
        ASSERT_EQ(tokens.size(), 2);
        EXPECT_EQ(tokens[0].getText(), "before");
        EXPECT_EQ(tokens[1].getText(), "after");
    }
    
    // 测试空块注释
    {
        std::string source = "before /**/ after";
        SourceManager::FileID fileID = createTestBuffer(source);
        Lexer lexer(sm, *diag, fileID);
        
        std::vector<Token> tokens = extractAllTokens(lexer);
        
        ASSERT_EQ(tokens.size(), 2);
        EXPECT_EQ(tokens[0].getText(), "before");
        EXPECT_EQ(tokens[1].getText(), "after");
    }
}