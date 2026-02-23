#ifndef YUAN_TOOLING_PROJECTCONFIG_H
#define YUAN_TOOLING_PROJECTCONFIG_H

#include "yuan/Frontend/CompilerInvocation.h"
#include <string>
#include <vector>

namespace yuan {

struct ProjectCompileConfig {
    bool HasStdLibPath = false;
    std::string StdLibPath;

    bool HasModuleCacheDir = false;
    std::string ModuleCacheDir;

    bool HasOptLevel = false;
    unsigned OptimizationLevel = 0;

    std::vector<std::string> IncludePaths;
    std::vector<std::string> PackagePaths;
    std::vector<std::string> LibraryPaths;
    std::vector<std::string> Libraries;
};

struct ProjectConfig {
    unsigned Version = 1;
    ProjectCompileConfig Compile;
};

class ProjectConfigLoader {
public:
    static std::string discover(const std::string& startPath);
    static bool loadFromFile(const std::string& path,
                             ProjectConfig& outConfig,
                             std::string& outError);
};

void applyProjectConfig(ProjectConfig const& config,
                        CompilerInvocation& invocation,
                        bool keepInvocationOverrides = true);

} // namespace yuan

#endif // YUAN_TOOLING_PROJECTCONFIG_H
