/// \file ModuleManager.h
/// \brief 模块管理器 - 负责模块的加载、解析和符号导入

#ifndef YUAN_SEMA_MODULE_MANAGER_H
#define YUAN_SEMA_MODULE_MANAGER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace yuan {

// 前向声明
class ASTContext;
class DiagnosticEngine;
class SourceManager;
class Decl;
class Scope;
class Sema;
class Type;

/// \brief 模块导出符号
struct ModuleExport {
    enum class Kind {
        Variable,
        Constant,
        Function,
        Struct,
        Enum,
        Trait,
        TypeAlias,
        ModuleAlias
    };

    Kind ExportKind = Kind::Function;
    std::string Name;
    Type* SemanticType = nullptr;
    Decl* DeclNode = nullptr;
    std::string LinkName;                 ///< 运行时链接符号（函数/全局）
    std::vector<std::string> GenericParams;
    std::vector<std::pair<std::string, Type*>> StructFields; ///< 仅 Struct 使用
    std::string ModulePath;               ///< 仅 ModuleAlias 使用
    Type* ImplOwnerType = nullptr;        ///< 若为 impl 方法，记录所属类型
};

/// \brief 模块信息
struct ModuleInfo {
    std::string Name;                    ///< 模块名称
    std::string FilePath;                ///< 模块文件路径
    std::vector<Decl*> Declarations;     ///< 模块中的声明
    std::vector<ModuleExport> Exports;   ///< 模块导出符号
    std::vector<std::string> Dependencies; ///< 导入依赖（逻辑路径）
    std::string InterfacePath;           ///< .ymi 接口文件路径
    std::string ObjectPath;              ///< .o 对象文件路径
    bool IsLoaded;                       ///< 是否已加载
    bool IsStdLib;                       ///< 是否为标准库模块
    bool IsFromInterface;                ///< 是否由接口文件加载

    ModuleInfo(const std::string& name, const std::string& path, bool isStdLib = false)
        : Name(name), FilePath(path), IsLoaded(false), IsStdLib(isStdLib),
          IsFromInterface(false) {}
};

/// \brief 模块管理器
///
/// 负责：
/// - 解析模块路径（标准库路径、相对路径）
/// - 加载和解析模块文件
/// - 管理已加载的模块
/// - 检测循环导入
class ModuleManager {
public:
    /// \brief 构造函数
    explicit ModuleManager(SourceManager& sourceMgr, DiagnosticEngine& diag,
                          ASTContext& ctx, Sema& sema);

    /// \brief 解析模块路径
    /// \param modulePath 模块路径（如 "std.io" 或 "./local"）
    /// \param currentFilePath 当前文件路径（用于解析相对路径）
    /// \return 模块文件的完整路径，失败返回空字符串
    std::string resolveModulePath(const std::string& modulePath,
                                   const std::string& currentFilePath);

    /// \brief 加载模块
    /// \param modulePath 模块路径
    /// \param currentFilePath 当前文件路径
    /// \param importChain 导入链（用于检测循环导入）
    /// \return 模块信息，失败返回 nullptr
    ModuleInfo* loadModule(const std::string& modulePath,
                          const std::string& currentFilePath,
                          std::vector<std::string>& importChain);

    /// \brief 获取已加载的模块
    /// \param moduleName 模块名称
    /// \return 模块信息，未找到返回 nullptr
    ModuleInfo* getLoadedModule(const std::string& moduleName);

    /// \brief 设置标准库路径
    void setStdLibPath(const std::string& path) { StdLibPath = path; }

    /// \brief 获取标准库路径
    const std::string& getStdLibPath() const { return StdLibPath; }

    /// \brief 设置模块缓存目录
    void setModuleCacheDir(const std::string& path) { ModuleCacheDir = path; }

    /// \brief 获取模块缓存目录
    const std::string& getModuleCacheDir() const { return ModuleCacheDir; }

    /// \brief 添加预编译包搜索路径
    void addPackagePath(const std::string& path);

    /// \brief 获取预编译包搜索路径
    const std::vector<std::string>& getPackagePaths() const { return PackagePaths; }

    /// \brief 检查是否在导入链中（检测循环导入）
    bool isInImportChain(const std::string& moduleName,
                        const std::vector<std::string>& importChain) const;

    /// \brief 获取所有已加载的模块
    const std::unordered_map<std::string, std::unique_ptr<ModuleInfo>>& getLoadedModules() const {
        return LoadedModules;
    }

private:
    /// \brief 解析标准库模块路径
    /// \param modulePath 标准库模块路径（如 "std.io"）
    /// \return 完整文件路径
    std::string resolveStdLibPath(const std::string& modulePath);

    /// \brief 解析相对路径模块
    /// \param modulePath 相对路径（如 "./local" 或 "../utils"）
    /// \param currentFilePath 当前文件路径
    /// \return 完整文件路径
    std::string resolveRelativePath(const std::string& modulePath,
                                    const std::string& currentFilePath);

    /// \brief 解析预编译包中的接口路径
    std::string resolvePackageInterfacePath(const std::string& modulePath) const;

    /// \brief 规范化模块名称
    /// \param modulePath 模块路径
    /// \return 规范化的模块名称
    std::string normalizeModuleName(const std::string& modulePath);

    /// \brief 生成模块缓存键
    std::string buildCacheKey(const std::string& moduleFilePath) const;

    /// \brief 根据缓存键返回接口路径
    std::string getInterfacePathForKey(const std::string& cacheKey) const;

    /// \brief 根据缓存键返回对象路径
    std::string getObjectPathForKey(const std::string& cacheKey) const;

    /// \brief 尝试加载接口文件
    bool loadModuleInterface(ModuleInfo& moduleInfo, const std::string& interfacePath,
                             std::vector<std::string>& importChain);

    /// \brief 将模块导出写入接口文件
    bool writeModuleInterface(ModuleInfo& moduleInfo);

    /// \brief 从声明构建模块导出列表
    void buildModuleExports(ModuleInfo& moduleInfo);

private:
    SourceManager& SourceMgr;                               ///< 源码管理器
    DiagnosticEngine& Diag;                                 ///< 诊断引擎
    ASTContext& Ctx;                                        ///< AST 上下文
    Sema& Sema_;                                            ///< 语义分析器
    std::string StdLibPath;                                 ///< 标准库路径
    std::string ModuleCacheDir = ".yuan/cache";            ///< 模块缓存目录
    std::vector<std::string> PackagePaths;                  ///< 预编译包搜索路径
    std::unordered_map<std::string, std::unique_ptr<ModuleInfo>> LoadedModules;  ///< 已加载的模块
};

} // namespace yuan

#endif // YUAN_SEMA_MODULE_MANAGER_H
