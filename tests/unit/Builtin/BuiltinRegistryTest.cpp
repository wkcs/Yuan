/// \file BuiltinRegistryTest.cpp
/// \brief Builtin 模块单元测试。
///
/// 测试内置函数注册表的功能，包括：
/// - 注册表单例模式
/// - 内置函数注册和查找
/// - 内置函数名称验证

#include "yuan/Builtin/BuiltinRegistry.h"
#include "yuan/Builtin/BuiltinHandler.h"
#include "yuan/AST/Expr.h"
#include <gtest/gtest.h>
#include <algorithm>

namespace yuan {
namespace {

/// \brief 测试注册表单例模式
TEST(BuiltinRegistryTest, SingletonInstance) {
    auto& registry1 = BuiltinRegistry::instance();
    auto& registry2 = BuiltinRegistry::instance();
    
    // 两次调用应该返回同一个实例
    EXPECT_EQ(&registry1, &registry2);
}

/// \brief 测试所有内置函数都已注册
TEST(BuiltinRegistryTest, AllBuiltinsRegistered) {
    auto& registry = BuiltinRegistry::instance();
    
    // 检查所有预期的内置函数都已注册
    EXPECT_TRUE(registry.isBuiltin("import"));
    EXPECT_TRUE(registry.isBuiltin("sizeof"));
    EXPECT_TRUE(registry.isBuiltin("alignof"));
    EXPECT_TRUE(registry.isBuiltin("typeof"));
    EXPECT_TRUE(registry.isBuiltin("panic"));
    EXPECT_TRUE(registry.isBuiltin("assert"));
    EXPECT_TRUE(registry.isBuiltin("file"));
    EXPECT_TRUE(registry.isBuiltin("line"));
    EXPECT_TRUE(registry.isBuiltin("column"));
    EXPECT_TRUE(registry.isBuiltin("func"));
    EXPECT_TRUE(registry.isBuiltin("ffi_open"));
    EXPECT_TRUE(registry.isBuiltin("ffi_sym"));
    EXPECT_TRUE(registry.isBuiltin("ffi_call1"));
}

/// \brief 测试无效的内置函数名称
TEST(BuiltinRegistryTest, InvalidBuiltinNames) {
    auto& registry = BuiltinRegistry::instance();
    
    // 这些不是有效的内置函数
    EXPECT_FALSE(registry.isBuiltin(""));
    EXPECT_FALSE(registry.isBuiltin("foo"));
    EXPECT_FALSE(registry.isBuiltin("bar"));
    EXPECT_FALSE(registry.isBuiltin("unknown_builtin"));
    EXPECT_FALSE(registry.isBuiltin("@import"));  // 不应包含 @ 前缀
}

/// \brief 测试通过名称获取处理器
TEST(BuiltinRegistryTest, GetHandlerByName) {
    auto& registry = BuiltinRegistry::instance();
    
    // 获取有效的处理器
    auto* importHandler = registry.getHandler("import");
    ASSERT_NE(importHandler, nullptr);
    EXPECT_STREQ(importHandler->getName(), "import");
    EXPECT_EQ(importHandler->getKind(), BuiltinKind::Import);
    
    auto* sizeofHandler = registry.getHandler("sizeof");
    ASSERT_NE(sizeofHandler, nullptr);
    EXPECT_STREQ(sizeofHandler->getName(), "sizeof");
    EXPECT_EQ(sizeofHandler->getKind(), BuiltinKind::Sizeof);
    
    // 获取无效的处理器
    EXPECT_EQ(registry.getHandler("invalid"), nullptr);
    EXPECT_EQ(registry.getHandler(""), nullptr);
}

/// \brief 测试通过 BuiltinKind 获取处理器
TEST(BuiltinRegistryTest, GetHandlerByKind) {
    auto& registry = BuiltinRegistry::instance();
    
    // 测试所有 BuiltinKind
    auto* importHandler = registry.getHandler(BuiltinKind::Import);
    ASSERT_NE(importHandler, nullptr);
    EXPECT_EQ(importHandler->getKind(), BuiltinKind::Import);
    
    auto* sizeofHandler = registry.getHandler(BuiltinKind::Sizeof);
    ASSERT_NE(sizeofHandler, nullptr);
    EXPECT_EQ(sizeofHandler->getKind(), BuiltinKind::Sizeof);
    
    auto* alignofHandler = registry.getHandler(BuiltinKind::Alignof);
    ASSERT_NE(alignofHandler, nullptr);
    EXPECT_EQ(alignofHandler->getKind(), BuiltinKind::Alignof);
    
    auto* typeofHandler = registry.getHandler(BuiltinKind::Typeof);
    ASSERT_NE(typeofHandler, nullptr);
    EXPECT_EQ(typeofHandler->getKind(), BuiltinKind::Typeof);
    
    auto* panicHandler = registry.getHandler(BuiltinKind::Panic);
    ASSERT_NE(panicHandler, nullptr);
    EXPECT_EQ(panicHandler->getKind(), BuiltinKind::Panic);
    
    auto* assertHandler = registry.getHandler(BuiltinKind::Assert);
    ASSERT_NE(assertHandler, nullptr);
    EXPECT_EQ(assertHandler->getKind(), BuiltinKind::Assert);
    
    auto* fileHandler = registry.getHandler(BuiltinKind::File);
    ASSERT_NE(fileHandler, nullptr);
    EXPECT_EQ(fileHandler->getKind(), BuiltinKind::File);
    
    auto* lineHandler = registry.getHandler(BuiltinKind::Line);
    ASSERT_NE(lineHandler, nullptr);
    EXPECT_EQ(lineHandler->getKind(), BuiltinKind::Line);
    
    auto* columnHandler = registry.getHandler(BuiltinKind::Column);
    ASSERT_NE(columnHandler, nullptr);
    EXPECT_EQ(columnHandler->getKind(), BuiltinKind::Column);
    
    auto* funcHandler = registry.getHandler(BuiltinKind::Func);
    ASSERT_NE(funcHandler, nullptr);
    EXPECT_EQ(funcHandler->getKind(), BuiltinKind::Func);
}

/// \brief 测试获取所有内置函数名称
TEST(BuiltinRegistryTest, GetAllBuiltinNames) {
    auto& registry = BuiltinRegistry::instance();
    auto names = registry.getAllBuiltinNames();
    
    // 与注册表数量一致
    EXPECT_EQ(names.size(), registry.getBuiltinCount());
    
    // 检查所有预期的名称都在列表中
    auto contains = [&names](const std::string& name) {
        return std::find(names.begin(), names.end(), name) != names.end();
    };
    
    EXPECT_TRUE(contains("import"));
    EXPECT_TRUE(contains("sizeof"));
    EXPECT_TRUE(contains("alignof"));
    EXPECT_TRUE(contains("typeof"));
    EXPECT_TRUE(contains("panic"));
    EXPECT_TRUE(contains("assert"));
    EXPECT_TRUE(contains("file"));
    EXPECT_TRUE(contains("line"));
    EXPECT_TRUE(contains("column"));
    EXPECT_TRUE(contains("func"));
    EXPECT_TRUE(contains("print"));
    EXPECT_TRUE(contains("format"));
    EXPECT_TRUE(contains("alloc"));
    EXPECT_TRUE(contains("async_scheduler_create"));
    EXPECT_TRUE(contains("async_promise_create"));
    EXPECT_TRUE(contains("async_step_count"));
    EXPECT_TRUE(contains("os_time_unix_nanos"));
    EXPECT_TRUE(contains("os_thread_spawn"));
    EXPECT_TRUE(contains("os_read_file"));
    EXPECT_TRUE(contains("os_http_get_status"));
    EXPECT_TRUE(contains("ffi_open"));
    EXPECT_TRUE(contains("ffi_call0"));
    EXPECT_TRUE(contains("ffi_last_error"));
}

/// \brief 测试内置函数数量
TEST(BuiltinRegistryTest, BuiltinCount) {
    auto& registry = BuiltinRegistry::instance();
    
    // 内置函数数量应至少覆盖历史基线（核心 + 内存 + 异步），
    // 允许后续新增 OS/平台能力内置函数。
    EXPECT_GE(registry.getBuiltinCount(), 37u);
}

/// \brief 测试处理器的期望参数数量
TEST(BuiltinRegistryTest, ExpectedArgCount) {
    auto& registry = BuiltinRegistry::instance();
    
    // @import 需要 1 个参数
    auto* importHandler = registry.getHandler("import");
    ASSERT_NE(importHandler, nullptr);
    EXPECT_EQ(importHandler->getExpectedArgCount(), 1);
    
    // @sizeof 需要 1 个参数
    auto* sizeofHandler = registry.getHandler("sizeof");
    ASSERT_NE(sizeofHandler, nullptr);
    EXPECT_EQ(sizeofHandler->getExpectedArgCount(), 1);
    
    // @alignof 需要 1 个参数
    auto* alignofHandler = registry.getHandler("alignof");
    ASSERT_NE(alignofHandler, nullptr);
    EXPECT_EQ(alignofHandler->getExpectedArgCount(), 1);
    
    // @typeof 需要 1 个参数
    auto* typeofHandler = registry.getHandler("typeof");
    ASSERT_NE(typeofHandler, nullptr);
    EXPECT_EQ(typeofHandler->getExpectedArgCount(), 1);
    
    // @panic 需要 1 个参数
    auto* panicHandler = registry.getHandler("panic");
    ASSERT_NE(panicHandler, nullptr);
    EXPECT_EQ(panicHandler->getExpectedArgCount(), 1);
    
    // @assert 是可变参数（1 或 2 个）
    auto* assertHandler = registry.getHandler("assert");
    ASSERT_NE(assertHandler, nullptr);
    EXPECT_EQ(assertHandler->getExpectedArgCount(), -1);
    
    // 位置信息内置函数不需要参数
    auto* fileHandler = registry.getHandler("file");
    ASSERT_NE(fileHandler, nullptr);
    EXPECT_EQ(fileHandler->getExpectedArgCount(), 0);
    
    auto* lineHandler = registry.getHandler("line");
    ASSERT_NE(lineHandler, nullptr);
    EXPECT_EQ(lineHandler->getExpectedArgCount(), 0);
    
    auto* columnHandler = registry.getHandler("column");
    ASSERT_NE(columnHandler, nullptr);
    EXPECT_EQ(columnHandler->getExpectedArgCount(), 0);
    
    auto* funcHandler = registry.getHandler("func");
    ASSERT_NE(funcHandler, nullptr);
    EXPECT_EQ(funcHandler->getExpectedArgCount(), 0);
}

/// \brief 测试处理器的参数描述
TEST(BuiltinRegistryTest, ArgDescription) {
    auto& registry = BuiltinRegistry::instance();
    
    // 检查参数描述不为空
    auto* importHandler = registry.getHandler("import");
    ASSERT_NE(importHandler, nullptr);
    EXPECT_FALSE(importHandler->getArgDescription().empty());
    
    auto* sizeofHandler = registry.getHandler("sizeof");
    ASSERT_NE(sizeofHandler, nullptr);
    EXPECT_FALSE(sizeofHandler->getArgDescription().empty());
    
    auto* fileHandler = registry.getHandler("file");
    ASSERT_NE(fileHandler, nullptr);
    EXPECT_FALSE(fileHandler->getArgDescription().empty());
}

/// \brief 测试名称和 Kind 的一致性
TEST(BuiltinRegistryTest, NameKindConsistency) {
    auto& registry = BuiltinRegistry::instance();
    auto names = registry.getAllBuiltinNames();
    
    for (const auto& name : names) {
        auto* handlerByName = registry.getHandler(name);
        ASSERT_NE(handlerByName, nullptr);
        
        BuiltinKind kind = handlerByName->getKind();
        auto* handlerByKind = registry.getHandler(kind);
        ASSERT_NE(handlerByKind, nullptr);
        
        // 通过名称和 Kind 获取的应该是同一个处理器
        EXPECT_EQ(handlerByName, handlerByKind);
        
        // 处理器的名称应该与查找时使用的名称一致
        EXPECT_EQ(std::string(handlerByName->getName()), name);
    }
}

} // namespace
} // namespace yuan
