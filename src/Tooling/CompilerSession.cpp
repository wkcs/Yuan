#include "yuan/Tooling/CompilerSession.h"
#include <sstream>

namespace yuan {

namespace {

void hashCombine(size_t& seed, size_t value) {
    seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
}

void hashStringVector(size_t& seed, const std::vector<std::string>& values) {
    for (const auto& value : values) {
        hashCombine(seed, std::hash<std::string>{}(value));
    }
}

} // namespace

size_t CompilerSession::computeInvocationHash(const CompilerInvocation& invocation) {
    size_t seed = std::hash<int>{}(static_cast<int>(invocation.Action));
    hashCombine(seed, std::hash<unsigned>{}(invocation.OptimizationLevel));
    hashCombine(seed, std::hash<bool>{}(invocation.Verbose));
    hashCombine(seed, std::hash<std::string>{}(invocation.StdLibPath));
    hashCombine(seed, std::hash<std::string>{}(invocation.ModuleCacheDir));
    hashCombine(seed, std::hash<std::string>{}(invocation.OutputFile));
    hashStringVector(seed, invocation.IncludePaths);
    hashStringVector(seed, invocation.PackagePaths);
    hashStringVector(seed, invocation.LibraryPaths);
    hashStringVector(seed, invocation.Libraries);
    return seed;
}

std::string CompilerSession::makeKey(const std::string& inputName,
                                     unsigned version,
                                     size_t invocationHash) {
    std::ostringstream oss;
    oss << inputName << "#" << version << "#" << invocationHash;
    return oss.str();
}

std::shared_ptr<SessionSnapshot> CompilerSession::getOrCreateSnapshot(
    const std::string& inputName,
    const std::string& content,
    unsigned version,
    const CompilerInvocation& invocation) {
    size_t invocationHash = computeInvocationHash(invocation);
    std::string key = makeKey(inputName, version, invocationHash);
    auto it = Snapshots.find(key);
    if (it != Snapshots.end()) {
        return it->second;
    }

    auto snapshot = std::make_shared<SessionSnapshot>();
    snapshot->InputName = inputName;
    snapshot->Version = version;
    snapshot->InvocationHash = invocationHash;

    CompilerInvocation invocationForSnapshot = invocation;
    invocationForSnapshot.Action = FrontendActionKind::SyntaxOnly;

    snapshot->Instance = std::make_unique<CompilerInstance>(invocationForSnapshot);
    snapshot->Instance->enableStoredDiagnostics();

    SyntaxOnlyAction action;
    std::vector<FrontendInputFile> inputs = {
        FrontendInputFile::fromBuffer(inputName, content)
    };
    snapshot->Result = executeFrontendAction(*snapshot->Instance, action, inputs);

    Snapshots[key] = snapshot;
    return snapshot;
}

void CompilerSession::invalidate(const std::string& inputName) {
    for (auto it = Snapshots.begin(); it != Snapshots.end();) {
        if (it->second && it->second->InputName == inputName) {
            it = Snapshots.erase(it);
        } else {
            ++it;
        }
    }
}

void CompilerSession::clear() {
    Snapshots.clear();
}

} // namespace yuan
