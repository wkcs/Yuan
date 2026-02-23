#include "yuan/Tooling/ToolRunner.h"

namespace yuan {

FrontendResult ToolRunner::runAction(const CompilerInvocation& invocation,
                                     FrontendAction& action,
                                     const std::vector<FrontendInputFile>& inputs,
                                     std::ostream& diagnosticOS) {
    CompilerInstance ci(invocation);
    ci.enableTextDiagnostics(diagnosticOS, true);
    return executeFrontendAction(ci, action, inputs);
}

} // namespace yuan
