/// \file DiagnosticPropertyTest.cpp
/// \brief Property-based tests for the diagnostic system.
///
/// **Property 17: 错误报告格式**
/// **Validates: Requirements 12.1-12.5**
///
/// This test validates that the diagnostic system correctly formats and
/// outputs error messages with all required components: file location,
/// severity level, error code, message, source line, and caret indicator.

#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/DiagnosticIDs.h"
#include "yuan/Basic/TextDiagnosticPrinter.h"
#include "yuan/Basic/SourceManager.h"
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

namespace yuan {
namespace {

// Number of iterations for property-based tests
constexpr int NUM_ITERATIONS = 100;

// Random number generator for property tests
class DiagnosticRandomGenerator {
public:
    DiagnosticRandomGenerator() : gen(std::random_device{}()) {}
    
    /// Generate random source code with specified number of lines
    std::string randomSourceCode(size_t numLines, size_t maxLineLength = 80) {
        std::string result;
        std::uniform_int_distribution<size_t> lineLenDist(10, maxLineLength);
        std::uniform_int_distribution<int> charDist('a', 'z');
        
        for (size_t i = 0; i < numLines; ++i) {
            size_t lineLen = lineLenDist(gen);
            for (size_t j = 0; j < lineLen; ++j) {
                result += static_cast<char>(charDist(gen));
            }
            if (i < numLines - 1) {
                result += '\n';
            }
        }
        return result;
    }
    
    /// Generate a random filename
    std::string randomFilename() {
        std::uniform_int_distribution<int> charDist('a', 'z');
        std::uniform_int_distribution<size_t> lenDist(3, 10);
        
        std::string name;
        size_t len = lenDist(gen);
        for (size_t i = 0; i < len; ++i) {
            name += static_cast<char>(charDist(gen));
        }
        return name + ".yu";
    }
    
    /// Generate a random identifier
    std::string randomIdentifier() {
        std::uniform_int_distribution<int> charDist('a', 'z');
        std::uniform_int_distribution<size_t> lenDist(1, 15);
        
        std::string name;
        size_t len = lenDist(gen);
        for (size_t i = 0; i < len; ++i) {
            name += static_cast<char>(charDist(gen));
        }
        return name;
    }
    
    /// Generate a random type name
    std::string randomTypeName() {
        static const std::vector<std::string> types = {
            "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64",
            "f32", "f64", "bool", "char", "str", "void"
        };
        std::uniform_int_distribution<size_t> dist(0, types.size() - 1);
        return types[dist(gen)];
    }
    
    /// Pick a random error DiagID
    DiagID randomErrorID() {
        static const std::vector<DiagID> errorIDs = {
            DiagID::err_invalid_character,
            DiagID::err_unterminated_string,
            DiagID::err_expected_token,
            DiagID::err_undeclared_identifier,
            DiagID::err_type_mismatch,
            DiagID::err_cannot_assign_to_const,
            DiagID::err_wrong_argument_count,
        };
        std::uniform_int_distribution<size_t> dist(0, errorIDs.size() - 1);
        return errorIDs[dist(gen)];
    }
    
    /// Pick a random warning DiagID
    DiagID randomWarningID() {
        static const std::vector<DiagID> warningIDs = {
            DiagID::warn_unused_variable,
            DiagID::warn_unreachable_code,
            DiagID::warn_implicit_conversion,
            DiagID::warn_unused_import,
            DiagID::warn_shadowed_variable,
        };
        std::uniform_int_distribution<size_t> dist(0, warningIDs.size() - 1);
        return warningIDs[dist(gen)];
    }
    
    /// Pick a random note DiagID
    DiagID randomNoteID() {
        static const std::vector<DiagID> noteIDs = {
            DiagID::note_declared_here,
            DiagID::note_previous_definition,
            DiagID::note_did_you_mean,
            DiagID::note_type_is,
        };
        std::uniform_int_distribution<size_t> dist(0, noteIDs.size() - 1);
        return noteIDs[dist(gen)];
    }
    
    /// Generate a random offset within bounds
    size_t randomOffset(size_t max) {
        if (max == 0) return 0;
        std::uniform_int_distribution<size_t> dist(0, max - 1);
        return dist(gen);
    }
    
    /// Generate a random line number
    unsigned randomLineNumber(unsigned max = 100) {
        std::uniform_int_distribution<unsigned> dist(1, max);
        return dist(gen);
    }
    
    /// Generate a random column number
    unsigned randomColumnNumber(unsigned max = 80) {
        std::uniform_int_distribution<unsigned> dist(1, max);
        return dist(gen);
    }
    
private:
    std::mt19937 gen;
};

// ============================================================================
// Property 17: 错误报告格式
// For any compilation error, the error report must include:
// - File name (Requirements 12.2)
// - Line number (Requirements 12.2)
// - Column number (Requirements 12.2)
// - Error message (Requirements 12.1)
// - Source code line (Requirements 12.3)
// - Position indicator (^) (Requirements 12.4)
// ============================================================================

/// Property: For any error diagnostic with a valid location, the output
/// must contain the filename.
TEST(DiagnosticPropertyTest, OutputContainsFilename) {
    // Feature: yuan-compiler, Property 17: 错误报告格式
    // Validates: Requirements 12.2
    
    DiagnosticRandomGenerator rng;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        SourceManager sm;
        
        std::string filename = rng.randomFilename();
        std::string content = rng.randomSourceCode(5);
        
        if (content.empty()) continue;
        
        auto fid = sm.createBuffer(content, filename);
        
        std::ostringstream oss;
        TextDiagnosticPrinter printer(oss, sm, false);
        
        size_t offset = rng.randomOffset(content.size());
        SourceLocation loc = sm.getLocation(fid, static_cast<uint32_t>(offset));
        
        DiagID diagID = rng.randomErrorID();
        Diagnostic diag(diagID, DiagnosticLevel::Error, loc);
        
        // Add appropriate arguments based on the diagnostic
        switch (diagID) {
            case DiagID::err_invalid_character:
                diag << "@";
                break;
            case DiagID::err_undeclared_identifier:
                diag << rng.randomIdentifier();
                break;
            case DiagID::err_type_mismatch:
                diag << rng.randomTypeName() << rng.randomTypeName();
                break;
            case DiagID::err_cannot_assign_to_const:
                diag << rng.randomIdentifier();
                break;
            case DiagID::err_wrong_argument_count:
                diag << 3 << 5;
                break;
            default:
                break;
        }
        
        printer.handleDiagnostic(diag);
        
        std::string output = oss.str();
        
        // Property: Output must contain the filename
        EXPECT_NE(output.find(filename), std::string::npos)
            << "Output should contain filename '" << filename << "'\n"
            << "Output was: " << output;
    }
}

/// Property: For any error diagnostic with a valid location, the output
/// must contain the line number.
TEST(DiagnosticPropertyTest, OutputContainsLineNumber) {
    // Feature: yuan-compiler, Property 17: 错误报告格式
    // Validates: Requirements 12.2
    
    DiagnosticRandomGenerator rng;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        SourceManager sm;
        
        std::string content = rng.randomSourceCode(10);
        if (content.empty()) continue;
        
        auto fid = sm.createBuffer(content, "test.yu");
        
        std::ostringstream oss;
        TextDiagnosticPrinter printer(oss, sm, false);
        
        size_t offset = rng.randomOffset(content.size());
        SourceLocation loc = sm.getLocation(fid, static_cast<uint32_t>(offset));
        
        auto [expectedLine, expectedCol] = sm.getLineAndColumn(loc);
        
        Diagnostic diag(DiagID::err_undeclared_identifier, DiagnosticLevel::Error, loc);
        diag << "foo";
        
        printer.handleDiagnostic(diag);
        
        std::string output = oss.str();
        
        // Property: Output must contain the line number
        std::string lineStr = std::to_string(expectedLine);
        EXPECT_NE(output.find(lineStr), std::string::npos)
            << "Output should contain line number " << expectedLine << "\n"
            << "Output was: " << output;
    }
}

/// Property: For any error diagnostic with a valid location, the output
/// must contain the column number.
TEST(DiagnosticPropertyTest, OutputContainsColumnNumber) {
    // Feature: yuan-compiler, Property 17: 错误报告格式
    // Validates: Requirements 12.2
    
    DiagnosticRandomGenerator rng;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        SourceManager sm;
        
        std::string content = rng.randomSourceCode(5);
        if (content.empty()) continue;
        
        auto fid = sm.createBuffer(content, "test.yu");
        
        std::ostringstream oss;
        TextDiagnosticPrinter printer(oss, sm, false);
        
        size_t offset = rng.randomOffset(content.size());
        SourceLocation loc = sm.getLocation(fid, static_cast<uint32_t>(offset));
        
        auto [expectedLine, expectedCol] = sm.getLineAndColumn(loc);
        
        Diagnostic diag(DiagID::err_undeclared_identifier, DiagnosticLevel::Error, loc);
        diag << "foo";
        
        printer.handleDiagnostic(diag);
        
        std::string output = oss.str();
        
        // Property: Output must contain the column number
        // The format is filename:line:column:
        std::string locationPattern = ":" + std::to_string(expectedLine) + 
                                      ":" + std::to_string(expectedCol) + ":";
        EXPECT_NE(output.find(locationPattern), std::string::npos)
            << "Output should contain location pattern '" << locationPattern << "'\n"
            << "Output was: " << output;
    }
}

/// Property: For any error diagnostic, the output must contain the
/// severity level indicator ("error", "warning", or "note").
TEST(DiagnosticPropertyTest, OutputContainsSeverityLevel) {
    // Feature: yuan-compiler, Property 17: 错误报告格式
    // Validates: Requirements 12.1, 12.5
    
    DiagnosticRandomGenerator rng;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        SourceManager sm;
        
        std::string content = rng.randomSourceCode(3);
        if (content.empty()) continue;
        
        auto fid = sm.createBuffer(content, "test.yu");
        
        size_t offset = rng.randomOffset(content.size());
        SourceLocation loc = sm.getLocation(fid, static_cast<uint32_t>(offset));
        
        // Test each severity level
        struct TestCase {
            DiagID id;
            DiagnosticLevel level;
            std::string expectedText;
        };
        
        std::vector<TestCase> testCases = {
            {rng.randomErrorID(), DiagnosticLevel::Error, "error"},
            {rng.randomWarningID(), DiagnosticLevel::Warning, "warning"},
            {rng.randomNoteID(), DiagnosticLevel::Note, "note"},
        };
        
        for (const auto& tc : testCases) {
            std::ostringstream oss;
            TextDiagnosticPrinter printer(oss, sm, false);
            
            Diagnostic diag(tc.id, tc.level, loc);
            
            // Add a generic argument
            diag << "test";
            
            printer.handleDiagnostic(diag);
            
            std::string output = oss.str();
            
            // Property: Output must contain the severity level
            EXPECT_NE(output.find(tc.expectedText), std::string::npos)
                << "Output should contain '" << tc.expectedText << "'\n"
                << "Output was: " << output;
        }
    }
}

/// Property: For any error diagnostic with a valid location, the output
/// must contain the source code line.
TEST(DiagnosticPropertyTest, OutputContainsSourceLine) {
    // Feature: yuan-compiler, Property 17: 错误报告格式
    // Validates: Requirements 12.3
    
    DiagnosticRandomGenerator rng;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        SourceManager sm;
        
        // Create source with known content
        std::string line1 = "var x = 42";
        std::string line2 = "var y = foo";
        std::string line3 = "return x + y";
        std::string content = line1 + "\n" + line2 + "\n" + line3;
        
        auto fid = sm.createBuffer(content, "test.yu");
        
        std::ostringstream oss;
        TextDiagnosticPrinter printer(oss, sm, false);
        
        // Point to 'foo' on line 2 (offset = line1.size() + 1 + 8)
        size_t offset = line1.size() + 1 + 8;  // "var y = " is 8 chars
        SourceLocation loc = sm.getLocation(fid, static_cast<uint32_t>(offset));
        
        Diagnostic diag(DiagID::err_undeclared_identifier, DiagnosticLevel::Error, loc);
        diag << "foo";
        
        printer.handleDiagnostic(diag);
        
        std::string output = oss.str();
        
        // Property: Output must contain the source line
        EXPECT_NE(output.find(line2), std::string::npos)
            << "Output should contain source line '" << line2 << "'\n"
            << "Output was: " << output;
    }
}

/// Property: For any error diagnostic with a valid location, the output
/// must contain a caret (^) indicator.
TEST(DiagnosticPropertyTest, OutputContainsCaretIndicator) {
    // Feature: yuan-compiler, Property 17: 错误报告格式
    // Validates: Requirements 12.4
    
    DiagnosticRandomGenerator rng;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        SourceManager sm;
        
        std::string content = rng.randomSourceCode(5);
        if (content.empty()) continue;
        
        auto fid = sm.createBuffer(content, "test.yu");
        
        std::ostringstream oss;
        TextDiagnosticPrinter printer(oss, sm, false);
        
        size_t offset = rng.randomOffset(content.size());
        SourceLocation loc = sm.getLocation(fid, static_cast<uint32_t>(offset));
        
        Diagnostic diag(DiagID::err_undeclared_identifier, DiagnosticLevel::Error, loc);
        diag << "foo";
        
        printer.handleDiagnostic(diag);
        
        std::string output = oss.str();
        
        // Property: Output must contain caret indicator
        EXPECT_NE(output.find("^"), std::string::npos)
            << "Output should contain caret indicator '^'\n"
            << "Output was: " << output;
    }
}

/// Property: For any error diagnostic, the output must contain the
/// error code in the format [EXXXX] or [WXXXX] or [NXXXX].
TEST(DiagnosticPropertyTest, OutputContainsErrorCode) {
    // Feature: yuan-compiler, Property 17: 错误报告格式
    // Validates: Requirements 12.8
    
    DiagnosticRandomGenerator rng;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        SourceManager sm;
        
        std::string content = rng.randomSourceCode(3);
        if (content.empty()) continue;
        
        auto fid = sm.createBuffer(content, "test.yu");
        
        size_t offset = rng.randomOffset(content.size());
        SourceLocation loc = sm.getLocation(fid, static_cast<uint32_t>(offset));
        
        std::ostringstream oss;
        TextDiagnosticPrinter printer(oss, sm, false);
        
        DiagID diagID = rng.randomErrorID();
        Diagnostic diag(diagID, DiagnosticLevel::Error, loc);
        diag << "test";
        
        printer.handleDiagnostic(diag);
        
        std::string output = oss.str();
        std::string expectedCode = getDiagnosticCode(diagID);
        
        // Property: Output must contain the error code
        EXPECT_NE(output.find(expectedCode), std::string::npos)
            << "Output should contain error code '" << expectedCode << "'\n"
            << "Output was: " << output;
    }
}

/// Property: For any diagnostic, the formatted message must have all
/// placeholders replaced with actual arguments.
TEST(DiagnosticPropertyTest, MessagePlaceholdersReplaced) {
    // Feature: yuan-compiler, Property 17: 错误报告格式
    // Validates: Requirements 12.1
    
    DiagnosticRandomGenerator rng;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        std::string arg1 = rng.randomIdentifier();
        std::string arg2 = rng.randomTypeName();
        
        // Test type mismatch which has two placeholders
        Diagnostic diag(DiagID::err_type_mismatch, DiagnosticLevel::Error, 
                        SourceLocation(100));
        diag << arg1 << arg2;
        
        std::string message = diag.getMessage();
        
        // Property: Message should not contain unsubstituted placeholders
        EXPECT_EQ(message.find("{0}"), std::string::npos)
            << "Message should not contain '{0}'\n"
            << "Message was: " << message;
        EXPECT_EQ(message.find("{1}"), std::string::npos)
            << "Message should not contain '{1}'\n"
            << "Message was: " << message;
        
        // Property: Message should contain the actual arguments
        EXPECT_NE(message.find(arg1), std::string::npos)
            << "Message should contain argument '" << arg1 << "'\n"
            << "Message was: " << message;
        EXPECT_NE(message.find(arg2), std::string::npos)
            << "Message should contain argument '" << arg2 << "'\n"
            << "Message was: " << message;
    }
}

/// Property: Error count must increase by exactly 1 for each error reported.
TEST(DiagnosticPropertyTest, ErrorCountIncrementsCorrectly) {
    // Feature: yuan-compiler, Property 17: 错误报告格式
    // Validates: Requirements 12.6
    
    DiagnosticRandomGenerator rng;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        SourceManager sm;
        DiagnosticEngine engine(sm);
        
        // Report a random number of errors
        unsigned numErrors = 1 + static_cast<unsigned>(rng.randomOffset(20));
        
        for (unsigned e = 0; e < numErrors; ++e) {
            engine.report(rng.randomErrorID(), SourceLocation(100)) << "test";
            
            // Property: Error count must equal number of errors reported
            EXPECT_EQ(engine.getErrorCount(), e + 1)
                << "Error count should be " << (e + 1) << " after " << (e + 1) << " errors";
        }
        
        EXPECT_EQ(engine.getErrorCount(), numErrors);
        EXPECT_TRUE(engine.hasErrors());
    }
}

/// Property: Warning count must increase by exactly 1 for each warning reported.
TEST(DiagnosticPropertyTest, WarningCountIncrementsCorrectly) {
    // Feature: yuan-compiler, Property 17: 错误报告格式
    // Validates: Requirements 12.6
    
    DiagnosticRandomGenerator rng;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        SourceManager sm;
        DiagnosticEngine engine(sm);
        
        // Report a random number of warnings
        unsigned numWarnings = 1 + static_cast<unsigned>(rng.randomOffset(20));
        
        for (unsigned w = 0; w < numWarnings; ++w) {
            engine.report(rng.randomWarningID(), SourceLocation(100)) << "test";
            
            // Property: Warning count must equal number of warnings reported
            EXPECT_EQ(engine.getWarningCount(), w + 1)
                << "Warning count should be " << (w + 1) << " after " << (w + 1) << " warnings";
        }
        
        EXPECT_EQ(engine.getWarningCount(), numWarnings);
        EXPECT_FALSE(engine.hasErrors());  // Warnings don't count as errors
    }
}

/// Property: Notes should not affect error or warning counts.
TEST(DiagnosticPropertyTest, NotesDoNotAffectCounts) {
    // Feature: yuan-compiler, Property 17: 错误报告格式
    // Validates: Requirements 12.6
    
    DiagnosticRandomGenerator rng;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        SourceManager sm;
        DiagnosticEngine engine(sm);
        
        // Report a random number of notes
        unsigned numNotes = 1 + static_cast<unsigned>(rng.randomOffset(20));
        
        for (unsigned n = 0; n < numNotes; ++n) {
            engine.report(rng.randomNoteID(), SourceLocation(100)) << "test";
        }
        
        // Property: Notes should not affect error or warning counts
        EXPECT_EQ(engine.getErrorCount(), 0u);
        EXPECT_EQ(engine.getWarningCount(), 0u);
        EXPECT_FALSE(engine.hasErrors());
    }
}

/// Property: When warnings-as-errors is enabled, warnings should be
/// counted as errors.
TEST(DiagnosticPropertyTest, WarningsAsErrorsPromotesWarnings) {
    // Feature: yuan-compiler, Property 17: 错误报告格式
    // Validates: Requirements 12.5
    
    DiagnosticRandomGenerator rng;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        SourceManager sm;
        DiagnosticEngine engine(sm);
        engine.setWarningsAsErrors(true);
        
        // Report a random number of warnings
        unsigned numWarnings = 1 + static_cast<unsigned>(rng.randomOffset(10));
        
        for (unsigned w = 0; w < numWarnings; ++w) {
            engine.report(rng.randomWarningID(), SourceLocation(100)) << "test";
        }
        
        // Property: Warnings should be promoted to errors
        EXPECT_EQ(engine.getErrorCount(), numWarnings);
        EXPECT_TRUE(engine.hasErrors());
    }
}

} // namespace
} // namespace yuan
