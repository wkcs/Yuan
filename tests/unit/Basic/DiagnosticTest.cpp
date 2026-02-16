/// \file DiagnosticTest.cpp
/// \brief Unit tests for the diagnostic system.

#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/DiagnosticIDs.h"
#include "yuan/Basic/TextDiagnosticPrinter.h"
#include "yuan/Basic/SourceManager.h"
#include <gtest/gtest.h>
#include <sstream>

namespace yuan {
namespace {

// ============================================================================
// DiagnosticIDs Tests
// ============================================================================

TEST(DiagnosticIDsTest, LexerErrorsHaveErrorLevel) {
    EXPECT_EQ(getDiagnosticLevel(DiagID::err_invalid_character), 
              DiagnosticLevel::Error);
    EXPECT_EQ(getDiagnosticLevel(DiagID::err_unterminated_string), 
              DiagnosticLevel::Error);
    EXPECT_EQ(getDiagnosticLevel(DiagID::err_unterminated_block_comment), 
              DiagnosticLevel::Error);
}

TEST(DiagnosticIDsTest, ParserErrorsHaveErrorLevel) {
    EXPECT_EQ(getDiagnosticLevel(DiagID::err_expected_token), 
              DiagnosticLevel::Error);
    EXPECT_EQ(getDiagnosticLevel(DiagID::err_expected_expression), 
              DiagnosticLevel::Error);
    EXPECT_EQ(getDiagnosticLevel(DiagID::err_unexpected_token), 
              DiagnosticLevel::Error);
}

TEST(DiagnosticIDsTest, SemanticErrorsHaveErrorLevel) {
    EXPECT_EQ(getDiagnosticLevel(DiagID::err_undeclared_identifier), 
              DiagnosticLevel::Error);
    EXPECT_EQ(getDiagnosticLevel(DiagID::err_type_mismatch), 
              DiagnosticLevel::Error);
    EXPECT_EQ(getDiagnosticLevel(DiagID::err_cannot_assign_to_const), 
              DiagnosticLevel::Error);
}

TEST(DiagnosticIDsTest, WarningsHaveWarningLevel) {
    EXPECT_EQ(getDiagnosticLevel(DiagID::warn_unused_variable), 
              DiagnosticLevel::Warning);
    EXPECT_EQ(getDiagnosticLevel(DiagID::warn_unreachable_code), 
              DiagnosticLevel::Warning);
    EXPECT_EQ(getDiagnosticLevel(DiagID::warn_implicit_conversion), 
              DiagnosticLevel::Warning);
}

TEST(DiagnosticIDsTest, NotesHaveNoteLevel) {
    EXPECT_EQ(getDiagnosticLevel(DiagID::note_declared_here), 
              DiagnosticLevel::Note);
    EXPECT_EQ(getDiagnosticLevel(DiagID::note_previous_definition), 
              DiagnosticLevel::Note);
    EXPECT_EQ(getDiagnosticLevel(DiagID::note_did_you_mean), 
              DiagnosticLevel::Note);
}

TEST(DiagnosticIDsTest, ErrorCodeFormat) {
    // Error codes should have format E/W/N followed by 4 digits
    EXPECT_EQ(getDiagnosticCode(DiagID::err_invalid_character), "E1001");
    EXPECT_EQ(getDiagnosticCode(DiagID::err_expected_token), "E2001");
    EXPECT_EQ(getDiagnosticCode(DiagID::err_undeclared_identifier), "E3001");
    EXPECT_EQ(getDiagnosticCode(DiagID::warn_unused_variable), "W4001");
    EXPECT_EQ(getDiagnosticCode(DiagID::note_declared_here), "N5001");
}

TEST(DiagnosticIDsTest, FormatStringsExist) {
    // All diagnostic IDs should have format strings
    EXPECT_NE(getDiagnosticFormatString(DiagID::err_invalid_character), nullptr);
    EXPECT_NE(getDiagnosticFormatString(DiagID::err_type_mismatch), nullptr);
    EXPECT_NE(getDiagnosticFormatString(DiagID::warn_unused_variable), nullptr);
    EXPECT_NE(getDiagnosticFormatString(DiagID::note_declared_here), nullptr);
}

TEST(DiagnosticIDsTest, IsErrorHelper) {
    EXPECT_TRUE(isError(DiagID::err_invalid_character));
    EXPECT_TRUE(isError(DiagID::err_type_mismatch));
    EXPECT_FALSE(isError(DiagID::warn_unused_variable));
    EXPECT_FALSE(isError(DiagID::note_declared_here));
}

TEST(DiagnosticIDsTest, IsWarningHelper) {
    EXPECT_FALSE(isWarning(DiagID::err_invalid_character));
    EXPECT_TRUE(isWarning(DiagID::warn_unused_variable));
    EXPECT_TRUE(isWarning(DiagID::warn_unreachable_code));
    EXPECT_FALSE(isWarning(DiagID::note_declared_here));
}

TEST(DiagnosticIDsTest, IsNoteHelper) {
    EXPECT_FALSE(isNote(DiagID::err_invalid_character));
    EXPECT_FALSE(isNote(DiagID::warn_unused_variable));
    EXPECT_TRUE(isNote(DiagID::note_declared_here));
    EXPECT_TRUE(isNote(DiagID::note_did_you_mean));
}

// ============================================================================
// Diagnostic Tests
// ============================================================================

TEST(DiagnosticTest, BasicConstruction) {
    SourceLocation loc(100);
    Diagnostic diag(DiagID::err_invalid_character, DiagnosticLevel::Error, loc);
    
    EXPECT_EQ(diag.getID(), DiagID::err_invalid_character);
    EXPECT_EQ(diag.getLevel(), DiagnosticLevel::Error);
    EXPECT_EQ(diag.getLocation().getOffset(), 100u);
}

TEST(DiagnosticTest, AddStringArgument) {
    SourceLocation loc(100);
    Diagnostic diag(DiagID::err_invalid_character, DiagnosticLevel::Error, loc);
    
    diag << "@";
    
    EXPECT_EQ(diag.getArgs().size(), 1u);
    EXPECT_EQ(diag.getArgs()[0], "@");
}

TEST(DiagnosticTest, AddMultipleArguments) {
    SourceLocation loc(100);
    Diagnostic diag(DiagID::err_type_mismatch, DiagnosticLevel::Error, loc);
    
    diag << "i32" << "str";
    
    EXPECT_EQ(diag.getArgs().size(), 2u);
    EXPECT_EQ(diag.getArgs()[0], "i32");
    EXPECT_EQ(diag.getArgs()[1], "str");
}

TEST(DiagnosticTest, AddIntegerArgument) {
    SourceLocation loc(100);
    Diagnostic diag(DiagID::err_wrong_argument_count, DiagnosticLevel::Error, loc);
    
    diag << 3 << 5;
    
    EXPECT_EQ(diag.getArgs().size(), 2u);
    EXPECT_EQ(diag.getArgs()[0], "3");
    EXPECT_EQ(diag.getArgs()[1], "5");
}

TEST(DiagnosticTest, MessageFormatting) {
    SourceLocation loc(100);
    Diagnostic diag(DiagID::err_invalid_character, DiagnosticLevel::Error, loc);
    
    diag << "@";
    
    std::string msg = diag.getMessage();
    EXPECT_EQ(msg, "invalid character '@'");
}

TEST(DiagnosticTest, MessageFormattingMultipleArgs) {
    SourceLocation loc(100);
    Diagnostic diag(DiagID::err_type_mismatch, DiagnosticLevel::Error, loc);
    
    diag << "i32" << "str";
    
    std::string msg = diag.getMessage();
    EXPECT_EQ(msg, "type mismatch: expected 'i32', found 'str'");
}

TEST(DiagnosticTest, AddFixIt) {
    SourceLocation loc(100);
    Diagnostic diag(DiagID::err_undeclared_identifier, DiagnosticLevel::Error, loc);
    
    SourceRange range(SourceLocation(100), SourceLocation(103));
    diag.addFixIt(range, "bar");
    
    EXPECT_TRUE(diag.hasFixIts());
    EXPECT_EQ(diag.getFixIts().size(), 1u);
    EXPECT_EQ(diag.getFixIts()[0].second, "bar");
}

TEST(DiagnosticTest, AddSourceRange) {
    SourceLocation loc(100);
    Diagnostic diag(DiagID::err_undeclared_identifier, DiagnosticLevel::Error, loc);
    
    SourceRange range(SourceLocation(100), SourceLocation(103));
    diag << range;
    
    EXPECT_EQ(diag.getRanges().size(), 1u);
    EXPECT_EQ(diag.getRanges()[0].getBegin().getOffset(), 100u);
    EXPECT_EQ(diag.getRanges()[0].getEnd().getOffset(), 103u);
}

TEST(DiagnosticTest, GetCode) {
    SourceLocation loc(100);
    Diagnostic diag(DiagID::err_invalid_character, DiagnosticLevel::Error, loc);
    
    EXPECT_EQ(diag.getCode(), "E1001");
}

// ============================================================================
// DiagnosticEngine Tests
// ============================================================================

TEST(DiagnosticEngineTest, BasicConstruction) {
    SourceManager sm;
    DiagnosticEngine engine(sm);
    
    EXPECT_EQ(engine.getErrorCount(), 0u);
    EXPECT_EQ(engine.getWarningCount(), 0u);
    EXPECT_FALSE(engine.hasErrors());
}

TEST(DiagnosticEngineTest, ReportError) {
    SourceManager sm;
    DiagnosticEngine engine(sm);
    
    engine.report(DiagID::err_invalid_character, SourceLocation(100)) << "@";
    
    EXPECT_EQ(engine.getErrorCount(), 1u);
    EXPECT_TRUE(engine.hasErrors());
}

TEST(DiagnosticEngineTest, ReportWarning) {
    SourceManager sm;
    DiagnosticEngine engine(sm);
    
    engine.report(DiagID::warn_unused_variable, SourceLocation(100)) << "x";
    
    EXPECT_EQ(engine.getWarningCount(), 1u);
    EXPECT_EQ(engine.getErrorCount(), 0u);
    EXPECT_FALSE(engine.hasErrors());
}

TEST(DiagnosticEngineTest, ReportNote) {
    SourceManager sm;
    DiagnosticEngine engine(sm);
    
    engine.report(DiagID::note_declared_here, SourceLocation(100)) << "x";
    
    // Notes don't count as errors or warnings
    EXPECT_EQ(engine.getErrorCount(), 0u);
    EXPECT_EQ(engine.getWarningCount(), 0u);
}

TEST(DiagnosticEngineTest, WarningsAsErrors) {
    SourceManager sm;
    DiagnosticEngine engine(sm);
    engine.setWarningsAsErrors(true);
    
    engine.report(DiagID::warn_unused_variable, SourceLocation(100)) << "x";
    
    // Warning should be promoted to error
    EXPECT_EQ(engine.getErrorCount(), 1u);
    EXPECT_TRUE(engine.hasErrors());
}

TEST(DiagnosticEngineTest, Reset) {
    SourceManager sm;
    DiagnosticEngine engine(sm);
    
    engine.report(DiagID::err_invalid_character, SourceLocation(100)) << "@";
    engine.report(DiagID::warn_unused_variable, SourceLocation(200)) << "x";
    
    EXPECT_EQ(engine.getErrorCount(), 1u);
    EXPECT_EQ(engine.getWarningCount(), 1u);
    
    engine.reset();
    
    EXPECT_EQ(engine.getErrorCount(), 0u);
    EXPECT_EQ(engine.getWarningCount(), 0u);
    EXPECT_FALSE(engine.hasErrors());
}

TEST(DiagnosticEngineTest, ErrorLimit) {
    SourceManager sm;
    DiagnosticEngine engine(sm);
    engine.setErrorLimit(2);
    
    engine.report(DiagID::err_invalid_character, SourceLocation(100)) << "@";
    EXPECT_FALSE(engine.hasReachedErrorLimit());
    
    engine.report(DiagID::err_invalid_character, SourceLocation(200)) << "#";
    EXPECT_TRUE(engine.hasReachedErrorLimit());
}

// ============================================================================
// StoredDiagnosticConsumer Tests
// ============================================================================

TEST(StoredDiagnosticConsumerTest, StoresDiagnostics) {
    StoredDiagnosticConsumer consumer;
    
    Diagnostic diag1(DiagID::err_invalid_character, DiagnosticLevel::Error, 
                     SourceLocation(100));
    diag1 << "@";
    
    Diagnostic diag2(DiagID::warn_unused_variable, DiagnosticLevel::Warning,
                     SourceLocation(200));
    diag2 << "x";
    
    consumer.handleDiagnostic(diag1);
    consumer.handleDiagnostic(diag2);
    
    EXPECT_EQ(consumer.getDiagnostics().size(), 2u);
    EXPECT_EQ(consumer.getDiagnostics()[0].getID(), DiagID::err_invalid_character);
    EXPECT_EQ(consumer.getDiagnostics()[1].getID(), DiagID::warn_unused_variable);
}

TEST(StoredDiagnosticConsumerTest, Clear) {
    StoredDiagnosticConsumer consumer;
    
    Diagnostic diag(DiagID::err_invalid_character, DiagnosticLevel::Error,
                    SourceLocation(100));
    consumer.handleDiagnostic(diag);
    
    EXPECT_EQ(consumer.getDiagnostics().size(), 1u);
    
    consumer.clear();
    
    EXPECT_EQ(consumer.getDiagnostics().size(), 0u);
}

// ============================================================================
// TextDiagnosticPrinter Tests
// ============================================================================

TEST(TextDiagnosticPrinterTest, PrintsErrorWithLocation) {
    SourceManager sm;
    auto fid = sm.createBuffer("var x = @invalid\n", "test.yu");
    
    std::ostringstream oss;
    TextDiagnosticPrinter printer(oss, sm, false);  // No colors for testing
    
    // Create a diagnostic at the @ character (offset 8)
    SourceLocation loc = sm.getLocation(fid, 8);
    Diagnostic diag(DiagID::err_invalid_character, DiagnosticLevel::Error, loc);
    diag << "@";
    
    printer.handleDiagnostic(diag);
    
    std::string output = oss.str();
    
    // Check that output contains expected elements
    EXPECT_NE(output.find("test.yu"), std::string::npos);
    EXPECT_NE(output.find("1:9"), std::string::npos);  // Line 1, column 9
    EXPECT_NE(output.find("error"), std::string::npos);
    EXPECT_NE(output.find("E1001"), std::string::npos);
    EXPECT_NE(output.find("invalid character '@'"), std::string::npos);
}

TEST(TextDiagnosticPrinterTest, PrintsWarning) {
    SourceManager sm;
    auto fid = sm.createBuffer("var unused = 42\n", "test.yu");
    
    std::ostringstream oss;
    TextDiagnosticPrinter printer(oss, sm, false);
    
    SourceLocation loc = sm.getLocation(fid, 4);
    Diagnostic diag(DiagID::warn_unused_variable, DiagnosticLevel::Warning, loc);
    diag << "unused";
    
    printer.handleDiagnostic(diag);
    
    std::string output = oss.str();
    
    EXPECT_NE(output.find("warning"), std::string::npos);
    EXPECT_NE(output.find("W4001"), std::string::npos);
    EXPECT_NE(output.find("unused variable 'unused'"), std::string::npos);
}

TEST(TextDiagnosticPrinterTest, PrintsNote) {
    SourceManager sm;
    auto fid = sm.createBuffer("var foo = 10\n", "test.yu");
    
    std::ostringstream oss;
    TextDiagnosticPrinter printer(oss, sm, false);
    
    SourceLocation loc = sm.getLocation(fid, 4);
    Diagnostic diag(DiagID::note_declared_here, DiagnosticLevel::Note, loc);
    diag << "foo";
    
    printer.handleDiagnostic(diag);
    
    std::string output = oss.str();
    
    EXPECT_NE(output.find("note"), std::string::npos);
    EXPECT_NE(output.find("N5001"), std::string::npos);
    EXPECT_NE(output.find("'foo' declared here"), std::string::npos);
}

TEST(TextDiagnosticPrinterTest, PrintsSourceLine) {
    SourceManager sm;
    auto fid = sm.createBuffer("var x = foo + 1\n", "test.yu");
    
    std::ostringstream oss;
    TextDiagnosticPrinter printer(oss, sm, false);
    
    SourceLocation loc = sm.getLocation(fid, 8);  // 'foo'
    Diagnostic diag(DiagID::err_undeclared_identifier, DiagnosticLevel::Error, loc);
    diag << "foo";
    
    printer.handleDiagnostic(diag);
    
    std::string output = oss.str();
    
    // Should contain the source line
    EXPECT_NE(output.find("var x = foo + 1"), std::string::npos);
    // Should contain caret
    EXPECT_NE(output.find("^"), std::string::npos);
}

TEST(TextDiagnosticPrinterTest, PrintsFixIt) {
    SourceManager sm;
    auto fid = sm.createBuffer("var x = fo\n", "test.yu");
    
    std::ostringstream oss;
    TextDiagnosticPrinter printer(oss, sm, false);
    
    SourceLocation loc = sm.getLocation(fid, 8);
    Diagnostic diag(DiagID::err_undeclared_identifier, DiagnosticLevel::Error, loc);
    diag << "fo";
    
    SourceRange range(loc, SourceLocation(loc.getOffset() + 2));
    diag.addFixIt(range, "foo");
    
    printer.handleDiagnostic(diag);
    
    std::string output = oss.str();
    
    EXPECT_NE(output.find("fix:"), std::string::npos);
    EXPECT_NE(output.find("foo"), std::string::npos);
}

TEST(TextDiagnosticPrinterTest, DisableErrorCodes) {
    SourceManager sm;
    auto fid = sm.createBuffer("var x = @\n", "test.yu");
    
    std::ostringstream oss;
    TextDiagnosticPrinter printer(oss, sm, false);
    printer.setShowErrorCodes(false);
    
    SourceLocation loc = sm.getLocation(fid, 8);
    Diagnostic diag(DiagID::err_invalid_character, DiagnosticLevel::Error, loc);
    diag << "@";
    
    printer.handleDiagnostic(diag);
    
    std::string output = oss.str();
    
    // Should not contain error code
    EXPECT_EQ(output.find("E1001"), std::string::npos);
}

TEST(TextDiagnosticPrinterTest, DisableSourceLine) {
    SourceManager sm;
    auto fid = sm.createBuffer("var x = @\n", "test.yu");
    
    std::ostringstream oss;
    TextDiagnosticPrinter printer(oss, sm, false);
    printer.setShowSourceLine(false);
    
    SourceLocation loc = sm.getLocation(fid, 8);
    Diagnostic diag(DiagID::err_invalid_character, DiagnosticLevel::Error, loc);
    diag << "@";
    
    printer.handleDiagnostic(diag);
    
    std::string output = oss.str();
    
    // Should not contain source line or caret
    EXPECT_EQ(output.find("var x = @"), std::string::npos);
    EXPECT_EQ(output.find("^"), std::string::npos);
}

TEST(TextDiagnosticPrinterTest, HandlesInvalidLocation) {
    SourceManager sm;
    
    std::ostringstream oss;
    TextDiagnosticPrinter printer(oss, sm, false);
    
    Diagnostic diag(DiagID::err_invalid_character, DiagnosticLevel::Error,
                    SourceLocation());  // Invalid location
    diag << "@";
    
    printer.handleDiagnostic(diag);
    
    std::string output = oss.str();
    
    // Should still print something
    EXPECT_NE(output.find("error"), std::string::npos);
    EXPECT_NE(output.find("invalid character '@'"), std::string::npos);
}

// ============================================================================
// MultiplexDiagnosticConsumer Tests
// ============================================================================

TEST(MultiplexDiagnosticConsumerTest, ForwardsToMultipleConsumers) {
    auto stored1 = std::make_unique<StoredDiagnosticConsumer>();
    auto stored2 = std::make_unique<StoredDiagnosticConsumer>();
    
    StoredDiagnosticConsumer* ptr1 = stored1.get();
    StoredDiagnosticConsumer* ptr2 = stored2.get();
    
    MultiplexDiagnosticConsumer multiplex;
    multiplex.addConsumer(std::move(stored1));
    multiplex.addConsumer(std::move(stored2));
    
    Diagnostic diag(DiagID::err_invalid_character, DiagnosticLevel::Error,
                    SourceLocation(100));
    diag << "@";
    
    multiplex.handleDiagnostic(diag);
    
    EXPECT_EQ(ptr1->getDiagnostics().size(), 1u);
    EXPECT_EQ(ptr2->getDiagnostics().size(), 1u);
}

} // namespace
} // namespace yuan
