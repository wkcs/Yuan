/// \file Pattern.h
/// \brief 模式 AST 节点定义。
///
/// 本文件定义了所有模式相关的 AST 节点，包括通配符模式、标识符模式、
/// 字面量模式、元组模式、结构体模式、枚举模式和范围模式。

#ifndef YUAN_AST_PATTERN_H
#define YUAN_AST_PATTERN_H

#include "yuan/AST/AST.h"
#include <string>
#include <vector>

namespace yuan {

// 前向声明
class Expr;
class TypeNode;

/// \brief 模式节点基类
///
/// 所有模式节点都继承自此类。模式用于 match 表达式、for 循环、
/// 变量解构等场景中的模式匹配。
class Pattern : public ASTNode {
public:
    /// \brief 构造模式节点
    /// \param kind 节点类型
    /// \param range 源码范围
    Pattern(Kind kind, SourceRange range)
        : ASTNode(kind, range) {}
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->isPattern();
    }
};


// ============================================================================
// 基本模式
// ============================================================================

/// \brief 通配符模式
///
/// 表示通配符模式 `_`，匹配任何值但不绑定变量。
/// 例如：
/// - match x { _ => "any" }
/// - var (_, y) = pair
class WildcardPattern : public Pattern {
public:
    /// \brief 构造通配符模式
    /// \param range 源码范围
    explicit WildcardPattern(SourceRange range);
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::WildcardPattern;
    }
};


/// \brief 标识符模式
///
/// 表示标识符模式，将匹配的值绑定到变量。
/// 例如：
/// - match x { n => n + 1 }
/// - var (a, b) = pair
/// - for item in list { ... }
class IdentifierPattern : public Pattern {
public:
    /// \brief 构造标识符模式
    /// \param range 源码范围
    /// \param name 标识符名称
    /// \param isMutable 是否可变
    /// \param type 类型注解（可为 nullptr）
    IdentifierPattern(SourceRange range, 
                      const std::string& name,
                      bool isMutable = false,
                      TypeNode* type = nullptr);
    
    /// \brief 获取标识符名称
    const std::string& getName() const { return Name; }
    
    /// \brief 是否可变
    bool isMutable() const { return IsMutable; }
    
    /// \brief 获取类型注解
    TypeNode* getType() const { return Type; }
    
    /// \brief 是否有类型注解
    bool hasType() const { return Type != nullptr; }

    /// \brief 设置关联的声明
    void setDecl(class Decl* decl) { DeclNode = decl; }

    /// \brief 获取关联的声明
    class Decl* getDecl() const { return DeclNode; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::IdentifierPattern;
    }
    
private:
    std::string Name;   ///< 标识符名称
    bool IsMutable;     ///< 是否可变
    TypeNode* Type;     ///< 类型注解
    class Decl* DeclNode; ///< 关联的声明
};


/// \brief 字面量模式
///
/// 表示字面量模式，匹配特定的字面量值。
/// 例如：
/// - match x { 0 => "zero", 1 => "one" }
/// - match ch { 'a' => ..., 'b' => ... }
/// - match s { "hello" => ... }
class LiteralPattern : public Pattern {
public:
    /// \brief 构造字面量模式
    /// \param range 源码范围
    /// \param literal 字面量表达式
    LiteralPattern(SourceRange range, Expr* literal);
    
    /// \brief 获取字面量表达式
    Expr* getLiteral() const { return Literal; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::LiteralPattern;
    }
    
private:
    Expr* Literal;  ///< 字面量表达式
};


// ============================================================================
// 复合模式
// ============================================================================

/// \brief 元组模式
///
/// 表示元组解构模式。
/// 例如：
/// - match point { (0, 0) => "origin" }
/// - var (x, y, z) = tuple
/// - match pair { (a, _) => a }
class TuplePattern : public Pattern {
public:
    /// \brief 构造元组模式
    /// \param range 源码范围
    /// \param elements 元素模式列表
    TuplePattern(SourceRange range, std::vector<Pattern*> elements);
    
    /// \brief 获取元素模式列表
    const std::vector<Pattern*>& getElements() const { return Elements; }
    
    /// \brief 获取元素数量
    size_t getElementCount() const { return Elements.size(); }
    
    /// \brief 是否为空元组模式
    bool isEmpty() const { return Elements.empty(); }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::TuplePattern;
    }
    
private:
    std::vector<Pattern*> Elements;  ///< 元素模式列表
};


/// \brief 结构体模式字段
struct StructPatternField {
    std::string Name;       ///< 字段名
    Pattern* Pat;           ///< 字段模式（可为 nullptr，表示简写形式）
    SourceLocation Loc;     ///< 字段位置
    
    StructPatternField() : Pat(nullptr) {}
    StructPatternField(const std::string& name, Pattern* pat, SourceLocation loc)
        : Name(name), Pat(pat), Loc(loc) {}
};

/// \brief 结构体模式
///
/// 表示结构体解构模式。
/// 例如：
/// - match point { Point { x: 0, y } => ... }
/// - var Person { name, age } = person
/// - match rect { Rect { width, .. } => width }
class StructPattern : public Pattern {
public:
    /// \brief 构造结构体模式
    /// \param range 源码范围
    /// \param typeName 结构体类型名
    /// \param fields 字段模式列表
    /// \param hasRest 是否有 .. 省略其余字段
    StructPattern(SourceRange range,
                  const std::string& typeName,
                  std::vector<StructPatternField> fields,
                  bool hasRest = false);
    
    /// \brief 获取结构体类型名
    const std::string& getTypeName() const { return TypeName; }
    
    /// \brief 获取字段模式列表
    const std::vector<StructPatternField>& getFields() const { return Fields; }
    
    /// \brief 获取字段数量
    size_t getFieldCount() const { return Fields.size(); }
    
    /// \brief 是否有 .. 省略其余字段
    bool hasRest() const { return HasRest; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::StructPattern;
    }
    
private:
    std::string TypeName;                   ///< 结构体类型名
    std::vector<StructPatternField> Fields; ///< 字段模式列表
    bool HasRest;                           ///< 是否有 .. 省略
};


/// \brief 枚举模式
///
/// 表示枚举变体解构模式。
/// 例如：
/// - match color { Color.Red => ... }
/// - match result { Result.Ok(value) => value }
/// - match option { Some(x) => x, None => 0 }
class EnumPattern : public Pattern {
public:
    /// \brief 构造枚举模式
    /// \param range 源码范围
    /// \param enumName 枚举类型名（可为空，表示省略）
    /// \param variantName 变体名
    /// \param payload 负载模式列表（可为空）
    EnumPattern(SourceRange range,
                const std::string& enumName,
                const std::string& variantName,
                std::vector<Pattern*> payload = {});
    
    /// \brief 获取枚举类型名
    const std::string& getEnumName() const { return EnumName; }
    
    /// \brief 获取变体名
    const std::string& getVariantName() const { return VariantName; }
    
    /// \brief 获取负载模式列表
    const std::vector<Pattern*>& getPayload() const { return Payload; }
    
    /// \brief 是否有负载
    bool hasPayload() const { return !Payload.empty(); }
    
    /// \brief 获取负载数量
    size_t getPayloadCount() const { return Payload.size(); }
    
    /// \brief 是否指定了枚举类型名
    bool hasEnumName() const { return !EnumName.empty(); }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::EnumPattern;
    }
    
private:
    std::string EnumName;           ///< 枚举类型名
    std::string VariantName;        ///< 变体名
    std::vector<Pattern*> Payload;  ///< 负载模式列表
};


/// \brief 范围模式
///
/// 表示范围模式，匹配指定范围内的值。
/// 例如：
/// - match age { 0..=12 => "child", 13..=19 => "teen" }
/// - match ch { 'a'..='z' => "lowercase" }
class RangePattern : public Pattern {
public:
    /// \brief 构造范围模式
    /// \param range 源码范围
    /// \param start 起始值表达式
    /// \param end 结束值表达式
    /// \param isInclusive 是否包含结束值（..= vs ..）
    RangePattern(SourceRange range, 
                 Expr* start, 
                 Expr* end,
                 bool isInclusive = true);
    
    /// \brief 获取起始值表达式
    Expr* getStart() const { return Start; }
    
    /// \brief 获取结束值表达式
    Expr* getEnd() const { return End; }
    
    /// \brief 是否包含结束值
    bool isInclusive() const { return IsInclusive; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::RangePattern;
    }
    
private:
    Expr* Start;        ///< 起始值表达式
    Expr* End;          ///< 结束值表达式
    bool IsInclusive;   ///< 是否包含结束值
};

// ============================================================================
// 组合模式
// ============================================================================

/// \brief 或模式
///
/// 表示多个模式的逻辑或（p1 | p2 | ...）。
class OrPattern : public Pattern {
public:
    /// \brief 构造或模式
    /// \param range 源码范围
    /// \param patterns 备选模式列表
    OrPattern(SourceRange range, std::vector<Pattern*> patterns);

    /// \brief 获取备选模式列表
    const std::vector<Pattern*>& getPatterns() const { return Patterns; }

    /// \brief 获取备选模式数量
    size_t getPatternCount() const { return Patterns.size(); }

    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::OrPattern;
    }

private:
    std::vector<Pattern*> Patterns;
};

/// \brief 绑定模式
///
/// 表示 name @ pattern 形式，将匹配的值绑定到 name。
class BindPattern : public Pattern {
public:
    /// \brief 构造绑定模式
    /// \param range 源码范围
    /// \param name 绑定名
    /// \param inner 内部模式
    /// \param isMutable 是否可变
    /// \param type 类型注解（可为 nullptr）
    BindPattern(SourceRange range,
                const std::string& name,
                Pattern* inner,
                bool isMutable = false,
                TypeNode* type = nullptr);

    /// \brief 获取绑定名
    const std::string& getName() const { return Name; }

    /// \brief 获取内部模式
    Pattern* getInner() const { return Inner; }

    /// \brief 是否可变
    bool isMutable() const { return IsMutable; }

    /// \brief 获取类型注解
    TypeNode* getType() const { return Type; }

    /// \brief 是否有类型注解
    bool hasType() const { return Type != nullptr; }

    /// \brief 设置关联的声明
    void setDecl(class Decl* decl) { DeclNode = decl; }

    /// \brief 获取关联的声明
    class Decl* getDecl() const { return DeclNode; }

    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::BindPattern;
    }

private:
    std::string Name;
    Pattern* Inner;
    bool IsMutable;
    TypeNode* Type;
    class Decl* DeclNode = nullptr;
};

} // namespace yuan

#endif // YUAN_AST_PATTERN_H
