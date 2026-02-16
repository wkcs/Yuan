/// \file Stmt.cpp
/// \brief 语句 AST 节点实现。
///
/// 本文件实现了所有语句相关的 AST 节点。

#include "yuan/AST/Stmt.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Pattern.h"

namespace yuan {

// ============================================================================
// 基本语句
// ============================================================================

ExprStmt::ExprStmt(SourceRange range, Expr* expr)
    : Stmt(Kind::ExprStmt, range), Expression(expr) {}

BlockStmt::BlockStmt(SourceRange range, std::vector<Stmt*> stmts)
    : Stmt(Kind::BlockStmt, range), Stmts(std::move(stmts)) {}

ReturnStmt::ReturnStmt(SourceRange range, Expr* value)
    : Stmt(Kind::ReturnStmt, range), Value(value) {}


// ============================================================================
// 控制流语句
// ============================================================================

IfStmt::IfStmt(SourceRange range, std::vector<Branch> branches)
    : Stmt(Kind::IfStmt, range), Branches(std::move(branches)) {}

WhileStmt::WhileStmt(SourceRange range, Expr* condition, BlockStmt* body,
                     const std::string& label)
    : Stmt(Kind::WhileStmt, range), Condition(condition), Body(body), Label(label) {}

LoopStmt::LoopStmt(SourceRange range, BlockStmt* body, const std::string& label)
    : Stmt(Kind::LoopStmt, range), Body(body), Label(label) {}

ForStmt::ForStmt(SourceRange range, Pattern* pattern, Expr* iterable, BlockStmt* body,
                 const std::string& label)
    : Stmt(Kind::ForStmt, range), Pat(pattern), Iterable(iterable), Body(body), Label(label) {}

MatchStmt::MatchStmt(SourceRange range, Expr* scrutinee, std::vector<Arm> arms)
    : Stmt(Kind::MatchStmt, range), Scrutinee(scrutinee), Arms(std::move(arms)) {}


// ============================================================================
// 跳转和延迟语句
// ============================================================================

BreakStmt::BreakStmt(SourceRange range, const std::string& label)
    : Stmt(Kind::BreakStmt, range), Label(label) {}

ContinueStmt::ContinueStmt(SourceRange range, const std::string& label)
    : Stmt(Kind::ContinueStmt, range), Label(label) {}

DeferStmt::DeferStmt(SourceRange range, Stmt* body)
    : Stmt(Kind::DeferStmt, range), Body(body) {}

} // namespace yuan
