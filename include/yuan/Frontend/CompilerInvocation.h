#ifndef YUAN_FRONTEND_COMPILERINVOCATION_H
#define YUAN_FRONTEND_COMPILERINVOCATION_H

#include <string>
#include <vector>

namespace yuan {

enum class FrontendActionKind {
    SyntaxOnly,
    EmitLLVM,
    EmitObj,
    DumpTokens,
    ASTDump,
    ASTPrint
};

struct FrontendInputFile {
    std::string Name;
    std::string Buffer;
    std::string OutputPath;
    bool IsBuffer = false;

    static FrontendInputFile fromFile(const std::string& path,
                                      const std::string& outputPath = "") {
        FrontendInputFile input;
        input.Name = path;
        input.OutputPath = outputPath;
        input.IsBuffer = false;
        return input;
    }

    static FrontendInputFile fromBuffer(const std::string& name,
                                        const std::string& buffer,
                                        const std::string& outputPath = "") {
        FrontendInputFile input;
        input.Name = name;
        input.Buffer = buffer;
        input.OutputPath = outputPath;
        input.IsBuffer = true;
        return input;
    }
};

class CompilerInvocation {
public:
    FrontendActionKind Action = FrontendActionKind::SyntaxOnly;
    unsigned OptimizationLevel = 0;
    bool Verbose = false;

    std::string OutputFile;
    std::string StdLibPath;
    std::string ModuleCacheDir = ".yuan/cache";
    std::vector<std::string> IncludePaths;
    std::vector<std::string> PackagePaths;
    std::vector<std::string> LibraryPaths;
    std::vector<std::string> Libraries;

    const char* getActionString() const;
};

} // namespace yuan

#endif // YUAN_FRONTEND_COMPILERINVOCATION_H
