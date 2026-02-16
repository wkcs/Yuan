/// \file LiteralParser.h
/// \brief Literal value parsing utilities.

#ifndef YUAN_LEXER_LITERALPARSER_H
#define YUAN_LEXER_LITERALPARSER_H

#include <string>
#include <cstdint>

namespace yuan {

/// \brief Utilities for parsing literal values.
/// 
/// This class provides static methods to parse various types of literal values
/// from their string representations, handling different bases, escape sequences,
/// and type suffixes according to Yuan language specification.
class LiteralParser {
public:
    /// \brief 解析整数字面量
    /// \param text 要解析的文本（包括可能的前缀和后缀）
    /// \param value 输出参数：解析得到的数值
    /// \param isSigned 输出参数：是否为有符号类型
    /// \param bitWidth 输出参数：位宽（0表示未指定）
    /// \return 解析是否成功
    static bool parseInteger(const std::string& text, 
                            uint64_t& value, 
                            bool& isSigned,
                            unsigned& bitWidth,
                            bool* hasTypeSuffix = nullptr,
                            bool* isPointerSizedSuffix = nullptr);
    
    /// \brief 解析浮点数字面量
    /// \param text 要解析的文本（包括可能的后缀）
    /// \param value 输出参数：解析得到的数值
    /// \param bitWidth 输出参数：位宽（32或64，0表示未指定）
    /// \return 解析是否成功
    static bool parseFloat(const std::string& text,
                          double& value,
                          unsigned& bitWidth);
    
    /// \brief 解析字符字面量
    /// \param text 要解析的文本（包括单引号）
    /// \param codepoint 输出参数：解析得到的Unicode码点
    /// \return 解析是否成功
    static bool parseChar(const std::string& text, uint32_t& codepoint);
    
    /// \brief 解析字符串字面量（处理转义）
    /// \param text 要解析的文本（包括引号）
    /// \param result 输出参数：解析得到的字符串内容
    /// \return 解析是否成功
    static bool parseString(const std::string& text, std::string& result);
    
    /// \brief 解析转义序列
    /// \param ptr 输入输出参数：当前解析位置的指针
    /// \param end 输入结束位置的指针
    /// \param result 输出参数：解析得到的Unicode码点
    /// \return 解析是否成功
    static bool parseEscapeSequence(const char*& ptr, 
                                    const char* end,
                                    uint32_t& result);

private:
    /// \brief 解析数字部分（不包括前缀和后缀）
    static bool parseDigits(const char* start, const char* end, 
                           int base, uint64_t& value);
    
    /// \brief 解析类型后缀
    static bool parseTypeSuffix(const std::string& suffix, 
                               bool isFloat, bool& isSigned, unsigned& bitWidth);
    
    /// \brief 将十六进制字符转换为数值
    static int hexDigitValue(char c);
    
    /// \brief 检查字符是否为有效的数字字符
    static bool isValidDigit(char c, int base);
    
    /// \brief 跳过下划线分隔符
    static const char* skipUnderscores(const char* ptr, const char* end);
};

} // namespace yuan

#endif // YUAN_LEXER_LITERALPARSER_H
