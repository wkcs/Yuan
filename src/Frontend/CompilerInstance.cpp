#include "yuan/Frontend/CompilerInstance.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/Basic/TextDiagnosticPrinter.h"
#include "yuan/Lexer/Lexer.h"
#include "yuan/Parser/Parser.h"
#include "yuan/Sema/Sema.h"
#include <cstdint>

namespace yuan {

CompilerInstance::CompilerInstance(const CompilerInvocation& invocation)
    : Invocation(invocation), Diagnostics(SourceMgr) {
    Diagnostics.setConsumer(std::make_unique<IgnoringDiagnosticConsumer>());
}

StoredDiagnosticConsumer* CompilerInstance::enableStoredDiagnostics() {
    auto consumer = std::make_unique<StoredDiagnosticConsumer>();
    StoredConsumer = consumer.get();
    Diagnostics.setConsumer(std::move(consumer));
    return StoredConsumer;
}

void CompilerInstance::enableTextDiagnostics(std::ostream& os, bool useColor) {
    StoredConsumer = nullptr;
    Diagnostics.setConsumer(std::make_unique<TextDiagnosticPrinter>(os, SourceMgr, useColor));
}

FrontendStatus CompilerInstance::loadInputs(const std::vector<FrontendInputFile>& inputs) {
    Diagnostics.reset();
    Units.clear();
    Units.reserve(inputs.size());

    for (const FrontendInputFile& input : inputs) {
        SourceManager::FileID fid = SourceManager::InvalidFileID;
        if (input.IsBuffer) {
            fid = SourceMgr.createBuffer(input.Buffer, input.Name);
        } else {
            fid = SourceMgr.loadFile(input.Name);
        }
        if (fid == SourceManager::InvalidFileID) {
            return FrontendStatus::IOError;
        }

        FrontendUnit unit;
        unit.Input = input;
        unit.FileID = fid;
        Units.push_back(std::move(unit));
    }

    return FrontendStatus::Success;
}

FrontendStatus CompilerInstance::ensureParsed(size_t unitIndex) {
    if (unitIndex >= Units.size()) {
        return FrontendStatus::InternalError;
    }

    FrontendUnit& unit = Units[unitIndex];
    if (unit.Parsed) {
        return FrontendStatus::Success;
    }

    unit.Context = std::make_unique<ASTContext>(SourceMgr);
    unit.Context->setPointerBitWidth(static_cast<unsigned>(sizeof(uintptr_t) * 8));

    Lexer lexer(SourceMgr, Diagnostics, unit.FileID);
    Parser parser(lexer, Diagnostics, *unit.Context);
    unit.Declarations = parser.parseCompilationUnit();
    unit.Parsed = true;

    if (Diagnostics.hasErrors()) {
        return FrontendStatus::ParserError;
    }
    return FrontendStatus::Success;
}

void CompilerInstance::configureModuleManager(Sema& sema) const {
    ModuleManager& moduleMgr = sema.getModuleManager();
    if (!Invocation.StdLibPath.empty()) {
        moduleMgr.setStdLibPath(Invocation.StdLibPath);
    }
    moduleMgr.setModuleCacheDir(Invocation.ModuleCacheDir);
    for (const auto& pkgPath : Invocation.PackagePaths) {
        moduleMgr.addPackagePath(pkgPath);
    }
    for (const auto& includePath : Invocation.IncludePaths) {
        moduleMgr.addPackagePath(includePath);
    }
}

FrontendStatus CompilerInstance::ensureAnalyzed(size_t unitIndex) {
    FrontendStatus parseStatus = ensureParsed(unitIndex);
    if (parseStatus != FrontendStatus::Success) {
        return parseStatus;
    }

    FrontendUnit& unit = Units[unitIndex];
    if (unit.Analyzed) {
        return FrontendStatus::Success;
    }

    unit.Semantic = std::make_unique<Sema>(*unit.Context, Diagnostics);
    configureModuleManager(*unit.Semantic);

    yuan::CompilationUnit semaUnit(unit.FileID);
    for (Decl* decl : unit.Declarations) {
        semaUnit.addDecl(decl);
    }
    (void)unit.Semantic->analyze(&semaUnit);
    unit.Analyzed = true;

    if (Diagnostics.hasErrors()) {
        return FrontendStatus::SemanticError;
    }
    return FrontendStatus::Success;
}

} // namespace yuan
