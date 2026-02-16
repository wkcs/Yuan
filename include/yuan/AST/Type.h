/// \file Type.h
/// \brief 类型 AST 节点定义。
///
/// 本文件定义了所有类型相关的 AST 节点，包括内置类型、标识符类型、
/// 数组类型、切片类型、元组类型、Optional 类型、引用类型、指针类型、
/// 函数类型、泛型类型和错误类型。

#ifndef YUAN_AST_TYPE_H
#define YUAN_AST_TYPE_H

#include "yuan/AST/AST.h"
#include <string>
#include <vector>

namespace yuan {

// 前向声明
class Expr;

/// \brief 类型节点基类
///
/// 所有类型节点都继承自此类。类型节点表示程序中的各种类型注解，
/// 如基本类型、数组类型、函数类型等。
class TypeNode : public ASTNode {
public:
    /// \brief 构造类型节点
    /// \param kind 节点类型
    /// \param range 源码范围
    TypeNode(Kind kind, SourceRange range)
        : ASTNode(kind, range) {}
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->isTypeNode();
    }
};


// ============================================================================
// 基本类型节点
// ============================================================================

/// \brief 内置类型节点
///
/// 表示内置类型，例如：
/// - i32, u64, f64
/// - bool, char, str
/// - void
class BuiltinTypeNode : public TypeNode {
public:
    /// \brief 内置类型种类
    enum class BuiltinKind {
        Void,       ///< void 类型
        Bool,       ///< bool 类型
        Char,       ///< char 类型
        Str,        ///< str 类型
        I8,         ///< i8 类型
        I16,        ///< i16 类型
        I32,        ///< i32 类型
        I64,        ///< i64 类型
        I128,       ///< i128 类型
        ISize,      ///< isize 类型
        U8,         ///< u8 类型
        U16,        ///< u16 类型
        U32,        ///< u32 类型
        U64,        ///< u64 类型
        U128,       ///< u128 类型
        USize,      ///< usize 类型
        F32,        ///< f32 类型
        F64,        ///< f64 类型
    };
    
    /// \brief 构造内置类型节点
    /// \param range 源码范围
    /// \param kind 内置类型种类
    BuiltinTypeNode(SourceRange range, BuiltinKind kind);
    
    /// \brief 获取内置类型种类
    BuiltinKind getBuiltinKind() const { return BKind; }
    
    /// \brief 获取内置类型的字符串表示
    static const char* getBuiltinKindName(BuiltinKind kind);
    
    /// \brief 是否为整数类型
    bool isInteger() const;
    
    /// \brief 是否为有符号整数类型
    bool isSignedInteger() const;
    
    /// \brief 是否为无符号整数类型
    bool isUnsignedInteger() const;
    
    /// \brief 是否为浮点类型
    bool isFloatingPoint() const;
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::BuiltinTypeNode;
    }
    
private:
    BuiltinKind BKind;  ///< 内置类型种类
};

/// \brief 标识符类型节点
///
/// 表示用户定义的类型名称，例如：
/// - Point
/// - Vec<T>
/// - std::io::File
class IdentifierTypeNode : public TypeNode {
public:
    /// \brief 构造标识符类型节点
    /// \param range 源码范围
    /// \param name 类型名称
    IdentifierTypeNode(SourceRange range, const std::string& name);
    
    /// \brief 获取类型名称
    const std::string& getName() const { return Name; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::IdentifierTypeNode;
    }
    
private:
    std::string Name;  ///< 类型名称
};


// ============================================================================
// 复合类型节点
// ============================================================================

/// \brief 数组类型节点
///
/// 表示固定大小的数组类型，例如：
/// - [i32; 10]
/// - [u8; 256]
class ArrayTypeNode : public TypeNode {
public:
    /// \brief 构造数组类型节点
    /// \param range 源码范围
    /// \param element 元素类型
    /// \param size 数组大小表达式
    ArrayTypeNode(SourceRange range, TypeNode* element, Expr* size);
    
    /// \brief 获取元素类型
    TypeNode* getElementType() const { return Element; }
    
    /// \brief 获取数组大小表达式
    Expr* getSize() const { return Size; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::ArrayTypeNode;
    }
    
private:
    TypeNode* Element;  ///< 元素类型
    Expr* Size;         ///< 数组大小表达式
};

/// \brief 切片类型节点
///
/// 表示切片类型，例如：
/// - &[i32]
/// - &mut [u8]
class SliceTypeNode : public TypeNode {
public:
    /// \brief 构造切片类型节点
    /// \param range 源码范围
    /// \param element 元素类型
    /// \param isMut 是否可变
    SliceTypeNode(SourceRange range, TypeNode* element, bool isMut);
    
    /// \brief 获取元素类型
    TypeNode* getElementType() const { return Element; }
    
    /// \brief 是否可变
    bool isMutable() const { return IsMut; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::SliceTypeNode;
    }
    
private:
    TypeNode* Element;  ///< 元素类型
    bool IsMut;         ///< 是否可变
};

/// \brief 元组类型节点
///
/// 表示元组类型，例如：
/// - (i32, str)
/// - (i32, i32, i32)
/// - ()  // 空元组/unit 类型
class TupleTypeNode : public TypeNode {
public:
    /// \brief 构造元组类型节点
    /// \param range 源码范围
    /// \param elements 元素类型列表
    TupleTypeNode(SourceRange range, std::vector<TypeNode*> elements);
    
    /// \brief 获取元素类型列表
    const std::vector<TypeNode*>& getElements() const { return Elements; }
    
    /// \brief 获取元素数量
    size_t getElementCount() const { return Elements.size(); }
    
    /// \brief 是否为空元组（unit 类型）
    bool isUnit() const { return Elements.empty(); }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::TupleTypeNode;
    }
    
private:
    std::vector<TypeNode*> Elements;  ///< 元素类型列表
};

/// \brief Optional 类型节点
///
/// 表示可选类型，例如：
/// - ?i32
/// - ?String
class OptionalTypeNode : public TypeNode {
public:
    /// \brief 构造 Optional 类型节点
    /// \param range 源码范围
    /// \param inner 内部类型
    OptionalTypeNode(SourceRange range, TypeNode* inner);
    
    /// \brief 获取内部类型
    TypeNode* getInnerType() const { return Inner; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::OptionalTypeNode;
    }
    
private:
    TypeNode* Inner;  ///< 内部类型
};


// ============================================================================
// 引用和指针类型节点
// ============================================================================

/// \brief 引用类型节点
///
/// 表示引用类型，例如：
/// - &i32
/// - &mut String
class ReferenceTypeNode : public TypeNode {
public:
    /// \brief 构造引用类型节点
    /// \param range 源码范围
    /// \param pointee 被引用的类型
    /// \param isMut 是否可变
    ReferenceTypeNode(SourceRange range, TypeNode* pointee, bool isMut);
    
    /// \brief 获取被引用的类型
    TypeNode* getPointeeType() const { return Pointee; }
    
    /// \brief 是否可变
    bool isMutable() const { return IsMut; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::ReferenceTypeNode;
    }
    
private:
    TypeNode* Pointee;  ///< 被引用的类型
    bool IsMut;         ///< 是否可变
};

/// \brief 指针类型节点
///
/// 表示指针类型，例如：
/// - *i32
/// - *mut u8
class PointerTypeNode : public TypeNode {
public:
    /// \brief 构造指针类型节点
    /// \param range 源码范围
    /// \param pointee 被指向的类型
    /// \param isMut 是否可变
    PointerTypeNode(SourceRange range, TypeNode* pointee, bool isMut);
    
    /// \brief 获取被指向的类型
    TypeNode* getPointeeType() const { return Pointee; }
    
    /// \brief 是否可变
    bool isMutable() const { return IsMut; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::PointerTypeNode;
    }
    
private:
    TypeNode* Pointee;  ///< 被指向的类型
    bool IsMut;         ///< 是否可变
};


// ============================================================================
// 函数和错误类型节点
// ============================================================================

/// \brief 函数类型节点
///
/// 表示函数类型，例如：
/// - func(i32, i32) -> i32
/// - func() -> void
/// - func(str) -> !i32  // 可能返回错误
class FunctionTypeNode : public TypeNode {
public:
    /// \brief 构造函数类型节点
    /// \param range 源码范围
    /// \param params 参数类型列表
    /// \param returnType 返回类型
    /// \param canError 是否可能返回错误
    FunctionTypeNode(SourceRange range,
                     std::vector<TypeNode*> params,
                     TypeNode* returnType,
                     bool canError);
    
    /// \brief 获取参数类型列表
    const std::vector<TypeNode*>& getParamTypes() const { return Params; }
    
    /// \brief 获取参数数量
    size_t getParamCount() const { return Params.size(); }
    
    /// \brief 获取返回类型
    TypeNode* getReturnType() const { return ReturnType; }
    
    /// \brief 是否可能返回错误
    bool canError() const { return CanError; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::FunctionTypeNode;
    }
    
private:
    std::vector<TypeNode*> Params;  ///< 参数类型列表
    TypeNode* ReturnType;           ///< 返回类型
    bool CanError;                  ///< 是否可能返回错误
};

/// \brief 错误返回类型节点
///
/// 表示可能返回错误的类型，例如：
/// - !i32
/// - !String
class ErrorTypeNode : public TypeNode {
public:
    /// \brief 构造错误返回类型节点
    /// \param range 源码范围
    /// \param successType 成功时的类型
    ErrorTypeNode(SourceRange range, TypeNode* successType);
    
    /// \brief 获取成功时的类型
    TypeNode* getSuccessType() const { return SuccessType; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::ErrorTypeNode;
    }
    
private:
    TypeNode* SuccessType;  ///< 成功时的类型
};

/// \brief 泛型类型节点
///
/// 表示带有类型参数的泛型类型，例如：
/// - Vec<i32>
/// - HashMap<String, i32>
/// - Result<T, E>
class GenericTypeNode : public TypeNode {
public:
    /// \brief 构造泛型类型节点
    /// \param range 源码范围
    /// \param base 基础类型名称
    /// \param typeArgs 类型参数列表
    GenericTypeNode(SourceRange range,
                    const std::string& base,
                    std::vector<TypeNode*> typeArgs);
    
    /// \brief 获取基础类型名称
    const std::string& getBaseName() const { return BaseName; }
    
    /// \brief 获取类型参数列表
    const std::vector<TypeNode*>& getTypeArgs() const { return TypeArgs; }
    
    /// \brief 获取类型参数数量
    size_t getTypeArgCount() const { return TypeArgs.size(); }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::GenericTypeNode;
    }
    
private:
    std::string BaseName;               ///< 基础类型名称
    std::vector<TypeNode*> TypeArgs;    ///< 类型参数列表
};

} // namespace yuan

#endif // YUAN_AST_TYPE_H
