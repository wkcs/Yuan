/// \file ErrorHandlingTest.cpp
/// \brief Unit tests for error handling code generation.

#include "yuan/CodeGen/CodeGen.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Stmt.h"
#include "yuan/Sema/Type.h"
#include "yuan/Basic/SourceManager.h"
#include <gtest/gtest.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Verifier.h>

namespace yuan {

/// \brief Test fixture for error handling code generation tests.
class ErrorHandlingTest : public ::testing::Test {
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
// Error type conversion tests
// ============================================================================

TEST_F(ErrorHandlingTest, ErrorTypeConversion) {
    // Create an error type: !i32
    Type* i32Type = Ctx->getIntegerType(32, true);
    ErrorType* errorType = ErrorType::get(*Ctx, i32Type);

    llvm::Type* llvmType = CodeGenerator->getLLVMType(errorType);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isStructTy());

    // Result type should be a struct
    llvm::StructType* structType = llvm::cast<llvm::StructType>(llvmType);
    EXPECT_EQ(structType->getNumElements(), 2u);

    // First element should be i8 (tag)
    EXPECT_TRUE(structType->getElementType(0)->isIntegerTy(8));
}

TEST_F(ErrorHandlingTest, ErrorTypeWithDifferentSuccessTypes) {
    // Test with different success types
    Type* boolType = Ctx->getBoolType();
    ErrorType* errorBool = ErrorType::get(*Ctx, boolType);

    llvm::Type* llvmBoolError = CodeGenerator->getLLVMType(errorBool);
    ASSERT_NE(llvmBoolError, nullptr);
    EXPECT_TRUE(llvmBoolError->isStructTy());

    // Test with f64
    Type* f64Type = Ctx->getFloatType(64);
    ErrorType* errorF64 = ErrorType::get(*Ctx, f64Type);

    llvm::Type* llvmF64Error = CodeGenerator->getLLVMType(errorF64);
    ASSERT_NE(llvmF64Error, nullptr);
    EXPECT_TRUE(llvmF64Error->isStructTy());
}

TEST_F(ErrorHandlingTest, ErrorTypeEquality) {
    Type* i32Type = Ctx->getIntegerType(32, true);
    ErrorType* error1 = ErrorType::get(*Ctx, i32Type);
    ErrorType* error2 = ErrorType::get(*Ctx, i32Type);

    // Same success type should give equal error types
    EXPECT_TRUE(error1->isEqual(error2));

    Type* i64Type = Ctx->getIntegerType(64, true);
    ErrorType* error3 = ErrorType::get(*Ctx, i64Type);

    // Different success types should not be equal
    EXPECT_FALSE(error1->isEqual(error3));
}

TEST_F(ErrorHandlingTest, ErrorTypeToString) {
    Type* i32Type = Ctx->getIntegerType(32, true);
    ErrorType* errorType = ErrorType::get(*Ctx, i32Type);

    std::string typeStr = errorType->toString();
    EXPECT_EQ(typeStr, "!i32");
}

TEST_F(ErrorHandlingTest, ErrorTypeGetSuccessType) {
    Type* f32Type = Ctx->getFloatType(32);
    ErrorType* errorType = ErrorType::get(*Ctx, f32Type);

    Type* successType = errorType->getSuccessType();
    ASSERT_NE(successType, nullptr);
    EXPECT_TRUE(successType->isFloat());
}

// ============================================================================
// Error propagation expression tests (structure only)
// ============================================================================

TEST_F(ErrorHandlingTest, ErrorPropagateExprStructure) {
    // Test that we can create ErrorPropagateExpr
    // Note: Full IR generation requires semantic analysis integration
    SourceRange range;

    // Create a dummy expression
    IntegerLiteralExpr* innerExpr = new IntegerLiteralExpr(range, 42, true, 32);

    ErrorPropagateExpr* propagateExpr = new ErrorPropagateExpr(range, innerExpr);

    EXPECT_NE(propagateExpr->getInner(), nullptr);
    EXPECT_EQ(propagateExpr->getInner(), innerExpr);
}

// ============================================================================
// Error handling expression tests (structure only)
// ============================================================================

TEST_F(ErrorHandlingTest, ErrorHandleExprStructure) {
    // Test that we can create ErrorHandleExpr
    SourceRange range;

    // Create inner expression and handler
    IntegerLiteralExpr* innerExpr = new IntegerLiteralExpr(range, 42, true, 32);

    std::vector<Stmt*> handlerStmts;
    BlockStmt* handler = new BlockStmt(range, handlerStmts);

    ErrorHandleExpr* handleExpr = new ErrorHandleExpr(
        range,
        innerExpr,
        "err",
        handler
    );

    EXPECT_NE(handleExpr->getInner(), nullptr);
    EXPECT_EQ(handleExpr->getErrorVar(), "err");
    EXPECT_NE(handleExpr->getHandler(), nullptr);
}

TEST_F(ErrorHandlingTest, ErrorHandleExprWithDifferentErrorVars) {
    SourceRange range;

    IntegerLiteralExpr* innerExpr = new IntegerLiteralExpr(range, 0, true, 32);
    std::vector<Stmt*> stmts;
    BlockStmt* handler = new BlockStmt(range, stmts);

    ErrorHandleExpr* expr1 = new ErrorHandleExpr(range, innerExpr, "error", handler);
    EXPECT_EQ(expr1->getErrorVar(), "error");

    IntegerLiteralExpr* innerExpr2 = new IntegerLiteralExpr(range, 1, true, 32);
    std::vector<Stmt*> stmts2;
    BlockStmt* handler2 = new BlockStmt(range, stmts2);

    ErrorHandleExpr* expr2 = new ErrorHandleExpr(range, innerExpr2, "e", handler2);
    EXPECT_EQ(expr2->getErrorVar(), "e");
}

// ============================================================================
// Integration tests
// ============================================================================

TEST_F(ErrorHandlingTest, ModuleVerificationWithErrorTypes) {
    // Create some error types and verify the module still validates
    Type* i32Type = Ctx->getIntegerType(32, true);
    ErrorType* errorType = ErrorType::get(*Ctx, i32Type);

    llvm::Type* llvmType = CodeGenerator->getLLVMType(errorType);
    ASSERT_NE(llvmType, nullptr);

    // Module should verify successfully
    llvm::Module* module = CodeGenerator->getModule();
    std::string errorMsg;
    llvm::raw_string_ostream errorStream(errorMsg);

    bool hasErrors = llvm::verifyModule(*module, &errorStream);
    EXPECT_FALSE(hasErrors) << "Module verification failed: " << errorMsg;
}

TEST_F(ErrorHandlingTest, ErrorTypeSize) {
    Type* i32Type = Ctx->getIntegerType(32, true);
    ErrorType* errorType = ErrorType::get(*Ctx, i32Type);

    // Error type should have a reasonable size
    size_t size = errorType->getSize();
    EXPECT_GT(size, 0u);

    // Should be at least tag (1 byte) + pointer/i32 (whichever is larger)
    EXPECT_GE(size, sizeof(int32_t) + 1);
}

TEST_F(ErrorHandlingTest, ErrorTypeAlignment) {
    Type* i32Type = Ctx->getIntegerType(32, true);
    ErrorType* errorType = ErrorType::get(*Ctx, i32Type);

    size_t alignment = errorType->getAlignment();
    EXPECT_GT(alignment, 0u);

    // Alignment should be at least the alignment of a pointer
    EXPECT_GE(alignment, alignof(void*));
}

} // namespace yuan
