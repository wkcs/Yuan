/// \file TextDiagnosticPrinter.h
/// \brief Text-based diagnostic printer with Clang-style output.
///
/// This file defines the TextDiagnosticPrinter class which formats and
/// outputs diagnostic messages in a style similar to Clang's error output.

#ifndef YUAN_BASIC_TEXTDIAGNOSTICPRINTER_H
#define YUAN_BASIC_TEXTDIAGNOSTICPRINTER_H

#include "yuan/Basic/Diagnostic.h"
#include <ostream>

namespace yuan {

class SourceManager;

/// \brief Text-based diagnostic printer (Clang-style output).
///
/// TextDiagnosticPrinter formats diagnostic messages with:
/// - File location (filename:line:column)
/// - Colored severity level (error/warning/note)
/// - Error code (e.g., E1001)
/// - Diagnostic message
/// - Source code line with caret (^) indicator
/// - Fix-it hints when available
///
/// Example output:
/// \code
/// test.yu:10:15: error[E3001]: use of undeclared identifier 'foo'
///     var x = foo + 1
///             ^~~
/// test.yu:5:5: note[N5007]: did you mean 'bar'?
///     var bar = 10
///         ^~~
/// \endcode
class TextDiagnosticPrinter : public DiagnosticConsumer {
public:
    /// \brief Construct a text diagnostic printer.
    /// \param os The output stream to write to.
    /// \param sm The source manager for location information.
    /// \param useColors Whether to use ANSI color codes.
    TextDiagnosticPrinter(std::ostream& os, SourceManager& sm,
                          bool useColors = true);
    
    /// \brief Handle a diagnostic message.
    void handleDiagnostic(const Diagnostic& diag) override;
    
    /// \brief Set whether to use colors.
    void setUseColors(bool value) { UseColors = value; }
    
    /// \brief Check if colors are enabled.
    bool getUseColors() const { return UseColors; }
    
    /// \brief Set whether to show error codes.
    void setShowErrorCodes(bool value) { ShowErrorCodes = value; }
    
    /// \brief Check if error codes are shown.
    bool getShowErrorCodes() const { return ShowErrorCodes; }
    
    /// \brief Set whether to show source lines.
    void setShowSourceLine(bool value) { ShowSourceLine = value; }
    
    /// \brief Check if source lines are shown.
    bool getShowSourceLine() const { return ShowSourceLine; }
    
    /// \brief Set whether to show fix-it hints.
    void setShowFixIts(bool value) { ShowFixIts = value; }
    
    /// \brief Check if fix-it hints are shown.
    bool getShowFixIts() const { return ShowFixIts; }
    
    /// \brief Set the number of context lines to show.
    void setContextLines(unsigned lines) { ContextLines = lines; }
    
    /// \brief Get the number of context lines.
    unsigned getContextLines() const { return ContextLines; }
    
private:
    std::ostream& OS;
    SourceManager& SM;
    bool UseColors;
    bool ShowErrorCodes = true;
    bool ShowSourceLine = true;
    bool ShowFixIts = true;
    unsigned ContextLines = 0;
    
    /// \brief Print the location prefix (filename:line:column:).
    void printLocation(SourceLocation loc);
    
    /// \brief Print the severity level with color.
    void printLevel(DiagnosticLevel level);
    
    /// \brief Print the error code.
    void printErrorCode(const Diagnostic& diag);
    
    /// \brief Print the source code line.
    void printSourceLine(SourceLocation loc);
    
    /// \brief Print the caret indicator.
    void printCaret(SourceLocation loc, const std::vector<SourceRange>& ranges);
    
    /// \brief Print fix-it hints.
    void printFixIts(const std::vector<std::pair<SourceRange, std::string>>& fixits);
    
    /// \brief Set the output color.
    void setColor(const char* color);
    
    /// \brief Reset the output color.
    void resetColor();
    
    /// \brief Set bold text.
    void setBold();
    
    /// \brief Get the column number for a location.
    unsigned getColumn(SourceLocation loc) const;
    
    /// \brief Calculate the display width of a string (handling tabs).
    unsigned getDisplayWidth(const std::string& str, unsigned startCol = 0) const;
    
    /// \brief Expand tabs in a string.
    std::string expandTabs(const std::string& str) const;
};

} // namespace yuan

#endif // YUAN_BASIC_TEXTDIAGNOSTICPRINTER_H
