#include "yuan/Frontend/CompilerInstance.h"
#include "yuan/Frontend/FrontendAction.h"
#include "yuan/Tooling/ProjectConfig.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>

using namespace yuan;

namespace {

struct FormatOptions {
    bool ShowHelp = false;
    bool CheckOnly = false;
    bool InPlace = false;
    bool UseStdin = false;

    std::optional<std::string> ProjectFile;
    std::optional<std::string> StdLibPath;
    std::optional<std::string> ModuleCacheDir;
    std::vector<std::string> IncludePaths;
    std::vector<std::string> PackagePaths;
    std::vector<std::string> InputFiles;
};

void printHelp(const char* argv0) {
    std::cout << "Usage: " << argv0 << " [options] <files...>\n";
    std::cout << "  --check                 check formatting only\n";
    std::cout << "  -i                      rewrite files in place\n";
    std::cout << "  --stdin                 read source from stdin\n";
    std::cout << "  --project <path>        project config path\n";
    std::cout << "  --stdlib <path>         override stdlib root\n";
    std::cout << "  --module-cache <path>   override module cache\n";
    std::cout << "  --pkg-path <path>       add package search path\n";
    std::cout << "  -I <path>               add include search path\n";
}

bool consumeValue(int argc, char* argv[], int& i, std::string& value, const char* name) {
    if (i + 1 >= argc) {
        std::cerr << "error: option " << name << " expects a value\n";
        return false;
    }
    value = argv[++i];
    return true;
}

bool parseArgs(int argc, char* argv[], FormatOptions& options) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            options.ShowHelp = true;
            return true;
        }
        if (arg == "--check") {
            options.CheckOnly = true;
            continue;
        }
        if (arg == "-i") {
            options.InPlace = true;
            continue;
        }
        if (arg == "--stdin") {
            options.UseStdin = true;
            continue;
        }
        if (arg == "--project") {
            std::string v;
            if (!consumeValue(argc, argv, i, v, "--project")) return false;
            options.ProjectFile = v;
            continue;
        }
        if (arg == "--stdlib") {
            std::string v;
            if (!consumeValue(argc, argv, i, v, "--stdlib")) return false;
            options.StdLibPath = v;
            continue;
        }
        if (arg == "--module-cache") {
            std::string v;
            if (!consumeValue(argc, argv, i, v, "--module-cache")) return false;
            options.ModuleCacheDir = v;
            continue;
        }
        if (arg == "--pkg-path") {
            std::string v;
            if (!consumeValue(argc, argv, i, v, "--pkg-path")) return false;
            options.PackagePaths.push_back(v);
            continue;
        }
        if (arg == "-I") {
            std::string v;
            if (!consumeValue(argc, argv, i, v, "-I")) return false;
            options.IncludePaths.push_back(v);
            continue;
        }
        if (arg.rfind("-I", 0) == 0 && arg.size() > 2) {
            options.IncludePaths.push_back(arg.substr(2));
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            std::cerr << "error: unknown option '" << arg << "'\n";
            return false;
        }
        options.InputFiles.push_back(arg);
    }

    if (options.UseStdin && !options.InputFiles.empty()) {
        std::cerr << "error: --stdin cannot be used with input files\n";
        return false;
    }
    if (!options.UseStdin && options.InputFiles.empty()) {
        std::cerr << "error: no input files\n";
        return false;
    }
    if (options.CheckOnly && options.InPlace) {
        std::cerr << "error: --check cannot be combined with -i\n";
        return false;
    }
    return true;
}

CompilerInvocation buildInvocation(const FormatOptions& options, const std::string& inputName) {
    CompilerInvocation invocation;
    invocation.Action = FrontendActionKind::ASTPrint;

    std::string projectFile = options.ProjectFile.value_or(ProjectConfigLoader::discover(inputName));
    if (!projectFile.empty()) {
        ProjectConfig config;
        std::string error;
        if (ProjectConfigLoader::loadFromFile(projectFile, config, error)) {
            applyProjectConfig(config, invocation, true);
        }
    }

    if (options.StdLibPath.has_value()) {
        invocation.StdLibPath = *options.StdLibPath;
    }
    if (options.ModuleCacheDir.has_value()) {
        invocation.ModuleCacheDir = *options.ModuleCacheDir;
    }
    invocation.IncludePaths.insert(invocation.IncludePaths.end(),
                                   options.IncludePaths.begin(),
                                   options.IncludePaths.end());
    invocation.PackagePaths.insert(invocation.PackagePaths.end(),
                                   options.PackagePaths.begin(),
                                   options.PackagePaths.end());
    return invocation;
}

bool runFormat(const FormatOptions& options,
               const std::string& inputName,
               const std::string& content,
               std::string& formatted) {
    CompilerInvocation invocation = buildInvocation(options, inputName);
    CompilerInstance ci(invocation);
    ci.enableTextDiagnostics(std::cerr, true);

    std::ostringstream oss;
    ASTPrintAction action(oss);
    FrontendResult result = executeFrontendAction(
        ci, action, {FrontendInputFile::fromBuffer(inputName, content)});
    if (!result.succeeded()) {
        return false;
    }

    formatted = oss.str();
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    FormatOptions options;
    if (!parseArgs(argc, argv, options)) {
        return 1;
    }
    if (options.ShowHelp) {
        printHelp(argv[0]);
        return 0;
    }

    int rc = 0;

    auto processOne = [&](const std::string& name, const std::string& input) {
        std::string formatted;
        if (!runFormat(options, name, input, formatted)) {
            rc = 2;
            return;
        }

        if (options.CheckOnly) {
            if (formatted != input) {
                std::cout << name << "\n";
                rc = 1;
            }
            return;
        }

        if (options.InPlace && name != "<stdin>") {
            std::ofstream out(name);
            out << formatted;
            return;
        }

        std::cout << formatted;
    };

    if (options.UseStdin) {
        std::ostringstream buffer;
        buffer << std::cin.rdbuf();
        processOne("<stdin>", buffer.str());
        return rc;
    }

    for (const auto& file : options.InputFiles) {
        std::ifstream in(file);
        if (!in.good()) {
            std::cerr << "error: unable to read file " << file << "\n";
            return 2;
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
        processOne(file, buffer.str());
    }

    return rc;
}
