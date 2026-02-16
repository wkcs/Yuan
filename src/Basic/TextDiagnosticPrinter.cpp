/// \file TextDiagnosticPrinter.cpp
/// \brief Implementation of text-based diagnostic printer.

#include "yuan/Basic/TextDiagnosticPrinter.h"
#include "yuan/Basic/SourceManager.h"
#include <algorithm>
#include <cstdlib>
#include <iomanip>

namespace yuan {

// ANSI color codes
namespace colors {
    const char* Reset = "\033[0m";
    const char* Bold = "\033[1m";
    const char* Red = "\033[31m";
    const char* Green = "\033[32m";
    const char* Yellow = "\033[33m";
    const char* Blue = "\033[34m";
    const char* Magenta = "\033[35m";
    const char* Cyan = "\033[36m";
    const char* White = "\033[37m";
    const char* BoldRed = "\033[1;31m";
    const char* BoldGreen = "\033[1;32m";
    const char* BoldYellow = "\033[1;33m";
    const char* BoldBlue = "\033[1;34m";
    const char* BoldMagenta = "\033[1;35m";
    const char* BoldCyan = "\033[1;36m";
    const char* BoldWhite = "\033[1;37m";
}

TextDiagnosticPrinter::TextDiagnosticPrinter(std::ostream& os, SourceManager& sm,
                                             bool useColors)
    : OS(os), SM(sm), UseColors(useColors) {
    // Check if we should disable colors (e.g., if output is not a terminal)
    // For now, we trust the caller's preference
}

void TextDiagnosticPrinter::handleDiagnostic(const Diagnostic& diag) {
    // Print location
    printLocation(diag.getLocation());
    
    // Print severity level
    printLevel(diag.getLevel());
    
    // Print error code if enabled
    if (ShowErrorCodes) {
        printErrorCode(diag);
    }
    
    // Print message
    setBold();
    OS << diag.getMessage();
    resetColor();
    OS << "\n";
    
    // Print source line if enabled and location is valid
    if (ShowSourceLine && diag.getLocation().isValid()) {
        printSourceLine(diag.getLocation());
        printCaret(diag.getLocation(), diag.getRanges());
    }
    
    // Print fix-it hints if enabled
    if (ShowFixIts && diag.hasFixIts()) {
        printFixIts(diag.getFixIts());
    }
}

void TextDiagnosticPrinter::printLocation(SourceLocation loc) {
    if (loc.isInvalid()) {
        setBold();
        OS << "<unknown>: ";
        resetColor();
        return;
    }
    
    auto fileID = SM.getFileID(loc);
    if (fileID == SourceManager::InvalidFileID) {
        setBold();
        OS << "<unknown>: ";
        resetColor();
        return;
    }
    
    auto [line, column] = SM.getLineAndColumn(loc);
    const std::string& filename = SM.getFilename(fileID);
    
    setBold();
    OS << filename << ":" << line << ":" << column << ": ";
    resetColor();
}

void TextDiagnosticPrinter::printLevel(DiagnosticLevel level) {
    switch (level) {
        case DiagnosticLevel::Note:
            setColor(colors::BoldCyan);
            OS << "note";
            break;
        case DiagnosticLevel::Warning:
            setColor(colors::BoldYellow);
            OS << "warning";
            break;
        case DiagnosticLevel::Error:
            setColor(colors::BoldRed);
            OS << "error";
            break;
        case DiagnosticLevel::Fatal:
            setColor(colors::BoldRed);
            OS << "fatal error";
            break;
    }
    resetColor();
}

void TextDiagnosticPrinter::printErrorCode(const Diagnostic& diag) {
    setColor(colors::Bold);
    OS << "[" << diag.getCode() << "]";
    resetColor();
    OS << ": ";
}

void TextDiagnosticPrinter::printSourceLine(SourceLocation loc) {
    std::string line = SM.getLineContent(loc);
    if (line.empty()) {
        return;
    }
    
    // Expand tabs for consistent display
    std::string expandedLine = expandTabs(line);
    
    // Get line number
    auto [lineNum, col] = SM.getLineAndColumn(loc);
    (void)col;  // Suppress unused variable warning
    
    // Print line number with formatting like compiler warnings
    OS << std::setw(5) << lineNum << " | " << expandedLine << "\n";
}

void TextDiagnosticPrinter::printCaret(SourceLocation loc, 
                                        const std::vector<SourceRange>& ranges) {
    if (loc.isInvalid()) {
        return;
    }
    
    std::string line = SM.getLineContent(loc);
    if (line.empty()) {
        return;
    }
    
    auto [lineNum, column] = SM.getLineAndColumn(loc);
    
    // 计算显示位置，考虑制表符
    std::string expandedLine = expandTabs(line);
    auto columnToDisplay = [&](unsigned col) -> unsigned {
        unsigned display = 0;
        for (unsigned i = 0; i < col - 1 && i < line.size(); ++i) {
            if (line[i] == '\t') {
                display = ((display / 4) + 1) * 4;
            } else {
                ++display;
            }
        }
        return display;
    };
    unsigned displayCol = columnToDisplay(column);
    
    // 打印缩进行，匹配行号格式
    OS << "      | ";  // 匹配行号格式（5个空格 + " | "）
    
    setColor(colors::BoldGreen);
    
    // 如果有范围，先处理范围高亮
    if (!ranges.empty()) {
        // 找到在同一行的范围
        for (const auto& range : ranges) {
            if (range.getBegin().isValid() && range.getEnd().isValid()) {
                auto [rangeStartLine, rangeStartCol] = SM.getLineAndColumn(range.getBegin());
                auto [rangeEndLine, rangeEndCol] = SM.getLineAndColumn(range.getEnd());
                
                // 只处理在同一行的范围
                if (rangeStartLine == lineNum && rangeEndLine == lineNum) {
                    // 计算范围的显示位置
                    unsigned rangeStartDisplay = columnToDisplay(rangeStartCol);
                    unsigned rangeEndDisplay = columnToDisplay(rangeEndCol);
                    unsigned maxDisplay = static_cast<unsigned>(expandedLine.size());
                    if (rangeStartDisplay > maxDisplay) {
                        rangeStartDisplay = maxDisplay;
                    }
                    if (rangeEndDisplay > maxDisplay) {
                        rangeEndDisplay = maxDisplay;
                    }
                    if (rangeEndDisplay <= rangeStartDisplay) {
                        rangeEndDisplay = std::min(rangeStartDisplay + 1, maxDisplay + 1);
                    }
                    unsigned caretPos = displayCol;
                    if (caretPos < rangeStartDisplay || caretPos >= rangeEndDisplay) {
                        caretPos = rangeStartDisplay;
                    }
                    
                    // 打印空格直到范围开始
                    for (unsigned i = 0; i < rangeStartDisplay; ++i) {
                        OS << ' ';
                    }
                    
                    // 打印波浪线覆盖范围，但在错误位置打印箭头
                    for (unsigned i = rangeStartDisplay; i < rangeEndDisplay; ++i) {
                        if (i == caretPos) {
                            OS << '^';  // 在错误位置打印箭头
                        } else {
                            OS << '~';  // 在范围内其他位置打印波浪线
                        }
                    }
                    
                    resetColor();
                    OS << "\n";
                    return;
                }
            }
        }
    }
    
    // 如果没有范围或范围不在同一行，使用原来的简单方式
    // 打印空格直到箭头位置
    for (unsigned i = 0; i < displayCol; ++i) {
        OS << ' ';
    }
    
    // 打印箭头
    OS << '^';
    
    // 打印波浪线表示范围（如果有）
    // 简化实现，只打印几个波浪线
    if (!ranges.empty()) {
        OS << "~~";
    }
    
    resetColor();
    OS << "\n";
}

void TextDiagnosticPrinter::printFixIts(
    const std::vector<std::pair<SourceRange, std::string>>& fixits) {
    for (const auto& [range, replacement] : fixits) {
        setColor(colors::BoldGreen);
        OS << "    fix: ";
        resetColor();
        
        if (replacement.empty()) {
            OS << "remove this code";
        } else {
            OS << "replace with '" << replacement << "'";
        }
        OS << "\n";
    }
}

void TextDiagnosticPrinter::setColor(const char* color) {
    if (UseColors) {
        OS << color;
    }
}

void TextDiagnosticPrinter::resetColor() {
    if (UseColors) {
        OS << colors::Reset;
    }
}

void TextDiagnosticPrinter::setBold() {
    if (UseColors) {
        OS << colors::Bold;
    }
}

unsigned TextDiagnosticPrinter::getColumn(SourceLocation loc) const {
    if (loc.isInvalid()) {
        return 0;
    }
    auto [line, column] = SM.getLineAndColumn(loc);
    return column;
}

unsigned TextDiagnosticPrinter::getDisplayWidth(const std::string& str, 
                                                 unsigned startCol) const {
    unsigned width = startCol;
    for (char c : str) {
        if (c == '\t') {
            width = ((width / 4) + 1) * 4;
        } else {
            ++width;
        }
    }
    return width - startCol;
}

std::string TextDiagnosticPrinter::expandTabs(const std::string& str) const {
    std::string result;
    unsigned col = 0;
    
    for (char c : str) {
        if (c == '\t') {
            unsigned spaces = 4 - (col % 4);
            result.append(spaces, ' ');
            col += spaces;
        } else {
            result += c;
            ++col;
        }
    }
    
    return result;
}

} // namespace yuan
