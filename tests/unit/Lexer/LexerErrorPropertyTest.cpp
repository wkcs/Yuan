/// \file LexerErrorPropertyTest.cpp
/// \brief 词法分析器错误处理属性测试
///
/// 这些测试验证词法分析器在遇到无效字符和错误情况时的行为。
/// 使用基于属性的测试方法来确保错误处理的正确性和一致性。

#include <gtest/gtest.h>
#include "yuan/Lexer/Lexer.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/TextDiagnosticPrinter.h"
#include <random>
#include <sstream>

namespace yuan {

/// \brief 词法分析器错误处理属性测试类
class LexerErrorPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建源码管理器
        SM = std::make_unique<SourceManager>();
        
        // 创建诊断输出流
        DiagStream = std::make_unique<std::ostringstream>();
        
        // 创建诊断引擎
        DiagEngine = std::make_unique<DiagnosticEngine>(*SM);
        
        // 创建并设置诊断打印器
        auto printer = std::make_unique<TextDiagnosticPrinter>(*DiagStream, *SM);
        DiagEngine->setConsumer(std::move(printer));
    }
    
    void TearDown() override {
        DiagEngine.reset();
        DiagStream.reset();
        SM.reset();
    }
    
    /// \brief 创建包含指定内容的词法分析器
    /// \param content 源码内容
    /// \return 词法分析器实例
    std::unique_ptr<Lexer> createLexer(const std::string& content) {
        auto fileID = SM->createBuffer(content, "test.yu");
        return std::make_unique<Lexer>(*SM, *DiagEngine, fileID);
    }
    
    /// \brief 获取诊断输出内容
    /// \return 诊断输出字符串
    std::string getDiagnosticOutput() {
        return DiagStream->str();
    }
    
    /// \brief 清空诊断输出
    void clearDiagnosticOutput() {
        DiagStream->str("");
        DiagStream->clear();
        DiagEngine->reset();
    }
    
    /// \brief 生成随机无效字符
    /// \param gen 随机数生成器
    /// \return 无效字符
    char generateInvalidChar(std::mt19937& gen) {
        // 生成一些在 Yuan 语言中无效的字符
        static const std::vector<char> invalidChars = {
            '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07', '\x08',
            '\x0B', '\x0C', '\x0E', '\x0F', '\x10', '\x11', '\x12', '\x13',
            '\x14', '\x15', '\x16', '\x17', '\x18', '\x19', '\x1A', '\x1B',
            '\x1C', '\x1D', '\x1E', '\x1F', '\x7F',
            '$', '#', '`', '\\', // 在 Yuan 中无效的 ASCII 字符
        };
        
        std::uniform_int_distribution<size_t> dist(0, invalidChars.size() - 1);
        return invalidChars[dist(gen)];
    }
    
    std::unique_ptr<std::ostringstream> DiagStream;
    std::unique_ptr<DiagnosticEngine> DiagEngine;
    std::unique_ptr<SourceManager> SM;
};

/// \brief 属性测试：无效字符报告错误
/// 验证词法分析器遇到无效字符时能正确报告错误
TEST_F(LexerErrorPropertyTest, InvalidCharacterReporting) {
    std::mt19937 gen(42); // 固定种子以确保可重现性
    
    for (int i = 0; i < 50; ++i) {
        clearDiagnosticOutput();
        
        // 生成包含无效字符的源码
        char invalidChar = generateInvalidChar(gen);
        std::string content = "var x = 10";
        content += invalidChar;
        content += " var y = 20";
        
        auto lexer = createLexer(content);
        
        // 词法分析直到遇到无效字符
        Token token;
        do {
            token = lexer->lex();
        } while (token.getKind() != TokenKind::Invalid && 
                 token.getKind() != TokenKind::EndOfFile);
        
        // 验证报告了错误
        EXPECT_GT(DiagEngine->getErrorCount(), 0u) 
            << "应该报告无效字符错误，字符: 0x" << std::hex << (int)(unsigned char)invalidChar;
        
        // 验证错误输出包含错误信息
        std::string diagOutput = getDiagnosticOutput();
        EXPECT_FALSE(diagOutput.empty()) 
            << "应该有诊断输出，字符: 0x" << std::hex << (int)(unsigned char)invalidChar;
        
        // 验证错误代码
        EXPECT_TRUE(diagOutput.find("E1001") != std::string::npos)
            << "应该包含无效字符错误代码 E1001，字符: 0x" << std::hex << (int)(unsigned char)invalidChar;
    }
}

/// \brief 属性测试：错误恢复继续分析
/// 验证词法分析器遇到错误后能继续分析后续内容
TEST_F(LexerErrorPropertyTest, ErrorRecoveryAndContinuation) {
    std::mt19937 gen(123); // 固定种子
    
    for (int i = 0; i < 30; ++i) {
        clearDiagnosticOutput();
        
        // 生成包含无效字符的源码，但后面有有效内容
        char invalidChar = generateInvalidChar(gen);
        std::string content = "var x = 10 ";
        content += invalidChar;
        content += " var y = 20";
        
        auto lexer = createLexer(content);
        
        // 收集所有 token
        std::vector<Token> tokens;
        Token token;
        do {
            token = lexer->lex();
            tokens.push_back(token);
        } while (token.getKind() != TokenKind::EndOfFile);
        
        // 验证报告了错误
        EXPECT_GT(DiagEngine->getErrorCount(), 0u) 
            << "应该报告无效字符错误";
        
        // 验证能继续分析后续内容
        bool foundSecondVar = false;
        bool foundIdentifierY = false;
        bool foundNumber20 = false;
        
        for (const auto& t : tokens) {
            if (t.getKind() == TokenKind::KW_var && t.getText() == "var") {
                // 检查是否是第二个 var（通过位置判断）
                if (t.getLocation().getOffset() > 10) {
                    foundSecondVar = true;
                }
            } else if (t.getKind() == TokenKind::Identifier && t.getText() == "y") {
                foundIdentifierY = true;
            } else if (t.getKind() == TokenKind::IntegerLiteral && t.getText() == "20") {
                foundNumber20 = true;
            }
        }
        
        EXPECT_TRUE(foundSecondVar) 
            << "应该能继续分析第二个 var 关键字";
        EXPECT_TRUE(foundIdentifierY) 
            << "应该能继续分析标识符 y";
        EXPECT_TRUE(foundNumber20) 
            << "应该能继续分析数字 20";
    }
}

/// \brief 属性测试：未终止字符串错误报告
/// 验证未终止字符串的错误处理
TEST_F(LexerErrorPropertyTest, UnterminatedStringErrorReporting) {
    std::mt19937 gen(456);
    
    for (int i = 0; i < 20; ++i) {
        clearDiagnosticOutput();
        
        // 生成随机字符串内容（不包含引号和换行）
        std::uniform_int_distribution<int> lengthDist(1, 20);
        std::uniform_int_distribution<int> charDist(32, 126); // 可打印 ASCII 字符
        
        int length = lengthDist(gen);
        std::string stringContent;
        for (int j = 0; j < length; ++j) {
            char c = static_cast<char>(charDist(gen));
            if (c != '"' && c != '\n' && c != '\r' && c != '\\') {  // 也排除反斜杠
                stringContent += c;
            }
        }
        
        // 创建未终止的字符串
        std::string content = "var s = \"" + stringContent; // 缺少结束引号
        
        auto lexer = createLexer(content);
        
        // 词法分析
        Token token;
        do {
            token = lexer->lex();
        } while (token.getKind() != TokenKind::Invalid && 
                 token.getKind() != TokenKind::EndOfFile);
        
        // 验证报告了未终止字符串错误
        EXPECT_GT(DiagEngine->getErrorCount(), 0u) 
            << "应该报告未终止字符串错误，内容: " << content;
        
        std::string diagOutput = getDiagnosticOutput();
        EXPECT_TRUE(diagOutput.find("E1002") != std::string::npos)
            << "应该包含未终止字符串错误代码 E1002，内容: " << content 
            << "，诊断输出: " << diagOutput;
    }
}

/// \brief 属性测试：未终止字符错误报告
/// 验证未终止字符字面量的错误处理
TEST_F(LexerErrorPropertyTest, UnterminatedCharErrorReporting) {
    std::mt19937 gen(789);
    
    for (int i = 0; i < 15; ++i) {
        clearDiagnosticOutput();
        
        // 生成随机字符内容
        std::uniform_int_distribution<int> charDist(32, 126);
        char charContent = static_cast<char>(charDist(gen));
        
        // 避免单引号
        if (charContent == '\'') {
            charContent = 'A';
        }
        
        // 创建未终止的字符字面量
        std::string content = "var c = '";
        content += charContent; // 缺少结束单引号
        
        auto lexer = createLexer(content);
        
        // 词法分析
        Token token;
        do {
            token = lexer->lex();
        } while (token.getKind() != TokenKind::Invalid && 
                 token.getKind() != TokenKind::EndOfFile);
        
        // 验证报告了未终止字符错误
        EXPECT_GT(DiagEngine->getErrorCount(), 0u) 
            << "应该报告未终止字符错误";
        
        std::string diagOutput = getDiagnosticOutput();
        EXPECT_TRUE(diagOutput.find("E1003") != std::string::npos)
            << "应该包含未终止字符错误代码 E1003";
    }
}

/// \brief 属性测试：无效转义序列错误报告
/// 验证无效转义序列的错误处理
TEST_F(LexerErrorPropertyTest, InvalidEscapeSequenceErrorReporting) {
    std::mt19937 gen(101112);
    
    // 无效的转义字符
    std::vector<char> invalidEscapes = {'a', 'b', 'c', 'd', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'p', 'q', 's', 'v', 'w', 'y', 'z'};
    
    for (int i = 0; i < 10; ++i) {
        clearDiagnosticOutput();
        
        // 选择一个无效的转义字符
        std::uniform_int_distribution<size_t> dist(0, invalidEscapes.size() - 1);
        char invalidEscape = invalidEscapes[dist(gen)];
        
        // 创建包含无效转义序列的字符串
        std::string content = "var s = \"\\";
        content += invalidEscape;
        content += "\"";
        
        auto lexer = createLexer(content);
        
        // 词法分析
        Token token;
        do {
            token = lexer->lex();
        } while (token.getKind() != TokenKind::Invalid && 
                 token.getKind() != TokenKind::EndOfFile);
        
        // 验证报告了无效转义序列错误
        EXPECT_GT(DiagEngine->getErrorCount(), 0u) 
            << "应该报告无效转义序列错误，转义字符: \\" << invalidEscape;
        
        std::string diagOutput = getDiagnosticOutput();
        EXPECT_TRUE(diagOutput.find("E1004") != std::string::npos)
            << "应该包含无效转义序列错误代码 E1004，转义字符: \\" << invalidEscape;
    }
}

/// \brief 属性测试：无效数字字面量错误报告
/// 验证无效数字字面量的错误处理
TEST_F(LexerErrorPropertyTest, InvalidNumberLiteralErrorReporting) {
    std::vector<std::string> invalidNumbers = {
        "0x",      // 十六进制前缀但没有数字
        "0o",      // 八进制前缀但没有数字
        "0b",      // 二进制前缀但没有数字
        "123abc",  // 无效的类型后缀
        "0x123xyz", // 十六进制数字后跟无效后缀
        "1.23e",   // 科学计数法缺少指数
        "1.23e+",  // 科学计数法指数符号后没有数字
    };
    
    for (const auto& invalidNum : invalidNumbers) {
        clearDiagnosticOutput();
        
        std::string content = "var n = " + invalidNum;
        auto lexer = createLexer(content);
        
        // 词法分析
        Token token;
        do {
            token = lexer->lex();
        } while (token.getKind() != TokenKind::Invalid && 
                 token.getKind() != TokenKind::EndOfFile);
        
        // 验证报告了无效数字字面量错误
        EXPECT_GT(DiagEngine->getErrorCount(), 0u) 
            << "应该报告无效数字字面量错误: " << invalidNum;
        
        std::string diagOutput = getDiagnosticOutput();
        EXPECT_TRUE(diagOutput.find("E1005") != std::string::npos)
            << "应该包含无效数字字面量错误代码 E1005: " << invalidNum;
    }
}

/// \brief 属性测试：错误位置准确性
/// 验证错误报告的位置信息是否准确
TEST_F(LexerErrorPropertyTest, ErrorLocationAccuracy) {
    std::mt19937 gen(131415);
    
    for (int i = 0; i < 20; ++i) {
        // 为每次测试创建新的 SourceManager 以重置偏移量
        auto localSM = std::make_unique<SourceManager>();
        auto localDiagStream = std::make_unique<std::ostringstream>();
        auto localDiagEngine = std::make_unique<DiagnosticEngine>(*localSM);
        auto printer = std::make_unique<TextDiagnosticPrinter>(*localDiagStream, *localSM);
        localDiagEngine->setConsumer(std::move(printer));
        
        // 生成随机前缀内容
        std::uniform_int_distribution<int> prefixLengthDist(5, 20);
        int prefixLength = prefixLengthDist(gen);
        
        std::string prefix = "var x = ";
        for (int j = 0; j < prefixLength - 8; ++j) {
            prefix += " ";
        }
        
        // 添加无效字符
        char invalidChar = generateInvalidChar(gen);
        std::string content = prefix + invalidChar + " var y = 20";
        
        auto fileID = localSM->createBuffer(content, "test.yu");
        Lexer lexer(*localSM, *localDiagEngine, fileID);
        
        // 词法分析直到遇到错误
        Token token;
        do {
            token = lexer.lex();
        } while (token.getKind() != TokenKind::Invalid && 
                 token.getKind() != TokenKind::EndOfFile);
        
        // 验证错误位置
        if (token.getKind() == TokenKind::Invalid) {
            // 由于我们使用了新的 SourceManager，偏移量应该从 1 开始
            // 期望的偏移量是 1 + prefix.length()
            size_t expectedOffset = 1 + prefix.length();
            size_t actualOffset = token.getLocation().getOffset();
            
            EXPECT_EQ(expectedOffset, actualOffset)
                << "错误位置应该准确指向无效字符的位置";
        }
    }
}

/// \brief 属性测试：多个错误累积
/// 验证多个错误能够正确累积和报告
TEST_F(LexerErrorPropertyTest, MultipleErrorAccumulation) {
    clearDiagnosticOutput();
    
    // 创建包含多个错误的源码
    std::string content = "var x = 10 \x01 var y = \"unterminated \x02 var z = '\x03";
    
    auto lexer = createLexer(content);
    
    // 词法分析所有内容
    Token token;
    do {
        token = lexer->lex();
    } while (token.getKind() != TokenKind::EndOfFile);
    
    // 验证报告了多个错误
    EXPECT_GE(DiagEngine->getErrorCount(), 2u) 
        << "应该报告多个错误";
    
    std::string diagOutput = getDiagnosticOutput();
    
    // 验证包含不同类型的错误代码
    int errorCodeCount = 0;
    if (diagOutput.find("E1001") != std::string::npos) errorCodeCount++; // 无效字符
    if (diagOutput.find("E1002") != std::string::npos) errorCodeCount++; // 未终止字符串
    if (diagOutput.find("E1003") != std::string::npos) errorCodeCount++; // 未终止字符
    
    EXPECT_GE(errorCodeCount, 2) 
        << "应该包含多种类型的错误代码";
}

} // namespace yuan