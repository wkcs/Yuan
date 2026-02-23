#ifndef YUAN_FRONTEND_FRONTENDACTION_H
#define YUAN_FRONTEND_FRONTENDACTION_H

#include "yuan/Frontend/CompilerInstance.h"
#include <iosfwd>

namespace yuan {

class Lexer;

class FrontendAction {
public:
    virtual ~FrontendAction() = default;

    virtual bool beginSourceFile(CompilerInstance& ci,
                                 size_t unitIndex,
                                 FrontendFileResult& fileResult);
    virtual FrontendStatus execute(CompilerInstance& ci,
                                   size_t unitIndex,
                                   FrontendFileResult& fileResult) = 0;
    virtual void endSourceFile(CompilerInstance& ci,
                               size_t unitIndex,
                               FrontendFileResult& fileResult);

    virtual bool requiresSema() const { return false; }
};

class DumpTokensAction : public FrontendAction {
public:
    explicit DumpTokensAction(std::ostream& os) : OS(os) {}

    FrontendStatus execute(CompilerInstance& ci,
                           size_t unitIndex,
                           FrontendFileResult& fileResult) override;

private:
    FrontendStatus emitTokens(CompilerInstance& ci,
                              Lexer& lexer,
                              const std::string& inputName);

    std::ostream& OS;
};

class ASTDumpAction : public FrontendAction {
public:
    explicit ASTDumpAction(std::ostream& os) : OS(os) {}

    FrontendStatus execute(CompilerInstance& ci,
                           size_t unitIndex,
                           FrontendFileResult& fileResult) override;

private:
    std::ostream& OS;
};

class ASTPrintAction : public FrontendAction {
public:
    explicit ASTPrintAction(std::ostream& os) : OS(os) {}

    FrontendStatus execute(CompilerInstance& ci,
                           size_t unitIndex,
                           FrontendFileResult& fileResult) override;

private:
    std::ostream& OS;
};

class SyntaxOnlyAction : public FrontendAction {
public:
    FrontendStatus execute(CompilerInstance& ci,
                           size_t unitIndex,
                           FrontendFileResult& fileResult) override;

    bool requiresSema() const override { return true; }
};

class EmitLLVMAction : public FrontendAction {
public:
    FrontendStatus execute(CompilerInstance& ci,
                           size_t unitIndex,
                           FrontendFileResult& fileResult) override;

    bool requiresSema() const override { return true; }
};

class EmitObjAction : public FrontendAction {
public:
    FrontendStatus execute(CompilerInstance& ci,
                           size_t unitIndex,
                           FrontendFileResult& fileResult) override;

    bool requiresSema() const override { return true; }
};

FrontendResult executeFrontendAction(CompilerInstance& ci,
                                     FrontendAction& action,
                                     const std::vector<FrontendInputFile>& inputs);

} // namespace yuan

#endif // YUAN_FRONTEND_FRONTENDACTION_H
