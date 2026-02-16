/// \file Parser.h
/// \brief Yuan 语法分析器接口。
///
/// 本文件定义了 Parser 类，负责将 Token 流转换为抽象语法树 (AST)。
/// Parser 使用递归下降和 Pratt 解析器技术来处理表达式的优先级。

#ifndef YUAN_PARSER_PARSER_H
#define YUAN_PARSER_PARSER_H

#include "yuan/Parser/ParseResult.h"
#include "yuan/Lexer/Token.h"
#include "yuan/AST/AST.h"
#include "yuan/AST/Expr.h"
#include "yuan/Basic/DiagnosticIDs.h"
#include <vector>
#include <string>

namespace yuan {

// 前向声明
class Lexer;
class DiagnosticEngine;
class ASTContext;
class Decl;
class Stmt;
class Expr;
class TypeNode;
class Pattern;
class BlockStmt;
class ParamDecl;
class VarDecl;
class ConstDecl;
class FuncDecl;
class StructDecl;
class EnumDecl;
class TraitDecl;
class ImplDecl;
class TypeAliasDecl;
class FieldDecl;
class EnumVariantDecl;

/// \brief Yuan 语法分析器
///
/// Parser 类负责将 Token 流转换为 AST。它使用递归下降解析器
/// 处理声明、语句和类型，使用 Pratt 解析器处理表达式以正确
/// 处理运算符优先级。
///
/// 使用示例：
/// \code
/// Lexer lexer(sm, diag, fileID);
/// ASTContext ctx(sm);
/// Parser parser(lexer, diag, ctx);
/// auto result = parser.parseCompilationUnit();
/// \endcode
class Parser {
public:
    /// \brief 构造语法分析器
    /// \param lexer 词法分析器
    /// \param diag 诊断引擎
    /// \param ctx AST 上下文
    Parser(Lexer& lexer, DiagnosticEngine& diag, ASTContext& ctx);
    
    /// \brief 析构函数
    ~Parser() = default;
    
    // =========================================================================
    // 顶层解析方法
    // =========================================================================
    
    /// \brief 解析整个编译单元
    /// \return 声明列表
    std::vector<Decl*> parseCompilationUnit();
    
    /// \brief 解析单个声明
    /// \return 解析结果
    ParseResult<Decl> parseDecl();
    
    /// \brief 解析单个语句
    /// \return 解析结果
    ParseResult<Stmt> parseStmt();
    
    /// \brief 解析单个表达式
    /// \return 解析结果
    ParseResult<Expr> parseExpr();
    
    /// \brief 解析类型
    /// \return 解析结果
    ParseResult<TypeNode> parseType();
    
    /// \brief 解析模式
    /// \return 解析结果
    ParseResult<Pattern> parsePattern();
    
    // =========================================================================
    // Token 操作方法
    // =========================================================================
    
    /// \brief 查看当前 Token（不消费）
    /// \return 当前 Token
    const Token& peek() const { return CurTok; }
    
    /// \brief 查看第 n 个 Token（不消费）
    /// \param n 向前看的位置（0 = 当前）
    /// \return 第 n 个 Token
    Token peekAhead(unsigned n = 1);
    
    /// \brief 消费当前 Token 并返回它
    /// \return 被消费的 Token
    Token consume();
    
    /// \brief 检查当前 Token 是否为指定类型
    /// \param kind 要检查的 Token 类型
    /// \return 如果匹配返回 true
    bool check(TokenKind kind) const;
    
    /// \brief 检查当前 Token 是否为指定类型之一
    /// \param k1 第一个 Token 类型
    /// \param k2 第二个 Token 类型
    /// \return 如果匹配返回 true
    bool checkOneOf(TokenKind k1, TokenKind k2) const;
    
    /// \brief 检查当前 Token 是否为指定类型之一（多个参数）
    template<typename... Ts>
    bool checkOneOf(TokenKind k1, TokenKind k2, Ts... ks) const {
        return check(k1) || checkOneOf(k2, ks...);
    }
    
    /// \brief 如果当前 Token 匹配则消费它
    /// \param kind 要匹配的 Token 类型
    /// \return 如果匹配并消费返回 true
    bool match(TokenKind kind);
    
    /// \brief 如果当前 Token 匹配指定类型之一则消费它
    /// \param k1 第一个 Token 类型
    /// \param k2 第二个 Token 类型
    /// \return 如果匹配并消费返回 true
    bool matchOneOf(TokenKind k1, TokenKind k2);
    
    /// \brief 如果当前 Token 匹配指定类型之一则消费它（多个参数）
    template<typename... Ts>
    bool matchOneOf(TokenKind k1, TokenKind k2, Ts... ks) {
        if (match(k1)) return true;
        return matchOneOf(k2, ks...);
    }
    
    /// \brief 期望当前 Token 为指定类型，否则报错
    /// \param kind 期望的 Token 类型
    /// \return 如果匹配返回 true，否则报错并返回 false
    bool expect(TokenKind kind);
    
    /// \brief 期望当前 Token 为指定类型并消费它
    /// \param kind 期望的 Token 类型
    /// \param diagID 错误时使用的诊断 ID
    /// \return 如果匹配返回 true，否则报错并返回 false
    bool expectAndConsume(TokenKind kind, DiagID diagID = DiagID::err_expected_token);
    
    /// \brief 检查是否到达文件末尾
    /// \return 如果到达文件末尾返回 true
    bool isAtEnd() const;
    
    // =========================================================================
    // 错误恢复
    // =========================================================================
    
    /// \brief 同步到下一个安全点
    ///
    /// 当遇到语法错误时，跳过 Token 直到找到一个可以安全继续解析的点，
    /// 如语句结束、块结束或声明开始。
    void synchronize();
    
    /// \brief 同步到指定的 Token 类型
    /// \param kind 要同步到的 Token 类型
    void synchronizeTo(TokenKind kind);
    
    /// \brief 跳过直到遇到指定的 Token 类型之一
    /// \param kinds 要停止的 Token 类型列表
    void skipUntil(std::initializer_list<TokenKind> kinds);
    
    /// \brief 同步到语句边界
    ///
    /// 跳过 Token 直到找到一个语句开始的位置，
    /// 如语句关键字、块开始或结束符。
    void synchronizeToStatement();
    
    /// \brief 同步到表达式边界
    ///
    /// 跳过 Token 直到找到一个表达式开始的位置，
    /// 如字面量、标识符、运算符或表达式结束符。
    void synchronizeToExpression();
    
    // =========================================================================
    // 运算符优先级
    // =========================================================================
    
    /// \brief 获取运算符优先级
    /// \param kind Token 类型
    /// \return 优先级值，-1 表示不是二元运算符
    static int getOperatorPrecedence(TokenKind kind);
    
    /// \brief 将 Token 类型转换为二元运算符
    /// \param kind Token 类型
    /// \return 对应的二元运算符，如果不是二元运算符返回 std::nullopt
    static std::optional<BinaryExpr::Op> tokenToBinaryOp(TokenKind kind);
    
    /// \brief 将 Token 类型转换为一元运算符
    /// \param kind Token 类型
    /// \return 对应的一元运算符，如果不是一元运算符返回 std::nullopt
    static std::optional<UnaryExpr::Op> tokenToUnaryOp(TokenKind kind);
    
    /// \brief 将 Token 类型转换为赋值运算符
    /// \param kind Token 类型
    /// \return 对应的赋值运算符，如果不是赋值运算符返回 std::nullopt
    static std::optional<AssignExpr::Op> tokenToAssignOp(TokenKind kind);
    
    /// \brief 检查 Token 是否为赋值运算符
    /// \param kind Token 类型
    /// \return 如果是赋值运算符返回 true
    static bool isAssignmentOp(TokenKind kind);
    
    /// \brief 检查 Token 是否为比较运算符
    /// \param kind Token 类型
    /// \return 如果是比较运算符返回 true
    static bool isComparisonOp(TokenKind kind);
    
    // =========================================================================
    // 辅助方法
    // =========================================================================
    
    /// \brief 获取 AST 上下文
    ASTContext& getContext() { return Ctx; }
    
    /// \brief 获取诊断引擎
    DiagnosticEngine& getDiagnostics() { return Diag; }
    
    /// \brief 检查是否有错误发生
    bool hasError() const;
    
private:
    // =========================================================================
    // 声明解析（在 ParseDecl.cpp 中实现）
    // =========================================================================
    
    ParseResult<Decl> parseVarDecl(Visibility vis = Visibility::Private);
    ParseResult<Decl> parseConstDecl(Visibility vis = Visibility::Private);
    ParseResult<Decl> parseFuncDecl(Visibility vis = Visibility::Private);
    ParseResult<Decl> parseStructDecl(Visibility vis = Visibility::Private);
    ParseResult<Decl> parseEnumDecl(Visibility vis = Visibility::Private);
    ParseResult<Decl> parseTraitDecl(Visibility vis = Visibility::Private);
    ParseResult<Decl> parseImplDecl();
    ParseResult<Decl> parseTypeAlias(Visibility vis = Visibility::Private);
    
    /// \brief 解析可见性修饰符
    Visibility parseVisibility();
    
    /// \brief 解析参数列表
    std::vector<ParamDecl*> parseParamList();
    
    /// \brief 解析单个参数
    ParseResult<ParamDecl> parseParam();

    /// \brief 解析泛型参数列表
    std::vector<GenericParam> parseGenericParams();

    /// \brief 解析泛型参数
    std::vector<TypeNode*> parseGenericArgs();

    /// \brief 解析 where 子句
    /// \return 解析到的约束列表（类型名 -> 约束列表）
    std::vector<std::pair<std::string, std::vector<std::string>>> parseWhereClause();

    /// \brief 解析结构体字段
    ParseResult<FieldDecl> parseFieldDecl();
    
    /// \brief 解析枚举变体
    ParseResult<EnumVariantDecl> parseEnumVariant();
    
    // =========================================================================
    // 语句解析（在 ParseStmt.cpp 中实现）
    // =========================================================================
    
    ParseResult<Stmt> parseBlockStmt();
    ParseResult<Stmt> parseReturnStmt();
    ParseResult<Stmt> parseIfStmt();
    ParseResult<Stmt> parseWhileStmt(const std::string& label = "");
    ParseResult<Stmt> parseLoopStmt(const std::string& label = "");
    ParseResult<Stmt> parseForStmt(const std::string& label = "");
    ParseResult<Stmt> parseMatchStmt();
    ParseResult<Stmt> parseDeferStmt();
    ParseResult<Stmt> parseBreakStmt();
    ParseResult<Stmt> parseContinueStmt();
    ParseResult<Stmt> parseExprStmt();
    
    // =========================================================================
    // 表达式解析（在 ParseExpr.cpp 中实现）
    // =========================================================================
    
    /// \brief 使用 Pratt 解析器解析表达式
    /// \param minPrec 最小优先级
    ParseResult<Expr> parseExprWithPrecedence(int minPrec);
    
    /// \brief 解析主表达式（字面量、标识符、括号表达式等）
    ParseResult<Expr> parsePrimaryExpr();
    
    /// \brief 解析一元表达式
    ParseResult<Expr> parseUnaryExpr();
    
    /// \brief 解析后缀表达式（调用、索引、成员访问）
    ParseResult<Expr> parsePostfixExpr(Expr* base);
    
    /// \brief 解析函数调用表达式
    ParseResult<Expr> parseCallExpr(Expr* callee, std::vector<TypeNode*> typeArgs = {});
    
    /// \brief 解析索引表达式
    ParseResult<Expr> parseIndexExpr(Expr* base);
    
    /// \brief 解析成员访问表达式
    ParseResult<Expr> parseMemberExpr(Expr* base);
    
    /// \brief 解析可选链表达式
    ParseResult<Expr> parseOptionalChainingExpr(Expr* base);
    
    /// \brief 解析 if 表达式
    ParseResult<Expr> parseIfExpr();

    /// \brief 解析 if 表达式的分支体（支持块表达式）
    ParseResult<Expr> parseIfBranchExpr();

    /// \brief 解析 match 表达式
    ParseResult<Expr> parseMatchExpr();

    /// \brief 解析 loop 表达式
    ParseResult<Expr> parseLoopExpr();

    /// \brief 解析闭包表达式
    ParseResult<Expr> parseClosureExpr();

    /// \brief 解析块表达式
    ParseResult<Expr> parseBlockExpr();

    /// \brief 解析数组表达式
    ParseResult<Expr> parseArrayExpr();
    
    /// \brief 解析元组表达式
    ParseResult<Expr> parseTupleExpr();
    
    /// \brief 解析结构体表达式
    ParseResult<Expr> parseStructExpr(const std::string& typeName,
                                      std::vector<TypeNode*> typeArgs = {});
    
    /// \brief 解析结构体表达式主体（用于后缀表达式）
    ParseResult<Expr> parseStructExprBody(SourceLocation startLoc,
                                          const std::string& typeName,
                                          std::vector<TypeNode*> typeArgs = {});
    
    /// \brief 解析内置函数调用
    ParseResult<Expr> parseBuiltinCallExpr();
    
    // =========================================================================
    // 类型解析（在 ParseType.cpp 中实现）
    // =========================================================================
    
    ParseResult<TypeNode> parseBuiltinType();
    ParseResult<TypeNode> parseArrayType();
    ParseResult<TypeNode> parseTupleType();
    ParseResult<TypeNode> parseReferenceType();
    ParseResult<TypeNode> parsePointerType();
    ParseResult<TypeNode> parseFunctionType();
    ParseResult<TypeNode> parseOptionalType();
    ParseResult<TypeNode> parseErrorType();
    ParseResult<TypeNode> parseIdentifierType();
    
    // =========================================================================
    // 模式解析（在 ParsePattern.cpp 中实现）
    // =========================================================================
    
    ParseResult<Pattern> parseTuplePattern();
    ParseResult<Pattern> parseArrayPattern();
    ParseResult<Pattern> parseStructPattern(const std::string& typeName);
    ParseResult<Pattern> parseEnumPattern(const std::string& typeName);
    ParseResult<Pattern> parseRangePattern(Expr* start);
    ParseResult<Pattern> parsePatternAtom();

    // =========================================================================
    // 语句/表达式辅助
    // =========================================================================

    /// \brief 检查表达式是否适合作为语句
    bool isValidExprStmt(Expr* expr) const;
    
    // =========================================================================
    // 错误报告
    // =========================================================================
    
    /// \brief 报告错误
    /// \param id 诊断 ID
    void reportError(DiagID id);
    
    /// \brief 报告错误（带位置）
    /// \param id 诊断 ID
    /// \param loc 错误位置
    void reportError(DiagID id, SourceLocation loc);
    
    /// \brief 报告错误（带范围）
    /// \param id 诊断 ID
    /// \param range 错误范围
    void reportError(DiagID id, SourceRange range);
    
    /// \brief 报告期望某个 Token 的错误
    /// \param expected 期望的 Token 类型
    void reportExpectedError(TokenKind expected);
    
    /// \brief 报告意外 Token 错误
    void reportUnexpectedError();
    
    /// \brief 检查当前 Token 是否可能是类型的开始
    /// \return 如果当前 Token 可能开始一个类型则返回 true
    bool isTypeStart() const;
    
    // =========================================================================
    // 成员变量
    // =========================================================================
    
    Lexer& Lex;             ///< 词法分析器
    DiagnosticEngine& Diag; ///< 诊断引擎
    ASTContext& Ctx;        ///< AST 上下文
    
    Token CurTok;           ///< 当前 Token
    Token PrevTok;          ///< 前一个 Token（用于错误恢复）

    bool AllowStructLiteral = true; ///< 是否允许解析结构体字面量

    /// \brief 前进到下一个 Token
    void advance();
};

} // namespace yuan

#endif // YUAN_PARSER_PARSER_H
