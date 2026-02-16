//===--- SymbolTableTest.cpp - 符号表单元测试 --------------------------===//
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

namespace yuan {

class SymbolTableTest : public ::testing::Test {
protected:
    void SetUp() override {
        symbolTable = std::make_unique<SymbolTable>(ctx);
    }

    SourceManager sm;
    ASTContext ctx{sm};
    std::unique_ptr<SymbolTable> symbolTable;
};

// 测试符号表基本功能
TEST_F(SymbolTableTest, BasicFunctionality) {
    // 检查初始状态
    EXPECT_NE(symbolTable->getCurrentScope(), nullptr);
    EXPECT_EQ(symbolTable->getCurrentScope()->getKind(), Scope::Kind::Global);
    EXPECT_EQ(symbolTable->getScopeDepth(), 1);
}

// 测试作用域进入和退出
TEST_F(SymbolTableTest, ScopeEnterExit) {
    // 进入函数作用域
    symbolTable->enterScope(Scope::Kind::Function);
    EXPECT_EQ(symbolTable->getCurrentScope()->getKind(), Scope::Kind::Function);
    EXPECT_EQ(symbolTable->getScopeDepth(), 2);

    // 进入块作用域
    symbolTable->enterScope(Scope::Kind::Block);
    EXPECT_EQ(symbolTable->getCurrentScope()->getKind(), Scope::Kind::Block);
    EXPECT_EQ(symbolTable->getScopeDepth(), 3);

    // 退出块作用域
    symbolTable->exitScope();
    EXPECT_EQ(symbolTable->getCurrentScope()->getKind(), Scope::Kind::Function);
    EXPECT_EQ(symbolTable->getScopeDepth(), 2);

    // 退出函数作用域
    symbolTable->exitScope();
    EXPECT_EQ(symbolTable->getCurrentScope()->getKind(), Scope::Kind::Global);
    EXPECT_EQ(symbolTable->getScopeDepth(), 1);
}

// 测试符号添加和查找
TEST_F(SymbolTableTest, SymbolAddAndLookup) {
    // 创建一个测试符号（使用 nullptr 作为类型，因为类型系统还未完全实现）
    SourceLocation loc;
    auto symbol = std::make_unique<Symbol>(
        SymbolKind::Variable, "test_var", nullptr, loc, Visibility::Private);
    Symbol* symPtr = symbol.get();

    // 添加符号到全局作用域
    EXPECT_TRUE(symbolTable->addSymbol(symPtr));

    // 查找符号
    Symbol* found = symbolTable->lookup("test_var");
    EXPECT_EQ(found, symPtr);
    EXPECT_EQ(found->getName(), "test_var");
    EXPECT_EQ(found->getKind(), SymbolKind::Variable);

    // 查找不存在的符号
    EXPECT_EQ(symbolTable->lookup("nonexistent"), nullptr);
}

// 测试符号重定义检测
TEST_F(SymbolTableTest, SymbolRedefinition) {
    SourceLocation loc;
    auto symbol1 = std::make_unique<Symbol>(
        SymbolKind::Variable, "test_var", nullptr, loc, Visibility::Private);
    auto symbol2 = std::make_unique<Symbol>(
        SymbolKind::Variable, "test_var", nullptr, loc, Visibility::Private);

    // 第一次添加应该成功
    EXPECT_TRUE(symbolTable->addSymbol(symbol1.get()));

    // 第二次添加同名符号应该失败
    EXPECT_FALSE(symbolTable->addSymbol(symbol2.get()));
}

// 测试作用域层次查找
TEST_F(SymbolTableTest, ScopeHierarchyLookup) {
    SourceLocation loc;
    
    // 在全局作用域添加符号
    auto globalSymbol = std::make_unique<Symbol>(
        SymbolKind::Variable, "global_var", nullptr, loc, Visibility::Private);
    EXPECT_TRUE(symbolTable->addSymbol(globalSymbol.get()));

    // 进入函数作用域
    symbolTable->enterScope(Scope::Kind::Function);
    
    // 在函数作用域添加符号
    auto funcSymbol = std::make_unique<Symbol>(
        SymbolKind::Variable, "func_var", nullptr, loc, Visibility::Private);
    EXPECT_TRUE(symbolTable->addSymbol(funcSymbol.get()));

    // 在函数作用域中应该能找到两个符号
    EXPECT_EQ(symbolTable->lookup("global_var"), globalSymbol.get());
    EXPECT_EQ(symbolTable->lookup("func_var"), funcSymbol.get());

    // 进入块作用域
    symbolTable->enterScope(Scope::Kind::Block);
    
    // 在块作用域中应该能找到所有符号
    EXPECT_EQ(symbolTable->lookup("global_var"), globalSymbol.get());
    EXPECT_EQ(symbolTable->lookup("func_var"), funcSymbol.get());

    // 在块作用域添加同名符号（遮蔽全局符号）
    auto blockSymbol = std::make_unique<Symbol>(
        SymbolKind::Variable, "global_var", nullptr, loc, Visibility::Private);
    EXPECT_TRUE(symbolTable->addSymbol(blockSymbol.get()));

    // 现在查找应该返回块作用域的符号
    EXPECT_EQ(symbolTable->lookup("global_var"), blockSymbol.get());
    EXPECT_EQ(symbolTable->lookup("func_var"), funcSymbol.get());

    // 退出块作用域
    symbolTable->exitScope();
    
    // 现在应该又能看到全局符号
    EXPECT_EQ(symbolTable->lookup("global_var"), globalSymbol.get());
    EXPECT_EQ(symbolTable->lookup("func_var"), funcSymbol.get());
}

// 测试 Scope 类的功能
class ScopeTest : public ::testing::Test {
protected:
    void SetUp() override {
        globalScope = std::make_unique<Scope>(Scope::Kind::Global);
    }

    std::unique_ptr<Scope> globalScope;
};

// 测试作用域基本功能
TEST_F(ScopeTest, BasicFunctionality) {
    EXPECT_EQ(globalScope->getKind(), Scope::Kind::Global);
    EXPECT_EQ(globalScope->getParent(), nullptr);
    EXPECT_EQ(globalScope->getSymbols().size(), 0);
}

// 测试符号添加和查找
TEST_F(ScopeTest, SymbolOperations) {
    SourceLocation loc;
    auto symbol = std::make_unique<Symbol>(
        SymbolKind::Function, "test_func", nullptr, loc, Visibility::Public);
    Symbol* symPtr = symbol.get();

    // 添加符号
    EXPECT_TRUE(globalScope->addSymbol(symPtr));
    EXPECT_EQ(globalScope->getSymbols().size(), 1);

    // 本地查找
    EXPECT_EQ(globalScope->lookupLocal("test_func"), symPtr);
    EXPECT_EQ(globalScope->lookupLocal("nonexistent"), nullptr);

    // 递归查找（对于全局作用域应该和本地查找相同）
    EXPECT_EQ(globalScope->lookup("test_func"), symPtr);
    EXPECT_EQ(globalScope->lookup("nonexistent"), nullptr);
}

// 测试循环检测
TEST_F(ScopeTest, LoopDetection) {
    // 全局作用域不在循环内
    EXPECT_FALSE(globalScope->isInLoop());

    // 创建函数作用域
    auto funcScope = std::make_unique<Scope>(Scope::Kind::Function, globalScope.get());
    EXPECT_FALSE(funcScope->isInLoop());

    // 创建循环作用域
    auto loopScope = std::make_unique<Scope>(Scope::Kind::Loop, funcScope.get());
    EXPECT_TRUE(loopScope->isInLoop());

    // 在循环内的块作用域
    auto blockScope = std::make_unique<Scope>(Scope::Kind::Block, loopScope.get());
    EXPECT_TRUE(blockScope->isInLoop());
}

// 测试函数检测
TEST_F(ScopeTest, FunctionDetection) {
    // 全局作用域不在函数内
    EXPECT_FALSE(globalScope->isInFunction());

    // 创建函数作用域
    auto funcScope = std::make_unique<Scope>(Scope::Kind::Function, globalScope.get());
    EXPECT_TRUE(funcScope->isInFunction());

    // 在函数内的块作用域
    auto blockScope = std::make_unique<Scope>(Scope::Kind::Block, funcScope.get());
    EXPECT_TRUE(blockScope->isInFunction());
}

// 测试 Symbol 类的功能
class SymbolTest : public ::testing::Test {};

// 测试符号基本功能
TEST_F(SymbolTest, BasicFunctionality) {
    SourceLocation loc;
    Symbol symbol(SymbolKind::Constant, "PI", nullptr, loc, Visibility::Public);

    EXPECT_EQ(symbol.getKind(), SymbolKind::Constant);
    EXPECT_EQ(symbol.getName(), "PI");
    EXPECT_EQ(symbol.getType(), nullptr);
    EXPECT_EQ(symbol.getLocation(), loc);
    EXPECT_EQ(symbol.getVisibility(), Visibility::Public);
    EXPECT_FALSE(symbol.isMutable());
}

// 测试符号可变性
TEST_F(SymbolTest, Mutability) {
    SourceLocation loc;
    Symbol symbol(SymbolKind::Variable, "counter", nullptr, loc);

    EXPECT_FALSE(symbol.isMutable());
    
    symbol.setMutable(true);
    EXPECT_TRUE(symbol.isMutable());
    
    symbol.setMutable(false);
    EXPECT_FALSE(symbol.isMutable());
}

// 测试符号类型检查
TEST_F(SymbolTest, TypeChecking) {
    SourceLocation loc;
    
    Symbol varSymbol(SymbolKind::Variable, "var", nullptr, loc);
    EXPECT_TRUE(varSymbol.isVariable());
    EXPECT_FALSE(varSymbol.isFunction());
    EXPECT_FALSE(varSymbol.isType());

    Symbol funcSymbol(SymbolKind::Function, "func", nullptr, loc);
    EXPECT_FALSE(funcSymbol.isVariable());
    EXPECT_TRUE(funcSymbol.isFunction());
    EXPECT_FALSE(funcSymbol.isType());

    Symbol structSymbol(SymbolKind::Struct, "MyStruct", nullptr, loc);
    EXPECT_FALSE(structSymbol.isVariable());
    EXPECT_FALSE(structSymbol.isFunction());
    EXPECT_TRUE(structSymbol.isType());
}

// 测试符号类型名称
TEST_F(SymbolTest, KindNames) {
    EXPECT_STREQ(Symbol::getKindName(SymbolKind::Variable), "variable");
    EXPECT_STREQ(Symbol::getKindName(SymbolKind::Function), "function");
    EXPECT_STREQ(Symbol::getKindName(SymbolKind::Struct), "struct");
    EXPECT_STREQ(Symbol::getKindName(SymbolKind::Enum), "enum");
}

} // namespace yuan
