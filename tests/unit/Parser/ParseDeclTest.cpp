/// \file ParseDeclTest.cpp
/// \brief 声明解析单元测试。
///
/// 本文件测试 Parser 类中与声明解析相关的方法，
/// 包括变量、常量、函数、结构体、枚举、Trait 和 Impl 的解析。

#include "yuan/Parser/Parser.h"
#include "yuan/Lexer/Lexer.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/Type.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Stmt.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/TextDiagnosticPrinter.h"
#include <gtest/gtest.h>
#include <sstream>

using namespace yuan;

// ============================================================================
// 测试辅助类
// ============================================================================

class ParseDeclTest : public ::testing::Test {
protected:
    void SetUp() override {
        SM = std::make_unique<SourceManager>();
        Diag = std::make_unique<DiagnosticEngine>(*SM);
        
        // 使用空的诊断消费者来抑制输出
        Diag->setConsumer(nullptr);
    }
    
    /// \brief 解析源代码并返回声明列表
    std::vector<Decl*> parse(const std::string& source) {
        auto fileID = SM->createBuffer(source, "test.yu");
        Ctx = std::make_unique<ASTContext>(*SM);
        Lex = std::make_unique<Lexer>(*SM, *Diag, fileID);
        Parser parser(*Lex, *Diag, *Ctx);
        return parser.parseCompilationUnit();
    }
    
    /// \brief 解析单个声明
    Decl* parseDecl(const std::string& source) {
        auto decls = parse(source);
        if (decls.empty()) return nullptr;
        return decls[0];
    }
    
    /// \brief 检查是否有解析错误
    bool hasErrors() const {
        return Diag->hasErrors();
    }
    
    std::unique_ptr<SourceManager> SM;
    std::unique_ptr<DiagnosticEngine> Diag;
    std::unique_ptr<ASTContext> Ctx;
    std::unique_ptr<Lexer> Lex;
};

// ============================================================================
// 变量声明测试
// ============================================================================

TEST_F(ParseDeclTest, VarDeclSimple) {
    auto* decl = parseDecl("var x: i32 = 10");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* varDecl = dynamic_cast<VarDecl*>(decl);
    ASSERT_NE(varDecl, nullptr);
    
    EXPECT_EQ(varDecl->getName(), "x");
    EXPECT_TRUE(varDecl->isMutable());  // var 声明的是可变变量
    EXPECT_NE(varDecl->getType(), nullptr);
    EXPECT_NE(varDecl->getInit(), nullptr);
}

TEST_F(ParseDeclTest, VarDeclMutable) {
    auto* decl = parseDecl("var y: i32 = 20");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* varDecl = dynamic_cast<VarDecl*>(decl);
    ASSERT_NE(varDecl, nullptr);
    
    EXPECT_EQ(varDecl->getName(), "y");
    EXPECT_TRUE(varDecl->isMutable());  // var 默认就是可变的
}

TEST_F(ParseDeclTest, VarDeclTypeInference) {
    auto* decl = parseDecl("var z = 42");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* varDecl = dynamic_cast<VarDecl*>(decl);
    ASSERT_NE(varDecl, nullptr);
    
    EXPECT_EQ(varDecl->getName(), "z");
    EXPECT_EQ(varDecl->getType(), nullptr);  // 类型推断
    EXPECT_NE(varDecl->getInit(), nullptr);
}

TEST_F(ParseDeclTest, VarDeclNoInit) {
    auto* decl = parseDecl("var a: i32");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* varDecl = dynamic_cast<VarDecl*>(decl);
    ASSERT_NE(varDecl, nullptr);
    
    EXPECT_EQ(varDecl->getName(), "a");
    EXPECT_NE(varDecl->getType(), nullptr);
    EXPECT_EQ(varDecl->getInit(), nullptr);
}

// ============================================================================
// 常量声明测试
// ============================================================================

TEST_F(ParseDeclTest, ConstDeclSimple) {
    auto* decl = parseDecl("const PI: f64 = 3.14");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* constDecl = dynamic_cast<ConstDecl*>(decl);
    ASSERT_NE(constDecl, nullptr);
    
    EXPECT_EQ(constDecl->getName(), "PI");
    EXPECT_NE(constDecl->getType(), nullptr);
    EXPECT_NE(constDecl->getInit(), nullptr);
}

TEST_F(ParseDeclTest, ConstDeclTypeInference) {
    auto* decl = parseDecl("const MAX = 100");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* constDecl = dynamic_cast<ConstDecl*>(decl);
    ASSERT_NE(constDecl, nullptr);
    
    EXPECT_EQ(constDecl->getName(), "MAX");
    EXPECT_EQ(constDecl->getType(), nullptr);  // 类型推断
    EXPECT_NE(constDecl->getInit(), nullptr);
}


// ============================================================================
// 函数声明测试
// ============================================================================

TEST_F(ParseDeclTest, FuncDeclSimple) {
    auto* decl = parseDecl("func add(a: i32, b: i32) -> i32 { }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* funcDecl = dynamic_cast<FuncDecl*>(decl);
    ASSERT_NE(funcDecl, nullptr);
    
    EXPECT_EQ(funcDecl->getName(), "add");
    EXPECT_EQ(funcDecl->getParams().size(), 2);
    EXPECT_NE(funcDecl->getReturnType(), nullptr);
    EXPECT_NE(funcDecl->getBody(), nullptr);
    EXPECT_FALSE(funcDecl->isAsync());
    EXPECT_FALSE(funcDecl->canError());
}

TEST_F(ParseDeclTest, FuncDeclNoParams) {
    auto* decl = parseDecl("func hello() { }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* funcDecl = dynamic_cast<FuncDecl*>(decl);
    ASSERT_NE(funcDecl, nullptr);
    
    EXPECT_EQ(funcDecl->getName(), "hello");
    EXPECT_TRUE(funcDecl->getParams().empty());
    EXPECT_EQ(funcDecl->getReturnType(), nullptr);  // void
}

TEST_F(ParseDeclTest, FuncDeclAsync) {
    auto* decl = parseDecl("async func fetch() { }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* funcDecl = dynamic_cast<FuncDecl*>(decl);
    ASSERT_NE(funcDecl, nullptr);
    
    EXPECT_TRUE(funcDecl->isAsync());
}

TEST_F(ParseDeclTest, FuncDeclCanError) {
    auto* decl = parseDecl("func divide(a: i32, b: i32) -> !i32 { }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* funcDecl = dynamic_cast<FuncDecl*>(decl);
    ASSERT_NE(funcDecl, nullptr);
    
    EXPECT_TRUE(funcDecl->canError());
    EXPECT_NE(funcDecl->getReturnType(), nullptr);
}

TEST_F(ParseDeclTest, FuncDeclGeneric) {
    auto* decl = parseDecl("func identity<T>(x: T) -> T { }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* funcDecl = dynamic_cast<FuncDecl*>(decl);
    ASSERT_NE(funcDecl, nullptr);
    
    EXPECT_TRUE(funcDecl->isGeneric());
    EXPECT_EQ(funcDecl->getGenericParams().size(), 1);
    EXPECT_EQ(funcDecl->getGenericParams()[0].Name, "T");
}

TEST_F(ParseDeclTest, FuncDeclGenericWithBounds) {
    auto* decl = parseDecl("func print<T: Display>(x: T) { }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* funcDecl = dynamic_cast<FuncDecl*>(decl);
    ASSERT_NE(funcDecl, nullptr);
    
    EXPECT_TRUE(funcDecl->isGeneric());
    EXPECT_EQ(funcDecl->getGenericParams().size(), 1);
    EXPECT_EQ(funcDecl->getGenericParams()[0].Bounds.size(), 1);
    EXPECT_EQ(funcDecl->getGenericParams()[0].Bounds[0], "Display");
}

TEST_F(ParseDeclTest, FuncDeclSelfParam) {
    auto* decl = parseDecl("func method(self: &Self) { }");
    ASSERT_NE(decl, nullptr);
    // 注意：这里 self: &Self 被解析为普通参数，不是 &self 语法糖
    
    auto* funcDecl = dynamic_cast<FuncDecl*>(decl);
    ASSERT_NE(funcDecl, nullptr);
    
    EXPECT_EQ(funcDecl->getParams().size(), 1);
}

TEST_F(ParseDeclTest, FuncDeclRefSelf) {
    auto* decl = parseDecl("func method(&self) { }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* funcDecl = dynamic_cast<FuncDecl*>(decl);
    ASSERT_NE(funcDecl, nullptr);
    
    EXPECT_EQ(funcDecl->getParams().size(), 1);
    EXPECT_TRUE(funcDecl->getParams()[0]->isSelf());
    EXPECT_EQ(funcDecl->getParams()[0]->getParamKind(), ParamDecl::ParamKind::RefSelf);
}

TEST_F(ParseDeclTest, FuncDeclMutRefSelf) {
    auto* decl = parseDecl("func method(&mut self) { }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* funcDecl = dynamic_cast<FuncDecl*>(decl);
    ASSERT_NE(funcDecl, nullptr);
    
    EXPECT_EQ(funcDecl->getParams().size(), 1);
    EXPECT_TRUE(funcDecl->getParams()[0]->isSelf());
    EXPECT_EQ(funcDecl->getParams()[0]->getParamKind(), ParamDecl::ParamKind::MutRefSelf);
}

// ============================================================================
// 结构体声明测试
// ============================================================================

TEST_F(ParseDeclTest, StructDeclSimple) {
    auto* decl = parseDecl("struct Point { x: f64, y: f64 }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* structDecl = dynamic_cast<StructDecl*>(decl);
    ASSERT_NE(structDecl, nullptr);
    
    EXPECT_EQ(structDecl->getName(), "Point");
    EXPECT_EQ(structDecl->getFields().size(), 2);
    EXPECT_EQ(structDecl->getFields()[0]->getName(), "x");
    EXPECT_EQ(structDecl->getFields()[1]->getName(), "y");
}

TEST_F(ParseDeclTest, StructDeclEmpty) {
    auto* decl = parseDecl("struct Empty { }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* structDecl = dynamic_cast<StructDecl*>(decl);
    ASSERT_NE(structDecl, nullptr);
    
    EXPECT_EQ(structDecl->getName(), "Empty");
    EXPECT_TRUE(structDecl->getFields().empty());
}

TEST_F(ParseDeclTest, StructDeclGeneric) {
    auto* decl = parseDecl("struct Pair<T, U> { first: T, second: U }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* structDecl = dynamic_cast<StructDecl*>(decl);
    ASSERT_NE(structDecl, nullptr);
    
    EXPECT_TRUE(structDecl->isGeneric());
    EXPECT_EQ(structDecl->getGenericParams().size(), 2);
    EXPECT_EQ(structDecl->getGenericParams()[0].Name, "T");
    EXPECT_EQ(structDecl->getGenericParams()[1].Name, "U");
}

TEST_F(ParseDeclTest, StructDeclWithVisibility) {
    auto* decl = parseDecl("struct Person { pub name: str, priv id: i32 }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* structDecl = dynamic_cast<StructDecl*>(decl);
    ASSERT_NE(structDecl, nullptr);
    
    EXPECT_EQ(structDecl->getFields().size(), 2);
    EXPECT_EQ(structDecl->getFields()[0]->getVisibility(), Visibility::Public);
    EXPECT_EQ(structDecl->getFields()[1]->getVisibility(), Visibility::Private);
}


// ============================================================================
// 枚举声明测试
// ============================================================================

TEST_F(ParseDeclTest, EnumDeclSimple) {
    auto* decl = parseDecl("enum Color { Red, Green, Blue }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* enumDecl = dynamic_cast<EnumDecl*>(decl);
    ASSERT_NE(enumDecl, nullptr);
    
    EXPECT_EQ(enumDecl->getName(), "Color");
    EXPECT_EQ(enumDecl->getVariants().size(), 3);
    EXPECT_EQ(enumDecl->getVariants()[0]->getName(), "Red");
    EXPECT_TRUE(enumDecl->getVariants()[0]->isUnit());
}

TEST_F(ParseDeclTest, EnumDeclTupleVariant) {
    auto* decl = parseDecl("enum Option<T> { None, Some(T) }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* enumDecl = dynamic_cast<EnumDecl*>(decl);
    ASSERT_NE(enumDecl, nullptr);
    
    EXPECT_EQ(enumDecl->getName(), "Option");
    EXPECT_TRUE(enumDecl->isGeneric());
    EXPECT_EQ(enumDecl->getVariants().size(), 2);
    
    // None 是单元变体
    EXPECT_TRUE(enumDecl->getVariants()[0]->isUnit());
    
    // Some(T) 是元组变体
    EXPECT_TRUE(enumDecl->getVariants()[1]->isTuple());
    EXPECT_EQ(enumDecl->getVariants()[1]->getTupleTypes().size(), 1);
}

TEST_F(ParseDeclTest, EnumDeclStructVariant) {
    auto* decl = parseDecl("enum Message { Quit, Move { x: i32, y: i32 } }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* enumDecl = dynamic_cast<EnumDecl*>(decl);
    ASSERT_NE(enumDecl, nullptr);
    
    EXPECT_EQ(enumDecl->getVariants().size(), 2);
    
    // Quit 是单元变体
    EXPECT_TRUE(enumDecl->getVariants()[0]->isUnit());
    
    // Move { x: i32, y: i32 } 是结构体变体
    EXPECT_TRUE(enumDecl->getVariants()[1]->isStruct());
    EXPECT_EQ(enumDecl->getVariants()[1]->getFields().size(), 2);
}

TEST_F(ParseDeclTest, EnumDeclGeneric) {
    auto* decl = parseDecl("enum Result<T, E> { Ok(T), Err(E) }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* enumDecl = dynamic_cast<EnumDecl*>(decl);
    ASSERT_NE(enumDecl, nullptr);
    
    EXPECT_TRUE(enumDecl->isGeneric());
    EXPECT_EQ(enumDecl->getGenericParams().size(), 2);
    EXPECT_EQ(enumDecl->getGenericParams()[0].Name, "T");
    EXPECT_EQ(enumDecl->getGenericParams()[1].Name, "E");
}

// ============================================================================
// Trait 声明测试
// ============================================================================

TEST_F(ParseDeclTest, TraitDeclSimple) {
    auto* decl = parseDecl("trait Display { func display(&self) -> str { } }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* traitDecl = dynamic_cast<TraitDecl*>(decl);
    ASSERT_NE(traitDecl, nullptr);
    
    EXPECT_EQ(traitDecl->getName(), "Display");
    EXPECT_EQ(traitDecl->getMethods().size(), 1);
    EXPECT_EQ(traitDecl->getMethods()[0]->getName(), "display");
}

TEST_F(ParseDeclTest, TraitDeclWithAssociatedType) {
    auto* decl = parseDecl("trait Iterator { type Item func next(&mut self) -> Item { } }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* traitDecl = dynamic_cast<TraitDecl*>(decl);
    ASSERT_NE(traitDecl, nullptr);
    
    EXPECT_EQ(traitDecl->getName(), "Iterator");
    EXPECT_EQ(traitDecl->getAssociatedTypes().size(), 1);
    EXPECT_EQ(traitDecl->getAssociatedTypes()[0]->getName(), "Item");
    EXPECT_EQ(traitDecl->getMethods().size(), 1);
}

TEST_F(ParseDeclTest, TraitDeclWithSuperTrait) {
    auto* decl = parseDecl("trait Debug: Display { func debug(&self) -> str { } }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* traitDecl = dynamic_cast<TraitDecl*>(decl);
    ASSERT_NE(traitDecl, nullptr);
    
    EXPECT_EQ(traitDecl->getSuperTraits().size(), 1);
    EXPECT_EQ(traitDecl->getSuperTraits()[0], "Display");
}

TEST_F(ParseDeclTest, TraitDeclGeneric) {
    auto* decl = parseDecl("trait From<T> { func from(value: T) -> Self { } }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* traitDecl = dynamic_cast<TraitDecl*>(decl);
    ASSERT_NE(traitDecl, nullptr);
    
    EXPECT_TRUE(traitDecl->isGeneric());
    EXPECT_EQ(traitDecl->getGenericParams().size(), 1);
    EXPECT_EQ(traitDecl->getGenericParams()[0].Name, "T");
}

// ============================================================================
// Impl 声明测试
// ============================================================================

TEST_F(ParseDeclTest, ImplDeclInherent) {
    auto* decl = parseDecl("impl Point { func new(x: f64, y: f64) -> Point { } }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* implDecl = dynamic_cast<ImplDecl*>(decl);
    ASSERT_NE(implDecl, nullptr);
    
    EXPECT_FALSE(implDecl->isTraitImpl());
    EXPECT_EQ(implDecl->getMethods().size(), 1);
    EXPECT_EQ(implDecl->getMethods()[0]->getName(), "new");
}

TEST_F(ParseDeclTest, ImplDeclTrait) {
    auto* decl = parseDecl("impl Display for Point { func display(&self) -> str { } }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* implDecl = dynamic_cast<ImplDecl*>(decl);
    ASSERT_NE(implDecl, nullptr);
    
    EXPECT_TRUE(implDecl->isTraitImpl());
    EXPECT_EQ(implDecl->getTraitName(), "Display");
    EXPECT_EQ(implDecl->getMethods().size(), 1);
}

TEST_F(ParseDeclTest, ImplDeclGeneric) {
    auto* decl = parseDecl("impl<T> Vec<T> { func len(&self) -> usize { } }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* implDecl = dynamic_cast<ImplDecl*>(decl);
    ASSERT_NE(implDecl, nullptr);
    
    EXPECT_TRUE(implDecl->isGeneric());
    EXPECT_EQ(implDecl->getGenericParams().size(), 1);
}

TEST_F(ParseDeclTest, ImplDeclWithAssociatedType) {
    auto* decl = parseDecl("impl Iterator for Range { type Item = i32 func next(&mut self) -> Item { } }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* implDecl = dynamic_cast<ImplDecl*>(decl);
    ASSERT_NE(implDecl, nullptr);
    
    EXPECT_TRUE(implDecl->isTraitImpl());
    EXPECT_EQ(implDecl->getAssociatedTypes().size(), 1);
    EXPECT_EQ(implDecl->getAssociatedTypes()[0]->getName(), "Item");
}

TEST_F(ParseDeclTest, ImplDeclTraitWithTypeArgs) {
    auto* decl = parseDecl("impl From<i32> for S { func from(value: i32) -> Self { } }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());

    auto* implDecl = dynamic_cast<ImplDecl*>(decl);
    ASSERT_NE(implDecl, nullptr);
    ASSERT_TRUE(implDecl->isTraitImpl());
    EXPECT_EQ(implDecl->getTraitName(), "From");
    ASSERT_NE(implDecl->getTraitRefType(), nullptr);
    EXPECT_TRUE(implDecl->hasTraitTypeArgs());
    EXPECT_EQ(implDecl->getTraitTypeArgs().size(), 1u);

    auto* traitRef = dynamic_cast<GenericTypeNode*>(implDecl->getTraitRefType());
    ASSERT_NE(traitRef, nullptr);
    EXPECT_EQ(traitRef->getBaseName(), "From");
    EXPECT_EQ(traitRef->getTypeArgCount(), 1u);
}

TEST_F(ParseDeclTest, ImplDeclGenericTraitWithTypeParam) {
    auto* decl = parseDecl("impl<T> From<T> for S { func from(value: T) -> Self { } }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());

    auto* implDecl = dynamic_cast<ImplDecl*>(decl);
    ASSERT_NE(implDecl, nullptr);
    EXPECT_TRUE(implDecl->isGeneric());
    EXPECT_EQ(implDecl->getGenericParams().size(), 1u);
    EXPECT_EQ(implDecl->getGenericParams()[0].Name, "T");
    EXPECT_TRUE(implDecl->isTraitImpl());
    EXPECT_EQ(implDecl->getTraitName(), "From");
    EXPECT_TRUE(implDecl->hasTraitTypeArgs());
    EXPECT_EQ(implDecl->getTraitTypeArgs().size(), 1u);
}

TEST_F(ParseDeclTest, ImplDeclWhereSynthesizesGenericParam) {
    auto* decl =
        parseDecl("impl Display for Option<T> where T: Display { func to_string(&self) -> str { } }");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());

    auto* implDecl = dynamic_cast<ImplDecl*>(decl);
    ASSERT_NE(implDecl, nullptr);
    ASSERT_TRUE(implDecl->isTraitImpl());
    ASSERT_TRUE(implDecl->isGeneric());
    ASSERT_EQ(implDecl->getGenericParams().size(), 1u);
    EXPECT_EQ(implDecl->getGenericParams()[0].Name, "T");
    ASSERT_EQ(implDecl->getGenericParams()[0].Bounds.size(), 1u);
    EXPECT_EQ(implDecl->getGenericParams()[0].Bounds[0], "Display");
}

// ============================================================================
// 类型别名测试
// ============================================================================

TEST_F(ParseDeclTest, TypeAliasSimple) {
    auto* decl = parseDecl("type StringList = Vec<str>");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* typeAlias = dynamic_cast<TypeAliasDecl*>(decl);
    ASSERT_NE(typeAlias, nullptr);
    
    EXPECT_EQ(typeAlias->getName(), "StringList");
    EXPECT_NE(typeAlias->getAliasedType(), nullptr);
    EXPECT_FALSE(typeAlias->isAssociatedType());
}

TEST_F(ParseDeclTest, TypeAliasGeneric) {
    auto* decl = parseDecl("type MyResult<T> = Result<T, Error>");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* typeAlias = dynamic_cast<TypeAliasDecl*>(decl);
    ASSERT_NE(typeAlias, nullptr);
    
    EXPECT_EQ(typeAlias->getName(), "MyResult");
    EXPECT_TRUE(typeAlias->isGeneric());
    EXPECT_EQ(typeAlias->getGenericParams().size(), 1);
}

TEST_F(ParseDeclTest, TypeAliasAssociated) {
    auto* decl = parseDecl("type Item");
    ASSERT_NE(decl, nullptr);
    ASSERT_FALSE(hasErrors());
    
    auto* typeAlias = dynamic_cast<TypeAliasDecl*>(decl);
    ASSERT_NE(typeAlias, nullptr);
    
    EXPECT_EQ(typeAlias->getName(), "Item");
    EXPECT_TRUE(typeAlias->isAssociatedType());
    EXPECT_EQ(typeAlias->getAliasedType(), nullptr);
}
