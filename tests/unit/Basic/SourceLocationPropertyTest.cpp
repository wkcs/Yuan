/// \file SourceLocationPropertyTest.cpp
/// \brief Property-based tests for SourceLocation.
///
/// **Property 1: Lexer Token 位置不变量**
/// **Validates: Requirements 2.11**
///
/// This test validates that SourceLocation and SourceManager correctly
/// track and report source positions, which is essential for the Lexer
/// to produce tokens with accurate location information.

#include "yuan/Basic/SourceLocation.h"
#include "yuan/Basic/SourceManager.h"
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>

namespace yuan {
namespace {

// Number of iterations for property-based tests
constexpr int NUM_ITERATIONS = 100;

// Random number generator for property tests
class RandomGenerator {
public:
    RandomGenerator() : gen(std::random_device{}()) {}
    
    uint32_t randomOffset(uint32_t max = 10000) {
        std::uniform_int_distribution<uint32_t> dist(1, max);
        return dist(gen);
    }
    
    std::string randomSourceCode(size_t numLines, size_t maxLineLength = 80) {
        std::string result;
        std::uniform_int_distribution<size_t> lineLenDist(1, maxLineLength);
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
    
    size_t randomIndex(size_t max) {
        if (max == 0) return 0;
        std::uniform_int_distribution<size_t> dist(0, max - 1);
        return dist(gen);
    }
    
private:
    std::mt19937 gen;
};

// ============================================================================
// Property 1: Lexer Token 位置不变量
// For any source code input, every position created by SourceManager must
// have valid location information (file, line, column), and the location
// must accurately point to the actual position in the source code.
// ============================================================================

/// Property: For any valid offset within a file, getLocation creates a valid
/// SourceLocation, and getLineAndColumn returns consistent line/column info.
TEST(SourceLocationPropertyTest, ValidOffsetsProduceValidLocations) {
    // Feature: yuan-compiler, Property 1: Lexer Token 位置不变量
    // Validates: Requirements 2.11
    
    RandomGenerator rng;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        SourceManager sm;
        
        // Generate random source code with 1-20 lines
        size_t numLines = 1 + rng.randomIndex(20);
        std::string content = rng.randomSourceCode(numLines);
        
        auto fid = sm.createBuffer(content, "test.yu");
        ASSERT_NE(fid, SourceManager::InvalidFileID);
        
        // For any valid offset, location should be valid
        for (size_t offset = 0; offset <= content.size(); ++offset) {
            auto loc = sm.getLocation(fid, static_cast<uint32_t>(offset));
            
            // Property: Location must be valid for valid offsets
            ASSERT_TRUE(loc.isValid()) 
                << "Offset " << offset << " should produce valid location";
            
            // Property: Line and column must be non-zero (1-based)
            auto [line, col] = sm.getLineAndColumn(loc);
            ASSERT_GT(line, 0u) 
                << "Line must be >= 1 for valid location at offset " << offset;
            ASSERT_GT(col, 0u) 
                << "Column must be >= 1 for valid location at offset " << offset;
        }
    }
}

/// Property: For any position in source code, the line content retrieved
/// must contain the character at that position (unless at end of line).
TEST(SourceLocationPropertyTest, LineContentContainsPosition) {
    // Feature: yuan-compiler, Property 1: Lexer Token 位置不变量
    // Validates: Requirements 2.11
    
    RandomGenerator rng;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        SourceManager sm;
        
        // Generate random source code
        size_t numLines = 1 + rng.randomIndex(10);
        std::string content = rng.randomSourceCode(numLines);
        
        if (content.empty()) continue;
        
        auto fid = sm.createBuffer(content, "test.yu");
        
        // Pick a random position that's not a newline
        size_t offset = rng.randomIndex(content.size());
        while (offset < content.size() && content[offset] == '\n') {
            offset = rng.randomIndex(content.size());
        }
        
        if (offset >= content.size()) continue;
        
        auto loc = sm.getLocation(fid, static_cast<uint32_t>(offset));
        auto [line, col] = sm.getLineAndColumn(loc);
        std::string lineContent = sm.getLineContent(loc);
        
        // Property: The character at the position should be in the line content
        // at the correct column (col is 1-based)
        if (col <= lineContent.size()) {
            EXPECT_EQ(lineContent[col - 1], content[offset])
                << "Character mismatch at offset " << offset 
                << ", line " << line << ", col " << col;
        }
    }
}

/// Property: For any two positions in the same file, their ordering by
/// SourceLocation comparison must match their offset ordering.
TEST(SourceLocationPropertyTest, LocationOrderingMatchesOffsetOrdering) {
    // Feature: yuan-compiler, Property 1: Lexer Token 位置不变量
    // Validates: Requirements 2.11
    
    RandomGenerator rng;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        SourceManager sm;
        std::string content = rng.randomSourceCode(5);
        
        if (content.size() < 2) continue;
        
        auto fid = sm.createBuffer(content, "test.yu");
        
        // Pick two random offsets
        uint32_t offset1 = static_cast<uint32_t>(rng.randomIndex(content.size()));
        uint32_t offset2 = static_cast<uint32_t>(rng.randomIndex(content.size()));
        
        auto loc1 = sm.getLocation(fid, offset1);
        auto loc2 = sm.getLocation(fid, offset2);
        
        // Property: Location ordering must match offset ordering
        if (offset1 < offset2) {
            EXPECT_TRUE(loc1 < loc2);
            EXPECT_TRUE(loc1 <= loc2);
            EXPECT_FALSE(loc1 > loc2);
            EXPECT_FALSE(loc1 >= loc2);
            EXPECT_FALSE(loc1 == loc2);
        } else if (offset1 > offset2) {
            EXPECT_TRUE(loc1 > loc2);
            EXPECT_TRUE(loc1 >= loc2);
            EXPECT_FALSE(loc1 < loc2);
            EXPECT_FALSE(loc1 <= loc2);
            EXPECT_FALSE(loc1 == loc2);
        } else {
            EXPECT_TRUE(loc1 == loc2);
            EXPECT_TRUE(loc1 <= loc2);
            EXPECT_TRUE(loc1 >= loc2);
            EXPECT_FALSE(loc1 < loc2);
            EXPECT_FALSE(loc1 > loc2);
        }
    }
}

/// Property: For any position, getFileID must return the correct file,
/// and the position must be within that file's bounds.
TEST(SourceLocationPropertyTest, FileIDConsistency) {
    // Feature: yuan-compiler, Property 1: Lexer Token 位置不变量
    // Validates: Requirements 2.11
    
    RandomGenerator rng;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        SourceManager sm;
        
        // Create multiple files
        std::vector<SourceManager::FileID> fileIDs;
        std::vector<std::string> contents;
        
        int numFiles = 1 + static_cast<int>(rng.randomIndex(5));
        for (int f = 0; f < numFiles; ++f) {
            std::string content = rng.randomSourceCode(3);
            contents.push_back(content);
            fileIDs.push_back(sm.createBuffer(content, "file" + std::to_string(f) + ".yu"));
        }
        
        // For each file, verify positions map back to correct file
        for (int f = 0; f < numFiles; ++f) {
            if (contents[f].empty()) continue;
            
            uint32_t offset = static_cast<uint32_t>(rng.randomIndex(contents[f].size()));
            auto loc = sm.getLocation(fileIDs[f], offset);
            
            // Property: getFileID must return the same file
            EXPECT_EQ(sm.getFileID(loc), fileIDs[f])
                << "FileID mismatch for file " << f << " at offset " << offset;
        }
    }
}

/// Property: Line numbers must be monotonically increasing as we traverse
/// through newlines in the source code.
TEST(SourceLocationPropertyTest, LineNumbersIncreaseAtNewlines) {
    // Feature: yuan-compiler, Property 1: Lexer Token 位置不变量
    // Validates: Requirements 2.11
    
    RandomGenerator rng;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        SourceManager sm;
        
        // Generate source with known newline positions
        size_t numLines = 2 + rng.randomIndex(10);
        std::string content = rng.randomSourceCode(numLines);
        
        auto fid = sm.createBuffer(content, "test.yu");
        
        unsigned lastLine = 0;
        for (size_t offset = 0; offset < content.size(); ++offset) {
            auto loc = sm.getLocation(fid, static_cast<uint32_t>(offset));
            auto [line, col] = sm.getLineAndColumn(loc);
            
            // Property: Line number must never decrease
            ASSERT_GE(line, lastLine)
                << "Line number decreased at offset " << offset;
            
            // After a newline, line number should increase
            if (offset > 0 && content[offset - 1] == '\n') {
                EXPECT_GT(line, lastLine)
                    << "Line number should increase after newline at offset " << offset;
            }
            
            lastLine = line;
        }
    }
}

/// Property: Column numbers must reset to 1 at the start of each line.
TEST(SourceLocationPropertyTest, ColumnResetsAtLineStart) {
    // Feature: yuan-compiler, Property 1: Lexer Token 位置不变量
    // Validates: Requirements 2.11
    
    RandomGenerator rng;
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        SourceManager sm;
        
        size_t numLines = 2 + rng.randomIndex(10);
        std::string content = rng.randomSourceCode(numLines);
        
        auto fid = sm.createBuffer(content, "test.yu");
        
        for (size_t offset = 0; offset < content.size(); ++offset) {
            // Check positions right after newlines
            if (offset > 0 && content[offset - 1] == '\n') {
                auto loc = sm.getLocation(fid, static_cast<uint32_t>(offset));
                auto [line, col] = sm.getLineAndColumn(loc);
                
                // Property: Column must be 1 at start of line
                EXPECT_EQ(col, 1u)
                    << "Column should be 1 at start of line " << line 
                    << " (offset " << offset << ")";
            }
        }
        
        // Also check first character
        if (!content.empty()) {
            auto loc = sm.getLocation(fid, 0);
            auto [line, col] = sm.getLineAndColumn(loc);
            EXPECT_EQ(line, 1u);
            EXPECT_EQ(col, 1u);
        }
    }
}

} // namespace
} // namespace yuan
