#include <gtest/gtest.h>
#include "yuan/Lexer/LiteralParser.h"

using namespace yuan;

class LiteralParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 设置测试环境
    }
    
    void TearDown() override {
        // 清理测试环境
    }
};

// ============================================================================
// 整数字面量解析测试
// ============================================================================

// 测试基本十进制整数解析
TEST_F(LiteralParserTest, ParseDecimalInteger) {
    uint64_t value;
    bool isSigned;
    unsigned bitWidth;
    
    // 基本十进制数
    EXPECT_TRUE(LiteralParser::parseInteger("42", value, isSigned, bitWidth));
    EXPECT_EQ(value, 42u);
    EXPECT_TRUE(isSigned);  // 默认有符号
    EXPECT_EQ(bitWidth, 0u);  // 未指定位宽
    
    // 零
    EXPECT_TRUE(LiteralParser::parseInteger("0", value, isSigned, bitWidth));
    EXPECT_EQ(value, 0u);
    
    // 大数
    EXPECT_TRUE(LiteralParser::parseInteger("1234567890", value, isSigned, bitWidth));
    EXPECT_EQ(value, 1234567890u);
}

// 测试带下划线分隔符的整数
TEST_F(LiteralParserTest, ParseIntegerWithUnderscores) {
    uint64_t value;
    bool isSigned;
    unsigned bitWidth;
    
    EXPECT_TRUE(LiteralParser::parseInteger("1_000_000", value, isSigned, bitWidth));
    EXPECT_EQ(value, 1000000u);
    
    EXPECT_TRUE(LiteralParser::parseInteger("123_456_789", value, isSigned, bitWidth));
    EXPECT_EQ(value, 123456789u);
    
    // 下划线在开头或结尾应该失败（但我们的实现可能不检查这个）
    // 这里主要测试正常情况
}

// 测试十六进制整数解析
TEST_F(LiteralParserTest, ParseHexadecimalInteger) {
    uint64_t value;
    bool isSigned;
    unsigned bitWidth;
    
    // 基本十六进制
    EXPECT_TRUE(LiteralParser::parseInteger("0x42", value, isSigned, bitWidth));
    EXPECT_EQ(value, 0x42u);
    
    EXPECT_TRUE(LiteralParser::parseInteger("0xFF", value, isSigned, bitWidth));
    EXPECT_EQ(value, 0xFFu);
    
    EXPECT_TRUE(LiteralParser::parseInteger("0xDEADBEEF", value, isSigned, bitWidth));
    EXPECT_EQ(value, 0xDEADBEEFu);
    
    // 大写 X
    EXPECT_TRUE(LiteralParser::parseInteger("0X123", value, isSigned, bitWidth));
    EXPECT_EQ(value, 0x123u);
    
    // 带下划线
    EXPECT_TRUE(LiteralParser::parseInteger("0xFF_FF", value, isSigned, bitWidth));
    EXPECT_EQ(value, 0xFFFFu);
}

// 测试八进制整数解析
TEST_F(LiteralParserTest, ParseOctalInteger) {
    uint64_t value;
    bool isSigned;
    unsigned bitWidth;
    
    EXPECT_TRUE(LiteralParser::parseInteger("0o777", value, isSigned, bitWidth));
    EXPECT_EQ(value, 0777u);
    
    EXPECT_TRUE(LiteralParser::parseInteger("0o123", value, isSigned, bitWidth));
    EXPECT_EQ(value, 0123u);
    
    // 大写 O
    EXPECT_TRUE(LiteralParser::parseInteger("0O456", value, isSigned, bitWidth));
    EXPECT_EQ(value, 0456u);
}

// 测试二进制整数解析
TEST_F(LiteralParserTest, ParseBinaryInteger) {
    uint64_t value;
    bool isSigned;
    unsigned bitWidth;
    
    EXPECT_TRUE(LiteralParser::parseInteger("0b1010", value, isSigned, bitWidth));
    EXPECT_EQ(value, 0b1010u);
    
    EXPECT_TRUE(LiteralParser::parseInteger("0b11111111", value, isSigned, bitWidth));
    EXPECT_EQ(value, 0b11111111u);
    
    // 大写 B
    EXPECT_TRUE(LiteralParser::parseInteger("0B1100", value, isSigned, bitWidth));
    EXPECT_EQ(value, 0b1100u);
    
    // 带下划线
    EXPECT_TRUE(LiteralParser::parseInteger("0b1111_0000", value, isSigned, bitWidth));
    EXPECT_EQ(value, 0b11110000u);
}

// 测试带类型后缀的整数
TEST_F(LiteralParserTest, ParseIntegerWithTypeSuffix) {
    uint64_t value;
    bool isSigned;
    unsigned bitWidth;
    
    // 有符号类型
    EXPECT_TRUE(LiteralParser::parseInteger("42i8", value, isSigned, bitWidth));
    EXPECT_EQ(value, 42u);
    EXPECT_TRUE(isSigned);
    EXPECT_EQ(bitWidth, 8u);
    
    EXPECT_TRUE(LiteralParser::parseInteger("1000i32", value, isSigned, bitWidth));
    EXPECT_EQ(value, 1000u);
    EXPECT_TRUE(isSigned);
    EXPECT_EQ(bitWidth, 32u);
    
    EXPECT_TRUE(LiteralParser::parseInteger("123456i64", value, isSigned, bitWidth));
    EXPECT_EQ(value, 123456u);
    EXPECT_TRUE(isSigned);
    EXPECT_EQ(bitWidth, 64u);
    
    // 无符号类型
    EXPECT_TRUE(LiteralParser::parseInteger("255u8", value, isSigned, bitWidth));
    EXPECT_EQ(value, 255u);
    EXPECT_FALSE(isSigned);
    EXPECT_EQ(bitWidth, 8u);
    
    EXPECT_TRUE(LiteralParser::parseInteger("4000000000u32", value, isSigned, bitWidth));
    EXPECT_EQ(value, 4000000000u);
    EXPECT_FALSE(isSigned);
    EXPECT_EQ(bitWidth, 32u);
    
    // 平台相关类型
    EXPECT_TRUE(LiteralParser::parseInteger("100isize", value, isSigned, bitWidth));
    EXPECT_EQ(value, 100u);
    EXPECT_TRUE(isSigned);
    EXPECT_EQ(bitWidth, 0u);  // 0 表示平台相关
    
    EXPECT_TRUE(LiteralParser::parseInteger("200usize", value, isSigned, bitWidth));
    EXPECT_EQ(value, 200u);
    EXPECT_FALSE(isSigned);
    EXPECT_EQ(bitWidth, 0u);
}

// 测试无效的整数字面量
TEST_F(LiteralParserTest, ParseInvalidInteger) {
    uint64_t value;
    bool isSigned;
    unsigned bitWidth;
    
    // 空字符串
    EXPECT_FALSE(LiteralParser::parseInteger("", value, isSigned, bitWidth));
    
    // 无效的类型后缀
    EXPECT_FALSE(LiteralParser::parseInteger("42i7", value, isSigned, bitWidth));
    EXPECT_FALSE(LiteralParser::parseInteger("42u9", value, isSigned, bitWidth));
    EXPECT_FALSE(LiteralParser::parseInteger("42f32", value, isSigned, bitWidth));
    
    // 无效的进制前缀
    EXPECT_FALSE(LiteralParser::parseInteger("0x", value, isSigned, bitWidth));
    EXPECT_FALSE(LiteralParser::parseInteger("0b", value, isSigned, bitWidth));
    EXPECT_FALSE(LiteralParser::parseInteger("0o", value, isSigned, bitWidth));
    
    // 无效的数字字符
    EXPECT_FALSE(LiteralParser::parseInteger("0b123", value, isSigned, bitWidth));  // 二进制中不能有2,3
    EXPECT_FALSE(LiteralParser::parseInteger("0o89", value, isSigned, bitWidth));   // 八进制中不能有8,9
}

// ============================================================================
// 浮点数字面量解析测试
// ============================================================================

// 测试基本浮点数解析
TEST_F(LiteralParserTest, ParseBasicFloat) {
    double value;
    unsigned bitWidth;
    
    // 基本小数
    EXPECT_TRUE(LiteralParser::parseFloat("3.14", value, bitWidth));
    EXPECT_DOUBLE_EQ(value, 3.14);
    EXPECT_EQ(bitWidth, 0u);  // 未指定位宽
    
    // 整数部分为零
    EXPECT_TRUE(LiteralParser::parseFloat("0.5", value, bitWidth));
    EXPECT_DOUBLE_EQ(value, 0.5);
    
    // 小数部分为零
    EXPECT_TRUE(LiteralParser::parseFloat("42.0", value, bitWidth));
    EXPECT_DOUBLE_EQ(value, 42.0);
}

// 测试科学计数法
TEST_F(LiteralParserTest, ParseScientificNotation) {
    double value;
    unsigned bitWidth;
    
    // 基本科学计数法
    EXPECT_TRUE(LiteralParser::parseFloat("1e10", value, bitWidth));
    EXPECT_DOUBLE_EQ(value, 1e10);
    
    EXPECT_TRUE(LiteralParser::parseFloat("2.5e-3", value, bitWidth));
    EXPECT_DOUBLE_EQ(value, 2.5e-3);
    
    EXPECT_TRUE(LiteralParser::parseFloat("1.23E+5", value, bitWidth));
    EXPECT_DOUBLE_EQ(value, 1.23e5);
    
    // 大写 E
    EXPECT_TRUE(LiteralParser::parseFloat("6.02E23", value, bitWidth));
    EXPECT_DOUBLE_EQ(value, 6.02e23);
}

// 测试带下划线的浮点数
TEST_F(LiteralParserTest, ParseFloatWithUnderscores) {
    double value;
    unsigned bitWidth;
    
    EXPECT_TRUE(LiteralParser::parseFloat("1_000.5", value, bitWidth));
    EXPECT_DOUBLE_EQ(value, 1000.5);
    
    EXPECT_TRUE(LiteralParser::parseFloat("3.141_592_653", value, bitWidth));
    EXPECT_DOUBLE_EQ(value, 3.141592653);
}

// 测试带类型后缀的浮点数
TEST_F(LiteralParserTest, ParseFloatWithTypeSuffix) {
    double value;
    unsigned bitWidth;
    
    // f32 后缀
    EXPECT_TRUE(LiteralParser::parseFloat("3.14f32", value, bitWidth));
    EXPECT_DOUBLE_EQ(value, 3.14);
    EXPECT_EQ(bitWidth, 32u);
    
    // f64 后缀
    EXPECT_TRUE(LiteralParser::parseFloat("2.718281828f64", value, bitWidth));
    EXPECT_DOUBLE_EQ(value, 2.718281828);
    EXPECT_EQ(bitWidth, 64u);
    
    // 科学计数法 + 后缀
    EXPECT_TRUE(LiteralParser::parseFloat("1.5e-10f32", value, bitWidth));
    EXPECT_DOUBLE_EQ(value, 1.5e-10);
    EXPECT_EQ(bitWidth, 32u);
}

// 测试无效的浮点数
TEST_F(LiteralParserTest, ParseInvalidFloat) {
    double value;
    unsigned bitWidth;
    
    // 空字符串
    EXPECT_FALSE(LiteralParser::parseFloat("", value, bitWidth));
    
    // 无效的类型后缀
    EXPECT_FALSE(LiteralParser::parseFloat("3.14i32", value, bitWidth));
    EXPECT_FALSE(LiteralParser::parseFloat("3.14f16", value, bitWidth));
    EXPECT_FALSE(LiteralParser::parseFloat("3.14f128", value, bitWidth));
}

// ============================================================================
// 字符字面量解析测试
// ============================================================================

// 测试基本字符解析
TEST_F(LiteralParserTest, ParseBasicChar) {
    uint32_t codepoint;
    
    // 普通ASCII字符
    EXPECT_TRUE(LiteralParser::parseChar("'a'", codepoint));
    EXPECT_EQ(codepoint, 'a');
    
    EXPECT_TRUE(LiteralParser::parseChar("'Z'", codepoint));
    EXPECT_EQ(codepoint, 'Z');
    
    EXPECT_TRUE(LiteralParser::parseChar("'5'", codepoint));
    EXPECT_EQ(codepoint, '5');
    
    EXPECT_TRUE(LiteralParser::parseChar("' '", codepoint));
    EXPECT_EQ(codepoint, ' ');
}

// 测试转义字符
TEST_F(LiteralParserTest, ParseEscapeChar) {
    uint32_t codepoint;
    
    // 基本转义字符
    EXPECT_TRUE(LiteralParser::parseChar("'\\n'", codepoint));
    EXPECT_EQ(codepoint, '\n');
    
    EXPECT_TRUE(LiteralParser::parseChar("'\\t'", codepoint));
    EXPECT_EQ(codepoint, '\t');
    
    EXPECT_TRUE(LiteralParser::parseChar("'\\r'", codepoint));
    EXPECT_EQ(codepoint, '\r');
    
    EXPECT_TRUE(LiteralParser::parseChar("'\\\\'", codepoint));
    EXPECT_EQ(codepoint, '\\');
    
    EXPECT_TRUE(LiteralParser::parseChar("'\\''", codepoint));
    EXPECT_EQ(codepoint, '\'');
    
    EXPECT_TRUE(LiteralParser::parseChar("'\\\"'", codepoint));
    EXPECT_EQ(codepoint, '"');
    
    EXPECT_TRUE(LiteralParser::parseChar("'\\0'", codepoint));
    EXPECT_EQ(codepoint, '\0');
}

// 测试十六进制转义字符
TEST_F(LiteralParserTest, ParseHexEscapeChar) {
    uint32_t codepoint;
    
    EXPECT_TRUE(LiteralParser::parseChar("'\\x41'", codepoint));
    EXPECT_EQ(codepoint, 0x41u);  // 'A'
    
    EXPECT_TRUE(LiteralParser::parseChar("'\\xFF'", codepoint));
    EXPECT_EQ(codepoint, 0xFFu);
    
    EXPECT_TRUE(LiteralParser::parseChar("'\\x00'", codepoint));
    EXPECT_EQ(codepoint, 0x00u);
}

// 测试Unicode转义字符
TEST_F(LiteralParserTest, ParseUnicodeEscapeChar) {
    uint32_t codepoint;
    
    // 基本Unicode转义
    EXPECT_TRUE(LiteralParser::parseChar("'\\u{41}'", codepoint));
    EXPECT_EQ(codepoint, 0x41u);  // 'A'
    
    EXPECT_TRUE(LiteralParser::parseChar("'\\u{1F600}'", codepoint));
    EXPECT_EQ(codepoint, 0x1F600u);  // 😀 emoji
    
    EXPECT_TRUE(LiteralParser::parseChar("'\\u{4E2D}'", codepoint));
    EXPECT_EQ(codepoint, 0x4E2Du);  // 中文字符 '中'
}

// 测试无效的字符字面量
TEST_F(LiteralParserTest, ParseInvalidChar) {
    uint32_t codepoint;
    
    // 格式错误
    EXPECT_FALSE(LiteralParser::parseChar("", codepoint));
    EXPECT_FALSE(LiteralParser::parseChar("a", codepoint));
    EXPECT_FALSE(LiteralParser::parseChar("'a", codepoint));
    EXPECT_FALSE(LiteralParser::parseChar("a'", codepoint));
    
    // 空字符字面量
    EXPECT_FALSE(LiteralParser::parseChar("''", codepoint));
    
    // 多个字符
    EXPECT_FALSE(LiteralParser::parseChar("'ab'", codepoint));
    
    // 无效的转义序列
    EXPECT_FALSE(LiteralParser::parseChar("'\\z'", codepoint));
    EXPECT_FALSE(LiteralParser::parseChar("'\\x'", codepoint));
    EXPECT_FALSE(LiteralParser::parseChar("'\\xG0'", codepoint));
    EXPECT_FALSE(LiteralParser::parseChar("'\\u{}'", codepoint));
    EXPECT_FALSE(LiteralParser::parseChar("'\\u{GGGG}'", codepoint));
}

// ============================================================================
// 字符串字面量解析测试
// ============================================================================

// 测试基本字符串解析
TEST_F(LiteralParserTest, ParseBasicString) {
    std::string result;
    
    // 空字符串
    EXPECT_TRUE(LiteralParser::parseString("\"\"", result));
    EXPECT_EQ(result, "");
    
    // 简单字符串
    EXPECT_TRUE(LiteralParser::parseString("\"hello\"", result));
    EXPECT_EQ(result, "hello");
    
    EXPECT_TRUE(LiteralParser::parseString("\"Hello, World!\"", result));
    EXPECT_EQ(result, "Hello, World!");
}

// 测试带转义字符的字符串
TEST_F(LiteralParserTest, ParseStringWithEscapes) {
    std::string result;
    
    // 基本转义字符
    EXPECT_TRUE(LiteralParser::parseString("\"hello\\nworld\"", result));
    EXPECT_EQ(result, "hello\nworld");
    
    EXPECT_TRUE(LiteralParser::parseString("\"tab\\there\"", result));
    EXPECT_EQ(result, "tab\there");
    
    EXPECT_TRUE(LiteralParser::parseString("\"quote\\\"here\"", result));
    EXPECT_EQ(result, "quote\"here");
    
    EXPECT_TRUE(LiteralParser::parseString("\"backslash\\\\here\"", result));
    EXPECT_EQ(result, "backslash\\here");
    
    // 十六进制转义
    EXPECT_TRUE(LiteralParser::parseString("\"\\x41\\x42\\x43\"", result));
    EXPECT_EQ(result, "ABC");
    
    // Unicode转义
    EXPECT_TRUE(LiteralParser::parseString("\"\\u{41}\\u{42}\\u{43}\"", result));
    EXPECT_EQ(result, "ABC");
}

// 测试多行字符串
TEST_F(LiteralParserTest, ParseMultilineString) {
    std::string result;
    
    // 基本多行字符串
    EXPECT_TRUE(LiteralParser::parseString("\"\"\"hello\nworld\"\"\"", result));
    EXPECT_EQ(result, "hello\nworld");
    
    // 带转义的多行字符串
    EXPECT_TRUE(LiteralParser::parseString("\"\"\"line1\\nline2\"\"\"", result));
    EXPECT_EQ(result, "line1\nline2");
    
    // 空的多行字符串
    EXPECT_TRUE(LiteralParser::parseString("\"\"\"\"\"\"", result));
    EXPECT_EQ(result, "");
}

// 测试原始字符串
TEST_F(LiteralParserTest, ParseRawString) {
    std::string result;
    
    // 基本原始字符串
    EXPECT_TRUE(LiteralParser::parseString("r\"hello\\nworld\"", result));
    EXPECT_EQ(result, "hello\\nworld");  // 转义字符不被处理
    
    // 带自定义分隔符的原始字符串
    EXPECT_TRUE(LiteralParser::parseString("r#\"hello\"world\"#", result));
    EXPECT_EQ(result, "hello\"world");
    
    EXPECT_TRUE(LiteralParser::parseString("r##\"hello#world\"##", result));
    EXPECT_EQ(result, "hello#world");
    
    // 空的原始字符串
    EXPECT_TRUE(LiteralParser::parseString("r\"\"", result));
    EXPECT_EQ(result, "");
}

// 测试无效的字符串字面量
TEST_F(LiteralParserTest, ParseInvalidString) {
    std::string result;
    
    // 格式错误
    EXPECT_FALSE(LiteralParser::parseString("", result));
    EXPECT_FALSE(LiteralParser::parseString("hello", result));
    EXPECT_FALSE(LiteralParser::parseString("\"hello", result));
    EXPECT_FALSE(LiteralParser::parseString("hello\"", result));
    
    // 无效的转义序列
    EXPECT_FALSE(LiteralParser::parseString("\"hello\\z\"", result));
    EXPECT_FALSE(LiteralParser::parseString("\"\\x\"", result));
    EXPECT_FALSE(LiteralParser::parseString("\"\\xGG\"", result));
    
    // 无效的原始字符串格式
    EXPECT_FALSE(LiteralParser::parseString("r\"hello", result));
    EXPECT_FALSE(LiteralParser::parseString("r#\"hello\"", result));  // 分隔符不匹配
}

// ============================================================================
// 转义序列解析测试
// ============================================================================

// 测试基本转义序列
TEST_F(LiteralParserTest, ParseBasicEscapeSequence) {
    uint32_t result;
    
    // 基本转义字符
    const char* ptr;
    const char* end;
    
    std::string input = "n";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_TRUE(LiteralParser::parseEscapeSequence(ptr, end, result));
    EXPECT_EQ(result, '\n');
    EXPECT_EQ(ptr, end);  // 指针应该移动到末尾
    
    input = "t";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_TRUE(LiteralParser::parseEscapeSequence(ptr, end, result));
    EXPECT_EQ(result, '\t');
    
    input = "r";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_TRUE(LiteralParser::parseEscapeSequence(ptr, end, result));
    EXPECT_EQ(result, '\r');
    
    input = "\\";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_TRUE(LiteralParser::parseEscapeSequence(ptr, end, result));
    EXPECT_EQ(result, '\\');
    
    input = "'";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_TRUE(LiteralParser::parseEscapeSequence(ptr, end, result));
    EXPECT_EQ(result, '\'');
    
    input = "\"";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_TRUE(LiteralParser::parseEscapeSequence(ptr, end, result));
    EXPECT_EQ(result, '"');
    
    input = "0";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_TRUE(LiteralParser::parseEscapeSequence(ptr, end, result));
    EXPECT_EQ(result, '\0');
}

// 测试十六进制转义序列
TEST_F(LiteralParserTest, ParseHexEscapeSequence) {
    uint32_t result;
    const char* ptr;
    const char* end;
    
    std::string input = "x41";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_TRUE(LiteralParser::parseEscapeSequence(ptr, end, result));
    EXPECT_EQ(result, 0x41u);
    EXPECT_EQ(ptr, end);
    
    input = "xFF";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_TRUE(LiteralParser::parseEscapeSequence(ptr, end, result));
    EXPECT_EQ(result, 0xFFu);
    
    input = "x00";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_TRUE(LiteralParser::parseEscapeSequence(ptr, end, result));
    EXPECT_EQ(result, 0x00u);
}

// 测试Unicode转义序列
TEST_F(LiteralParserTest, ParseUnicodeEscapeSequence) {
    uint32_t result;
    const char* ptr;
    const char* end;
    
    std::string input = "u{41}";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_TRUE(LiteralParser::parseEscapeSequence(ptr, end, result));
    EXPECT_EQ(result, 0x41u);
    EXPECT_EQ(ptr, end);
    
    input = "u{1F600}";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_TRUE(LiteralParser::parseEscapeSequence(ptr, end, result));
    EXPECT_EQ(result, 0x1F600u);
    
    input = "u{4E2D}";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_TRUE(LiteralParser::parseEscapeSequence(ptr, end, result));
    EXPECT_EQ(result, 0x4E2Du);
}

// 测试无效的转义序列
TEST_F(LiteralParserTest, ParseInvalidEscapeSequence) {
    uint32_t result;
    const char* ptr;
    const char* end;
    
    // 无效的转义字符
    std::string input = "z";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_FALSE(LiteralParser::parseEscapeSequence(ptr, end, result));
    
    // 不完整的十六进制转义
    input = "x";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_FALSE(LiteralParser::parseEscapeSequence(ptr, end, result));
    
    input = "x4";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_FALSE(LiteralParser::parseEscapeSequence(ptr, end, result));
    
    // 无效的十六进制字符
    input = "xGG";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_FALSE(LiteralParser::parseEscapeSequence(ptr, end, result));
    
    // 不完整的Unicode转义
    input = "u";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_FALSE(LiteralParser::parseEscapeSequence(ptr, end, result));
    
    input = "u{";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_FALSE(LiteralParser::parseEscapeSequence(ptr, end, result));
    
    input = "u{41";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_FALSE(LiteralParser::parseEscapeSequence(ptr, end, result));
    
    // 空的Unicode转义
    input = "u{}";
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_FALSE(LiteralParser::parseEscapeSequence(ptr, end, result));
    
    // 无效的Unicode码点
    input = "u{110000}";  // 超出Unicode范围
    ptr = input.c_str();
    end = ptr + input.length();
    EXPECT_FALSE(LiteralParser::parseEscapeSequence(ptr, end, result));
}