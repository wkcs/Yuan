#ifndef YUAN_TOOLING_COMPILERSESSION_H
#define YUAN_TOOLING_COMPILERSESSION_H

#include "yuan/Frontend/CompilerInstance.h"
#include "yuan/Frontend/FrontendAction.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace yuan {

struct SessionSnapshot {
    std::string InputName;
    unsigned Version = 0;
    size_t InvocationHash = 0;
    FrontendResult Result;
    std::unique_ptr<CompilerInstance> Instance;
};

class CompilerSession {
public:
    std::shared_ptr<SessionSnapshot> getOrCreateSnapshot(const std::string& inputName,
                                                         const std::string& content,
                                                         unsigned version,
                                                         const CompilerInvocation& invocation);

    void invalidate(const std::string& inputName);
    void clear();

private:
    static size_t computeInvocationHash(const CompilerInvocation& invocation);
    static std::string makeKey(const std::string& inputName,
                               unsigned version,
                               size_t invocationHash);

    std::unordered_map<std::string, std::shared_ptr<SessionSnapshot>> Snapshots;
};

} // namespace yuan

#endif // YUAN_TOOLING_COMPILERSESSION_H
