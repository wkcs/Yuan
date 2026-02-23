#include "yuan/Frontend/FrontendAction.h"
#include "yuan/AST/ASTDumper.h"
#include "yuan/AST/ASTPrinter.h"
#include "yuan/Basic/TokenKinds.h"
#include "yuan/CodeGen/CodeGen.h"
#include "yuan/Lexer/Lexer.h"
#include <filesystem>
#include <fstream>

namespace yuan {

namespace {

std::string deduceOutputPath(const CompilerInstance& ci,
                             const FrontendInputFile& input,
                             const std::string& ext) {
    if (!input.OutputPath.empty()) {
        return input.OutputPath;
    }
    if (!ci.getInvocation().OutputFile.empty()) {
        return ci.getInvocation().OutputFile;
    }

    std::filesystem::path p(input.Name);
    std::string stem = p.stem().string();
    if (stem.empty()) {
        stem = "output";
    }
    return stem + ext;
}

void ensureParentDirectory(const std::string& outputPath) {
    std::filesystem::path p(outputPath);
    if (!p.parent_path().empty()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }
}

std::string moduleNameFromInput(const FrontendInputFile& input) {
    std::filesystem::path p(input.Name);
    std::string stem = p.stem().string();
    return stem.empty() ? "module" : stem;
}

FrontendStatus statusFromDiagnostics(const DiagnosticEngine& diagnostics,
                                     FrontendStatus defaultStatus) {
    if (!diagnostics.hasErrors()) {
        return FrontendStatus::Success;
    }
    return defaultStatus;
}

} // namespace

bool FrontendAction::beginSourceFile(CompilerInstance&,
                                     size_t,
                                     FrontendFileResult&) {
    return true;
}

void FrontendAction::endSourceFile(CompilerInstance&,
                                   size_t,
                                   FrontendFileResult&) {}

FrontendStatus DumpTokensAction::emitTokens(CompilerInstance& ci,
                                            Lexer& lexer,
                                            const std::string& inputName) {
    OS << "// Yuan 词法分析结果\n";
    OS << "// 源文件: " << inputName << "\n\n";

    Token token;
    unsigned tokenCount = 0;
    do {
        unsigned errorsBefore = ci.getDiagnostics().getErrorCount();
        token = lexer.lex();
        unsigned errorsAfter = ci.getDiagnostics().getErrorCount();
        bool hasTokenError = errorsAfter > errorsBefore;
        if (token.getKind() != TokenKind::EndOfFile && !hasTokenError) {
            SourceLocation loc = token.getLocation();
            auto [line, col] = ci.getSourceManager().getLineAndColumn(loc);
            OS << "Token[" << tokenCount << "]: "
               << getTokenName(token.getKind())
               << " \"" << token.getText() << "\""
               << " @" << inputName << ":" << line << ":" << col << "\n";
            ++tokenCount;
        }
    } while (token.getKind() != TokenKind::EndOfFile);

    OS << "\n// 总计: " << tokenCount << " 个 token\n";
    return statusFromDiagnostics(ci.getDiagnostics(), FrontendStatus::LexerError);
}

FrontendStatus DumpTokensAction::execute(CompilerInstance& ci,
                                         size_t unitIndex,
                                         FrontendFileResult& fileResult) {
    if (unitIndex >= ci.getUnits().size()) {
        return FrontendStatus::InternalError;
    }

    if (ci.getUnits().size() > 1) {
        OS << "== Tokens: " << fileResult.InputName << " ==\n";
    }
    Lexer lexer(ci.getSourceManager(), ci.getDiagnostics(), ci.getUnits()[unitIndex].FileID);
    return emitTokens(ci, lexer, fileResult.InputName);
}

FrontendStatus ASTDumpAction::execute(CompilerInstance& ci,
                                      size_t unitIndex,
                                      FrontendFileResult& fileResult) {
    FrontendStatus parseStatus = ci.ensureParsed(unitIndex);
    if (parseStatus != FrontendStatus::Success) {
        return parseStatus;
    }

    if (ci.getUnits().size() > 1) {
        OS << "== AST: " << fileResult.InputName << " ==\n";
    }

    ASTDumper dumper(OS);
    const auto& decls = ci.getUnits()[unitIndex].Declarations;
    for (const auto* decl : decls) {
        dumper.dump(decl);
    }
    return FrontendStatus::Success;
}

FrontendStatus ASTPrintAction::execute(CompilerInstance& ci,
                                       size_t unitIndex,
                                       FrontendFileResult& fileResult) {
    FrontendStatus parseStatus = ci.ensureParsed(unitIndex);
    if (parseStatus != FrontendStatus::Success) {
        return parseStatus;
    }

    if (ci.getUnits().size() > 1) {
        OS << "== Pretty: " << fileResult.InputName << " ==\n";
    }

    ASTPrinter printer(OS);
    const auto& decls = ci.getUnits()[unitIndex].Declarations;
    for (const auto* decl : decls) {
        printer.print(decl);
        OS << "\n";
    }
    return FrontendStatus::Success;
}

FrontendStatus SyntaxOnlyAction::execute(CompilerInstance& ci,
                                         size_t unitIndex,
                                         FrontendFileResult&) {
    return ci.ensureAnalyzed(unitIndex);
}

FrontendStatus EmitLLVMAction::execute(CompilerInstance& ci,
                                       size_t unitIndex,
                                       FrontendFileResult& fileResult) {
    FrontendStatus semaStatus = ci.ensureAnalyzed(unitIndex);
    if (semaStatus != FrontendStatus::Success) {
        return semaStatus;
    }

    auto& unit = ci.getUnits()[unitIndex];
    CodeGen codeGen(*unit.Context, moduleNameFromInput(unit.Input));
    for (Decl* decl : unit.Declarations) {
        if (!codeGen.generateDecl(decl)) {
            return FrontendStatus::CodeGenError;
        }
    }

    std::string verifyError;
    if (!codeGen.verifyModule(&verifyError)) {
        (void)verifyError;
        return FrontendStatus::CodeGenError;
    }

    std::string outputPath = deduceOutputPath(ci, unit.Input, ".ll");
    ensureParentDirectory(outputPath);
    if (!codeGen.emitIRToFile(outputPath)) {
        return FrontendStatus::CodeGenError;
    }
    fileResult.OutputPath = outputPath;
    return FrontendStatus::Success;
}

FrontendStatus EmitObjAction::execute(CompilerInstance& ci,
                                      size_t unitIndex,
                                      FrontendFileResult& fileResult) {
    FrontendStatus semaStatus = ci.ensureAnalyzed(unitIndex);
    if (semaStatus != FrontendStatus::Success) {
        return semaStatus;
    }

    auto& unit = ci.getUnits()[unitIndex];
    CodeGen codeGen(*unit.Context, moduleNameFromInput(unit.Input));
    for (Decl* decl : unit.Declarations) {
        if (!codeGen.generateDecl(decl)) {
            return FrontendStatus::CodeGenError;
        }
    }

    std::string verifyError;
    if (!codeGen.verifyModule(&verifyError)) {
        (void)verifyError;
        return FrontendStatus::CodeGenError;
    }

    std::string outputPath = deduceOutputPath(ci, unit.Input, ".o");
    ensureParentDirectory(outputPath);
    if (!codeGen.emitObjectFile(outputPath, ci.getInvocation().OptimizationLevel)) {
        return FrontendStatus::CodeGenError;
    }
    fileResult.OutputPath = outputPath;
    return FrontendStatus::Success;
}

FrontendResult executeFrontendAction(CompilerInstance& ci,
                                     FrontendAction& action,
                                     const std::vector<FrontendInputFile>& inputs) {
    FrontendResult result;
    result.OverallStatus = FrontendStatus::Success;

    FrontendStatus loadStatus = ci.loadInputs(inputs);
    if (loadStatus != FrontendStatus::Success) {
        result.OverallStatus = loadStatus;
        return result;
    }

    result.Files.reserve(ci.getUnits().size());
    for (size_t i = 0; i < ci.getUnits().size(); ++i) {
        FrontendFileResult fileResult;
        fileResult.InputName = ci.getUnits()[i].Input.Name;

        if (!action.beginSourceFile(ci, i, fileResult)) {
            fileResult.Status = FrontendStatus::InternalError;
        } else {
            fileResult.Status = action.execute(ci, i, fileResult);
            action.endSourceFile(ci, i, fileResult);
        }
        fileResult.ErrorCount = ci.getDiagnostics().getErrorCount();
        fileResult.WarningCount = ci.getDiagnostics().getWarningCount();

        if (result.OverallStatus == FrontendStatus::Success &&
            fileResult.Status != FrontendStatus::Success) {
            result.OverallStatus = fileResult.Status;
        }
        result.Files.push_back(std::move(fileResult));
    }

    return result;
}

} // namespace yuan
