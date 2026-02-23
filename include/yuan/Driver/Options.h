#ifndef YUAN_DRIVER_OPTIONS_H
#define YUAN_DRIVER_OPTIONS_H

#include <iosfwd>
#include <string>
#include <vector>

namespace yuan {

enum class DriverAction {
    Link,
    EmitObj,
    EmitLLVM,
    SyntaxOnly,
    DumpTokens,
    ASTDump,
    ASTPrint
};

enum class OptLevel {
    O0,
    O1,
    O2,
    O3
};

class DriverOptions {
public:
    std::vector<std::string> InputFiles;
    std::string OutputFile;

    DriverAction Action = DriverAction::Link;
    OptLevel Optimization = OptLevel::O0;

    bool ShowHelp = false;
    bool ShowVersion = false;
    bool Verbose = false;

    std::string ProjectFile;
    std::vector<std::string> IncludePaths;
    std::string ModuleCacheDir = ".yuan/cache";
    std::vector<std::string> PackagePaths;
    std::string StdLibPath;
    std::vector<std::string> LibraryPaths;
    std::vector<std::string> Libraries;
    bool LinkRuntimeNet = true;
    bool LinkRuntimeGUI = false;

    std::string getOutputFileName() const;
    const char* getOptLevelString() const;
    const char* getActionString() const;
    unsigned getOptimizationLevel() const;

    bool validate(std::string& errorMsg) const;

private:
    std::string deduceOutputFileName() const;
};

bool parseDriverOptions(int argc,
                        char* argv[],
                        DriverOptions& options,
                        std::string& errorMsg);

void printDriverHelp(const char* programName, std::ostream& os);
void printDriverVersion(std::ostream& os);

} // namespace yuan

#endif // YUAN_DRIVER_OPTIONS_H
