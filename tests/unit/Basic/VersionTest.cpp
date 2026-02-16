/// \file VersionTest.cpp
/// \brief Unit tests for version information.

#include "yuan/Basic/Version.h"
#include <gtest/gtest.h>
#include <regex>
#include <sstream>

namespace yuan {
namespace {

TEST(VersionTest, VersionStringFormat) {
    std::string version = VersionInfo::getVersionString();
    // Version should match semantic versioning format: X.Y.Z
    std::regex semver_regex(R"(\d+\.\d+\.\d+)");
    EXPECT_TRUE(std::regex_match(version, semver_regex))
        << "Version string '" << version << "' does not match X.Y.Z format";
}

TEST(VersionTest, VersionComponents) {
    unsigned major = VersionInfo::getMajor();
    unsigned minor = VersionInfo::getMinor();
    unsigned patch = VersionInfo::getPatch();
    
    // Reconstruct version string from components
    std::ostringstream oss;
    oss << major << "." << minor << "." << patch;
    
    EXPECT_EQ(oss.str(), VersionInfo::getVersionString());
}

TEST(VersionTest, GitHashNotEmpty) {
    std::string hash = VersionInfo::getGitHash();
    EXPECT_FALSE(hash.empty()) << "Git hash should not be empty";
}

TEST(VersionTest, BuildTimeNotEmpty) {
    std::string buildTime = VersionInfo::getBuildTime();
    EXPECT_FALSE(buildTime.empty()) << "Build time should not be empty";
}

TEST(VersionTest, FullVersionStringContainsVersion) {
    std::string fullVersion = VersionInfo::getFullVersionString();
    std::string version = VersionInfo::getVersionString();
    
    EXPECT_NE(fullVersion.find(version), std::string::npos)
        << "Full version string should contain version number";
    EXPECT_NE(fullVersion.find("Yuan"), std::string::npos)
        << "Full version string should contain 'Yuan'";
}

TEST(VersionTest, PrintVersionOutput) {
    std::ostringstream oss;
    VersionInfo::printVersion(oss);
    std::string output = oss.str();
    
    EXPECT_FALSE(output.empty()) << "printVersion should produce output";
    EXPECT_NE(output.find("Yuan"), std::string::npos);
    EXPECT_NE(output.find("Build time"), std::string::npos);
    EXPECT_NE(output.find("LLVM"), std::string::npos);
}

} // namespace
} // namespace yuan
