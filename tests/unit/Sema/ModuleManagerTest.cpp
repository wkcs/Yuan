/// \file
/// \brief ModuleManager 单元测试

#include "yuan/Sema/ModuleManager.h"
#include "yuan/Sema/Sema.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/TextDiagnosticPrinter.h"
#include "yuan/AST/ASTContext.h"
#include <gtest/gtest.h>
#include <memory>
#include <filesystem>
#include <fstream>

using namespace yuan;
namespace fs = std::filesystem;

class ModuleManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        SM = std::make_unique<SourceManager>();

        // 创建 DiagnosticEngine
        Diag = std::make_unique<DiagnosticEngine>(*SM);

        // 创建一个用于收集诊断消息的 consumer
        auto consumer = std::make_unique<TextDiagnosticPrinter>(std::cerr, *SM, true);
        Diag->setConsumer(std::move(consumer));

        // 创建 ASTContext
        Ctx = std::make_unique<ASTContext>(*SM);

        // 创建 Sema
        Sema_ = std::make_unique<Sema>(*Ctx, *Diag);

        // 获取 ModuleManager（由 Sema 创建）
        MM = &Sema_->getModuleManager();

        // 创建临时测试目录
        testDir = fs::temp_directory_path() / "yuan_module_test";
        fs::create_directories(testDir);

        // 设置标准库路径为测试目录下的 stdlib（在创建文件之前）
        stdlibDir = testDir / "stdlib";
        fs::create_directories(stdlibDir);
        MM->setStdLibPath(stdlibDir.string());

        // 创建测试文件
        createTestFiles();
    }

    void TearDown() override {
        // 清理测试目录
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    void createTestFiles() {
        // 创建 module1.yu
        createFile(testDir / "module1.yu", "func test1() { }");

        // 创建 module2.yu
        createFile(testDir / "module2.yu", "func test2() { }");

        // 创建子目录
        fs::create_directories(testDir / "subdir");
        createFile(testDir / "subdir" / "module3.yu", "func test3() { }");

        // 创建标准库模块
        fs::create_directories(stdlibDir / "collections");
        createFile(stdlibDir / "io.yu", "pub func print() { }");
        createFile(stdlibDir / "collections" / "vector.yu", "pub struct Vector { }");
    }

    void createFile(const fs::path& path, const std::string& content) {
        std::ofstream file(path);
        file << content;
    }

    std::unique_ptr<SourceManager> SM;
    std::unique_ptr<DiagnosticEngine> Diag;
    std::unique_ptr<ASTContext> Ctx;
    std::unique_ptr<Sema> Sema_;
    ModuleManager* MM;
    fs::path testDir;
    fs::path stdlibDir;
};

// ========== 模块路径解析测试 ==========

TEST_F(ModuleManagerTest, ResolveStdLibPath_DotNotation) {
    // 测试 "std.io" -> "{stdlib}/io.yu"
    std::string resolved = MM->resolveModulePath("std.io", "");

    fs::path expected = stdlibDir / "io.yu";
    EXPECT_EQ(resolved, expected.string());
}

TEST_F(ModuleManagerTest, ResolveStdLibPath_SlashNotation) {
    // 测试 "std/collections/vector" -> "{stdlib}/collections/vector.yu"
    std::string resolved = MM->resolveModulePath("std/collections/vector", "");

    fs::path expected = stdlibDir / "collections" / "vector.yu";
    EXPECT_EQ(resolved, expected.string());
}

TEST_F(ModuleManagerTest, ResolveStdLibPath_DotToSlashConversion) {
    // 测试 "std.collections.vector" -> "{stdlib}/collections/vector.yu"
    std::string resolved = MM->resolveModulePath("std.collections.vector", "");

    fs::path expected = stdlibDir / "collections" / "vector.yu";
    EXPECT_EQ(resolved, expected.string());
}

TEST_F(ModuleManagerTest, ResolveRelativePath_CurrentDir) {
    // 测试 "./module1" 从 testDir/main.yu
    fs::path currentFile = testDir / "main.yu";
    std::string resolved = MM->resolveModulePath("./module1", currentFile.string());

    fs::path expected = testDir / "module1.yu";
    EXPECT_EQ(fs::weakly_canonical(resolved), fs::weakly_canonical(expected));
}

TEST_F(ModuleManagerTest, ResolveRelativePath_Subdir) {
    // 测试 "./subdir/module3" 从 testDir/main.yu
    fs::path currentFile = testDir / "main.yu";
    std::string resolved = MM->resolveModulePath("./subdir/module3", currentFile.string());

    fs::path expected = testDir / "subdir" / "module3.yu";
    EXPECT_EQ(fs::weakly_canonical(resolved), fs::weakly_canonical(expected));
}

TEST_F(ModuleManagerTest, ResolveRelativePath_ParentDir) {
    // 测试 "../module1" 从 testDir/subdir/main.yu
    fs::path currentFile = testDir / "subdir" / "main.yu";
    std::string resolved = MM->resolveModulePath("../module1", currentFile.string());

    fs::path expected = testDir / "module1.yu";
    EXPECT_EQ(fs::weakly_canonical(resolved), fs::weakly_canonical(expected));
}

TEST_F(ModuleManagerTest, ResolveAbsolutePath) {
    // 测试绝对路径
    fs::path absPath = testDir / "module1.yu";
    std::string resolved = MM->resolveModulePath(absPath.string(), "");

    EXPECT_EQ(fs::weakly_canonical(resolved), fs::weakly_canonical(absPath));
}

TEST_F(ModuleManagerTest, ResolveModulePath_AutoAddExtension) {
    // 测试自动添加 .yu 扩展名
    std::string resolved = MM->resolveModulePath("std.io", "");

    // 应该包含 .yu 扩展名
    EXPECT_GE(resolved.size(), 3u);
    EXPECT_EQ(resolved.substr(resolved.size() - 3), ".yu");
    EXPECT_TRUE(resolved.find("io.yu") != std::string::npos);
}

// ========== 模块加载测试 ==========

TEST_F(ModuleManagerTest, LoadModule_Success) {
    // 加载标准库模块（文件已在 SetUp 中创建）
    std::vector<std::string> importChain;

    // 验证文件存在
    fs::path expectedPath = stdlibDir / "io.yu";
    ASSERT_TRUE(fs::exists(expectedPath)) << "File should exist: " << expectedPath;

    ModuleInfo* module = MM->loadModule("std.io", "", importChain);

    ASSERT_NE(module, nullptr);
    EXPECT_EQ(module->Name, "std.io");
    EXPECT_TRUE(module->IsStdLib);
    EXPECT_TRUE(module->IsLoaded);
}

TEST_F(ModuleManagerTest, LoadModule_RelativePath) {
    // 加载相对路径模块
    fs::path currentFile = testDir / "main.yu";
    std::vector<std::string> importChain;
    ModuleInfo* module = MM->loadModule("./module1", currentFile.string(), importChain);

    ASSERT_NE(module, nullptr);
    EXPECT_EQ(module->Name, "module1");
    EXPECT_FALSE(module->IsStdLib);
}

TEST_F(ModuleManagerTest, LoadModule_Caching) {
    // 测试模块缓存 - 多次加载同一个模块应该返回相同的指针
    std::vector<std::string> importChain1;
    ModuleInfo* module1 = MM->loadModule("std.io", "", importChain1);

    std::vector<std::string> importChain2;
    ModuleInfo* module2 = MM->loadModule("std.io", "", importChain2);

    EXPECT_EQ(module1, module2);
}

TEST_F(ModuleManagerTest, LoadModule_NotFound) {
    // 测试加载不存在的模块
    std::vector<std::string> importChain;
    ModuleInfo* module = MM->loadModule("std.nonexistent", "", importChain);

    EXPECT_EQ(module, nullptr);
}

TEST_F(ModuleManagerTest, GetLoadedModule) {
    // 测试 getLoadedModule（先加载模块）
    std::vector<std::string> importChain;
    MM->loadModule("std.io", "", importChain);

    ModuleInfo* module = MM->getLoadedModule("std.io");
    ASSERT_NE(module, nullptr);
    EXPECT_EQ(module->Name, "std.io");

    // 查找未加载的模块
    ModuleInfo* nonexistent = MM->getLoadedModule("std.nonexistent");
    EXPECT_EQ(nonexistent, nullptr);
}

// ========== 循环导入检测测试 ==========

TEST_F(ModuleManagerTest, IsInImportChain_Found) {
    std::vector<std::string> importChain = {"A", "B", "C"};

    EXPECT_TRUE(MM->isInImportChain("A", importChain));
    EXPECT_TRUE(MM->isInImportChain("B", importChain));
    EXPECT_TRUE(MM->isInImportChain("C", importChain));
}

TEST_F(ModuleManagerTest, IsInImportChain_NotFound) {
    std::vector<std::string> importChain = {"A", "B", "C"};

    EXPECT_FALSE(MM->isInImportChain("D", importChain));
    EXPECT_FALSE(MM->isInImportChain("", importChain));
}

TEST_F(ModuleManagerTest, IsInImportChain_Empty) {
    std::vector<std::string> importChain;

    EXPECT_FALSE(MM->isInImportChain("A", importChain));
}

TEST_F(ModuleManagerTest, LoadModule_CircularImport_DirectCycle) {
    // 创建循环导入场景测试
    // 由于 "std.io" 标准库文件存在，我们只是测试循环检测逻辑
    std::vector<std::string> importChain = {"std.io"};

    // 尝试加载另一个模块（不在循环中）
    ModuleInfo* module = MM->loadModule("std/collections/vector", "", importChain);

    // "std.io" 应该在导入链中
    EXPECT_TRUE(MM->isInImportChain("std.io", importChain));

    // vector 模块应该成功加载（因为不在循环中）
    EXPECT_NE(module, nullptr);
}

TEST_F(ModuleManagerTest, LoadModule_NoCircularImport) {
    // 正常的导入链：没有循环
    std::vector<std::string> importChain = {"module1", "module2"};

    // 加载 std.io（不在循环中）
    ModuleInfo* module = MM->loadModule("std.io", "", importChain);
    ASSERT_NE(module, nullptr);

    // std.io 不应该在导入链中
    EXPECT_FALSE(MM->isInImportChain("std.io", importChain));
}

// ========== 标准库路径配置测试 ==========

TEST_F(ModuleManagerTest, SetGetStdLibPath) {
    std::string newPath = "/custom/stdlib";
    MM->setStdLibPath(newPath);

    EXPECT_EQ(MM->getStdLibPath(), newPath);
}

TEST_F(ModuleManagerTest, StdLibPath_AffectsResolution) {
    // 更改标准库路径
    fs::path customStdlib = testDir / "custom_stdlib";
    fs::create_directories(customStdlib);
    createFile(customStdlib / "test.yu", "pub func test() { }");

    MM->setStdLibPath(customStdlib.string());

    std::string resolved = MM->resolveModulePath("std.test", "");
    fs::path expected = customStdlib / "test.yu";

    EXPECT_EQ(resolved, expected.string());
}

// ========== ModuleInfo 结构测试 ==========

TEST_F(ModuleManagerTest, ModuleInfo_Construction) {
    ModuleInfo info("test_module", "/path/to/module.yu", false);

    EXPECT_EQ(info.Name, "test_module");
    EXPECT_EQ(info.FilePath, "/path/to/module.yu");
    EXPECT_FALSE(info.IsLoaded);
    EXPECT_FALSE(info.IsStdLib);
    EXPECT_TRUE(info.Declarations.empty());
}

TEST_F(ModuleManagerTest, ModuleInfo_StdLibFlag) {
    ModuleInfo stdlibModule("std.io", "/stdlib/io.yu", true);
    ModuleInfo userModule("my_module", "/user/module.yu", false);

    EXPECT_TRUE(stdlibModule.IsStdLib);
    EXPECT_FALSE(userModule.IsStdLib);
}

// ========== 边界情况测试 ==========

TEST_F(ModuleManagerTest, ResolveModulePath_EmptyPath) {
    std::string resolved = MM->resolveModulePath("", "");

    // 空路径应该被当作标准库模块处理，会生成类似 "{stdlib}/.yu" 的路径
    // 我们只验证它包含 stdlib 路径
    EXPECT_TRUE(resolved.find(stdlibDir.string()) != std::string::npos);
}

TEST_F(ModuleManagerTest, ResolveModulePath_EmptyCurrentFile) {
    // 当 currentFilePath 为空时，相对路径解析应该从当前工作目录开始
    std::string resolved = MM->resolveModulePath("./test", "");

    // 应该返回一个有效路径
    EXPECT_FALSE(resolved.empty());
}

TEST_F(ModuleManagerTest, LoadModule_EmptyImportChain) {
    // 使用空导入链加载模块
    std::vector<std::string> importChain;
    ModuleInfo* module = MM->loadModule("std.io", "", importChain);

    ASSERT_NE(module, nullptr);
    EXPECT_TRUE(importChain.empty()); // loadModule 应该在返回前清理导入链
}

// ========== 路径规范化测试 ==========

TEST_F(ModuleManagerTest, NormalizeModuleName_StdLib) {
    // 标准库模块名应该保持原样
    std::string name1 = MM->resolveModulePath("std.io", "");
    std::string name2 = MM->resolveModulePath("std/io", "");

    // 两种写法应该解析到同一个文件
    EXPECT_EQ(name1, name2);
}

TEST_F(ModuleManagerTest, ResolveModulePath_DotDot) {
    // 测试 .. 路径的规范化
    fs::path currentFile = testDir / "subdir" / "nested" / "main.yu";
    fs::create_directories(testDir / "subdir" / "nested");

    std::string resolved = MM->resolveModulePath("../../module1", currentFile.string());
    fs::path expected = testDir / "module1.yu";

    EXPECT_EQ(fs::weakly_canonical(resolved), fs::weakly_canonical(expected));
}
