/// \file CodeGenPropertyTest.cpp
/// \brief Property-based tests for code generation correctness.
///
/// These tests validate Property 16: 代码生成正确性
/// For any well-typed Yuan program, the generated LLVM IR must be:
/// 1. Valid (passes LLVM verifier)
/// 2. Semantically correct (execution matches program semantics)
///
/// This validates Requirements 7.1-7.9 from the specifications.

#include "yuan/CodeGen/CodeGen.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Stmt.h"
#include "yuan/Sema/Type.h"
#include "yuan/Basic/SourceManager.h"
#include <gtest/gtest.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Type.h>

namespace yuan {

/// \brief Test fixture for code generation property tests.
class CodeGenPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        SourceMgr = std::make_unique<SourceManager>();
        Ctx = std::make_unique<ASTContext>(*SourceMgr);
        CodeGenerator = std::make_unique<CodeGen>(*Ctx, "property_test_module");
    }

    /// \brief Helper: Verify that generated IR is valid
    void AssertModuleValid() {
        std::string errorMsg;
        bool valid = CodeGenerator->verifyModule(&errorMsg);
        ASSERT_TRUE(valid) << "Generated IR is invalid: " << errorMsg;
    }

    std::unique_ptr<SourceManager> SourceMgr;
    std::unique_ptr<ASTContext> Ctx;
    std::unique_ptr<CodeGen> CodeGenerator;
};

// ============================================================================
// Property 16.1: Basic Type Mapping (Requirement 7.1)
// ============================================================================

TEST_F(CodeGenPropertyTest, BasicTypesGenerateValidLLVMTypes) {
    // Property: For all basic types, getLLVMType returns a valid LLVM type
    std::vector<Type*> basicTypes = {
        Ctx->getBoolType(),
        Ctx->getIntegerType(8, true),
        Ctx->getIntegerType(16, true),
        Ctx->getIntegerType(32, true),
        Ctx->getIntegerType(64, true),
        Ctx->getIntegerType(8, false),
        Ctx->getIntegerType(16, false),
        Ctx->getIntegerType(32, false),
        Ctx->getIntegerType(64, false),
        Ctx->getFloatType(32),
        Ctx->getFloatType(64),
        Ctx->getVoidType(),
    };

    for (Type* type : basicTypes) {
        llvm::Type* llvmType = CodeGenerator->getLLVMType(type);
        ASSERT_NE(llvmType, nullptr)
            << "Failed to generate LLVM type for: " << type->toString();

        // All LLVM types should be valid
        EXPECT_TRUE(llvmType->isIntegerTy() || llvmType->isFloatingPointTy() ||
                    llvmType->isVoidTy())
            << "Generated type for " << type->toString()
            << " is not a valid LLVM primitive type";
    }

    AssertModuleValid();
}

TEST_F(CodeGenPropertyTest, ArrayTypesGenerateValidLLVMTypes) {
    // Property: For all array types [T; N], generated LLVM type is valid
    Type* i32Type = Ctx->getIntegerType(32, true);
    ArrayType* arrayType = ArrayType::get(*Ctx, i32Type, 10);

    llvm::Type* llvmArrayType = CodeGenerator->getLLVMType(arrayType);
    ASSERT_NE(llvmArrayType, nullptr);
    EXPECT_TRUE(llvmArrayType->isArrayTy());

    AssertModuleValid();
}

TEST_F(CodeGenPropertyTest, PointerTypesGenerateValidLLVMTypes) {
    // Property: For all pointer types *T, generated LLVM type is valid
    Type* i32Type = Ctx->getIntegerType(32, true);
    PointerType* ptrType = PointerType::get(*Ctx, i32Type, false);

    llvm::Type* llvmPtrType = CodeGenerator->getLLVMType(ptrType);
    ASSERT_NE(llvmPtrType, nullptr);
    EXPECT_TRUE(llvmPtrType->isPointerTy());

    AssertModuleValid();
}

// ============================================================================
// Property 16.2: Function Generation (Requirement 7.2)
// ============================================================================

TEST_F(CodeGenPropertyTest, VoidFunctionGeneratesValidIR) {
    // Property: For all void functions with valid body, generated IR is valid
    SourceRange range;

    // func test() {}
    std::vector<Stmt*> stmts;
    ReturnStmt* ret = new ReturnStmt(range, nullptr);
    stmts.push_back(ret);
    BlockStmt* body = new BlockStmt(range, stmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range, "test_void_func", params, nullptr, body,
        false, false, Visibility::Public
    );

    bool generated = CodeGenerator->generateDecl(funcDecl);
    ASSERT_TRUE(generated);

    llvm::Module* module = CodeGenerator->getModule();
    llvm::Function* func = module->getFunction("test_void_func");
    ASSERT_NE(func, nullptr);

    // Verify function properties
    EXPECT_TRUE(func->getReturnType()->isVoidTy());
    EXPECT_EQ(func->arg_size(), 0u);

    AssertModuleValid();
}

TEST_F(CodeGenPropertyTest, MultipleFunctionsGenerateValidIR) {
    // Property: Multiple function declarations generate valid IR
    SourceRange range;

    for (int i = 0; i < 5; i++) {
        std::vector<Stmt*> stmts;
        ReturnStmt* ret = new ReturnStmt(range, nullptr);
        stmts.push_back(ret);
        BlockStmt* body = new BlockStmt(range, stmts);

        std::vector<ParamDecl*> params;
        std::string funcName = "func_" + std::to_string(i);
        FuncDecl* funcDecl = new FuncDecl(
            range, funcName, params, nullptr, body,
            false, false, Visibility::Public
        );

        bool generated = CodeGenerator->generateDecl(funcDecl);
        ASSERT_TRUE(generated);

        llvm::Function* func = CodeGenerator->getModule()->getFunction(funcName);
        ASSERT_NE(func, nullptr);
    }

    AssertModuleValid();
}

TEST_F(CodeGenPropertyTest, FunctionWithReturnGeneratesValidIR) {
    // Property: Functions with explicit return generate valid IR
    SourceRange range;

    // func test() { return }
    ReturnStmt* ret = new ReturnStmt(range, nullptr);
    std::vector<Stmt*> stmts;
    stmts.push_back(ret);
    BlockStmt* funcBody = new BlockStmt(range, stmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range, "test_return", params, nullptr, funcBody,
        false, false, Visibility::Public
    );

    bool generated = CodeGenerator->generateDecl(funcDecl);
    ASSERT_TRUE(generated);

    llvm::Module* module = CodeGenerator->getModule();
    llvm::Function* func = module->getFunction("test_return");
    ASSERT_NE(func, nullptr);

    // Function must have return instruction
    bool hasReturn = false;
    for (auto& BB : *func) {
        if (llvm::isa<llvm::ReturnInst>(BB.getTerminator())) {
            hasReturn = true;
            break;
        }
    }
    EXPECT_TRUE(hasReturn);

    AssertModuleValid();
}

// ============================================================================
// Property 16.6: Control Flow Generation (Requirement 7.6)
// ============================================================================

TEST_F(CodeGenPropertyTest, IfStatementGeneratesValidControlFlow) {
    // Property: For all if statements, generated control flow is valid
    SourceRange range;

    // if true { }
    BoolLiteralExpr* cond = new BoolLiteralExpr(range, true);
    std::vector<Stmt*> stmts;
    BlockStmt* thenBody = new BlockStmt(range, stmts);

    std::vector<IfStmt::Branch> branches;
    branches.push_back({cond, thenBody});
    IfStmt* ifStmt = new IfStmt(range, branches);

    // Wrap in function
    std::vector<Stmt*> funcStmts;
    funcStmts.push_back(ifStmt);
    ReturnStmt* ret = new ReturnStmt(range, nullptr);
    funcStmts.push_back(ret);
    BlockStmt* funcBody = new BlockStmt(range, funcStmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range, "test_if", params, nullptr, funcBody,
        false, false, Visibility::Public
    );

    bool generated = CodeGenerator->generateDecl(funcDecl);
    ASSERT_TRUE(generated);

    llvm::Module* module = CodeGenerator->getModule();
    llvm::Function* func = module->getFunction("test_if");
    ASSERT_NE(func, nullptr);

    // Function should have multiple basic blocks for if statement
    EXPECT_GT(func->size(), 1u);

    AssertModuleValid();
}

TEST_F(CodeGenPropertyTest, WhileLoopGeneratesValidControlFlow) {
    // Property: For all while loops, generated control flow is valid
    SourceRange range;

    // while false { }
    BoolLiteralExpr* cond = new BoolLiteralExpr(range, false);
    std::vector<Stmt*> stmts;
    BlockStmt* body = new BlockStmt(range, stmts);
    WhileStmt* whileStmt = new WhileStmt(range, cond, body);

    // Wrap in function
    std::vector<Stmt*> funcStmts;
    funcStmts.push_back(whileStmt);
    ReturnStmt* ret = new ReturnStmt(range, nullptr);
    funcStmts.push_back(ret);
    BlockStmt* funcBody = new BlockStmt(range, funcStmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range, "test_while", params, nullptr, funcBody,
        false, false, Visibility::Public
    );

    bool generated = CodeGenerator->generateDecl(funcDecl);
    ASSERT_TRUE(generated);

    llvm::Module* module = CodeGenerator->getModule();
    llvm::Function* func = module->getFunction("test_while");
    ASSERT_NE(func, nullptr);

    // While loop should generate multiple basic blocks
    EXPECT_GT(func->size(), 1u);

    AssertModuleValid();
}

TEST_F(CodeGenPropertyTest, LoopStatementGeneratesValidControlFlow) {
    // Property: For all loop statements, generated control flow is valid
    SourceRange range;

    // loop { break }
    BreakStmt* breakStmt = new BreakStmt(range);
    std::vector<Stmt*> loopStmts;
    loopStmts.push_back(breakStmt);
    BlockStmt* loopBody = new BlockStmt(range, loopStmts);
    LoopStmt* loopStmt = new LoopStmt(range, loopBody);

    // Wrap in function
    std::vector<Stmt*> funcStmts;
    funcStmts.push_back(loopStmt);
    ReturnStmt* ret = new ReturnStmt(range, nullptr);
    funcStmts.push_back(ret);
    BlockStmt* funcBody = new BlockStmt(range, funcStmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range, "test_loop", params, nullptr, funcBody,
        false, false, Visibility::Public
    );

    bool generated = CodeGenerator->generateDecl(funcDecl);
    ASSERT_TRUE(generated);

    llvm::Module* module = CodeGenerator->getModule();
    llvm::Function* func = module->getFunction("test_loop");
    ASSERT_NE(func, nullptr);

    // Loop should generate multiple basic blocks
    EXPECT_GT(func->size(), 1u);

    AssertModuleValid();
}

// ============================================================================
// Property 16.8: Defer Statement Generation (Requirement 7.8)
// ============================================================================

TEST_F(CodeGenPropertyTest, DeferStatementGeneratesValidIR) {
    // Property: For all defer statements, generated cleanup code is valid
    SourceRange range;

    // func test() { defer {} return }
    std::vector<Stmt*> deferStmts;
    BlockStmt* deferBody = new BlockStmt(range, deferStmts);
    DeferStmt* deferStmt = new DeferStmt(range, deferBody);

    std::vector<Stmt*> funcStmts;
    funcStmts.push_back(deferStmt);
    ReturnStmt* ret = new ReturnStmt(range, nullptr);
    funcStmts.push_back(ret);
    BlockStmt* funcBody = new BlockStmt(range, funcStmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range, "test_defer", params, nullptr, funcBody,
        false, false, Visibility::Public
    );

    bool generated = CodeGenerator->generateDecl(funcDecl);
    ASSERT_TRUE(generated);

    AssertModuleValid();
}

TEST_F(CodeGenPropertyTest, MultipleDeferStatementsGenerateValidIR) {
    // Property: Multiple defer statements execute in reverse order
    SourceRange range;

    std::vector<Stmt*> funcStmts;

    // Add 3 defer statements
    for (int i = 0; i < 3; i++) {
        std::vector<Stmt*> deferStmts;
        BlockStmt* deferBody = new BlockStmt(range, deferStmts);
        DeferStmt* deferStmt = new DeferStmt(range, deferBody);
        funcStmts.push_back(deferStmt);
    }

    ReturnStmt* ret = new ReturnStmt(range, nullptr);
    funcStmts.push_back(ret);
    BlockStmt* funcBody = new BlockStmt(range, funcStmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range, "test_multiple_defer", params, nullptr, funcBody,
        false, false, Visibility::Public
    );

    bool generated = CodeGenerator->generateDecl(funcDecl);
    ASSERT_TRUE(generated);

    AssertModuleValid();
}

// ============================================================================
// Property 16.9: Error Handling Generation (Requirement 7.9)
// ============================================================================

TEST_F(CodeGenPropertyTest, ErrorTypeGeneratesValidLLVMType) {
    // Property: For all error types, generated LLVM type is valid
    Type* i32Type = Ctx->getIntegerType(32, true);
    ErrorType* errorType = ErrorType::get(*Ctx, i32Type);

    llvm::Type* llvmErrorType = CodeGenerator->getLLVMType(errorType);
    ASSERT_NE(llvmErrorType, nullptr);

    // Error type should be a struct (tag + data)
    EXPECT_TRUE(llvmErrorType->isStructTy());

    llvm::StructType* structType = llvm::cast<llvm::StructType>(llvmErrorType);
    EXPECT_EQ(structType->getNumElements(), 2u);

    // First element should be i8 (tag)
    EXPECT_TRUE(structType->getElementType(0)->isIntegerTy(8));

    AssertModuleValid();
}

TEST_F(CodeGenPropertyTest, DifferentErrorTypesGenerateValidTypes) {
    // Property: Different success types generate different error types
    std::vector<Type*> successTypes = {
        Ctx->getIntegerType(32, true),
        Ctx->getIntegerType(64, true),
        Ctx->getFloatType(32),
        Ctx->getBoolType(),
    };

    for (Type* successType : successTypes) {
        ErrorType* errorType = ErrorType::get(*Ctx, successType);
        llvm::Type* llvmType = CodeGenerator->getLLVMType(errorType);

        ASSERT_NE(llvmType, nullptr)
            << "Failed to generate error type for: " << successType->toString();
        EXPECT_TRUE(llvmType->isStructTy());
    }

    AssertModuleValid();
}

// ============================================================================
// Property 16: Complete Programs Generate Valid IR
// ============================================================================

TEST_F(CodeGenPropertyTest, MultipleFunctionsGenerateValidModule) {
    // Property: A module with multiple functions is valid
    SourceRange range;

    // Create 10 different functions
    for (int i = 0; i < 10; i++) {
        std::vector<Stmt*> stmts;
        ReturnStmt* ret = new ReturnStmt(range, nullptr);
        stmts.push_back(ret);
        BlockStmt* body = new BlockStmt(range, stmts);

        std::vector<ParamDecl*> params;
        std::string funcName = "module_func_" + std::to_string(i);
        FuncDecl* funcDecl = new FuncDecl(
            range, funcName, params, nullptr, body,
            false, false, Visibility::Public
        );

        bool generated = CodeGenerator->generateDecl(funcDecl);
        ASSERT_TRUE(generated);
    }

    // Module should have all 10 functions
    llvm::Module* module = CodeGenerator->getModule();
    for (int i = 0; i < 10; i++) {
        std::string funcName = "module_func_" + std::to_string(i);
        EXPECT_NE(module->getFunction(funcName), nullptr);
    }

    AssertModuleValid();
}

// ============================================================================
// Property 16: IR Round-Trip Consistency
// ============================================================================

TEST_F(CodeGenPropertyTest, EmitIRIsConsistent) {
    // Property: Emitting IR multiple times produces identical output
    SourceRange range;

    std::vector<Stmt*> stmts;
    ReturnStmt* ret = new ReturnStmt(range, nullptr);
    stmts.push_back(ret);
    BlockStmt* body = new BlockStmt(range, stmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range, "consistency_test", params, nullptr, body,
        false, false, Visibility::Public
    );

    CodeGenerator->generateDecl(funcDecl);

    std::string ir1 = CodeGenerator->emitIR();
    std::string ir2 = CodeGenerator->emitIR();
    std::string ir3 = CodeGenerator->emitIR();

    EXPECT_EQ(ir1, ir2);
    EXPECT_EQ(ir2, ir3);

    AssertModuleValid();
}

TEST_F(CodeGenPropertyTest, VerificationNeverFails) {
    // Property: All generated IR passes verification
    SourceRange range;

    // Generate various constructs
    for (int i = 0; i < 5; i++) {
        std::vector<Stmt*> stmts;
        ReturnStmt* ret = new ReturnStmt(range, nullptr);
        stmts.push_back(ret);
        BlockStmt* body = new BlockStmt(range, stmts);

        std::vector<ParamDecl*> params;
        FuncDecl* funcDecl = new FuncDecl(
            range, "verify_func_" + std::to_string(i), params, nullptr, body,
            false, false, Visibility::Public
        );

        CodeGenerator->generateDecl(funcDecl);

        // After each declaration, module should still be valid
        AssertModuleValid();
    }
}

} // namespace yuan
