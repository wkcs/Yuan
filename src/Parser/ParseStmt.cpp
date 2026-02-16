/// \file ParseStmt.cpp
/// \brief 语句解析实现。
///
/// 本文件实现了 Parser 类中所有语句解析相关的方法，
/// 包括基本语句、控制流语句、跳转语句和延迟语句的解析。

#include "yuan/Parser/Parser.h"
#include "yuan/Lexer/Lexer.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/TokenKinds.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Stmt.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Pattern.h"
#include "yuan/AST/Decl.h"

namespace yuan {

// ============================================================================
// 基本语句解析
// ============================================================================

ParseResult<Stmt> Parser::parseStmt() {
    // 处理带标签的循环语句: label: for/while/loop ...
    if (check(TokenKind::Identifier) && peekAhead(1).is(TokenKind::Colon)) {
        Token labelTok = CurTok;
        Token afterColon = peekAhead(2);
        if (afterColon.is(TokenKind::KW_for) ||
            afterColon.is(TokenKind::KW_while) ||
            afterColon.is(TokenKind::KW_loop)) {
            std::string label = std::string(labelTok.getText());
            advance(); // consume label
            expectAndConsume(TokenKind::Colon);

            switch (CurTok.getKind()) {
                case TokenKind::KW_for:
                    return parseForStmt(label);
                case TokenKind::KW_while:
                    return parseWhileStmt(label);
                case TokenKind::KW_loop:
                    return parseLoopStmt(label);
                default:
                    break;
            }
        }
    }

    switch (CurTok.getKind()) {
        case TokenKind::LBrace:
            return parseBlockStmt();
        case TokenKind::KW_var: {
            auto declResult = parseVarDecl();
            if (declResult.isError()) {
                return ParseResult<Stmt>::error();
            }
            SourceRange range = declResult.get()->getRange();
            return ParseResult<Stmt>(Ctx.create<DeclStmt>(range, declResult.get()));
        }
        case TokenKind::KW_const: {
            auto declResult = parseConstDecl();
            if (declResult.isError()) {
                return ParseResult<Stmt>::error();
            }
            SourceRange range = declResult.get()->getRange();
            return ParseResult<Stmt>(Ctx.create<DeclStmt>(range, declResult.get()));
        }
        case TokenKind::KW_func: {
            // 需要区分嵌套函数定义和闭包表达式
            // 如果 func 后面是标识符，则是函数定义
            // 如果 func 后面是 ( 或 <，则是闭包表达式
            Token next = peekAhead(1);
            if (next.is(TokenKind::Identifier)) {
                // 嵌套函数定义: func name(...) { ... }
                auto declResult = parseFuncDecl();
                if (declResult.isError()) {
                    return ParseResult<Stmt>::error();
                }
                SourceRange range = declResult.get()->getRange();
                return ParseResult<Stmt>(Ctx.create<DeclStmt>(range, declResult.get()));
            } else {
                // 闭包表达式: func(...) { ... } 或 func<T>(...) { ... }
                // 作为表达式语句处理
                return parseExprStmt();
            }
        }
        case TokenKind::KW_return:
            return parseReturnStmt();
        case TokenKind::KW_if:
            return parseIfStmt();
        case TokenKind::KW_while:
            return parseWhileStmt();
        case TokenKind::KW_loop:
            return parseLoopStmt();
        case TokenKind::KW_for:
            return parseForStmt();
        case TokenKind::KW_match:
            return parseMatchStmt();
        case TokenKind::KW_break:
            return parseBreakStmt();
        case TokenKind::KW_continue:
            return parseContinueStmt();
        case TokenKind::KW_defer:
            return parseDeferStmt();
        default:
            // 默认解析为表达式语句
            return parseExprStmt();
    }
}

ParseResult<Stmt> Parser::parseExprStmt() {
    SourceLocation startLoc = CurTok.getLocation();

    // 解析表达式
    auto exprResult = parseExpr();
    if (exprResult.isError()) {
        return ParseResult<Stmt>::error();
    }

    Expr* expr = exprResult.get();

    // 检查表达式是否适合作为语句
    if (!isValidExprStmt(expr)) {
        // Allow a pure expression as the last statement in a block (block value).
        if (!check(TokenKind::RBrace)) {
            reportError(DiagID::err_expression_statement_no_effect);
            return ParseResult<Stmt>::error();
        }
    }

    // Yuan语言不需要分号，表达式语句以换行结束
    SourceRange range(startLoc, PrevTok.getLocation());
    return ParseResult<Stmt>(Ctx.create<ExprStmt>(range, expr));
}

bool Parser::isValidExprStmt(Expr* expr) const {
    if (!expr) {
        return false;
    }

    // 只允许有副作用的表达式作为语句
    switch (expr->getKind()) {
        case ASTNode::Kind::CallExpr:           // 函数调用
        case ASTNode::Kind::BuiltinCallExpr:    // 内置函数调用
        case ASTNode::Kind::AssignExpr:         // 赋值表达式
        case ASTNode::Kind::BinaryExpr:         // 二元表达式（包含赋值等副作用）
        case ASTNode::Kind::IfExpr:             // if 表达式
        case ASTNode::Kind::MatchExpr:          // match 表达式
        case ASTNode::Kind::ClosureExpr:        // 闭包表达式
        case ASTNode::Kind::StructExpr:         // 结构体构造
        case ASTNode::Kind::ArrayExpr:          // 数组构造
        case ASTNode::Kind::TupleExpr:          // 元组构造
        case ASTNode::Kind::UnaryExpr:          // 一元表达式（可能有副作用）
        case ASTNode::Kind::RangeExpr:          // 范围表达式
            return true;

        // 单独的标识符和字面量是无意义的
        case ASTNode::Kind::IdentifierExpr:
        case ASTNode::Kind::IntegerLiteralExpr:
        case ASTNode::Kind::FloatLiteralExpr:
        case ASTNode::Kind::BoolLiteralExpr:
        case ASTNode::Kind::CharLiteralExpr:
        case ASTNode::Kind::StringLiteralExpr:
            return false;

        // 成员访问和索引访问本身没有副作用，但可能是链式调用的一部分
        // 为了保持灵活性，暂时允许
        case ASTNode::Kind::MemberExpr:
        case ASTNode::Kind::IndexExpr:
        case ASTNode::Kind::CastExpr:
            return true;

        default:
            // 其他表达式暂时允许
            return true;
    }
}

ParseResult<Stmt> Parser::parseBlockStmt() {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 期望左花括号
    if (!expectAndConsume(TokenKind::LBrace, DiagID::err_expected_lbrace)) {
        return ParseResult<Stmt>::error();
    }
    
    std::vector<Stmt*> stmts;
    
    // 解析语句序列
    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        auto stmtResult = parseStmt();
        if (stmtResult.isSuccess()) {
            Stmt* stmt = stmtResult.get();
            stmts.push_back(stmt);

            // 语句必须由换行分隔（同一行紧邻的下一个语句起始 token 视为语法错误）
            if (!check(TokenKind::RBrace) && !isAtEnd()) {
                SourceLocation stmtEnd = stmt->getEndLoc();
                SourceLocation nextLoc = CurTok.getLocation();
                if (!Lex.isNewLineBetween(stmtEnd, nextLoc)) {
                    reportError(DiagID::err_unexpected_token);
                    return ParseResult<Stmt>::error();
                }
            }
        } else {
            // 错误恢复：跳到下一个语句
            synchronize();
        }
    }
    
    // 期望右花括号
    if (!expectAndConsume(TokenKind::RBrace, DiagID::err_expected_rbrace)) {
        return ParseResult<Stmt>::error();
    }
    
    SourceRange range(startLoc, PrevTok.getLocation());
    return ParseResult<Stmt>(Ctx.create<BlockStmt>(range, std::move(stmts)));
}

ParseResult<Stmt> Parser::parseReturnStmt() {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 'return' 关键字
    advance();
    
    Expr* value = nullptr;
    
    // 检查是否有返回值（如果不是文件结束且不是语句结束符）
    // 不解析的情况: 块结束、文件结束、或下一个token是语句关键字
    if (!isAtEnd() &&
        CurTok.getKind() != TokenKind::RBrace &&  // 不是块结束
        CurTok.getKind() != TokenKind::EndOfFile &&
        CurTok.getKind() != TokenKind::KW_return &&
        CurTok.getKind() != TokenKind::KW_if &&
        CurTok.getKind() != TokenKind::KW_while &&
        CurTok.getKind() != TokenKind::KW_loop &&
        CurTok.getKind() != TokenKind::KW_for &&
        CurTok.getKind() != TokenKind::KW_break &&
        CurTok.getKind() != TokenKind::KW_continue &&
        CurTok.getKind() != TokenKind::KW_defer &&
        CurTok.getKind() != TokenKind::KW_var &&
        CurTok.getKind() != TokenKind::KW_const) {
        auto exprResult = parseExpr();
        if (exprResult.isError()) {
            return ParseResult<Stmt>::error();
        }
        value = exprResult.get();
    }
    
    // Yuan语言不需要分号
    SourceRange range(startLoc, PrevTok.getLocation());
    return ParseResult<Stmt>(Ctx.create<ReturnStmt>(range, value));
}

// ============================================================================
// 控制流语句解析
// ============================================================================

ParseResult<Stmt> Parser::parseIfStmt() {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 'if' 关键字
    advance();
    
    std::vector<IfStmt::Branch> branches;
    
    // 解析第一个分支（if 分支）
    auto conditionResult = parseExpr();
    if (conditionResult.isError()) {
        return ParseResult<Stmt>::error();
    }
    
    auto bodyResult = parseBlockStmt();
    if (bodyResult.isError()) {
        return ParseResult<Stmt>::error();
    }
    
    branches.push_back({conditionResult.get(), 
                       static_cast<BlockStmt*>(bodyResult.get())});
    
    // 解析 elif 分支
    while (match(TokenKind::KW_elif)) {
        auto elifCondResult = parseExpr();
        if (elifCondResult.isError()) {
            return ParseResult<Stmt>::error();
        }
        
        auto elifBodyResult = parseBlockStmt();
        if (elifBodyResult.isError()) {
            return ParseResult<Stmt>::error();
        }
        
        branches.push_back({elifCondResult.get(),
                           static_cast<BlockStmt*>(elifBodyResult.get())});
    }
    
    // 解析可选的 else 分支
    if (match(TokenKind::KW_else)) {
        auto elseBodyResult = parseBlockStmt();
        if (elseBodyResult.isError()) {
            return ParseResult<Stmt>::error();
        }
        
        // else 分支的条件为 nullptr
        branches.push_back({nullptr, 
                           static_cast<BlockStmt*>(elseBodyResult.get())});
    }
    
    SourceRange range(startLoc, PrevTok.getLocation());
    return ParseResult<Stmt>(Ctx.create<IfStmt>(range, std::move(branches)));
}

ParseResult<Stmt> Parser::parseWhileStmt(const std::string& label) {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 'while' 关键字
    advance();
    
    // 解析循环条件
    auto conditionResult = parseExpr();
    if (conditionResult.isError()) {
        return ParseResult<Stmt>::error();
    }
    
    // 解析循环体
    auto bodyResult = parseBlockStmt();
    if (bodyResult.isError()) {
        return ParseResult<Stmt>::error();
    }
    
    SourceRange range(startLoc, PrevTok.getLocation());
    return ParseResult<Stmt>(Ctx.create<WhileStmt>(range,
                                                   conditionResult.get(),
                                                   static_cast<BlockStmt*>(bodyResult.get()),
                                                   label));
}

ParseResult<Stmt> Parser::parseLoopStmt(const std::string& label) {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 'loop' 关键字
    advance();
    
    // 解析循环体
    auto bodyResult = parseBlockStmt();
    if (bodyResult.isError()) {
        return ParseResult<Stmt>::error();
    }
    
    SourceRange range(startLoc, PrevTok.getLocation());
    return ParseResult<Stmt>(Ctx.create<LoopStmt>(range,
                                                  static_cast<BlockStmt*>(bodyResult.get()),
                                                  label));
}

ParseResult<Stmt> Parser::parseForStmt(const std::string& label) {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 'for' 关键字
    advance();
    
    // 解析循环变量模式
    auto patternResult = parsePattern();
    if (patternResult.isError()) {
        return ParseResult<Stmt>::error();
    }
    
    // 期望 'in' 关键字
    if (!expectAndConsume(TokenKind::KW_in, DiagID::err_expected_in)) {
        return ParseResult<Stmt>::error();
    }
    
    // 解析可迭代表达式（避免把循环体误解析为结构体字面量）
    bool prevAllowStructLiteral = AllowStructLiteral;
    AllowStructLiteral = false;
    auto iterableResult = parseExpr();
    AllowStructLiteral = prevAllowStructLiteral;
    if (iterableResult.isError()) {
        return ParseResult<Stmt>::error();
    }
    
    // 解析循环体
    auto bodyResult = parseBlockStmt();
    if (bodyResult.isError()) {
        return ParseResult<Stmt>::error();
    }
    
    SourceRange range(startLoc, PrevTok.getLocation());
    return ParseResult<Stmt>(Ctx.create<ForStmt>(range,
                                                 patternResult.get(),
                                                 iterableResult.get(),
                                                 static_cast<BlockStmt*>(bodyResult.get()),
                                                 label));
}

ParseResult<Stmt> Parser::parseMatchStmt() {
    SourceLocation startLoc = CurTok.getLocation();

    // 消费 'match' 关键字
    advance();

    // 解析被匹配的表达式
    // 使用 parseUnaryExpr 而不是 parseExpr，避免解析后缀表达式
    // 这样可以防止 x { .. } 被误解析为结构体字面量
    auto scrutineeResult = parseUnaryExpr();
    if (scrutineeResult.isError()) {
        return ParseResult<Stmt>::error();
    }
    Expr* scrutinee = scrutineeResult.get();
    // 期望左花括号
    if (!expectAndConsume(TokenKind::LBrace, DiagID::err_expected_lbrace)) {
        return ParseResult<Stmt>::error();
    }
    
    std::vector<MatchStmt::Arm> arms;
    
    // 解析匹配分支
    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        // 解析模式
        auto patternResult = parsePattern();
        if (patternResult.isError()) {
            synchronize();
            continue;
        }
        
        Expr* guard = nullptr;
        
        // 解析可选的守卫条件
        if (match(TokenKind::KW_if)) {
            auto guardResult = parseExpr();
            if (guardResult.isError()) {
                synchronize();
                continue;
            }
            guard = guardResult.get();
        }
        
        // 期望 '=>' 
        if (!expectAndConsume(TokenKind::FatArrow, DiagID::err_expected_fat_arrow)) {
            synchronize();
            continue;
        }
        
        // 解析分支体（可以是块语句或表达式语句）
        Stmt* body = nullptr;
        if (check(TokenKind::LBrace)) {
            auto blockResult = parseBlockStmt();
            if (blockResult.isError()) {
                synchronize();
                continue;
            }
            body = blockResult.get();
        } else {
            // 表达式后跟逗号或右花括号
            auto exprResult = parseExpr();
            if (exprResult.isError()) {
                synchronize();
                continue;
            }
            
            SourceRange exprRange = exprResult.get()->getRange();
            body = Ctx.create<ExprStmt>(exprRange, exprResult.get());
        }
        
        arms.push_back({patternResult.get(), guard, body});
        
        // 可选的逗号
        match(TokenKind::Comma);
    }
    
    // 期望右花括号
    if (!expectAndConsume(TokenKind::RBrace, DiagID::err_expected_rbrace)) {
        return ParseResult<Stmt>::error();
    }
    
    SourceRange range(startLoc, PrevTok.getLocation());
    return ParseResult<Stmt>(Ctx.create<MatchStmt>(range,
                                                   scrutinee,
                                                   std::move(arms)));
}

// ============================================================================
// 跳转和延迟语句解析
// ============================================================================

ParseResult<Stmt> Parser::parseBreakStmt() {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 'break' 关键字
    advance();
    
    std::string label;
    
    // 可选的标签：break label
    if (check(TokenKind::Identifier)) {
        label = std::string(CurTok.getText());
        advance();
    }

    // Yuan语言不需要分号
    SourceRange range(startLoc, PrevTok.getLocation());
    return ParseResult<Stmt>(Ctx.create<BreakStmt>(range, label));
}

ParseResult<Stmt> Parser::parseContinueStmt() {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 'continue' 关键字
    advance();
    
    std::string label;
    
    // 可选的标签：continue label
    if (check(TokenKind::Identifier)) {
        label = std::string(CurTok.getText());
        advance();
    }
    
    // Yuan语言不需要分号
    SourceRange range(startLoc, PrevTok.getLocation());
    return ParseResult<Stmt>(Ctx.create<ContinueStmt>(range, label));
}

ParseResult<Stmt> Parser::parseDeferStmt() {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 'defer' 关键字
    advance();
    
    // 解析延迟执行的语句
    // defer 后面可以跟块语句或单个语句
    auto bodyResult = parseStmt();
    if (bodyResult.isError()) {
        return ParseResult<Stmt>::error();
    }
    
    SourceRange range(startLoc, PrevTok.getLocation());
    return ParseResult<Stmt>(Ctx.create<DeferStmt>(range, bodyResult.get()));
}

} // namespace yuan
