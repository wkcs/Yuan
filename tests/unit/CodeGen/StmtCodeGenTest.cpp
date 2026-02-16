/// \file StmtCodeGenTest.cpp
/// \brief Unit tests for statement code generation.

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
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Verifier.h>

namespace yuan {

/// \brief Test fixture for statement code generation tests.
class StmtCodeGenTest : public ::testing::Test {
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
// Basic statement tests
// ============================================================================

TEST_F(StmtCodeGenTest, EmptyBlockStatement) {
    SourceRange range;
    std::vector<Stmt*> stmts;

    BlockStmt* block = new BlockStmt(range, stmts);

    // Create a test function to contain the block
    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range,
        "test_func",
        params,
        nullptr,  // void return
        block,
        false,
        false,
        Visibility::Public
    );

    bool result = CodeGenerator->generateDecl(funcDecl);
    EXPECT_TRUE(result);

    // Verify function exists
    llvm::Module* module = CodeGenerator->getModule();
    llvm::Function* func = module->getFunction("test_func");
    EXPECT_NE(func, nullptr);

    // Verify module
    std::string errorMsg;
    llvm::raw_string_ostream errorStream(errorMsg);
    bool hasErrors = llvm::verifyModule(*module, &errorStream);
    EXPECT_FALSE(hasErrors) << "Module verification failed: " << errorMsg;
}

TEST_F(StmtCodeGenTest, ReturnVoidStatement) {
    SourceRange range;

    // Create return statement without value
    ReturnStmt* retStmt = new ReturnStmt(range, nullptr);
    EXPECT_FALSE(retStmt->hasValue());

    std::vector<Stmt*> stmts;
    stmts.push_back(retStmt);
    BlockStmt* block = new BlockStmt(range, stmts);

    // Create test function
    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range,
        "test_return",
        params,
        nullptr,  // void return
        block,
        false,
        false,
        Visibility::Public
    );

    bool result = CodeGenerator->generateDecl(funcDecl);
    EXPECT_TRUE(result);

    // Verify function
    llvm::Module* module = CodeGenerator->getModule();
    llvm::Function* func = module->getFunction("test_return");
    EXPECT_NE(func, nullptr);

    if (func) {
        // Check that function has a return instruction
        bool hasReturn = false;
        for (auto& BB : *func) {
            if (llvm::isa<llvm::ReturnInst>(BB.getTerminator())) {
                hasReturn = true;
                break;
            }
        }
        EXPECT_TRUE(hasReturn);
    }

    // Verify module
    std::string errorMsg;
    llvm::raw_string_ostream errorStream(errorMsg);
    bool hasErrors = llvm::verifyModule(*module, &errorStream);
    EXPECT_FALSE(hasErrors) << "Module verification failed: " << errorMsg;
}

// ============================================================================
// Control flow statement tests
// ============================================================================

TEST_F(StmtCodeGenTest, IfStatementStructure) {
    // Test structure of if statement
    SourceRange range;

    // Create simple if: if true { }
    BoolLiteralExpr* cond = new BoolLiteralExpr(range, true);
    std::vector<Stmt*> stmts;
    BlockStmt* thenBody = new BlockStmt(range, stmts);

    std::vector<IfStmt::Branch> branches;
    branches.push_back({cond, thenBody});

    IfStmt* ifStmt = new IfStmt(range, branches);

    EXPECT_TRUE(ifStmt->getCondition() != nullptr);
    EXPECT_TRUE(ifStmt->getThenBody() != nullptr);
    EXPECT_FALSE(ifStmt->hasElse());
}

TEST_F(StmtCodeGenTest, WhileLoopStructure) {
    // Test structure of while loop
    SourceRange range;

    BoolLiteralExpr* cond = new BoolLiteralExpr(range, false);
    std::vector<Stmt*> stmts;
    BlockStmt* body = new BlockStmt(range, stmts);

    WhileStmt* whileStmt = new WhileStmt(range, cond, body);

    EXPECT_NE(whileStmt->getCondition(), nullptr);
    EXPECT_NE(whileStmt->getBody(), nullptr);
}

TEST_F(StmtCodeGenTest, LoopStatementStructure) {
    // Test structure of infinite loop
    SourceRange range;

    std::vector<Stmt*> stmts;
    BlockStmt* body = new BlockStmt(range, stmts);

    LoopStmt* loopStmt = new LoopStmt(range, body);

    EXPECT_NE(loopStmt->getBody(), nullptr);
}

// ============================================================================
// Jump statement tests
// ============================================================================

TEST_F(StmtCodeGenTest, BreakStatementStructure) {
    SourceRange range;

    // Break without label
    BreakStmt* breakStmt = new BreakStmt(range);
    EXPECT_FALSE(breakStmt->hasLabel());

    // Break with label
    BreakStmt* labeledBreak = new BreakStmt(range, "loop1");
    EXPECT_TRUE(labeledBreak->hasLabel());
    EXPECT_EQ(labeledBreak->getLabel(), "loop1");
}

TEST_F(StmtCodeGenTest, ContinueStatementStructure) {
    SourceRange range;

    // Continue without label
    ContinueStmt* contStmt = new ContinueStmt(range);
    EXPECT_FALSE(contStmt->hasLabel());

    // Continue with label
    ContinueStmt* labeledCont = new ContinueStmt(range, "loop1");
    EXPECT_TRUE(labeledCont->hasLabel());
    EXPECT_EQ(labeledCont->getLabel(), "loop1");
}

// ============================================================================
// Defer statement tests
// ============================================================================

TEST_F(StmtCodeGenTest, DeferStatementStructure) {
    SourceRange range;

    // Create defer statement
    std::vector<Stmt*> stmts;
    BlockStmt* deferBody = new BlockStmt(range, stmts);
    DeferStmt* deferStmt = new DeferStmt(range, deferBody);

    EXPECT_NE(deferStmt->getBody(), nullptr);
}

// ============================================================================
// Integration tests
// ============================================================================

TEST_F(StmtCodeGenTest, FunctionWithReturnValue) {
    SourceRange range;

    // Create: func test() -> i32 { return 42 }
    IntegerLiteralExpr* retValue = new IntegerLiteralExpr(range, 42, true, 32);
    ReturnStmt* retStmt = new ReturnStmt(range, retValue);

    std::vector<Stmt*> stmts;
    stmts.push_back(retStmt);
    BlockStmt* block = new BlockStmt(range, stmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range,
        "test_return_value",
        params,
        nullptr,  // TODO: should have i32 return type
        block,
        false,
        false,
        Visibility::Public
    );

    // Note: This test verifies structure, not full IR generation
    // Full IR generation requires type system integration
    EXPECT_EQ(funcDecl->getName(), "test_return_value");
    EXPECT_TRUE(funcDecl->hasBody());
}

TEST_F(StmtCodeGenTest, NestedBlocks) {
    SourceRange range;

    // Create nested blocks: { { } }
    std::vector<Stmt*> inner;
    BlockStmt* innerBlock = new BlockStmt(range, inner);

    std::vector<Stmt*> outer;
    outer.push_back(innerBlock);
    BlockStmt* outerBlock = new BlockStmt(range, outer);

    EXPECT_EQ(outerBlock->getStatementCount(), 1u);
    EXPECT_EQ(innerBlock->getStatementCount(), 0u);
}

// ============================================================================
// IR verification tests
// ============================================================================

TEST_F(StmtCodeGenTest, ModuleVerificationAfterStmtGen) {
    llvm::Module* module = CodeGenerator->getModule();

    // Empty module should verify
    std::string errorMsg;
    llvm::raw_string_ostream errorStream(errorMsg);
    bool hasErrors = llvm::verifyModule(*module, &errorStream);

    EXPECT_FALSE(hasErrors) << "Module verification failed: " << errorMsg;
}

} // namespace yuan
