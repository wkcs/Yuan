/// \file Diagnostic.cpp
/// \brief Implementation of diagnostic system.

#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/SourceManager.h"
#include <sstream>
#include <regex>

namespace yuan {

// ============================================================================
// Diagnostic Implementation
// ============================================================================

Diagnostic::Diagnostic(DiagID id, DiagnosticLevel level, SourceLocation loc)
    : ID(id), Level(level), Loc(loc) {}

Diagnostic& Diagnostic::operator<<(const std::string& arg) {
    Args.push_back(arg);
    return *this;
}

Diagnostic& Diagnostic::operator<<(const char* arg) {
    Args.push_back(arg ? arg : "(null)");
    return *this;
}

Diagnostic& Diagnostic::operator<<(int arg) {
    Args.push_back(std::to_string(arg));
    return *this;
}

Diagnostic& Diagnostic::operator<<(unsigned arg) {
    Args.push_back(std::to_string(arg));
    return *this;
}

Diagnostic& Diagnostic::operator<<(size_t arg) {
    Args.push_back(std::to_string(arg));
    return *this;
}

Diagnostic& Diagnostic::operator<<(SourceRange range) {
    Ranges.push_back(range);
    return *this;
}

Diagnostic& Diagnostic::addFixIt(SourceRange range, const std::string& replacement) {
    FixIts.emplace_back(range, replacement);
    return *this;
}

std::string Diagnostic::getMessage() const {
    std::string format = getDiagnosticFormatString(ID);
    std::string result = format;
    
    // Replace {0}, {1}, etc. with arguments
    for (size_t i = 0; i < Args.size(); ++i) {
        std::string placeholder = "{" + std::to_string(i) + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), Args[i]);
            pos += Args[i].length();
        }
    }

    // Replace any remaining placeholders with a fallback to avoid leaking "{0}"
    static const std::regex placeholderPattern("\\{\\d+\\}");
    result = std::regex_replace(result, placeholderPattern, "?");
    
    return result;
}

// ============================================================================
// DiagnosticBuilder Implementation
// ============================================================================

DiagnosticBuilder::DiagnosticBuilder(DiagnosticEngine& engine, DiagID id, SourceLocation loc)
    : Engine(&engine), Diag(id, getDiagnosticLevel(id), loc) {}

DiagnosticBuilder::~DiagnosticBuilder() {
    if (!Emitted && Engine) {
        emit();
    }
}

DiagnosticBuilder::DiagnosticBuilder(DiagnosticBuilder&& other) noexcept
    : Engine(other.Engine), Diag(std::move(other.Diag)), Emitted(other.Emitted) {
    other.Engine = nullptr;
    other.Emitted = true;
}

DiagnosticBuilder& DiagnosticBuilder::operator=(DiagnosticBuilder&& other) noexcept {
    if (this != &other) {
        if (!Emitted && Engine) {
            emit();
        }
        
        Engine = other.Engine;
        Diag = std::move(other.Diag);
        Emitted = other.Emitted;
        
        other.Engine = nullptr;
        other.Emitted = true;
    }
    return *this;
}

DiagnosticBuilder& DiagnosticBuilder::operator<<(const std::string& arg) {
    Diag << arg;
    return *this;
}

DiagnosticBuilder& DiagnosticBuilder::operator<<(const char* arg) {
    Diag << arg;
    return *this;
}

DiagnosticBuilder& DiagnosticBuilder::operator<<(int arg) {
    Diag << arg;
    return *this;
}

DiagnosticBuilder& DiagnosticBuilder::operator<<(unsigned arg) {
    Diag << arg;
    return *this;
}

DiagnosticBuilder& DiagnosticBuilder::operator<<(size_t arg) {
    Diag << arg;
    return *this;
}

DiagnosticBuilder& DiagnosticBuilder::operator<<(SourceRange range) {
    Diag << range;
    return *this;
}

DiagnosticBuilder& DiagnosticBuilder::addFixIt(SourceRange range, const std::string& replacement) {
    Diag.addFixIt(range, replacement);
    return *this;
}

void DiagnosticBuilder::emit() {
    if (Emitted || !Engine) {
        return;
    }
    
    Engine->emitDiagnostic(Diag);
    Emitted = true;
}

// ============================================================================
// DiagnosticEngine Implementation
// ============================================================================

DiagnosticEngine::DiagnosticEngine(SourceManager& sm) : SM(sm) {}

DiagnosticBuilder DiagnosticEngine::report(DiagID id, SourceLocation loc) {
    DiagnosticLevel level = getDiagnosticLevel(id);
    return report(id, loc, level);
}

DiagnosticBuilder DiagnosticEngine::report(DiagID id, SourceLocation loc, 
                                           DiagnosticLevel level) {
    // Apply warnings-as-errors if enabled
    if (WarningsAsErrors && level == DiagnosticLevel::Warning) {
        level = DiagnosticLevel::Error;
    }
    
    // Update counts
    switch (level) {
        case DiagnosticLevel::Error:
        case DiagnosticLevel::Fatal:
            ++ErrorCount;
            break;
        case DiagnosticLevel::Warning:
            ++WarningCount;
            break;
        case DiagnosticLevel::Note:
            // Notes don't count
            break;
    }
    
    return DiagnosticBuilder(*this, id, loc);
}

DiagnosticBuilder DiagnosticEngine::report(DiagID id, SourceLocation loc, SourceRange range) {
    DiagnosticLevel level = getDiagnosticLevel(id);
    
    // Apply warnings-as-errors if enabled
    if (WarningsAsErrors && level == DiagnosticLevel::Warning) {
        level = DiagnosticLevel::Error;
    }
    
    // Update counts
    switch (level) {
        case DiagnosticLevel::Error:
        case DiagnosticLevel::Fatal:
            ++ErrorCount;
            break;
        case DiagnosticLevel::Warning:
            ++WarningCount;
            break;
        case DiagnosticLevel::Note:
            // Notes don't count
            break;
    }
    
    DiagnosticBuilder builder(*this, id, loc);
    builder << range;  // 添加范围高亮
    return builder;
}

void DiagnosticEngine::setConsumer(std::unique_ptr<DiagnosticConsumer> consumer) {
    Consumer = std::move(consumer);
}

void DiagnosticEngine::reset() {
    ErrorCount = 0;
    WarningCount = 0;
}

void DiagnosticEngine::emitDiagnostic(const Diagnostic& diag) {
    if (Consumer) {
        Consumer->handleDiagnostic(diag);
    }
}

// ============================================================================
// StoredDiagnosticConsumer Implementation
// ============================================================================

void StoredDiagnosticConsumer::handleDiagnostic(const Diagnostic& diag) {
    Diagnostics.push_back(diag);
}

// ============================================================================
// MultiplexDiagnosticConsumer Implementation
// ============================================================================

void MultiplexDiagnosticConsumer::addConsumer(
    std::unique_ptr<DiagnosticConsumer> consumer) {
    Consumers.push_back(std::move(consumer));
}

void MultiplexDiagnosticConsumer::handleDiagnostic(const Diagnostic& diag) {
    for (auto& consumer : Consumers) {
        consumer->handleDiagnostic(diag);
    }
}

void MultiplexDiagnosticConsumer::finish() {
    for (auto& consumer : Consumers) {
        consumer->finish();
    }
}

} // namespace yuan
