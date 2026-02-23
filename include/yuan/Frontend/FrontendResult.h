#ifndef YUAN_FRONTEND_FRONTENDRESULT_H
#define YUAN_FRONTEND_FRONTENDRESULT_H

#include <string>
#include <vector>

namespace yuan {

enum class FrontendStatus {
    Success,
    LexerError,
    ParserError,
    SemanticError,
    CodeGenError,
    IOError,
    InternalError
};

struct FrontendFileResult {
    std::string InputName;
    std::string OutputPath;
    FrontendStatus Status = FrontendStatus::Success;
    unsigned ErrorCount = 0;
    unsigned WarningCount = 0;
};

struct FrontendResult {
    FrontendStatus OverallStatus = FrontendStatus::Success;
    std::vector<FrontendFileResult> Files;

    bool succeeded() const { return OverallStatus == FrontendStatus::Success; }
};

} // namespace yuan

#endif // YUAN_FRONTEND_FRONTENDRESULT_H
