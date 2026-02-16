/// \file SourceManagerTest.cpp
/// \brief Unit tests for SourceManager.

#include "yuan/Basic/SourceManager.h"
#include <gtest/gtest.h>
#include <fstream>

namespace yuan {
namespace {

// ============================================================================
// Buffer Creation Tests
// ============================================================================

TEST(SourceManagerTest, CreateBufferReturnsValidFileID) {
    SourceManager sm;
    auto fid = sm.createBuffer("hello world", "test.yu");
    
    EXPECT_NE(fid, SourceManager::InvalidFileID);
}

TEST(SourceManagerTest, CreateMultipleBuffersReturnsDifferentFileIDs) {
    SourceManager sm;
    auto fid1 = sm.createBuffer("content1", "file1.yu");
    auto fid2 = sm.createBuffer("content2", "file2.yu");
    
    EXPECT_NE(fid1, SourceManager::InvalidFileID);
    EXPECT_NE(fid2, SourceManager::InvalidFileID);
    EXPECT_NE(fid1, fid2);
}

TEST(SourceManagerTest, GetBufferDataReturnsContent) {
    SourceManager sm;
    std::string content = "func main() { }";
    auto fid = sm.createBuffer(content, "test.yu");
    
    EXPECT_EQ(sm.getBufferData(fid), content);
}

TEST(SourceManagerTest, GetBufferDataWithInvalidFileIDReturnsEmpty) {
    SourceManager sm;
    EXPECT_TRUE(sm.getBufferData(SourceManager::InvalidFileID).empty());
    EXPECT_TRUE(sm.getBufferData(999).empty());
}

TEST(SourceManagerTest, GetFilenameReturnsCorrectName) {
    SourceManager sm;
    auto fid = sm.createBuffer("content", "myfile.yu");
    
    EXPECT_EQ(sm.getFilename(fid), "myfile.yu");
}

TEST(SourceManagerTest, GetFilenameWithInvalidFileIDReturnsEmpty) {
    SourceManager sm;
    EXPECT_TRUE(sm.getFilename(SourceManager::InvalidFileID).empty());
}

// ============================================================================
// Line and Column Tests
// ============================================================================

TEST(SourceManagerTest, GetLineAndColumnForFirstCharacter) {
    SourceManager sm;
    auto fid = sm.createBuffer("hello\nworld", "test.yu");
    auto loc = sm.getLocation(fid, 0);
    
    auto [line, col] = sm.getLineAndColumn(loc);
    EXPECT_EQ(line, 1u);
    EXPECT_EQ(col, 1u);
}

TEST(SourceManagerTest, GetLineAndColumnForSecondLine) {
    SourceManager sm;
    auto fid = sm.createBuffer("hello\nworld", "test.yu");
    // 'w' is at offset 6 (after "hello\n")
    auto loc = sm.getLocation(fid, 6);
    
    auto [line, col] = sm.getLineAndColumn(loc);
    EXPECT_EQ(line, 2u);
    EXPECT_EQ(col, 1u);
}

TEST(SourceManagerTest, GetLineAndColumnForMiddleOfLine) {
    SourceManager sm;
    auto fid = sm.createBuffer("hello\nworld", "test.yu");
    // 'o' in "world" is at offset 8
    auto loc = sm.getLocation(fid, 8);
    
    auto [line, col] = sm.getLineAndColumn(loc);
    EXPECT_EQ(line, 2u);
    EXPECT_EQ(col, 3u);
}

TEST(SourceManagerTest, GetLineAndColumnWithMultipleLines) {
    SourceManager sm;
    std::string content = "line1\nline2\nline3\nline4";
    auto fid = sm.createBuffer(content, "test.yu");
    
    // Test first character of each line
    auto loc1 = sm.getLocation(fid, 0);   // 'l' in line1
    auto loc2 = sm.getLocation(fid, 6);   // 'l' in line2
    auto loc3 = sm.getLocation(fid, 12);  // 'l' in line3
    auto loc4 = sm.getLocation(fid, 18);  // 'l' in line4
    
    EXPECT_EQ(sm.getLineAndColumn(loc1), std::make_pair(1u, 1u));
    EXPECT_EQ(sm.getLineAndColumn(loc2), std::make_pair(2u, 1u));
    EXPECT_EQ(sm.getLineAndColumn(loc3), std::make_pair(3u, 1u));
    EXPECT_EQ(sm.getLineAndColumn(loc4), std::make_pair(4u, 1u));
}

TEST(SourceManagerTest, GetLineAndColumnWithInvalidLocation) {
    SourceManager sm;
    sm.createBuffer("content", "test.yu");
    
    auto [line, col] = sm.getLineAndColumn(SourceLocation());
    EXPECT_EQ(line, 0u);
    EXPECT_EQ(col, 0u);
}

// ============================================================================
// Line Content Tests
// ============================================================================

TEST(SourceManagerTest, GetLineContentReturnsCorrectLine) {
    SourceManager sm;
    auto fid = sm.createBuffer("first line\nsecond line\nthird line", "test.yu");
    
    auto loc1 = sm.getLocation(fid, 0);   // In first line
    auto loc2 = sm.getLocation(fid, 11);  // In second line
    auto loc3 = sm.getLocation(fid, 23);  // In third line
    
    EXPECT_EQ(sm.getLineContent(loc1), "first line");
    EXPECT_EQ(sm.getLineContent(loc2), "second line");
    EXPECT_EQ(sm.getLineContent(loc3), "third line");
}

TEST(SourceManagerTest, GetLineContentWithWindowsLineEndings) {
    SourceManager sm;
    auto fid = sm.createBuffer("line1\r\nline2\r\nline3", "test.yu");
    
    auto loc1 = sm.getLocation(fid, 0);
    auto loc2 = sm.getLocation(fid, 7);  // After "line1\r\n"
    
    EXPECT_EQ(sm.getLineContent(loc1), "line1");
    EXPECT_EQ(sm.getLineContent(loc2), "line2");
}

TEST(SourceManagerTest, GetLineContentWithInvalidLocation) {
    SourceManager sm;
    sm.createBuffer("content", "test.yu");
    
    EXPECT_TRUE(sm.getLineContent(SourceLocation()).empty());
}

// ============================================================================
// FileID Lookup Tests
// ============================================================================

TEST(SourceManagerTest, GetFileIDReturnsCorrectFile) {
    SourceManager sm;
    auto fid1 = sm.createBuffer("content1", "file1.yu");
    auto fid2 = sm.createBuffer("content2", "file2.yu");
    
    auto loc1 = sm.getLocation(fid1, 0);
    auto loc2 = sm.getLocation(fid2, 0);
    
    EXPECT_EQ(sm.getFileID(loc1), fid1);
    EXPECT_EQ(sm.getFileID(loc2), fid2);
}

TEST(SourceManagerTest, GetFileIDWithInvalidLocationReturnsInvalid) {
    SourceManager sm;
    sm.createBuffer("content", "test.yu");
    
    EXPECT_EQ(sm.getFileID(SourceLocation()), SourceManager::InvalidFileID);
}

// ============================================================================
// Location Creation Tests
// ============================================================================

TEST(SourceManagerTest, GetLocationCreatesValidLocation) {
    SourceManager sm;
    auto fid = sm.createBuffer("hello world", "test.yu");
    
    auto loc = sm.getLocation(fid, 5);
    EXPECT_TRUE(loc.isValid());
}

TEST(SourceManagerTest, GetLocationWithInvalidFileIDReturnsInvalid) {
    SourceManager sm;
    
    auto loc = sm.getLocation(SourceManager::InvalidFileID, 0);
    EXPECT_TRUE(loc.isInvalid());
}

TEST(SourceManagerTest, GetLocationWithOutOfBoundsOffsetReturnsInvalid) {
    SourceManager sm;
    auto fid = sm.createBuffer("short", "test.yu");
    
    auto loc = sm.getLocation(fid, 1000);
    EXPECT_TRUE(loc.isInvalid());
}

TEST(SourceManagerTest, GetLocationAtEndOfFileIsValid) {
    SourceManager sm;
    std::string content = "hello";
    auto fid = sm.createBuffer(content, "test.yu");
    
    // Location at end of file (one past last char) should be valid
    auto loc = sm.getLocation(fid, static_cast<uint32_t>(content.size()));
    EXPECT_TRUE(loc.isValid());
}

// ============================================================================
// File Loading Tests
// ============================================================================

TEST(SourceManagerTest, LoadNonExistentFileReturnsInvalid) {
    SourceManager sm;
    auto fid = sm.loadFile("/nonexistent/path/to/file.yu");
    
    EXPECT_EQ(fid, SourceManager::InvalidFileID);
}

TEST(SourceManagerTest, LoadExistingFile) {
    // Create a temporary file
    const char* tempPath = "/tmp/yuan_test_file.yu";
    {
        std::ofstream ofs(tempPath);
        ofs << "func main() {\n    return 0\n}\n";
    }
    
    SourceManager sm;
    auto fid = sm.loadFile(tempPath);
    
    EXPECT_NE(fid, SourceManager::InvalidFileID);
    EXPECT_EQ(sm.getFilename(fid), tempPath);
    EXPECT_FALSE(sm.getBufferData(fid).empty());
    
    // Clean up
    std::remove(tempPath);
}

} // namespace
} // namespace yuan
