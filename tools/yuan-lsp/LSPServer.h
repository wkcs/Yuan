#ifndef YUAN_LSP_LSPSERVER_H
#define YUAN_LSP_LSPSERVER_H

#include "JSONRPC.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/Sema/Sema.h"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <vector>

using json = nlohmann::json;

namespace yuan {
namespace lsp {

class LSPServer {
public:
    LSPServer(std::istream& in, std::ostream& out);

    /// Run the server loop. Blocks until exit.
    void run();

private:
    std::istream& In;
    std::ostream& Out;
    bool ShutdownRequested = false;

    // Document state
    struct DocumentInfo {
        std::string Uri;
        std::string Content;
        unsigned Version = 0;
    };
    std::unordered_map<std::string, DocumentInfo> Documents;

    // Result of running the frontend pipeline on a document.
    struct FrontendResult {
        SourceManager SM;
        SourceManager::FileID FID = SourceManager::InvalidFileID;
        std::unique_ptr<ASTContext> Ctx;
        std::unique_ptr<DiagnosticEngine> DiagEngine;
        StoredDiagnosticConsumer* DiagConsumer = nullptr; // owned by DiagEngine
        std::vector<Decl*> Decls;
        std::unique_ptr<Sema> SemaInst;
    };

    /// Run Lexer -> Parser -> Sema on the given document content.
    FrontendResult runFrontend(const std::string& uri, const std::string& content);

    void handleMessage(const json& msg);
    void handleRequest(const std::string& method, const json& params, const json& id);
    void handleNotification(const std::string& method, const json& params);

    // Request handlers
    void onInitialize(const json& params, const json& id);
    void onInitialized(const json& params);
    void onShutdown(const json& id);
    void onExit();
    void onTextDocumentDidOpen(const json& params);
    void onTextDocumentDidChange(const json& params);
    void onTextDocumentDidClose(const json& params);
    void onHover(const json& params, const json& id);
    void onCompletion(const json& params, const json& id);
    void onDefinition(const json& params, const json& id);
    void onDocumentSymbol(const json& params, const json& id);

    // Helpers
    void reply(const json& id, const json& result);
    void replyError(const json& id, int code, const std::string& message);
    void sendNotification(const std::string& method, const json& params);

    // Diagnostics
    void validateDocument(const std::string& uri);

    // Convert LSP 0-indexed position to byte offset in content.
    // Returns SIZE_MAX if out of range.
    static size_t positionToOffset(const std::string& content,
                                   unsigned line, unsigned character);
};

} // namespace lsp
} // namespace yuan

#endif // YUAN_LSP_LSPSERVER_H
