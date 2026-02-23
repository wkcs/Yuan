#include "yuan/Driver/Driver.h"
#include "yuan/Basic/Version.h"
#include "yuan/Frontend/CompilerInstance.h"
#include "yuan/Frontend/FrontendAction.h"
#include "yuan/Sema/ModuleManager.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
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

FrontendActionKind toFrontendActionKind(DriverAction action) {
    switch (action) {
        case DriverAction::Link:
        case DriverAction::EmitObj:
            return FrontendActionKind::EmitObj;
        case DriverAction::EmitLLVM:
            return FrontendActionKind::EmitLLVM;
        case DriverAction::SyntaxOnly:
            return FrontendActionKind::SyntaxOnly;
        case DriverAction::DumpTokens:
            return FrontendActionKind::DumpTokens;
        case DriverAction::ASTDump:
            return FrontendActionKind::ASTDump;
        case DriverAction::ASTPrint:
            return FrontendActionKind::ASTPrint;
    }
    return FrontendActionKind::SyntaxOnly;
}

CompilationResult fromFrontendStatus(FrontendStatus status) {
    switch (status) {
        case FrontendStatus::Success: return CompilationResult::Success;
        case FrontendStatus::LexerError: return CompilationResult::LexerError;
        case FrontendStatus::ParserError: return CompilationResult::ParserError;
        case FrontendStatus::SemanticError: return CompilationResult::SemanticError;
        case FrontendStatus::CodeGenError: return CompilationResult::CodeGenError;
        case FrontendStatus::IOError: return CompilationResult::IOError;
        case FrontendStatus::InternalError: return CompilationResult::InternalError;
    }
    return CompilationResult::InternalError;
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

CompilerInvocation buildInvocation(const DriverOptions& options,
                                   FrontendActionKind action) {
    CompilerInvocation invocation;
    invocation.Action = action;
    invocation.Verbose = options.Verbose;
    invocation.OptimizationLevel = options.getOptimizationLevel();
    invocation.OutputFile = options.OutputFile;
    invocation.StdLibPath = options.StdLibPath;
    invocation.ModuleCacheDir = options.ModuleCacheDir;
    invocation.IncludePaths = options.IncludePaths;
    invocation.PackagePaths = options.PackagePaths;
    invocation.LibraryPaths = options.LibraryPaths;
    invocation.Libraries = options.Libraries;
    return invocation;
}

struct DriverContext {
    const DriverOptions& Options;
    std::ostream& Out;
    std::ostream& Err;
    std::vector<std::string> ObjectFiles;
    std::unordered_set<std::string> SeenObjectFiles;
};

void addObjectFile(DriverContext& ctx, const std::string& objectFile) {
    if (objectFile.empty()) {
        return;
    }
    if (ctx.SeenObjectFiles.insert(objectFile).second) {
        ctx.ObjectFiles.push_back(objectFile);
    }
}

CompilationResult buildModuleObject(const std::string& moduleSourcePath,
                                    const DriverOptions& options,
                                    const std::string& preferredObjectPath,
                                    std::string& outObjectFile,
                                    std::ostream& err) {
    std::filesystem::path srcPath(moduleSourcePath);
    try {
        srcPath = std::filesystem::weakly_canonical(srcPath);
    } catch (...) {
        srcPath = srcPath.lexically_normal();
    }

    if (!std::filesystem::exists(srcPath)) {
        err << "错误：依赖模块源文件不存在: " << srcPath.string() << "\n";
        return CompilationResult::IOError;
    }

    std::string outputPath = preferredObjectPath;
    if (outputPath.empty()) {
        outputPath = (std::filesystem::path(options.ModuleCacheDir) /
                      (srcPath.stem().string() + ".o")).string();
    }

    CompilerInvocation invocation = buildInvocation(options, FrontendActionKind::EmitObj);
    CompilerInstance ci(invocation);
    ci.enableTextDiagnostics(err, true);

    EmitObjAction action;
    std::vector<FrontendInputFile> inputs = {
        FrontendInputFile::fromFile(srcPath.string(), outputPath)
    };
    FrontendResult result = executeFrontendAction(ci, action, inputs);
    if (!result.succeeded()) {
        return fromFrontendStatus(result.OverallStatus);
    }

    outObjectFile = result.Files.empty() ? outputPath : result.Files.front().OutputPath;
    return CompilationResult::Success;
}

CompilationResult collectDependencyObjects(CompilerInstance& ci, DriverContext& ctx) {
    std::unordered_set<std::string> mainInputs;
    for (const auto& unit : ci.getUnits()) {
        std::filesystem::path p(unit.Input.Name);
        try {
            p = std::filesystem::weakly_canonical(p);
        } catch (...) {
            p = p.lexically_normal();
        }
        mainInputs.insert(p.string());
    }

    for (const auto& unit : ci.getUnits()) {
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
                        ctx.Options,
                        info->ObjectPath,
                        depObj,
                        ctx.Err);
                    if (depResult != CompilationResult::Success) {
                        return depResult;
                    }
                }
            } else {
                if (!hasDepObj) {
                    ctx.Err << "错误：预编译模块缺少对象文件: " << info->Name << "\n";
                    return CompilationResult::LinkError;
                }
            }

            if (!depObj.empty() && std::filesystem::exists(depObj)) {
                addObjectFile(ctx, depObj);
            }
        }
    }

    return CompilationResult::Success;
}

class ToolChain {
public:
    explicit ToolChain(const DriverOptions& options) : Options(options) {}

    CompilationResult linkObjects(const std::vector<std::string>& objectFiles,
                                  const std::string& executableFile,
                                  std::ostream& out,
                                  std::ostream& err) const {
        if (objectFiles.empty()) {
            err << "错误：没有可链接的目标文件\n";
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
            out << "链接命令: " << cmd.str() << "\n";
        }

        int rc = std::system(cmd.str().c_str());
        if (rc != 0) {
            err << "错误：链接失败\n";
            return CompilationResult::LinkError;
        }
        return CompilationResult::Success;
    }

private:
    const DriverOptions& Options;
};

class Command {
public:
    virtual ~Command() = default;
    virtual CompilationResult execute(DriverContext& ctx) = 0;
};

class FrontendCommand : public Command {
public:
    FrontendCommand(FrontendActionKind actionKind,
                    std::vector<FrontendInputFile> inputs,
                    std::string textOutputPath,
                    bool collectObjectOutputs,
                    bool collectModuleDeps)
        : ActionKind(actionKind),
          Inputs(std::move(inputs)),
          TextOutputPath(std::move(textOutputPath)),
          CollectObjectOutputs(collectObjectOutputs),
          CollectModuleDependencies(collectModuleDeps) {}

    CompilationResult execute(DriverContext& ctx) override {
        CompilerInvocation invocation = buildInvocation(ctx.Options, ActionKind);
        CompilerInstance ci(invocation);
        ci.enableTextDiagnostics(ctx.Err, true);

        std::unique_ptr<std::ofstream> outFile;
        std::ostream* textOut = &ctx.Out;
        if (!TextOutputPath.empty() && TextOutputPath != "-") {
            outFile = std::make_unique<std::ofstream>(TextOutputPath);
            if (!outFile->good()) {
                ctx.Err << "错误：无法创建输出文件 " << TextOutputPath << "\n";
                return CompilationResult::IOError;
            }
            textOut = outFile.get();
        }

        std::unique_ptr<FrontendAction> action;
        switch (ActionKind) {
            case FrontendActionKind::DumpTokens:
                action = std::make_unique<DumpTokensAction>(*textOut);
                break;
            case FrontendActionKind::ASTDump:
                action = std::make_unique<ASTDumpAction>(*textOut);
                break;
            case FrontendActionKind::ASTPrint:
                action = std::make_unique<ASTPrintAction>(*textOut);
                break;
            case FrontendActionKind::SyntaxOnly:
                action = std::make_unique<SyntaxOnlyAction>();
                break;
            case FrontendActionKind::EmitLLVM:
                action = std::make_unique<EmitLLVMAction>();
                break;
            case FrontendActionKind::EmitObj:
                action = std::make_unique<EmitObjAction>();
                break;
        }

        FrontendResult result = executeFrontendAction(ci, *action, Inputs);
        if (!result.succeeded()) {
            return fromFrontendStatus(result.OverallStatus);
        }

        if (CollectObjectOutputs) {
            for (const auto& fileResult : result.Files) {
                addObjectFile(ctx, fileResult.OutputPath);
            }
        }

        if (CollectModuleDependencies) {
            CompilationResult depStatus = collectDependencyObjects(ci, ctx);
            if (depStatus != CompilationResult::Success) {
                return depStatus;
            }
        }

        return CompilationResult::Success;
    }

private:
    FrontendActionKind ActionKind;
    std::vector<FrontendInputFile> Inputs;
    std::string TextOutputPath;
    bool CollectObjectOutputs;
    bool CollectModuleDependencies;
};

class LinkCommand : public Command {
public:
    explicit LinkCommand(std::string executablePath)
        : ExecutablePath(std::move(executablePath)) {}

    CompilationResult execute(DriverContext& ctx) override {
        ToolChain toolChain(ctx.Options);
        return toolChain.linkObjects(ctx.ObjectFiles, ExecutablePath, ctx.Out, ctx.Err);
    }

private:
    std::string ExecutablePath;
};

struct Compilation {
    std::vector<std::unique_ptr<Command>> Commands;
};

Compilation buildCompilation(const DriverOptions& options) {
    Compilation compilation;

    std::vector<FrontendInputFile> inputs;
    inputs.reserve(options.InputFiles.size());

    switch (options.Action) {
        case DriverAction::DumpTokens:
        case DriverAction::ASTDump:
        case DriverAction::ASTPrint:
        case DriverAction::SyntaxOnly:
        case DriverAction::EmitLLVM:
        case DriverAction::EmitObj: {
            for (const auto& input : options.InputFiles) {
                if (!options.OutputFile.empty() && options.InputFiles.size() == 1 &&
                    (options.Action == DriverAction::EmitObj ||
                     options.Action == DriverAction::EmitLLVM)) {
                    inputs.push_back(FrontendInputFile::fromFile(input, options.OutputFile));
                } else {
                    inputs.push_back(FrontendInputFile::fromFile(input));
                }
            }

            std::string textOutput;
            if (options.Action == DriverAction::DumpTokens ||
                options.Action == DriverAction::ASTDump ||
                options.Action == DriverAction::ASTPrint) {
                textOutput = options.OutputFile;
            }

            compilation.Commands.push_back(std::make_unique<FrontendCommand>(
                toFrontendActionKind(options.Action),
                std::move(inputs),
                textOutput,
                false,
                false));
            break;
        }
        case DriverAction::Link: {
            for (const auto& input : options.InputFiles) {
                std::string objPath = makeCachedMainObjectPath(input, options.ModuleCacheDir);
                inputs.push_back(FrontendInputFile::fromFile(input, objPath));
            }

            compilation.Commands.push_back(std::make_unique<FrontendCommand>(
                FrontendActionKind::EmitObj,
                std::move(inputs),
                "",
                true,
                true));

            std::string executable = options.OutputFile.empty()
                ? options.getOutputFileName()
                : options.OutputFile;
            compilation.Commands.push_back(std::make_unique<LinkCommand>(executable));
            break;
        }
    }

    return compilation;
}

} // namespace

Driver::Driver(const DriverOptions& options)
    : Options(options) {}

CompilationResult Driver::run() {
    auto startTime = std::chrono::high_resolution_clock::now();

    std::string errorMsg;
    if (!Options.validate(errorMsg)) {
        std::cerr << errorMsg << "\n";
        return CompilationResult::IOError;
    }

    if (Options.Verbose) {
        std::cout << "Yuan 编译器 v" << VersionInfo::getVersionString() << "\n";
        std::cout << "驱动动作: " << Options.getActionString() << "\n";
        std::cout << "优化级别: " << Options.getOptLevelString() << "\n";
        if (!Options.ProjectFile.empty()) {
            std::cout << "项目配置: " << Options.ProjectFile << "\n";
        }
    }

    Compilation compilation = buildCompilation(Options);
    DriverContext ctx{Options, std::cout, std::cerr, {}, {}};

    for (const auto& command : compilation.Commands) {
        CompilationResult result = command->execute(ctx);
        if (result != CompilationResult::Success) {
            return result;
        }
    }

    if (Options.Verbose) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        std::cout << "编译完成，用时: " << duration.count() << "ms\n";
    }

    return CompilationResult::Success;
}

int Driver::getExitCode(CompilationResult result) {
    switch (result) {
        case CompilationResult::Success:
            return 0;
        case CompilationResult::LexerError:
        case CompilationResult::ParserError:
        case CompilationResult::SemanticError:
            return 1;
        case CompilationResult::CodeGenError:
        case CompilationResult::LinkError:
            return 2;
        case CompilationResult::IOError:
            return 3;
        case CompilationResult::InternalError:
            return 4;
    }
    return 4;
}

} // namespace yuan
