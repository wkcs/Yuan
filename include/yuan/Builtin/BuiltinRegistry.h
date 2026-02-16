/// \file BuiltinRegistry.h
/// \brief 内置函数注册表定义。
///
/// 本文件定义了内置函数注册表，采用单例模式管理所有内置函数处理器。
/// 提供注册、查找和验证内置函数的功能。

#ifndef YUAN_BUILTIN_BUILTINREGISTRY_H
#define YUAN_BUILTIN_BUILTINREGISTRY_H

#include "yuan/Builtin/BuiltinHandler.h"
#include "yuan/AST/Expr.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace yuan {

/// \brief 内置函数注册表
///
/// 单例模式，管理所有内置函数处理器。
/// 在编译器初始化时自动注册所有内置函数。
///
/// 使用示例：
/// \code
/// auto& registry = BuiltinRegistry::instance();
/// if (registry.isBuiltin("sizeof")) {
///     auto* handler = registry.getHandler("sizeof");
///     Type* resultType = handler->analyze(expr, sema);
/// }
/// \endcode
class BuiltinRegistry {
public:
    /// \brief 获取单例实例
    /// \return 注册表单例引用
    static BuiltinRegistry& instance();
    
    /// \brief 注册内置函数处理器
    /// \param handler 处理器的唯一指针
    void registerHandler(std::unique_ptr<BuiltinHandler> handler);
    
    /// \brief 根据名称查找处理器
    /// \param name 内置函数名称（不含 @ 前缀）
    /// \return 处理器指针，未找到返回 nullptr
    BuiltinHandler* getHandler(const std::string& name) const;
    
    /// \brief 根据 BuiltinKind 查找处理器
    /// \param kind 内置函数类型
    /// \return 处理器指针，未找到返回 nullptr
    BuiltinHandler* getHandler(BuiltinKind kind) const;
    
    /// \brief 检查名称是否为有效的内置函数
    /// \param name 内置函数名称（不含 @ 前缀）
    /// \return 如果是有效的内置函数返回 true
    bool isBuiltin(const std::string& name) const;
    
    /// \brief 获取所有已注册的内置函数名称
    /// \return 内置函数名称列表
    std::vector<std::string> getAllBuiltinNames() const;
    
    /// \brief 获取已注册的内置函数数量
    /// \return 内置函数数量
    size_t getBuiltinCount() const { return NameToHandler.size(); }
    
private:
    /// \brief 私有构造函数（单例模式）
    BuiltinRegistry();
    
    /// \brief 私有析构函数
    ~BuiltinRegistry();
    
    /// \brief 注册所有内置函数（在构造函数中调用）
    void registerAllBuiltins();
    
    // 禁止拷贝和移动
    BuiltinRegistry(const BuiltinRegistry&) = delete;
    BuiltinRegistry& operator=(const BuiltinRegistry&) = delete;
    BuiltinRegistry(BuiltinRegistry&&) = delete;
    BuiltinRegistry& operator=(BuiltinRegistry&&) = delete;
    
    /// 名称到处理器的映射
    std::unordered_map<std::string, std::unique_ptr<BuiltinHandler>> NameToHandler;
    
    /// BuiltinKind 到处理器的映射（非拥有指针）
    std::unordered_map<BuiltinKind, BuiltinHandler*> KindToHandler;
};

} // namespace yuan

#endif // YUAN_BUILTIN_BUILTINREGISTRY_H
