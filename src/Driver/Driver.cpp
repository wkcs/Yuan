/// \file
/// \brief 编译器驱动器实现

#include "yuan/Driver/Driver.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/ASTDumper.h"
#include "yuan/AST/ASTPrinter.h"
#include "yuan/Basic/TextDiagnosticPrinter.h"
#include "yuan/Basic/TokenKinds.h"
#include "yuan/Basic/Version.h"
#include "yuan/CodeGen/CodeGen.h"
#include "yuan/Lexer/Lexer.h"
#include "yuan/Parser/Parser.h"
#include "yuan/Sema/Sema.h"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>

namespace yuan {

namespace {

std::string quoteArg(const std::string& value) {
    std::string out = "\"";
    for (char c : value) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

std::string getModuleNameFromPath(const std::string& inputFile) {
    std::filesystem::path p(inputFile);
    return p.stem().string();
}

std::string makeCachedMainObjectPath(const std::string& inputFile,
                                     const std::string& moduleCacheDir) {
    std::filesystem::path srcPath(inputFile);
    try {
        srcPath = std::filesystem::weakly_canonical(srcPath);
    } catch (...) {
        srcPath = srcPath.lexically_normal();
    }

    const std::string normalized = srcPath.string();
    const size_t pathHash = std::hash<std::string>{}(normalized);
    const std::string stem = srcPath.stem().string();
    const std::string fileName = stem + "." + std::to_string(pathHash) + ".o";
    return (std::filesystem::path(moduleCacheDir) / "main" / fileName).string();
}

unsigned mapOptLevel(OptLevel level) {
    switch (level) {
        case OptLevel::O0: return 0;
        case OptLevel::O1: return 1;
        case OptLevel::O2: return 2;
        case OptLevel::O3: return 3;
    }
    return 0;
}

} // namespace

Driver::Driver(const CompilerOptions& options)
    : Options(options) {
    SourceMgr = std::make_unique<SourceManager>();
    initializeDiagnostics();
}

Driver::~Driver() = default;

void Driver::initializeDiagnostics() {
    Diagnostics = std::make_unique<DiagnosticEngine>(*SourceMgr);
    DiagConsumer = std::make_unique<TextDiagnosticPrinter>(
        std::cerr, *SourceMgr, true /* 彩色输出 */);
    Diagnostics->setConsumer(std::move(DiagConsumer));
}

CompilationResult Driver::run() {
    auto startTime = std::chrono::high_resolution_clock::now();

    if (Options.Verbose) {
        std::cout << "Yuan 编译器 v" << VersionInfo::getVersionString() << "\n";
        std::cout << "驱动动作: " << Options.getActionString() << "\n";
        std::cout << "优化级别: " << Options.getOptLevelString() << "\n";
    }

    std::string errorMsg;
    if (!Options.validate(errorMsg)) {
        std::cerr << errorMsg << "\n";
        return CompilationResult::IOError;
    }

    CompilationResult result = loadInputFiles();
    if (result != CompilationResult::Success) {
        return result;
    }

    switch (Options.Action) {
        case DriverAction::Tokens:
            result = runTokenDump();
            break;
        case DriverAction::AST:
            result = runASTLikeDump(true);
            break;
        case DriverAction::Pretty:
            result = runASTLikeDump(false);
            break;
        case DriverAction::SyntaxOnly:
            result = runFrontend(true);
            break;
        case DriverAction::IR:
        case DriverAction::Object:
        case DriverAction::Link:
            result = runCodeGeneration();
            break;
    }

    if (Options.Verbose) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        std::cout << "编译完成，用时: " << duration.count() << "ms\n";
        printStatistics();
    }

    return result;
}

CompilationResult Driver::loadInputFiles() {
    if (Options.Verbose) {
        std::cout << "加载输入文件...\n";
    }

    Units.clear();
    Units.reserve(Options.InputFiles.size());
    for (const auto& inputFile : Options.InputFiles) {
        SourceManager::FileID fileID = SourceMgr->loadFile(inputFile);
        if (fileID == SourceManager::InvalidFileID) {
            std::cerr << "错误：无法加载文件 " << inputFile << "\n";
            return CompilationResult::IOError;
        }
        CompilationUnit unit;
        unit.InputFile = inputFile;
        unit.FileID = fileID;
        Units.push_back(std::move(unit));
        if (Options.Verbose) {
            std::cout << "  已加载: " << inputFile << "\n";
        }
    }
    return CompilationResult::Success;
}

CompilationResult Driver::runTokenDump() {
    std::unique_ptr<std::ofstream> outFile;
    std::ostream* out = &std::cout;
    if (!Options.OutputFile.empty() && Options.OutputFile != "-") {
        outFile = std::make_unique<std::ofstream>(Options.OutputFile);
        if (!outFile->good()) {
            std::cerr << "错误：无法创建输出文件 " << Options.OutputFile << "\n";
            return CompilationResult::IOError;
        }
        out = outFile.get();
    }

    for (size_t i = 0; i < Units.size(); ++i) {
        auto& unit = Units[i];
        if (Units.size() > 1) {
            *out << "== Tokens: " << unit.InputFile << " ==\n";
        }
        Lexer lexer(*SourceMgr, *Diagnostics, unit.FileID);
        CompilationResult result = emitTokens(lexer, unit.InputFile, *out);
        if (result != CompilationResult::Success) {
            return result;
        }
        if (Units.size() > 1 && i + 1 < Units.size()) {
            *out << "\n";
        }
    }

    if (Diagnostics->hasErrors()) {
        return CompilationResult::LexerError;
    }
    return CompilationResult::Success;
}

CompilationResult Driver::runFrontend(bool needSema) {
    for (auto& unit : Units) {
        if (!unit.Parsed) {
            unit.Context = std::make_unique<ASTContext>(*SourceMgr);
            unit.Context->setPointerBitWidth(static_cast<unsigned>(sizeof(uintptr_t) * 8));
            Lexer lexer(*SourceMgr, *Diagnostics, unit.FileID);
            Parser parser(lexer, *Diagnostics, *unit.Context);
            unit.Declarations = parser.parseCompilationUnit();
            unit.Parsed = true;
            if (Diagnostics->hasErrors()) {
                return CompilationResult::ParserError;
            }
        }

        if (needSema && !unit.Analyzed) {
            unit.Semantic = std::make_unique<Sema>(*unit.Context, *Diagnostics);
            configureModuleManager(*unit.Semantic);
            yuan::CompilationUnit semaUnit(unit.FileID);
            for (Decl* decl : unit.Declarations) {
                semaUnit.addDecl(decl);
            }
            (void)unit.Semantic->analyze(&semaUnit);
            unit.Analyzed = true;
            if (Diagnostics->hasErrors()) {
                return CompilationResult::SemanticError;
            }
        }
    }
    return CompilationResult::Success;
}

CompilationResult Driver::runASTLikeDump(bool treeMode) {
    CompilationResult frontendResult = runFrontend(false);
    if (frontendResult != CompilationResult::Success) {
        return frontendResult;
    }

    std::unique_ptr<std::ofstream> outFile;
    std::ostream* out = &std::cout;
    if (!Options.OutputFile.empty() && Options.OutputFile != "-") {
        outFile = std::make_unique<std::ofstream>(Options.OutputFile);
        if (!outFile->good()) {
            std::cerr << "错误：无法创建输出文件 " << Options.OutputFile << "\n";
            return CompilationResult::IOError;
        }
        out = outFile.get();
    }

    for (size_t i = 0; i < Units.size(); ++i) {
        auto& unit = Units[i];
        if (Units.size() > 1) {
            *out << "== " << (treeMode ? "AST" : "Pretty") << ": " << unit.InputFile << " ==\n";
        }

        if (treeMode) {
            ASTDumper dumper(*out);
            for (const auto* decl : unit.Declarations) {
                dumper.dump(decl);
            }
        } else {
            ASTPrinter printer(*out);
            for (const auto* decl : unit.Declarations) {
                printer.print(decl);
                *out << "\n";
            }
        }
        if (Units.size() > 1 && i + 1 < Units.size()) {
            *out << "\n";
        }
    }
    return CompilationResult::Success;
}

CompilationResult Driver::runCodeGeneration() {
    CompilationResult frontendResult = runFrontend(true);
    if (frontendResult != CompilationResult::Success) {
        return frontendResult;
    }

    const bool emitIR = Options.Action == DriverAction::IR;
    const bool emitObj = Options.Action == DriverAction::Object;
    const bool emitExe = Options.Action == DriverAction::Link;

    std::vector<std::string> objectFiles;
    std::unordered_set<std::string> seenObjects;
    objectFiles.reserve(Units.size() * 2);
    unsigned optLevel = mapOptLevel(Options.Optimization);

    for (size_t i = 0; i < Units.size(); ++i) {
        auto& unit = Units[i];
        std::string moduleName = getModuleNameFromPath(unit.InputFile);
        CodeGen codeGen(*unit.Context, moduleName);

        for (Decl* decl : unit.Declarations) {
            if (!codeGen.generateDecl(decl)) {
                std::cerr << "错误：代码生成失败: " << unit.InputFile << "\n";
                return CompilationResult::CodeGenError;
            }
        }

        std::string verifyError;
        if (!codeGen.verifyModule(&verifyError)) {
            std::cerr << "错误：LLVM IR 验证失败: " << verifyError << "\n";
            return CompilationResult::CodeGenError;
        }

        if (emitIR) {
            std::string output = Options.OutputFile.empty()
                ? deducePerInputOutput(unit.InputFile, ".ll")
                : Options.OutputFile;
            if (!codeGen.emitIRToFile(output)) {
                std::cerr << "错误：无法写入 IR 文件 " << output << "\n";
                return CompilationResult::CodeGenError;
            }
            continue;
        }

        std::string objFile;
        if (emitObj && !Options.OutputFile.empty()) {
            objFile = Options.OutputFile;
        } else if (emitExe) {
            // 链接模式下的主输入对象写入模块缓存，避免污染输出目录。
            objFile = makeCachedMainObjectPath(unit.InputFile, Options.ModuleCacheDir);
        } else {
            objFile = deducePerInputOutput(unit.InputFile, ".o");
        }

        std::filesystem::path objPath(objFile);
        if (!objPath.parent_path().empty()) {
            std::error_code ec;
            std::filesystem::create_directories(objPath.parent_path(), ec);
            if (ec) {
                std::cerr << "错误：无法创建目标文件目录 " << objPath.parent_path().string() << "\n";
                return CompilationResult::IOError;
            }
        }

        if (!codeGen.emitObjectFile(objFile, optLevel)) {
            std::cerr << "错误：无法生成目标文件 " << objFile << "\n";
            return CompilationResult::CodeGenError;
        }
        if (seenObjects.insert(objFile).second) {
            objectFiles.push_back(objFile);
        }
    }

    if (!emitExe) {
        return CompilationResult::Success;
    }

    // 链接模式：将导入模块作为独立编译单元生成/复用对象，再统一链接。
    std::unordered_set<std::string> mainInputs;
    for (const auto& unit : Units) {
        std::filesystem::path p(unit.InputFile);
        try {
            p = std::filesystem::weakly_canonical(p);
        } catch (...) {
            p = p.lexically_normal();
        }
        mainInputs.insert(p.string());
    }

    for (auto& unit : Units) {
        if (!unit.Semantic) {
            continue;
        }

        ModuleManager& moduleMgr = unit.Semantic->getModuleManager();
        for (const auto& entry : moduleMgr.getLoadedModules()) {
            const ModuleInfo* info = entry.second.get();
            if (!info) {
                continue;
            }

            std::string depObj = info->ObjectPath;
            bool hasDepObj = !depObj.empty() && std::filesystem::exists(depObj);

            if (!info->FilePath.empty()) {
                std::filesystem::path srcPath(info->FilePath);
                try {
                    srcPath = std::filesystem::weakly_canonical(srcPath);
                } catch (...) {
                    srcPath = srcPath.lexically_normal();
                }

                if (mainInputs.find(srcPath.string()) != mainInputs.end()) {
                    continue;
                }

                bool needRebuild = !hasDepObj;
                if (!needRebuild && std::filesystem::exists(depObj)) {
                    std::error_code ec1, ec2;
                    auto srcTime = std::filesystem::last_write_time(srcPath, ec1);
                    auto objTime = std::filesystem::last_write_time(depObj, ec2);
                    if (!ec1 && !ec2 && objTime < srcTime) {
                        needRebuild = true;
                    }
                }

                if (needRebuild) {
                    CompilationResult depResult = buildModuleObject(
                        srcPath.string(),
                        optLevel,
                        info->ObjectPath,
                        depObj);
                    if (depResult != CompilationResult::Success) {
                        return depResult;
                    }
                }
            } else {
                // 仅接口包：必须直接提供对象文件
                if (!hasDepObj) {
                    std::cerr << "错误：预编译模块缺少对象文件: " << info->Name << "\n";
                    return CompilationResult::LinkError;
                }
            }

            if (!depObj.empty() && std::filesystem::exists(depObj) &&
                seenObjects.insert(depObj).second) {
                objectFiles.push_back(depObj);
            }
        }
    }

    std::string executable = Options.OutputFile.empty()
        ? Options.getOutputFileName()
        : Options.OutputFile;
    return linkObjects(objectFiles, executable);
}

void Driver::configureModuleManager(Sema& sema) const {
    ModuleManager& moduleMgr = sema.getModuleManager();

    if (!Options.StdLibPath.empty()) {
        moduleMgr.setStdLibPath(Options.StdLibPath);
    }
    moduleMgr.setModuleCacheDir(Options.ModuleCacheDir);
    for (const auto& pkgPath : Options.PackagePaths) {
        moduleMgr.addPackagePath(pkgPath);
    }
    for (const auto& includePath : Options.IncludePaths) {
        moduleMgr.addPackagePath(includePath);
    }
}

CompilationResult Driver::buildModuleObject(const std::string& moduleSourcePath,
                                            unsigned optLevel,
                                            const std::string& preferredObjectPath,
                                            std::string& outObjectFile) {
    std::filesystem::path srcPath(moduleSourcePath);
    try {
        srcPath = std::filesystem::weakly_canonical(srcPath);
    } catch (...) {
        srcPath = srcPath.lexically_normal();
    }

    if (!std::filesystem::exists(srcPath)) {
        std::cerr << "错误：依赖模块源文件不存在: " << srcPath.string() << "\n";
        return CompilationResult::IOError;
    }

    SourceManager::FileID fileID = SourceMgr->loadFile(srcPath.string());
    if (fileID == SourceManager::InvalidFileID) {
        std::cerr << "错误：无法加载依赖模块文件 " << srcPath.string() << "\n";
        return CompilationResult::IOError;
    }

    auto depCtx = std::make_unique<ASTContext>(*SourceMgr);
    Lexer lexer(*SourceMgr, *Diagnostics, fileID);
    Parser parser(lexer, *Diagnostics, *depCtx);
    std::vector<Decl*> depDecls = parser.parseCompilationUnit();
    if (Diagnostics->hasErrors()) {
        return CompilationResult::ParserError;
    }

    auto depSema = std::make_unique<Sema>(*depCtx, *Diagnostics);
    configureModuleManager(*depSema);
    yuan::CompilationUnit depUnit(fileID);
    for (Decl* decl : depDecls) {
        depUnit.addDecl(decl);
    }
    (void)depSema->analyze(&depUnit);
    if (Diagnostics->hasErrors()) {
        return CompilationResult::SemanticError;
    }

    std::string moduleName = srcPath.stem().string();
    CodeGen codeGen(*depCtx, moduleName);
    for (Decl* decl : depDecls) {
        if (!codeGen.generateDecl(decl)) {
            std::cerr << "错误：依赖模块代码生成失败: " << srcPath.string() << "\n";
            return CompilationResult::CodeGenError;
        }
    }

    std::string verifyError;
    if (!codeGen.verifyModule(&verifyError)) {
        std::cerr << "错误：依赖模块 LLVM IR 验证失败: " << verifyError << "\n";
        return CompilationResult::CodeGenError;
    }

    std::filesystem::path outputPath;
    if (!preferredObjectPath.empty()) {
        outputPath = preferredObjectPath;
    } else {
        outputPath = std::filesystem::path(Options.ModuleCacheDir) / (srcPath.stem().string() + ".o");
    }

    std::error_code ec;
    std::filesystem::create_directories(outputPath.parent_path(), ec);
    if (ec) {
        std::cerr << "错误：无法创建依赖模块输出目录: " << outputPath.parent_path().string() << "\n";
        return CompilationResult::IOError;
    }

    if (!codeGen.emitObjectFile(outputPath.string(), optLevel)) {
        std::cerr << "错误：无法生成依赖模块目标文件 " << outputPath.string() << "\n";
        return CompilationResult::CodeGenError;
    }

    outObjectFile = outputPath.string();
    return CompilationResult::Success;
}

CompilationResult Driver::linkObjects(const std::vector<std::string>& objectFiles,
                                      const std::string& executableFile) {
    if (objectFiles.empty()) {
        std::cerr << "错误：没有可链接的目标文件\n";
        return CompilationResult::LinkError;
    }

    std::ostringstream cmd;
#if defined(__APPLE__)
    cmd << "clang++";
#elif defined(__linux__)
    cmd << "g++";
#elif defined(_WIN32)
    cmd << "clang++";
#else
    return CompilationResult::LinkError;
#endif
    cmd << " -o " << quoteArg(executableFile);
    for (const auto& obj : objectFiles) {
        cmd << " " << quoteArg(obj);
    }

#ifdef YUAN_RUNTIME_LIB_PATH
    cmd << " " << quoteArg(YUAN_RUNTIME_LIB_PATH);
#endif
#ifdef YUAN_RUNTIME_LINK_FLAGS
    cmd << " " << YUAN_RUNTIME_LINK_FLAGS;
#endif
    for (const auto& libPath : Options.LibraryPaths) {
        cmd << " -L" << quoteArg(libPath);
    }
    for (const auto& lib : Options.Libraries) {
        cmd << " -l" << lib;
    }

    if (Options.Verbose) {
        std::cout << "链接命令: " << cmd.str() << "\n";
    }
    int rc = std::system(cmd.str().c_str());
    if (rc != 0) {
        std::cerr << "错误：链接失败\n";
        return CompilationResult::LinkError;
    }
    return CompilationResult::Success;
}

CompilationResult Driver::emitTokens(Lexer& lexer,
                                     const std::string& inputFile,
                                     std::ostream& output) {
    output << "// Yuan 词法分析结果\n";
    output << "// 源文件: " << inputFile << "\n\n";

    Token token;
    unsigned tokenCount = 0;
    do {
        unsigned errorsBefore = Diagnostics->getErrorCount();
        token = lexer.lex();
        unsigned errorsAfter = Diagnostics->getErrorCount();
        bool hasTokenError = errorsAfter > errorsBefore;
        if (token.getKind() != TokenKind::EndOfFile && !hasTokenError) {
            SourceLocation loc = token.getLocation();
            auto [line, col] = SourceMgr->getLineAndColumn(loc);
            output << "Token[" << tokenCount << "]: "
                   << getTokenName(token.getKind())
                   << " \"" << token.getText() << "\""
                   << " @" << inputFile << ":" << line << ":" << col << "\n";
            ++tokenCount;
        }
    } while (token.getKind() != TokenKind::EndOfFile);

    output << "\n// 总计: " << tokenCount << " 个 token\n";
    return CompilationResult::Success;
}

std::string Driver::deducePerInputOutput(const std::string& inputFile, const std::string& ext) const {
    std::filesystem::path p(inputFile);
    return p.stem().string() + ext;
}

const char* Driver::getResultString(CompilationResult result) const {
    switch (result) {
        case CompilationResult::Success: return "成功";
        case CompilationResult::LexerError: return "词法分析错误";
        case CompilationResult::ParserError: return "语法分析错误";
        case CompilationResult::SemanticError: return "语义分析错误";
        case CompilationResult::CodeGenError: return "代码生成错误";
        case CompilationResult::LinkError: return "链接错误";
        case CompilationResult::IOError: return "文件 I/O 错误";
        case CompilationResult::InternalError: return "内部错误";
    }
    return "未知错误";
}

void Driver::printStatistics() const {
    std::cout << "编译统计:\n";
    std::cout << "  错误数量: " << Diagnostics->getErrorCount() << "\n";
    std::cout << "  警告数量: " << Diagnostics->getWarningCount() << "\n";
    std::cout << "  输入文件: " << Options.InputFiles.size() << "\n";
}

} // namespace yuan
