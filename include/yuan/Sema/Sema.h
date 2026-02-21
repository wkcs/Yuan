/// \file Sema.h
/// \brief 语义分析器接口定义。
///
/// 本文件定义了 Yuan 语言的语义分析器，负责类型检查、符号解析、
/// 语义验证等工作。

#ifndef YUAN_SEMA_SEMA_H
#define YUAN_SEMA_SEMA_H

#include "yuan/Sema/Scope.h"
#include "yuan/Sema/Symbol.h"
#include "yuan/Sema/ModuleManager.h"
#include "yuan/Basic/SourceLocation.h"
#include "yuan/Basic/DiagnosticIDs.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace yuan {

// 前向声明
class ASTContext;
class DiagnosticEngine;
class SourceManager;
class TypeChecker;
class Type;
class EnumType;
class Decl;
class Stmt;
class Expr;
class TypeNode;
class BuiltinTypeNode;
class IdentifierTypeNode;
class ArrayTypeNode;
class SliceTypeNode;
class TupleTypeNode;
class OptionalTypeNode;
class ReferenceTypeNode;
class PointerTypeNode;
class FunctionTypeNode;
class ErrorTypeNode;
class GenericTypeNode;
class VarDecl;
class ConstDecl;
class FuncDecl;
class StructDecl;
class EnumDecl;
class TraitDecl;
class ImplDecl;
class TypeAliasDecl;
class BlockStmt;
class ReturnStmt;
class IfStmt;
class WhileStmt;
class LoopStmt;
class ForStmt;
class MatchStmt;
class DeferStmt;
class BreakStmt;
class ContinueStmt;
class IntegerLiteralExpr;
class FloatLiteralExpr;
class BoolLiteralExpr;
class CharLiteralExpr;
class StringLiteralExpr;
class NoneLiteralExpr;
class IdentifierExpr;
class BinaryExpr;
class UnaryExpr;
class AssignExpr;
class CallExpr;
class BuiltinCallExpr;
class MemberExpr;
class IndexExpr;
class SliceExpr;
class CastExpr;
class IfExpr;
class MatchExpr;
class ClosureExpr;
class ArrayExpr;
class TupleExpr;
class StructExpr;
class RangeExpr;
class AwaitExpr;
class ErrorPropagateExpr;
class ErrorHandleExpr;
class Pattern;

/// \brief 编译单元
///
/// 表示一个源文件的编译单元，包含所有顶层声明
class CompilationUnit {
public:
    /// \brief 构造编译单元
    /// \param fileID 源文件 ID
    explicit CompilationUnit(unsigned fileID) : FileID(fileID) {}
    
    /// \brief 获取源文件 ID
    unsigned getFileID() const { return FileID; }
    
    /// \brief 添加声明
    void addDecl(Decl* decl) { Decls.push_back(decl); }
    
    /// \brief 获取所有声明
    const std::vector<Decl*>& getDecls() const { return Decls; }
    
private:
    unsigned FileID;                ///< 源文件 ID
    std::vector<Decl*> Decls;       ///< 顶层声明列表
};

/// \brief 语义分析器
///
/// Yuan 语言的语义分析器，负责：
/// - 构建和维护符号表
/// - 类型检查和类型推断
/// - 语义验证（如变量使用前声明、类型兼容性等）
/// - 错误报告
class Sema {
public:
    /// \brief 构造语义分析器
    /// \param ctx AST 上下文
    /// \param diag 诊断引擎
    Sema(ASTContext& ctx, DiagnosticEngine& diag);
    
    /// \brief 析构函数
    ~Sema();
    
    /// \brief 分析整个编译单元
    /// \param unit 编译单元
    /// \return 成功返回 true，有错误返回 false
    bool analyze(CompilationUnit* unit);
    
    /// \brief 分析单个声明
    /// \param decl 声明节点
    /// \return 成功返回 true，有错误返回 false
    bool analyzeDecl(Decl* decl);
    
    /// \brief 分析单个语句
    /// \param stmt 语句节点
    /// \return 成功返回 true，有错误返回 false
    bool analyzeStmt(Stmt* stmt);
    
    /// \brief 分析单个表达式并返回其类型
    /// \param expr 表达式节点
    /// \return 表达式的类型，出错返回 nullptr
    Type* analyzeExpr(Expr* expr);
    
    /// \brief 解析类型节点为语义类型
    /// \param node 类型节点
    /// \return 解析后的类型，出错返回 nullptr
    Type* resolveType(TypeNode* node);
    
    /// \brief 获取 AST 上下文
    ASTContext& getContext() { return Ctx; }
    const ASTContext& getContext() const { return Ctx; }
    
    /// \brief 获取符号表
    SymbolTable& getSymbolTable() { return Symbols; }
    const SymbolTable& getSymbolTable() const { return Symbols; }
    
    /// \brief 获取诊断引擎
    DiagnosticEngine& getDiagnostics() { return Diag; }
    const DiagnosticEngine& getDiagnostics() const { return Diag; }
    
    /// \brief 报告错误
    /// \param id 诊断 ID
    /// \param loc 位置
    void reportError(DiagID id, SourceLocation loc);
    
    /// \brief 报告注释
    /// \param id 诊断 ID
    /// \param loc 位置
    void reportNote(DiagID id, SourceLocation loc);

    /// \brief 报告警告
    /// \param id 诊断 ID
    /// \param loc 位置
    void reportWarning(DiagID id, SourceLocation loc);

    /// \brief 获取模块管理器
    ModuleManager& getModuleManager() { return *ModuleMgr; }

    /// \brief 模块解析（供 ImportBuiltin 调用）
    /// \param modulePath 模块路径
    /// \param loc 调用位置
    /// \return 模块类型，出错返回 nullptr
    Type* resolveModuleType(const std::string& modulePath, SourceLocation loc);

private:
    /// \brief 注册内置 Trait（Display/Debug）
    void registerBuiltinTraits();

    /// \brief 进入泛型参数作用域并注册泛型参数
    /// \return 成功返回 true
    bool enterGenericParamScope(const std::vector<GenericParam>& params);

    /// \brief 退出泛型参数作用域（与 enterGenericParamScope 成对使用）
    void exitGenericParamScope();

    /// \brief 在类型中替换泛型参数
    Type* substituteType(Type* type, const std::unordered_map<std::string, Type*>& mapping);

    /// \brief 根据实参与形参类型推导泛型参数
    bool unifyGenericTypes(Type* expected, Type* actual,
                           std::unordered_map<std::string, Type*>& mapping);

    /// \brief 构建泛型参数到类型实参的映射
    bool buildGenericSubstitution(Type* baseType,
                                  const std::vector<Type*>& typeArgs,
                                  std::unordered_map<std::string, Type*>& mapping);

    /// \brief 获取期望类型的枚举基类型（去除引用/指针/泛型实例包装）
    EnumType* getExpectedEnumType(Type* type);

    /// \brief 在已知期望枚举类型时，应用未限定枚举变体的语法糖
    Expr* applyEnumVariantSugar(Expr* expr, Type* expectedType);

    ASTContext& Ctx;                ///< AST 上下文
    DiagnosticEngine& Diag;         ///< 诊断引擎
    SymbolTable Symbols;            ///< 符号表
    std::unique_ptr<TypeChecker> TypeCheckerImpl; ///< 类型检查器
    std::unique_ptr<ModuleManager> ModuleMgr;  ///< 模块管理器
    std::vector<std::string> ImportChain;      ///< 导入链（用于检测循环导入）
    std::unordered_map<const Type*, std::unordered_set<std::string>> ImplTraitMap;
    
    // ========================================================================
    // 类型解析辅助方法
    // ========================================================================
    
    /// \brief 解析内置类型
    Type* resolveBuiltinType(BuiltinTypeNode* node);
    
    /// \brief 解析标识符类型
    Type* resolveIdentifierType(IdentifierTypeNode* node);
    
    /// \brief 解析数组类型
    Type* resolveArrayType(ArrayTypeNode* node);
    
    /// \brief 解析切片类型
    Type* resolveSliceType(SliceTypeNode* node);
    
    /// \brief 解析元组类型
    Type* resolveTupleType(TupleTypeNode* node);
    
    /// \brief 解析可选类型
    Type* resolveOptionalType(OptionalTypeNode* node);
    
    /// \brief 解析引用类型
    Type* resolveReferenceType(ReferenceTypeNode* node);
    
    /// \brief 解析指针类型
    Type* resolvePointerType(PointerTypeNode* node);
    
    /// \brief 解析函数类型
    Type* resolveFunctionType(FunctionTypeNode* node);
    
    /// \brief 解析错误类型
    Type* resolveErrorType(ErrorTypeNode* node);
    
    /// \brief 解析泛型类型
    Type* resolveGenericType(GenericTypeNode* node);
    
    // ========================================================================
    // 声明分析
    // ========================================================================
    
    /// \brief 分析变量声明
    bool analyzeVarDecl(VarDecl* decl);
    
    /// \brief 分析常量声明
    bool analyzeConstDecl(ConstDecl* decl);
    
    /// \brief 分析函数声明
    bool analyzeFuncDecl(FuncDecl* decl);
    
    /// \brief 分析结构体声明
    bool analyzeStructDecl(StructDecl* decl);
    
    /// \brief 分析枚举声明
    bool analyzeEnumDecl(EnumDecl* decl);
    
    /// \brief 分析 Trait 声明
    bool analyzeTraitDecl(TraitDecl* decl);

    /// \brief 分析类型别名声明
    bool analyzeTypeAliasDecl(TypeAliasDecl* decl);

    /// \brief 分析 Impl 块声明
    bool analyzeImplDecl(ImplDecl* decl);
    
    // ========================================================================
    // 语句分析
    // ========================================================================
    
    /// \brief 分析块语句
    bool analyzeBlockStmt(BlockStmt* stmt);
    
    /// \brief 分析 return 语句
    bool analyzeReturnStmt(ReturnStmt* stmt);
    
    /// \brief 分析 if 语句
    bool analyzeIfStmt(IfStmt* stmt);
    
    /// \brief 分析 while 语句
    bool analyzeWhileStmt(WhileStmt* stmt);
    
    /// \brief 分析 loop 语句
    bool analyzeLoopStmt(LoopStmt* stmt);
    
    /// \brief 分析 for 语句
    bool analyzeForStmt(ForStmt* stmt);
    
    /// \brief 分析 match 语句
    bool analyzeMatchStmt(MatchStmt* stmt);
    
    /// \brief 分析 defer 语句
    bool analyzeDeferStmt(DeferStmt* stmt);
    
    /// \brief 分析 break 语句
    bool analyzeBreakStmt(BreakStmt* stmt);
    
    /// \brief 分析 continue 语句
    bool analyzeContinueStmt(ContinueStmt* stmt);
    
    // ========================================================================
    // 表达式分析
    // ========================================================================
    
    /// \brief 分析整数字面量
    Type* analyzeIntegerLiteral(IntegerLiteralExpr* expr);
    
    /// \brief 分析浮点数字面量
    Type* analyzeFloatLiteral(FloatLiteralExpr* expr);
    
    /// \brief 分析布尔字面量
    Type* analyzeBoolLiteral(BoolLiteralExpr* expr);
    
    /// \brief 分析字符字面量
    Type* analyzeCharLiteral(CharLiteralExpr* expr);
    
    /// \brief 分析字符串字面量
    Type* analyzeStringLiteral(StringLiteralExpr* expr);
    
    /// \brief 分析 None 字面量
    Type* analyzeNoneLiteral(NoneLiteralExpr* expr);
    
    /// \brief 分析标识符表达式
    Type* analyzeIdentifier(IdentifierExpr* expr);
    
    /// \brief 分析二元表达式
    Type* analyzeBinaryExpr(BinaryExpr* expr);
    
    /// \brief 分析一元表达式
    Type* analyzeUnaryExpr(UnaryExpr* expr);
    
    /// \brief 分析赋值表达式
    Type* analyzeAssignExpr(AssignExpr* expr);
    
    /// \brief 分析函数调用表达式
    Type* analyzeCallExpr(CallExpr* expr);
    
    /// \brief 分析内置函数调用表达式（委托给 BuiltinRegistry）
    Type* analyzeBuiltinCallExpr(BuiltinCallExpr* expr);
    
    /// \brief 分析成员访问表达式
    Type* analyzeMemberExpr(MemberExpr* expr);
    
    /// \brief 分析索引表达式
    Type* analyzeIndexExpr(IndexExpr* expr);
    
    /// \brief 分析切片表达式
    Type* analyzeSliceExpr(SliceExpr* expr);
    
    /// \brief 分析类型转换表达式
    Type* analyzeCastExpr(CastExpr* expr);
    
    /// \brief 分析 if 表达式
    Type* analyzeIfExpr(IfExpr* expr);

    /// \brief 分析块表达式
    Type* analyzeBlockExpr(class BlockExpr* expr);
    
    /// \brief 分析 match 表达式
    Type* analyzeMatchExpr(MatchExpr* expr);
    
    /// \brief 分析闭包表达式
    Type* analyzeClosureExpr(ClosureExpr* expr);
    
    /// \brief 分析数组表达式
    Type* analyzeArrayExpr(ArrayExpr* expr);
    
    /// \brief 分析元组表达式
    Type* analyzeTupleExpr(TupleExpr* expr);
    
    /// \brief 分析结构体表达式
    Type* analyzeStructExpr(StructExpr* expr);
    
    /// \brief 分析范围表达式
    Type* analyzeRangeExpr(RangeExpr* expr);

    /// \brief 分析 await 表达式
    Type* analyzeAwaitExpr(AwaitExpr* expr);
    
    /// \brief 分析错误传播表达式
    Type* analyzeErrorPropagateExpr(ErrorPropagateExpr* expr);
    
    /// \brief 分析错误处理表达式
    Type* analyzeErrorHandleExpr(ErrorHandleExpr* expr);
    
    // ========================================================================
    // 类型检查辅助函数
    // ========================================================================
    
    /// \brief 检查类型兼容性
    /// \param expected 期望类型
    /// \param actual 实际类型
    /// \param loc 位置（用于错误报告）
    /// \return 兼容返回 true
    bool checkTypeCompatible(Type* expected, Type* actual, SourceLocation loc);
    bool checkTypeCompatible(Type* expected, Type* actual, SourceRange range);
    
    /// \brief 检查表达式是否可赋值（左值检查）
    /// \param target 目标表达式
    /// \param loc 位置（用于错误报告）
    /// \return 可赋值返回 true
    bool checkAssignable(Expr* target, SourceLocation loc);
    
    /// \brief 检查表达式是否可变
    /// \param target 目标表达式
    /// \param loc 位置（用于错误报告）
    /// \return 可变返回 true
    bool checkMutable(Expr* target, SourceLocation loc);
    
    /// \brief 获取两个类型的公共类型
    /// \param t1 类型1
    /// \param t2 类型2
    /// \return 公共类型，无法确定返回 nullptr
    Type* getCommonType(Type* t1, Type* t2);
    
    // ========================================================================
    // 模式分析
    // ========================================================================
    
    /// \brief 分析模式
    /// \param pattern 模式节点
    /// \param expectedType 期望类型
    /// \return 成功返回 true
    bool analyzePattern(Pattern* pattern, Type* expectedType);
    
    /// \brief 检查 match 表达式的穷尽性
    /// \param match match 语句或表达式
    /// \return 穷尽返回 true
    bool checkExhaustive(MatchStmt* match);
    
    // ========================================================================
    // Trait 检查
    // ========================================================================
    
    /// \brief 检查 Trait 实现
    /// \param impl Impl 块
    /// \return 实现正确返回 true
    bool checkTraitImpl(ImplDecl* impl);
    
    /// \brief 检查方法签名是否匹配
    /// \param traitMethod Trait 中的方法声明
    /// \param implMethod Impl 中的方法实现
    /// \param targetTypeNode 目标类型节点
    /// \return 签名匹配返回 true
    bool checkMethodSignatureMatch(FuncDecl* traitMethod, FuncDecl* implMethod, TypeNode* targetTypeNode);
    
    /// \brief 检查类型是否满足 Trait 约束
    /// \param type 类型
    /// \param trait Trait 声明
    /// \return 满足约束返回 true
    bool checkTraitBound(Type* type, TraitDecl* trait);
    
    // ========================================================================
    // 错误处理检查
    // ========================================================================

    /// \brief 检查函数的错误处理
    /// \param func 函数声明
    /// \return 错误处理正确返回 true
    bool checkErrorHandling(FuncDecl* func);

    /// \brief 检查返回值是否被使用
    /// \param expr 表达式
    /// \param loc 位置
    /// \return 使用正确返回 true
    bool checkUnusedResult(Expr* expr, SourceLocation loc);

    // ========================================================================
    // 常量求值
    // ========================================================================

    /// \brief 求值编译时常量表达式
    /// \param expr 表达式
    /// \param result 输出结果值
    /// \return 成功返回 true
    bool evaluateConstExpr(Expr* expr, int64_t& result);

    // 禁止拷贝和移动
    Sema(const Sema&) = delete;
    Sema& operator=(const Sema&) = delete;
    Sema(Sema&&) = delete;
    Sema& operator=(Sema&&) = delete;
};

} // namespace yuan

#endif // YUAN_SEMA_SEMA_H
