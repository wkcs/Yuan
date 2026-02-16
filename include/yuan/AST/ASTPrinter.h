/// \file ASTPrinter.h
/// \brief AST 打印器定义。
///
/// 本文件定义了 ASTPrinter 类，用于将 AST 格式化输出为有效的 Yuan 源代码。

#ifndef YUAN_AST_ASTPRINTER_H
#define YUAN_AST_ASTPRINTER_H

#include "yuan/AST/AST.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Stmt.h"
#include "yuan/AST/Type.h"
#include "yuan/AST/Pattern.h"
#include <ostream>
#include <string>

namespace yuan {

/// \brief AST 打印器
///
/// 将 AST 节点格式化输出为有效的 Yuan 源代码。
/// 支持缩进控制和格式化选项。
class ASTPrinter {
public:
    /// \brief 构造 AST 打印器
    /// \param os 输出流
    /// \param indentSize 缩进大小（空格数）
    explicit ASTPrinter(std::ostream& os, unsigned indentSize = 4);
    
    /// \brief 打印 AST 节点
    /// \param node 要打印的节点
    void print(const ASTNode* node);
    
    // ===== 声明打印 =====
    
    /// \brief 打印变量声明
    void printVarDecl(const VarDecl* decl);
    
    /// \brief 打印常量声明
    void printConstDecl(const ConstDecl* decl);
    
    /// \brief 打印参数声明
    void printParamDecl(const ParamDecl* decl);
    
    /// \brief 打印函数声明
    void printFuncDecl(const FuncDecl* decl);
    
    /// \brief 打印字段声明
    void printFieldDecl(const FieldDecl* decl);
    
    /// \brief 打印结构体声明
    void printStructDecl(const StructDecl* decl);
    
    /// \brief 打印枚举变体声明
    void printEnumVariantDecl(const EnumVariantDecl* decl);
    
    /// \brief 打印枚举声明
    void printEnumDecl(const EnumDecl* decl);
    
    /// \brief 打印类型别名声明
    void printTypeAliasDecl(const TypeAliasDecl* decl);
    
    /// \brief 打印 Trait 声明
    void printTraitDecl(const TraitDecl* decl);
    
    /// \brief 打印 Impl 块
    void printImplDecl(const ImplDecl* decl);
    
    // ===== 语句打印 =====
    
    /// \brief 打印表达式语句
    void printExprStmt(const ExprStmt* stmt);
    
    /// \brief 打印块语句
    void printBlockStmt(const BlockStmt* stmt);
    
    /// \brief 打印 return 语句
    void printReturnStmt(const ReturnStmt* stmt);
    
    /// \brief 打印 if 语句
    void printIfStmt(const IfStmt* stmt);
    
    /// \brief 打印 while 语句
    void printWhileStmt(const WhileStmt* stmt);
    
    /// \brief 打印 loop 语句
    void printLoopStmt(const LoopStmt* stmt);
    
    /// \brief 打印 for 语句
    void printForStmt(const ForStmt* stmt);
    
    /// \brief 打印 match 语句
    void printMatchStmt(const MatchStmt* stmt);
    
    /// \brief 打印 break 语句
    void printBreakStmt(const BreakStmt* stmt);
    
    /// \brief 打印 continue 语句
    void printContinueStmt(const ContinueStmt* stmt);
    
    /// \brief 打印 defer 语句
    void printDeferStmt(const DeferStmt* stmt);
    
    // ===== 表达式打印 =====
    
    /// \brief 打印整数字面量
    void printIntegerLiteralExpr(const IntegerLiteralExpr* expr);
    
    /// \brief 打印浮点数字面量
    void printFloatLiteralExpr(const FloatLiteralExpr* expr);
    
    /// \brief 打印布尔字面量
    void printBoolLiteralExpr(const BoolLiteralExpr* expr);
    
    /// \brief 打印字符字面量
    void printCharLiteralExpr(const CharLiteralExpr* expr);
    
    /// \brief 打印字符串字面量
    void printStringLiteralExpr(const StringLiteralExpr* expr);
    
    /// \brief 打印 None 字面量
    void printNoneLiteralExpr(const NoneLiteralExpr* expr);
    
    /// \brief 打印标识符表达式
    void printIdentifierExpr(const IdentifierExpr* expr);
    
    /// \brief 打印成员访问表达式
    void printMemberExpr(const MemberExpr* expr);
    
    /// \brief 打印二元表达式
    void printBinaryExpr(const BinaryExpr* expr);
    
    /// \brief 打印一元表达式
    void printUnaryExpr(const UnaryExpr* expr);
    
    /// \brief 打印赋值表达式
    void printAssignExpr(const AssignExpr* expr);
    
    /// \brief 打印函数调用表达式
    void printCallExpr(const CallExpr* expr);
    
    /// \brief 打印索引表达式
    void printIndexExpr(const IndexExpr* expr);
    
    /// \brief 打印切片表达式
    void printSliceExpr(const SliceExpr* expr);
    
    /// \brief 打印内置函数调用表达式
    void printBuiltinCallExpr(const BuiltinCallExpr* expr);
    
    /// \brief 打印 if 表达式
    void printIfExpr(const IfExpr* expr);
    
    /// \brief 打印 match 表达式
    void printMatchExpr(const MatchExpr* expr);
    
    /// \brief 打印闭包表达式
    void printClosureExpr(const ClosureExpr* expr);
    
    /// \brief 打印数组表达式
    void printArrayExpr(const ArrayExpr* expr);
    
    /// \brief 打印元组表达式
    void printTupleExpr(const TupleExpr* expr);
    
    /// \brief 打印结构体表达式
    void printStructExpr(const StructExpr* expr);
    
    /// \brief 打印范围表达式
    void printRangeExpr(const RangeExpr* expr);

    /// \brief 打印 await 表达式
    void printAwaitExpr(const AwaitExpr* expr);
    
    /// \brief 打印错误传播表达式
    void printErrorPropagateExpr(const ErrorPropagateExpr* expr);
    
    /// \brief 打印错误处理表达式
    void printErrorHandleExpr(const ErrorHandleExpr* expr);
    
    /// \brief 打印类型转换表达式
    void printCastExpr(const CastExpr* expr);
    
    // ===== 类型打印 =====
    
    /// \brief 打印类型节点
    void printTypeNode(const TypeNode* type);
    
    /// \brief 打印内置类型
    void printBuiltinTypeNode(const BuiltinTypeNode* type);
    
    /// \brief 打印标识符类型
    void printIdentifierTypeNode(const IdentifierTypeNode* type);
    
    /// \brief 打印数组类型
    void printArrayTypeNode(const ArrayTypeNode* type);
    
    /// \brief 打印切片类型
    void printSliceTypeNode(const SliceTypeNode* type);
    
    /// \brief 打印元组类型
    void printTupleTypeNode(const TupleTypeNode* type);
    
    /// \brief 打印 Optional 类型
    void printOptionalTypeNode(const OptionalTypeNode* type);
    
    /// \brief 打印引用类型
    void printReferenceTypeNode(const ReferenceTypeNode* type);
    
    /// \brief 打印指针类型
    void printPointerTypeNode(const PointerTypeNode* type);
    
    /// \brief 打印函数类型
    void printFunctionTypeNode(const FunctionTypeNode* type);
    
    /// \brief 打印错误类型
    void printErrorTypeNode(const ErrorTypeNode* type);
    
    /// \brief 打印泛型类型
    void printGenericTypeNode(const GenericTypeNode* type);
    
    // ===== 模式打印 =====
    
    /// \brief 打印模式
    void printPattern(const Pattern* pattern);
    
    /// \brief 打印通配符模式
    void printWildcardPattern(const WildcardPattern* pattern);
    
    /// \brief 打印标识符模式
    void printIdentifierPattern(const IdentifierPattern* pattern);
    
    /// \brief 打印字面量模式
    void printLiteralPattern(const LiteralPattern* pattern);
    
    /// \brief 打印元组模式
    void printTuplePattern(const TuplePattern* pattern);
    
    /// \brief 打印结构体模式
    void printStructPattern(const StructPattern* pattern);
    
    /// \brief 打印枚举模式
    void printEnumPattern(const EnumPattern* pattern);
    
    /// \brief 打印范围模式
    void printRangePattern(const RangePattern* pattern);

    /// \brief 打印或模式
    void printOrPattern(const OrPattern* pattern);

    /// \brief 打印绑定模式
    void printBindPattern(const BindPattern* pattern);
    
    // ===== 辅助方法 =====
    
    /// \brief 打印泛型参数列表
    void printGenericParams(const std::vector<GenericParam>& params);
    
    /// \brief 打印可见性修饰符
    void printVisibility(Visibility vis);
    
private:
    std::ostream& OS;       ///< 输出流
    unsigned IndentSize;    ///< 缩进大小
    unsigned IndentLevel;   ///< 当前缩进级别
    
    /// \brief 输出当前缩进
    void indent();
    
    /// \brief 增加缩进级别
    void increaseIndent() { ++IndentLevel; }
    
    /// \brief 减少缩进级别
    void decreaseIndent() { if (IndentLevel > 0) --IndentLevel; }
    
    /// \brief 打印表达式（通用分发）
    void printExpr(const Expr* expr);
    
    /// \brief 打印语句（通用分发）
    void printStmt(const Stmt* stmt);
    
    /// \brief 打印声明（通用分发）
    void printDecl(const Decl* decl);
    
    /// \brief 转义字符串中的特殊字符
    std::string escapeString(const std::string& str);
    
    /// \brief 转义字符
    std::string escapeChar(uint32_t codepoint);
};

} // namespace yuan

#endif // YUAN_AST_ASTPRINTER_H
