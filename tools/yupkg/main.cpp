#include "yuan/Driver/Driver.h"
#include "yuan/Driver/Options.h"
#include "yuan/Frontend/CompilerInstance.h"
#include "yuan/Frontend/FrontendAction.h"
#include "yuan/Sema/ModuleManager.h"
#include "yuan/Tooling/ProjectConfig.h"
#include "yuan/Tooling/YamlLite.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace yuan;
namespace fs = std::filesystem;

namespace {

struct DepSpec {
    enum class Kind { Path, Git };
    Kind kind = Kind::Path;
    std::string name;
    std::string path;
    std::string git;
    std::string rev;
    std::string tag;
    std::string branch;
};

struct PackageManifest {
    std::string name;
    std::string version;
    fs::path manifestPath;
    std::vector<DepSpec> deps;
};

struct BuildContext {
    fs::path workspaceRoot;
    fs::path yuanDir;
    fs::path cacheDir;
    fs::path pkgDir;
    fs::path depsDir;
    fs::path targetDir;
    fs::path lockPath;
};

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

std::string yamlQuote(const std::string& value) {
    std::string out = "\"";
    out.reserve(value.size() + 2);
    for (char c : value) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    out.push_back('"');
    return out;
}

int runCommand(const std::vector<std::string>& args) {
    if (args.empty()) {
        return 1;
    }
    std::ostringstream cmd;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i) cmd << " ";
        cmd << quoteArg(args[i]);
    }
    return std::system(cmd.str().c_str());
}

std::string searchUpForStdLib(fs::path start) {
    std::error_code ec;
    fs::path current = fs::absolute(start, ec);
    if (ec) {
        current = start;
    }
    if (fs::is_regular_file(current)) {
        current = current.parent_path();
    }
    while (!current.empty()) {
        fs::path candidate = current / "stdlib";
        if (fs::exists(candidate) && fs::is_directory(candidate)) {
            return candidate.string();
        }
        fs::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return "";
}

std::string discoverStdLibPath(const char* argv0) {
    if (const char* env = std::getenv("YUAN_STDLIB")) {
        if (env[0] != '\0' && fs::exists(env) && fs::is_directory(env)) {
            return std::string(env);
        }
    }

    std::string fromCwd = searchUpForStdLib(fs::current_path());
    if (!fromCwd.empty()) {
        return fromCwd;
    }

    if (argv0 && argv0[0] != '\0') {
        std::error_code ec;
        fs::path exe = fs::absolute(argv0, ec);
        if (!ec) {
            std::string fromExe = searchUpForStdLib(exe);
            if (!fromExe.empty()) {
                return fromExe;
            }
        }
    }

    return "";
}

bool writeFileIfMissing(const fs::path& path, const std::string& content) {
    if (fs::exists(path)) {
        return true;
    }
    std::ofstream out(path);
    if (!out) {
        return false;
    }
    out << content;
    return true;
}

fs::path discoverProjectRoot(const fs::path& start) {
    fs::path current = fs::absolute(start);
    if (fs::is_regular_file(current)) {
        current = current.parent_path();
    }
    while (!current.empty()) {
        fs::path candidate = current / "project.yaml";
        if (fs::exists(candidate)) {
            return current;
        }
        fs::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return {};
}

bool isPathWithin(const fs::path& base, const fs::path& target) {
    fs::path rel = fs::weakly_canonical(target).lexically_relative(fs::weakly_canonical(base));
    if (rel.empty()) {
        return false;
    }
    for (const auto& part : rel) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

bool readTextFile(const fs::path& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream oss;
    oss << in.rdbuf();
    out = oss.str();
    return true;
}

bool writeTextFile(const fs::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << content;
    return true;
}

std::optional<std::string> readGitHeadRef(const fs::path& gitDir) {
    std::string head;
    if (!readTextFile(gitDir / "HEAD", head)) {
        return std::nullopt;
    }
    if (head.rfind("ref:", 0) == 0) {
        std::string ref = head.substr(4);
        while (!ref.empty() && (ref[0] == ' ' || ref[0] == '\t')) {
            ref.erase(ref.begin());
        }
        while (!ref.empty() && (ref.back() == '\n' || ref.back() == '\r')) {
            ref.pop_back();
        }
        return ref;
    }
    // Detached head
    while (!head.empty() && (head.back() == '\n' || head.back() == '\r')) {
        head.pop_back();
    }
    if (head.empty()) {
        return std::nullopt;
    }
    return head;
}

std::optional<std::string> readPackedRef(const fs::path& gitDir, const std::string& ref) {
    std::string packed;
    if (!readTextFile(gitDir / "packed-refs", packed)) {
        return std::nullopt;
    }
    std::istringstream iss(packed);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty() || line[0] == '#' || line[0] == '^') {
            continue;
        }
        std::istringstream ls(line);
        std::string hash, name;
        if (!(ls >> hash >> name)) {
            continue;
        }
        if (name == ref) {
            return hash;
        }
    }
    return std::nullopt;
}

std::optional<std::string> getGitHeadCommit(const fs::path& repoPath) {
    fs::path gitDir = repoPath / ".git";
    if (!fs::exists(gitDir)) {
        return std::nullopt;
    }
    auto headRef = readGitHeadRef(gitDir);
    if (!headRef.has_value()) {
        return std::nullopt;
    }
    const std::string& ref = *headRef;
    if (ref.rfind("refs/", 0) != 0) {
        return ref;
    }
    std::string refContent;
    if (readTextFile(gitDir / ref, refContent)) {
        while (!refContent.empty() && (refContent.back() == '\n' || refContent.back() == '\r')) {
            refContent.pop_back();
        }
        if (!refContent.empty()) {
            return refContent;
        }
    }
    return readPackedRef(gitDir, ref);
}

bool readManifest(const fs::path& manifestPath, PackageManifest& out, std::string& errorMsg) {
    std::string text;
    if (!readTextFile(manifestPath, text)) {
        errorMsg = "无法读取项目配置文件: " + manifestPath.string();
        return false;
    }

    yaml_lite::Value root;
    yaml_lite::ParseError parseError;
    try {
        if (!yaml_lite::parse(text, root, parseError)) {
            errorMsg = "项目配置 YAML 解析失败: 第 " + std::to_string(parseError.line) +
                       " 行: " + parseError.message;
            return false;
        }
    } catch (const std::exception& ex) {
        errorMsg = std::string("项目配置 YAML 解析失败: ") + ex.what();
        return false;
    }

    const auto* pkg = yaml_lite::lookup(root, "package");
    if (!pkg || !pkg->isMap()) {
        errorMsg = "项目配置缺少 package 字段";
        return false;
    }
    const auto* name = yaml_lite::lookup(*pkg, "name");
    if (!name || !name->isScalar()) {
        errorMsg = "项目配置缺少 package.name";
        return false;
    }
    const auto* version = yaml_lite::lookup(*pkg, "version");
    if (!version || !version->isScalar()) {
        errorMsg = "项目配置缺少 package.version";
        return false;
    }

    out.name = name->scalar;
    out.version = version->scalar;
    out.manifestPath = manifestPath;
    out.deps.clear();

    if (const auto* deps = yaml_lite::lookup(root, "dependencies")) {
        if (deps->isScalar() && deps->scalar.empty()) {
            return true;
        }
        if (!deps->isMap()) {
            errorMsg = "dependencies 必须是对象";
            return false;
        }
        for (const auto& entry : deps->map) {
            if (!entry.second.isMap()) {
                errorMsg = "dependencies 项必须是对象: " + entry.first;
                return false;
            }
            DepSpec dep;
            dep.name = entry.first;
            const auto& node = entry.second;

            const auto* pathNode = yaml_lite::lookup(node, "path");
            const auto* gitNode = yaml_lite::lookup(node, "git");
            bool hasPath = pathNode && pathNode->isScalar();
            bool hasGit = gitNode && gitNode->isScalar();
            if (hasPath == hasGit) {
                errorMsg = "依赖必须且只能包含 path 或 git: " + dep.name;
                return false;
            }
            if (hasPath) {
                dep.kind = DepSpec::Kind::Path;
                dep.path = pathNode->scalar;
            } else {
                dep.kind = DepSpec::Kind::Git;
                dep.git = gitNode->scalar;
                if (const auto* rev = yaml_lite::lookup(node, "rev"); rev && rev->isScalar()) {
                    dep.rev = rev->scalar;
                }
                if (const auto* tag = yaml_lite::lookup(node, "tag"); tag && tag->isScalar()) {
                    dep.tag = tag->scalar;
                }
                if (const auto* branch = yaml_lite::lookup(node, "branch");
                    branch && branch->isScalar()) {
                    dep.branch = branch->scalar;
                }
            }
            out.deps.push_back(std::move(dep));
        }
    }

    return true;
}

bool loadProjectConfig(const fs::path& manifestPath, ProjectConfig& outConfig, std::string& errorMsg) {
    return ProjectConfigLoader::loadFromFile(manifestPath.string(), outConfig, errorMsg);
}

void prependVector(std::vector<std::string>& target, const std::vector<std::string>& prefix) {
    if (prefix.empty()) {
        return;
    }
    std::vector<std::string> merged;
    merged.reserve(prefix.size() + target.size());
    merged.insert(merged.end(), prefix.begin(), prefix.end());
    merged.insert(merged.end(), target.begin(), target.end());
    target.swap(merged);
}

void applyProjectConfigToDriver(const ProjectConfig& config,
                                DriverOptions& options,
                                bool& hasOpt) {
    if (config.Compile.HasStdLibPath && options.StdLibPath.empty()) {
        options.StdLibPath = config.Compile.StdLibPath;
    }
    if (config.Compile.HasModuleCacheDir && options.ModuleCacheDir.empty()) {
        options.ModuleCacheDir = config.Compile.ModuleCacheDir;
    }
    if (config.Compile.HasOptLevel) {
        hasOpt = true;
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
}

std::string moduleNameToRelPath(const std::string& moduleName) {
    std::string rel = moduleName;
    std::replace(rel.begin(), rel.end(), '.', '/');
    return rel;
}

bool rewriteInterfaceObjectPath(const fs::path& src,
                                const fs::path& dst,
                                const std::string& newObjectPath) {
    std::ifstream in(src, std::ios::binary);
    if (!in) {
        return false;
    }
    std::ostringstream out;
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("object\t", 0) == 0) {
            out << "object\t" << newObjectPath << "\n";
        } else {
            out << line << "\n";
        }
    }
    return writeTextFile(dst, out.str());
}

bool isPkgModuleName(const std::string& name, const std::string& pkgName) {
    if (name == pkgName) return true;
    if (name.rfind(pkgName + ".", 0) == 0) return true;
    if (name.rfind(pkgName + "/", 0) == 0) return true;
    return false;
}

struct PackageNode {
    PackageManifest manifest;
    fs::path root;
};

bool loadLockFile(const fs::path& lockPath, nlohmann::json& lock) {
    if (!fs::exists(lockPath)) {
        lock = nlohmann::json::object();
        return true;
    }
    std::ifstream in(lockPath);
    if (!in) return false;
    try {
        in >> lock;
    } catch (...) {
        return false;
    }
    if (!lock.is_object()) {
        lock = nlohmann::json::object();
    }
    if (!lock.contains("dependencies") || !lock["dependencies"].is_object()) {
        lock["dependencies"] = nlohmann::json::object();
    }
    return true;
}

std::optional<std::string> getLockedCommit(const nlohmann::json& lock,
                                           const std::string& name) {
    if (!lock.contains("dependencies")) {
        return std::nullopt;
    }
    const auto& deps = lock["dependencies"];
    if (!deps.contains(name)) {
        return std::nullopt;
    }
    const auto& node = deps[name];
    if (!node.contains("resolved") || !node["resolved"].is_object()) {
        return std::nullopt;
    }
    const auto& res = node["resolved"];
    if (res.contains("commit") && res["commit"].is_string()) {
        return res["commit"].get<std::string>();
    }
    return std::nullopt;
}

void setLockedGit(const std::string& name,
                  const DepSpec& dep,
                  const std::string& commit,
                  nlohmann::json& lock) {
    auto& deps = lock["dependencies"];
    auto& node = deps[name];
    node["source"] = nlohmann::json::object();
    node["source"]["git"] = dep.git;
    if (!dep.rev.empty()) node["source"]["rev"] = dep.rev;
    if (!dep.tag.empty()) node["source"]["tag"] = dep.tag;
    if (!dep.branch.empty()) node["source"]["branch"] = dep.branch;
    node["resolved"] = nlohmann::json::object();
    node["resolved"]["commit"] = commit;
}

void setLockedPath(const std::string& name,
                   const DepSpec& dep,
                   nlohmann::json& lock) {
    auto& deps = lock["dependencies"];
    auto& node = deps[name];
    node["source"] = nlohmann::json::object();
    node["source"]["path"] = dep.path;
    node["resolved"] = nlohmann::json::object();
}

bool writeLockFile(const fs::path& lockPath, const nlohmann::json& lock) {
    std::ofstream out(lockPath);
    if (!out) return false;
    out << lock.dump(2) << "\n";
    return true;
}

bool ensureGitDependency(const DepSpec& dep,
                         const fs::path& dest,
                         const std::optional<std::string>& lockedCommit,
                         std::string& outCommit) {
    if (!fs::exists(dest)) {
        if (runCommand({"git", "clone", dep.git, dest.string()}) != 0) {
            return false;
        }
    }

    if (runCommand({"git", "-C", dest.string(), "fetch", "--all", "--tags"}) != 0) {
        return false;
    }

    std::string target;
    if (lockedCommit.has_value()) {
        target = *lockedCommit;
    } else if (!dep.rev.empty()) {
        target = dep.rev;
    } else if (!dep.tag.empty()) {
        target = dep.tag;
    } else if (!dep.branch.empty()) {
        target = dep.branch;
    }

    if (!target.empty()) {
        if (runCommand({"git", "-C", dest.string(), "checkout", target}) != 0) {
            return false;
        }
    }

    auto commit = getGitHeadCommit(dest);
    if (!commit.has_value()) {
        return false;
    }
    outCommit = *commit;
    return true;
}

bool buildPackageLib(const PackageNode& node,
                     const BuildContext& ctx,
                     const std::vector<std::string>& depPkgPaths,
                     const std::string& stdlibPathOverride) {
    const fs::path pkgRoot = node.root;
    const std::string& pkgName = node.manifest.name;
    fs::path libPath = pkgRoot / "src" / "lib.yu";
    if (!fs::exists(libPath)) {
        std::cerr << "错误：包缺少 src/lib.yu: " << pkgName << "\n";
        return false;
    }

    ProjectConfig config;
    std::string configError;
    (void)loadProjectConfig(node.manifest.manifestPath, config, configError);

    CompilerInvocation invocation;
    invocation.Action = FrontendActionKind::SyntaxOnly;
    applyProjectConfig(config, invocation, true);
    if (!stdlibPathOverride.empty()) {
        invocation.StdLibPath = stdlibPathOverride;
    }
    invocation.ModuleCacheDir = ctx.cacheDir.string();
    invocation.PackagePaths.insert(invocation.PackagePaths.end(),
                                   depPkgPaths.begin(),
                                   depPkgPaths.end());
    invocation.PackageSourceRoots.push_back({pkgName, pkgRoot.string()});

    std::string stub = "const _ = @import(\"" + pkgName + "\")\n";
    CompilerInstance ci(invocation);
    ci.enableTextDiagnostics(std::cerr, true);
    SyntaxOnlyAction action;
    FrontendResult result = executeFrontendAction(
        ci, action, {FrontendInputFile::fromBuffer("yupkg-stub.yu", stub)});
    if (!result.succeeded()) {
        return false;
    }

    std::unordered_map<std::string, ModuleInfo*> pkgModules;
    bool hasBadModule = false;
    std::string badModuleName;

    for (const auto& unit : ci.getUnits()) {
        if (!unit.Semantic) {
            continue;
        }
        ModuleManager& mm = unit.Semantic->getModuleManager();
        for (const auto& entry : mm.getLoadedModules()) {
            ModuleInfo* info = entry.second.get();
            if (!info || info->FilePath.empty()) {
                continue;
            }
            if (!isPathWithin(pkgRoot, info->FilePath)) {
                continue;
            }
            if (!isPkgModuleName(info->Name, pkgName)) {
                hasBadModule = true;
                badModuleName = info->Name;
                continue;
            }
            pkgModules.emplace(info->Name, info);
        }
    }

    if (hasBadModule) {
        std::cerr << "错误：包内存在未使用包名前缀的导入模块: " << badModuleName
                  << "，请使用 @import(\"" << pkgName << ".xxx\")\n";
        return false;
    }

    fs::path outModules = ctx.pkgDir / pkgName / "modules";
    fs::path outObjects = ctx.pkgDir / pkgName / "objects";
    fs::create_directories(outModules);
    fs::create_directories(outObjects);

    CompilerInvocation objInvocation = invocation;
    objInvocation.Action = FrontendActionKind::EmitObj;

    for (const auto& entry : pkgModules) {
        ModuleInfo* info = entry.second;
        if (!info || info->FilePath.empty()) {
            continue;
        }
        if (info->ObjectPath.empty() || info->InterfacePath.empty()) {
            std::cerr << "错误：模块缺少缓存路径: " << info->Name << "\n";
            return false;
        }

        CompilerInstance objCi(objInvocation);
        objCi.enableTextDiagnostics(std::cerr, true);
        EmitObjAction objAction;
        FrontendResult objResult = executeFrontendAction(
            objCi, objAction, {FrontendInputFile::fromFile(info->FilePath, info->ObjectPath)});
        if (!objResult.succeeded()) {
            return false;
        }

        std::string rel = moduleNameToRelPath(info->Name);
        fs::path dstIfc = outModules / (rel + ".ymi");
        fs::path dstObj = outObjects / (rel + ".o");
        fs::create_directories(dstIfc.parent_path());
        fs::create_directories(dstObj.parent_path());

        if (!fs::exists(info->ObjectPath)) {
            std::cerr << "错误：对象文件未生成: " << info->ObjectPath << "\n";
            return false;
        }
        std::error_code ec;
        fs::copy_file(info->ObjectPath, dstObj, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            std::cerr << "错误：复制对象文件失败: " << dstObj.string() << "\n";
            return false;
        }

        if (!fs::exists(info->InterfacePath)) {
            std::cerr << "错误：接口文件未生成: " << info->InterfacePath << "\n";
            return false;
        }
        if (!rewriteInterfaceObjectPath(info->InterfacePath, dstIfc, dstObj.string())) {
            std::cerr << "错误：写入接口文件失败: " << dstIfc.string() << "\n";
            return false;
        }
    }

    return true;
}

bool buildBinary(const PackageNode& node,
                 const BuildContext& ctx,
                 const std::vector<std::string>& depPkgPaths,
                 bool release,
                 const fs::path& inputFile,
                 const fs::path& outputFile,
                 const std::string& stdlibPathOverride) {
    DriverOptions options;
    options.InputFiles = {inputFile.string()};
    options.OutputFile = outputFile.string();

    ProjectConfig config;
    std::string configError;
    bool hasOpt = false;
    if (loadProjectConfig(node.manifest.manifestPath, config, configError)) {
        applyProjectConfigToDriver(config, options, hasOpt);
    }
    if (!stdlibPathOverride.empty()) {
        options.StdLibPath = stdlibPathOverride;
    }

    if (release && !hasOpt) {
        options.Optimization = OptLevel::O3;
    }
    if (!release && !hasOpt) {
        options.Optimization = OptLevel::O0;
    }

    options.ModuleCacheDir = ctx.cacheDir.string();
    options.PackagePaths.insert(options.PackagePaths.end(),
                                depPkgPaths.begin(),
                                depPkgPaths.end());
    options.PackageSourceRoots.push_back({node.manifest.name, node.root.string()});

    Driver driver(options);
    CompilationResult result = driver.run();
    return result == CompilationResult::Success;
}

bool runExecutable(const fs::path& exePath, const std::vector<std::string>& args) {
    std::vector<std::string> cmd;
    cmd.push_back(exePath.string());
    for (const auto& a : args) cmd.push_back(a);
    return runCommand(cmd) == 0;
}

void printHelp() {
    std::cout << "yupkg <init|build|run|test|clean> [options]\n";
    std::cout << "  init            initialize a new package\n";
    std::cout << "  build           build package and dependencies\n";
    std::cout << "  run             build and run the package binary\n";
    std::cout << "  test            build and run tests under tests/*.yu\n";
    std::cout << "  clean           remove build artifacts\n";
    std::cout << "Options:\n";
    std::cout << "  --release       build with release optimization\n";
    std::cout << "  --stdlib <path> override stdlib root\n";
    std::cout << "  --all           (clean) also remove .yuan/deps\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 1;
    }

    std::string command = argv[1];
    bool release = false;
    bool cleanAll = false;
    std::string stdlibOverride;
    std::vector<std::string> runArgs;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--release") {
            release = true;
            continue;
        }
        if (arg == "--stdlib") {
            if (i + 1 >= argc) {
                std::cerr << "错误：--stdlib 需要参数\n";
                return 1;
            }
            stdlibOverride = argv[++i];
            continue;
        }
        if (arg == "--all") {
            cleanAll = true;
            continue;
        }
        if (arg == "--") {
            for (++i; i < argc; ++i) {
                runArgs.push_back(argv[i]);
            }
            break;
        }
        if (arg == "-h" || arg == "--help") {
            printHelp();
            return 0;
        }
    }

    fs::path root = discoverProjectRoot(fs::current_path());
    std::string discoveredStdlib = stdlibOverride.empty()
        ? discoverStdLibPath(argv[0])
        : stdlibOverride;

    if (command == "init") {
        fs::path initRoot = fs::current_path();
        fs::path manifestPath = initRoot / "project.yaml";
        fs::create_directories(initRoot / "src");
        fs::create_directories(initRoot / "tests");

        std::string pkgName = initRoot.filename().string();
        std::ostringstream manifest;
        manifest << "version: 1\n";
        manifest << "package:\n";
        manifest << "  name: " << yamlQuote(pkgName) << "\n";
        manifest << "  version: \"0.1.0\"\n";
        manifest << "dependencies: {}\n";
        if (!discoveredStdlib.empty()) {
            manifest << "compile:\n";
            manifest << "  stdlib: " << yamlQuote(discoveredStdlib) << "\n";
        }

        if (!fs::exists(manifestPath)) {
            std::ofstream out(manifestPath);
            if (!out) {
                std::cerr << "错误：无法创建 " << manifestPath.string() << "\n";
                return 1;
            }
            out << manifest.str();
        }

        if (!writeFileIfMissing(initRoot / "src" / "main.yu",
                                "const std = @import(\"std\")\n\nfunc main() {\n    std.io.print(\"Hello Yuan\")\n}\n")) {
            std::cerr << "错误：无法创建 src/main.yu\n";
            return 1;
        }
        if (!writeFileIfMissing(initRoot / "src" / "lib.yu",
                                "pub func hello() {\n}\n")) {
            std::cerr << "错误：无法创建 src/lib.yu\n";
            return 1;
        }
        std::cout << "yupkg init 完成\n";
        return 0;
    }

    if (root.empty()) {
        std::cerr << "错误：未找到 project.yaml\n";
        return 1;
    }

    BuildContext ctx;
    ctx.workspaceRoot = root;
    ctx.yuanDir = root / ".yuan";
    ctx.cacheDir = ctx.yuanDir / "cache";
    ctx.pkgDir = ctx.yuanDir / "pkg";
    ctx.depsDir = ctx.yuanDir / "deps";
    ctx.targetDir = ctx.yuanDir / "target" / (release ? "release" : "debug");
    ctx.lockPath = root / "yuan.lock.json";

    if (command == "clean") {
        fs::remove_all(ctx.targetDir);
        fs::remove_all(ctx.pkgDir);
        fs::remove_all(ctx.cacheDir);
        if (cleanAll) {
            fs::remove_all(ctx.depsDir);
        }
        std::cout << "yupkg clean 完成\n";
        return 0;
    }

    PackageManifest rootManifest;
    std::string errorMsg;
    if (!readManifest(root / "project.yaml", rootManifest, errorMsg)) {
        std::cerr << "错误：" << errorMsg << "\n";
        return 1;
    }
    if (discoveredStdlib.empty()) {
        discoveredStdlib = searchUpForStdLib(root);
    }

    nlohmann::json lock;
    if (!loadLockFile(ctx.lockPath, lock)) {
        std::cerr << "错误：无法读取 lock 文件\n";
        return 1;
    }

    std::unordered_map<std::string, PackageNode> nodes;
    std::unordered_set<std::string> visiting;
    std::vector<std::string> buildOrder;

    std::function<bool(const PackageNode&)> dfsBuild = [&](const PackageNode& node) -> bool {
        auto existing = nodes.find(node.manifest.name);
        if (existing != nodes.end()) {
            fs::path existingRoot = fs::weakly_canonical(existing->second.root);
            fs::path newRoot = fs::weakly_canonical(node.root);
            if (existingRoot != newRoot) {
                std::cerr << "错误：依赖包名冲突: " << node.manifest.name << "\n";
                return false;
            }
            if (!visiting.count(node.manifest.name)) {
                return true;
            }
        }
        if (visiting.count(node.manifest.name)) {
            std::cerr << "错误：检测到循环依赖: " << node.manifest.name << "\n";
            return false;
        }
        visiting.insert(node.manifest.name);
        nodes[node.manifest.name] = node;

        for (const auto& dep : node.manifest.deps) {
            fs::path depRoot;
            if (dep.kind == DepSpec::Kind::Path) {
                depRoot = fs::weakly_canonical(node.root / dep.path);
                setLockedPath(dep.name, dep, lock);
            } else {
                fs::create_directories(ctx.depsDir);
                depRoot = ctx.depsDir / dep.name;
                std::string commit;
                auto locked = getLockedCommit(lock, dep.name);
                if (!ensureGitDependency(dep, depRoot, locked, commit)) {
                    std::cerr << "错误：拉取 git 依赖失败: " << dep.name << "\n";
                    return false;
                }
                setLockedGit(dep.name, dep, commit, lock);
            }

            PackageManifest depManifest;
            std::string depErr;
            if (!readManifest(depRoot / "project.yaml", depManifest, depErr)) {
                std::cerr << "错误：读取依赖 manifest 失败: " << dep.name << " (" << depErr << ")\n";
                return false;
            }
            if (depManifest.name != dep.name) {
                std::cerr << "错误：依赖名与 manifest 不一致: " << dep.name << " != " << depManifest.name << "\n";
                return false;
            }
            PackageNode depNode{depManifest, depRoot};
            if (!dfsBuild(depNode)) {
                return false;
            }
        }

        visiting.erase(node.manifest.name);
        buildOrder.push_back(node.manifest.name);
        return true;
    };

    PackageNode rootNode{rootManifest, root};
    if (!dfsBuild(rootNode)) {
        return 1;
    }

    if (!writeLockFile(ctx.lockPath, lock)) {
        std::cerr << "错误：写入 lock 文件失败\n";
        return 1;
    }

    auto packagePathsFor = [&](const PackageNode& node) {
        std::vector<std::string> paths;
        for (const auto& dep : node.manifest.deps) {
            paths.push_back((ctx.pkgDir / dep.name).string());
        }
        return paths;
    };

    fs::create_directories(ctx.cacheDir);
    fs::create_directories(ctx.pkgDir);
    fs::create_directories(ctx.targetDir);

    for (const auto& name : buildOrder) {
        const auto& node = nodes.at(name);
        std::vector<std::string> depPkgPaths = packagePathsFor(node);
        if (!buildPackageLib(node, ctx, depPkgPaths, discoveredStdlib)) {
            std::cerr << "错误：构建包失败: " << node.manifest.name << "\n";
            return 1;
        }
    }

    if (command == "build" || command == "run" || command == "test") {
        fs::path mainFile = root / "src" / "main.yu";
        if (fs::exists(mainFile)) {
            fs::path outputFile = ctx.targetDir / rootManifest.name;
            std::vector<std::string> depPkgPaths = packagePathsFor(rootNode);
            if (!buildBinary(rootNode, ctx, depPkgPaths, release, mainFile, outputFile,
                             discoveredStdlib)) {
                std::cerr << "错误：构建主程序失败\n";
                return 1;
            }
        }
    }

    if (command == "run") {
        fs::path exePath = ctx.targetDir / rootManifest.name;
        if (!fs::exists(exePath)) {
            std::cerr << "错误：可执行文件不存在: " << exePath.string() << "\n";
            return 1;
        }
        if (!runExecutable(exePath, runArgs)) {
            return 1;
        }
        return 0;
    }

    if (command == "test") {
        fs::path testsDir = root / "tests";
        if (!fs::exists(testsDir)) {
            std::cout << "tests/ 目录不存在，跳过\n";
            return 0;
        }

        std::vector<fs::path> testFiles;
        for (const auto& entry : fs::directory_iterator(testsDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".yu") {
                testFiles.push_back(entry.path());
            }
        }

        if (testFiles.empty()) {
            std::cout << "未发现 tests/*.yu\n";
            return 0;
        }

        std::vector<std::string> depPkgPaths = packagePathsFor(rootNode);
        fs::path testsOutDir = ctx.targetDir / "tests";
        fs::create_directories(testsOutDir);

        for (const auto& testFile : testFiles) {
            std::string stem = testFile.stem().string();
            fs::path output = testsOutDir / stem;
            if (!buildBinary(rootNode, ctx, depPkgPaths, release, testFile, output,
                             discoveredStdlib)) {
                std::cerr << "错误：构建测试失败: " << testFile.string() << "\n";
                return 1;
            }
            if (!runExecutable(output, {})) {
                std::cerr << "错误：测试失败: " << testFile.string() << "\n";
                return 1;
            }
        }
        return 0;
    }

    if (command == "build") {
        return 0;
    }

    std::cerr << "错误：未知命令 " << command << "\n";
    printHelp();
    return 1;
}
