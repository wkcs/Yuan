/// \file SourceLocation.h
/// \brief Source code location tracking.
///
/// This file defines classes for tracking locations in source files.

#ifndef YUAN_BASIC_SOURCELOCATION_H
#define YUAN_BASIC_SOURCELOCATION_H

#include <cstdint>

namespace yuan {

/// \brief Represents a location in source code.
///
/// SourceLocation uses a compact 32-bit encoding to represent a position
/// in the source code. The offset is relative to the start of all loaded
/// source files managed by SourceManager.
class SourceLocation {
public:
    /// \brief Construct an invalid source location.
    SourceLocation() = default;
    
    /// \brief Construct a source location from an offset.
    explicit SourceLocation(uint32_t offset) : Offset(offset) {}
    
    /// \brief Check if this location is valid.
    bool isValid() const { return Offset != 0; }
    
    /// \brief Check if this location is invalid.
    bool isInvalid() const { return !isValid(); }
    
    /// \brief Get the raw offset value.
    uint32_t getOffset() const { return Offset; }
    
    bool operator==(const SourceLocation& other) const {
        return Offset == other.Offset;
    }
    
    bool operator!=(const SourceLocation& other) const {
        return Offset != other.Offset;
    }
    
    bool operator<(const SourceLocation& other) const {
        return Offset < other.Offset;
    }
    
    bool operator<=(const SourceLocation& other) const {
        return Offset <= other.Offset;
    }
    
    bool operator>(const SourceLocation& other) const {
        return Offset > other.Offset;
    }
    
    bool operator>=(const SourceLocation& other) const {
        return Offset >= other.Offset;
    }
    
private:
    uint32_t Offset = 0;
};

/// \brief Represents a range in source code.
///
/// SourceRange represents a contiguous range of source code, defined by
/// a beginning and ending SourceLocation.
class SourceRange {
public:
    /// \brief Construct an invalid source range.
    SourceRange() = default;
    
    /// \brief Construct a source range from begin and end locations.
    SourceRange(SourceLocation begin, SourceLocation end)
        : Begin(begin), End(end) {}
    
    /// \brief Construct a source range from a single location.
    explicit SourceRange(SourceLocation loc)
        : Begin(loc), End(loc) {}
    
    /// \brief Get the beginning of the range.
    SourceLocation getBegin() const { return Begin; }
    
    /// \brief Get the end of the range.
    SourceLocation getEnd() const { return End; }
    
    /// \brief Check if this range is valid.
    bool isValid() const { return Begin.isValid() && End.isValid(); }
    
    /// \brief Check if this range is invalid.
    bool isInvalid() const { return !isValid(); }
    
    bool operator==(const SourceRange& other) const {
        return Begin == other.Begin && End == other.End;
    }
    
    bool operator!=(const SourceRange& other) const {
        return !(*this == other);
    }
    
private:
    SourceLocation Begin;
    SourceLocation End;
};

} // namespace yuan

#endif // YUAN_BASIC_SOURCELOCATION_H
