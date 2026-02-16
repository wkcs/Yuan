/// \file DeclCodeGenTest.cpp
/// \brief Unit tests for declaration code generation.

#include "yuan/CodeGen/CodeGen.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Stmt.h"
#include "yuan/Sema/Type.h"
#include "yuan/Basic/SourceManager.h"
#include <gtest/gtest.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Verifier.h>

namespace yuan {

/// \brief Test fixture for declaration code generation tests.
class DeclCodeGenTest : public ::testing::Test {
protected:
    void SetUp() override {
        SourceMgr = std::make_unique<SourceManager>();
        Ctx = std::make_unique<ASTContext>(*SourceMgr);
        CodeGenerator = std::make_unique<CodeGen>(*Ctx, "test_module");
    }

    std::unique_ptr<SourceManager> SourceMgr;
    std::unique_ptr<ASTContext> Ctx;
    std::unique_ptr<CodeGen> CodeGenerator;
};

// ============================================================================
// Module and IR generation tests
// ============================================================================

TEST_F(DeclCodeGenTest, ModuleCreation) {
    llvm::Module* module = CodeGenerator->getModule();

    ASSERT_NE(module, nullptr);
    EXPECT_EQ(module->getName(), "test_module");
}

TEST_F(DeclCodeGenTest, EmptyModuleIR) {
    std::string ir = CodeGenerator->emitIR();

    EXPECT_FALSE(ir.empty());
    EXPECT_NE(ir.find("test_module"), std::string::npos);
}

// ============================================================================
// Function declaration tests
// ============================================================================

TEST_F(DeclCodeGenTest, EmptyFunctionDeclaration) {
    // Create a simple function: func test() {}
    SourceRange range;
    std::vector<ParamDecl*> params;
    BlockStmt* body = nullptr; // No body for declaration

    FuncDecl* funcDecl = new FuncDecl(
        range,
        "test_func",
        params,
        nullptr,  // void return type
        body,
        false,    // not async
        false,    // doesn't error
        Visibility::Public
    );

    // Function declarations without a body should succeed
    // (they're like forward declarations)
    bool result = CodeGenerator->generateDecl(funcDecl);

    // Should succeed since we're creating a declaration
    EXPECT_TRUE(result);

    // Verify the function exists in the module
    llvm::Module* module = CodeGenerator->getModule();
    llvm::Function* func = module->getFunction("test_func");
    EXPECT_NE(func, nullptr);
    if (func) {
        EXPECT_EQ(func->getName(), "test_func");
        EXPECT_TRUE(func->getReturnType()->isVoidTy());
        EXPECT_EQ(func->arg_size(), 0u);
    }
}

TEST_F(DeclCodeGenTest, FunctionWithParametersStructure) {
    // Test that we can create the AST structure for a function with parameters
    // func add(x: i32, y: i32) -> i32
    SourceRange range;

    // Create parameter declarations
    std::vector<ParamDecl*> params;

    // Note: Creating TypeNode is complex, so we'll skip actual generation
    // This test just verifies the structure

    FuncDecl* funcDecl = new FuncDecl(
        range,
        "add",
        params,
        nullptr,  // return type
        nullptr,  // no body
        false,    // not async
        false,    // doesn't error
        Visibility::Public
    );

    EXPECT_EQ(funcDecl->getName(), "add");
    EXPECT_EQ(funcDecl->getParams().size(), 0u);
    EXPECT_FALSE(funcDecl->hasBody());
}

// ============================================================================
// Variable declaration tests
// ============================================================================

TEST_F(DeclCodeGenTest, GlobalVariableStructure) {
    // Test structure for: var x: i32 = 0
    SourceRange range;

    VarDecl* varDecl = new VarDecl(
        range,
        "global_var",
        nullptr,  // type (would need TypeNode)
        nullptr,  // initializer
        false     // not mutable
    );

    EXPECT_EQ(varDecl->getName(), "global_var");
    EXPECT_FALSE(varDecl->isMutable());
}

TEST_F(DeclCodeGenTest, ConstDeclarationStructure) {
    // Test structure for: const PI: f64 = 3.14
    SourceRange range;

    ConstDecl* constDecl = new ConstDecl(
        range,
        "PI",
        nullptr,  // type
        nullptr   // initializer (required but nullptr for test)
    );

    EXPECT_EQ(constDecl->getName(), "PI");
}

// ============================================================================
// Struct declaration tests
// ============================================================================

TEST_F(DeclCodeGenTest, StructDeclarationStructure) {
    // Test structure for: struct Point { x: f64, y: f64 }
    SourceRange range;

    std::vector<FieldDecl*> fields;

    StructDecl* structDecl = new StructDecl(
        range,
        "Point",
        fields,
        Visibility::Public
    );

    EXPECT_EQ(structDecl->getName(), "Point");
    EXPECT_EQ(structDecl->getFields().size(), 0u);
    EXPECT_FALSE(structDecl->isGeneric());

    // Generation should succeed (no-op for structs)
    bool result = CodeGenerator->generateDecl(structDecl);
    EXPECT_TRUE(result);
}

// ============================================================================
// Enum declaration tests
// ============================================================================

TEST_F(DeclCodeGenTest, EnumDeclarationStructure) {
    // Test structure for: enum Color { Red, Green, Blue }
    SourceRange range;

    std::vector<EnumVariantDecl*> variants;

    EnumDecl* enumDecl = new EnumDecl(
        range,
        "Color",
        variants,
        Visibility::Public
    );

    EXPECT_EQ(enumDecl->getName(), "Color");
    EXPECT_EQ(enumDecl->getVariants().size(), 0u);

    // Generation should succeed (no-op for enums)
    bool result = CodeGenerator->generateDecl(enumDecl);
    EXPECT_TRUE(result);
}

// ============================================================================
// IR verification tests
// ============================================================================

TEST_F(DeclCodeGenTest, ModuleVerification) {
    llvm::Module* module = CodeGenerator->getModule();

    // Empty module should verify successfully
    std::string errorMsg;
    llvm::raw_string_ostream errorStream(errorMsg);

    bool hasErrors = llvm::verifyModule(*module, &errorStream);

    EXPECT_FALSE(hasErrors) << "Module verification failed: " << errorMsg;
}

TEST_F(DeclCodeGenTest, IREmissionToString) {
    // Test that we can emit IR to a string
    std::string ir = CodeGenerator->emitIR();

    EXPECT_FALSE(ir.empty());

    // Should contain module metadata
    EXPECT_NE(ir.find("ModuleID"), std::string::npos);
}

// ============================================================================
// Edge cases
// ============================================================================

TEST_F(DeclCodeGenTest, NullDeclHandling) {
    // Test that null decl is handled gracefully
    bool result = CodeGenerator->generateDecl(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(DeclCodeGenTest, TraitDeclGeneration) {
    // Traits should be no-op at codegen time
    SourceRange range;

    std::vector<FuncDecl*> methods;
    std::vector<TypeAliasDecl*> associatedTypes;
    TraitDecl* traitDecl = new TraitDecl(
        range,
        "Display",
        methods,
        associatedTypes,
        Visibility::Public
    );

    bool result = CodeGenerator->generateDecl(traitDecl);
    EXPECT_TRUE(result);  // Should succeed as no-op
}

} // namespace yuan
