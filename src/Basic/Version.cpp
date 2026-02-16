/// \file Version.cpp
/// \brief Implementation of version information utilities.

#include "yuan/Basic/Version.h"
#include "yuan/Basic/Version.gen.h"

namespace yuan {

std::string VersionInfo::getVersionString() {
    return YUAN_VERSION_STRING;
}

unsigned VersionInfo::getMajor() {
    return YUAN_VERSION_MAJOR;
}

unsigned VersionInfo::getMinor() {
    return YUAN_VERSION_MINOR;
}

unsigned VersionInfo::getPatch() {
    return YUAN_VERSION_PATCH;
}

std::string VersionInfo::getGitHash() {
    return YUAN_GIT_HASH;
}

std::string VersionInfo::getGitHashFull() {
    return YUAN_GIT_HASH_FULL;
}

std::string VersionInfo::getBuildTime() {
    return YUAN_BUILD_TIME;
}

std::string VersionInfo::getLLVMVersion() {
    return YUAN_LLVM_VERSION;
}

std::string VersionInfo::getFullVersionString() {
    std::string result = "Yuan Compiler version ";
    result += getVersionString();
    
    std::string gitHash = getGitHash();
    if (!gitHash.empty() && gitHash != "unknown") {
        result += " (";
        result += gitHash;
        result += ")";
    }
    
    return result;
}

void VersionInfo::printVersion(std::ostream& os) {
    os << getFullVersionString() << "\n";
    os << "  Build time: " << getBuildTime() << "\n";
    os << "  LLVM version: " << getLLVMVersion() << "\n";
}

} // namespace yuan
