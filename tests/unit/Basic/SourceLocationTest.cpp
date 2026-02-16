/// \file SourceLocationTest.cpp
/// \brief Unit tests for SourceLocation and SourceRange.

#include "yuan/Basic/SourceLocation.h"
#include <gtest/gtest.h>

namespace yuan {
namespace {

// ============================================================================
// SourceLocation Tests
// ============================================================================

TEST(SourceLocationTest, DefaultConstructorCreatesInvalidLocation) {
    SourceLocation loc;
    EXPECT_TRUE(loc.isInvalid());
    EXPECT_FALSE(loc.isValid());
    EXPECT_EQ(loc.getOffset(), 0u);
}

TEST(SourceLocationTest, ConstructorWithOffsetCreatesValidLocation) {
    SourceLocation loc(42);
    EXPECT_TRUE(loc.isValid());
    EXPECT_FALSE(loc.isInvalid());
    EXPECT_EQ(loc.getOffset(), 42u);
}

TEST(SourceLocationTest, ZeroOffsetIsInvalid) {
    SourceLocation loc(0);
    EXPECT_TRUE(loc.isInvalid());
    EXPECT_FALSE(loc.isValid());
}

TEST(SourceLocationTest, EqualityComparison) {
    SourceLocation loc1(10);
    SourceLocation loc2(10);
    SourceLocation loc3(20);
    
    EXPECT_TRUE(loc1 == loc2);
    EXPECT_FALSE(loc1 == loc3);
    EXPECT_FALSE(loc1 != loc2);
    EXPECT_TRUE(loc1 != loc3);
}

TEST(SourceLocationTest, LessThanComparison) {
    SourceLocation loc1(10);
    SourceLocation loc2(20);
    SourceLocation loc3(10);
    
    EXPECT_TRUE(loc1 < loc2);
    EXPECT_FALSE(loc2 < loc1);
    EXPECT_FALSE(loc1 < loc3);
}

TEST(SourceLocationTest, LessEqualComparison) {
    SourceLocation loc1(10);
    SourceLocation loc2(20);
    SourceLocation loc3(10);
    
    EXPECT_TRUE(loc1 <= loc2);
    EXPECT_TRUE(loc1 <= loc3);
    EXPECT_FALSE(loc2 <= loc1);
}

TEST(SourceLocationTest, GreaterThanComparison) {
    SourceLocation loc1(20);
    SourceLocation loc2(10);
    SourceLocation loc3(20);
    
    EXPECT_TRUE(loc1 > loc2);
    EXPECT_FALSE(loc2 > loc1);
    EXPECT_FALSE(loc1 > loc3);
}

TEST(SourceLocationTest, GreaterEqualComparison) {
    SourceLocation loc1(20);
    SourceLocation loc2(10);
    SourceLocation loc3(20);
    
    EXPECT_TRUE(loc1 >= loc2);
    EXPECT_TRUE(loc1 >= loc3);
    EXPECT_FALSE(loc2 >= loc1);
}

// ============================================================================
// SourceRange Tests
// ============================================================================

TEST(SourceRangeTest, DefaultConstructorCreatesInvalidRange) {
    SourceRange range;
    EXPECT_TRUE(range.isInvalid());
    EXPECT_FALSE(range.isValid());
}

TEST(SourceRangeTest, ConstructorWithBeginAndEnd) {
    SourceLocation begin(10);
    SourceLocation end(20);
    SourceRange range(begin, end);
    
    EXPECT_TRUE(range.isValid());
    EXPECT_EQ(range.getBegin().getOffset(), 10u);
    EXPECT_EQ(range.getEnd().getOffset(), 20u);
}

TEST(SourceRangeTest, ConstructorWithSingleLocation) {
    SourceLocation loc(15);
    SourceRange range(loc);
    
    EXPECT_TRUE(range.isValid());
    EXPECT_EQ(range.getBegin(), loc);
    EXPECT_EQ(range.getEnd(), loc);
}

TEST(SourceRangeTest, RangeWithInvalidBeginIsInvalid) {
    SourceLocation begin;  // Invalid
    SourceLocation end(20);
    SourceRange range(begin, end);
    
    EXPECT_TRUE(range.isInvalid());
}

TEST(SourceRangeTest, RangeWithInvalidEndIsInvalid) {
    SourceLocation begin(10);
    SourceLocation end;  // Invalid
    SourceRange range(begin, end);
    
    EXPECT_TRUE(range.isInvalid());
}

TEST(SourceRangeTest, EqualityComparison) {
    SourceRange range1(SourceLocation(10), SourceLocation(20));
    SourceRange range2(SourceLocation(10), SourceLocation(20));
    SourceRange range3(SourceLocation(10), SourceLocation(30));
    SourceRange range4(SourceLocation(5), SourceLocation(20));
    
    EXPECT_TRUE(range1 == range2);
    EXPECT_FALSE(range1 == range3);
    EXPECT_FALSE(range1 == range4);
    
    EXPECT_FALSE(range1 != range2);
    EXPECT_TRUE(range1 != range3);
    EXPECT_TRUE(range1 != range4);
}

} // namespace
} // namespace yuan
