/// \file ParseResult.h
/// \brief 解析结果包装器。
///
/// 本文件定义了 ParseResult 模板类，用于包装解析操作的结果，
/// 支持成功/失败状态和错误处理。

#ifndef YUAN_PARSER_PARSERESULT_H
#define YUAN_PARSER_PARSERESULT_H

#include <cassert>

namespace yuan {

/// \brief 解析结果包装器
///
/// ParseResult 用于包装解析操作的结果。它可以表示成功（包含一个值）
/// 或失败（不包含值）。这种设计允许解析器在遇到错误时优雅地处理，
/// 而不需要使用异常。
///
/// 使用示例：
/// \code
/// ParseResult<Expr> result = parseExpr();
/// if (result.isSuccess()) {
///     Expr* expr = result.get();
///     // 使用 expr
/// } else {
///     // 处理错误
/// }
/// \endcode
template<typename T>
class ParseResult {
public:
    /// \brief 默认构造函数，创建错误结果
    ParseResult() : Value(nullptr), HasError(true) {}
    
    /// \brief 从值构造成功结果
    /// \param value 解析得到的值
    ParseResult(T* value) : Value(value), HasError(value == nullptr) {}
    
    /// \brief 创建错误结果
    /// \return 表示错误的 ParseResult
    static ParseResult error() { return ParseResult(); }
    
    /// \brief 创建成功结果
    /// \param value 解析得到的值
    /// \return 表示成功的 ParseResult
    static ParseResult success(T* value) { return ParseResult(value); }
    
    /// \brief 检查是否为错误结果
    /// \return 如果是错误结果返回 true
    bool isError() const { return HasError; }
    
    /// \brief 检查是否为成功结果
    /// \return 如果是成功结果返回 true
    bool isSuccess() const { return !HasError; }
    
    /// \brief 获取解析得到的值
    /// \return 解析得到的值，如果是错误结果则返回 nullptr
    T* get() const { return Value; }
    
    /// \brief 箭头运算符，用于访问值的成员
    /// \return 解析得到的值
    T* operator->() const {
        assert(isSuccess() && "Cannot dereference error result");
        return Value;
    }
    
    /// \brief 解引用运算符
    /// \return 解析得到的值的引用
    T& operator*() const {
        assert(isSuccess() && "Cannot dereference error result");
        return *Value;
    }
    
    /// \brief 布尔转换运算符
    /// \return 如果是成功结果返回 true
    explicit operator bool() const { return isSuccess(); }
    
    /// \brief 获取值并释放所有权（用于转移）
    /// \return 解析得到的值
    T* release() {
        T* result = Value;
        Value = nullptr;
        HasError = true;
        return result;
    }
    
private:
    T* Value;       ///< 解析得到的值
    bool HasError;  ///< 是否为错误结果
};

} // namespace yuan

#endif // YUAN_PARSER_PARSERESULT_H
