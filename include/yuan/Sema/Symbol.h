//===--- Symbol.h - 符号定义 -------------------------------------------===//
//
// Yuan 编译器
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief 符号定义，用于符号表管理
///
//===----------------------------------------------------------------------===//

#ifndef YUAN_SEMA_SYMBOL_H
#define YUAN_SEMA_SYMBOL_H

#include "yuan/Basic/SourceLocation.h"
#include "yuan/AST/AST.h"
#include "llvm/ADT/StringRef.h"
#include <string>

namespace yuan {

class Type;
class Decl;

/// 符号类型
enum class SymbolKind {
    Variable,       ///< 变量
    Constant,       ///< 常量
    Function,       ///< 函数
    Parameter,      ///< 参数
    Struct,         ///< 结构体
    Enum,           ///< 枚举
    EnumVariant,    ///< 枚举变体
    Trait,          ///< Trait
    TypeAlias,      ///< 类型别名
    Field,          ///< 字段
    Method,         ///< 方法
    GenericParam,   ///< 泛型参数
};

/// 符号
/// 表示程序中的一个命名实体，包含其类型、位置、可见性等信息
class Symbol {
public:
    /// 构造符号
    /// \param kind 符号类型
    /// \param name 符号名称
    /// \param type 符号的类型
    /// \param loc 符号定义位置
    /// \param vis 可见性
    Symbol(SymbolKind kind, llvm::StringRef name, Type* type,
           SourceLocation loc, Visibility vis = Visibility::Private);

    /// 获取符号类型
    SymbolKind getKind() const { return Kind; }

    /// 获取符号名称
    llvm::StringRef getName() const { return Name; }

    /// 获取符号的类型
    Type* getType() const { return SymType; }

    /// 获取符号定义位置
    SourceLocation getLocation() const { return Loc; }

    /// 获取可见性
    Visibility getVisibility() const { return Vis; }

    /// 是否可变
    bool isMutable() const { return IsMutable; }

    /// 设置可变性
    void setMutable(bool mut) { IsMutable = mut; }

    /// 获取关联的声明节点
    Decl* getDecl() const { return DeclNode; }

    /// 设置关联的声明节点
    void setDecl(Decl* decl) { DeclNode = decl; }

    /// 是否为变量或常量
    bool isVariable() const { 
        return Kind == SymbolKind::Variable || Kind == SymbolKind::Constant; 
    }

    /// 是否为函数
    bool isFunction() const { return Kind == SymbolKind::Function; }

    /// 是否为类型
    bool isType() const {
        return Kind == SymbolKind::Struct || Kind == SymbolKind::Enum ||
               Kind == SymbolKind::Trait || Kind == SymbolKind::TypeAlias;
    }

    /// 获取符号类型的字符串表示
    static const char* getKindName(SymbolKind kind);

private:
    SymbolKind Kind;        ///< 符号类型
    std::string Name;       ///< 符号名称
    Type* SymType;          ///< 符号的类型
    SourceLocation Loc;     ///< 定义位置
    Visibility Vis;         ///< 可见性
    bool IsMutable = false; ///< 是否可变
    Decl* DeclNode = nullptr; ///< 关联的声明节点
};

} // namespace yuan

#endif // YUAN_SEMA_SYMBOL_H