#include "LSPServer.h"
#include "yuan/AST/AST.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/ASTVisitor.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Tooling/ProjectConfig.h"
#include "yuan/Sema/Sema.h"
#include "yuan/Sema/Scope.h"
#include "yuan/Sema/Symbol.h"
#include "yuan/Sema/Type.h"
#include <cctype>
#include <climits>
#include <ctime>
#include <iostream>

using json = nlohmann::json;

namespace yuan {
namespace lsp {

// Log to stderr with timestamp (stdout is reserved for LSP wire protocol).
static void log(const std::string& msg) {
    std::time_t t = std::time(nullptr);
    char buf[20];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));
    std::cerr << "[yuan-lsp " << buf << "] " << msg << "\n";
    std::cerr.flush();
}

LSPServer::LSPServer(std::istream& in, std::ostream& out) : In(in), Out(out) {
    log("Server starting");
}

void LSPServer::run() {
    log("Entering message loop");
    while (!ShutdownRequested) {
        auto msgOpt = readMessage(In);
        if (!msgOpt) {
            log("EOF or read error, exiting");
            break;
        }
        handleMessage(*msgOpt);
    }
    log("Message loop exited");
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
    log("Request: " + method);
    if (method == "initialize") {
        onInitialize(params, id);
    } else if (method == "shutdown") {
        onShutdown(id);
    } else if (method == "textDocument/hover") {
        onHover(params, id);
    } else if (method == "textDocument/completion") {
        onCompletion(params, id);
    } else if (method == "textDocument/definition") {
        onDefinition(params, id);
    } else if (method == "textDocument/documentSymbol") {
        onDocumentSymbol(params, id);
    } else {
        // MethodNotFound
        replyError(id, -32601, "Method not found: " + method);
    }
}

void LSPServer::handleNotification(const std::string& method, const json& params) {
    log("Notification: " + method);
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
    log("initialize: client connected");
    json result = {
        {"capabilities", {
            {"textDocumentSync", 1}, // Full sync
            {"hoverProvider", true},
            {"completionProvider", {
                {"triggerCharacters", json::array({".", ":"})}
            }},
            {"definitionProvider", true},
            {"documentSymbolProvider", true},
        }}
    };
    reply(id, result);
}

void LSPServer::onInitialized(const json& params) {}

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
    unsigned version = doc.value("version", 0);

    Documents[uri] = DocumentInfo{uri, text, version};
    Session.invalidate(uri);
    validateDocument(uri);
}

void LSPServer::onTextDocumentDidChange(const json& params) {
    const auto& doc = params["textDocument"];
    std::string uri = doc["uri"];
    unsigned version = doc.value("version", 0);

    if (Documents.find(uri) != Documents.end()) {
        const auto& changes = params["contentChanges"];
        if (!changes.empty()) {
            Documents[uri].Content = changes.back()["text"]; // Full sync
            Documents[uri].Version = version;
            Session.invalidate(uri);
            validateDocument(uri);
        }
    }
}

void LSPServer::onTextDocumentDidClose(const json& params) {
    std::string uri = params["textDocument"]["uri"];
    Documents.erase(uri);
    Session.invalidate(uri);

    // Clear diagnostics
    json diagParams = {
        {"uri", uri},
        {"diagnostics", json::array()}
    };
    sendNotification("textDocument/publishDiagnostics", diagParams);
}

std::string LSPServer::uriToPath(const std::string& uri) {
    static const std::string fileScheme = "file://";
    if (uri.rfind(fileScheme, 0) == 0) {
        return uri.substr(fileScheme.size());
    }
    return uri;
}

CompilerInvocation LSPServer::buildInvocationForUri(const std::string& uri) const {
    CompilerInvocation invocation;
    invocation.Action = FrontendActionKind::SyntaxOnly;

    std::string path = uriToPath(uri);
    std::string projectFile = ProjectConfigLoader::discover(path);
    if (!projectFile.empty()) {
        ProjectConfig config;
        std::string errorMsg;
        if (ProjectConfigLoader::loadFromFile(projectFile, config, errorMsg)) {
            applyProjectConfig(config, invocation, true);
        } else {
            log("project config load failed: " + errorMsg);
        }
    }
    return invocation;
}

std::shared_ptr<SessionSnapshot> LSPServer::getSnapshot(const std::string& uri) {
    auto it = Documents.find(uri);
    if (it == Documents.end()) {
        return nullptr;
    }

    const DocumentInfo& doc = it->second;
    CompilerInvocation invocation = buildInvocationForUri(uri);
    return Session.getOrCreateSnapshot(uri, doc.Content, doc.Version, invocation);
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

void LSPServer::validateDocument(const std::string& uri) {
    auto snapshot = getSnapshot(uri);
    if (!snapshot || !snapshot->Instance || snapshot->Instance->getUnits().empty()) {
        return;
    }

    CompilerInstance& ci = *snapshot->Instance;
    SourceManager& sm = ci.getSourceManager();
    auto* diagConsumer = dynamic_cast<StoredDiagnosticConsumer*>(ci.getDiagnostics().getConsumer());
    if (!diagConsumer) {
        return;
    }

    json diagnosticsArray = json::array();

    for (const Diagnostic& d : diagConsumer->getDiagnostics()) {
        auto [line, col] = sm.getLineAndColumn(d.getLocation());

        // LSP is 0-indexed, Yuan is 1-indexed
        int lspLine = (line > 0) ? line - 1 : 0;
        int lspCol  = (col  > 0) ? col  - 1 : 0;

        int severity = 1; // 1=Error, 2=Warning, 3=Info, 4=Hint
        if (d.getLevel() == DiagnosticLevel::Warning) severity = 2;
        else if (d.getLevel() == DiagnosticLevel::Note) severity = 3;

        json diag = {
            {"range", {
                {"start", {{"line", lspLine}, {"character", lspCol}}},
                {"end",   {{"line", lspLine}, {"character", lspCol}}}
            }},
            {"severity", severity},
            {"code", d.getCode()},
            {"message", d.getMessage()}
        };

        if (!d.getRanges().empty()) {
            const SourceRange& r = d.getRanges()[0];
            auto [startL, startC] = sm.getLineAndColumn(r.getBegin());
            auto [endL,   endC  ] = sm.getLineAndColumn(r.getEnd());
            int sl = (startL > 0) ? startL - 1 : 0;
            int sc = (startC > 0) ? startC - 1 : 0;
            int el = (endL   > 0) ? endL   - 1 : 0;
            int ec = (endC   > 0) ? endC   - 1 : 0;
            diag["range"]["start"]["line"]      = sl;
            diag["range"]["start"]["character"] = sc;
            diag["range"]["end"]["line"]        = el;
            diag["range"]["end"]["character"]   = ec;
        }

        diagnosticsArray.push_back(diag);
    }

    json diagParams = {
        {"uri", uri},
        {"diagnostics", diagnosticsArray}
    };
    sendNotification("textDocument/publishDiagnostics", diagParams);
}

// ---------------------------------------------------------------------------
// positionToOffset: LSP 0-based (line, character) -> byte offset
//
// LSP character是UTF-16码元数。UTF-8解码后，BMP字符（U+0000..U+FFFF）
// 计1个UTF-16码元，补充平面字符（U+10000+）计2个码元。
// ---------------------------------------------------------------------------

// 解码一个UTF-8序列，返回Unicode码点，同时推进字节指针。
// 若遇到非法序列，按1字节步进返回替换字符。
static uint32_t decodeUtf8(const char* data, size_t size, size_t& pos) {
    unsigned char c = static_cast<unsigned char>(data[pos]);
    if (c < 0x80) {
        ++pos;
        return c;
    }
    int bytes;
    uint32_t cp;
    if      ((c & 0xE0) == 0xC0) { bytes = 2; cp = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { bytes = 3; cp = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { bytes = 4; cp = c & 0x07; }
    else { ++pos; return 0xFFFD; } // 无效序列
    if (pos + bytes > size) { ++pos; return 0xFFFD; }
    for (int i = 1; i < bytes; ++i) {
        unsigned char b = static_cast<unsigned char>(data[pos + i]);
        if ((b & 0xC0) != 0x80) { ++pos; return 0xFFFD; }
        cp = (cp << 6) | (b & 0x3F);
    }
    pos += bytes;
    return cp;
}

size_t LSPServer::positionToOffset(const std::string& content,
                                   unsigned line, unsigned character) {
    // 先定位到目标行首
    unsigned curLine = 0;
    size_t i = 0;
    while (i < content.size() && curLine < line) {
        if (content[i] == '\n') ++curLine;
        ++i;
    }
    if (curLine < line) return SIZE_MAX; // 行越界

    // 在目标行内，按UTF-16码元数步进到目标列
    unsigned utf16Units = 0;
    while (i < content.size() && content[i] != '\n') {
        if (utf16Units >= character) break;
        size_t before = i;
        uint32_t cp = decodeUtf8(content.data(), content.size(), i);
        // 超出BMP的字符占2个UTF-16码元
        unsigned units = (cp >= 0x10000) ? 2 : 1;
        if (utf16Units + units > character) {
            // character落在一个宽字符内部，退回到字符起始
            i = before;
            break;
        }
        utf16Units += units;
    }
    return i;
}

// ---------------------------------------------------------------------------
// AST Visitor for finding node under cursor
// ---------------------------------------------------------------------------

class HoverDefVisitor : public ASTVisitor<HoverDefVisitor> {
public:
    SourceLocation TargetLoc;
    ASTNode* BestNode = nullptr;

    HoverDefVisitor(SourceLocation loc) : TargetLoc(loc) {}

    void checkNode(ASTNode* node) {
        if (!node) return;
        SourceRange r = node->getRange();
        if (r.isValid() && TargetLoc >= r.getBegin() && TargetLoc <= r.getEnd()) {
            BestNode = node;
        }
    }

    void visitDecl(Decl* decl) {
        checkNode(decl);
        ASTVisitor::visitDecl(decl);
    }

    void visitStmt(Stmt* stmt) {
        checkNode(stmt);
        ASTVisitor::visitStmt(stmt);
    }

    void visitExpr(Expr* expr) {
        checkNode(expr);
        ASTVisitor::visitExpr(expr);
    }

    void visitTypeNode(TypeNode* type) {
        checkNode(type);
        ASTVisitor::visitTypeNode(type);
    }

    void visitPattern(Pattern* pat) {
        checkNode(pat);
        ASTVisitor::visitPattern(pat);
    }
};

// ---------------------------------------------------------------------------
// Hover
// ---------------------------------------------------------------------------

void LSPServer::onHover(const json& params, const json& id) {
    std::string uri = params["textDocument"]["uri"];
    if (Documents.find(uri) == Documents.end()) {
        log("hover: document not found: " + uri);
        reply(id, nullptr);
        return;
    }
    const auto& info = Documents[uri];

    unsigned lspLine = params["position"]["line"];
    unsigned lspChar = params["position"]["character"];
    log("hover: " + uri + " " + std::to_string(lspLine) + ":" + std::to_string(lspChar));

    auto snapshot = getSnapshot(uri);
    if (!snapshot || !snapshot->Instance || snapshot->Instance->getUnits().empty()) {
        reply(id, nullptr);
        return;
    }

    CompilerInstance& ci = *snapshot->Instance;
    FrontendUnit& unit = ci.getUnits().front();

    size_t offset = positionToOffset(info.Content, lspLine, lspChar);
    if (offset == SIZE_MAX) {
        log("hover: position out of range");
        reply(id, nullptr);
        return;
    }

    // LSP offset -> SourceLocation (SourceManager offsets are 1-based global)
    SourceLocation targetLoc = ci.getSourceManager().getLocation(unit.FileID, static_cast<uint32_t>(offset));

    HoverDefVisitor visitor(targetLoc);
    for (Decl* decl : unit.Declarations) {
        visitor.visit(decl);
    }

    if (!visitor.BestNode) {
        log("hover: no AST node at cursor");
        reply(id, nullptr);
        return;
    }

    std::string hoverText;
    ASTNode* node = visitor.BestNode;

    if (node->isExpr()) {
        Expr* expr = static_cast<Expr*>(node);
        if (expr->getType()) {
            hoverText = "```yuan\n" + expr->getType()->toString() + "\n```";
        }
    } else if (node->isDecl()) {
        Decl* decl = static_cast<Decl*>(node);
        ASTNode::Kind k = decl->getKind();
        if (k == ASTNode::Kind::VarDecl) {
            auto* vd = static_cast<VarDecl*>(decl);
            hoverText = "```yuan\n" + std::string(vd->isMutable() ? "var " : "let ") + vd->getName();
            if (vd->getType()) hoverText += ": type"; // We'd need Sema to resolve it
            hoverText += "\n```";
        } else if (k == ASTNode::Kind::FuncDecl) {
            auto* fd = static_cast<FuncDecl*>(decl);
            hoverText = "```yuan\nfunc " + fd->getName() + "()\n```";
        }
    }

    if (hoverText.empty()) {
        log("hover: AST node has no type information");
        reply(id, nullptr);
        return;
    }

    json hoverResult = {
        {"contents", {
            {"kind", "markdown"},
            {"value", hoverText}
        }}
    };
    reply(id, hoverResult);
}

// ---------------------------------------------------------------------------
// Completion
// ---------------------------------------------------------------------------

void LSPServer::onCompletion(const json& params, const json& id) {
    std::string uri = params["textDocument"]["uri"];
    if (Documents.find(uri) == Documents.end()) {
        reply(id, json::array());
        return;
    }
    auto snapshot = getSnapshot(uri);
    if (!snapshot || !snapshot->Instance || snapshot->Instance->getUnits().empty()) {
        reply(id, json::array());
        return;
    }

    CompilerInstance& ci = *snapshot->Instance;
    FrontendUnit& unit = ci.getUnits().front();
    if (!unit.Semantic) {
        reply(id, json::array());
        return;
    }

    // LSP CompletionItemKind values
    // Text=1, Method=2, Function=3, Constructor=4, Field=5, Variable=6,
    // Class=7, Interface=8, Module=9, Property=10, Keyword=14,
    // Enum=13, EnumMember=20, Struct=22
    auto kindFor = [](SymbolKind k) -> int {
        switch (k) {
        case SymbolKind::Function:    return 3;
        case SymbolKind::Method:      return 2;
        case SymbolKind::Variable:
        case SymbolKind::Parameter:   return 6;
        case SymbolKind::Constant:    return 21; // Constant
        case SymbolKind::Struct:      return 22;
        case SymbolKind::Enum:        return 13;
        case SymbolKind::EnumVariant: return 20;
        case SymbolKind::Trait:       return 8;
        case SymbolKind::TypeAlias:   return 7;
        case SymbolKind::Field:       return 5;
        case SymbolKind::GenericParam:return 1;
        }
        return 1;
    };

    json items = json::array();

    // Walk all scopes from current up to global to collect visible symbols.
    Scope* scope = unit.Semantic->getSymbolTable().getCurrentScope();
    // Gather unique names to avoid duplicates from shadowing.
    std::unordered_map<std::string, bool> seen;
    while (scope) {
        for (const auto& [name, sym] : scope->getSymbols()) {
            if (seen.count(name)) continue;
            seen[name] = true;

            json item = {
                {"label", name},
                {"kind",  kindFor(sym->getKind())}
            };
            if (sym->getType()) {
                item["detail"] = sym->getType()->toString();
            }
            items.push_back(std::move(item));
        }
        scope = scope->getParent();
    }

    reply(id, items);
}

// ---------------------------------------------------------------------------
// Definition
// ---------------------------------------------------------------------------

void LSPServer::onDefinition(const json& params, const json& id) {
    std::string uri = params["textDocument"]["uri"];
    if (Documents.find(uri) == Documents.end()) {
        reply(id, nullptr);
        return;
    }
    const auto& info = Documents[uri];

    unsigned lspLine = params["position"]["line"];
    unsigned lspChar = params["position"]["character"];

    auto snapshot = getSnapshot(uri);
    if (!snapshot || !snapshot->Instance || snapshot->Instance->getUnits().empty()) {
        reply(id, nullptr);
        return;
    }
    CompilerInstance& ci = *snapshot->Instance;
    FrontendUnit& unit = ci.getUnits().front();
    if (!unit.Semantic) {
        reply(id, nullptr);
        return;
    }

    size_t offset = positionToOffset(info.Content, lspLine, lspChar);
    if (offset == SIZE_MAX) {
        reply(id, nullptr);
        return;
    }

    // Extract identifier under cursor.
    auto isIdentChar2 = [](unsigned char c) {
        return std::isalnum(c) || c == '_' || c >= 0x80;
    };
    size_t start = offset;
    while (start > 0 && isIdentChar2(static_cast<unsigned char>(info.Content[start - 1]))) {
        --start;
    }
    size_t end = offset;
    while (end < info.Content.size() && isIdentChar2(static_cast<unsigned char>(info.Content[end]))) {
        ++end;
    }

    if (start == end) {
        reply(id, nullptr);
        return;
    }

    std::string word = info.Content.substr(start, end - start);
    Symbol* sym = unit.Semantic->getSymbolTable().lookup(word);
    if (!sym) {
        reply(id, nullptr);
        return;
    }

    SourceLocation defLoc = sym->getLocation();
    if (!defLoc.isValid()) {
        reply(id, nullptr);
        return;
    }

    auto [defLine, defCol] = ci.getSourceManager().getLineAndColumn(defLoc);
    int defLspLine = (defLine > 0) ? defLine - 1 : 0;
    int defLspCol  = (defCol  > 0) ? defCol  - 1 : 0;

    json location = {
        {"uri", uri},
        {"range", {
            {"start", {{"line", defLspLine}, {"character", defLspCol}}},
            {"end",   {{"line", defLspLine}, {"character", defLspCol}}}
        }}
    };
    reply(id, location);
}

// ---------------------------------------------------------------------------
// DocumentSymbol
// ---------------------------------------------------------------------------

void LSPServer::onDocumentSymbol(const json& params, const json& id) {
    std::string uri = params["textDocument"]["uri"];
    if (Documents.find(uri) == Documents.end()) {
        reply(id, json::array());
        return;
    }
    auto snapshot = getSnapshot(uri);
    if (!snapshot || !snapshot->Instance || snapshot->Instance->getUnits().empty()) {
        reply(id, json::array());
        return;
    }
    CompilerInstance& ci = *snapshot->Instance;
    FrontendUnit& unit = ci.getUnits().front();

    // LSP SymbolKind: File=1, Module=2, Namespace=3, Class=5, Method=6,
    // Property=7, Field=8, Constructor=9, Enum=10, Interface=11,
    // Function=12, Variable=13, Constant=14, String=15, Struct=23,
    // EnumMember=22, TypeParameter=26
    json symbols = json::array();

    auto makeRange = [&](SourceLocation loc) -> json {
        auto [line, col] = ci.getSourceManager().getLineAndColumn(loc);
        int l = (line > 0) ? line - 1 : 0;
        int c = (col  > 0) ? col  - 1 : 0;
        return {
            {"start", {{"line", l}, {"character", c}}},
            {"end",   {{"line", l}, {"character", c}}}
        };
    };

    for (Decl* decl : unit.Declarations) {
        if (!decl) continue;

        json sym;
        ASTNode::Kind k = decl->getKind();

        if (k == ASTNode::Kind::FuncDecl) {
            auto* fd = static_cast<FuncDecl*>(decl);
            json range = makeRange(fd->getRange().getBegin());
            sym = {
                {"name",           fd->getName()},
                {"kind",           12}, // Function
                {"range",          range},
                {"selectionRange", makeRange(fd->getRange().getBegin())}
            };
        } else if (k == ASTNode::Kind::StructDecl) {
            auto* sd = static_cast<StructDecl*>(decl);
            json range = makeRange(sd->getRange().getBegin());
            sym = {
                {"name",           sd->getName()},
                {"kind",           23}, // Struct
                {"range",          range},
                {"selectionRange", makeRange(sd->getRange().getBegin())}
            };
        } else if (k == ASTNode::Kind::EnumDecl) {
            auto* ed = static_cast<EnumDecl*>(decl);
            json range = makeRange(ed->getRange().getBegin());
            sym = {
                {"name",           ed->getName()},
                {"kind",           10}, // Enum
                {"range",          range},
                {"selectionRange", makeRange(ed->getRange().getBegin())}
            };
        } else if (k == ASTNode::Kind::TraitDecl) {
            auto* td = static_cast<TraitDecl*>(decl);
            json range = makeRange(td->getRange().getBegin());
            sym = {
                {"name",           td->getName()},
                {"kind",           11}, // Interface
                {"range",          range},
                {"selectionRange", makeRange(td->getRange().getBegin())}
            };
        } else if (k == ASTNode::Kind::VarDecl) {
            auto* vd = static_cast<VarDecl*>(decl);
            json range = makeRange(vd->getRange().getBegin());
            sym = {
                {"name",           vd->getName()},
                {"kind",           13}, // Variable
                {"range",          range},
                {"selectionRange", makeRange(vd->getRange().getBegin())}
            };
        } else if (k == ASTNode::Kind::ConstDecl) {
            auto* cd = static_cast<ConstDecl*>(decl);
            json range = makeRange(cd->getRange().getBegin());
            sym = {
                {"name",           cd->getName()},
                {"kind",           14}, // Constant
                {"range",          range},
                {"selectionRange", makeRange(cd->getRange().getBegin())}
            };
        } else {
            continue;
        }

        symbols.push_back(std::move(sym));
    }

    reply(id, symbols);
}

// ---------------------------------------------------------------------------
// Wire helpers
// ---------------------------------------------------------------------------

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
