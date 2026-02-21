/// \file TypeChecker.h
/// \brief 类型检查工具。

#ifndef YUAN_SEMA_TYPECHECKER_H
#define YUAN_SEMA_TYPECHECKER_H

#include "yuan/Basic/SourceLocation.h"
#include <cstdint>

namespace yuan {

class Type;
class Expr;
class SymbolTable;
class DiagnosticEngine;

/// \brief 类型检查器。
class TypeChecker {
public:
    /// \brief 构造类型检查器。
    /// \param symbols 符号表引用
    /// \param diag 诊断引擎引用
    TypeChecker(SymbolTable& symbols, DiagnosticEngine& diag)
        : Symbols(symbols), Diag(diag) {}

    /// \brief 检查类型兼容性（使用单点位置）。
    /// \param expected 期望类型
    /// \param actual 实际类型
    /// \param loc 错误位置
    /// \return 兼容返回 true
    bool checkTypeCompatible(Type* expected, Type* actual, SourceLocation loc);

    /// \brief 检查类型兼容性（使用范围位置）。
    /// \param expected 期望类型
    /// \param actual 实际类型
    /// \param range 错误范围
    /// \return 兼容返回 true
    bool checkTypeCompatible(Type* expected, Type* actual, SourceRange range);

    /// \brief 检查表达式是否可作为赋值目标（左值）。
    /// \param target 赋值目标表达式
    /// \param loc 错误位置
    /// \return 可赋值返回 true
    bool checkAssignable(Expr* target, SourceLocation loc);

    /// \brief 检查表达式是否可变。
    /// \param target 目标表达式
    /// \param loc 错误位置
    /// \return 可变返回 true
    bool checkMutable(Expr* target, SourceLocation loc);

    /// \brief 计算两个类型的公共类型。
    /// \param t1 类型1
    /// \param t2 类型2
    /// \return 公共类型，无法确定返回 nullptr
    Type* getCommonType(Type* t1, Type* t2);

    /// \brief 求值编译期常量表达式。
    /// \param expr 待求值表达式
    /// \param result 输出结果
    /// \return 求值成功返回 true
    bool evaluateConstExpr(Expr* expr, int64_t& result);

private:
    /// \brief 解包类型别名到真实类型。
    /// \param type 输入类型
    /// \return 解包后的类型
    static Type* unwrapAliases(Type* type);

    SymbolTable& Symbols;
    DiagnosticEngine& Diag;
};

} // namespace yuan

#endif // YUAN_SEMA_TYPECHECKER_H
