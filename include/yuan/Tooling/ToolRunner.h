#ifndef YUAN_TOOLING_TOOLRUNNER_H
#define YUAN_TOOLING_TOOLRUNNER_H

#include "yuan/Frontend/FrontendAction.h"
#include <iosfwd>

namespace yuan {

class ToolRunner {
public:
    static FrontendResult runAction(const CompilerInvocation& invocation,
                                    FrontendAction& action,
                                    const std::vector<FrontendInputFile>& inputs,
                                    std::ostream& diagnosticOS);
};

} // namespace yuan

#endif // YUAN_TOOLING_TOOLRUNNER_H
