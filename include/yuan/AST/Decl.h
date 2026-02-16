/// \file Decl.h
/// \brief 声明 AST 节点定义。
///
/// 本文件定义了所有声明相关的 AST 节点，包括变量声明、常量声明、
/// 函数声明、结构体声明、枚举声明、Trait 声明和 Impl 块等。

#ifndef YUAN_AST_DECL_H
#define YUAN_AST_DECL_H

#include "yuan/AST/AST.h"
#include <string>
#include <vector>

namespace yuan {

// 前向声明
class TypeNode;
class Expr;
class BlockStmt;
class Pattern;
class Type;

/// \brief 声明节点基类
///
/// 所有声明节点都继承自此类。声明节点表示程序中的各种声明，
/// 如变量、常量、函数、结构体、枚举等。
class Decl : public ASTNode {
public:
    /// \brief 构造声明节点
    /// \param kind 节点类型
    /// \param range 源码范围
    Decl(Kind kind, SourceRange range)
        : ASTNode(kind, range) {}

    /// \brief 设置文档注释
    void setDocComment(std::string comment) { DocComment = std::move(comment); }

    /// \brief 获取文档注释
    const std::string& getDocComment() const { return DocComment; }

    /// \brief 是否有文档注释
    bool hasDocComment() const { return !DocComment.empty(); }

    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->isDecl();
    }

protected:
    std::string DocComment;
};

/// \brief 变量声明节点
///
/// 表示 var 声明，例如：
/// - var x: i32 = 10
/// - var mut y = "hello"
class VarDecl : public Decl {
public:
    /// \brief 构造变量声明
    /// \param range 源码范围
    /// \param name 变量名
    /// \param type 类型注解（可为 nullptr，表示类型推断）
    /// \param init 初始化表达式（可为 nullptr）
    /// \param isMutable 是否可变
    /// \param vis 可见性
    VarDecl(SourceRange range,
            const std::string& name,
            TypeNode* type,
            Expr* init,
            bool isMutable,
            Visibility vis = Visibility::Private,
            Pattern* pattern = nullptr);

    /// \brief 获取变量名
    const std::string& getName() const { return Name; }

    /// \brief 获取类型注解
    TypeNode* getType() const { return Type; }

    /// \brief 获取初始化表达式
    Expr* getInit() const { return Init; }

    /// \brief 设置初始化表达式
    void setInit(Expr* init) { Init = init; }

    /// \brief 获取解构模式（可为 nullptr）
    Pattern* getPattern() const { return PatternNode; }

    /// \brief 是否包含解构模式
    bool hasPattern() const { return PatternNode != nullptr; }

    /// \brief 是否可变
    bool isMutable() const { return IsMutable; }

    /// \brief 获取可见性
    Visibility getVisibility() const { return Vis; }

    /// \brief 设置类型注解
    void setType(TypeNode* type) { Type = type; }

    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::VarDecl;
    }

private:
    std::string Name;       ///< 变量名
    TypeNode* Type;         ///< 类型注解（可为 nullptr）
    Expr* Init;             ///< 初始化表达式（可为 nullptr）
    bool IsMutable;         ///< 是否可变
    Visibility Vis;         ///< 可见性
    Pattern* PatternNode;   ///< 解构模式（可为 nullptr）
};

/// \brief 常量声明节点
///
/// 表示 const 声明，例如：
/// - const PI: f64 = 3.14159
/// - const MAX_SIZE = 100
class ConstDecl : public Decl {
public:
    /// \brief 构造常量声明
    /// \param range 源码范围
    /// \param name 常量名
    /// \param type 类型注解（可为 nullptr，表示类型推断）
    /// \param init 初始化表达式（必须有）
    /// \param vis 可见性
    ConstDecl(SourceRange range,
              const std::string& name,
              TypeNode* type,
              Expr* init,
              Visibility vis = Visibility::Private);

    /// \brief 获取常量名
    const std::string& getName() const { return Name; }

    /// \brief 获取类型注解
    TypeNode* getType() const { return Type; }

    /// \brief 获取初始化表达式
    Expr* getInit() const { return Init; }

    /// \brief 设置初始化表达式
    void setInit(Expr* init) { Init = init; }

    /// \brief 获取可见性
    Visibility getVisibility() const { return Vis; }

    /// \brief 设置类型注解
    void setType(TypeNode* type) { Type = type; }

    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::ConstDecl;
    }

private:
    std::string Name;       ///< 常量名
    TypeNode* Type;         ///< 类型注解（可为 nullptr）
    Expr* Init;             ///< 初始化表达式
    Visibility Vis;         ///< 可见性
};

/// \brief 参数声明节点
///
/// 表示函数参数或闭包参数，例如：
/// - x: i32
/// - mut y: &str
/// - self
/// - &self
/// - &mut self
class ParamDecl : public Decl {
public:
    /// \brief 参数类型
    enum class ParamKind {
        Normal,     ///< 普通参数
        Self,       ///< self 参数
        RefSelf,    ///< &self 参数
        MutRefSelf, ///< &mut self 参数
        Variadic    ///< 可变参数 (...args)
    };

    /// \brief 构造普通参数声明
    /// \param range 源码范围
    /// \param name 参数名
    /// \param type 参数类型
    /// \param isMutable 是否可变
    ParamDecl(SourceRange range,
              const std::string& name,
              TypeNode* type,
              bool isMutable);

    /// \brief 构造普通参数声明（带默认值）
    /// \param range 源码范围
    /// \param name 参数名
    /// \param type 参数类型
    /// \param defaultValue 默认参数值（可为 nullptr）
    /// \param isMutable 是否可变
    ParamDecl(SourceRange range,
              const std::string& name,
              TypeNode* type,
              Expr* defaultValue = nullptr,
              bool isMutable = false);

    /// \brief 构造可变参数声明
    /// \param range 源码范围
    /// \param name 参数名 (通常是 "args")
    /// \param elementType 元素类型约束 (可为 nullptr)
    static ParamDecl* createVariadic(SourceRange range,
                                      const std::string& name,
                                      TypeNode* elementType = nullptr);

    /// \brief 构造 self 参数声明
    /// \param range 源码范围
    /// \param kind self 参数类型
    static ParamDecl* createSelf(SourceRange range, ParamKind kind);

    /// \brief 获取参数名
    const std::string& getName() const { return Name; }

    /// \brief 获取参数类型
    TypeNode* getType() const { return Type; }

    /// \brief 获取默认参数值
    Expr* getDefaultValue() const { return DefaultValue; }

    /// \brief 是否有默认参数值
    bool hasDefaultValue() const { return DefaultValue != nullptr; }

    /// \brief 是否可变
    bool isMutable() const { return IsMutable; }

    /// \brief 获取参数类型
    ParamKind getParamKind() const { return PK; }

    /// \brief 是否为 self 参数
    bool isSelf() const {
        return PK == ParamKind::Self ||
               PK == ParamKind::RefSelf ||
               PK == ParamKind::MutRefSelf;
    }

    /// \brief 是否为可变参数
    bool isVariadic() const {
        return PK == ParamKind::Variadic;
    }

    /// \brief 设置参数类型
    void setType(TypeNode* type) { Type = type; }

    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::ParamDecl;
    }

private:
    /// \brief 私有构造函数（用于 self 参数）
    ParamDecl(SourceRange range, ParamKind kind);

    std::string Name;       ///< 参数名
    TypeNode* Type;         ///< 参数类型
    Expr* DefaultValue;     ///< 默认参数值（可为 nullptr）
    bool IsMutable;         ///< 是否可变
    ParamKind PK;           ///< 参数类型
};


/// \brief 函数声明节点
///
/// 表示函数定义，例如：
/// - func add(x: i32, y: i32) -> i32 { return x + y }
/// - pub async func fetch(url: str) -> !Response { ... }
/// - func identity<T>(x: T) -> T { return x }
class FuncDecl : public Decl {
public:
    /// \brief 构造函数声明
    /// \param range 源码范围
    /// \param name 函数名
    /// \param params 参数列表
    /// \param returnType 返回类型（可为 nullptr，表示 void）
    /// \param body 函数体（可为 nullptr，表示声明）
    /// \param isAsync 是否为 async 函数
    /// \param canError 返回类型是否为 !T
    /// \param vis 可见性
    FuncDecl(SourceRange range,
             const std::string& name,
             std::vector<ParamDecl*> params,
             TypeNode* returnType,
             BlockStmt* body,
             bool isAsync,
             bool canError,
             Visibility vis);

    /// \brief 获取函数名
    const std::string& getName() const { return Name; }

    /// \brief 获取参数列表
    const std::vector<ParamDecl*>& getParams() const { return Params; }

    /// \brief 获取返回类型
    TypeNode* getReturnType() const { return ReturnType; }

    /// \brief 获取函数体
    BlockStmt* getBody() const { return Body; }

    /// \brief 是否为 async 函数
    bool isAsync() const { return IsAsync; }

    /// \brief 返回类型是否为 !T
    bool canError() const { return CanError; }

    /// \brief 获取可见性
    Visibility getVisibility() const { return Vis; }

    /// \brief 设置泛型参数
    void setGenericParams(std::vector<GenericParam> params) {
        GenericParams = std::move(params);
    }

    /// \brief 获取泛型参数
    const std::vector<GenericParam>& getGenericParams() const {
        return GenericParams;
    }

    /// \brief 是否为泛型函数
    bool isGeneric() const { return !GenericParams.empty(); }

    /// \brief 是否有可变参数
    bool hasVariadicParam() const {
        if (Params.empty()) return false;
        return Params.back()->isVariadic();
    }

    /// \brief 获取可变参数 (如果有)
    /// \return 可变参数,如果没有返回 nullptr
    ParamDecl* getVariadicParam() const {
        if (hasVariadicParam()) {
            return Params.back();
        }
        return nullptr;
    }

    /// \brief 是否有函数体
    bool hasBody() const { return Body != nullptr; }

    /// \brief 设置函数体
    void setBody(BlockStmt* body) { Body = body; }

    /// \brief 设置外部链接名（用于导入桩函数）
    void setLinkName(std::string linkName) { LinkName = std::move(linkName); }

    /// \brief 获取外部链接名
    const std::string& getLinkName() const { return LinkName; }

    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::FuncDecl;
    }

private:
    std::string Name;                       ///< 函数名
    std::vector<ParamDecl*> Params;         ///< 参数列表
    TypeNode* ReturnType;                   ///< 返回类型
    BlockStmt* Body;                        ///< 函数体
    bool IsAsync;                           ///< 是否为 async 函数
    bool CanError;                          ///< 返回类型是否为 !T
    Visibility Vis;                         ///< 可见性
    std::string LinkName;                   ///< 可选外部链接名
    std::vector<GenericParam> GenericParams; ///< 泛型参数
};

/// \brief 字段声明节点
///
/// 表示结构体字段，例如：
/// - name: str
/// - pub age: i32
class FieldDecl : public Decl {
public:
    /// \brief 构造字段声明
    /// \param range 源码范围
    /// \param name 字段名
    /// \param type 字段类型
    /// \param defaultValue 默认值（可为 nullptr）
    /// \param vis 可见性
    FieldDecl(SourceRange range,
              const std::string& name,
              TypeNode* type,
              Expr* defaultValue = nullptr,
              Visibility vis = Visibility::Private);

    /// \brief 获取字段名
    const std::string& getName() const { return Name; }

    /// \brief 获取字段类型
    TypeNode* getType() const { return Type; }

    /// \brief 获取默认值表达式
    Expr* getDefaultValue() const { return DefaultValue; }

    /// \brief 是否有默认值
    bool hasDefaultValue() const { return DefaultValue != nullptr; }

    /// \brief 获取可见性
    Visibility getVisibility() const { return Vis; }

    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::FieldDecl;
    }

private:
    std::string Name;       ///< 字段名
    TypeNode* Type;         ///< 字段类型
    Expr* DefaultValue;     ///< 默认值（可为 nullptr）
    Visibility Vis;         ///< 可见性
};

/// \brief 结构体声明节点
///
/// 表示结构体定义，例如：
/// - struct Point { x: f64, y: f64 }
/// - pub struct Vec<T> { data: *T, len: usize, cap: usize }
class StructDecl : public Decl {
public:
    /// \brief 构造结构体声明
    /// \param range 源码范围
    /// \param name 结构体名
    /// \param fields 字段列表
    /// \param vis 可见性
    StructDecl(SourceRange range,
               const std::string& name,
               std::vector<FieldDecl*> fields,
               Visibility vis);

    /// \brief 获取结构体名
    const std::string& getName() const { return Name; }

    /// \brief 获取字段列表
    const std::vector<FieldDecl*>& getFields() const { return Fields; }

    /// \brief 获取可见性
    Visibility getVisibility() const { return Vis; }

    /// \brief 设置泛型参数
    void setGenericParams(std::vector<GenericParam> params) {
        GenericParams = std::move(params);
    }

    /// \brief 获取泛型参数
    const std::vector<GenericParam>& getGenericParams() const {
        return GenericParams;
    }

    /// \brief 是否为泛型结构体
    bool isGeneric() const { return !GenericParams.empty(); }

    /// \brief 根据名称查找字段
    FieldDecl* findField(const std::string& name) const;

    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::StructDecl;
    }

private:
    std::string Name;                       ///< 结构体名
    std::vector<FieldDecl*> Fields;         ///< 字段列表
    Visibility Vis;                         ///< 可见性
    std::vector<GenericParam> GenericParams; ///< 泛型参数
};


/// \brief 枚举变体声明节点
///
/// 表示枚举变体，例如：
/// - None
/// - Some(T)
/// - Point { x: f64, y: f64 }
class EnumVariantDecl : public Decl {
public:
    /// \brief 变体类型
    enum class VariantKind {
        Unit,       ///< 单元变体，如 None
        Tuple,      ///< 元组变体，如 Some(T)
        Struct      ///< 结构体变体，如 Point { x: f64, y: f64 }
    };

    /// \brief 构造单元变体
    /// \param range 源码范围
    /// \param name 变体名
    static EnumVariantDecl* createUnit(SourceRange range,
                                       const std::string& name);

    /// \brief 构造元组变体
    /// \param range 源码范围
    /// \param name 变体名
    /// \param types 元组类型列表
    static EnumVariantDecl* createTuple(SourceRange range,
                                        const std::string& name,
                                        std::vector<TypeNode*> types);

    /// \brief 构造结构体变体
    /// \param range 源码范围
    /// \param name 变体名
    /// \param fields 字段列表
    static EnumVariantDecl* createStruct(SourceRange range,
                                         const std::string& name,
                                         std::vector<FieldDecl*> fields);

    /// \brief 获取变体名
    const std::string& getName() const { return Name; }

    /// \brief 获取变体类型
    VariantKind getVariantKind() const { return VK; }

    /// \brief 是否为单元变体
    bool isUnit() const { return VK == VariantKind::Unit; }

    /// \brief 是否为元组变体
    bool isTuple() const { return VK == VariantKind::Tuple; }

    /// \brief 是否为结构体变体
    bool isStruct() const { return VK == VariantKind::Struct; }

    /// \brief 获取元组类型列表（仅元组变体有效）
    const std::vector<TypeNode*>& getTupleTypes() const { return TupleTypes; }

    /// \brief 获取字段列表（仅结构体变体有效）
    const std::vector<FieldDecl*>& getFields() const { return Fields; }

    /// \brief 设置判别值
    void setDiscriminant(int64_t value) {
        Discriminant = value;
        HasDiscriminant = true;
    }

    /// \brief 是否有显式判别值
    bool hasDiscriminant() const { return HasDiscriminant; }

    /// \brief 获取判别值
    int64_t getDiscriminant() const { return Discriminant; }

    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::EnumVariantDecl;
    }

private:
    /// \brief 私有构造函数
    EnumVariantDecl(SourceRange range, const std::string& name, VariantKind kind);

    std::string Name;                   ///< 变体名
    VariantKind VK;                     ///< 变体类型
    std::vector<TypeNode*> TupleTypes;  ///< 元组类型列表
    std::vector<FieldDecl*> Fields;     ///< 字段列表
    int64_t Discriminant = 0;           ///< 判别值
    bool HasDiscriminant = false;       ///< 是否有显式判别值
};

/// \brief 枚举声明节点
///
/// 表示枚举定义，例如：
/// - enum Color { Red, Green, Blue }
/// - enum Option<T> { None, Some(T) }
/// - enum Result<T, E> { Ok(T), Err(E) }
class EnumDecl : public Decl {
public:
    /// \brief 构造枚举声明
    /// \param range 源码范围
    /// \param name 枚举名
    /// \param variants 变体列表
    /// \param vis 可见性
    EnumDecl(SourceRange range,
             const std::string& name,
             std::vector<EnumVariantDecl*> variants,
             Visibility vis);

    /// \brief 获取枚举名
    const std::string& getName() const { return Name; }

    /// \brief 获取变体列表
    const std::vector<EnumVariantDecl*>& getVariants() const { return Variants; }

    /// \brief 获取可见性
    Visibility getVisibility() const { return Vis; }

    /// \brief 设置泛型参数
    void setGenericParams(std::vector<GenericParam> params) {
        GenericParams = std::move(params);
    }

    /// \brief 获取泛型参数
    const std::vector<GenericParam>& getGenericParams() const {
        return GenericParams;
    }

    /// \brief 是否为泛型枚举
    bool isGeneric() const { return !GenericParams.empty(); }

    /// \brief 根据名称查找变体
    EnumVariantDecl* findVariant(const std::string& name) const;

    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::EnumDecl;
    }

private:
    std::string Name;                       ///< 枚举名
    std::vector<EnumVariantDecl*> Variants; ///< 变体列表
    Visibility Vis;                         ///< 可见性
    std::vector<GenericParam> GenericParams; ///< 泛型参数
};


/// \brief 类型别名声明节点
///
/// 表示类型别名，例如：
/// - type StringList = Vec<str>
/// - type Result<T> = Result<T, Error>
class TypeAliasDecl : public Decl {
public:
    /// \brief 构造类型别名声明
    /// \param range 源码范围
    /// \param name 别名名称
    /// \param aliasedType 被别名的类型（可为 nullptr，表示关联类型声明）
    /// \param vis 可见性
    TypeAliasDecl(SourceRange range,
                  const std::string& name,
                  TypeNode* aliasedType,
                  Visibility vis = Visibility::Private);

    /// \brief 获取别名名称
    const std::string& getName() const { return Name; }

    /// \brief 获取被别名的类型
    TypeNode* getAliasedType() const { return AliasedType; }

    /// \brief 获取可见性
    Visibility getVisibility() const { return Vis; }

    /// \brief 设置泛型参数
    void setGenericParams(std::vector<GenericParam> params) {
        GenericParams = std::move(params);
    }

    /// \brief 获取泛型参数
    const std::vector<GenericParam>& getGenericParams() const {
        return GenericParams;
    }

    /// \brief 是否为泛型类型别名
    bool isGeneric() const { return !GenericParams.empty(); }

    /// \brief 设置 Trait 约束
    void setTraitBounds(std::vector<std::string> bounds) {
        TraitBounds = std::move(bounds);
    }

    /// \brief 获取 Trait 约束
    const std::vector<std::string>& getTraitBounds() const {
        return TraitBounds;
    }

    /// \brief 是否有 Trait 约束
    bool hasTraitBounds() const { return !TraitBounds.empty(); }

    /// \brief 是否为关联类型声明（无具体类型）
    bool isAssociatedType() const { return AliasedType == nullptr; }

    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::TypeAliasDecl;
    }

private:
    std::string Name;                       ///< 别名名称
    TypeNode* AliasedType;                  ///< 被别名的类型
    Visibility Vis;                         ///< 可见性
    std::vector<GenericParam> GenericParams; ///< 泛型参数
    std::vector<std::string> TraitBounds;   ///< Trait 约束
};

/// \brief Trait 声明节点
///
/// 表示 Trait 定义，例如：
/// - trait Display { func display(&self) -> str }
/// - trait Iterator { type Item; func next(&mut self) -> ?Item }
class TraitDecl : public Decl {
public:
    /// \brief 构造 Trait 声明
    /// \param range 源码范围
    /// \param name Trait 名称
    /// \param methods 方法列表
    /// \param associatedTypes 关联类型列表
    /// \param vis 可见性
    TraitDecl(SourceRange range,
              const std::string& name,
              std::vector<FuncDecl*> methods,
              std::vector<TypeAliasDecl*> associatedTypes,
              Visibility vis);

    /// \brief 获取 Trait 名称
    const std::string& getName() const { return Name; }

    /// \brief 获取方法列表
    const std::vector<FuncDecl*>& getMethods() const { return Methods; }

    /// \brief 获取关联类型列表
    const std::vector<TypeAliasDecl*>& getAssociatedTypes() const {
        return AssociatedTypes;
    }

    /// \brief 获取可见性
    Visibility getVisibility() const { return Vis; }

    /// \brief 设置泛型参数
    void setGenericParams(std::vector<GenericParam> params) {
        GenericParams = std::move(params);
    }

    /// \brief 获取泛型参数
    const std::vector<GenericParam>& getGenericParams() const {
        return GenericParams;
    }

    /// \brief 是否为泛型 Trait
    bool isGeneric() const { return !GenericParams.empty(); }

    /// \brief 设置父 Trait 列表
    void setSuperTraits(std::vector<std::string> traits) {
        SuperTraits = std::move(traits);
    }

    /// \brief 获取父 Trait 列表
    const std::vector<std::string>& getSuperTraits() const {
        return SuperTraits;
    }

    /// \brief 根据名称查找方法
    FuncDecl* findMethod(const std::string& name) const;

    /// \brief 根据名称查找关联类型
    TypeAliasDecl* findAssociatedType(const std::string& name) const;

    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::TraitDecl;
    }

private:
    std::string Name;                           ///< Trait 名称
    std::vector<FuncDecl*> Methods;             ///< 方法列表
    std::vector<TypeAliasDecl*> AssociatedTypes; ///< 关联类型列表
    Visibility Vis;                             ///< 可见性
    std::vector<GenericParam> GenericParams;    ///< 泛型参数
    std::vector<std::string> SuperTraits;       ///< 父 Trait 列表
};

/// \brief Impl 块声明节点
///
/// 表示 impl 块，例如：
/// - impl Point { func new(x: f64, y: f64) -> Point { ... } }
/// - impl Display for Point { func display(&self) -> str { ... } }
class ImplDecl : public Decl {
public:
    /// \brief 构造 Impl 块声明
    /// \param range 源码范围
    /// \param targetType 目标类型
    /// \param traitName 实现的 Trait 名称（可为空，表示固有实现）
    /// \param methods 方法列表
    ImplDecl(SourceRange range,
             TypeNode* targetType,
             const std::string& traitName,
             std::vector<FuncDecl*> methods);

    /// \brief 获取目标类型
    TypeNode* getTargetType() const { return TargetType; }

    /// \brief 设置语义目标类型（由语义分析阶段填充）
    void setSemanticTargetType(Type* type) { SemanticTargetType = type; }

    /// \brief 获取语义目标类型
    Type* getSemanticTargetType() const { return SemanticTargetType; }

    /// \brief 获取实现的 Trait 名称
    const std::string& getTraitName() const { return TraitName; }

    /// \brief 获取方法列表
    const std::vector<FuncDecl*>& getMethods() const { return Methods; }

    /// \brief 是否为 Trait 实现
    bool isTraitImpl() const { return !TraitName.empty(); }

    /// \brief 设置泛型参数
    void setGenericParams(std::vector<GenericParam> params) {
        GenericParams = std::move(params);
    }

    /// \brief 获取泛型参数
    const std::vector<GenericParam>& getGenericParams() const {
        return GenericParams;
    }

    /// \brief 是否为泛型 Impl
    bool isGeneric() const { return !GenericParams.empty(); }

    /// \brief 设置关联类型实现
    void setAssociatedTypes(std::vector<TypeAliasDecl*> types) {
        AssociatedTypes = std::move(types);
    }

    /// \brief 获取关联类型实现
    const std::vector<TypeAliasDecl*>& getAssociatedTypes() const {
        return AssociatedTypes;
    }

    /// \brief 根据名称查找方法
    FuncDecl* findMethod(const std::string& name) const;

    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::ImplDecl;
    }

private:
    TypeNode* TargetType;                       ///< 目标类型
    Type* SemanticTargetType = nullptr;        ///< 语义目标类型
    std::string TraitName;                      ///< 实现的 Trait 名称
    std::vector<FuncDecl*> Methods;             ///< 方法列表
    std::vector<GenericParam> GenericParams;    ///< 泛型参数
    std::vector<TypeAliasDecl*> AssociatedTypes; ///< 关联类型实现
};

} // namespace yuan

#endif // YUAN_AST_DECL_H
