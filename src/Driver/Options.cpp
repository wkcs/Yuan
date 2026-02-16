/// \file
/// \brief 编译器选项实现

#include "yuan/Driver/Options.h"
#include <filesystem>

namespace yuan {

std::string CompilerOptions::getOutputFileName() const {
    if (!OutputFile.empty()) {
        return OutputFile;
    }
    return deduceOutputFileName();
}

const char* CompilerOptions::getOptLevelString() const {
    switch (Optimization) {
        case OptLevel::O0: return "O0";
        case OptLevel::O1: return "O1";
        case OptLevel::O2: return "O2";
        case OptLevel::O3: return "O3";
    }
    return "O0";
}

const char* CompilerOptions::getActionString() const {
    switch (Action) {
        case DriverAction::Link: return "link";
        case DriverAction::Object: return "obj";
        case DriverAction::IR: return "ir";
        case DriverAction::SyntaxOnly: return "syntax-only";
        case DriverAction::Tokens: return "tokens";
        case DriverAction::AST: return "ast";
        case DriverAction::Pretty: return "pretty";
    }
    return "link";
}

bool CompilerOptions::validate(std::string& errorMsg) const {
    // 检查是否有输入文件
    if (InputFiles.empty() && !ShowHelp && !ShowVersion) {
        errorMsg = "错误：未指定输入文件";
        return false;
    }
    
    // 检查输入文件是否存在
    for (const auto& file : InputFiles) {
        if (!std::filesystem::exists(file)) {
            errorMsg = "错误：输入文件不存在: " + file;
            return false;
        }
        
        // 检查文件扩展名
        std::filesystem::path path(file);
        if (path.extension() != ".yu") {
            errorMsg = "错误：输入文件必须是 .yu 文件: " + file;
            return false;
        }
    }
    
    // 检查输出文件路径是否有效
    if (!OutputFile.empty()) {
        std::filesystem::path outputPath(OutputFile);
        auto parentPath = outputPath.parent_path();
        if (!parentPath.empty() && !std::filesystem::exists(parentPath)) {
            errorMsg = "错误：输出目录不存在: " + parentPath.string();
            return false;
        }
    }

    if (!StdLibPath.empty() && !std::filesystem::exists(StdLibPath)) {
        errorMsg = "错误：标准库目录不存在: " + StdLibPath;
        return false;
    }

    for (const auto& pkgPath : PackagePaths) {
        if (!std::filesystem::exists(pkgPath)) {
            errorMsg = "错误：包搜索路径不存在: " + pkgPath;
            return false;
        }
    }

    // 非链接动作在多输入时不允许使用单一 -o 输出
    if (InputFiles.size() > 1 &&
        (Action == DriverAction::Object ||
         Action == DriverAction::IR ||
         Action == DriverAction::Tokens ||
         Action == DriverAction::AST ||
         Action == DriverAction::Pretty) &&
        !OutputFile.empty()) {
        errorMsg = "错误：多输入文件模式下，当前动作不支持单一 -o 输出";
        return false;
    }
    
    return true;
}

std::string CompilerOptions::deduceOutputFileName() const {
    if (InputFiles.empty()) {
        return "a.out";  // 默认输出文件名
    }
    
    std::filesystem::path inputPath(InputFiles[0]);
    std::string baseName = inputPath.stem().string();
    
    switch (Action) {
        case DriverAction::Tokens:
            return baseName + ".tokens";
        case DriverAction::AST:
            return baseName + ".ast";
        case DriverAction::Pretty:
            return baseName + ".pretty.yu";
        case DriverAction::IR:
            return baseName + ".ll";
        case DriverAction::Object:
            return baseName + ".o";
        case DriverAction::Link:
#ifdef _WIN32
            return baseName + ".exe";
#else
            return baseName;
#endif
        case DriverAction::SyntaxOnly:
            return "";
    }
    
    return "a.out";
}

} // namespace yuan
