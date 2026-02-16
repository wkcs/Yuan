/// \file ParseExpr.cpp
/// \brief 表达式解析实现。
///
/// 本文件实现了 Parser 类中与表达式解析相关的方法，
/// 使用 Pratt 解析器技术处理运算符优先级。

#include "yuan/Parser/Parser.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Stmt.h"
#include "yuan/AST/Type.h"
#include "yuan/AST/Decl.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Lexer/LiteralParser.h"
#include "yuan/Lexer/Lexer.h"
#include "yuan/Builtin/BuiltinRegistry.h"

namespace yuan {

// ============================================================================
// Pratt 解析器核心
// ============================================================================

ParseResult<Expr> Parser::parseExprWithPrecedence(int minPrec) {
    // 解析左操作数（一元表达式或主表达式）
    auto leftResult = parseUnaryExpr();
    if (leftResult.isError()) {
        // 尝试同步到表达式边界
        synchronizeToExpression();
        return ParseResult<Expr>::error();
    }
    
    Expr* left = leftResult.get();
    
    // 处理二元运算符
    while (true) {
        int prec = getOperatorPrecedence(CurTok.getKind());
        if (prec < minPrec) {
            break;
        }

        // Treat a leading '+' or '-' on a new line as the start of a new statement.
        if ((CurTok.is(TokenKind::Plus) || CurTok.is(TokenKind::Minus)) &&
            Lex.isNewLineBetween(left->getEndLoc(), CurTok.getLocation())) {
            break;
        }
        
        // 检查是否为赋值运算符
        auto assignOp = tokenToAssignOp(CurTok.getKind());
        if (assignOp.has_value()) {
            // 赋值运算符是右结合的
            Token opTok = consume();
            auto rightResult = parseExprWithPrecedence(prec);
            if (rightResult.isError()) {
                // 赋值表达式错误，尝试恢复
                synchronizeToExpression();
                return ParseResult<Expr>::error();
            }
            
            SourceRange range(left->getBeginLoc(), rightResult->getEndLoc());
            left = Ctx.create<AssignExpr>(range, *assignOp, left, rightResult.get());
            continue;
        }
        
        // 检查是否为二元运算符
        auto binOp = tokenToBinaryOp(CurTok.getKind());
        if (!binOp.has_value()) {
            break;
        }
        
        Token opTok = consume();
        
        // 特殊处理范围运算符（创建 RangeExpr 而不是 BinaryExpr）
        if (*binOp == BinaryExpr::Op::Range || *binOp == BinaryExpr::Op::RangeInclusive) {
            bool isInclusive = (*binOp == BinaryExpr::Op::RangeInclusive);

            // 检查是否有右侧表达式
            // 如果下一个 token 是分隔符或语句关键字，则这是开放式范围 (如 1..)
            Expr* right = nullptr;
            SourceLocation endLoc = opTok.getRange().getEnd();

            if (!check(TokenKind::Comma) && !check(TokenKind::RBracket) &&
                !check(TokenKind::RBrace) && !check(TokenKind::RParen) &&
                !check(TokenKind::Semicolon) && !isAtEnd() &&
                !check(TokenKind::KW_var) && !check(TokenKind::KW_const) &&
                !check(TokenKind::KW_return) && !check(TokenKind::KW_if) &&
                !check(TokenKind::KW_while) && !check(TokenKind::KW_for) &&
                !check(TokenKind::KW_loop) && !check(TokenKind::KW_match) &&
                !check(TokenKind::KW_break) && !check(TokenKind::KW_continue) &&
                !check(TokenKind::KW_defer)) {
                auto rightResult = parseExprWithPrecedence(prec + 1);
                if (rightResult.isSuccess()) {
                    right = rightResult.get();
                    endLoc = right->getEndLoc();
                }
            }

            SourceRange range(left->getBeginLoc(), endLoc);
            left = Ctx.create<RangeExpr>(range, left, right, isInclusive);
            continue;
        }
        
        // 解析右操作数（使用更高的优先级以实现左结合）
        auto rightResult = parseExprWithPrecedence(prec + 1);
        if (rightResult.isError()) {
            // 二元表达式错误，尝试恢复
            synchronizeToExpression();
            return ParseResult<Expr>::error();
        }
        
        SourceRange range(left->getBeginLoc(), rightResult->getEndLoc());
        left = Ctx.create<BinaryExpr>(range, *binOp, left, rightResult.get());
    }
    
    return ParseResult<Expr>(left);
}

ParseResult<Expr> Parser::parseUnaryExpr() {
    // 检查一元运算符
    auto unaryOp = tokenToUnaryOp(CurTok.getKind());
    if (unaryOp.has_value()) {
        Token opTok = consume();
        
        // 特殊处理 &mut
        if (*unaryOp == UnaryExpr::Op::Ref && match(TokenKind::KW_mut)) {
            unaryOp = UnaryExpr::Op::RefMut;
        }
        
        auto operandResult = parseUnaryExpr();
        if (operandResult.isError()) {
            // 一元表达式操作数解析失败，尝试恢复
            synchronizeToExpression();
            return ParseResult<Expr>::error();
        }
        
        SourceRange range(opTok.getLocation(), operandResult->getEndLoc());
        return ParseResult<Expr>(
            Ctx.create<UnaryExpr>(range, *unaryOp, operandResult.get()));
    }
    
    // 解析主表达式和后缀
    auto primaryResult = parsePrimaryExpr();
    if (primaryResult.isError()) {
        return ParseResult<Expr>::error();
    }
    
    return parsePostfixExpr(primaryResult.get());
}

ParseResult<Expr> Parser::parsePrimaryExpr() {
    SourceLocation startLoc = CurTok.getLocation();
    
    switch (CurTok.getKind()) {
        // 整数字面量
        case TokenKind::IntegerLiteral: {
            Token tok = consume();
            
            // 使用 LiteralParser 解析整数值
            uint64_t value = 0;
            bool isSigned = true;
            unsigned bitWidth = 0;
            bool hasTypeSuffix = false;
            bool isPointerSizedSuffix = false;
            
            if (!LiteralParser::parseInteger(tok.getText(),
                                            value,
                                            isSigned,
                                            bitWidth,
                                            &hasTypeSuffix,
                                            &isPointerSizedSuffix)) {
                reportError(DiagID::err_invalid_number_literal, tok.getLocation());
                return ParseResult<Expr>::error();
            }
            
            SourceRange range(tok.getLocation(), tok.getRange().getEnd());
            return ParseResult<Expr>(
                Ctx.create<IntegerLiteralExpr>(
                    range, value, isSigned, bitWidth, hasTypeSuffix, isPointerSizedSuffix));
        }
        
        // 浮点数字面量
        case TokenKind::FloatLiteral: {
            Token tok = consume();
            
            // 使用 LiteralParser 解析浮点数值
            double value = 0.0;
            unsigned bitWidth = 0;
            
            if (!LiteralParser::parseFloat(tok.getText(), value, bitWidth)) {
                reportError(DiagID::err_invalid_number_literal, tok.getLocation());
                return ParseResult<Expr>::error();
            }
            
            SourceRange range(tok.getLocation(), tok.getRange().getEnd());
            return ParseResult<Expr>(
                Ctx.create<FloatLiteralExpr>(range, value, bitWidth));
        }
        
        // 布尔字面量
        case TokenKind::KW_true: {
            Token tok = consume();
            SourceRange range(tok.getLocation(), tok.getRange().getEnd());
            return ParseResult<Expr>(
                Ctx.create<BoolLiteralExpr>(range, true));
        }
        case TokenKind::KW_false: {
            Token tok = consume();
            SourceRange range(tok.getLocation(), tok.getRange().getEnd());
            return ParseResult<Expr>(
                Ctx.create<BoolLiteralExpr>(range, false));
        }
        
        // 字符字面量
        case TokenKind::CharLiteral: {
            Token tok = consume();
            
            // 使用 LiteralParser 解析字符值
            uint32_t codepoint = 0;
            
            if (!LiteralParser::parseChar(tok.getText(), codepoint)) {
                reportError(DiagID::err_invalid_character_literal, tok.getLocation());
                return ParseResult<Expr>::error();
            }
            
            SourceRange range(tok.getLocation(), tok.getRange().getEnd());
            return ParseResult<Expr>(
                Ctx.create<CharLiteralExpr>(range, codepoint));
        }
        
        // 字符串字面量
        case TokenKind::StringLiteral:
        case TokenKind::RawStringLiteral:
        case TokenKind::MultilineStringLiteral: {
            Token tok = consume();
            StringLiteralExpr::StringKind kind = StringLiteralExpr::StringKind::Normal;
            if (tok.is(TokenKind::RawStringLiteral)) {
                kind = StringLiteralExpr::StringKind::Raw;
            } else if (tok.is(TokenKind::MultilineStringLiteral)) {
                kind = StringLiteralExpr::StringKind::Multiline;
            }
            
            // 解析字符串值（处理转义序列）
            std::string value;
            if (kind == StringLiteralExpr::StringKind::Raw) {
                // 原始字符串不处理转义
                value = tok.getText();
            } else {
                // 普通字符串和多行字符串需要处理转义
                if (!LiteralParser::parseString(tok.getText(), value)) {
                    reportError(DiagID::err_invalid_string_literal, tok.getLocation());
                    return ParseResult<Expr>::error();
                }
            }
            
            SourceRange range(tok.getLocation(), tok.getRange().getEnd());
            return ParseResult<Expr>(
                Ctx.create<StringLiteralExpr>(range, value, kind));
        }
        
        // None 字面量
        case TokenKind::KW_None: {
            Token tok = consume();
            SourceRange range(tok.getLocation(), tok.getRange().getEnd());
            return ParseResult<Expr>(
                Ctx.create<NoneLiteralExpr>(range));
        }
        
        // 标识符或 Self/self
        case TokenKind::Identifier:
        case TokenKind::KW_Self:
        case TokenKind::KW_self: {
            Token tok = consume();
            std::string name = tok.getText();
            
            // 检查是否为结构体表达式 Name { ... }
            if (AllowStructLiteral && check(TokenKind::LBrace)) {
                // 向前看，检查是否真的是结构体字面量语法
                Token next = peekAhead(1);
                bool isStructLiteral = false;

                if (next.is(TokenKind::DotDot)) {
                    // 不将 { .. } 解析为结构体字面量
                    // 因为这会与 match 模式冲突
                    // 结构体更新语法应该明确写成 { field: value, ..base }
                    isStructLiteral = false;
                } else if (next.is(TokenKind::Identifier)) {
                    // 检查标识符后面是否有冒号
                    Token afterIdent = peekAhead(2);
                    if (afterIdent.is(TokenKind::Colon)) {
                        // { field: value } 语法
                        isStructLiteral = true;
                    }
                } else if (next.is(TokenKind::RBrace)) {
                    // 空结构体 { }
                    isStructLiteral = true;
                }

                if (isStructLiteral) {
                    return parseStructExpr(name);
                }
            }
            
            SourceRange range(tok.getLocation(), tok.getRange().getEnd());
            return ParseResult<Expr>(
                Ctx.create<IdentifierExpr>(range, name));
        }

        // 在表达式位置允许类型关键字退化为标识符，
        // 以便将类似 `sizeof(i32)` 交给语义层产生“未声明标识符”诊断。
        case TokenKind::KW_void:
        case TokenKind::KW_bool:
        case TokenKind::KW_char:
        case TokenKind::KW_str:
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
        case TokenKind::KW_f64: {
            Token tok = consume();
            SourceRange range(tok.getLocation(), tok.getRange().getEnd());
            return ParseResult<Expr>(
                Ctx.create<IdentifierExpr>(range, std::string(tok.getText())));
        }
        
        // 内置函数调用 @name(...)
        case TokenKind::BuiltinIdentifier:
            return parseBuiltinCallExpr();
        
        // 括号表达式或元组
        case TokenKind::LParen:
            return parseTupleExpr();
        
        // 数组表达式
        case TokenKind::LBracket:
            return parseArrayExpr();
        
        // if 表达式
        case TokenKind::KW_if:
            return parseIfExpr();

        // match 表达式
        case TokenKind::KW_match:
            return parseMatchExpr();

        // await 表达式
        case TokenKind::KW_await: {
            Token awaitTok = consume();
            auto awaited = parseUnaryExpr();
            if (awaited.isError()) {
                return ParseResult<Expr>::error();
            }
            Expr* awaitedExpr = awaited.get();

            // `await expr!` / `await expr! -> err { ... }`
            // 应绑定为 `(await expr)!` / `(await expr) -> err { ... }`，
            // 以便 await 后的错误统一走 `!`/`err` 处理链路。
            if (auto* errHandle = dynamic_cast<ErrorHandleExpr*>(awaitedExpr)) {
                Expr* handleInner = errHandle->getInner();
                SourceRange awaitRange(awaitTok.getLocation(), handleInner->getEndLoc());
                auto* awaitExpr = Ctx.create<AwaitExpr>(awaitRange, handleInner);
                SourceRange handleRange(awaitTok.getLocation(), errHandle->getEndLoc());
                return ParseResult<Expr>(
                    Ctx.create<ErrorHandleExpr>(
                        handleRange,
                        awaitExpr,
                        errHandle->getErrorVar(),
                        errHandle->getHandler()));
            }

            if (auto* errProp = dynamic_cast<ErrorPropagateExpr*>(awaitedExpr)) {
                Expr* propInner = errProp->getInner();
                SourceRange awaitRange(awaitTok.getLocation(), propInner->getEndLoc());
                auto* awaitExpr = Ctx.create<AwaitExpr>(awaitRange, propInner);
                SourceRange propRange(awaitTok.getLocation(), errProp->getEndLoc());
                return ParseResult<Expr>(
                    Ctx.create<ErrorPropagateExpr>(propRange, awaitExpr));
            }

            SourceRange range(awaitTok.getLocation(), awaitedExpr->getEndLoc());
            return ParseResult<Expr>(
                Ctx.create<AwaitExpr>(range, awaitedExpr));
        }

        // loop 表达式
        case TokenKind::KW_loop:
            return parseLoopExpr();

        // 闭包表达式
        case TokenKind::Pipe:
            return parseClosureExpr();

        // 函数式闭包表达式 func(params) -> ReturnType { ... }
        case TokenKind::KW_func:
            return parseClosureExpr();
        
        // 范围表达式 ..end 或 ..=end 或 ..
        case TokenKind::DotDot:
        case TokenKind::DotDotEqual: {
            bool isInclusive = check(TokenKind::DotDotEqual);
            Token rangeOp = consume();
            
            // 检查是否有结束表达式
            Expr* end = nullptr;

            // 如果下一个 token 可以是表达式的开始，则解析结束表达式
            // 否则这是一个完整范围 ..
            // 不解析的情况：分隔符、语句关键字、文件结束
            if (!check(TokenKind::Comma) && !check(TokenKind::RBracket) &&
                !check(TokenKind::RBrace) && !check(TokenKind::RParen) &&
                !check(TokenKind::Semicolon) && !check(TokenKind::FatArrow) &&
                !check(TokenKind::Pipe) && !isAtEnd() &&
                !check(TokenKind::KW_var) && !check(TokenKind::KW_const) &&
                !check(TokenKind::KW_return) && !check(TokenKind::KW_if) &&
                !check(TokenKind::KW_while) && !check(TokenKind::KW_for) &&
                !check(TokenKind::KW_loop) && !check(TokenKind::KW_match) &&
                !check(TokenKind::KW_break) && !check(TokenKind::KW_continue) &&
                !check(TokenKind::KW_defer)) {
                auto endResult = parseExprWithPrecedence(0);
                if (endResult.isSuccess()) {
                    end = endResult.get();
                }
            }
            
            SourceLocation endLoc = end ? end->getEndLoc() : rangeOp.getRange().getEnd();
            SourceRange range(startLoc, endLoc);
            return ParseResult<Expr>(
                Ctx.create<RangeExpr>(range, nullptr, end, isInclusive));
        }

        // 块表达式 { ... }
        case TokenKind::LBrace:
            return parseBlockExpr();

        default:
            reportError(DiagID::err_expected_expression);
            return ParseResult<Expr>::error();
    }

}

ParseResult<Expr> Parser::parsePostfixExpr(Expr* base) {
    while (true) {
        switch (CurTok.getKind()) {
            // 泛型函数调用 expr<Args>(...)
            case TokenKind::Less: {
                // 仅允许标识符或成员访问后接泛型参数
                if (base->getKind() != ASTNode::Kind::IdentifierExpr &&
                    base->getKind() != ASTNode::Kind::MemberExpr) {
                    return ParseResult<Expr>(base);
                }

                auto scanGenericTail = [&](TokenKind& nextKind) -> bool {
                    int depth = 0;
                    int i = 0;
                    while (true) {
                        Token tok = (i == 0) ? CurTok : peekAhead(i);
                        if (tok.is(TokenKind::Less)) {
                            depth++;
                        } else if (tok.is(TokenKind::Greater)) {
                            depth--;
                            if (depth == 0) {
                                nextKind = peekAhead(i + 1).getKind();
                                return true;
                            }
                        } else if (tok.is(TokenKind::GreaterGreater)) {
                            depth -= 2;
                            if (depth == 0) {
                                nextKind = peekAhead(i + 1).getKind();
                                return true;
                            }
                            if (depth < 0) {
                                return false;
                            }
                        } else if (tok.is(TokenKind::EndOfFile)) {
                            return false;
                        }
                        i++;
                    }
                };

                TokenKind genericTail = TokenKind::Invalid;
                if (!scanGenericTail(genericTail)) {
                    return ParseResult<Expr>(base);
                }

                if (genericTail == TokenKind::LParen) {
                    std::vector<TypeNode*> typeArgs = parseGenericArgs();
                    if (typeArgs.empty() && !check(TokenKind::LParen)) {
                        return ParseResult<Expr>::error();
                    }

                    auto callResult = parseCallExpr(base, std::move(typeArgs));
                    if (callResult.isError()) {
                        return ParseResult<Expr>::error();
                    }
                    base = callResult.get();
                    break;
                }

                if (genericTail == TokenKind::LBrace) {
                    std::vector<TypeNode*> typeArgs = parseGenericArgs();
                    if (!check(TokenKind::LBrace)) {
                        return ParseResult<Expr>::error();
                    }

                    std::string typeName;
                    if (base->getKind() == ASTNode::Kind::IdentifierExpr) {
                        auto* identExpr = static_cast<IdentifierExpr*>(base);
                        typeName = identExpr->getName();
                    } else {
                        auto* memberExpr = static_cast<MemberExpr*>(base);
                        if (memberExpr->getBase()->getKind() == ASTNode::Kind::IdentifierExpr) {
                            auto* baseIdent = static_cast<IdentifierExpr*>(memberExpr->getBase());
                            typeName = baseIdent->getName() + "." + memberExpr->getMember();
                        } else {
                            return ParseResult<Expr>(base);
                        }
                    }

                    auto structResult = parseStructExprBody(base->getBeginLoc(), typeName, std::move(typeArgs));
                    if (structResult.isError()) {
                        return ParseResult<Expr>::error();
                    }
                    base = structResult.get();
                    break;
                }

                return ParseResult<Expr>(base);
            }

            // 函数调用 expr(args)
            case TokenKind::LParen: {
                auto callResult = parseCallExpr(base);
                if (callResult.isError()) {
                    return ParseResult<Expr>::error();
                }
                base = callResult.get();
                break;
            }
            
            // 索引或切片 expr[index] 或 expr[start..end]
            case TokenKind::LBracket: {
                auto indexResult = parseIndexExpr(base);
                if (indexResult.isError()) {
                    return ParseResult<Expr>::error();
                }
                base = indexResult.get();
                break;
            }
            
            // 成员访问 expr.member 或 expr::member
            case TokenKind::Dot:
            case TokenKind::ColonColon: {
                auto memberResult = parseMemberExpr(base);
                if (memberResult.isError()) {
                    return ParseResult<Expr>::error();
                }
                base = memberResult.get();
                break;
            }
            
            // 可选链 expr?.member
            case TokenKind::QuestionDot: {
                auto optionalResult = parseOptionalChainingExpr(base);
                if (optionalResult.isError()) {
                    return ParseResult<Expr>::error();
                }
                base = optionalResult.get();
                break;
            }
            
            // 错误传播 expr!
            case TokenKind::Exclaim: {
                // 宏样式调用 vec![...] （简化处理为数组表达式）
                if (base->getKind() == ASTNode::Kind::IdentifierExpr) {
                    auto* ident = static_cast<IdentifierExpr*>(base);
                    if (ident->getName() == "vec" && peekAhead(1).is(TokenKind::LBracket)) {
                        consume(); // 消费 '!'
                        auto arrayResult = parseArrayExpr();
                        if (arrayResult.isError()) {
                            return ParseResult<Expr>::error();
                        }
                        base = arrayResult.get();
                        break;
                    }
                }

                // 后缀 !：错误传播；若后接 -> err { ... } 则构造错误处理表达式
                Expr* innerExpr = base;
                Token exclaimTok = consume();

                if (match(TokenKind::Arrow)) {
                    if (!check(TokenKind::Identifier)) {
                        std::string found = CurTok.getText();
                        if (found.empty()) {
                            found = "?";
                        }
                        Diag.report(DiagID::err_expected_token,
                                    CurTok.getLocation(),
                                    CurTok.getRange())
                            << "err"
                            << found;
                        return ParseResult<Expr>::error();
                    }
                    std::string errorVar = CurTok.getText();
                    consume();

                    auto handlerResult = parseBlockStmt();
                    if (handlerResult.isError()) {
                        return ParseResult<Expr>::error();
                    }
                    auto* handler = dynamic_cast<BlockStmt*>(handlerResult.get());
                    if (!handler) {
                        reportError(DiagID::err_expected_lbrace);
                        return ParseResult<Expr>::error();
                    }

                    SourceRange range(innerExpr->getBeginLoc(), handler->getEndLoc());
                    base = Ctx.create<ErrorHandleExpr>(range, innerExpr, errorVar, handler);
                    break;
                }

                SourceRange range(innerExpr->getBeginLoc(), exclaimTok.getRange().getEnd());
                base = Ctx.create<ErrorPropagateExpr>(range, innerExpr);
                break;
            }
            
            // 类型转换 expr as Type
            case TokenKind::KW_as: {
                consume();
                auto typeResult = parseType();
                if (typeResult.isError()) {
                    return ParseResult<Expr>::error();
                }
                SourceRange range(base->getBeginLoc(), typeResult->getEndLoc());
                base = Ctx.create<CastExpr>(range, base, typeResult.get());
                break;
            }
            
            // 结构体字面量 expr { ... }
            case TokenKind::LBrace: {
                if (!AllowStructLiteral) {
                    return ParseResult<Expr>(base);
                }
                // 只有当 base 是标识符或成员访问表达式时才解析为结构体字面量
                if (base->getKind() == ASTNode::Kind::IdentifierExpr ||
                    base->getKind() == ASTNode::Kind::MemberExpr) {
                    
                    // 向前看，检查是否真的是结构体字面量语法
                    // 结构体字面量应该是 { field: value, ... } 或 { ..base }
                    Token next = peekAhead(1);
                    bool isStructLiteral = false;

                    if (next.is(TokenKind::DotDot)) {
                        // 不将 { .. } 解析为结构体字面量
                        // 因为这会与 match 模式冲突
                        // 结构体更新语法应该明确写成 { field: value, ..base }
                        isStructLiteral = false;
                    } else if (next.is(TokenKind::Identifier)) {
                        // 检查标识符后面是否有冒号
                        Token afterIdent = peekAhead(2);
                        if (afterIdent.is(TokenKind::Colon)) {
                            // { field: value } 语法
                            isStructLiteral = true;
                        }
                    } else if (next.is(TokenKind::RBrace)) {
                        // 空结构体 { }
                        isStructLiteral = true;
                    }
                    
                    if (isStructLiteral) {
                        // 获取类型名称
                        std::string typeName;
                        if (base->getKind() == ASTNode::Kind::IdentifierExpr) {
                            auto* identExpr = static_cast<IdentifierExpr*>(base);
                            typeName = identExpr->getName();
                        } else {
                            // 对于成员访问表达式，我们需要构造完整的类型名
                            // 这里简化处理，实际应该递归构造
                            auto* memberExpr = static_cast<MemberExpr*>(base);
                            if (memberExpr->getBase()->getKind() == ASTNode::Kind::IdentifierExpr) {
                                auto* baseIdent = static_cast<IdentifierExpr*>(memberExpr->getBase());
                                typeName = baseIdent->getName() + "." + memberExpr->getMember();
                            } else {
                                // 复杂的成员访问，暂时不支持
                                return ParseResult<Expr>(base);
                            }
                        }
                        
                        auto structResult = parseStructExprBody(base->getBeginLoc(), typeName);
                        if (structResult.isError()) {
                            return ParseResult<Expr>::error();
                        }
                        base = structResult.get();
                        break;
                    } else {
                        // 不是结构体字面量，停止后缀解析
                        return ParseResult<Expr>(base);
                    }
                } else {
                    return ParseResult<Expr>(base);
                }
            }
            
            default:
                return ParseResult<Expr>(base);
        }
    }
}

ParseResult<Expr> Parser::parseCallExpr(Expr* callee, std::vector<TypeNode*> typeArgs) {
    SourceLocation startLoc = callee->getBeginLoc();
    
    // 消费 '('
    if (!expectAndConsume(TokenKind::LParen)) {
        return ParseResult<Expr>::error();
    }
    
    // 解析参数列表
    std::vector<CallExpr::Arg> args;
    if (!check(TokenKind::RParen)) {
        do {
            bool isSpread = false;
            if (match(TokenKind::Ellipsis)) {
                isSpread = true;
            }
            auto argResult = parseExpr();
            if (argResult.isError()) {
                return ParseResult<Expr>::error();
            }
            args.emplace_back(argResult.get(), isSpread);
        } while (match(TokenKind::Comma));
    }
    
    // 消费 ')'
    if (!expect(TokenKind::RParen)) {
        return ParseResult<Expr>::error();
    }
    Token endTok = consume();
    
    SourceRange range(startLoc, endTok.getRange().getEnd());
    return ParseResult<Expr>(
        Ctx.create<CallExpr>(range, callee, std::move(args), std::move(typeArgs)));
}

ParseResult<Expr> Parser::parseIndexExpr(Expr* base) {
    SourceLocation startLoc = base->getBeginLoc();
    
    // 消费 '['
    if (!expectAndConsume(TokenKind::LBracket)) {
        return ParseResult<Expr>::error();
    }
    
    // 检查是否为空切片 [..]
    if (check(TokenKind::DotDot) || check(TokenKind::DotDotEqual)) {
        // 空起始的切片 [..end] 或 [..=end]
        bool isInclusive = match(TokenKind::DotDotEqual);
        if (!isInclusive) {
            consume(); // 消费 '..'
        }
        
        Expr* end = nullptr;
        if (!check(TokenKind::RBracket)) {
            auto endResult = parseExpr();
            if (endResult.isError()) {
                return ParseResult<Expr>::error();
            }
            end = endResult.get();
        }
        
        if (!expect(TokenKind::RBracket)) {
            return ParseResult<Expr>::error();
        }
        Token endTok = consume();
        
        SourceRange range(startLoc, endTok.getRange().getEnd());
        return ParseResult<Expr>(
            Ctx.create<SliceExpr>(range, base, nullptr, end, isInclusive));
    }
    
    // 解析第一个表达式，但需要特殊处理以避免将 "2.." 解析为范围表达式
    Expr* firstExpr = nullptr;
    
    // 如果第一个 token 是数字或标识符，并且后面跟着 .. 或 ..=，
    // 那么我们只解析第一个 token，不解析范围
    if ((check(TokenKind::IntegerLiteral) || check(TokenKind::Identifier)) &&
        (peekAhead(1).is(TokenKind::DotDot) || peekAhead(1).is(TokenKind::DotDotEqual))) {
        
        // 只解析基本表达式，不包括后缀
        auto primaryResult = parsePrimaryExpr();
        if (primaryResult.isError()) {
            return ParseResult<Expr>::error();
        }
        firstExpr = primaryResult.get();
    } else {
        // 正常解析表达式
        auto firstResult = parseExpr();
        if (firstResult.isError()) {
            return ParseResult<Expr>::error();
        }
        firstExpr = firstResult.get();
    }
    
    // 检查是否为切片语法
    if (check(TokenKind::DotDot) || check(TokenKind::DotDotEqual)) {
        // 切片语法 [start..end] 或 [start..=end]
        bool isInclusive = match(TokenKind::DotDotEqual);
        if (!isInclusive) {
            consume(); // 消费 '..'
        }
        
        Expr* end = nullptr;
        if (!check(TokenKind::RBracket)) {
            auto endResult = parseExpr();
            if (endResult.isError()) {
                return ParseResult<Expr>::error();
            }
            end = endResult.get();
        }
        
        if (!expect(TokenKind::RBracket)) {
            return ParseResult<Expr>::error();
        }
        Token endTok = consume();
        
        SourceRange range(startLoc, endTok.getRange().getEnd());
        return ParseResult<Expr>(
            Ctx.create<SliceExpr>(range, base, firstExpr, end, isInclusive));
    }
    
    // 普通索引语法 [index]
    if (!expect(TokenKind::RBracket)) {
        return ParseResult<Expr>::error();
    }
    Token endTok = consume();
    
    SourceRange range(startLoc, endTok.getRange().getEnd());
    return ParseResult<Expr>(
        Ctx.create<IndexExpr>(range, base, firstExpr));
}

ParseResult<Expr> Parser::parseMemberExpr(Expr* base) {
    SourceLocation startLoc = base->getBeginLoc();
    
    // 消费 '.' 或 '::'
    if (check(TokenKind::Dot) || check(TokenKind::ColonColon)) {
        consume();
    } else {
        reportError(DiagID::err_expected_identifier);
        return ParseResult<Expr>::error();
    }
    
    // 期望标识符或整数字面量（用于元组成员访问）
    std::string memberName;
    Token memberTok;
    
    if (check(TokenKind::Identifier) ||
        check(TokenKind::KW_internal) ||
        check(TokenKind::KW_type) ||
        check(TokenKind::KW_None) ||
        check(TokenKind::KW_ptr)) {
        memberTok = consume();
        memberName = memberTok.getText();
    } else if (check(TokenKind::IntegerLiteral)) {
        // 支持元组成员访问 tuple.0, tuple.1 等
        memberTok = consume();
        memberName = memberTok.getText();
    } else {
        reportError(DiagID::err_expected_identifier);
        return ParseResult<Expr>::error();
    }
    
    SourceRange range(startLoc, memberTok.getRange().getEnd());
    return ParseResult<Expr>(
        Ctx.create<MemberExpr>(range, base, memberName));
}

ParseResult<Expr> Parser::parseOptionalChainingExpr(Expr* base) {
    SourceLocation startLoc = base->getBeginLoc();
    
    // 消费 '?.'
    if (!expectAndConsume(TokenKind::QuestionDot)) {
        return ParseResult<Expr>::error();
    }
    
    // 期望标识符或整数字面量（用于元组成员访问）
    std::string memberName;
    Token memberTok;
    
    if (check(TokenKind::Identifier) || check(TokenKind::KW_ptr)) {
        memberTok = consume();
        memberName = memberTok.getText();
    } else if (check(TokenKind::IntegerLiteral)) {
        // 支持元组成员访问 tuple?.0, tuple?.1 等
        memberTok = consume();
        memberName = memberTok.getText();
    } else {
        reportError(DiagID::err_expected_identifier);
        return ParseResult<Expr>::error();
    }
    
    SourceRange range(startLoc, memberTok.getRange().getEnd());
    return ParseResult<Expr>(
        Ctx.create<OptionalChainingExpr>(range, base, memberName));
}

// ============================================================================
// 复合表达式解析（占位实现）
// ============================================================================

ParseResult<Expr> Parser::parseIfExpr() {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 'if'
    if (!expectAndConsume(TokenKind::KW_if)) {
        return ParseResult<Expr>::error();
    }
    
    std::vector<IfExpr::Branch> branches;
    
    // 解析第一个分支（if 分支）
    auto conditionResult = parseExpr();
    if (conditionResult.isError()) {
        return ParseResult<Expr>::error();
    }
    
    // 解析 if 分支体
    auto thenResult = parseIfBranchExpr();
    if (thenResult.isError()) {
        return ParseResult<Expr>::error();
    }
    
    branches.push_back({conditionResult.get(), thenResult.get()});
    
    // 解析 elif 分支
    while (match(TokenKind::KW_elif)) {
        auto elifCondResult = parseExpr();
        if (elifCondResult.isError()) {
            return ParseResult<Expr>::error();
        }
        
        auto elifBodyResult = parseIfBranchExpr();
        if (elifBodyResult.isError()) {
            return ParseResult<Expr>::error();
        }
        
        branches.push_back({elifCondResult.get(), elifBodyResult.get()});
    }
    
    // 解析 else 分支（可选）
    SourceLocation endLoc = PrevTok.getRange().getEnd();
    if (match(TokenKind::KW_else)) {
        auto elseResult = parseIfBranchExpr();
        if (elseResult.isError()) {
            return ParseResult<Expr>::error();
        }
        
        endLoc = elseResult.get()->getEndLoc();
        branches.push_back({nullptr, elseResult.get()});  // else 分支条件为 nullptr
    }
    
    SourceRange range(startLoc, endLoc);
    return ParseResult<Expr>(
        Ctx.create<IfExpr>(range, std::move(branches)));
}

ParseResult<Expr> Parser::parseIfBranchExpr() {
    // 检查是否为块表达式 { stmts; expr }
    if (check(TokenKind::LBrace)) {
        return parseBlockExpr();
    } else {
        // 普通表达式，但需要避免与后续的 { 产生冲突
        // 使用较高的最小优先级来避免解析后续的 {
        return parseExprWithPrecedence(0);
    }
}

ParseResult<Expr> Parser::parseMatchExpr() {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 'match'
    if (!expectAndConsume(TokenKind::KW_match)) {
        return ParseResult<Expr>::error();
    }
    
    // 解析被匹配的表达式
    auto scrutineeResult = parseExpr();
    if (scrutineeResult.isError()) {
        return ParseResult<Expr>::error();
    }
    
    // 期望 '{'
    if (!expectAndConsume(TokenKind::LBrace)) {
        return ParseResult<Expr>::error();
    }
    
    std::vector<MatchExpr::Arm> arms;
    
    // 解析匹配分支
    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        // 解析模式
        auto patternResult = parsePattern();
        if (patternResult.isError()) {
            return ParseResult<Expr>::error();
        }
        
        // 解析守卫条件（可选）
        Expr* guard = nullptr;
        if (match(TokenKind::KW_if)) {
            auto guardResult = parseExpr();
            if (guardResult.isError()) {
                return ParseResult<Expr>::error();
            }
            guard = guardResult.get();
        }
        
        // 期望 '=>'
        if (!expectAndConsume(TokenKind::FatArrow)) {
            return ParseResult<Expr>::error();
        }
        
        // 解析分支体
        auto bodyResult = parseExpr();
        if (bodyResult.isError()) {
            return ParseResult<Expr>::error();
        }
        
        arms.push_back({patternResult.get(), guard, bodyResult.get()});
        
        // 可选的逗号
        if (check(TokenKind::Comma)) {
            consume();
        } else if (!check(TokenKind::RBrace)) {
            Diag.report(DiagID::err_expected_comma_or_close, CurTok.getLocation(), CurTok.getRange())
                << "}";
            return ParseResult<Expr>::error();
        }
    }
    
    // 期望 '}'
    if (!expectAndConsume(TokenKind::RBrace)) {
        return ParseResult<Expr>::error();
    }
    
    SourceRange range(startLoc, PrevTok.getRange().getEnd());
    return ParseResult<Expr>(
        Ctx.create<MatchExpr>(range, scrutineeResult.get(), std::move(arms)));
}

ParseResult<Expr> Parser::parseClosureExpr() {
    SourceLocation startLoc = CurTok.getLocation();

    std::vector<ParamDecl*> params;
    std::vector<GenericParam> genericParams;
    TypeNode* returnType = nullptr;

    if (check(TokenKind::KW_func)) {
        // func<GenericParams>(params) -> ReturnType { ... } 语法
        consume(); // 消费 'func'

        // 解析可选的泛型参数
        if (check(TokenKind::Less)) {
            genericParams = parseGenericParams();
        }

        // 期望 '('
        if (!expectAndConsume(TokenKind::LParen)) {
            return ParseResult<Expr>::error();
        }

        // 解析参数列表（允许省略类型）
        if (!check(TokenKind::RParen)) {
            do {
                // 解析闭包参数（类型注解可选）
                SourceLocation paramStart = CurTok.getLocation();

                // 检查是否有 & 引用前缀
                bool isReference = match(TokenKind::Amp);

                // 检查是否有 'mut' 修饰符
                bool isMutable = match(TokenKind::KW_mut);

                // 解析参数名
                if (!check(TokenKind::Identifier)) {
                    reportError(DiagID::err_expected_identifier);
                    return ParseResult<Expr>::error();
                }

                std::string paramName = std::string(CurTok.getText());
                consume();

                // 解析可选的类型注解
                TypeNode* paramType = nullptr;
                if (match(TokenKind::Colon)) {
                    auto typeResult = parseType();
                    if (typeResult.isError()) {
                        return ParseResult<Expr>::error();
                    }
                    paramType = typeResult.get();
                }
                // 如果有 & 但没有类型注解，允许类型推断（paramType 为 nullptr）

                SourceLocation paramEnd = PrevTok.getRange().getEnd();
                SourceRange paramRange(paramStart, paramEnd);

                params.push_back(
                    Ctx.create<ParamDecl>(paramRange, paramName, paramType, nullptr, isMutable));

            } while (match(TokenKind::Comma));
        }

        // 期望 ')'
        if (!expectAndConsume(TokenKind::RParen)) {
            return ParseResult<Expr>::error();
        }

        // 解析返回类型
        if (match(TokenKind::Arrow)) {
            auto typeResult = parseType();
            if (typeResult.isError()) {
                return ParseResult<Expr>::error();
            }
            returnType = typeResult.get();
        }

    } else if (check(TokenKind::Pipe)) {
        // |params| -> ReturnType expr 语法
        consume(); // 消费 '|'

        // 解析参数列表（允许省略类型）
        if (!check(TokenKind::Pipe)) {
            do {
                // 解析闭包参数（类型注解可选）
                SourceLocation paramStart = CurTok.getLocation();

                // 检查是否有 & 引用前缀
                bool isReference = match(TokenKind::Amp);

                // 检查是否有 'mut' 修饰符
                bool isMutable = match(TokenKind::KW_mut);

                // 解析参数名
                if (!check(TokenKind::Identifier)) {
                    reportError(DiagID::err_expected_identifier);
                    return ParseResult<Expr>::error();
                }

                std::string paramName = std::string(CurTok.getText());
                consume();

                // 解析可选的类型注解
                TypeNode* paramType = nullptr;
                if (match(TokenKind::Colon)) {
                    auto typeResult = parseType();
                    if (typeResult.isError()) {
                        return ParseResult<Expr>::error();
                    }
                    paramType = typeResult.get();
                }
                // 如果有 & 但没有类型注解，允许类型推断（paramType 为 nullptr）

                SourceLocation paramEnd = PrevTok.getRange().getEnd();
                SourceRange paramRange(paramStart, paramEnd);

                params.push_back(
                    Ctx.create<ParamDecl>(paramRange, paramName, paramType, nullptr, isMutable));

            } while (match(TokenKind::Comma));
        }

        // 消费 '|'
        if (!expectAndConsume(TokenKind::Pipe)) {
            return ParseResult<Expr>::error();
        }

        // 解析返回类型（可选）
        if (match(TokenKind::Arrow)) {
            auto typeResult = parseType();
            if (typeResult.isError()) {
                return ParseResult<Expr>::error();
            }
            returnType = typeResult.get();
        }
    } else {
        reportError(DiagID::err_expected_pipe_or_func);
        return ParseResult<Expr>::error();
    }

    // 解析闭包体
    Expr* body = nullptr;
    if (check(TokenKind::LBrace)) {
        // 块表达式作为闭包体
        auto blockResult = parseBlockExpr();
        if (blockResult.isError()) {
            return ParseResult<Expr>::error();
        }
        body = blockResult.get();
    } else {
        // 单个表达式作为闭包体
        auto exprResult = parseExpr();
        if (exprResult.isError()) {
            return ParseResult<Expr>::error();
        }
        body = exprResult.get();
    }

    SourceRange range(startLoc, body->getEndLoc());
    auto* closureExpr = Ctx.create<ClosureExpr>(range, std::move(params), returnType, body,
                                                std::move(genericParams));

    return ParseResult<Expr>(closureExpr);
}


ParseResult<Expr> Parser::parseArrayExpr() {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 '['
    if (!expectAndConsume(TokenKind::LBracket)) {
        return ParseResult<Expr>::error();
    }
    
    // 空数组 []
    if (match(TokenKind::RBracket)) {
        SourceRange range(startLoc, PrevTok.getRange().getEnd());
        return ParseResult<Expr>(
            Ctx.create<ArrayExpr>(range, std::vector<Expr*>()));
    }
    
    // 解析第一个元素
    auto firstResult = parseExpr();
    if (firstResult.isError()) {
        return ParseResult<Expr>::error();
    }
    
    // 检查是否为重复初始化语法 [element; count]
    if (match(TokenKind::Semicolon)) {
        auto countResult = parseExpr();
        if (countResult.isError()) {
            return ParseResult<Expr>::error();
        }
        
        if (!expectAndConsume(TokenKind::RBracket)) {
            return ParseResult<Expr>::error();
        }
        
        SourceRange range(startLoc, PrevTok.getRange().getEnd());
        return ParseResult<Expr>(
            ArrayExpr::createRepeat(range, firstResult.get(), countResult.get()));
    }
    
    // 普通数组语法 [elem1, elem2, ...]
    std::vector<Expr*> elements;
    elements.push_back(firstResult.get());
    
    while (match(TokenKind::Comma)) {
        // 允许尾随逗号
        if (check(TokenKind::RBracket)) {
            break;
        }
        
        auto elemResult = parseExpr();
        if (elemResult.isError()) {
            return ParseResult<Expr>::error();
        }
        elements.push_back(elemResult.get());
    }
    
    if (!expectAndConsume(TokenKind::RBracket)) {
        return ParseResult<Expr>::error();
    }
    
    SourceRange range(startLoc, PrevTok.getRange().getEnd());
    return ParseResult<Expr>(
        Ctx.create<ArrayExpr>(range, std::move(elements)));
}

ParseResult<Expr> Parser::parseTupleExpr() {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 '('
    if (!expectAndConsume(TokenKind::LParen)) {
        return ParseResult<Expr>::error();
    }
    
    // 空元组 ()
    if (match(TokenKind::RParen)) {
        SourceRange range(startLoc, PrevTok.getRange().getEnd());
        return ParseResult<Expr>(
            Ctx.create<TupleExpr>(range, std::vector<Expr*>()));
    }
    
    // 解析第一个元素
    auto firstResult = parseExpr();
    if (firstResult.isError()) {
        return ParseResult<Expr>::error();
    }
    
    // 检查是否为单元素括号表达式 (expr)
    if (match(TokenKind::RParen)) {
        // 这是括号表达式，不是元组
        return ParseResult<Expr>(firstResult.get());
    }
    
    // 必须有逗号才是元组
    if (!expectAndConsume(TokenKind::Comma)) {
        return ParseResult<Expr>::error();
    }

    // "(x,)" 在规范中非法：逗号后必须还有一个元素
    if (check(TokenKind::RParen)) {
        reportError(DiagID::err_expected_expression);
        return ParseResult<Expr>::error();
    }
    
    std::vector<Expr*> elements;
    elements.push_back(firstResult.get());
    
    // 解析剩余元素
    if (!check(TokenKind::RParen)) {
        do {
            // 允许尾随逗号
            if (check(TokenKind::RParen)) {
                break;
            }
            
            auto elemResult = parseExpr();
            if (elemResult.isError()) {
                return ParseResult<Expr>::error();
            }
            elements.push_back(elemResult.get());
        } while (match(TokenKind::Comma));
    }
    
    if (!expectAndConsume(TokenKind::RParen)) {
        return ParseResult<Expr>::error();
    }
    
    SourceRange range(startLoc, PrevTok.getRange().getEnd());
    return ParseResult<Expr>(
        Ctx.create<TupleExpr>(range, std::move(elements)));
}

ParseResult<Expr> Parser::parseStructExpr(const std::string& typeName,
                                          std::vector<TypeNode*> typeArgs) {
    SourceLocation startLoc = PrevTok.getLocation(); // 标识符的位置
    
    // 消费 '{'
    if (!expectAndConsume(TokenKind::LBrace)) {
        return ParseResult<Expr>::error();
    }
    
    std::vector<StructExpr::FieldInit> fields;
    Expr* base = nullptr;
    
    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        // 检查是否为基础表达式语法 ..base
        if (match(TokenKind::DotDot)) {
            auto baseResult = parseExpr();
            if (baseResult.isError()) {
                return ParseResult<Expr>::error();
            }
            base = baseResult.get();
            
            // 基础表达式后面不能再有字段
            if (match(TokenKind::Comma)) {
                if (!check(TokenKind::RBrace)) {
                    reportUnexpectedError();
                    return ParseResult<Expr>::error();
                }
            }
            break;
        }
        
        // 解析字段名（允许某些关键字作为字段名）
        if (!check(TokenKind::Identifier) &&
            !check(TokenKind::KW_internal) &&
            !check(TokenKind::KW_type)) {
            reportError(DiagID::err_expected_identifier);
            return ParseResult<Expr>::error();
        }

        Token fieldTok = consume();
        std::string fieldName = fieldTok.getText();
        
        // 期望 ':'
        if (!expectAndConsume(TokenKind::Colon)) {
            return ParseResult<Expr>::error();
        }
        
        // 解析字段值
        auto valueResult = parseExpr();
        if (valueResult.isError()) {
            return ParseResult<Expr>::error();
        }
        
        fields.push_back({fieldName, valueResult.get(), fieldTok.getLocation()});
        
        // 可选的逗号
        if (check(TokenKind::Comma)) {
            consume();
        } else if (!check(TokenKind::RBrace)) {
            Diag.report(DiagID::err_expected_comma_or_close, CurTok.getLocation(), CurTok.getRange())
                << "}";
            return ParseResult<Expr>::error();
        }
    }
    
    if (!expectAndConsume(TokenKind::RBrace)) {
        return ParseResult<Expr>::error();
    }
    
    SourceRange range(startLoc, PrevTok.getRange().getEnd());
    return ParseResult<Expr>(
        Ctx.create<StructExpr>(range, typeName, std::move(fields), std::move(typeArgs), base));
}

ParseResult<Expr> Parser::parseStructExprBody(SourceLocation startLoc,
                                              const std::string& typeName,
                                              std::vector<TypeNode*> typeArgs) {
    // 消费 '{'
    if (!expectAndConsume(TokenKind::LBrace)) {
        return ParseResult<Expr>::error();
    }
    
    std::vector<StructExpr::FieldInit> fields;
    Expr* base = nullptr;
    
    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        // 检查是否为基础表达式语法 ..base
        if (match(TokenKind::DotDot)) {
            auto baseResult = parseExpr();
            if (baseResult.isError()) {
                return ParseResult<Expr>::error();
            }
            base = baseResult.get();
            
            // 基础表达式后面不能再有字段
            if (match(TokenKind::Comma)) {
                if (!check(TokenKind::RBrace)) {
                    reportUnexpectedError();
                    return ParseResult<Expr>::error();
                }
            }
            break;
        }
        
        // 解析字段名（允许某些关键字作为字段名）
        if (!check(TokenKind::Identifier) &&
            !check(TokenKind::KW_internal) &&
            !check(TokenKind::KW_type)) {
            reportError(DiagID::err_expected_identifier);
            return ParseResult<Expr>::error();
        }

        Token fieldTok = consume();
        std::string fieldName = fieldTok.getText();
        
        // 期望 ':'
        if (!expectAndConsume(TokenKind::Colon)) {
            return ParseResult<Expr>::error();
        }
        
        // 解析字段值
        auto valueResult = parseExpr();
        if (valueResult.isError()) {
            return ParseResult<Expr>::error();
        }
        
        fields.push_back({fieldName, valueResult.get(), fieldTok.getLocation()});
        
        // 可选的逗号
        if (check(TokenKind::Comma)) {
            consume();
        } else if (!check(TokenKind::RBrace)) {
            Diag.report(DiagID::err_expected_comma_or_close, CurTok.getLocation(), CurTok.getRange())
                << "}";
            return ParseResult<Expr>::error();
        }
    }
    
    if (!expectAndConsume(TokenKind::RBrace)) {
        return ParseResult<Expr>::error();
    }
    
    SourceRange range(startLoc, PrevTok.getRange().getEnd());
    return ParseResult<Expr>(
        Ctx.create<StructExpr>(range, typeName, std::move(fields), std::move(typeArgs), base));
}

ParseResult<Expr> Parser::parseBuiltinCallExpr() {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费内置函数标识符 @name
    if (!check(TokenKind::BuiltinIdentifier)) {
        reportError(DiagID::err_expected_builtin_identifier);
        return ParseResult<Expr>::error();
    }
    
    Token builtinTok = consume();
    std::string builtinName = builtinTok.getText();
    
    // 移除 @ 前缀
    if (builtinName.empty() || builtinName[0] != '@') {
        reportError(DiagID::err_invalid_builtin_name, builtinTok.getLocation());
        return ParseResult<Expr>::error();
    }
    builtinName = builtinName.substr(1);
    
    // 解析内置函数类型
    auto builtinKind = BuiltinCallExpr::getBuiltinKind(builtinName);
    if (!builtinKind.has_value()) {
        reportError(DiagID::err_unknown_builtin_function, builtinTok.getLocation());
        return ParseResult<Expr>::error();
    }
    
    // 验证内置函数名称有效性（通过 BuiltinRegistry）
    if (!BuiltinRegistry::instance().isBuiltin(builtinName)) {
        reportError(DiagID::err_unknown_builtin_function, builtinTok.getLocation());
        return ParseResult<Expr>::error();
    }
    
    // 期望 '('
    if (!expectAndConsume(TokenKind::LParen)) {
        return ParseResult<Expr>::error();
    }
    
    // 解析参数列表（支持类型和表达式混合）
    std::vector<BuiltinCallExpr::Argument> args;
    if (!check(TokenKind::RParen)) {
        do {
            // 对于某些内置函数（如 @sizeof, @alignof），需要判断是类型还是表达式
            // 策略：如果是明确的类型关键字或类型构造符，解析为类型；否则解析为表达式
            bool shouldParseAsType = false;

            if (*builtinKind == BuiltinKind::Sizeof ||
                *builtinKind == BuiltinKind::Alignof) {
                // 检查是否是明确的类型开始
                TokenKind kind = CurTok.getKind();
                if (kind == TokenKind::KW_i8 || kind == TokenKind::KW_i16 ||
                    kind == TokenKind::KW_i32 || kind == TokenKind::KW_i64 ||
                    kind == TokenKind::KW_i128 || kind == TokenKind::KW_isize ||
                    kind == TokenKind::KW_u8 || kind == TokenKind::KW_u16 ||
                    kind == TokenKind::KW_u32 || kind == TokenKind::KW_u64 ||
                    kind == TokenKind::KW_u128 || kind == TokenKind::KW_usize ||
                    kind == TokenKind::KW_f32 || kind == TokenKind::KW_f64 ||
                    kind == TokenKind::KW_bool || kind == TokenKind::KW_char ||
                    kind == TokenKind::KW_str || kind == TokenKind::KW_void ||
                    kind == TokenKind::Amp || kind == TokenKind::Star ||
                    kind == TokenKind::Question || kind == TokenKind::KW_func) {
                    shouldParseAsType = true;
                } else if (kind == TokenKind::LBracket) {
                    // [T; N] 或 [T] 是类型
                    shouldParseAsType = true;
                } else if (kind == TokenKind::LParen) {
                    // (T, U) 可能是元组类型或元组表达式，默认解析为表达式
                    shouldParseAsType = false;
                } else if (kind == TokenKind::Identifier) {
                    // 标识符后面跟 < 可能是泛型类型，否则可能是表达式
                    // 向前看一个 token
                    Token next = peekAhead(1);
                    if (next.is(TokenKind::Less)) {
                        shouldParseAsType = true;
                    } else if (next.is(TokenKind::LParen)) {
                        // Name(...) 是函数调用表达式
                        shouldParseAsType = false;
                    } else {
                        // 其他情况，默认解析为类型
                        shouldParseAsType = true;
                    }
                }
            }

            if (shouldParseAsType) {
                // 解析类型
                auto typeResult = parseType();
                if (typeResult.isError()) {
                    return ParseResult<Expr>::error();
                }
                args.emplace_back(typeResult.get());
            } else {
                // 解析表达式
                auto argResult = parseExpr();
                if (argResult.isError()) {
                    return ParseResult<Expr>::error();
                }
                args.emplace_back(argResult.get());
            }
        } while (match(TokenKind::Comma));
    }
    
    // 消费 ')'
    if (!expectAndConsume(TokenKind::RParen)) {
        return ParseResult<Expr>::error();
    }
    
    SourceRange range(startLoc, PrevTok.getRange().getEnd());
    return ParseResult<Expr>(
        Ctx.create<BuiltinCallExpr>(range, *builtinKind, std::move(args)));
}

/// \brief 解析块表达式 { stmts; expr }
ParseResult<Expr> Parser::parseBlockExpr() {
    SourceLocation startLoc = CurTok.getLocation();

    // 期望左花括号
    if (!expectAndConsume(TokenKind::LBrace, DiagID::err_expected_lbrace)) {
        return ParseResult<Expr>::error();
    }

    std::vector<Stmt*> stmts;
    Expr* resultExpr = nullptr;

    // 解析块内容：允许最后一个元素作为结果表达式
    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        // 语句关键字优先处理（这些不能作为表达式）
        bool isStmtOnly =
            check(TokenKind::KW_var) ||
            check(TokenKind::KW_const) ||
            check(TokenKind::KW_return) ||
            check(TokenKind::KW_while) ||
            check(TokenKind::KW_for) ||
            check(TokenKind::KW_break) ||
            check(TokenKind::KW_continue) ||
            check(TokenKind::KW_defer);

        if (isStmtOnly) {
            auto stmtResult = parseStmt();
            if (stmtResult.isSuccess()) {
                Stmt* stmt = stmtResult.get();
                stmts.push_back(stmt);
                if (!check(TokenKind::RBrace) && !isAtEnd() &&
                    !Lex.isNewLineBetween(stmt->getEndLoc(), CurTok.getLocation())) {
                    reportError(DiagID::err_unexpected_token);
                    return ParseResult<Expr>::error();
                }
            } else {
                synchronize();
            }
            continue;
        }

        // 尝试解析表达式
        auto exprResult = parseExpr();
        if (exprResult.isError()) {
            synchronize();
            continue;
        }

        Expr* expr = exprResult.get();

        // 如果后面紧跟右花括号，将其作为结果表达式
        if (check(TokenKind::RBrace)) {
            resultExpr = expr;
            break;
        }

        // 否则作为表达式语句加入块
        if (!isValidExprStmt(expr)) {
            reportError(DiagID::err_expression_statement_no_effect);
            return ParseResult<Expr>::error();
        }

        SourceRange exprRange = expr->getRange();
        stmts.push_back(Ctx.create<ExprStmt>(exprRange, expr));
        if (!check(TokenKind::RBrace) && !isAtEnd() &&
            !Lex.isNewLineBetween(expr->getEndLoc(), CurTok.getLocation())) {
            reportError(DiagID::err_unexpected_token);
            return ParseResult<Expr>::error();
        }
    }

    // 期望右花括号
    if (!expectAndConsume(TokenKind::RBrace, DiagID::err_expected_rbrace)) {
        return ParseResult<Expr>::error();
    }

    SourceRange range(startLoc, PrevTok.getLocation());
    return ParseResult<Expr>(Ctx.create<BlockExpr>(range, std::move(stmts), resultExpr));
}

/// \brief 解析 loop 表达式
ParseResult<Expr> Parser::parseLoopExpr() {
    SourceLocation startLoc = CurTok.getLocation();

    // 消费 'loop' 关键字
    if (!expectAndConsume(TokenKind::KW_loop)) {
        return ParseResult<Expr>::error();
    }

    // 解析循环体（块表达式）
    auto bodyResult = parseBlockExpr();
    if (bodyResult.isError()) {
        return ParseResult<Expr>::error();
    }

    SourceRange range(startLoc, PrevTok.getLocation());
    return ParseResult<Expr>(Ctx.create<LoopExpr>(range, bodyResult.get()));
}

} // namespace yuan
