/// \file LiteralParser.cpp
/// \brief Implementation of literal parsing utilities.

#include "yuan/Lexer/LiteralParser.h"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>

namespace yuan {

bool LiteralParser::parseInteger(const std::string& text, 
                                uint64_t& value, 
                                bool& isSigned,
                                unsigned& bitWidth,
                                bool* hasTypeSuffix,
                                bool* isPointerSizedSuffix) {
    if (text.empty()) {
        return false;
    }
    
    const char* start = text.c_str();
    const char* end = start + text.length();
    const char* ptr = start;
    
    // 默认值
    value = 0;
    isSigned = true;  // 默认有符号
    bitWidth = 0;     // 未指定
    if (hasTypeSuffix) {
        *hasTypeSuffix = false;
    }
    if (isPointerSizedSuffix) {
        *isPointerSizedSuffix = false;
    }
    
    // 检查进制前缀
    int base = 10;
    if (ptr + 1 < end && *ptr == '0') {
        char prefix = *(ptr + 1);
        if (prefix == 'x' || prefix == 'X') {
            base = 16;
            ptr += 2;
        } else if (prefix == 'o' || prefix == 'O') {
            base = 8;
            ptr += 2;
        } else if (prefix == 'b' || prefix == 'B') {
            base = 2;
            ptr += 2;
        }
    }
    
    // 找到数字部分的结束位置（排除类型后缀）
    const char* digitEnd = ptr;
    while (digitEnd < end && (isValidDigit(*digitEnd, base) || *digitEnd == '_')) {
        digitEnd++;
    }
    
    // 解析数字部分
    if (!parseDigits(ptr, digitEnd, base, value)) {
        return false;
    }
    
    // 解析类型后缀
    if (digitEnd < end) {
        std::string suffix(digitEnd, end - digitEnd);
        if (hasTypeSuffix) {
            *hasTypeSuffix = true;
        }
        if (isPointerSizedSuffix && (suffix == "isize" || suffix == "usize")) {
            *isPointerSizedSuffix = true;
        }
        if (!parseTypeSuffix(suffix, false, isSigned, bitWidth)) {
            return false;
        }
    }
    
    return true;
}

bool LiteralParser::parseFloat(const std::string& text,
                              double& value,
                              unsigned& bitWidth) {
    if (text.empty()) {
        return false;
    }
    
    // 默认值
    value = 0.0;
    bitWidth = 0;  // 未指定
    
    // 找到后缀的开始位置
    // 从后往前找，找到最后一个连续的字母序列，但要排除科学计数法中的 'e' 或 'E'
    size_t suffixStart = text.length();
    bool foundLetter = false;
    
    for (int i = text.length() - 1; i >= 0; i--) {
        char c = text[i];
        if (std::isalpha(c)) {
            // 检查是否是科学计数法中的 'e' 或 'E'
            if ((c == 'e' || c == 'E') && i > 0) {
                // 检查前面是否有数字，后面是否有数字或符号
                bool hasDigitBefore = false;
                bool hasDigitOrSignAfter = false;
                
                // 检查前面是否有数字
                for (int j = i - 1; j >= 0; j--) {
                    if (std::isdigit(text[j]) || text[j] == '.') {
                        hasDigitBefore = true;
                        break;
                    } else if (text[j] == '_') {
                        continue; // 跳过下划线
                    } else {
                        break;
                    }
                }
                
                // 检查后面是否有数字或符号
                if (static_cast<size_t>(i + 1) < text.length()) {
                    char next = text[i + 1];
                    if (std::isdigit(next) || next == '+' || next == '-') {
                        hasDigitOrSignAfter = true;
                    }
                }
                
                // 如果这是科学计数法的一部分，不作为后缀处理
                if (hasDigitBefore && hasDigitOrSignAfter) {
                    if (foundLetter) {
                        // 我们已经找到了真正的后缀，现在遇到科学计数法，停止
                        break;
                    }
                    // 否则继续寻找
                    continue;
                }
            }
            
            if (!foundLetter) {
                foundLetter = true;
            }
            suffixStart = i;
        } else {
            if (foundLetter) {
                // 我们已经找到了字母序列，现在遇到非字母字符，停止
                break;
            }
            // 否则继续寻找
        }
    }
    
    // 解析数字部分（不包括后缀）
    std::string numberPart = text.substr(0, suffixStart);
    std::string suffix = text.substr(suffixStart);
    
    // 移除下划线分隔符
    numberPart.erase(std::remove(numberPart.begin(), numberPart.end(), '_'), 
                     numberPart.end());
    
    // 使用标准库解析浮点数
    char* endPtr;
    value = std::strtod(numberPart.c_str(), &endPtr);
    
    // 检查是否完全解析
    if (endPtr != numberPart.c_str() + numberPart.length()) {
        return false;
    }
    
    // 解析类型后缀
    if (!suffix.empty()) {
        bool isSigned; // 浮点数总是有符号的，这个参数会被忽略
        if (!parseTypeSuffix(suffix, true, isSigned, bitWidth)) {
            return false;
        }
    }
    
    return true;
}

bool LiteralParser::parseChar(const std::string& text, uint32_t& codepoint) {
    if (text.length() < 3 || text[0] != '\'' || text.back() != '\'') {
        return false;
    }
    
    const char* ptr = text.c_str() + 1;  // 跳过开始的单引号
    const char* end = text.c_str() + text.length() - 1;  // 排除结束的单引号
    
    if (ptr >= end) {
        return false;  // 空字符字面量
    }
    
    if (*ptr == '\\') {
        // 转义字符
        ptr++;
        return parseEscapeSequence(ptr, end, codepoint);
    } else {
        // 普通字符
        codepoint = static_cast<uint32_t>(*ptr);
        ptr++;
        
        // 确保只有一个字符
        return ptr == end;
    }
}

bool LiteralParser::parseString(const std::string& text, std::string& result) {
    result.clear();
    
    if (text.length() < 2) {
        return false;
    }
    
    // 检查字符串类型
    if (text.length() >= 6 && text.substr(0, 3) == "\"\"\"" && 
        text.substr(text.length() - 3) == "\"\"\"") {
        // 多行字符串
        const char* ptr = text.c_str() + 3;  // 跳过开始的三个双引号
        const char* end = text.c_str() + text.length() - 3;  // 排除结束的三个双引号
        
        while (ptr < end) {
            if (*ptr == '\\') {
                // 转义字符
                ptr++;
                uint32_t codepoint;
                if (!parseEscapeSequence(ptr, end, codepoint)) {
                    return false;
                }
                
                // 将Unicode码点转换为UTF-8（同上）
                if (codepoint <= 0x7F) {
                    result += static_cast<char>(codepoint);
                } else if (codepoint <= 0x7FF) {
                    result += static_cast<char>(0xC0 | (codepoint >> 6));
                    result += static_cast<char>(0x80 | (codepoint & 0x3F));
                } else if (codepoint <= 0xFFFF) {
                    result += static_cast<char>(0xE0 | (codepoint >> 12));
                    result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (codepoint & 0x3F));
                } else if (codepoint <= 0x10FFFF) {
                    result += static_cast<char>(0xF0 | (codepoint >> 18));
                    result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                    result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (codepoint & 0x3F));
                } else {
                    return false;
                }
            } else {
                // 普通字符
                result += *ptr;
                ptr++;
            }
        }
        
        return true;
    } else if (text[0] == '"' && text.back() == '"') {
        // 普通字符串
        const char* ptr = text.c_str() + 1;  // 跳过开始的双引号
        const char* end = text.c_str() + text.length() - 1;  // 排除结束的双引号
        
        while (ptr < end) {
            if (*ptr == '\\') {
                // 转义字符
                ptr++;
                uint32_t codepoint;
                if (!parseEscapeSequence(ptr, end, codepoint)) {
                    return false;
                }
                
                // 将Unicode码点转换为UTF-8
                if (codepoint <= 0x7F) {
                    result += static_cast<char>(codepoint);
                } else if (codepoint <= 0x7FF) {
                    result += static_cast<char>(0xC0 | (codepoint >> 6));
                    result += static_cast<char>(0x80 | (codepoint & 0x3F));
                } else if (codepoint <= 0xFFFF) {
                    result += static_cast<char>(0xE0 | (codepoint >> 12));
                    result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (codepoint & 0x3F));
                } else if (codepoint <= 0x10FFFF) {
                    result += static_cast<char>(0xF0 | (codepoint >> 18));
                    result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                    result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (codepoint & 0x3F));
                } else {
                    return false;  // 无效的Unicode码点
                }
            } else {
                // 普通字符
                result += *ptr;
                ptr++;
            }
        }
        
        return true;
    } else if (text[0] == 'r') {
        // 原始字符串 r"..." 或 r###"..."###
        size_t hashCount = 0;
        size_t pos = 1;
        
        // 计算 # 的数量
        while (pos < text.length() && text[pos] == '#') {
            hashCount++;
            pos++;
        }
        
        // 检查格式
        if (pos >= text.length() || text[pos] != '"') {
            return false;
        }
        
        // 检查结束标记
        std::string endMarker = "\"" + std::string(hashCount, '#');
        if (text.length() < pos + 1 + endMarker.length() ||
            text.substr(text.length() - endMarker.length()) != endMarker) {
            return false;
        }
        
        // 提取内容（原始字符串不处理转义）
        size_t contentStart = pos + 1;
        size_t contentEnd = text.length() - endMarker.length();
        result = text.substr(contentStart, contentEnd - contentStart);
        
        return true;
    }
    
    return false;
}

bool LiteralParser::parseEscapeSequence(const char*& ptr, 
                                        const char* end,
                                        uint32_t& result) {
    if (ptr >= end) {
        return false;
    }
    
    char escapeChar = *ptr++;
    
    switch (escapeChar) {
        case 'n':
            result = '\n';
            return true;
        case 't':
            result = '\t';
            return true;
        case 'r':
            result = '\r';
            return true;
        case '\\':
            result = '\\';
            return true;
        case '\'':
            result = '\'';
            return true;
        case '"':
            result = '"';
            return true;
        case '0':
            result = '\0';
            return true;
            
        case 'x': {
            // 十六进制转义 \xNN
            if (ptr + 2 > end) {
                return false;
            }
            
            int digit1 = hexDigitValue(*ptr++);
            int digit2 = hexDigitValue(*ptr++);
            
            if (digit1 < 0 || digit2 < 0) {
                return false;
            }
            
            result = (digit1 << 4) | digit2;
            return true;
        }
        
        case 'u': {
            // Unicode转义 \u{NNNN}
            if (ptr >= end || *ptr != '{') {
                return false;
            }
            ptr++; // 跳过 '{'
            
            result = 0;
            bool hasDigits = false;
            
            while (ptr < end && *ptr != '}') {
                int digit = hexDigitValue(*ptr++);
                if (digit < 0) {
                    return false;
                }
                
                result = (result << 4) | digit;
                hasDigits = true;
                
                // 防止溢出
                if (result > 0x10FFFF) {
                    return false;
                }
            }
            
            if (!hasDigits || ptr >= end || *ptr != '}') {
                return false;
            }
            ptr++; // 跳过 '}'
            
            // 检查是否为有效的Unicode码点
            if (result > 0x10FFFF || (result >= 0xD800 && result <= 0xDFFF)) {
                return false;
            }
            
            return true;
        }
        
        default:
            return false;
    }
}

bool LiteralParser::parseDigits(const char* start, const char* end, 
                               int base, uint64_t& value) {
    value = 0;
    bool hasDigits = false;
    
    const char* ptr = start;
    while (ptr < end) {
        if (*ptr == '_') {
            // 跳过下划线分隔符
            ptr++;
            continue;
        }
        
        if (!isValidDigit(*ptr, base)) {
            break;
        }
        
        int digitValue;
        if (*ptr >= '0' && *ptr <= '9') {
            digitValue = *ptr - '0';
        } else if (*ptr >= 'a' && *ptr <= 'f') {
            digitValue = *ptr - 'a' + 10;
        } else if (*ptr >= 'A' && *ptr <= 'F') {
            digitValue = *ptr - 'A' + 10;
        } else {
            return false;
        }
        
        // 检查溢出
        if (value > (UINT64_MAX - digitValue) / base) {
            return false;
        }
        
        value = value * base + digitValue;
        hasDigits = true;
        ptr++;
    }
    
    return hasDigits && ptr == end;
}

bool LiteralParser::parseTypeSuffix(const std::string& suffix, 
                                   bool isFloat, bool& isSigned, unsigned& bitWidth) {
    if (suffix.empty()) {
        return true;  // 没有后缀是有效的
    }
    
    static const std::unordered_map<std::string, std::pair<bool, unsigned>> intSuffixes = {
        {"i8",    {true,  8}},
        {"i16",   {true,  16}},
        {"i32",   {true,  32}},
        {"i64",   {true,  64}},
        {"i128",  {true,  128}},
        {"isize", {true,  0}},  // 0 表示平台相关
        {"u8",    {false, 8}},
        {"u16",   {false, 16}},
        {"u32",   {false, 32}},
        {"u64",   {false, 64}},
        {"u128",  {false, 128}},
        {"usize", {false, 0}},  // 0 表示平台相关
    };
    
    static const std::unordered_map<std::string, unsigned> floatSuffixes = {
        {"f32", 32},
        {"f64", 64},
    };
    
    if (isFloat) {
        auto it = floatSuffixes.find(suffix);
        if (it == floatSuffixes.end()) {
            return false;
        }
        bitWidth = it->second;
        isSigned = true;  // 浮点数总是有符号的
    } else {
        auto it = intSuffixes.find(suffix);
        if (it == intSuffixes.end()) {
            return false;
        }
        isSigned = it->second.first;
        bitWidth = it->second.second;
    }
    
    return true;
}

int LiteralParser::hexDigitValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    } else {
        return -1;
    }
}

bool LiteralParser::isValidDigit(char c, int base) {
    switch (base) {
        case 2:
            return c == '0' || c == '1';
        case 8:
            return c >= '0' && c <= '7';
        case 10:
            return c >= '0' && c <= '9';
        case 16:
            return (c >= '0' && c <= '9') || 
                   (c >= 'a' && c <= 'f') || 
                   (c >= 'A' && c <= 'F');
        default:
            return false;
    }
}

const char* LiteralParser::skipUnderscores(const char* ptr, const char* end) {
    while (ptr < end && *ptr == '_') {
        ptr++;
    }
    return ptr;
}

} // namespace yuan
