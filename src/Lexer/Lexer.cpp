/// \file Lexer.cpp
/// \brief Implementation of lexical analyzer.

#include "yuan/Lexer/Lexer.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/DiagnosticIDs.h"
#include "yuan/Basic/TokenKinds.h"
#include <cctype>
#include <cassert>
#include <unordered_map>
#include <unordered_set>

namespace yuan {

/// \brief 关键字查找表
static const std::unordered_map<std::string, TokenKind> KeywordMap = {
    // 基本关键字
    {"var", TokenKind::KW_var},
    {"const", TokenKind::KW_const},
    {"func", TokenKind::KW_func},
    {"return", TokenKind::KW_return},
    {"struct", TokenKind::KW_struct},
    {"enum", TokenKind::KW_enum},
    {"trait", TokenKind::KW_trait},
    {"impl", TokenKind::KW_impl},
    
    // 可见性关键字
    {"pub", TokenKind::KW_pub},
    {"priv", TokenKind::KW_priv},
    {"internal", TokenKind::KW_internal},
    
    // 控制流关键字
    {"if", TokenKind::KW_if},
    {"elif", TokenKind::KW_elif},
    {"else", TokenKind::KW_else},
    {"match", TokenKind::KW_match},
    {"while", TokenKind::KW_while},
    {"loop", TokenKind::KW_loop},
    {"for", TokenKind::KW_for},
    {"in", TokenKind::KW_in},
    {"break", TokenKind::KW_break},
    {"continue", TokenKind::KW_continue},
    
    // 字面量关键字
    {"true", TokenKind::KW_true},
    {"false", TokenKind::KW_false},
    {"None", TokenKind::KW_None},
    
    // 异步和其他关键字
    {"async", TokenKind::KW_async},
    {"await", TokenKind::KW_await},
    {"as", TokenKind::KW_as},
    {"self", TokenKind::KW_self},
    {"Self", TokenKind::KW_Self},
    {"mut", TokenKind::KW_mut},
    {"ref", TokenKind::KW_ref},
    {"ptr", TokenKind::KW_ptr},
    {"void", TokenKind::KW_void},
    {"defer", TokenKind::KW_defer},
    {"type", TokenKind::KW_type},
    {"where", TokenKind::KW_where},
    {"orelse", TokenKind::KW_orelse},
    
    // 类型关键字
    {"i8", TokenKind::KW_i8},
    {"i16", TokenKind::KW_i16},
    {"i32", TokenKind::KW_i32},
    {"i64", TokenKind::KW_i64},
    {"i128", TokenKind::KW_i128},
    {"isize", TokenKind::KW_isize},
    {"u8", TokenKind::KW_u8},
    {"u16", TokenKind::KW_u16},
    {"u32", TokenKind::KW_u32},
    {"u64", TokenKind::KW_u64},
    {"u128", TokenKind::KW_u128},
    {"usize", TokenKind::KW_usize},
    {"f32", TokenKind::KW_f32},
    {"f64", TokenKind::KW_f64},
    {"bool", TokenKind::KW_bool},
    {"char", TokenKind::KW_char},
    {"str", TokenKind::KW_str},
};

Lexer::Lexer(SourceManager& sm, DiagnosticEngine& diag, 
             SourceManager::FileID fileID)
    : SM(sm), Diag(diag), FileID(fileID) {
    
    // 获取文件内容
    const std::string& content = SM.getBufferData(fileID);
    BufferStart = content.data();
    BufferEnd = BufferStart + content.size();
    CurPtr = BufferStart;
}

Token Lexer::lex() {
    // 如果有缓存的 lookahead tokens，返回第一个
    if (!LookaheadTokens.empty()) {
        Token token = LookaheadTokens.front();
        LookaheadTokens.pop_front();
        return token;
    }
    
    return lexImpl();
}

Token Lexer::peek() {
    return peek(0);
}

Token Lexer::peek(unsigned n) {
    // 确保我们有足够的 lookahead tokens
    while (LookaheadTokens.size() <= n) {
        LookaheadTokens.push_back(lexImpl());
    }
    
    return LookaheadTokens[n];
}

SourceLocation Lexer::getCurrentLocation() const {
    return getLocation();
}

bool Lexer::isNewLineBetween(SourceLocation left, SourceLocation right) const {
    if (left.isInvalid() || right.isInvalid()) {
        return false;
    }
    auto leftPos = SM.getLineAndColumn(left);
    auto rightPos = SM.getLineAndColumn(right);
    return leftPos.first != rightPos.first;
}

bool Lexer::isAtEnd() const {
    return CurPtr >= BufferEnd;
}

void Lexer::splitGreaterGreater() {
    // 创建一个 '>' token 并插入到 lookahead 缓存的前面
    // 这样下一次调用 lex() 时会返回这个 '>' token
    SourceLocation loc = getLocation();
    Token greaterToken(TokenKind::Greater, loc, ">");
    LookaheadTokens.push_front(greaterToken);
}

Token Lexer::attachDocComment(Token token) {
    if (PendingDocComment.empty()) {
        return token;
    }

    Token withDoc(token.getKind(), token.getLocation(), token.getText(), PendingDocComment);
    PendingDocComment.clear();
    return withDoc;
}

Token Lexer::lexImpl() {
    // 跳过空白字符和注释
    skipWhitespace();
    
    // 检查是否到达文件末尾
    if (isAtEnd()) {
        PendingDocComment.clear();
        return Token(TokenKind::EndOfFile, getLocation(), "");
    }
    
    char c = peekChar();
    
    // 原始字符串 (r"..." 或 r###"..."###) - 必须在标识符检查之前
    if (c == 'r') {
        // 检查是否是原始字符串
        unsigned pos = 1;
        while (pos < 100 && peekChar(pos) == '#') { // 限制最大分隔符长度
            pos++;
        }
        if (peekChar(pos) == '"') {
            return attachDocComment(lexRawString());
        }
        // 否则作为普通标识符处理
        return attachDocComment(lexIdentifier());
    }
    
    // 标识符和关键字
    if (isIdentifierStart(c)) {
        return attachDocComment(lexIdentifier());
    }
    
    // 检查 UTF-8 标识符开始字符
    if (static_cast<unsigned char>(c) >= 0x80) {
        uint32_t codepoint;
        int bytesConsumed;
        if (decodeUTF8(CurPtr, BufferEnd, codepoint, bytesConsumed)) {
            if (isUnicodeIdentifierStart(codepoint)) {
                return attachDocComment(lexIdentifier());
            }
        }
        // 如果不是有效的标识符开始字符，继续处理为无效字符
    }
    
    // 数字字面量
    if (isDigit(c)) {
        return attachDocComment(lexNumber());
    }
    
    // 多行字符串（需要在普通字符串之前检查）
    if (c == '"' && peekChar(1) == '"' && peekChar(2) == '"') {
        return attachDocComment(lexMultilineString());
    }
    
    // 字符串字面量
    if (c == '"') {
        return attachDocComment(lexString());
    }
    
    // 字符字面量
    if (c == '\'') {
        return attachDocComment(lexChar());
    }
    
    // 运算符和标点符号
    return attachDocComment(lexOperator());
}

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = peekChar();
        
        // 空白字符
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            consumeChar();
            continue;
        }
        
        // 行注释 (// 或 ///)
        if (c == '/' && peekChar(1) == '/') {
            skipLineComment();
            continue;
        }
        
        // 块注释 (/* ... */)
        if (c == '/' && peekChar(1) == '*') {
            skipBlockComment();
            continue;
        }
        
        // 不是空白字符或注释，停止跳过
        break;
    }
}

void Lexer::skipLineComment() {
    assert(peekChar() == '/' && peekChar(1) == '/');
    
    // 检查是否是文档注释 (///)
    bool isDocComment = (peekChar(2) == '/');
    
    // 跳过 "//" 或 "///"
    consumeChar();
    consumeChar();
    if (isDocComment) {
        consumeChar();
    }

    const char* commentStart = CurPtr;

    // 跳过直到行尾
    while (!isAtEnd() && peekChar() != '\n') {
        char c = peekChar();

        // 处理 UTF-8 字符
        if (static_cast<unsigned char>(c) >= 0x80) {
            uint32_t codepoint;
            int bytesConsumed;
            if (decodeUTF8(CurPtr, BufferEnd, codepoint, bytesConsumed)) {
                // 有效的 UTF-8 字符，跳过所有字节
                CurPtr += bytesConsumed;
            } else {
                // 无效的 UTF-8 字符，只跳过一个字节
                consumeChar();
            }
        } else {
            // ASCII 字符
            consumeChar();
        }
    }

    const char* commentEnd = CurPtr;

    if (isDocComment) {
        std::string text(commentStart, commentEnd - commentStart);
        if (!text.empty() && text[0] == ' ') {
            text.erase(text.begin());
        }
        if (!PendingDocComment.empty()) {
            PendingDocComment += "\n";
        }
        PendingDocComment += text;
    }
    
    // 跳过换行符
    if (!isAtEnd() && peekChar() == '\n') {
        consumeChar();
    }
}

void Lexer::skipBlockComment() {
    assert(peekChar() == '/' && peekChar(1) == '*');
    
    SourceLocation startLoc = getLocation();
    
    // 跳过 "/*"
    consumeChar();
    consumeChar();
    
    // 跳过直到 "*/"
    while (!isAtEnd()) {
        char c = peekChar();
        if (c == '*' && peekChar(1) == '/') {
            // 跳过 "*/"
            consumeChar();
            consumeChar();
            return;
        }
        
        // 处理 UTF-8 字符
        if (static_cast<unsigned char>(c) >= 0x80) {
            uint32_t codepoint;
            int bytesConsumed;
            if (decodeUTF8(CurPtr, BufferEnd, codepoint, bytesConsumed)) {
                // 有效的 UTF-8 字符，跳过所有字节
                CurPtr += bytesConsumed;
            } else {
                // 无效的 UTF-8 字符，只跳过一个字节
                consumeChar();
            }
        } else {
            // ASCII 字符
            consumeChar();
        }
    }
    
    // 如果到达文件末尾但没有找到结束标记，报告错误
    SourceRange commentRange(startLoc, getLocation());
    Diag.report(DiagID::err_unterminated_block_comment, startLoc, commentRange);
}

bool Lexer::isIdentifierStart(char c) const {
    // ASCII 字母、下划线或 @ (内置函数)
    if (std::isalpha(c) || c == '_' || c == '@') {
        return true;
    }
    
    // 检查 UTF-8 字符
    if (static_cast<unsigned char>(c) >= 0x80) {
        // 这是一个 UTF-8 多字节字符的开始
        uint32_t codepoint;
        int bytesConsumed;
        if (decodeUTF8(CurPtr - 1, BufferEnd, codepoint, bytesConsumed)) {
            return isUnicodeIdentifierStart(codepoint);
        }
    }
    
    return false;
}

bool Lexer::isIdentifierContinue(char c) const {
    // ASCII 字母、数字或下划线
    if (std::isalnum(c) || c == '_') {
        return true;
    }
    
    // 检查 UTF-8 字符
    if (static_cast<unsigned char>(c) >= 0x80) {
        // 这是一个 UTF-8 多字节字符的开始
        uint32_t codepoint;
        int bytesConsumed;
        if (decodeUTF8(CurPtr - 1, BufferEnd, codepoint, bytesConsumed)) {
            return isUnicodeIdentifierContinue(codepoint);
        }
    }
    
    return false;
}

bool Lexer::isDigit(char c) const {
    return std::isdigit(c);
}

bool Lexer::isHexDigit(char c) const {
    return std::isxdigit(c);
}

char Lexer::peekChar() const {
    if (isAtEnd()) {
        return '\0';
    }
    return *CurPtr;
}

char Lexer::peekChar(unsigned n) const {
    if (CurPtr + n >= BufferEnd) {
        return '\0';
    }
    return *(CurPtr + n);
}

char Lexer::consumeChar() {
    if (isAtEnd()) {
        return '\0';
    }
    return *CurPtr++;
}

SourceLocation Lexer::getLocation() const {
    if (isAtEnd()) {
        return SM.getLocation(FileID, BufferEnd - BufferStart);
    }
    return SM.getLocation(FileID, CurPtr - BufferStart);
}

void Lexer::reportError(DiagID id, SourceLocation loc) {
    Diag.report(id, loc);
}

void Lexer::reportError(DiagID id, SourceLocation loc, const std::string& arg) {
    Diag.report(id, loc) << arg;
}

bool Lexer::processEscapeSequence(SourceLocation startLoc, char escapeChar) {
    switch (escapeChar) {
        case 'n': case 't': case 'r': case '\\': case '\'': case '"': case '0':
            return true;
        case 'x': {
            // 十六进制转义 \xNN
            for (int i = 0; i < 2; i++) {
                if (isAtEnd() || !isHexDigit(peekChar())) {
                    reportError(DiagID::err_invalid_escape_sequence, startLoc, "x");
                    // 跳过剩余的无效字符，但继续分析
                    while (!isAtEnd() && peekChar() != '"' && peekChar() != '\'' && peekChar() != '\n') {
                        if (isHexDigit(peekChar())) {
                            consumeChar();
                        } else {
                            break;
                        }
                    }
                    return true; // 继续分析，不停止
                }
                consumeChar();
            }
            return true;
        }
        case 'u': {
            // Unicode 转义 \u{NNNN}
            if (isAtEnd() || peekChar() != '{') {
                reportError(DiagID::err_invalid_escape_sequence, startLoc, "u");
                return true; // 继续分析
            }
            consumeChar(); // '{'
            
            bool hasDigits = false;
            while (!isAtEnd() && peekChar() != '}') {
                if (!isHexDigit(peekChar())) {
                    reportError(DiagID::err_invalid_escape_sequence, startLoc, "u");
                    // 跳过到结束括号或字符串结束
                    while (!isAtEnd() && peekChar() != '}' && peekChar() != '"' && peekChar() != '\'' && peekChar() != '\n') {
                        consumeChar();
                    }
                    if (!isAtEnd() && peekChar() == '}') {
                        consumeChar(); // 消费 '}'
                    }
                    return true; // 继续分析
                }
                consumeChar();
                hasDigits = true;
            }
            
            if (!hasDigits || isAtEnd() || peekChar() != '}') {
                reportError(DiagID::err_invalid_escape_sequence, startLoc, "u");
                if (!isAtEnd() && peekChar() == '}') {
                    consumeChar(); // 消费 '}'
                }
                return true; // 继续分析
            }
            consumeChar(); // '}'
            return true;
        }
        default:
            reportError(DiagID::err_invalid_escape_sequence, startLoc, std::string(1, escapeChar));
            return true; // 继续分析，不停止
    }
}

bool Lexer::decodeUTF8(const char* start, const char* end, uint32_t& codepoint, int& bytesConsumed) const {
    if (start >= end) {
        return false;
    }
    
    unsigned char first = static_cast<unsigned char>(*start);
    
    // ASCII 字符
    if (first < 0x80) {
        codepoint = first;
        bytesConsumed = 1;
        return true;
    }
    
    // 2 字节序列: 110xxxxx 10xxxxxx
    if ((first & 0xE0) == 0xC0) {
        if (start + 1 >= end) return false;
        unsigned char second = static_cast<unsigned char>(start[1]);
        if ((second & 0xC0) != 0x80) return false;
        
        codepoint = ((first & 0x1F) << 6) | (second & 0x3F);
        bytesConsumed = 2;
        return codepoint >= 0x80; // 检查过编码
    }
    
    // 3 字节序列: 1110xxxx 10xxxxxx 10xxxxxx
    if ((first & 0xF0) == 0xE0) {
        if (start + 2 >= end) return false;
        unsigned char second = static_cast<unsigned char>(start[1]);
        unsigned char third = static_cast<unsigned char>(start[2]);
        if ((second & 0xC0) != 0x80 || (third & 0xC0) != 0x80) return false;
        
        codepoint = ((first & 0x0F) << 12) | ((second & 0x3F) << 6) | (third & 0x3F);
        bytesConsumed = 3;
        return codepoint >= 0x800; // 检查过编码
    }
    
    // 4 字节序列: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    if ((first & 0xF8) == 0xF0) {
        if (start + 3 >= end) return false;
        unsigned char second = static_cast<unsigned char>(start[1]);
        unsigned char third = static_cast<unsigned char>(start[2]);
        unsigned char fourth = static_cast<unsigned char>(start[3]);
        if ((second & 0xC0) != 0x80 || (third & 0xC0) != 0x80 || (fourth & 0xC0) != 0x80) return false;
        
        codepoint = ((first & 0x07) << 18) | ((second & 0x3F) << 12) | ((third & 0x3F) << 6) | (fourth & 0x3F);
        bytesConsumed = 4;
        return codepoint >= 0x10000 && codepoint <= 0x10FFFF; // 检查过编码和有效范围
    }
    
    return false;
}

bool Lexer::isUnicodeIdentifierStart(uint32_t codepoint) const {
    // Unicode 标识符开始字符的简化实现
    // 基于 Unicode 标准的 ID_Start 属性
    
    // 基本拉丁字母 (A-Z, a-z)
    if ((codepoint >= 0x41 && codepoint <= 0x5A) || 
        (codepoint >= 0x61 && codepoint <= 0x7A)) {
        return true;
    }
    
    // 中文字符 (CJK Unified Ideographs)
    if (codepoint >= 0x4E00 && codepoint <= 0x9FFF) {
        return true;
    }
    
    // 中文扩展 A
    if (codepoint >= 0x3400 && codepoint <= 0x4DBF) {
        return true;
    }
    
    // 中文扩展 B
    if (codepoint >= 0x20000 && codepoint <= 0x2A6DF) {
        return true;
    }
    
    // 日文平假名
    if (codepoint >= 0x3040 && codepoint <= 0x309F) {
        return true;
    }
    
    // 日文片假名
    if (codepoint >= 0x30A0 && codepoint <= 0x30FF) {
        return true;
    }
    
    // 韩文字母
    if (codepoint >= 0xAC00 && codepoint <= 0xD7AF) {
        return true;
    }
    
    // 西里尔字母 (俄文等)
    if (codepoint >= 0x0400 && codepoint <= 0x04FF) {
        return true;
    }
    
    // 希腊字母
    if (codepoint >= 0x0370 && codepoint <= 0x03FF) {
        return true;
    }
    
    // 拉丁字母扩展 A (带重音符号等)
    if (codepoint >= 0x0100 && codepoint <= 0x017F) {
        return true;
    }
    
    // 拉丁字母扩展 B
    if (codepoint >= 0x0180 && codepoint <= 0x024F) {
        return true;
    }
    
    // 阿拉伯字母
    if (codepoint >= 0x0600 && codepoint <= 0x06FF) {
        return true;
    }
    
    // 希伯来字母
    if (codepoint >= 0x0590 && codepoint <= 0x05FF) {
        return true;
    }
    
    return false;
}

bool Lexer::isUnicodeIdentifierContinue(uint32_t codepoint) const {
    // Unicode 标识符继续字符的简化实现
    // 基于 Unicode 标准的 ID_Continue 属性
    
    // 如果可以作为开始字符，也可以作为继续字符
    if (isUnicodeIdentifierStart(codepoint)) {
        return true;
    }
    
    // 数字字符
    if (codepoint >= 0x30 && codepoint <= 0x39) { // ASCII 数字
        return true;
    }
    
    // 全角数字
    if (codepoint >= 0xFF10 && codepoint <= 0xFF19) {
        return true;
    }
    
    // 其他数字字符 (简化实现)
    if (codepoint >= 0x0660 && codepoint <= 0x0669) { // 阿拉伯数字
        return true;
    }
    
    // 组合字符和修饰符 (简化实现)
    if (codepoint >= 0x0300 && codepoint <= 0x036F) { // 组合变音符号
        return true;
    }
    
    return false;
}

Token Lexer::lexIdentifier() {
    SourceLocation startLoc = getLocation();
    const char* start = CurPtr;
    
    // 检查是否是内置函数标识符
    bool isBuiltin = (peekChar() == '@');
    
    // 消费第一个字符（可能是多字节 UTF-8）
    if (static_cast<unsigned char>(*CurPtr) >= 0x80) {
        // UTF-8 多字节字符
        uint32_t codepoint;
        int bytesConsumed;
        if (decodeUTF8(CurPtr, BufferEnd, codepoint, bytesConsumed)) {
            CurPtr += bytesConsumed;
        } else {
            // 无效的 UTF-8 序列，报告错误但继续分析
            reportError(DiagID::err_invalid_character, startLoc);
            consumeChar(); // 跳过无效字符，继续分析
        }
    } else {
        // ASCII 字符
        consumeChar();
    }
    
    // 跳过标识符的其余字符
    while (!isAtEnd()) {
        char c = peekChar();
        if (static_cast<unsigned char>(c) >= 0x80) {
            // UTF-8 多字节字符
            uint32_t codepoint;
            int bytesConsumed;
            if (decodeUTF8(CurPtr, BufferEnd, codepoint, bytesConsumed)) {
                if (isUnicodeIdentifierContinue(codepoint)) {
                    CurPtr += bytesConsumed;
                } else {
                    break;
                }
            } else {
                // 无效的 UTF-8 序列，停止解析
                break;
            }
        } else {
            // ASCII 字符
            if (isIdentifierContinue(c)) {
                consumeChar();
            } else {
                break;
            }
        }
    }
    
    std::string text(start, CurPtr - start);
    
    // 如果是内置函数标识符，直接返回
    if (isBuiltin) {
        return Token(TokenKind::BuiltinIdentifier, startLoc, text);
    }
    
    // 查找关键字
    auto it = KeywordMap.find(text);
    TokenKind kind = (it != KeywordMap.end()) ? it->second : TokenKind::Identifier;
    
    return Token(kind, startLoc, text);
}

Token Lexer::lexNumber() {
    SourceLocation startLoc = getLocation();
    const char* start = CurPtr;
    
    // 检查进制前缀
    bool hasPrefix = false;
    int base = 10;
    
    if (peekChar() == '0') {
        char nextChar = peekChar(1);
        if (nextChar == 'x' || nextChar == 'X') {
            // 十六进制 0x
            base = 16;
            hasPrefix = true;
            consumeChar(); // '0'
            consumeChar(); // 'x'
        } else if (nextChar == 'o' || nextChar == 'O') {
            // 八进制 0o
            base = 8;
            hasPrefix = true;
            consumeChar(); // '0'
            consumeChar(); // 'o'
        } else if (nextChar == 'b' || nextChar == 'B') {
            // 二进制 0b
            base = 2;
            hasPrefix = true;
            consumeChar(); // '0'
            consumeChar(); // 'b'
        }
    }
    
    // 如果没有前缀，消费第一个数字
    if (!hasPrefix) {
        consumeChar();
    }
    
    // 读取数字部分
    bool hasDigits = hasPrefix ? false : true; // 如果有前缀，需要至少一个数字
    bool isFloat = false;
    bool lastWasUnderscore = false; // 跟踪上一个字符是否是下划线
    SourceLocation lastUnderscoreLoc; // 跟踪最后一个下划线的位置
    
    while (!isAtEnd()) {
        char c = peekChar();
        
        // 检查是否是有效数字
        bool isValidDigit = false;
        if (base == 2) {
            isValidDigit = (c == '0' || c == '1');
        } else if (base == 8) {
            isValidDigit = (c >= '0' && c <= '7');
        } else if (base == 10) {
            isValidDigit = std::isdigit(c);
        } else if (base == 16) {
            isValidDigit = std::isxdigit(c);
        }
        
        if (isValidDigit) {
            consumeChar();
            hasDigits = true;
            lastWasUnderscore = false;
        } else if (c == '_') {
            // 下划线分隔符验证
            SourceLocation underscoreLoc = getLocation();
            if (lastWasUnderscore) {
                // 连续下划线是错误的
                // 先继续消费字符直到遇到非数字字母字符，然后计算完整范围
                while (!isAtEnd() && (std::isalnum(peekChar()) || peekChar() == '_')) {
                    consumeChar();
                }
                
                // 创建包含完整数字范围的诊断
                SourceLocation numberStart = startLoc;
                SourceLocation numberEnd = getLocation();
                SourceRange numberRange(numberStart, numberEnd);
                
                // 报告错误，位置指向第二个下划线，包含整个数字的范围高亮
                Diag.report(DiagID::err_invalid_number_literal, underscoreLoc, numberRange);
                
                std::string text(start, CurPtr - start);
                TokenKind kind = isFloat ? TokenKind::FloatLiteral : TokenKind::IntegerLiteral;
                return Token(kind, startLoc, text);
            }
            consumeChar();
            lastWasUnderscore = true;
            lastUnderscoreLoc = underscoreLoc;
        } else if (c == '.' && base == 10) {
            // 小数点（只在十进制中有效）
            char nextChar = peekChar(1);
            if (std::isdigit(nextChar)) {
                if (isFloat) {
                    // 已经有一个小数点了，这是第二个小数点，报错
                    SourceRange numberRange(startLoc, getLocation());
                    Diag.report(DiagID::err_invalid_number_literal, getLocation(), numberRange);
                    // 继续分析，但不消费这个小数点，让它作为单独的token处理
                    break;
                } else {
                    isFloat = true;
                    consumeChar(); // '.'
                    lastWasUnderscore = false;
                }
            } else {
                break; // 不是浮点数，可能是范围操作符 ..
            }
        } else if ((c == 'e' || c == 'E') && base == 10 && hasDigits) {
            // 科学计数法指数
            isFloat = true;
            consumeChar(); // 'e' 或 'E'
            
            // 可选的正负号
            char signChar = peekChar();
            if (signChar == '+' || signChar == '-') {
                consumeChar();
            }
            
            // 指数部分必须有数字
            bool hasExpDigits = false;
            while (!isAtEnd() && std::isdigit(peekChar())) {
                consumeChar();
                hasExpDigits = true;
            }
            
            if (!hasExpDigits) {
                SourceRange numberRange(startLoc, getLocation());
                Diag.report(DiagID::err_invalid_number_literal, startLoc, numberRange);
                // 返回一个数字token，即使格式不正确
                std::string text(start, CurPtr - start);
                return Token(TokenKind::IntegerLiteral, startLoc, text);
            }
            lastWasUnderscore = false;
            break;
        } else if (std::isdigit(c)) {
            // 遇到了对当前进制无效的数字，这是错误
            // 创建包含完整数字范围的诊断
            SourceLocation numberStart = startLoc;
            SourceLocation invalidDigitLoc = getLocation();
            
            // 继续消费字符直到遇到非数字字母字符
            while (!isAtEnd() && (std::isalnum(peekChar()) || peekChar() == '_')) {
                consumeChar();
            }
            
            SourceLocation numberEnd = getLocation();
            SourceRange numberRange(numberStart, numberEnd);
            
            // 报告错误，位置指向无效数字，包含整个数字的范围高亮
            Diag.report(DiagID::err_invalid_number_literal, invalidDigitLoc, numberRange);
            
            std::string text(start, CurPtr - start);
            TokenKind kind = isFloat ? TokenKind::FloatLiteral : TokenKind::IntegerLiteral;
            return Token(kind, startLoc, text);
        } else if (std::isalpha(c)) {
            // 遇到字母，可能是类型后缀，退出循环让后缀处理逻辑处理
            break;
        } else {
            break;
        }
    }
    
    // 检查是否以下划线结尾
    if (lastWasUnderscore) {
        // 创建一个包含完整数字范围的诊断，但错误位置指向下划线
        SourceLocation numberStart = startLoc;
        SourceLocation numberEnd = getLocation();
        SourceRange numberRange(numberStart, numberEnd);
        
        // 报告错误，位置指向下划线，包含整个数字的范围高亮
        Diag.report(DiagID::err_invalid_number_literal, lastUnderscoreLoc, numberRange);
    }
    
    if (!hasDigits) {
        SourceRange numberRange(startLoc, getLocation());
        Diag.report(DiagID::err_invalid_number_literal, startLoc, numberRange);
        // 返回一个数字token，即使格式不正确
        std::string text(start, CurPtr - start);
        return Token(TokenKind::IntegerLiteral, startLoc, text);
    }
    
    // 检查类型后缀
    std::string suffix;
    if (!isAtEnd() && std::isalpha(peekChar())) {
        SourceLocation suffixStartLoc = getLocation();
        const char* suffixStart = CurPtr;
        while (!isAtEnd() && (std::isalnum(peekChar()) || peekChar() == '_')) {
            consumeChar();
        }
        suffix = std::string(suffixStart, CurPtr - suffixStart);
        
        // 验证后缀是否有效
        static const std::unordered_set<std::string> validIntSuffixes = {
            "i8", "i16", "i32", "i64", "i128", "isize",
            "u8", "u16", "u32", "u64", "u128", "usize"
        };
        static const std::unordered_set<std::string> validFloatSuffixes = {
            "f32", "f64"
        };
        
        bool validSuffix = false;
        if (isFloat) {
            validSuffix = validFloatSuffixes.find(suffix) != validFloatSuffixes.end();
        } else {
            validSuffix = validIntSuffixes.find(suffix) != validIntSuffixes.end();
        }
        
        if (!validSuffix) {
            SourceRange numberRange(startLoc, getLocation());
            if (!suffix.empty()) {
                Diag.report(DiagID::err_invalid_character, suffixStartLoc, numberRange)
                    << suffix[0];
            } else {
                Diag.report(DiagID::err_invalid_number_literal, startLoc, numberRange);
            }
            // 返回一个数字token，即使后缀不正确
            std::string text(start, CurPtr - start);
            TokenKind kind = isFloat ? TokenKind::FloatLiteral : TokenKind::IntegerLiteral;
            return Token(kind, startLoc, text);
        }
    }
    
    std::string text(start, CurPtr - start);
    TokenKind kind = isFloat ? TokenKind::FloatLiteral : TokenKind::IntegerLiteral;
    
    return Token(kind, startLoc, text);
}

Token Lexer::lexString() {
    SourceLocation startLoc = getLocation();
    const char* start = CurPtr;
    
    // 消费开始的双引号
    consumeChar(); // "
    
    while (!isAtEnd()) {
        char c = peekChar();
        
        if (c == '"') {
            // 找到结束的双引号
            consumeChar();
            std::string text(start, CurPtr - start);
            return Token(TokenKind::StringLiteral, startLoc, text);
        } else if (c == '\\') {
            // 处理转义字符
            consumeChar(); // '\'
            if (isAtEnd()) {
                SourceRange stringRange(startLoc, getLocation());
                Diag.report(DiagID::err_unterminated_string, startLoc, stringRange);
                // 跳过错误，继续分析下一个token
                return lexImpl();
            }
            
            char escapeChar = peekChar();
            consumeChar();
            
            // 处理转义序列
            processEscapeSequence(startLoc, escapeChar);
            // 注意：processEscapeSequence现在总是返回true，错误已经报告
        } else if (c == '\n' || c == '\r') {
            // 普通字符串不能包含未转义的换行符
            SourceRange stringRange(startLoc, getLocation());
            Diag.report(DiagID::err_unterminated_string, startLoc, stringRange);
            // 跳过错误，继续分析下一个token
            return lexImpl();
        } else {
            // 普通字符
            consumeChar();
        }
    }
    
    // 如果到达这里，说明没有找到结束的双引号
    SourceRange stringRange(startLoc, getLocation());
    Diag.report(DiagID::err_unterminated_string, startLoc, stringRange);
    // 跳过错误，继续分析下一个token
    return lexImpl();
}

Token Lexer::lexRawString() {
    SourceLocation startLoc = getLocation();
    const char* start = CurPtr;
    
    // 消费 'r'
    consumeChar();
    
    // 检查是否有自定义分隔符
    std::string delimiter;
    while (!isAtEnd() && peekChar() == '#') {
        delimiter += '#';
        consumeChar();
    }
    
    // 检查开始的双引号
    if (isAtEnd() || peekChar() != '"') {
        reportError(DiagID::err_invalid_character, startLoc);
        // 返回一个标识符token而不是Invalid
        std::string text(start, CurPtr - start);
        return Token(TokenKind::Identifier, startLoc, text);
    }
    consumeChar(); // "
    
    // 读取字符串内容，直到找到匹配的结束序列
    std::string endSequence = "\"" + delimiter;
    
    while (!isAtEnd()) {
        // 检查是否匹配结束序列
        bool foundEnd = true;
        for (size_t i = 0; i < endSequence.length(); i++) {
            if (peekChar(i) != endSequence[i]) {
                foundEnd = false;
                break;
            }
        }
        
        if (foundEnd) {
            // 消费结束序列
            for (size_t i = 0; i < endSequence.length(); i++) {
                consumeChar();
            }
            std::string text(start, CurPtr - start);
            return Token(TokenKind::RawStringLiteral, startLoc, text);
        } else {
            // 继续读取字符（原始字符串中所有字符都按字面意思处理）
            consumeChar();
        }
    }
    
    // 如果到达这里，说明没有找到结束序列
    SourceRange stringRange(startLoc, getLocation());
    Diag.report(DiagID::err_unterminated_raw_string, startLoc, stringRange);
    // 跳过错误，继续分析下一个token
    return lexImpl();
}

Token Lexer::lexMultilineString() {
    SourceLocation startLoc = getLocation();
    const char* start = CurPtr;
    
    // 消费开始的三个双引号
    consumeChar(); // "
    consumeChar(); // "
    consumeChar(); // "
    
    while (!isAtEnd()) {
        // 检查是否是结束的三个双引号
        if (peekChar() == '"' && peekChar(1) == '"' && peekChar(2) == '"') {
            // 消费结束的三个双引号
            consumeChar(); // "
            consumeChar(); // "
            consumeChar(); // "
            std::string text(start, CurPtr - start);
            return Token(TokenKind::MultilineStringLiteral, startLoc, text);
        } else if (peekChar() == '\\') {
            // 处理转义字符（多行字符串也支持转义）
            consumeChar(); // '\'
            if (isAtEnd()) {
                SourceRange stringRange(startLoc, getLocation());
                Diag.report(DiagID::err_unterminated_multiline_string, startLoc, stringRange);
                // 跳过错误，继续分析下一个token
                return lexImpl();
            }
            
            char escapeChar = peekChar();
            consumeChar();
            
            // 处理转义序列
            if (!processEscapeSequence(startLoc, escapeChar)) {
                return Token(TokenKind::Invalid, startLoc, "");
            }
        } else {
            // 普通字符（包括换行符）
            consumeChar();
        }
    }
    
    // 如果到达这里，说明没有找到结束的三个双引号
    SourceRange stringRange(startLoc, getLocation());
    Diag.report(DiagID::err_unterminated_multiline_string, startLoc, stringRange);
    // 跳过错误，继续分析下一个token
    return lexImpl();
}

Token Lexer::lexChar() {
    SourceLocation startLoc = getLocation();
    const char* start = CurPtr;
    
    // 消费开始的单引号
    consumeChar(); // '
    
    if (isAtEnd()) {
        SourceRange charRange(startLoc, getLocation());
        Diag.report(DiagID::err_unterminated_char, startLoc, charRange);
        // 跳过错误，继续分析下一个token
        return lexImpl();
    }

    char c = peekChar();
    if (c == '\'') {
        // 空字符字面量 ''
        consumeChar(); // closing '
        reportError(DiagID::err_empty_char_literal, startLoc);
        return lexImpl();
    }

    if (c == '\n' || c == '\r') {
        SourceRange charRange(startLoc, getLocation());
        Diag.report(DiagID::err_unterminated_char, startLoc, charRange);
        return lexImpl();
    }

    // 读取一个字符（支持转义）
    if (c == '\\') {
        consumeChar(); // '\'
        if (isAtEnd()) {
            SourceRange charRange(startLoc, getLocation());
            Diag.report(DiagID::err_unterminated_char, startLoc, charRange);
            return lexImpl();
        }
        char escapeChar = peekChar();
        consumeChar();
        processEscapeSequence(startLoc, escapeChar);
    } else {
        consumeChar();
    }

    // 如果不是结束引号，则说明字符字面量内容非法（例如 'ab'）
    if (isAtEnd() || peekChar() != '\'') {
        while (!isAtEnd() && peekChar() != '\'' && peekChar() != '\n' && peekChar() != '\r') {
            consumeChar();
        }
        reportError(DiagID::err_empty_char_literal, startLoc);
        if (!isAtEnd() && peekChar() == '\'') {
            consumeChar();
        }
        return lexImpl();
    }

    consumeChar(); // closing '
    
    std::string text(start, CurPtr - start);
    return Token(TokenKind::CharLiteral, startLoc, text);
}

Token Lexer::lexOperator() {
    SourceLocation startLoc = getLocation();
    
    char c = peekChar();
    
    switch (c) {
        // 单字符运算符和标点符号
        case '+':
            consumeChar();
            if (peekChar() == '=') {
                consumeChar();
                return Token(TokenKind::PlusEqual, startLoc, "+=");
            }
            return Token(TokenKind::Plus, startLoc, "+");
            
        case '-':
            consumeChar();
            if (peekChar() == '=') {
                consumeChar();
                return Token(TokenKind::MinusEqual, startLoc, "-=");
            } else if (peekChar() == '>') {
                consumeChar();
                return Token(TokenKind::Arrow, startLoc, "->");
            }
            return Token(TokenKind::Minus, startLoc, "-");
            
        case '*':
            consumeChar();
            if (peekChar() == '=') {
                consumeChar();
                return Token(TokenKind::StarEqual, startLoc, "*=");
            }
            return Token(TokenKind::Star, startLoc, "*");
            
        case '/':
            consumeChar();
            if (peekChar() == '=') {
                consumeChar();
                return Token(TokenKind::SlashEqual, startLoc, "/=");
            }
            return Token(TokenKind::Slash, startLoc, "/");
            
        case '%':
            consumeChar();
            if (peekChar() == '=') {
                consumeChar();
                return Token(TokenKind::PercentEqual, startLoc, "%=");
            }
            return Token(TokenKind::Percent, startLoc, "%");
            
        case '&':
            consumeChar();
            if (peekChar() == '&') {
                consumeChar();
                return Token(TokenKind::AmpAmp, startLoc, "&&");
            } else if (peekChar() == '=') {
                consumeChar();
                return Token(TokenKind::AmpEqual, startLoc, "&=");
            }
            return Token(TokenKind::Amp, startLoc, "&");
            
        case '|':
            consumeChar();
            if (peekChar() == '|') {
                consumeChar();
                return Token(TokenKind::PipePipe, startLoc, "||");
            } else if (peekChar() == '=') {
                consumeChar();
                return Token(TokenKind::PipeEqual, startLoc, "|=");
            }
            return Token(TokenKind::Pipe, startLoc, "|");
            
        case '^':
            consumeChar();
            if (peekChar() == '=') {
                consumeChar();
                return Token(TokenKind::CaretEqual, startLoc, "^=");
            }
            return Token(TokenKind::Caret, startLoc, "^");
            
        case '~':
            consumeChar();
            return Token(TokenKind::Tilde, startLoc, "~");
            
        case '!':
            consumeChar();
            if (peekChar() == '=') {
                consumeChar();
                return Token(TokenKind::ExclaimEqual, startLoc, "!=");
            }
            return Token(TokenKind::Exclaim, startLoc, "!");
            
        case '=':
            consumeChar();
            if (peekChar() == '=') {
                consumeChar();
                return Token(TokenKind::EqualEqual, startLoc, "==");
            } else if (peekChar() == '>') {
                consumeChar();
                return Token(TokenKind::FatArrow, startLoc, "=>");
            }
            return Token(TokenKind::Equal, startLoc, "=");
            
        case '<':
            consumeChar();
            if (peekChar() == '=') {
                consumeChar();
                return Token(TokenKind::LessEqual, startLoc, "<=");
            } else if (peekChar() == '<') {
                consumeChar();
                if (peekChar() == '=') {
                    consumeChar();
                    return Token(TokenKind::LessLessEqual, startLoc, "<<=");
                }
                return Token(TokenKind::LessLess, startLoc, "<<");
            }
            return Token(TokenKind::Less, startLoc, "<");
            
        case '>':
            consumeChar();
            if (peekChar() == '=') {
                consumeChar();
                return Token(TokenKind::GreaterEqual, startLoc, ">=");
            } else if (peekChar() == '>') {
                consumeChar();
                if (peekChar() == '=') {
                    consumeChar();
                    return Token(TokenKind::GreaterGreaterEqual, startLoc, ">>=");
                }
                return Token(TokenKind::GreaterGreater, startLoc, ">>");
            }
            return Token(TokenKind::Greater, startLoc, ">");
            
        case '.':
            consumeChar();
            if (peekChar() == '.') {
                consumeChar();
                if (peekChar() == '.') {
                    consumeChar();
                    return Token(TokenKind::Ellipsis, startLoc, "...");
                }
                if (peekChar() == '=') {
                    consumeChar();
                    return Token(TokenKind::DotDotEqual, startLoc, "..=");
                }
                return Token(TokenKind::DotDot, startLoc, "..");
            }
            return Token(TokenKind::Dot, startLoc, ".");
            
        case '?':
            consumeChar();
            if (peekChar() == '.') {
                consumeChar();
                return Token(TokenKind::QuestionDot, startLoc, "?.");
            }
            return Token(TokenKind::Question, startLoc, "?");
            
        // 标点符号
        case '(':
            consumeChar();
            return Token(TokenKind::LParen, startLoc, "(");
            
        case ')':
            consumeChar();
            return Token(TokenKind::RParen, startLoc, ")");
            
        case '[':
            consumeChar();
            return Token(TokenKind::LBracket, startLoc, "[");
            
        case ']':
            consumeChar();
            return Token(TokenKind::RBracket, startLoc, "]");
            
        case '{':
            consumeChar();
            return Token(TokenKind::LBrace, startLoc, "{");
            
        case '}':
            consumeChar();
            return Token(TokenKind::RBrace, startLoc, "}");
            
        case ',':
            consumeChar();
            return Token(TokenKind::Comma, startLoc, ",");
            
        case ':':
            consumeChar();
            if (peekChar() == ':') {
                consumeChar();
                return Token(TokenKind::ColonColon, startLoc, "::");
            }
            return Token(TokenKind::Colon, startLoc, ":");
            
        case ';':
            consumeChar();
            return Token(TokenKind::Semicolon, startLoc, ";");
            
        case '@':
            consumeChar();
            return Token(TokenKind::At, startLoc, "@");
            
        case '_':
            consumeChar();
            return Token(TokenKind::Underscore, startLoc, "_");
            
        default:
            // 无效字符，报告错误并跳过，但继续分析
            reportError(DiagID::err_invalid_character, startLoc, std::string(1, c));
            consumeChar();
            // 递归调用lexImpl继续分析下一个token，而不是返回Invalid
            return lexImpl();
    }
}

} // namespace yuan
