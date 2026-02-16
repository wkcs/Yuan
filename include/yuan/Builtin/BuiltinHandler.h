/// \file BuiltinHandler.h
/// \brief 内置函数处理器接口定义。
///
/// 本文件定义了内置函数处理器的基类接口。每个内置函数（如 @import, @sizeof 等）
/// 都需要实现一个派生类来处理语义分析和代码生成。

#ifndef YUAN_BUILTIN_BUILTINHANDLER_H
#define YUAN_BUILTIN_BUILTINHANDLER_H

#include "yuan/AST/Expr.h"
#include <string>

// 前向声明 LLVM 类型
namespace llvm {
class Value;
} // namespace llvm

namespace yuan {

// 前向声明
class Sema;
class CodeGen;
class Type;
class BuiltinCallExpr;

/// \brief 内置函数处理器基类
///
/// 每个内置函数实现一个派生类，负责：
/// - 语义分析：检查参数类型和数量，返回结果类型
/// - 代码生成：生成对应的 LLVM IR
///
/// 使用示例：
/// \code
/// class SizeofBuiltin : public BuiltinHandler {
/// public:
///     llvm::StringRef getName() const override { return "sizeof"; }
///     BuiltinKind getKind() const override { return BuiltinKind::Sizeof; }
///     // ... 实现其他方法
/// };
/// \endcode
class BuiltinHandler {
public:
    virtual ~BuiltinHandler() = default;
    
    /// \brief 获取内置函数名称（不含 @ 前缀）
    /// \return 内置函数名称
    virtual const char* getName() const = 0;
    
    /// \brief 获取内置函数类型
    /// \return 内置函数类型枚举值
    virtual BuiltinKind getKind() const = 0;
    
    /// \brief 语义分析：检查参数并返回结果类型
    ///
    /// 在语义分析阶段调用，负责：
    /// - 检查参数数量是否正确
    /// - 检查参数类型是否正确
    /// - 返回表达式的结果类型
    ///
    /// \param expr 内置函数调用表达式
    /// \param sema 语义分析器引用（用于类型解析和错误报告）
    /// \return 返回类型，如果出错返回 nullptr
    virtual Type* analyze(BuiltinCallExpr* expr, Sema& sema) = 0;
    
    /// \brief 代码生成：生成 LLVM IR
    ///
    /// 在代码生成阶段调用，负责生成对应的 LLVM IR。
    ///
    /// \param expr 内置函数调用表达式
    /// \param codegen 代码生成器引用
    /// \return 生成的 LLVM Value，某些内置函数可能返回 nullptr
    virtual llvm::Value* generate(BuiltinCallExpr* expr, CodeGen& codegen) = 0;
    
    /// \brief 获取期望的参数数量
    /// \return 期望的参数数量，-1 表示可变参数
    virtual int getExpectedArgCount() const = 0;
    
    /// \brief 获取参数描述（用于错误消息）
    /// \return 参数描述字符串
    virtual std::string getArgDescription() const = 0;
    
protected:
    /// \brief 受保护的默认构造函数
    BuiltinHandler() = default;
    
    // 禁止拷贝和移动
    BuiltinHandler(const BuiltinHandler&) = delete;
    BuiltinHandler& operator=(const BuiltinHandler&) = delete;
    BuiltinHandler(BuiltinHandler&&) = delete;
    BuiltinHandler& operator=(BuiltinHandler&&) = delete;
};

} // namespace yuan

#endif // YUAN_BUILTIN_BUILTINHANDLER_H
