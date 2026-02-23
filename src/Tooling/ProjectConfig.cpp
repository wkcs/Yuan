#include "yuan/Tooling/ProjectConfig.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace yuan {

namespace {

unsigned parseOptLevel(const std::string& text) {
    if (text == "O0") return 0;
    if (text == "O1") return 1;
    if (text == "O2") return 2;
    if (text == "O3") return 3;
    return 0;
}

void readStringArray(const nlohmann::json& node, std::vector<std::string>& out) {
    if (!node.is_array()) {
        return;
    }
    out.clear();
    for (const auto& entry : node) {
        if (entry.is_string()) {
            out.push_back(entry.get<std::string>());
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
        std::filesystem::path candidate = current / "yuan-project.json";
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

    nlohmann::json root;
    try {
        in >> root;
    } catch (const std::exception& ex) {
        outError = "项目配置 JSON 解析失败: " + std::string(ex.what());
        return false;
    }

    if (root.contains("version") && root["version"].is_number_unsigned()) {
        outConfig.Version = root["version"].get<unsigned>();
    }

    if (!root.contains("compile") || !root["compile"].is_object()) {
        return true;
    }

    const auto& compile = root["compile"];
    if (compile.contains("stdlib") && compile["stdlib"].is_string()) {
        outConfig.Compile.HasStdLibPath = true;
        outConfig.Compile.StdLibPath = compile["stdlib"].get<std::string>();
    }
    if (compile.contains("moduleCache") && compile["moduleCache"].is_string()) {
        outConfig.Compile.HasModuleCacheDir = true;
        outConfig.Compile.ModuleCacheDir = compile["moduleCache"].get<std::string>();
    }
    if (compile.contains("optLevel") && compile["optLevel"].is_string()) {
        outConfig.Compile.HasOptLevel = true;
        outConfig.Compile.OptimizationLevel = parseOptLevel(compile["optLevel"].get<std::string>());
    }

    readStringArray(compile.value("includePaths", nlohmann::json::array()), outConfig.Compile.IncludePaths);
    readStringArray(compile.value("packagePaths", nlohmann::json::array()), outConfig.Compile.PackagePaths);
    readStringArray(compile.value("libraryPaths", nlohmann::json::array()), outConfig.Compile.LibraryPaths);
    readStringArray(compile.value("libraries", nlohmann::json::array()), outConfig.Compile.Libraries);
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
