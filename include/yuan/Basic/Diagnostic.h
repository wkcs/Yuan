/// \file Diagnostic.h
/// \brief Diagnostic system for compiler messages.
///
/// This file defines the diagnostic infrastructure for reporting errors,
/// warnings, and notes during compilation. The design follows Clang's
/// diagnostic system for familiar error output format.

#ifndef YUAN_BASIC_DIAGNOSTIC_H
#define YUAN_BASIC_DIAGNOSTIC_H

#include "yuan/Basic/DiagnosticIDs.h"
#include "yuan/Basic/SourceLocation.h"
#include <string>
#include <vector>
#include <memory>
#include <ostream>
#include <functional>

namespace yuan {

class SourceManager;
class Type;
class DiagnosticEngine;  // 前向声明

/// \brief A single diagnostic message.
///
/// Diagnostic represents a single compiler message (error, warning, or note)
/// with its location, arguments, and optional fix-it hints.
class Diagnostic {
public:
    /// \brief Construct a diagnostic.
    /// \param id The diagnostic ID.
    /// \param level The severity level.
    /// \param loc The source location.
    Diagnostic(DiagID id, DiagnosticLevel level, SourceLocation loc);
    
    /// \brief Get the diagnostic ID.
    DiagID getID() const { return ID; }
    
    /// \brief Get the severity level.
    DiagnosticLevel getLevel() const { return Level; }
    
    /// \brief Get the source location.
    SourceLocation getLocation() const { return Loc; }
    
    /// \brief Add a string argument to the diagnostic.
    Diagnostic& operator<<(const std::string& arg);
    
    /// \brief Add a C-string argument to the diagnostic.
    Diagnostic& operator<<(const char* arg);
    
    /// \brief Add an integer argument to the diagnostic.
    Diagnostic& operator<<(int arg);
    
    /// \brief Add an unsigned integer argument to the diagnostic.
    Diagnostic& operator<<(unsigned arg);
    
    /// \brief Add a size_t argument to the diagnostic.
    Diagnostic& operator<<(size_t arg);
    
    /// \brief Add a source range for highlighting.
    Diagnostic& operator<<(SourceRange range);
    
    /// \brief Add a fix-it hint.
    /// \param range The range to replace.
    /// \param replacement The replacement text.
    /// \return Reference to this diagnostic for chaining.
    Diagnostic& addFixIt(SourceRange range, const std::string& replacement);
    
    /// \brief Get the formatted message.
    /// \return The message with arguments substituted.
    std::string getMessage() const;
    
    /// \brief Get the raw arguments.
    const std::vector<std::string>& getArgs() const { return Args; }
    
    /// \brief Get the highlighted ranges.
    const std::vector<SourceRange>& getRanges() const { return Ranges; }
    
    /// \brief Get the fix-it hints.
    const std::vector<std::pair<SourceRange, std::string>>& getFixIts() const {
        return FixIts;
    }
    
    /// \brief Check if this diagnostic has fix-it hints.
    bool hasFixIts() const { return !FixIts.empty(); }
    
    /// \brief Get the error code string (e.g., "E1001").
    std::string getCode() const { return getDiagnosticCode(ID); }
    
private:
    DiagID ID;
    DiagnosticLevel Level;
    SourceLocation Loc;
    std::vector<std::string> Args;
    std::vector<SourceRange> Ranges;
    std::vector<std::pair<SourceRange, std::string>> FixIts;
};

/// \brief A builder for constructing diagnostics with arguments.
///
/// DiagnosticBuilder allows chaining arguments to a diagnostic before
/// it is emitted. The diagnostic is automatically emitted when the
/// builder is destroyed.
class DiagnosticBuilder {
public:
    /// \brief Construct a diagnostic builder.
    DiagnosticBuilder(DiagnosticEngine& engine, DiagID id, SourceLocation loc);
    
    /// \brief Destructor - emits the diagnostic.
    ~DiagnosticBuilder();
    
    /// \brief Move constructor.
    DiagnosticBuilder(DiagnosticBuilder&& other) noexcept;
    
    /// \brief Move assignment operator.
    DiagnosticBuilder& operator=(DiagnosticBuilder&& other) noexcept;
    
    // Disable copy operations
    DiagnosticBuilder(const DiagnosticBuilder&) = delete;
    DiagnosticBuilder& operator=(const DiagnosticBuilder&) = delete;
    
    /// \brief Add a string argument to the diagnostic.
    DiagnosticBuilder& operator<<(const std::string& arg);
    
    /// \brief Add a C-string argument to the diagnostic.
    DiagnosticBuilder& operator<<(const char* arg);
    
    /// \brief Add an integer argument to the diagnostic.
    DiagnosticBuilder& operator<<(int arg);
    
    /// \brief Add an unsigned integer argument to the diagnostic.
    DiagnosticBuilder& operator<<(unsigned arg);
    
    /// \brief Add a size_t argument to the diagnostic.
    DiagnosticBuilder& operator<<(size_t arg);
    
    /// \brief Add a source range for highlighting.
    DiagnosticBuilder& operator<<(SourceRange range);
    
    /// \brief Add a fix-it hint.
    DiagnosticBuilder& addFixIt(SourceRange range, const std::string& replacement);
    
    /// \brief Emit the diagnostic immediately.
    void emit();
    
private:
    DiagnosticEngine* Engine;
    Diagnostic Diag;
    bool Emitted = false;
};
///
/// DiagnosticConsumer is an abstract base class for objects that receive
/// and process diagnostic messages. Implementations can print to console,
/// store for later processing, or forward to other systems.
class DiagnosticConsumer {
public:
    virtual ~DiagnosticConsumer() = default;
    
    /// \brief Handle a diagnostic message.
    /// \param diag The diagnostic to handle.
    virtual void handleDiagnostic(const Diagnostic& diag) = 0;
    
    /// \brief Called when all diagnostics have been emitted.
    virtual void finish() {}
};

/// \brief The main diagnostic engine.
///
/// DiagnosticEngine is the central hub for reporting diagnostics. It manages
/// the diagnostic consumer, tracks error/warning counts, and provides the
/// interface for reporting new diagnostics.
class DiagnosticEngine {
public:
    /// \brief Construct a diagnostic engine.
    /// \param sm The source manager for location information.
    explicit DiagnosticEngine(SourceManager& sm);
    
    /// \brief Report a diagnostic.
    /// \param id The diagnostic ID.
    /// \param loc The source location.
    /// \return Reference to the created diagnostic for adding arguments.
    DiagnosticBuilder report(DiagID id, SourceLocation loc);
    
    /// \brief Report a diagnostic with automatic level detection.
    /// \param id The diagnostic ID.
    /// \param loc The source location.
    /// \return Reference to the created diagnostic for adding arguments.
    DiagnosticBuilder report(DiagID id, SourceLocation loc, DiagnosticLevel level);
    
    /// \brief Report a diagnostic with a source range for highlighting.
    /// \param id The diagnostic ID.
    /// \param loc The source location.
    /// \param range The source range to highlight.
    /// \return Reference to the created diagnostic for adding arguments.
    DiagnosticBuilder report(DiagID id, SourceLocation loc, SourceRange range);
    
    /// \brief Get the number of errors reported.
    unsigned getErrorCount() const { return ErrorCount; }
    
    /// \brief Get the number of warnings reported.
    unsigned getWarningCount() const { return WarningCount; }
    
    /// \brief Check if any errors have been reported.
    bool hasErrors() const { return ErrorCount > 0; }
    
    /// \brief Set the diagnostic consumer.
    /// \param consumer The consumer to receive diagnostics.
    void setConsumer(std::unique_ptr<DiagnosticConsumer> consumer);
    
    /// \brief Get the current diagnostic consumer.
    DiagnosticConsumer* getConsumer() const { return Consumer.get(); }
    
    /// \brief Get the source manager.
    SourceManager& getSourceManager() { return SM; }
    const SourceManager& getSourceManager() const { return SM; }
    
    /// \brief Reset error and warning counts.
    void reset();
    
    /// \brief Set whether warnings should be treated as errors.
    void setWarningsAsErrors(bool value) { WarningsAsErrors = value; }
    
    /// \brief Check if warnings are treated as errors.
    bool getWarningsAsErrors() const { return WarningsAsErrors; }
    
    /// \brief Set the maximum number of errors before stopping.
    void setErrorLimit(unsigned limit) { ErrorLimit = limit; }
    
    /// \brief Check if the error limit has been reached.
    bool hasReachedErrorLimit() const {
        return ErrorLimit > 0 && ErrorCount >= ErrorLimit;
    }
    
private:
    friend class Diagnostic;  // 允许 Diagnostic 访问 emitDiagnostic
    friend class DiagnosticBuilder;  // 允许 DiagnosticBuilder 访问 emitDiagnostic
    
    SourceManager& SM;
    std::unique_ptr<DiagnosticConsumer> Consumer;
    unsigned ErrorCount = 0;
    unsigned WarningCount = 0;
    bool WarningsAsErrors = false;
    unsigned ErrorLimit = 0;
    
    /// \brief Emit a diagnostic to the consumer.
    void emitDiagnostic(const Diagnostic& diag);
};

/// \brief A diagnostic consumer that stores diagnostics for later processing.
class StoredDiagnosticConsumer : public DiagnosticConsumer {
public:
    void handleDiagnostic(const Diagnostic& diag) override;
    
    /// \brief Get all stored diagnostics.
    const std::vector<Diagnostic>& getDiagnostics() const { return Diagnostics; }
    
    /// \brief Clear all stored diagnostics.
    void clear() { Diagnostics.clear(); }
    
private:
    std::vector<Diagnostic> Diagnostics;
};

/// \brief A diagnostic consumer that ignores all diagnostics.
class IgnoringDiagnosticConsumer : public DiagnosticConsumer {
public:
    void handleDiagnostic(const Diagnostic& /*diag*/) override {}
};

/// \brief A diagnostic consumer that forwards to multiple consumers.
class MultiplexDiagnosticConsumer : public DiagnosticConsumer {
public:
    /// \brief Add a consumer to forward diagnostics to.
    void addConsumer(std::unique_ptr<DiagnosticConsumer> consumer);
    
    void handleDiagnostic(const Diagnostic& diag) override;
    void finish() override;
    
private:
    std::vector<std::unique_ptr<DiagnosticConsumer>> Consumers;
};

} // namespace yuan

#endif // YUAN_BASIC_DIAGNOSTIC_H
