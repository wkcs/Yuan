/// \file
/// \brief 编译器驱动器
///
/// 编译器的主要驱动类，协调各个编译阶段的执行。

#ifndef YUAN_DRIVER_DRIVER_H
#define YUAN_DRIVER_DRIVER_H

#include "yuan/Driver/Options.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/SourceManager.h"
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace yuan {

// 前向声明
class Lexer;
class Decl;
class ASTContext;
class Sema;

/// 编译结果
enum class CompilationResult {
    Success,        ///< 编译成功
    LexerError,     ///< 词法分析错误
    ParserError,    ///< 语法分析错误
    SemanticError,  ///< 语义分析错误
    CodeGenError,   ///< 代码生成错误
    LinkError,      ///< 链接错误
    IOError,        ///< 文件 I/O 错误
    InternalError   ///< 内部错误
};

/// 编译器驱动器
class Driver {
public:
    /// 构造函数
    explicit Driver(const CompilerOptions& options);
    
    /// 析构函数
    ~Driver();
    
    /// 运行编译器
    CompilationResult run();
    
    /// 获取诊断引擎
    DiagnosticEngine& getDiagnostics() { return *Diagnostics; }
    
    /// 获取源码管理器
    SourceManager& getSourceManager() { return *SourceMgr; }
    
    /// 获取编译选项
    const CompilerOptions& getOptions() const { return Options; }

private:
    struct CompilationUnit {
        std::string InputFile;
        SourceManager::FileID FileID = SourceManager::InvalidFileID;
        std::unique_ptr<ASTContext> Context;
        std::unique_ptr<Sema> Semantic;
        std::vector<Decl*> Declarations;
        bool Parsed = false;
        bool Analyzed = false;
    };

    /// 编译选项
    CompilerOptions Options;
    
    /// 源码管理器
    std::unique_ptr<SourceManager> SourceMgr;
    
    /// 诊断引擎
    std::unique_ptr<DiagnosticEngine> Diagnostics;
    
    /// 诊断消费者
    std::unique_ptr<DiagnosticConsumer> DiagConsumer;
    
    /// 初始化诊断系统
    void initializeDiagnostics();
    
    /// 加载输入文件
    CompilationResult loadInputFiles();

    /// 对所有输入执行词法输出
    CompilationResult runTokenDump();

    /// 对所有输入执行语法/语义前端（每文件只执行一次）
    CompilationResult runFrontend(bool needSema);

    /// 生成/输出 AST（树形或 pretty）
    CompilationResult runASTLikeDump(bool treeMode);

    /// 生成 IR/Obj/Exe
    CompilationResult runCodeGeneration();

    /// 链接目标文件
    CompilationResult linkObjects(const std::vector<std::string>& objectFiles,
                                  const std::string& executableFile);

    /// 输出 tokens 到文件或标准输出
    CompilationResult emitTokens(Lexer& lexer,
                                 const std::string& inputFile,
                                 std::ostream& output);

    /// 配置语义分析器中的模块管理器
    void configureModuleManager(Sema& sema) const;

    /// 生成单个模块对象（用于链接阶段依赖模块）
    CompilationResult buildModuleObject(const std::string& moduleSourcePath,
                                        unsigned optLevel,
                                        const std::string& preferredObjectPath,
                                        std::string& outObjectFile);

    /// 根据动作和输入推导每文件输出名
    std::string deducePerInputOutput(const std::string& inputFile, const std::string& ext) const;
    
    /// 获取编译结果的字符串描述
    const char* getResultString(CompilationResult result) const;
    
    /// 打印编译统计信息
    void printStatistics() const;

    std::vector<CompilationUnit> Units;
};

} // namespace yuan

#endif // YUAN_DRIVER_DRIVER_H
