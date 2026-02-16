//===--- SymbolTablePropertyTest.cpp - 符号表属性测试 ------------------===//
//
// Yuan 编译器
//
//===----------------------------------------------------------------------===//

#include "yuan/Sema/Scope.h"
#include "yuan/Sema/Symbol.h"
#include "yuan/Sema/Type.h"
#include "yuan/Basic/SourceLocation.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/AST.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <string>
#include <set>

namespace yuan {

/// 属性测试基类
class SymbolTablePropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 设置随机数生成器
        gen.seed(42); // 使用固定种子以确保测试可重现
        symbolTable = std::make_unique<SymbolTable>(ctx);
    }

    /// 生成随机符号名称
    std::string generateRandomName() {
        static const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        std::uniform_int_distribution<> lengthDist(3, 10);
        std::uniform_int_distribution<> charDist(0, sizeof(chars) - 2);
        
        int length = lengthDist(gen);
        std::string name;
        name.reserve(length);
        
        for (int i = 0; i < length; ++i) {
            name += chars[charDist(gen)];
        }
        
        return name;
    }

    /// 生成随机符号类型
    SymbolKind generateRandomSymbolKind() {
        static const SymbolKind kinds[] = {
            SymbolKind::Variable,
            SymbolKind::Constant,
            SymbolKind::Function,
            SymbolKind::Parameter,
            SymbolKind::Struct,
            SymbolKind::Enum,
            SymbolKind::Trait,
            SymbolKind::TypeAlias
        };
        
        std::uniform_int_distribution<> dist(0, sizeof(kinds) / sizeof(kinds[0]) - 1);
        return kinds[dist(gen)];
    }

    /// 生成随机作用域类型
    Scope::Kind generateRandomScopeKind() {
        static const Scope::Kind kinds[] = {
            Scope::Kind::Function,
            Scope::Kind::Block,
            Scope::Kind::Struct,
            Scope::Kind::Enum,
            Scope::Kind::Trait,
            Scope::Kind::Impl,
            Scope::Kind::Loop
        };
        
        std::uniform_int_distribution<> dist(0, sizeof(kinds) / sizeof(kinds[0]) - 1);
        return kinds[dist(gen)];
    }

    /// 创建测试符号
    std::unique_ptr<Symbol> createTestSymbol(const std::string& name, SymbolKind kind) {
        SourceLocation loc;
        return std::make_unique<Symbol>(kind, name, nullptr, loc, Visibility::Private);
    }

    std::mt19937 gen;
    SourceManager sm;
    ASTContext ctx{sm};
    std::unique_ptr<SymbolTable> symbolTable;
};

/// Property 12: 符号表作用域
/// 验证符号表的作用域管理正确性
class SymbolTableScopePropertyTest : public SymbolTablePropertyTest {};

/// 属性：符号查找的单调性
/// 如果在某个作用域能找到符号，那么在其子作用域中也应该能找到（除非被遮蔽）
TEST_F(SymbolTableScopePropertyTest, SymbolLookupMonotonicity) {
    const int numIterations = 100;
    
    for (int iter = 0; iter < numIterations; ++iter) {
        // 重置符号表
        symbolTable = std::make_unique<SymbolTable>(ctx);
        
        // 在全局作用域添加符号
        std::string globalSymName = generateRandomName();
        auto globalSym = createTestSymbol(globalSymName, generateRandomSymbolKind());
        Symbol* globalSymPtr = globalSym.get();
        
        ASSERT_TRUE(symbolTable->addSymbol(globalSymPtr));
        
        // 进入多层作用域
        std::uniform_int_distribution<> depthDist(1, 5);
        int depth = depthDist(gen);
        
        for (int i = 0; i < depth; ++i) {
            symbolTable->enterScope(generateRandomScopeKind());
            
            // 在每一层都应该能找到全局符号
            Symbol* found = symbolTable->lookup(globalSymName);
            EXPECT_EQ(found, globalSymPtr) 
                << "在作用域深度 " << (i + 1) << " 无法找到全局符号 " << globalSymName;
        }
        
        // 退出所有作用域
        for (int i = 0; i < depth; ++i) {
            symbolTable->exitScope();
        }
        
        // 回到全局作用域后仍应能找到符号
        Symbol* found = symbolTable->lookup(globalSymName);
        EXPECT_EQ(found, globalSymPtr);
    }
}

/// 属性：符号遮蔽的正确性
/// 内层作用域的同名符号应该遮蔽外层作用域的符号
TEST_F(SymbolTableScopePropertyTest, SymbolShadowing) {
    const int numIterations = 100;
    
    for (int iter = 0; iter < numIterations; ++iter) {
        // 重置符号表
        symbolTable = std::make_unique<SymbolTable>(ctx);
        
        std::string symName = generateRandomName();
        
        // 在全局作用域添加符号
        auto outerSym = createTestSymbol(symName, generateRandomSymbolKind());
        Symbol* outerSymPtr = outerSym.get();
        ASSERT_TRUE(symbolTable->addSymbol(outerSymPtr));
        
        // 进入内层作用域
        symbolTable->enterScope(generateRandomScopeKind());
        
        // 在内层作用域添加同名符号
        auto innerSym = createTestSymbol(symName, generateRandomSymbolKind());
        Symbol* innerSymPtr = innerSym.get();
        ASSERT_TRUE(symbolTable->addSymbol(innerSymPtr));
        
        // 查找应该返回内层符号
        Symbol* found = symbolTable->lookup(symName);
        EXPECT_EQ(found, innerSymPtr) 
            << "符号遮蔽失败：应该找到内层符号，但找到了外层符号";
        
        // 退出内层作用域
        symbolTable->exitScope();
        
        // 现在应该找到外层符号
        found = symbolTable->lookup(symName);
        EXPECT_EQ(found, outerSymPtr) 
            << "退出内层作用域后应该找到外层符号";
    }
}

/// 属性：作用域深度的一致性
/// 进入 n 个作用域后深度应该增加 n，退出 n 个作用域后深度应该减少 n
TEST_F(SymbolTableScopePropertyTest, ScopeDepthConsistency) {
    const int numIterations = 50;
    
    for (int iter = 0; iter < numIterations; ++iter) {
        // 重置符号表
        symbolTable = std::make_unique<SymbolTable>(ctx);
        
        size_t initialDepth = symbolTable->getScopeDepth();
        EXPECT_EQ(initialDepth, 1); // 全局作用域
        
        // 随机进入多个作用域
        std::uniform_int_distribution<> depthDist(1, 10);
        int enterCount = depthDist(gen);
        
        for (int i = 0; i < enterCount; ++i) {
            symbolTable->enterScope(generateRandomScopeKind());
            EXPECT_EQ(symbolTable->getScopeDepth(), initialDepth + i + 1);
        }
        
        // 退出所有作用域
        for (int i = 0; i < enterCount; ++i) {
            symbolTable->exitScope();
            EXPECT_EQ(symbolTable->getScopeDepth(), initialDepth + enterCount - i - 1);
        }
        
        // 最终应该回到初始深度
        EXPECT_EQ(symbolTable->getScopeDepth(), initialDepth);
    }
}

/// 属性：符号添加的幂等性
/// 在同一作用域中重复添加同名符号应该失败
TEST_F(SymbolTableScopePropertyTest, SymbolAdditionIdempotency) {
    const int numIterations = 100;
    
    for (int iter = 0; iter < numIterations; ++iter) {
        // 重置符号表
        symbolTable = std::make_unique<SymbolTable>(ctx);
        
        std::string symName = generateRandomName();
        
        // 第一次添加应该成功
        auto sym1 = createTestSymbol(symName, generateRandomSymbolKind());
        EXPECT_TRUE(symbolTable->addSymbol(sym1.get()));
        
        // 第二次添加同名符号应该失败
        auto sym2 = createTestSymbol(symName, generateRandomSymbolKind());
        EXPECT_FALSE(symbolTable->addSymbol(sym2.get()));
        
        // 查找应该返回第一个符号
        Symbol* found = symbolTable->lookup(symName);
        EXPECT_EQ(found, sym1.get());
    }
}

/// 属性：作用域隔离性
/// 不同作用域中的符号不应该相互干扰
TEST_F(SymbolTableScopePropertyTest, ScopeIsolation) {
    const int numIterations = 50;
    
    for (int iter = 0; iter < numIterations; ++iter) {
        // 重置符号表
        symbolTable = std::make_unique<SymbolTable>(ctx);
        
        // 生成多个唯一的符号名称
        std::set<std::string> usedNames;
        std::vector<std::string> symNames;
        
        for (int i = 0; i < 5; ++i) {
            std::string name;
            do {
                name = generateRandomName();
            } while (usedNames.count(name));
            
            usedNames.insert(name);
            symNames.push_back(name);
        }
        
        // 在全局作用域添加符号
        std::vector<std::unique_ptr<Symbol>> globalSyms;
        for (const auto& name : symNames) {
            auto sym = createTestSymbol(name, generateRandomSymbolKind());
            ASSERT_TRUE(symbolTable->addSymbol(sym.get()));
            globalSyms.push_back(std::move(sym));
        }
        
        // 进入新作用域
        symbolTable->enterScope(Scope::Kind::Function);
        
        // 在新作用域中添加不同的符号
        std::vector<std::unique_ptr<Symbol>> funcSyms;
        for (int i = 0; i < 3; ++i) {
            std::string name;
            do {
                name = generateRandomName();
            } while (usedNames.count(name));
            
            usedNames.insert(name);
            auto sym = createTestSymbol(name, generateRandomSymbolKind());
            ASSERT_TRUE(symbolTable->addSymbol(sym.get()));
            funcSyms.push_back(std::move(sym));
        }
        
        // 验证可以找到所有符号
        for (const auto& sym : globalSyms) {
            EXPECT_EQ(symbolTable->lookup(sym->getName()), sym.get());
        }
        
        for (const auto& sym : funcSyms) {
            EXPECT_EQ(symbolTable->lookup(sym->getName()), sym.get());
        }
        
        // 退出函数作用域
        symbolTable->exitScope();
        
        // 现在应该只能找到全局符号
        for (const auto& sym : globalSyms) {
            EXPECT_EQ(symbolTable->lookup(sym->getName()), sym.get());
        }
        
        // 函数作用域的符号应该找不到
        for (const auto& sym : funcSyms) {
            EXPECT_EQ(symbolTable->lookup(sym->getName()), nullptr);
        }
    }
}

/// 属性：循环检测的正确性
/// 循环作用域的检测应该正确工作
TEST_F(SymbolTableScopePropertyTest, LoopDetection) {
    const int numIterations = 50;
    
    for (int iter = 0; iter < numIterations; ++iter) {
        // 重置符号表
        symbolTable = std::make_unique<SymbolTable>(ctx);
        
        // 全局作用域不在循环内
        EXPECT_FALSE(symbolTable->getCurrentScope()->isInLoop());
        
        // 进入非循环作用域
        std::uniform_int_distribution<> nonLoopDist(0, 5);
        int nonLoopKindIndex = nonLoopDist(gen);
        Scope::Kind nonLoopKinds[] = {
            Scope::Kind::Function,
            Scope::Kind::Block,
            Scope::Kind::Struct,
            Scope::Kind::Enum,
            Scope::Kind::Trait,
            Scope::Kind::Impl
        };
        
        symbolTable->enterScope(nonLoopKinds[nonLoopKindIndex]);
        EXPECT_FALSE(symbolTable->getCurrentScope()->isInLoop());
        
        // 进入循环作用域
        symbolTable->enterScope(Scope::Kind::Loop);
        EXPECT_TRUE(symbolTable->getCurrentScope()->isInLoop());
        
        // 在循环内进入块作用域
        symbolTable->enterScope(Scope::Kind::Block);
        EXPECT_TRUE(symbolTable->getCurrentScope()->isInLoop());
        
        // 退出块作用域，仍在循环内
        symbolTable->exitScope();
        EXPECT_TRUE(symbolTable->getCurrentScope()->isInLoop());
        
        // 退出循环作用域
        symbolTable->exitScope();
        EXPECT_FALSE(symbolTable->getCurrentScope()->isInLoop());
        
        // 退出非循环作用域
        symbolTable->exitScope();
        EXPECT_FALSE(symbolTable->getCurrentScope()->isInLoop());
    }
}

} // namespace yuan
