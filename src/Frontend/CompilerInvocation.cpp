#include "yuan/Frontend/CompilerInvocation.h"

namespace yuan {

const char* CompilerInvocation::getActionString() const {
    switch (Action) {
        case FrontendActionKind::SyntaxOnly: return "syntax-only";
        case FrontendActionKind::EmitLLVM: return "emit-llvm";
        case FrontendActionKind::EmitObj: return "emit-obj";
        case FrontendActionKind::DumpTokens: return "dump-tokens";
        case FrontendActionKind::ASTDump: return "ast-dump";
        case FrontendActionKind::ASTPrint: return "ast-print";
    }
    return "syntax-only";
}

} // namespace yuan
