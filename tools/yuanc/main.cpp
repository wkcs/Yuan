/// \file
/// \brief Yuan 编译器主程序。

#include "yuan/Basic/Version.h"
#include "yuan/Driver/Driver.h"
#include "yuan/Driver/Options.h"
#include <iostream>
#include <string>

using namespace yuan;

namespace {

void printHelp(const char* programName) {
    std::cout << "Yuan 编译器 v" << VersionInfo::getVersionString() << "\n\n";
    std::cout << "用法: " << programName << " [选项] <输入文件...>\n\n";
    std::cout << "驱动动作:\n";
    std::cout << "  (默认)                  链接生成可执行文件\n";
    std::cout << "  -fsyntax-only           仅进行语法/语义检查\n";
    std::cout << "  -S                      生成 LLVM IR\n";
    std::cout << "  -c                      生成目标文件\n";
    std::cout << "  --emit=tokens           输出词法 token\n";
    std::cout << "  --emit=ast              输出树形 AST\n";
    std::cout << "  --emit=pretty           输出源码重建结果\n\n";
    std::cout << "通用选项:\n";
    std::cout << "  -h, --help              显示此帮助信息\n";
    std::cout << "  --version               显示版本信息\n";
    std::cout << "  -o <文件>               指定输出文件名\n";
    std::cout << "  -O<级别>                优化级别 (0,1,2,3)\n";
    std::cout << "  -v, --verbose           启用详细输出\n";
    std::cout << "  -I<路径> / -I <路径>    添加包含路径\n";
    std::cout << "  -L<路径> / -L <路径>    添加库路径\n";
    std::cout << "  -l<库名> / -l <库名>    添加链接库\n";
    std::cout << "  --module-cache <路径>   模块缓存目录（.ymi/.o）\n";
    std::cout << "  --pkg-path <路径>       预编译包搜索路径（可重复）\n";
    std::cout << "  --stdlib <路径>         指定标准库根目录\n";
}

void printVersion() {
    std::cout << "Yuan 编译器 v" << VersionInfo::getVersionString() << "\n";
    std::cout << "构建时间: " << VersionInfo::getBuildTime() << "\n";
    std::cout << "Git 提交: " << VersionInfo::getGitHash() << "\n";
    std::cout << "LLVM 版本: " << VersionInfo::getLLVMVersion() << "\n";
}

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

bool parseEmitAction(const std::string& mode, DriverAction& action) {
    if (mode == "tokens") {
        action = DriverAction::Tokens;
        return true;
    }
    if (mode == "ast") {
        action = DriverAction::AST;
        return true;
    }
    if (mode == "pretty") {
        action = DriverAction::Pretty;
        return true;
    }
    return false;
}

bool consumeValueArg(int argc, char* argv[], int& i, std::string& value, const char* optionName) {
    if (i + 1 >= argc) {
        std::cerr << "错误：" << optionName << " 选项需要参数\n";
        return false;
    }
    value = argv[++i];
    return true;
}

bool parseArguments(int argc, char* argv[], CompilerOptions& options) {
    bool seenEmit = false;
    bool seenPhaseAction = false; // -fsyntax-only / -S / -c

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

        if (arg == "-fsyntax-only" || arg == "-S" || arg == "-c") {
            if (seenEmit) {
                std::cerr << "错误：--emit=* 不能与 -fsyntax-only/-S/-c 同时使用\n";
                return false;
            }
            if (seenPhaseAction) {
                std::cerr << "错误：-fsyntax-only/-S/-c 只能指定一个\n";
                return false;
            }
            seenPhaseAction = true;
            if (arg == "-fsyntax-only") {
                options.Action = DriverAction::SyntaxOnly;
            } else if (arg == "-S") {
                options.Action = DriverAction::IR;
            } else {
                options.Action = DriverAction::Object;
            }
            continue;
        }

        if (arg.rfind("--emit=", 0) == 0) {
            if (seenPhaseAction) {
                std::cerr << "错误：--emit=* 不能与 -fsyntax-only/-S/-c 同时使用\n";
                return false;
            }
            if (seenEmit) {
                std::cerr << "错误：--emit=* 只能指定一次\n";
                return false;
            }
            std::string mode = arg.substr(7);
            if (!parseEmitAction(mode, options.Action)) {
                std::cerr << "错误：无效的 --emit 模式 '" << mode << "'，仅支持 tokens/ast/pretty\n";
                return false;
            }
            seenEmit = true;
            continue;
        }

        if (arg == "-o") {
            if (!consumeValueArg(argc, argv, i, options.OutputFile, "-o")) {
                return false;
            }
            continue;
        }
        if (arg == "--module-cache") {
            if (!consumeValueArg(argc, argv, i, options.ModuleCacheDir, "--module-cache")) {
                return false;
            }
            continue;
        }
        if (arg == "--pkg-path") {
            std::string value;
            if (!consumeValueArg(argc, argv, i, value, "--pkg-path")) {
                return false;
            }
            options.PackagePaths.push_back(value);
            continue;
        }
        if (arg == "--stdlib") {
            if (!consumeValueArg(argc, argv, i, options.StdLibPath, "--stdlib")) {
                return false;
            }
            continue;
        }
        if (arg.rfind("-O", 0) == 0) {
            std::string level = arg.substr(2);
            if (!parseOptLevel(level, options.Optimization)) {
                std::cerr << "错误：无效的优化级别 '" << level << "'\n";
                return false;
            }
            continue;
        }
        if (arg == "-I" || arg == "-L" || arg == "-l") {
            std::string value;
            if (!consumeValueArg(argc, argv, i, value, arg.c_str())) {
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
            std::cerr << "错误：未知选项 '" << arg << "'\n";
            return false;
        }

        options.InputFiles.push_back(arg);
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    CompilerOptions options;
    if (!parseArguments(argc, argv, options)) {
        return 1;
    }

    if (options.ShowHelp) {
        printHelp(argv[0]);
        return 0;
    }
    if (options.ShowVersion) {
        printVersion();
        return 0;
    }

    if (options.InputFiles.empty()) {
        std::cerr << "错误：未指定输入文件\n";
        std::cerr << "使用 '" << argv[0] << " --help' 查看帮助信息\n";
        return 1;
    }

    try {
        Driver driver(options);
        CompilationResult result = driver.run();
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
    } catch (const std::exception& e) {
        std::cerr << "内部错误: " << e.what() << "\n";
        return 4;
    } catch (...) {
        std::cerr << "未知内部错误\n";
        return 4;
    }

    return 0;
}
