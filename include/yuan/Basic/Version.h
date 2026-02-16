/// \file Version.h
/// \brief Yuan compiler version information.
///
/// This file provides access to version information about the Yuan compiler,
/// including version numbers, Git commit hash, build time, and LLVM version.

#ifndef YUAN_BASIC_VERSION_H
#define YUAN_BASIC_VERSION_H

#include <string>
#include <ostream>

namespace yuan {

/// \brief Provides version information about the Yuan compiler.
///
/// This class provides static methods to access various version-related
/// information about the compiler build.
class VersionInfo {
public:
    /// \brief Get the version string (e.g., "0.1.0").
    static std::string getVersionString();
    
    /// \brief Get the major version number.
    static unsigned getMajor();
    
    /// \brief Get the minor version number.
    static unsigned getMinor();
    
    /// \brief Get the patch version number.
    static unsigned getPatch();
    
    /// \brief Get the Git commit hash (short form).
    /// \return The short Git hash, or "unknown" if not available.
    static std::string getGitHash();
    
    /// \brief Get the full Git commit hash.
    /// \return The full Git hash, or "unknown" if not available.
    static std::string getGitHashFull();
    
    /// \brief Get the build timestamp.
    /// \return The build time in "YYYY-MM-DD HH:MM:SS UTC" format.
    static std::string getBuildTime();
    
    /// \brief Get the LLVM version string.
    /// \return The LLVM version, or "not found" if LLVM is not available.
    static std::string getLLVMVersion();
    
    /// \brief Get the full version string with Git hash.
    /// \return A string like "Yuan Compiler version 0.1.0 (abc1234)".
    static std::string getFullVersionString();
    
    /// \brief Print version information to an output stream.
    /// \param os The output stream to print to.
    static void printVersion(std::ostream& os);
};

} // namespace yuan

#endif // YUAN_BASIC_VERSION_H
