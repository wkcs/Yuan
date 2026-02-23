#include "yuan/AST/Decl.h"
#include "yuan/Frontend/CompilerInstance.h"
#include "yuan/Frontend/FrontendAction.h"
#include "yuan/Tooling/ProjectConfig.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>

using namespace yuan;

namespace {

struct AnalyzeOptions {
    bool ShowHelp = false;
    bool ListChecks = false;

    std::optional<std::string> ProjectFile;
    std::optional<std::string> StdLibPath;
    std::optional<std::string> ModuleCacheDir;
    std::vector<std::string> IncludePaths;
    std::vector<std::string> PackagePaths;
    std::set<std::string> Checks;
    std::vector<std::string> InputFiles;
};

constexpr const char* kCheckTooManyParams = "style-too-many-params";
constexpr const char* kCheckLongFunction = "style-long-function";

void printHelp(const char* argv0) {
    std::cout << "Usage: " << argv0 << " [options] <files...>\n";
    std::cout << "  --list-checks           list available checks\n";
    std::cout << "  --checks=a,b            enable checks\n";
    std::cout << "  --project <path>        project config path\n";
    std::cout << "  --stdlib <path>         override stdlib root\n";
    std::cout << "  --module-cache <path>   override module cache\n";
    std::cout << "  --pkg-path <path>       add package search path\n";
    std::cout << "  -I <path>               add include search path\n";
}

void printChecks() {
    std::cout << kCheckTooManyParams << "\n";
    std::cout << kCheckLongFunction << "\n";
}

bool consumeValue(int argc, char* argv[], int& i, std::string& value, const char* name) {
    if (i + 1 >= argc) {
        std::cerr << "error: option " << name << " expects a value\n";
        return false;
    }
    value = argv[++i];
    return true;
}

void parseCheckList(const std::string& text, std::set<std::string>& checks) {
    size_t start = 0;
    while (start <= text.size()) {
        size_t comma = text.find(',', start);
        std::string item = text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        if (!item.empty()) {
            checks.insert(item);
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
}

bool parseArgs(int argc, char* argv[], AnalyzeOptions& options) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            options.ShowHelp = true;
            return true;
        }
        if (arg == "--list-checks") {
            options.ListChecks = true;
            continue;
        }
        if (arg.rfind("--checks=", 0) == 0) {
            parseCheckList(arg.substr(9), options.Checks);
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

    if (!options.ShowHelp && !options.ListChecks && options.InputFiles.empty()) {
        std::cerr << "error: no input files\n";
        return false;
    }

    if (options.Checks.empty()) {
        options.Checks.insert(kCheckTooManyParams);
        options.Checks.insert(kCheckLongFunction);
    }

    return true;
}

CompilerInvocation buildInvocation(const AnalyzeOptions& options, const std::string& inputName) {
    CompilerInvocation invocation;
    invocation.Action = FrontendActionKind::SyntaxOnly;

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

void reportIssue(const std::string& file,
                 unsigned line,
                 unsigned col,
                 const std::string& check,
                 const std::string& message,
                 unsigned& count) {
    std::cout << file << ":" << line << ":" << col
              << ": warning[" << check << "]: " << message << "\n";
    ++count;
}

void runStyleChecks(const AnalyzeOptions& options,
                    const std::string& file,
                    CompilerInstance& ci,
                    const FrontendUnit& unit,
                    unsigned& issueCount) {
    SourceManager& sm = ci.getSourceManager();
    constexpr size_t kMaxParams = 6;
    constexpr unsigned kMaxLines = 80;

    for (Decl* decl : unit.Declarations) {
        if (!decl || decl->getKind() != ASTNode::Kind::FuncDecl) {
            continue;
        }

        auto* fn = static_cast<FuncDecl*>(decl);
        auto [line, col] = sm.getLineAndColumn(fn->getRange().getBegin());

        if (options.Checks.count(kCheckTooManyParams) && fn->getParams().size() > kMaxParams) {
            reportIssue(file,
                        line,
                        col,
                        kCheckTooManyParams,
                        "function '" + fn->getName() + "' has " +
                            std::to_string(fn->getParams().size()) +
                            " parameters (max " + std::to_string(kMaxParams) + ")",
                        issueCount);
        }

        if (options.Checks.count(kCheckLongFunction)) {
            auto [beginLine, beginCol] = sm.getLineAndColumn(fn->getRange().getBegin());
            auto [endLine, endCol] = sm.getLineAndColumn(fn->getRange().getEnd());
            (void)beginCol;
            (void)endCol;
            unsigned lines = (endLine >= beginLine) ? (endLine - beginLine + 1) : 0;
            if (lines > kMaxLines) {
                reportIssue(file,
                            line,
                            col,
                            kCheckLongFunction,
                            "function '" + fn->getName() + "' has " +
                                std::to_string(lines) + " lines (max " +
                                std::to_string(kMaxLines) + ")",
                            issueCount);
            }
        }
    }
}

} // namespace

int main(int argc, char* argv[]) {
    AnalyzeOptions options;
    if (!parseArgs(argc, argv, options)) {
        return 1;
    }

    if (options.ShowHelp) {
        printHelp(argv[0]);
        return 0;
    }
    if (options.ListChecks) {
        printChecks();
        return 0;
    }

    unsigned issueCount = 0;

    for (const auto& file : options.InputFiles) {
        CompilerInvocation invocation = buildInvocation(options, file);
        CompilerInstance ci(invocation);
        ci.enableStoredDiagnostics();

        SyntaxOnlyAction action;
        FrontendResult result = executeFrontendAction(
            ci, action, {FrontendInputFile::fromFile(file)});

        auto* stored = dynamic_cast<StoredDiagnosticConsumer*>(ci.getDiagnostics().getConsumer());
        if (stored) {
            for (const Diagnostic& d : stored->getDiagnostics()) {
                auto [line, col] = ci.getSourceManager().getLineAndColumn(d.getLocation());
                std::cout << file << ":" << line << ":" << col << ": "
                          << d.getCode() << ": " << d.getMessage() << "\n";
                if (d.getLevel() == DiagnosticLevel::Error ||
                    d.getLevel() == DiagnosticLevel::Warning) {
                    ++issueCount;
                }
            }
        }

        if (result.succeeded() && !ci.getUnits().empty()) {
            runStyleChecks(options, file, ci, ci.getUnits().front(), issueCount);
        }
    }

    return issueCount == 0 ? 0 : 1;
}
