#include "yuan/Driver/Options.h"
#include "yuan/Basic/Version.h"
#include "yuan/Tooling/ProjectConfig.h"
#include <filesystem>
#include <iostream>

namespace yuan {

namespace {

bool parseOptLevel(const std::string& level, OptLevel& result) {
    if (level == "0") {
        result = OptLevel::O0;
        return true;
    }
    if (level == "1") {
        result = OptLevel::O1;
        return true;
    }
    if (level == "2") {
        result = OptLevel::O2;
        return true;
    }
    if (level == "3") {
        result = OptLevel::O3;
        return true;
    }
    return false;
}

bool consumeValueArg(int argc,
                     char* argv[],
                     int& i,
                     std::string& value,
                     const char* optionName,
                     std::string& errorMsg) {
    if (i + 1 >= argc) {
        errorMsg = std::string("错误：") + optionName + " 选项需要参数";
        return false;
    }
    value = argv[++i];
    return true;
}

struct ParseState {
    bool SeenAction = false;
    bool SeenPhaseS = false;
    bool SeenEmitLLVM = false;

    bool CliSetOpt = false;
    bool CliSetStdLib = false;
    bool CliSetModuleCache = false;
};

bool setSingleAction(DriverOptions& options,
                     DriverAction action,
                     ParseState& state,
                     std::string& errorMsg) {
    if (state.SeenAction || state.SeenPhaseS || state.SeenEmitLLVM) {
        errorMsg = "错误：编译动作参数互斥，只能指定一个动作";
        return false;
    }
    options.Action = action;
    state.SeenAction = true;
    return true;
}

void prependVector(std::vector<std::string>& target,
                   const std::vector<std::string>& prefix) {
    if (prefix.empty()) {
        return;
    }
    std::vector<std::string> merged;
    merged.reserve(prefix.size() + target.size());
    merged.insert(merged.end(), prefix.begin(), prefix.end());
    merged.insert(merged.end(), target.begin(), target.end());
    target.swap(merged);
}

bool loadAndMergeProjectConfig(DriverOptions& options,
                               const ParseState& state,
                               std::string& errorMsg) {
    std::string projectFile = options.ProjectFile;
    if (projectFile.empty()) {
        std::string start = options.InputFiles.empty()
            ? std::filesystem::current_path().string()
            : options.InputFiles.front();
        projectFile = ProjectConfigLoader::discover(start);
    }

    if (projectFile.empty()) {
        return true;
    }

    ProjectConfig config;
    if (!ProjectConfigLoader::loadFromFile(projectFile, config, errorMsg)) {
        return false;
    }

    options.ProjectFile = projectFile;

    if (!state.CliSetStdLib && config.Compile.HasStdLibPath) {
        options.StdLibPath = config.Compile.StdLibPath;
    }
    if (!state.CliSetModuleCache && config.Compile.HasModuleCacheDir) {
        options.ModuleCacheDir = config.Compile.ModuleCacheDir;
    }
    if (!state.CliSetOpt && config.Compile.HasOptLevel) {
        switch (config.Compile.OptimizationLevel) {
            case 0: options.Optimization = OptLevel::O0; break;
            case 1: options.Optimization = OptLevel::O1; break;
            case 2: options.Optimization = OptLevel::O2; break;
            case 3:
            default:
                options.Optimization = OptLevel::O3;
                break;
        }
    }

    prependVector(options.IncludePaths, config.Compile.IncludePaths);
    prependVector(options.PackagePaths, config.Compile.PackagePaths);
    prependVector(options.LibraryPaths, config.Compile.LibraryPaths);
    prependVector(options.Libraries, config.Compile.Libraries);
    return true;
}

} // namespace

std::string DriverOptions::getOutputFileName() const {
    if (!OutputFile.empty()) {
        return OutputFile;
    }
    return deduceOutputFileName();
}

const char* DriverOptions::getOptLevelString() const {
    switch (Optimization) {
        case OptLevel::O0: return "O0";
        case OptLevel::O1: return "O1";
        case OptLevel::O2: return "O2";
        case OptLevel::O3: return "O3";
    }
    return "O0";
}

unsigned DriverOptions::getOptimizationLevel() const {
    switch (Optimization) {
        case OptLevel::O0: return 0;
        case OptLevel::O1: return 1;
        case OptLevel::O2: return 2;
        case OptLevel::O3: return 3;
    }
    return 0;
}

const char* DriverOptions::getActionString() const {
    switch (Action) {
        case DriverAction::Link: return "link";
        case DriverAction::EmitObj: return "emit-obj";
        case DriverAction::EmitLLVM: return "emit-llvm";
        case DriverAction::SyntaxOnly: return "syntax-only";
        case DriverAction::DumpTokens: return "dump-tokens";
        case DriverAction::ASTDump: return "ast-dump";
        case DriverAction::ASTPrint: return "ast-print";
    }
    return "link";
}

bool DriverOptions::validate(std::string& errorMsg) const {
    if (InputFiles.empty() && !ShowHelp && !ShowVersion) {
        errorMsg = "错误：未指定输入文件";
        return false;
    }

    for (const auto& file : InputFiles) {
        if (!std::filesystem::exists(file)) {
            errorMsg = "错误：输入文件不存在: " + file;
            return false;
        }

        std::filesystem::path path(file);
        if (path.extension() != ".yu") {
            errorMsg = "错误：输入文件必须是 .yu 文件: " + file;
            return false;
        }
    }

    if (!OutputFile.empty()) {
        std::filesystem::path outputPath(OutputFile);
        auto parentPath = outputPath.parent_path();
        if (!parentPath.empty() && !std::filesystem::exists(parentPath)) {
            errorMsg = "错误：输出目录不存在: " + parentPath.string();
            return false;
        }
    }

    if (!StdLibPath.empty() && !std::filesystem::exists(StdLibPath)) {
        errorMsg = "错误：标准库目录不存在: " + StdLibPath;
        return false;
    }

    for (const auto& pkgPath : PackagePaths) {
        if (!std::filesystem::exists(pkgPath)) {
            errorMsg = "错误：包搜索路径不存在: " + pkgPath;
            return false;
        }
    }

    if (InputFiles.size() > 1 &&
        (Action == DriverAction::EmitObj ||
         Action == DriverAction::EmitLLVM ||
         Action == DriverAction::DumpTokens ||
         Action == DriverAction::ASTDump ||
         Action == DriverAction::ASTPrint) &&
        !OutputFile.empty()) {
        errorMsg = "错误：多输入文件模式下，当前动作不支持单一 -o 输出";
        return false;
    }

    return true;
}

std::string DriverOptions::deduceOutputFileName() const {
    if (InputFiles.empty()) {
        return "a.out";
    }

    std::filesystem::path inputPath(InputFiles[0]);
    std::string baseName = inputPath.stem().string();

    switch (Action) {
        case DriverAction::DumpTokens:
            return baseName + ".tokens";
        case DriverAction::ASTDump:
            return baseName + ".ast";
        case DriverAction::ASTPrint:
            return baseName + ".pretty.yu";
        case DriverAction::EmitLLVM:
            return baseName + ".ll";
        case DriverAction::EmitObj:
            return baseName + ".o";
        case DriverAction::Link:
#ifdef _WIN32
            return baseName + ".exe";
#else
            return baseName;
#endif
        case DriverAction::SyntaxOnly:
            return "";
    }

    return "a.out";
}

bool parseDriverOptions(int argc,
                        char* argv[],
                        DriverOptions& options,
                        std::string& errorMsg) {
    ParseState state;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--") {
            for (++i; i < argc; ++i) {
                options.InputFiles.push_back(argv[i]);
            }
            break;
        }

        if (arg == "-h" || arg == "--help") {
            options.ShowHelp = true;
            return true;
        }
        if (arg == "--version") {
            options.ShowVersion = true;
            return true;
        }
        if (arg == "-v" || arg == "--verbose") {
            options.Verbose = true;
            continue;
        }
        if (arg == "-fruntime-net") {
            options.LinkRuntimeNet = true;
            continue;
        }
        if (arg == "-fno-runtime-net") {
            options.LinkRuntimeNet = false;
            continue;
        }
        if (arg == "-fruntime-gui") {
            options.LinkRuntimeGUI = true;
            continue;
        }
        if (arg == "-fno-runtime-gui") {
            options.LinkRuntimeGUI = false;
            continue;
        }

        if (arg == "-fsyntax-only") {
            if (!setSingleAction(options, DriverAction::SyntaxOnly, state, errorMsg)) {
                return false;
            }
            continue;
        }
        if (arg == "-c") {
            if (!setSingleAction(options, DriverAction::EmitObj, state, errorMsg)) {
                return false;
            }
            continue;
        }
        if (arg == "-dump-tokens") {
            if (!setSingleAction(options, DriverAction::DumpTokens, state, errorMsg)) {
                return false;
            }
            continue;
        }
        if (arg == "-ast-dump") {
            if (!setSingleAction(options, DriverAction::ASTDump, state, errorMsg)) {
                return false;
            }
            continue;
        }
        if (arg == "-ast-print") {
            if (!setSingleAction(options, DriverAction::ASTPrint, state, errorMsg)) {
                return false;
            }
            continue;
        }

        if (arg == "-S") {
            if (state.SeenAction) {
                errorMsg = "错误：-S/-emit-llvm 不能与其他动作同时使用";
                return false;
            }
            state.SeenPhaseS = true;
            continue;
        }
        if (arg == "-emit-llvm") {
            if (state.SeenAction) {
                errorMsg = "错误：-S/-emit-llvm 不能与其他动作同时使用";
                return false;
            }
            state.SeenEmitLLVM = true;
            continue;
        }

        if (arg == "--emit=tokens" || arg == "--emit=ast" || arg == "--emit=pretty" ||
            arg == "--emit=ir" || arg == "--emit=obj") {
            errorMsg = "错误：旧参数 --emit=* 已移除，请使用 -dump-tokens/-ast-dump/-ast-print/-S -emit-llvm/-c";
            return false;
        }

        if (arg == "-o") {
            if (!consumeValueArg(argc, argv, i, options.OutputFile, "-o", errorMsg)) {
                return false;
            }
            continue;
        }
        if (arg == "--module-cache") {
            if (!consumeValueArg(argc, argv, i, options.ModuleCacheDir, "--module-cache", errorMsg)) {
                return false;
            }
            state.CliSetModuleCache = true;
            continue;
        }
        if (arg == "--pkg-path") {
            std::string value;
            if (!consumeValueArg(argc, argv, i, value, "--pkg-path", errorMsg)) {
                return false;
            }
            options.PackagePaths.push_back(value);
            continue;
        }
        if (arg == "--stdlib") {
            if (!consumeValueArg(argc, argv, i, options.StdLibPath, "--stdlib", errorMsg)) {
                return false;
            }
            state.CliSetStdLib = true;
            continue;
        }
        if (arg == "--project") {
            if (!consumeValueArg(argc, argv, i, options.ProjectFile, "--project", errorMsg)) {
                return false;
            }
            continue;
        }

        if (arg.rfind("-O", 0) == 0) {
            std::string level = arg.substr(2);
            if (!parseOptLevel(level, options.Optimization)) {
                errorMsg = "错误：无效的优化级别 '" + level + "'";
                return false;
            }
            state.CliSetOpt = true;
            continue;
        }

        if (arg == "-I" || arg == "-L" || arg == "-l") {
            std::string value;
            if (!consumeValueArg(argc, argv, i, value, arg.c_str(), errorMsg)) {
                return false;
            }
            if (arg == "-I") {
                options.IncludePaths.push_back(value);
            } else if (arg == "-L") {
                options.LibraryPaths.push_back(value);
            } else {
                options.Libraries.push_back(value);
            }
            continue;
        }
        if (arg.rfind("-I", 0) == 0 && arg.size() > 2) {
            options.IncludePaths.push_back(arg.substr(2));
            continue;
        }
        if (arg.rfind("-L", 0) == 0 && arg.size() > 2) {
            options.LibraryPaths.push_back(arg.substr(2));
            continue;
        }
        if (arg.rfind("-l", 0) == 0 && arg.size() > 2) {
            options.Libraries.push_back(arg.substr(2));
            continue;
        }

        if (!arg.empty() && arg[0] == '-') {
            errorMsg = "错误：未知选项 '" + arg + "'";
            return false;
        }

        options.InputFiles.push_back(arg);
    }

    if (state.SeenPhaseS || state.SeenEmitLLVM) {
        if (!(state.SeenPhaseS && state.SeenEmitLLVM)) {
            errorMsg = "错误：输出 LLVM IR 必须同时指定 -S 和 -emit-llvm";
            return false;
        }
        options.Action = DriverAction::EmitLLVM;
    }

    if (!loadAndMergeProjectConfig(options, state, errorMsg)) {
        return false;
    }

    return true;
}

void printDriverHelp(const char* programName, std::ostream& os) {
    os << "Yuan 编译器 v" << VersionInfo::getVersionString() << "\n\n";
    os << "用法: " << programName << " [选项] <输入文件...>\n\n";
    os << "驱动动作:\n";
    os << "  (默认)                  链接生成可执行文件\n";
    os << "  -fsyntax-only           仅进行语法/语义检查\n";
    os << "  -c                      生成目标文件\n";
    os << "  -S -emit-llvm           生成 LLVM IR\n";
    os << "  -dump-tokens            输出词法 token\n";
    os << "  -ast-dump               输出树形 AST\n";
    os << "  -ast-print              输出源码重建结果\n\n";
    os << "通用选项:\n";
    os << "  -h, --help              显示此帮助信息\n";
    os << "  --version               显示版本信息\n";
    os << "  -o <文件>               指定输出文件名\n";
    os << "  -O<级别>                优化级别 (0,1,2,3)\n";
    os << "  -v, --verbose           启用详细输出\n";
    os << "  -I<路径> / -I <路径>    添加包含路径\n";
    os << "  -L<路径> / -L <路径>    添加库路径\n";
    os << "  -l<库名> / -l <库名>    添加链接库\n";
    os << "  -fruntime-net           链接网络运行时库（yuan_runtime_net）\n";
    os << "  -fno-runtime-net        不链接网络运行时库（改为链接 net stub）\n";
    os << "  -fruntime-gui           链接 GUI 运行时动态库（按平台）\n";
    os << "  -fno-runtime-gui        不链接 GUI 运行时动态库\n";
    os << "  --module-cache <路径>   模块缓存目录（.ymi/.o）\n";
    os << "  --pkg-path <路径>       预编译包搜索路径（可重复）\n";
    os << "  --stdlib <路径>         指定标准库根目录\n";
    os << "  --project <路径>        指定项目配置文件（yuan-project.json）\n";
}

void printDriverVersion(std::ostream& os) {
    os << "Yuan 编译器 v" << VersionInfo::getVersionString() << "\n";
    os << "构建时间: " << VersionInfo::getBuildTime() << "\n";
    os << "Git 提交: " << VersionInfo::getGitHash() << "\n";
    os << "LLVM 版本: " << VersionInfo::getLLVMVersion() << "\n";
}

} // namespace yuan
