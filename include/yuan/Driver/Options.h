/// \file
/// \brief 编译器选项定义
///
/// 定义编译器的各种选项和配置，包括输入文件、输出文件、编译模式等。

#ifndef YUAN_DRIVER_OPTIONS_H
#define YUAN_DRIVER_OPTIONS_H

#include <string>
#include <vector>

namespace yuan {

/// 驱动执行动作
enum class DriverAction {
    Link,       ///< 生成可执行文件（默认）
    Object,     ///< -c，生成目标文件
    IR,         ///< -S，生成 LLVM IR
    SyntaxOnly, ///< -fsyntax-only，仅前端检查
    Tokens,     ///< --emit=tokens
    AST,        ///< --emit=ast（树形）
    Pretty      ///< --emit=pretty（源码重建）
};

/// 优化级别
enum class OptLevel {
    O0,  ///< 无优化
    O1,  ///< 基本优化
    O2,  ///< 标准优化
    O3   ///< 激进优化
};

/// 编译器选项
class CompilerOptions {
public:
    CompilerOptions() = default;
    
    /// 输入文件列表
    std::vector<std::string> InputFiles;
    
    /// 输出文件名（可选，默认根据输入文件和模式推导）
    std::string OutputFile;

    /// 执行动作
    DriverAction Action = DriverAction::Link;
    
    /// 优化级别
    OptLevel Optimization = OptLevel::O0;
    
    /// 是否显示帮助信息
    bool ShowHelp = false;
    
    /// 是否显示版本信息
    bool ShowVersion = false;
    
    /// 是否启用详细输出
    bool Verbose = false;
    
    /// 包含路径列表
    std::vector<std::string> IncludePaths;

    /// 模块缓存目录（.ymi/.o）
    std::string ModuleCacheDir = ".yuan/cache";

    /// 预编译包搜索路径
    std::vector<std::string> PackagePaths;

    /// 标准库根路径（可选，默认由 ModuleManager 决定）
    std::string StdLibPath;
    
    /// 库路径列表
    std::vector<std::string> LibraryPaths;
    
    /// 链接库列表
    std::vector<std::string> Libraries;
    
    /// 获取输出文件名（如果未指定则推导）
    std::string getOutputFileName() const;
    
    /// 获取优化级别字符串
    const char* getOptLevelString() const;
    
    /// 获取动作字符串
    const char* getActionString() const;
    
    /// 验证选项的有效性
    bool validate(std::string& errorMsg) const;
    
private:
    /// 根据输入文件和动作推导输出文件名
    std::string deduceOutputFileName() const;
};

} // namespace yuan

#endif // YUAN_DRIVER_OPTIONS_H
