/// \file SourceManager.h
/// \brief Source file management.
///
/// This file defines the SourceManager class for managing source files.

#ifndef YUAN_BASIC_SOURCEMANAGER_H
#define YUAN_BASIC_SOURCEMANAGER_H

#include "yuan/Basic/SourceLocation.h"
#include <string>
#include <vector>
#include <utility>

namespace yuan {

/// \brief Manages source files and provides location services.
///
/// SourceManager is responsible for loading source files, managing their
/// content, and providing services to convert between offsets and line/column
/// numbers.
class SourceManager {
public:
    /// \brief File identifier type.
    using FileID = uint32_t;
    
    /// \brief Invalid file ID constant.
    static constexpr FileID InvalidFileID = 0;
    
    SourceManager() = default;
    
    /// \brief Load a source file from disk.
    /// \param path The path to the source file.
    /// \return The FileID for the loaded file, or InvalidFileID on error.
    FileID loadFile(const std::string& path);
    
    /// \brief Create a buffer from a string (useful for testing).
    /// \param content The content of the buffer.
    /// \param name The name to associate with the buffer.
    /// \return The FileID for the created buffer.
    FileID createBuffer(const std::string& content,
                        const std::string& name = "<buffer>");
    
    /// \brief Get the content of a file.
    /// \param fid The file ID.
    /// \return The file content, or empty string if invalid.
    const std::string& getBufferData(FileID fid) const;
    
    /// \brief Get the filename for a file ID.
    /// \param fid The file ID.
    /// \return The filename, or empty string if invalid.
    const std::string& getFilename(FileID fid) const;
    
    /// \brief Convert a source location to line and column numbers.
    /// \param loc The source location.
    /// \return A pair of (line, column), both 1-based.
    std::pair<unsigned, unsigned> getLineAndColumn(SourceLocation loc) const;
    
    /// \brief Get the content of the line containing a location.
    /// \param loc The source location.
    /// \return The line content (without newline).
    std::string getLineContent(SourceLocation loc) const;
    
    /// \brief Get the FileID for a source location.
    /// \param loc The source location.
    /// \return The FileID, or InvalidFileID if invalid.
    FileID getFileID(SourceLocation loc) const;
    
    /// \brief Create a SourceLocation for a position in a file.
    /// \param fid The file ID.
    /// \param offset The offset within the file.
    /// \return The SourceLocation.
    SourceLocation getLocation(FileID fid, uint32_t offset) const;
    
private:
    struct FileInfo {
        std::string Filename;
        std::string Content;
        std::vector<uint32_t> LineOffsets;  // Offset of each line start
        uint32_t StartOffset;               // Global offset where this file starts
    };
    
    std::vector<FileInfo> Files;
    uint32_t NextOffset = 1;  // 0 is reserved for invalid location
    
    static const std::string EmptyString;
    
    void computeLineOffsets(FileInfo& info);
};

} // namespace yuan

#endif // YUAN_BASIC_SOURCEMANAGER_H
