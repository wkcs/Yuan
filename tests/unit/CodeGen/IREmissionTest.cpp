/// \file IREmissionTest.cpp
/// \brief Unit tests for IR emission and verification.

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
#include <llvm/Support/raw_ostream.h>
#include <fstream>

namespace yuan {

/// \brief Test fixture for IR emission tests.
class IREmissionTest : public ::testing::Test {
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
// IR emission to string tests
// ============================================================================

TEST_F(IREmissionTest, EmitIRToString) {
    // Empty module should still produce valid IR
    std::string ir = CodeGenerator->emitIR();

    EXPECT_FALSE(ir.empty());
    EXPECT_NE(ir.find("test_module"), std::string::npos);
}

TEST_F(IREmissionTest, EmitIRContainsFunctionDeclaration) {
    // Create a simple function: func test() {}
    SourceRange range;
    std::vector<Stmt*> stmts;
    BlockStmt* body = new BlockStmt(range, stmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range,
        "test_func",
        params,
        nullptr,  // void return
        body,
        false,
        false,
        Visibility::Public
    );

    bool result = CodeGenerator->generateDecl(funcDecl);
    ASSERT_TRUE(result);

    std::string ir = CodeGenerator->emitIR();

    // IR should contain the function definition
    EXPECT_NE(ir.find("test_func"), std::string::npos);
    EXPECT_NE(ir.find("define"), std::string::npos);
}

// ============================================================================
// IR emission to file tests
// ============================================================================

TEST_F(IREmissionTest, EmitIRToFile) {
    const std::string filename = "/tmp/test_output.ll";

    bool result = CodeGenerator->emitIRToFile(filename);
    EXPECT_TRUE(result);

    // Check that file was created and is not empty
    std::ifstream file(filename);
    ASSERT_TRUE(file.is_open());

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    EXPECT_FALSE(content.empty());
    EXPECT_NE(content.find("test_module"), std::string::npos);

    // Clean up
    std::remove(filename.c_str());
}

TEST_F(IREmissionTest, EmitIRToFileWithFunction) {
    const std::string filename = "/tmp/test_func_output.ll";

    // Create a simple function
    SourceRange range;
    std::vector<Stmt*> stmts;
    ReturnStmt* retStmt = new ReturnStmt(range, nullptr);
    stmts.push_back(retStmt);
    BlockStmt* body = new BlockStmt(range, stmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range,
        "emit_test",
        params,
        nullptr,  // void return
        body,
        false,
        false,
        Visibility::Public
    );

    CodeGenerator->generateDecl(funcDecl);

    bool result = CodeGenerator->emitIRToFile(filename);
    ASSERT_TRUE(result);

    // Check file content
    std::ifstream file(filename);
    ASSERT_TRUE(file.is_open());

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    EXPECT_NE(content.find("emit_test"), std::string::npos);
    EXPECT_NE(content.find("ret void"), std::string::npos);

    // Clean up
    std::remove(filename.c_str());
}

TEST_F(IREmissionTest, EmitIRToInvalidPath) {
    // Try to emit to an invalid path
    const std::string filename = "/invalid/path/that/does/not/exist/output.ll";

    bool result = CodeGenerator->emitIRToFile(filename);
    EXPECT_FALSE(result);
}

// ============================================================================
// Module verification tests
// ============================================================================

TEST_F(IREmissionTest, VerifyEmptyModule) {
    // Empty module should verify successfully
    std::string errorMsg;
    bool verified = CodeGenerator->verifyModule(&errorMsg);

    EXPECT_TRUE(verified) << "Verification failed: " << errorMsg;
    EXPECT_TRUE(errorMsg.empty());
}

TEST_F(IREmissionTest, VerifyModuleWithValidFunction) {
    // Create a valid function
    SourceRange range;
    std::vector<Stmt*> stmts;
    ReturnStmt* retStmt = new ReturnStmt(range, nullptr);
    stmts.push_back(retStmt);
    BlockStmt* body = new BlockStmt(range, stmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range,
        "verify_test",
        params,
        nullptr,  // void return
        body,
        false,
        false,
        Visibility::Public
    );

    bool generated = CodeGenerator->generateDecl(funcDecl);
    ASSERT_TRUE(generated);

    std::string errorMsg;
    bool verified = CodeGenerator->verifyModule(&errorMsg);

    EXPECT_TRUE(verified) << "Verification failed: " << errorMsg;
    EXPECT_TRUE(errorMsg.empty());
}

TEST_F(IREmissionTest, VerifyModuleWithoutErrorMessage) {
    // Test verification without requesting error message
    bool verified = CodeGenerator->verifyModule();
    EXPECT_TRUE(verified);
}

TEST_F(IREmissionTest, VerifyModuleReturnsErrorMessage) {
    // Create a function and verify
    SourceRange range;
    std::vector<Stmt*> stmts;
    BlockStmt* body = new BlockStmt(range, stmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range,
        "test_error_msg",
        params,
        nullptr,
        body,
        false,
        false,
        Visibility::Public
    );

    CodeGenerator->generateDecl(funcDecl);

    std::string errorMsg;
    bool verified = CodeGenerator->verifyModule(&errorMsg);

    // Even if verification succeeds, errorMsg should be set (empty or not)
    // This test just ensures the API works correctly
    if (!verified) {
        EXPECT_FALSE(errorMsg.empty());
    }
}

// ============================================================================
// Integration tests
// ============================================================================

TEST_F(IREmissionTest, EmitAndVerifyCompleteWorkflow) {
    // Create a function: func workflow_test() {}
    SourceRange range;
    ReturnStmt* retStmt = new ReturnStmt(range, nullptr);

    std::vector<Stmt*> stmts;
    stmts.push_back(retStmt);
    BlockStmt* body = new BlockStmt(range, stmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range,
        "workflow_test",
        params,
        nullptr,  // void return
        body,
        false,
        false,
        Visibility::Public
    );

    // Generate
    bool generated = CodeGenerator->generateDecl(funcDecl);
    ASSERT_TRUE(generated);

    // Verify
    std::string errorMsg;
    bool verified = CodeGenerator->verifyModule(&errorMsg);
    EXPECT_TRUE(verified) << "Verification failed: " << errorMsg;

    // Emit to string
    std::string ir = CodeGenerator->emitIR();
    EXPECT_NE(ir.find("workflow_test"), std::string::npos);

    // Emit to file
    const std::string filename = "/tmp/workflow_test.ll";
    bool emitted = CodeGenerator->emitIRToFile(filename);
    EXPECT_TRUE(emitted);

    // Verify file content matches string
    std::ifstream file(filename);
    std::string fileContent((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    file.close();

    EXPECT_EQ(ir, fileContent);

    // Clean up
    std::remove(filename.c_str());
}

TEST_F(IREmissionTest, MultipleEmissions) {
    // Test that we can emit IR multiple times
    std::string ir1 = CodeGenerator->emitIR();
    std::string ir2 = CodeGenerator->emitIR();

    // Both emissions should produce identical output
    EXPECT_EQ(ir1, ir2);
}

TEST_F(IREmissionTest, EmitAfterModification) {
    // Emit IR
    std::string ir1 = CodeGenerator->emitIR();

    // Add a function
    SourceRange range;
    std::vector<Stmt*> stmts;
    BlockStmt* body = new BlockStmt(range, stmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range,
        "added_func",
        params,
        nullptr,
        body,
        false,
        false,
        Visibility::Public
    );

    CodeGenerator->generateDecl(funcDecl);

    // Emit IR again
    std::string ir2 = CodeGenerator->emitIR();

    // IR should be different now
    EXPECT_NE(ir1, ir2);
    EXPECT_EQ(ir1.find("added_func"), std::string::npos);
    EXPECT_NE(ir2.find("added_func"), std::string::npos);
}

} // namespace yuan
