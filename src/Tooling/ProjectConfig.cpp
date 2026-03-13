#include "yuan/Tooling/ProjectConfig.h"
#include "yuan/Tooling/YamlLite.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace yuan {

namespace {

unsigned parseOptLevel(const std::string& text) {
    if (text == "O0") return 0;
    if (text == "O1") return 1;
    if (text == "O2") return 2;
    if (text == "O3") return 3;
    return 0;
}

bool parseUnsigned(const std::string& text, unsigned& value) {
    if (text.empty()) {
        return false;
    }
    unsigned result = 0;
    for (char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
        result = result * 10 + static_cast<unsigned>(c - '0');
    }
    value = result;
    return true;
}

void readStringArray(const yaml_lite::Value* node, std::vector<std::string>& out) {
    if (!node || !node->isSeq()) {
        return;
    }
    out.clear();
    for (const auto& entry : node->seq) {
        if (entry.isScalar()) {
            out.push_back(entry.scalar);
        }
    }
}

} // namespace

std::string ProjectConfigLoader::discover(const std::string& startPath) {
    std::filesystem::path base = startPath.empty()
        ? std::filesystem::current_path()
        : std::filesystem::path(startPath);

    if (std::filesystem::is_regular_file(base)) {
        base = base.parent_path();
    }

    std::error_code ec;
    std::filesystem::path current = std::filesystem::absolute(base, ec);
    if (ec) {
        current = base;
    }

    while (!current.empty()) {
        std::filesystem::path candidate = current / "project.yaml";
        if (std::filesystem::exists(candidate)) {
            return candidate.string();
        }
        std::filesystem::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }

    return "";
}

bool ProjectConfigLoader::loadFromFile(const std::string& path,
                                       ProjectConfig& outConfig,
                                       std::string& outError) {
    std::ifstream in(path);
    if (!in.good()) {
        outError = "无法读取项目配置文件: " + path;
        return false;
    }

    std::string text;
    {
        std::ostringstream oss;
        oss << in.rdbuf();
        text = oss.str();
    }

    yaml_lite::Value root;
    yaml_lite::ParseError parseError;
    try {
        if (!yaml_lite::parse(text, root, parseError)) {
            outError = "项目配置 YAML 解析失败: 第 " + std::to_string(parseError.line) +
                       " 行: " + parseError.message;
            return false;
        }
    } catch (const std::exception& ex) {
        outError = "项目配置 YAML 解析失败: " + std::string(ex.what());
        return false;
    }

    if (const auto* version = yaml_lite::lookup(root, "version")) {
        if (version->isScalar()) {
            unsigned parsed = 0;
            if (parseUnsigned(version->scalar, parsed)) {
                outConfig.Version = parsed;
            }
        }
    }

    const auto* compile = yaml_lite::lookup(root, "compile");
    if (!compile || !compile->isMap()) {
        return true;
    }

    if (const auto* stdlib = yaml_lite::lookup(*compile, "stdlib"); stdlib && stdlib->isScalar()) {
        outConfig.Compile.HasStdLibPath = true;
        outConfig.Compile.StdLibPath = stdlib->scalar;
    }
    if (const auto* moduleCache = yaml_lite::lookup(*compile, "moduleCache");
        moduleCache && moduleCache->isScalar()) {
        outConfig.Compile.HasModuleCacheDir = true;
        outConfig.Compile.ModuleCacheDir = moduleCache->scalar;
    }
    if (const auto* optLevel = yaml_lite::lookup(*compile, "optLevel");
        optLevel && optLevel->isScalar()) {
        outConfig.Compile.HasOptLevel = true;
        outConfig.Compile.OptimizationLevel = parseOptLevel(optLevel->scalar);
    }

    readStringArray(yaml_lite::lookup(*compile, "includePaths"), outConfig.Compile.IncludePaths);
    readStringArray(yaml_lite::lookup(*compile, "packagePaths"), outConfig.Compile.PackagePaths);
    readStringArray(yaml_lite::lookup(*compile, "libraryPaths"), outConfig.Compile.LibraryPaths);
    readStringArray(yaml_lite::lookup(*compile, "libraries"), outConfig.Compile.Libraries);
    return true;
}

void applyProjectConfig(ProjectConfig const& config,
                        CompilerInvocation& invocation,
                        bool keepInvocationOverrides) {
    if (config.Compile.HasStdLibPath &&
        (!keepInvocationOverrides || invocation.StdLibPath.empty())) {
        invocation.StdLibPath = config.Compile.StdLibPath;
    }
    if (config.Compile.HasModuleCacheDir &&
        (!keepInvocationOverrides || invocation.ModuleCacheDir.empty() ||
         invocation.ModuleCacheDir == ".yuan/cache")) {
        invocation.ModuleCacheDir = config.Compile.ModuleCacheDir;
    }
    if (config.Compile.HasOptLevel &&
        (!keepInvocationOverrides || invocation.OptimizationLevel == 0)) {
        invocation.OptimizationLevel = config.Compile.OptimizationLevel;
    }

    if (!keepInvocationOverrides || invocation.IncludePaths.empty()) {
        invocation.IncludePaths = config.Compile.IncludePaths;
    }
    if (!keepInvocationOverrides || invocation.PackagePaths.empty()) {
        invocation.PackagePaths = config.Compile.PackagePaths;
    }
    if (!keepInvocationOverrides || invocation.LibraryPaths.empty()) {
        invocation.LibraryPaths = config.Compile.LibraryPaths;
    }
    if (!keepInvocationOverrides || invocation.Libraries.empty()) {
        invocation.Libraries = config.Compile.Libraries;
    }
}

} // namespace yuan
