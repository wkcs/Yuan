#ifndef YUAN_FRONTEND_COMPILERINSTANCE_H
#define YUAN_FRONTEND_COMPILERINSTANCE_H

#include "yuan/Frontend/CompilerInvocation.h"
#include "yuan/Frontend/FrontendResult.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Sema/Sema.h"
#include <iosfwd>
#include <memory>
#include <vector>

namespace yuan {

class Decl;
struct FrontendUnit {
    FrontendInputFile Input;
    SourceManager::FileID FileID = SourceManager::InvalidFileID;
    std::unique_ptr<ASTContext> Context;
    std::vector<Decl*> Declarations;
    std::unique_ptr<Sema> Semantic;
    bool Parsed = false;
    bool Analyzed = false;
};

class CompilerInstance {
public:
    explicit CompilerInstance(const CompilerInvocation& invocation);

    SourceManager& getSourceManager() { return SourceMgr; }
    const SourceManager& getSourceManager() const { return SourceMgr; }

    DiagnosticEngine& getDiagnostics() { return Diagnostics; }
    const DiagnosticEngine& getDiagnostics() const { return Diagnostics; }

    const CompilerInvocation& getInvocation() const { return Invocation; }
    CompilerInvocation& getInvocation() { return Invocation; }

    const std::vector<FrontendUnit>& getUnits() const { return Units; }
    std::vector<FrontendUnit>& getUnits() { return Units; }

    StoredDiagnosticConsumer* enableStoredDiagnostics();
    void enableTextDiagnostics(std::ostream& os, bool useColor = true);

    FrontendStatus loadInputs(const std::vector<FrontendInputFile>& inputs);
    FrontendStatus ensureParsed(size_t unitIndex);
    FrontendStatus ensureAnalyzed(size_t unitIndex);

private:
    void configureModuleManager(Sema& sema) const;

    CompilerInvocation Invocation;
    SourceManager SourceMgr;
    DiagnosticEngine Diagnostics;
    StoredDiagnosticConsumer* StoredConsumer = nullptr;
    std::vector<FrontendUnit> Units;
};

} // namespace yuan

#endif // YUAN_FRONTEND_COMPILERINSTANCE_H
