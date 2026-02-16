/// \file SourceManager.cpp
/// \brief Implementation of source file management.

#include "yuan/Basic/SourceManager.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace yuan {

const std::string SourceManager::EmptyString;

SourceManager::FileID SourceManager::loadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return InvalidFileID;
    }
    
    std::ostringstream oss;
    oss << file.rdbuf();
    std::string content = oss.str();
    
    return createBuffer(content, path);
}

SourceManager::FileID SourceManager::createBuffer(const std::string& content,
                                                   const std::string& name) {
    FileInfo info;
    info.Filename = name;
    info.Content = content;
    info.StartOffset = NextOffset;
    
    computeLineOffsets(info);
    
    // Update NextOffset for the next file
    // +1 to ensure there's a gap between files
    NextOffset += static_cast<uint32_t>(content.size()) + 1;
    
    Files.push_back(std::move(info));
    
    // FileID is 1-based (0 is InvalidFileID)
    return static_cast<FileID>(Files.size());
}

const std::string& SourceManager::getBufferData(FileID fid) const {
    if (fid == InvalidFileID || fid > Files.size()) {
        return EmptyString;
    }
    return Files[fid - 1].Content;
}

const std::string& SourceManager::getFilename(FileID fid) const {
    if (fid == InvalidFileID || fid > Files.size()) {
        return EmptyString;
    }
    return Files[fid - 1].Filename;
}

std::pair<unsigned, unsigned> SourceManager::getLineAndColumn(SourceLocation loc) const {
    if (loc.isInvalid()) {
        return {0, 0};
    }
    
    FileID fid = getFileID(loc);
    if (fid == InvalidFileID) {
        return {0, 0};
    }
    
    const FileInfo& info = Files[fid - 1];
    uint32_t localOffset = loc.getOffset() - info.StartOffset;
    
    // LineOffsets contains the start offset of each line AFTER the first line.
    // So LineOffsets[0] is the start of line 2, LineOffsets[1] is start of line 3, etc.
    // 
    // Binary search to find the first line offset that is greater than localOffset.
    // The line number is then the index + 1 (since line numbers are 1-based).
    auto it = std::upper_bound(info.LineOffsets.begin(), 
                               info.LineOffsets.end(), 
                               localOffset);
    
    // Line number is 1-based
    // If it points to begin(), we're on line 1
    // If it points to LineOffsets[i], we're on line i+1
    unsigned line = static_cast<unsigned>(it - info.LineOffsets.begin()) + 1;
    
    // Column is offset from line start, 1-based
    // Line 1 starts at offset 0
    // Line N (N > 1) starts at LineOffsets[N-2]
    uint32_t lineStart = (line == 1) ? 0 : info.LineOffsets[line - 2];
    unsigned column = localOffset - lineStart + 1;
    
    return {line, column};
}

std::string SourceManager::getLineContent(SourceLocation loc) const {
    if (loc.isInvalid()) {
        return "";
    }
    
    FileID fid = getFileID(loc);
    if (fid == InvalidFileID) {
        return "";
    }
    
    const FileInfo& info = Files[fid - 1];
    uint32_t localOffset = loc.getOffset() - info.StartOffset;
    
    // Find line number using the same logic as getLineAndColumn
    auto it = std::upper_bound(info.LineOffsets.begin(), 
                               info.LineOffsets.end(), 
                               localOffset);
    unsigned line = static_cast<unsigned>(it - info.LineOffsets.begin()) + 1;
    
    // Get line start
    // Line 1 starts at offset 0
    // Line N (N > 1) starts at LineOffsets[N-2]
    uint32_t lineStart = (line == 1) ? 0 : info.LineOffsets[line - 2];
    
    // Get line end
    // Line N ends at LineOffsets[N-1] (or end of content for last line)
    uint32_t lineEnd;
    if (line <= info.LineOffsets.size()) {
        lineEnd = info.LineOffsets[line - 1];
    } else {
        lineEnd = static_cast<uint32_t>(info.Content.size());
    }
    
    // Extract line content (without trailing newline)
    std::string lineContent = info.Content.substr(lineStart, lineEnd - lineStart);
    
    // Remove trailing newline characters
    while (!lineContent.empty() && 
           (lineContent.back() == '\n' || lineContent.back() == '\r')) {
        lineContent.pop_back();
    }
    
    return lineContent;
}

SourceManager::FileID SourceManager::getFileID(SourceLocation loc) const {
    if (loc.isInvalid()) {
        return InvalidFileID;
    }
    
    uint32_t offset = loc.getOffset();
    
    // Linear search through files (could be optimized with binary search)
    for (size_t i = 0; i < Files.size(); ++i) {
        const FileInfo& info = Files[i];
        // fileEnd is one past the last valid offset (EOF position is valid)
        uint32_t fileEnd = info.StartOffset + static_cast<uint32_t>(info.Content.size());
        
        // Include the EOF position (offset == fileEnd is valid)
        if (offset >= info.StartOffset && offset <= fileEnd) {
            return static_cast<FileID>(i + 1);
        }
    }
    
    return InvalidFileID;
}

SourceLocation SourceManager::getLocation(FileID fid, uint32_t offset) const {
    if (fid == InvalidFileID || fid > Files.size()) {
        return SourceLocation();
    }
    
    const FileInfo& info = Files[fid - 1];
    
    // Check if offset is within file bounds
    if (offset > info.Content.size()) {
        return SourceLocation();
    }
    
    return SourceLocation(info.StartOffset + offset);
}

void SourceManager::computeLineOffsets(FileInfo& info) {
    info.LineOffsets.clear();
    
    // First line starts at offset 0 (implicit)
    // We store the start offset of each line after the first
    
    const std::string& content = info.Content;
    for (size_t i = 0; i < content.size(); ++i) {
        if (content[i] == '\n') {
            // Next line starts after the newline
            info.LineOffsets.push_back(static_cast<uint32_t>(i + 1));
        } else if (content[i] == '\r') {
            // Handle \r\n (Windows) and \r (old Mac)
            if (i + 1 < content.size() && content[i + 1] == '\n') {
                // \r\n - skip the \n
                ++i;
            }
            info.LineOffsets.push_back(static_cast<uint32_t>(i + 1));
        }
    }
}

} // namespace yuan
