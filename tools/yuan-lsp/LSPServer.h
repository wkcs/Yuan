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

    void handleMessage(const json& msg);
    void handleRequest(const std::string& method, const json& params, const json& id);
    void handleNotification(const std::string& method, const json& params);

    // Handlers
    void onInitialize(const json& params, const json& id);
    void onInitialized(const json& params);
    void onShutdown(const json& id);
    void onExit();
    void onTextDocumentDidOpen(const json& params);
    void onTextDocumentDidChange(const json& params);
    void onTextDocumentDidClose(const json& params);

    // Helpers
    void reply(const json& id, const json& result);
    void replyError(const json& id, int code, const std::string& message);
    void sendNotification(const std::string& method, const json& params);

    // Diagnostics
    void validateDocument(const std::string& uri);
};

} // namespace lsp
} // namespace yuan

#endif // YUAN_LSP_LSPSERVER_H
