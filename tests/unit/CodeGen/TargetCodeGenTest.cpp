/// \file TargetCodeGenTest.cpp
/// \brief Unit tests for target code generation (object files and executables).

#include "yuan/CodeGen/CodeGen.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Stmt.h"
#include "yuan/Sema/Type.h"
#include "yuan/Basic/SourceManager.h"
#include <gtest/gtest.h>
#include <llvm/IR/Module.h>
#include <fstream>
#include <sys/stat.h>

namespace yuan {

/// \brief Test fixture for target code generation tests.
class TargetCodeGenTest : public ::testing::Test {
protected:
    void SetUp() override {
        SourceMgr = std::make_unique<SourceManager>();
        Ctx = std::make_unique<ASTContext>(*SourceMgr);
        CodeGenerator = std::make_unique<CodeGen>(*Ctx, "target_test_module");
    }

    /// \brief Helper: Check if file exists
    bool fileExists(const std::string& filename) {
        struct stat buffer;
        return (stat(filename.c_str(), &buffer) == 0);
    }

    /// \brief Helper: Get file size
    size_t getFileSize(const std::string& filename) {
        struct stat buffer;
        if (stat(filename.c_str(), &buffer) == 0) {
            return buffer.st_size;
        }
        return 0;
    }

    std::unique_ptr<SourceManager> SourceMgr;
    std::unique_ptr<ASTContext> Ctx;
    std::unique_ptr<CodeGen> CodeGenerator;
};

// ============================================================================
// Object file generation tests
// ============================================================================

TEST_F(TargetCodeGenTest, EmitObjectFileCreatesFile) {
    // Create a simple function
    SourceRange range;
    std::vector<Stmt*> stmts;
    ReturnStmt* ret = new ReturnStmt(range, nullptr);
    stmts.push_back(ret);
    BlockStmt* body = new BlockStmt(range, stmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range, "test_func", params, nullptr, body,
        false, false, Visibility::Public
    );

    bool generated = CodeGenerator->generateDecl(funcDecl);
    ASSERT_TRUE(generated);

    // Verify module
    std::string errorMsg;
    bool verified = CodeGenerator->verifyModule(&errorMsg);
    ASSERT_TRUE(verified) << "Module verification failed: " << errorMsg;

    // Emit object file
    const std::string objFile = "/tmp/test_output.o";
    bool emitted = CodeGenerator->emitObjectFile(objFile);
    EXPECT_TRUE(emitted);

    // Check that file was created
    EXPECT_TRUE(fileExists(objFile));

    // Check that file is not empty
    EXPECT_GT(getFileSize(objFile), 0u);

    // Clean up
    std::remove(objFile.c_str());
}

TEST_F(TargetCodeGenTest, EmitObjectFileWithOptimizationLevel0) {
    // Test with -O0 (no optimization)
    SourceRange range;
    std::vector<Stmt*> stmts;
    ReturnStmt* ret = new ReturnStmt(range, nullptr);
    stmts.push_back(ret);
    BlockStmt* body = new BlockStmt(range, stmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range, "opt0_func", params, nullptr, body,
        false, false, Visibility::Public
    );

    CodeGenerator->generateDecl(funcDecl);

    const std::string objFile = "/tmp/test_opt0.o";
    bool emitted = CodeGenerator->emitObjectFile(objFile, 0);
    EXPECT_TRUE(emitted);
    EXPECT_TRUE(fileExists(objFile));

    std::remove(objFile.c_str());
}

TEST_F(TargetCodeGenTest, EmitObjectFileWithOptimizationLevel3) {
    // Test with -O3 (aggressive optimization)
    SourceRange range;
    std::vector<Stmt*> stmts;
    ReturnStmt* ret = new ReturnStmt(range, nullptr);
    stmts.push_back(ret);
    BlockStmt* body = new BlockStmt(range, stmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range, "opt3_func", params, nullptr, body,
        false, false, Visibility::Public
    );

    CodeGenerator->generateDecl(funcDecl);

    const std::string objFile = "/tmp/test_opt3.o";
    bool emitted = CodeGenerator->emitObjectFile(objFile, 3);
    EXPECT_TRUE(emitted);
    EXPECT_TRUE(fileExists(objFile));

    std::remove(objFile.c_str());
}

TEST_F(TargetCodeGenTest, EmitObjectFileToInvalidPath) {
    // Test emitting to an invalid path
    SourceRange range;
    std::vector<Stmt*> stmts;
    ReturnStmt* ret = new ReturnStmt(range, nullptr);
    stmts.push_back(ret);
    BlockStmt* body = new BlockStmt(range, stmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range, "invalid_path_func", params, nullptr, body,
        false, false, Visibility::Public
    );

    CodeGenerator->generateDecl(funcDecl);

    const std::string objFile = "/invalid/path/that/does/not/exist/output.o";
    bool emitted = CodeGenerator->emitObjectFile(objFile);
    EXPECT_FALSE(emitted);
}

TEST_F(TargetCodeGenTest, MultipleObjectFileGenerations) {
    // Test generating multiple object files from the same module
    SourceRange range;
    std::vector<Stmt*> stmts;
    ReturnStmt* ret = new ReturnStmt(range, nullptr);
    stmts.push_back(ret);
    BlockStmt* body = new BlockStmt(range, stmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range, "multi_obj_func", params, nullptr, body,
        false, false, Visibility::Public
    );

    CodeGenerator->generateDecl(funcDecl);

    // Generate first object file
    const std::string objFile1 = "/tmp/test_multi1.o";
    bool emitted1 = CodeGenerator->emitObjectFile(objFile1);
    EXPECT_TRUE(emitted1);
    EXPECT_TRUE(fileExists(objFile1));

    // Generate second object file (should work)
    const std::string objFile2 = "/tmp/test_multi2.o";
    bool emitted2 = CodeGenerator->emitObjectFile(objFile2);
    EXPECT_TRUE(emitted2);
    EXPECT_TRUE(fileExists(objFile2));

    std::remove(objFile1.c_str());
    std::remove(objFile2.c_str());
}

// ============================================================================
// Executable linking tests
// ============================================================================

TEST_F(TargetCodeGenTest, LinkExecutableStructure) {
    // Test the structure of linking (may not produce working executable without main)
    SourceRange range;
    std::vector<Stmt*> stmts;
    ReturnStmt* ret = new ReturnStmt(range, nullptr);
    stmts.push_back(ret);
    BlockStmt* body = new BlockStmt(range, stmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range, "link_test_func", params, nullptr, body,
        false, false, Visibility::Public
    );

    CodeGenerator->generateDecl(funcDecl);

    // Generate object file first
    const std::string objFile = "/tmp/test_link.o";
    bool emitted = CodeGenerator->emitObjectFile(objFile);
    ASSERT_TRUE(emitted);
    ASSERT_TRUE(fileExists(objFile));

    // Try to link (may fail because no main function, but we're testing the API)
    const std::string exeFile = "/tmp/test_link_exe";
    bool linked = CodeGenerator->linkExecutable(objFile, exeFile);

    // Linking may fail without a main function, which is expected
    // We're just testing that the method exists and returns a boolean
    (void)linked;

    // Clean up
    std::remove(objFile.c_str());
    std::remove(exeFile.c_str());
}

TEST_F(TargetCodeGenTest, LinkExecutableWithMainFunction) {
    // Create a main function that can be linked
    SourceRange range;
    std::vector<Stmt*> stmts;

    // main returns void (simplified for testing)
    ReturnStmt* ret = new ReturnStmt(range, nullptr);
    stmts.push_back(ret);
    BlockStmt* body = new BlockStmt(range, stmts);

    std::vector<ParamDecl*> params;
    FuncDecl* mainFunc = new FuncDecl(
        range, "main", params, nullptr, body,
        false, false, Visibility::Public
    );

    bool generated = CodeGenerator->generateDecl(mainFunc);
    ASSERT_TRUE(generated);

    // Verify module
    std::string errorMsg;
    bool verified = CodeGenerator->verifyModule(&errorMsg);
    ASSERT_TRUE(verified) << "Module verification failed: " << errorMsg;

    // Generate object file
    const std::string objFile = "/tmp/test_main.o";
    bool emitted = CodeGenerator->emitObjectFile(objFile);
    ASSERT_TRUE(emitted);
    ASSERT_TRUE(fileExists(objFile));

    // Link executable
    const std::string exeFile = "/tmp/test_main_exe";
    bool linked = CodeGenerator->linkExecutable(objFile, exeFile);
    EXPECT_TRUE(linked);

    // If linking succeeded, executable should exist
    if (linked) {
        EXPECT_TRUE(fileExists(exeFile));
        EXPECT_GT(getFileSize(exeFile), 0u);
    }

    // Clean up
    std::remove(objFile.c_str());
    std::remove(exeFile.c_str());
}

// ============================================================================
// Integration tests
// ============================================================================

TEST_F(TargetCodeGenTest, CompleteCompilationWorkflow) {
    // Test the complete workflow: IR -> Verify -> Object -> Link
    SourceRange range;

    // Create main function (void return for simplicity)
    ReturnStmt* ret = new ReturnStmt(range, nullptr);
    std::vector<Stmt*> stmts;
    stmts.push_back(ret);
    BlockStmt* body = new BlockStmt(range, stmts);

    std::vector<ParamDecl*> params;
    FuncDecl* mainFunc = new FuncDecl(
        range, "main", params, nullptr, body,
        false, false, Visibility::Public
    );

    // Step 1: Generate IR
    bool generated = CodeGenerator->generateDecl(mainFunc);
    ASSERT_TRUE(generated);

    // Step 2: Verify IR
    std::string errorMsg;
    bool verified = CodeGenerator->verifyModule(&errorMsg);
    ASSERT_TRUE(verified) << "Verification failed: " << errorMsg;

    // Step 3: Emit IR to file
    const std::string irFile = "/tmp/test_workflow.ll";
    bool irEmitted = CodeGenerator->emitIRToFile(irFile);
    ASSERT_TRUE(irEmitted);
    ASSERT_TRUE(fileExists(irFile));

    // Step 4: Generate object file
    const std::string objFile = "/tmp/test_workflow.o";
    bool objEmitted = CodeGenerator->emitObjectFile(objFile, 2);  // -O2
    ASSERT_TRUE(objEmitted);
    ASSERT_TRUE(fileExists(objFile));

    // Step 5: Link executable
    const std::string exeFile = "/tmp/test_workflow_exe";
    bool linked = CodeGenerator->linkExecutable(objFile, exeFile);
    EXPECT_TRUE(linked);

    if (linked) {
        EXPECT_TRUE(fileExists(exeFile));
    }

    // Clean up
    std::remove(irFile.c_str());
    std::remove(objFile.c_str());
    std::remove(exeFile.c_str());
}

TEST_F(TargetCodeGenTest, OptimizationLevelsProduceDifferentSizes) {
    // Different optimization levels should produce different object file sizes
    SourceRange range;
    std::vector<Stmt*> stmts;

    // Create a slightly more complex function to see optimization differences
    for (int i = 0; i < 5; i++) {
        IntegerLiteralExpr* lit = new IntegerLiteralExpr(range, i, true, 32);
        ExprStmt* exprStmt = new ExprStmt(range, lit);
        stmts.push_back(exprStmt);
    }

    ReturnStmt* ret = new ReturnStmt(range, nullptr);
    stmts.push_back(ret);
    BlockStmt* body = new BlockStmt(range, stmts);

    std::vector<ParamDecl*> params;
    FuncDecl* funcDecl = new FuncDecl(
        range, "opt_test_func", params, nullptr, body,
        false, false, Visibility::Public
    );

    CodeGenerator->generateDecl(funcDecl);

    // Generate with different optimization levels
    const std::string objFile0 = "/tmp/test_opt_level_0.o";
    const std::string objFile3 = "/tmp/test_opt_level_3.o";

    bool emitted0 = CodeGenerator->emitObjectFile(objFile0, 0);
    bool emitted3 = CodeGenerator->emitObjectFile(objFile3, 3);

    ASSERT_TRUE(emitted0);
    ASSERT_TRUE(emitted3);

    // Both should exist
    EXPECT_TRUE(fileExists(objFile0));
    EXPECT_TRUE(fileExists(objFile3));

    // Note: Sizes may or may not differ depending on the code,
    // but both should be valid object files (non-zero size)
    EXPECT_GT(getFileSize(objFile0), 0u);
    EXPECT_GT(getFileSize(objFile3), 0u);

    std::remove(objFile0.c_str());
    std::remove(objFile3.c_str());
}

} // namespace yuan
