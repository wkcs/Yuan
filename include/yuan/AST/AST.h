/// \file AST.h
/// \brief AST 节点基类定义。
///
/// 本文件定义了所有 AST 节点的基类、Kind 枚举、Visibility 枚举和 GenericParam 结构。

#ifndef YUAN_AST_AST_H
#define YUAN_AST_AST_H

#include "yuan/Basic/SourceLocation.h"
#include <string>
#include <vector>

namespace yuan {

// 前向声明
class TypeNode;
class Type;  // 语义类型

/// \brief 可见性修饰符
enum class Visibility {
    Private,    ///< 私有（默认）
    Public,     ///< 公开
    Internal    ///< 模块内部可见
};

/// \brief 泛型参数
struct GenericParam {
    std::string Name;                    ///< 参数名称
    std::vector<std::string> Bounds;     ///< Trait 约束列表
    SourceLocation Loc;                  ///< 参数位置
    
    GenericParam() = default;
    GenericParam(const std::string& name, SourceLocation loc)
        : Name(name), Loc(loc) {}
    GenericParam(const std::string& name, 
                 const std::vector<std::string>& bounds,
                 SourceLocation loc)
        : Name(name), Bounds(bounds), Loc(loc) {}
};

/// \brief AST 节点基类
///
/// 所有 AST 节点都继承自此类。每个节点都有一个 Kind 枚举值
/// 用于 RTTI，以及一个 SourceRange 表示其在源码中的位置。
class ASTNode {
public:
    /// \brief AST 节点类型枚举
    enum class Kind {
        // ===== 声明节点 =====
        VarDecl,            ///< 变量声明
        ConstDecl,          ///< 常量声明
        FuncDecl,           ///< 函数声明
        ParamDecl,          ///< 参数声明
        StructDecl,         ///< 结构体声明
        FieldDecl,          ///< 字段声明
        EnumDecl,           ///< 枚举声明
        EnumVariantDecl,    ///< 枚举变体声明
        TraitDecl,          ///< Trait 声明
        ImplDecl,           ///< Impl 块
        TypeAliasDecl,      ///< 类型别名声明
        
        // ===== 语句节点 =====
        DeclStmt,           ///< 声明语句
        ExprStmt,           ///< 表达式语句
        ReturnStmt,         ///< return 语句
        IfStmt,             ///< if 语句
        WhileStmt,          ///< while 语句
        LoopStmt,           ///< loop 语句
        ForStmt,            ///< for 语句
        BreakStmt,          ///< break 语句
        ContinueStmt,       ///< continue 语句
        DeferStmt,          ///< defer 语句
        BlockStmt,          ///< 块语句
        MatchStmt,          ///< match 语句
        
        // ===== 表达式节点 =====
        IntegerLiteralExpr,     ///< 整数字面量
        FloatLiteralExpr,       ///< 浮点数字面量
        BoolLiteralExpr,        ///< 布尔字面量
        CharLiteralExpr,        ///< 字符字面量
        StringLiteralExpr,      ///< 字符串字面量
        NoneLiteralExpr,        ///< None 字面量
        IdentifierExpr,         ///< 标识符表达式
        BinaryExpr,             ///< 二元表达式
        UnaryExpr,              ///< 一元表达式
        CallExpr,               ///< 函数调用表达式
        BuiltinCallExpr,        ///< 内置函数调用表达式 (@import, @sizeof 等)
        MemberExpr,             ///< 成员访问表达式
        OptionalChainingExpr,   ///< 可选链表达式
        IndexExpr,              ///< 索引表达式
        SliceExpr,              ///< 切片表达式
        CastExpr,               ///< 类型转换表达式
        BlockExpr,              ///< 块表达式
        LoopExpr,               ///< loop 表达式
        IfExpr,                 ///< if 表达式
        MatchExpr,              ///< match 表达式
        ClosureExpr,            ///< 闭包表达式
        ArrayExpr,              ///< 数组表达式
        TupleExpr,              ///< 元组表达式
        StructExpr,             ///< 结构体表达式
        RangeExpr,              ///< 范围表达式
        AssignExpr,             ///< 赋值表达式
        AwaitExpr,              ///< await 表达式
        ErrorPropagateExpr,     ///< 错误传播表达式 (!)
        ErrorHandleExpr,        ///< 错误处理表达式 (-> err {})
        
        // ===== 类型节点 =====
        BuiltinTypeNode,        ///< 内置类型
        IdentifierTypeNode,     ///< 标识符类型
        ArrayTypeNode,          ///< 数组类型
        SliceTypeNode,          ///< 切片类型
        TupleTypeNode,          ///< 元组类型
        OptionalTypeNode,       ///< Optional 类型
        ReferenceTypeNode,      ///< 引用类型
        PointerTypeNode,        ///< 指针类型
        FunctionTypeNode,       ///< 函数类型
        GenericTypeNode,        ///< 泛型类型
        ErrorTypeNode,          ///< 错误类型 (!T)
        
        // ===== 模式节点 =====
        WildcardPattern,        ///< 通配符模式 (_)
        IdentifierPattern,      ///< 标识符模式
        LiteralPattern,         ///< 字面量模式
        TuplePattern,           ///< 元组模式
        StructPattern,          ///< 结构体模式
        EnumPattern,            ///< 枚举模式
        RangePattern,           ///< 范围模式
        OrPattern,              ///< 或模式
        BindPattern,            ///< 绑定模式
    };
    
    /// \brief 构造 AST 节点
    /// \param kind 节点类型
    /// \param range 源码范围
    explicit ASTNode(Kind kind, SourceRange range)
        : NodeKind(kind), Range(range) {}
    
    /// \brief 虚析构函数
    virtual ~ASTNode() = default;
    
    /// \brief 获取节点类型
    Kind getKind() const { return NodeKind; }
    
    /// \brief 获取源码范围
    SourceRange getRange() const { return Range; }
    
    /// \brief 设置源码范围
    void setRange(SourceRange range) { Range = range; }
    
    /// \brief 获取起始位置
    SourceLocation getBeginLoc() const { return Range.getBegin(); }
    
    /// \brief 获取结束位置
    SourceLocation getEndLoc() const { return Range.getEnd(); }
    
    /// \brief 获取节点类型的字符串表示
    static const char* getKindName(Kind kind);
    
    /// \brief 检查节点是否为声明
    bool isDecl() const {
        return NodeKind >= Kind::VarDecl && NodeKind <= Kind::TypeAliasDecl;
    }
    
    /// \brief 检查节点是否为语句
    bool isStmt() const {
        return NodeKind >= Kind::ExprStmt && NodeKind <= Kind::MatchStmt;
    }
    
    /// \brief 检查节点是否为表达式
    bool isExpr() const {
        return NodeKind >= Kind::IntegerLiteralExpr && 
               NodeKind <= Kind::ErrorHandleExpr;
    }
    
    /// \brief 检查节点是否为类型节点
    bool isTypeNode() const {
        return NodeKind >= Kind::BuiltinTypeNode && 
               NodeKind <= Kind::ErrorTypeNode;
    }
    
    /// \brief 检查节点是否为模式
    bool isPattern() const {
        return NodeKind >= Kind::WildcardPattern &&
               NodeKind <= Kind::BindPattern;
    }

    /// \brief 设置语义类型（由 Sema 调用）
    void setSemanticType(Type* type) { SemanticType = type; }

    /// \brief 获取语义类型
    Type* getSemanticType() const { return SemanticType; }

protected:
    Kind NodeKind;      ///< 节点类型
    SourceRange Range;  ///< 源码范围
    Type* SemanticType = nullptr;  ///< 语义类型（由 Sema 设置）
};

/// \brief 获取 Visibility 的字符串表示
inline const char* getVisibilityName(Visibility vis) {
    switch (vis) {
        case Visibility::Private:  return "priv";
        case Visibility::Public:   return "pub";
        case Visibility::Internal: return "internal";
    }
    return "unknown";
}

} // namespace yuan

#endif // YUAN_AST_AST_H
