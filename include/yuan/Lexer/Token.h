#ifndef YUAN_LEXER_TOKEN_H
#define YUAN_LEXER_TOKEN_H

#include "yuan/Basic/TokenKinds.h"
#include "yuan/Basic/SourceLocation.h"
#include <string>

namespace llvm {
class StringRef;
}

namespace yuan {

/// Token 结构 - 表示词法分析产生的单个 Token
class Token {
public:
    /// 默认构造函数，创建无效 Token
    Token() = default;
    
    /// 构造函数
    /// \param kind Token 类型
    /// \param loc Token 在源码中的位置
    /// \param text Token 的文本内容
    /// \param docComment 关联的文档注释（可为空）
    Token(TokenKind kind, SourceLocation loc, const std::string& text,
          const std::string& docComment = "");
    
    /// 获取 Token 类型
    TokenKind getKind() const { return Kind; }
    
    /// 获取 Token 位置
    SourceLocation getLocation() const { return Loc; }
    
    /// 获取 Token 范围
    SourceRange getRange() const;
    
    /// 获取 Token 文本
    const std::string& getText() const { return Text; }

    /// 获取文档注释（如果有）
    const std::string& getDocComment() const { return DocComment; }

    /// 是否有文档注释
    bool hasDocComment() const { return !DocComment.empty(); }
    
    /// 检查是否为指定类型
    bool is(TokenKind k) const { return Kind == k; }
    
    /// 检查是否不是指定类型
    bool isNot(TokenKind k) const { return Kind != k; }
    
    /// 检查是否为指定类型之一（两个参数）
    bool isOneOf(TokenKind k1, TokenKind k2) const {
        return is(k1) || is(k2);
    }
    
    /// 检查是否为指定类型之一（多个参数）
    template<typename... Ts>
    bool isOneOf(TokenKind k1, TokenKind k2, Ts... ks) const {
        return is(k1) || isOneOf(k2, ks...);
    }
    
    /// 检查是否为关键字
    bool isKeyword() const;
    
    /// 检查是否为字面量
    bool isLiteral() const;
    
    /// 检查是否为运算符
    bool isOperator() const;
    
    /// 检查是否为有效 Token
    bool isValid() const { return Kind != TokenKind::Invalid; }
    
    /// 检查是否为文件结束
    bool isEOF() const { return Kind == TokenKind::EndOfFile; }
    
    /// 获取 Token 类型的字符串表示（用于调试）
    const char* getKindName() const { return getTokenName(Kind); }
    
    /// 获取 Token 类型的拼写
    const char* getSpelling() const { return yuan::getSpelling(Kind); }
    
private:
    TokenKind Kind = TokenKind::Invalid;
    SourceLocation Loc;
    std::string Text;
    std::string DocComment;
};

} // namespace yuan

#endif // YUAN_LEXER_TOKEN_H
