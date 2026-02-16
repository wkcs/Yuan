/// \file Parser.cpp
/// \brief Yuan 语法分析器核心实现。
///
/// 本文件实现了 Parser 类的核心功能，包括 Token 操作、
/// 错误恢复和运算符优先级处理。

#include "yuan/Parser/Parser.h"
#include "yuan/Lexer/Lexer.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/TokenKinds.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/Stmt.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Type.h"
#include "yuan/AST/Pattern.h"

namespace yuan {

// ============================================================================
// 构造函数
// ============================================================================

Parser::Parser(Lexer& lexer, DiagnosticEngine& diag, ASTContext& ctx)
    : Lex(lexer), Diag(diag), Ctx(ctx) {
    // 读取第一个 Token
    advance();
}

// ============================================================================
// Token 操作方法
// ============================================================================

void Parser::advance() {
    PrevTok = CurTok;
    CurTok = Lex.lex();
}

Token Parser::consume() {
    Token tok = CurTok;
    advance();
    return tok;
}

Token Parser::peekAhead(unsigned n) {
    if (n == 0) {
        return CurTok;
    }
    return Lex.peek(n - 1);
}

bool Parser::check(TokenKind kind) const {
    return CurTok.is(kind);
}

bool Parser::checkOneOf(TokenKind k1, TokenKind k2) const {
    return check(k1) || check(k2);
}

bool Parser::match(TokenKind kind) {
    if (check(kind)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::matchOneOf(TokenKind k1, TokenKind k2) {
    if (check(k1) || check(k2)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::expect(TokenKind kind) {
    if (check(kind)) {
        return true;
    }
    reportExpectedError(kind);
    return false;
}

bool Parser::expectAndConsume(TokenKind kind, DiagID diagID) {
    if (match(kind)) {
        return true;
    }
    if (diagID == DiagID::err_expected_token) {
        reportExpectedError(kind);
    } else {
        reportError(diagID);
    }
    return false;
}

bool Parser::isAtEnd() const {
    return CurTok.is(TokenKind::EndOfFile);
}

bool Parser::hasError() const {
    return Diag.hasErrors();
}

// ============================================================================
// 错误恢复
// ============================================================================

void Parser::synchronize() {
    // 如果前一个 Token 是分号，我们已经在语句边界
    if (PrevTok.is(TokenKind::Semicolon)) {
        return;
    }
    
    advance();
    
    while (!isAtEnd()) {
        // 在声明关键字处停止
        switch (CurTok.getKind()) {
            // 顶层声明关键字
            case TokenKind::KW_var:
            case TokenKind::KW_const:
            case TokenKind::KW_func:
            case TokenKind::KW_struct:
            case TokenKind::KW_enum:
            case TokenKind::KW_trait:
            case TokenKind::KW_impl:
            case TokenKind::KW_type:
            
            // 可见性修饰符
            case TokenKind::KW_pub:
            case TokenKind::KW_priv:
            case TokenKind::KW_internal:
            
            // 语句关键字
            case TokenKind::KW_if:
            case TokenKind::KW_while:
            case TokenKind::KW_loop:
            case TokenKind::KW_for:
            case TokenKind::KW_return:
            case TokenKind::KW_break:
            case TokenKind::KW_continue:
            case TokenKind::KW_defer:
            case TokenKind::KW_match:
            
            // 块结束符
            case TokenKind::RBrace:
            case TokenKind::Semicolon:
                return;
            default:
                break;
        }
        
        advance();
    }
}

void Parser::synchronizeTo(TokenKind kind) {
    while (!isAtEnd() && !check(kind)) {
        advance();
    }
}

void Parser::skipUntil(std::initializer_list<TokenKind> kinds) {
    while (!isAtEnd()) {
        for (TokenKind kind : kinds) {
            if (check(kind)) {
                return;
            }
        }
        advance();
    }
}

void Parser::synchronizeToStatement() {
    while (!isAtEnd()) {
        switch (CurTok.getKind()) {
            // 语句开始关键字
            case TokenKind::KW_var:
            case TokenKind::KW_const:
            case TokenKind::KW_if:
            case TokenKind::KW_while:
            case TokenKind::KW_loop:
            case TokenKind::KW_for:
            case TokenKind::KW_return:
            case TokenKind::KW_break:
            case TokenKind::KW_continue:
            case TokenKind::KW_defer:
            case TokenKind::KW_match:
            case TokenKind::LBrace:
            case TokenKind::RBrace:
            case TokenKind::Semicolon:
                return;
            default:
                break;
        }
        advance();
    }
}

void Parser::synchronizeToExpression() {
    while (!isAtEnd()) {
        switch (CurTok.getKind()) {
            // 表达式开始符号
            case TokenKind::Identifier:
            case TokenKind::IntegerLiteral:
            case TokenKind::FloatLiteral:
            case TokenKind::CharLiteral:
            case TokenKind::StringLiteral:
            case TokenKind::KW_true:
            case TokenKind::KW_false:
            case TokenKind::KW_None:
            case TokenKind::LParen:
            case TokenKind::LBracket:
            case TokenKind::LBrace:
            case TokenKind::KW_if:
            case TokenKind::KW_match:
            case TokenKind::At:  // 内置函数
            // 一元运算符
            case TokenKind::Minus:
            case TokenKind::Exclaim:
            case TokenKind::Tilde:
            case TokenKind::Amp:
            case TokenKind::Star:
            // 语句/表达式结束符
            case TokenKind::Semicolon:
            case TokenKind::RBrace:
            case TokenKind::RParen:
            case TokenKind::RBracket:
            case TokenKind::Comma:
                return;
            default:
                break;
        }
        advance();
    }
}

// ============================================================================
// 错误报告
// ============================================================================

void Parser::reportError(DiagID id) {
    Diag.report(id, CurTok.getLocation(), CurTok.getRange());
}

void Parser::reportError(DiagID id, SourceLocation loc) {
    Diag.report(id, loc);
}

void Parser::reportError(DiagID id, SourceRange range) {
    Diag.report(id, range.getBegin(), range);
}

void Parser::reportExpectedError(TokenKind expected) {
    Diag.report(DiagID::err_expected_token, CurTok.getLocation(), CurTok.getRange())
        << yuan::getSpelling(expected)
        << CurTok.getText();
}

void Parser::reportUnexpectedError() {
    Diag.report(DiagID::err_unexpected_token, CurTok.getLocation(), CurTok.getRange())
        << CurTok.getText();
}

// ============================================================================
// 顶层解析方法
// ============================================================================

std::vector<Decl*> Parser::parseCompilationUnit() {
    std::vector<Decl*> decls;
    
    while (!isAtEnd()) {
        auto result = parseDecl();
        if (result.isSuccess()) {
            decls.push_back(result.get());
        } else {
            // 错误恢复：跳到下一个声明
            synchronize();
        }
    }
    
    return decls;
}

ParseResult<Decl> Parser::parseDecl() {
    std::string docComment = CurTok.getDocComment();

    // 解析可见性修饰符
    Visibility vis = parseVisibility();
    
    // 根据关键字分发到具体的声明解析
    ParseResult<Decl> result;
    switch (CurTok.getKind()) {
        case TokenKind::KW_var:
            result = parseVarDecl(vis);
            break;
        case TokenKind::KW_const:
            result = parseConstDecl(vis);
            break;
        case TokenKind::KW_async:
        case TokenKind::KW_func:
            result = parseFuncDecl(vis);
            break;
        case TokenKind::KW_struct:
            result = parseStructDecl(vis);
            break;
        case TokenKind::KW_enum:
            result = parseEnumDecl(vis);
            break;
        case TokenKind::KW_trait:
            result = parseTraitDecl(vis);
            break;
        case TokenKind::KW_impl:
            result = parseImplDecl();
            break;
        case TokenKind::KW_type:
            result = parseTypeAlias(vis);
            break;
        default:
            reportError(DiagID::err_expected_declaration);
            return ParseResult<Decl>::error();
    }

    if (result.isSuccess() && !docComment.empty()) {
        result.get()->setDocComment(docComment);
    }

    return result;
}

ParseResult<Expr> Parser::parseExpr() {
    return parseExprWithPrecedence(0);
}

// ============================================================================
// 辅助方法
// ============================================================================

Visibility Parser::parseVisibility() {
    if (match(TokenKind::KW_pub)) {
        return Visibility::Public;
    }
    if (match(TokenKind::KW_priv)) {
        return Visibility::Private;
    }
    if (match(TokenKind::KW_internal)) {
        return Visibility::Internal;
    }
    return Visibility::Private;  // 默认私有
}

// ============================================================================
// 运算符优先级表
// ============================================================================

// Yuan 运算符优先级（从低到高）：
// 1:  = += -= *= /= %= &= |= ^= <<= >>=  (赋值，右结合)
// 2:  ||                                  (逻辑或)
// 3:  &&                                  (逻辑与)
// 4:  == != < <= > >=                     (比较)
// 5:  |                                   (位或)
// 6:  ^                                   (位异或)
// 7:  &                                   (位与)
// 8:  << >>                               (位移)
// 9:  + -                                 (加减)
// 10: * / %                               (乘除模)
// 11: .. ..=                              (范围)
// 12: as                                  (类型转换)
// 13: orelse                              (Optional 解包)
// 14: 一元运算符 - ! ~ & * (前缀)
// 15: 后缀运算符 () [] . ! (后缀)

int Parser::getOperatorPrecedence(TokenKind kind) {
    switch (kind) {
        // 赋值运算符（优先级 1，右结合）
        case TokenKind::Equal:
        case TokenKind::PlusEqual:
        case TokenKind::MinusEqual:
        case TokenKind::StarEqual:
        case TokenKind::SlashEqual:
        case TokenKind::PercentEqual:
        case TokenKind::AmpEqual:
        case TokenKind::PipeEqual:
        case TokenKind::CaretEqual:
        case TokenKind::LessLessEqual:
        case TokenKind::GreaterGreaterEqual:
            return 1;
        
        // 逻辑或（优先级 2）
        case TokenKind::PipePipe:
            return 2;
        
        // 逻辑与（优先级 3）
        case TokenKind::AmpAmp:
            return 3;
        
        // 比较运算符（优先级 4）
        case TokenKind::EqualEqual:
        case TokenKind::ExclaimEqual:
        case TokenKind::Less:
        case TokenKind::LessEqual:
        case TokenKind::Greater:
        case TokenKind::GreaterEqual:
            return 4;
        
        // 位或（优先级 5）
        case TokenKind::Pipe:
            return 5;
        
        // 位异或（优先级 6）
        case TokenKind::Caret:
            return 6;
        
        // 位与（优先级 7）
        case TokenKind::Amp:
            return 7;
        
        // 位移（优先级 8）
        case TokenKind::LessLess:
        case TokenKind::GreaterGreater:
            return 8;
        
        // 加减（优先级 9）
        case TokenKind::Plus:
        case TokenKind::Minus:
            return 9;
        
        // 乘除模（优先级 10）
        case TokenKind::Star:
        case TokenKind::Slash:
        case TokenKind::Percent:
            return 10;
        
        // 范围（优先级 11）
        case TokenKind::DotDot:
        case TokenKind::DotDotEqual:
            return 11;
        
        // orelse（优先级 13）
        case TokenKind::KW_orelse:
            return 13;
        
        default:
            return -1;  // 不是二元运算符
    }
}

std::optional<BinaryExpr::Op> Parser::tokenToBinaryOp(TokenKind kind) {
    switch (kind) {
        // 算术运算符
        case TokenKind::Plus:    return BinaryExpr::Op::Add;
        case TokenKind::Minus:   return BinaryExpr::Op::Sub;
        case TokenKind::Star:    return BinaryExpr::Op::Mul;
        case TokenKind::Slash:   return BinaryExpr::Op::Div;
        case TokenKind::Percent: return BinaryExpr::Op::Mod;
        
        // 位运算符
        case TokenKind::Amp:            return BinaryExpr::Op::BitAnd;
        case TokenKind::Pipe:           return BinaryExpr::Op::BitOr;
        case TokenKind::Caret:          return BinaryExpr::Op::BitXor;
        case TokenKind::LessLess:       return BinaryExpr::Op::Shl;
        case TokenKind::GreaterGreater: return BinaryExpr::Op::Shr;
        
        // 逻辑运算符
        case TokenKind::AmpAmp:   return BinaryExpr::Op::And;
        case TokenKind::PipePipe: return BinaryExpr::Op::Or;
        
        // 比较运算符
        case TokenKind::EqualEqual:   return BinaryExpr::Op::Eq;
        case TokenKind::ExclaimEqual: return BinaryExpr::Op::Ne;
        case TokenKind::Less:         return BinaryExpr::Op::Lt;
        case TokenKind::LessEqual:    return BinaryExpr::Op::Le;
        case TokenKind::Greater:      return BinaryExpr::Op::Gt;
        case TokenKind::GreaterEqual: return BinaryExpr::Op::Ge;
        
        // 范围运算符
        case TokenKind::DotDot:      return BinaryExpr::Op::Range;
        case TokenKind::DotDotEqual: return BinaryExpr::Op::RangeInclusive;
        
        // orelse
        case TokenKind::KW_orelse: return BinaryExpr::Op::OrElse;
        
        default:
            return std::nullopt;
    }
}

std::optional<UnaryExpr::Op> Parser::tokenToUnaryOp(TokenKind kind) {
    switch (kind) {
        case TokenKind::Minus:   return UnaryExpr::Op::Neg;
        case TokenKind::Exclaim: return UnaryExpr::Op::Not;
        case TokenKind::Tilde:   return UnaryExpr::Op::BitNot;
        case TokenKind::Amp:     return UnaryExpr::Op::Ref;
        case TokenKind::Star:    return UnaryExpr::Op::Deref;
        default:
            return std::nullopt;
    }
}

std::optional<AssignExpr::Op> Parser::tokenToAssignOp(TokenKind kind) {
    switch (kind) {
        case TokenKind::Equal:              return AssignExpr::Op::Assign;
        case TokenKind::PlusEqual:          return AssignExpr::Op::AddAssign;
        case TokenKind::MinusEqual:         return AssignExpr::Op::SubAssign;
        case TokenKind::StarEqual:          return AssignExpr::Op::MulAssign;
        case TokenKind::SlashEqual:         return AssignExpr::Op::DivAssign;
        case TokenKind::PercentEqual:       return AssignExpr::Op::ModAssign;
        case TokenKind::AmpEqual:           return AssignExpr::Op::BitAndAssign;
        case TokenKind::PipeEqual:          return AssignExpr::Op::BitOrAssign;
        case TokenKind::CaretEqual:         return AssignExpr::Op::BitXorAssign;
        case TokenKind::LessLessEqual:      return AssignExpr::Op::ShlAssign;
        case TokenKind::GreaterGreaterEqual: return AssignExpr::Op::ShrAssign;
        default:
            return std::nullopt;
    }
}

bool Parser::isAssignmentOp(TokenKind kind) {
    return tokenToAssignOp(kind).has_value();
}

bool Parser::isComparisonOp(TokenKind kind) {
    switch (kind) {
        case TokenKind::EqualEqual:
        case TokenKind::ExclaimEqual:
        case TokenKind::Less:
        case TokenKind::LessEqual:
        case TokenKind::Greater:
        case TokenKind::GreaterEqual:
            return true;
        default:
            return false;
    }
}

bool Parser::isTypeStart() const {
    switch (CurTok.getKind()) {
        // 基本类型关键字
        case TokenKind::KW_i8:
        case TokenKind::KW_i16:
        case TokenKind::KW_i32:
        case TokenKind::KW_i64:
        case TokenKind::KW_i128:
        case TokenKind::KW_isize:
        case TokenKind::KW_u8:
        case TokenKind::KW_u16:
        case TokenKind::KW_u32:
        case TokenKind::KW_u64:
        case TokenKind::KW_u128:
        case TokenKind::KW_usize:
        case TokenKind::KW_f32:
        case TokenKind::KW_f64:
        case TokenKind::KW_bool:
        case TokenKind::KW_char:
        case TokenKind::KW_str:
        case TokenKind::KW_void:
            return true;
        
        // 标识符（可能是自定义类型）
        case TokenKind::Identifier:
            return true;
        
        // 引用和指针类型
        case TokenKind::Amp:        // &T 或 &mut T
        case TokenKind::Star:       // *T 或 *mut T
            return true;
        
        // 数组类型 [T; N]
        case TokenKind::LBracket:
            return true;
        
        // 元组类型 (T, U, ...)
        case TokenKind::LParen:
            return true;
        
        // Optional 类型 ?T
        case TokenKind::Question:
            return true;
        
        default:
            return false;
    }
}

} // namespace yuan
