#include "LSPServer.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/Lexer/Lexer.h"
#include "yuan/Parser/Parser.h"
#include "yuan/Sema/Sema.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/SourceManager.h"
#include <iostream>

using json = nlohmann::json;

namespace yuan {
namespace lsp {

LSPServer::LSPServer(std::istream& in, std::ostream& out) : In(in), Out(out) {}

void LSPServer::run() {
    while (!ShutdownRequested) {
        auto msgOpt = readMessage(In);
        if (!msgOpt) {
            break; // EOF or error
        }
        handleMessage(*msgOpt);
    }
}

void LSPServer::handleMessage(const json& msg) {
    if (msg.contains("id")) {
        if (msg.contains("method")) {
            handleRequest(msg["method"], msg.value("params", json::object()), msg["id"]);
        }
    } else if (msg.contains("method")) {
        handleNotification(msg["method"], msg.value("params", json::object()));
    }
}

void LSPServer::handleRequest(const std::string& method, const json& params, const json& id) {
    if (method == "initialize") {
        onInitialize(params, id);
    } else if (method == "shutdown") {
        onShutdown(id);
    } else {
        // MethodNotFound
        replyError(id, -32601, "Method not found: " + method);
    }
}

void LSPServer::handleNotification(const std::string& method, const json& params) {
    if (method == "initialized") {
        onInitialized(params);
    } else if (method == "exit") {
        onExit();
    } else if (method == "textDocument/didOpen") {
        onTextDocumentDidOpen(params);
    } else if (method == "textDocument/didChange") {
        onTextDocumentDidChange(params);
    } else if (method == "textDocument/didClose") {
        onTextDocumentDidClose(params);
    }
}

void LSPServer::onInitialize(const json& params, const json& id) {
    json result = {
        {"capabilities", {
            {"textDocumentSync", 1}, // Full sync
            // Add other capabilities here later (hover, completion, etc.)
        }}
    };
    reply(id, result);
}

void LSPServer::onInitialized(const json& params) {
    // Initialized, nothing to do currently.
}

void LSPServer::onShutdown(const json& id) {
    ShutdownRequested = true;
    reply(id, nullptr);
}

void LSPServer::onExit() {
    ShutdownRequested = true;
}

void LSPServer::onTextDocumentDidOpen(const json& params) {
    const auto& doc = params["textDocument"];
    std::string uri = doc["uri"];
    std::string text = doc["text"];

    unsigned version = 0;
    if (doc.contains("version")) {
        version = doc["version"];
    }

    Documents[uri] = DocumentInfo{uri, text, version};
    validateDocument(uri);
}

void LSPServer::onTextDocumentDidChange(const json& params) {
    const auto& doc = params["textDocument"];
    std::string uri = doc["uri"];

    unsigned version = 0;
    if (doc.contains("version")) {
        version = doc["version"];
    }

    if (Documents.find(uri) != Documents.end()) {
        const auto& changes = params["contentChanges"];
        if (!changes.empty()) {
            Documents[uri].Content = changes.back()["text"]; // Full sync
            Documents[uri].Version = version;
            validateDocument(uri);
        }
    }
}

void LSPServer::onTextDocumentDidClose(const json& params) {
    std::string uri = params["textDocument"]["uri"];
    Documents.erase(uri);

    // Clear diagnostics
    json diagParams = {
        {"uri", uri},
        {"diagnostics", json::array()}
    };
    sendNotification("textDocument/publishDiagnostics", diagParams);
}

void LSPServer::validateDocument(const std::string& uri) {
    if (Documents.find(uri) == Documents.end()) return;
    const auto& info = Documents[uri];

    SourceManager sm;
    SourceManager::FileID fid = sm.createBuffer(info.Content, uri);

    DiagnosticEngine diagEngine(sm);
    auto consumer = std::make_unique<StoredDiagnosticConsumer>();
    StoredDiagnosticConsumer* consumerPtr = consumer.get();
    diagEngine.setConsumer(std::move(consumer));

    ASTContext ctx(sm);
    ctx.setPointerBitWidth(64);

    Lexer lexer(sm, diagEngine, fid);
    Parser parser(lexer, diagEngine, ctx);
    std::vector<Decl*> decls = parser.parseCompilationUnit();

    // Semantic analysis
    Sema sema(ctx, diagEngine);
    for (Decl* decl : decls) {
        (void)sema.analyzeDecl(decl);
    }

    json diagnosticsArray = json::array();

    for (const Diagnostic& d : consumerPtr->getDiagnostics()) {
        auto [line, col] = sm.getLineAndColumn(d.getLocation());

        // LSP is 0-indexed, Yuan is 1-indexed
        int lspLine = (line > 0) ? line - 1 : 0;
        int lspCol = (col > 0) ? col - 1 : 0;

        int severity = 1; // 1 = Error, 2 = Warning, 3 = Info, 4 = Hint
        if (d.getLevel() == DiagnosticLevel::Warning) severity = 2;
        else if (d.getLevel() == DiagnosticLevel::Note) severity = 3;

        std::string message = d.getMessage();
        std::string code = d.getCode();

        json diag = {
            {"range", {
                {"start", {{"line", lspLine}, {"character", lspCol}}},
                {"end", {{"line", lspLine}, {"character", lspCol}}}
            }},
            {"severity", severity},
            {"code", code},
            {"message", message}
        };

        // Add range highlights if available
        if (!d.getRanges().empty()) {
            const SourceRange& r = d.getRanges()[0];
            auto [startL, startC] = sm.getLineAndColumn(r.getBegin());
            auto [endL, endC] = sm.getLineAndColumn(r.getEnd());
            int sl = (startL > 0) ? startL - 1 : 0;
            int sc = (startC > 0) ? startC - 1 : 0;
            int el = (endL > 0) ? endL - 1 : 0;
            int ec = (endC > 0) ? endC - 1 : 0;
            diag["range"]["start"]["line"] = sl;
            diag["range"]["start"]["character"] = sc;
            diag["range"]["end"]["line"] = el;
            diag["range"]["end"]["character"] = ec; // TODO: handle precise token end column
        }

        diagnosticsArray.push_back(diag);
    }

    json diagParams = {
        {"uri", uri},
        {"diagnostics", diagnosticsArray}
    };
    sendNotification("textDocument/publishDiagnostics", diagParams);
}

void LSPServer::reply(const json& id, const json& result) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", result}
    };
    writeMessage(Out, msg);
}

void LSPServer::replyError(const json& id, int code, const std::string& message) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {
            {"code", code},
            {"message", message}
        }}
    };
    writeMessage(Out, msg);
}

void LSPServer::sendNotification(const std::string& method, const json& params) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params}
    };
    writeMessage(Out, msg);
}

} // namespace lsp
} // namespace yuan
