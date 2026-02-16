/// \file ParsePattern.cpp
/// \brief 模式解析实现。
///
/// 本文件实现了 Parser 类中与模式解析相关的方法，
/// 包括通配符、标识符、字面量、元组、结构体、枚举模式等。

#include "yuan/Parser/Parser.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Pattern.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Type.h"
#include "yuan/Basic/Diagnostic.h"

namespace yuan {

// ============================================================================
// 模式解析
// ============================================================================

ParseResult<Pattern> Parser::parsePattern() {
    auto firstResult = parsePatternAtom();
    if (firstResult.isError()) {
        return ParseResult<Pattern>::error();
    }

    Pattern* base = firstResult.get();
    std::vector<Pattern*> alternatives;
    alternatives.push_back(base);

    // 处理或模式：p1 | p2 | p3
    while (match(TokenKind::Pipe)) {
        auto rhsResult = parsePatternAtom();
        if (rhsResult.isError()) {
            return ParseResult<Pattern>::error();
        }
        alternatives.push_back(rhsResult.get());
    }

    if (alternatives.size() == 1) {
        return ParseResult<Pattern>(base);
    }

    SourceRange range(alternatives.front()->getBeginLoc(),
                      alternatives.back()->getEndLoc());
    return ParseResult<Pattern>(
        Ctx.create<OrPattern>(range, std::move(alternatives)));
}

ParseResult<Pattern> Parser::parsePatternAtom() {
    SourceLocation startLoc = CurTok.getLocation();

    // 处理带泛型参数的类型模式（例如 Option<T>.None / Point<T> { ... }）
    if (check(TokenKind::Identifier) && peekAhead(1).is(TokenKind::Less)) {
        auto typeResult = parseType();
        if (typeResult.isError()) {
            return ParseResult<Pattern>::error();
        }

        TypeNode* typeNode = typeResult.get();
        std::string typeName;
        if (auto* genericType = dynamic_cast<GenericTypeNode*>(typeNode)) {
            typeName = genericType->getBaseName();
        } else if (auto* identType = dynamic_cast<IdentifierTypeNode*>(typeNode)) {
            typeName = identType->getName();
        }

        if (typeName.empty()) {
            reportError(DiagID::err_expected_pattern);
            return ParseResult<Pattern>::error();
        }

        if (check(TokenKind::LBrace)) {
            return parseStructPattern(typeName);
        }

        if (check(TokenKind::ColonColon) || check(TokenKind::Dot)) {
            return parseEnumPattern(typeName);
        }

        reportError(DiagID::err_expected_pattern);
        return ParseResult<Pattern>::error();
    }

    // 处理 box 前缀模式（例如 box Pat）
    if (check(TokenKind::Identifier) && CurTok.getText() == "box") {
        Token next = peekAhead(1);
        if (next.isOneOf(TokenKind::Identifier, TokenKind::KW_mut, TokenKind::LParen,
                         TokenKind::LBracket, TokenKind::IntegerLiteral, TokenKind::FloatLiteral,
                         TokenKind::CharLiteral, TokenKind::StringLiteral, TokenKind::KW_true,
                         TokenKind::KW_false, TokenKind::KW_None, TokenKind::DotDot,
                         TokenKind::DotDotEqual)) {
            consume(); // 消费 box
            // 直接解析内部模式
            return parsePattern();
        }
    }

    switch (CurTok.getKind()) {
        // 标识符模式或枚举/结构体模式
        case TokenKind::Identifier: {
            Token tok = consume();
            std::string name = std::string(tok.getText());

            // 特殊处理：如果标识符是"_"，则解析为通配符模式
            if (name == "_") {
                SourceRange range(tok.getLocation(), tok.getRange().getEnd());
                return ParseResult<Pattern>(
                    Ctx.create<WildcardPattern>(range));
            }

            // 检查是否为结构体模式 Name { ... }
            if (check(TokenKind::LBrace)) {
                return parseStructPattern(name);
            }

            // 检查是否为枚举模式 Name::Variant 或 Name.Variant
            if (check(TokenKind::ColonColon) || check(TokenKind::Dot)) {
                return parseEnumPattern(name);
            }

            // 检查是否为枚举变体模式 Some(value) - 直接使用变体名
            if (check(TokenKind::LParen)) {
                // 这是一个枚举变体模式，省略了枚举名
                return parseEnumPattern(name);
            }

            // 检查是否有类型注解 name: Type
            TypeNode* type = nullptr;
            if (check(TokenKind::Colon)) {
                consume(); // 消费 ':'
                auto typeResult = parseType();
                if (typeResult.isError()) {
                    return ParseResult<Pattern>::error();
                }
                type = typeResult.get();
            }

            // 绑定模式 name @ pattern（@ 在词法层可能是 BuiltinIdentifier）
            if (match(TokenKind::At) ||
                (check(TokenKind::BuiltinIdentifier) && CurTok.getText() == "@" && (consume(), true))) {
                auto innerResult = parsePattern();
                if (innerResult.isError()) {
                    return ParseResult<Pattern>::error();
                }
                SourceRange range(startLoc, innerResult.get()->getEndLoc());
                return ParseResult<Pattern>(
                    Ctx.create<BindPattern>(range, name, innerResult.get(), false, type));
            }

            // 普通标识符模式
            SourceRange range(tok.getLocation(),
                              type ? type->getRange().getEnd() : tok.getRange().getEnd());
            return ParseResult<Pattern>(
                Ctx.create<IdentifierPattern>(range, name, false, type));
        }

        // Self 类型名模式（仅允许用于结构体/枚举模式）
        case TokenKind::KW_Self: {
            Token tok = consume();
            std::string name = std::string(tok.getText());

            if (check(TokenKind::LBrace)) {
                return parseStructPattern(name);
            }

            if (check(TokenKind::ColonColon) || check(TokenKind::Dot)) {
                return parseEnumPattern(name);
            }

            reportError(DiagID::err_expected_pattern);
            return ParseResult<Pattern>::error();
        }
        
        // mut 标识符模式
        case TokenKind::KW_mut: {
            consume();
            if (!check(TokenKind::Identifier)) {
                reportError(DiagID::err_expected_identifier);
                return ParseResult<Pattern>::error();
            }
            Token tok = consume();
            std::string name = std::string(tok.getText());
            
            // 检查是否有类型注解 mut name: Type
            TypeNode* type = nullptr;
            if (check(TokenKind::Colon)) {
                consume(); // 消费 ':'
                auto typeResult = parseType();
                if (typeResult.isError()) {
                    return ParseResult<Pattern>::error();
                }
                type = typeResult.get();
            }

            // 绑定模式 mut name @ pattern
            if (match(TokenKind::At) ||
                (check(TokenKind::BuiltinIdentifier) && CurTok.getText() == "@" && (consume(), true))) {
                auto innerResult = parsePattern();
                if (innerResult.isError()) {
                    return ParseResult<Pattern>::error();
                }
                SourceRange range(startLoc, innerResult.get()->getEndLoc());
                return ParseResult<Pattern>(
                    Ctx.create<BindPattern>(range, name, innerResult.get(), true, type));
            }
            
            SourceRange range(startLoc, 
                            type ? type->getRange().getEnd() : tok.getRange().getEnd());
            return ParseResult<Pattern>(
                Ctx.create<IdentifierPattern>(range, name, true, type));
        }
        
        // 字面量模式
        case TokenKind::IntegerLiteral:
        case TokenKind::FloatLiteral:
        case TokenKind::CharLiteral:
        case TokenKind::StringLiteral:
        case TokenKind::KW_true:
        case TokenKind::KW_false:
        case TokenKind::KW_None: {
            auto exprResult = parsePrimaryExpr();
            if (exprResult.isError()) {
                return ParseResult<Pattern>::error();
            }
            
            // 检查是否为范围模式
            if (check(TokenKind::DotDot) || check(TokenKind::DotDotEqual)) {
                return parseRangePattern(exprResult.get());
            }
            
            return ParseResult<Pattern>(
                Ctx.create<LiteralPattern>(exprResult->getRange(), exprResult.get()));
        }
        
        // 元组模式 (p1, p2, ...)
        case TokenKind::LParen:
            return parseTuplePattern();

        // 数组/切片模式 [p1, p2, ..]
        case TokenKind::LBracket:
            return parseArrayPattern();

        // 范围模式 ..end 或 ..=end 或 ..
        case TokenKind::DotDot:
        case TokenKind::DotDotEqual: {
            bool isInclusive = check(TokenKind::DotDotEqual);
            Token rangeOp = consume(); // 消费 '..' 或 '..='

            // 解析结束值（可选，支持完整范围 ..）
            Expr* end = nullptr;
            SourceLocation endLoc = rangeOp.getRange().getEnd();

            // 如果下一个 token 可以开始一个表达式，则解析结束值
            // 支持字面量和一元表达式（如 -5）
            if (check(TokenKind::IntegerLiteral) || check(TokenKind::FloatLiteral) ||
                check(TokenKind::CharLiteral) || check(TokenKind::StringLiteral) ||
                check(TokenKind::KW_true) || check(TokenKind::KW_false) ||
                check(TokenKind::KW_None) || check(TokenKind::Minus) ||
                check(TokenKind::Plus) || check(TokenKind::Exclaim)) {
                // 直接使用 parsePrimaryExpr，不解析后缀
                auto endResult = parsePrimaryExpr();
                if (endResult.isSuccess()) {
                    end = endResult.get();
                    endLoc = end->getRange().getEnd();
                }
            }

            SourceRange range(startLoc, endLoc);
            return ParseResult<Pattern>(
                Ctx.create<RangePattern>(range, nullptr, end, isInclusive));
        }

        default:
            reportError(DiagID::err_expected_pattern);
            return ParseResult<Pattern>::error();
    }
}

ParseResult<Pattern> Parser::parseTuplePattern() {
    SourceLocation startLoc = CurTok.getLocation();
    
    if (!check(TokenKind::LParen)) {
        reportExpectedError(TokenKind::LParen);
        return ParseResult<Pattern>::error();
    }
    consume(); // 消费 '('
    
    std::vector<Pattern*> elements;
    
    // 处理空元组 ()
    if (check(TokenKind::RParen)) {
        Token endTok = consume();
        SourceRange range(startLoc, endTok.getRange().getEnd());
        return ParseResult<Pattern>(
            Ctx.create<TuplePattern>(range, std::move(elements)));
    }
    
    // 解析第一个元素
    auto firstResult = parsePattern();
    if (firstResult.isError()) {
        return ParseResult<Pattern>::error();
    }
    elements.push_back(firstResult.get());
    
    // 如果只有一个元素且没有逗号，这是括号表达式，不是元组
    if (check(TokenKind::RParen)) {
        Token endTok = consume();
        // 单元素元组需要有逗号，如 (x,)
        // 没有逗号的 (x) 不是元组模式，而是括号包围的模式
        return firstResult; // 返回括号内的模式
    }
    
    // 解析剩余元素
    while (check(TokenKind::Comma)) {
        consume(); // 消费 ','
        
        // 允许尾随逗号
        if (check(TokenKind::RParen)) {
            break;
        }
        
        auto elementResult = parsePattern();
        if (elementResult.isError()) {
            return ParseResult<Pattern>::error();
        }
        elements.push_back(elementResult.get());
    }
    
    if (!check(TokenKind::RParen)) {
        reportExpectedError(TokenKind::RParen);
        return ParseResult<Pattern>::error();
    }
    
    Token endTok = consume(); // 消费 ')'
    SourceRange range(startLoc, endTok.getRange().getEnd());
    
    return ParseResult<Pattern>(
        Ctx.create<TuplePattern>(range, std::move(elements)));
}

ParseResult<Pattern> Parser::parseArrayPattern() {
    SourceLocation startLoc = CurTok.getLocation();

    if (!expectAndConsume(TokenKind::LBracket)) {
        return ParseResult<Pattern>::error();
    }

    std::vector<Pattern*> elements;

    // 空数组模式 []
    if (check(TokenKind::RBracket)) {
        Token endTok = consume();
        SourceRange range(startLoc, endTok.getRange().getEnd());
        return ParseResult<Pattern>(
            Ctx.create<TuplePattern>(range, std::move(elements)));
    }

    // 解析元素模式
    while (!check(TokenKind::RBracket) && !isAtEnd()) {
        auto elementResult = parsePattern();
        if (elementResult.isError()) {
            return ParseResult<Pattern>::error();
        }
        elements.push_back(elementResult.get());

        if (match(TokenKind::Comma)) {
            // 允许尾随逗号
            if (check(TokenKind::RBracket)) {
                break;
            }
            continue;
        }
        break;
    }

    if (!expectAndConsume(TokenKind::RBracket)) {
        return ParseResult<Pattern>::error();
    }

    SourceLocation endLoc = PrevTok.getRange().getEnd();
    SourceRange range(startLoc, endLoc);

    // 当前实现复用 TuplePattern 来表示数组/切片模式
    return ParseResult<Pattern>(
        Ctx.create<TuplePattern>(range, std::move(elements)));
}

ParseResult<Pattern> Parser::parseStructPattern(const std::string& typeName) {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 注意：调用此方法时，当前Token应该已经是LBrace
    if (!check(TokenKind::LBrace)) {
        reportExpectedError(TokenKind::LBrace);
        return ParseResult<Pattern>::error();
    }
    consume(); // 消费 '{'
    
    std::vector<StructPatternField> fields;
    bool hasRest = false;
    
    // 处理空结构体模式 Name {}
    if (check(TokenKind::RBrace)) {
        Token endTok = consume();
        SourceRange range(startLoc, endTok.getRange().getEnd());
        return ParseResult<Pattern>(
            Ctx.create<StructPattern>(range, typeName, std::move(fields), hasRest));
    }
    
    while (!check(TokenKind::RBrace)) {
        // 检查是否为 .. 省略其余字段
        if (check(TokenKind::DotDot)) {
            consume();
            hasRest = true;
            break;
        }
        
        // 解析字段名
        if (!check(TokenKind::Identifier)) {
            reportError(DiagID::err_expected_identifier);
            return ParseResult<Pattern>::error();
        }
        
        Token fieldTok = consume();
        std::string fieldName = std::string(fieldTok.getText());
        SourceLocation fieldLoc = fieldTok.getLocation();
        
        Pattern* fieldPattern = nullptr;
        
        // 检查是否有 : pattern
        if (check(TokenKind::Colon)) {
            consume(); // 消费 ':'
            auto patternResult = parsePattern();
            if (patternResult.isError()) {
                return ParseResult<Pattern>::error();
            }
            fieldPattern = patternResult.get();
        } else {
            // 简写形式，字段名即为标识符模式
            SourceRange fieldRange(fieldLoc, fieldTok.getRange().getEnd());
            fieldPattern = Ctx.create<IdentifierPattern>(fieldRange, fieldName, false);
        }
        
        fields.emplace_back(fieldName, fieldPattern, fieldLoc);
        
        // 检查是否有逗号
        if (check(TokenKind::Comma)) {
            consume();
            // 允许尾随逗号
            if (check(TokenKind::RBrace)) {
                break;
            }
        } else if (!check(TokenKind::RBrace) && !check(TokenKind::DotDot)) {
            Diag.report(DiagID::err_expected_comma_or_close, CurTok.getLocation(), CurTok.getRange())
                << "}";
            return ParseResult<Pattern>::error();
        }
    }
    
    if (!check(TokenKind::RBrace)) {
        reportExpectedError(TokenKind::RBrace);
        return ParseResult<Pattern>::error();
    }
    
    Token endTok = consume(); // 消费 '}'
    SourceRange range(startLoc, endTok.getRange().getEnd());
    
    return ParseResult<Pattern>(
        Ctx.create<StructPattern>(range, typeName, std::move(fields), hasRest));
}

ParseResult<Pattern> Parser::parseEnumPattern(const std::string& typeName) {
    SourceLocation startLoc = CurTok.getLocation();
    std::string enumName = typeName;
    std::string variantName;
    
    // 解析 Name::Variant 或 Name.Variant
    if (check(TokenKind::ColonColon) || check(TokenKind::Dot)) {
        consume(); // 消费 '::' 或 '.'
        
        if (!check(TokenKind::Identifier) && !check(TokenKind::KW_None)) {
            reportError(DiagID::err_expected_identifier);
            return ParseResult<Pattern>::error();
        }

        Token variantTok = consume();
        variantName = std::string(variantTok.getText());
    } else {
        // 如果没有 :: 或 .，说明这是一个简单的变体名（省略枚举名）
        variantName = typeName;
        enumName = ""; // 空枚举名表示省略
    }
    
    std::vector<Pattern*> payload;
    SourceLocation endLoc = CurTok.getLocation();
    
    // 检查是否有负载 Variant(p1, p2, ...)
    if (check(TokenKind::LParen)) {
        consume(); // 消费 '('
        
        // 处理空负载 Variant()
        if (check(TokenKind::RParen)) {
            Token endTok = consume();
            endLoc = endTok.getRange().getEnd();
        } else {
            // 解析负载模式
            while (!check(TokenKind::RParen)) {
                auto patternResult = parsePattern();
                if (patternResult.isError()) {
                    return ParseResult<Pattern>::error();
                }
                payload.push_back(patternResult.get());
                
                if (check(TokenKind::Comma)) {
                    consume();
                    // 允许尾随逗号
                    if (check(TokenKind::RParen)) {
                        break;
                    }
                } else if (!check(TokenKind::RParen)) {
                    Diag.report(DiagID::err_expected_comma_or_close, CurTok.getLocation(), CurTok.getRange())
                        << ")";
                    return ParseResult<Pattern>::error();
                }
            }
            
            if (!check(TokenKind::RParen)) {
                reportExpectedError(TokenKind::RParen);
                return ParseResult<Pattern>::error();
            }
            Token endTok = consume(); // 消费 ')'
            endLoc = endTok.getRange().getEnd();
        }
    } else if (check(TokenKind::LBrace)) {
        // 结构体样式负载 Variant { field, field: pat, .. }
        consume(); // 消费 '{'

        // 处理空负载 Variant {}
        if (check(TokenKind::RBrace)) {
            Token endTok = consume();
            endLoc = endTok.getRange().getEnd();
        } else {
            while (!check(TokenKind::RBrace)) {
                // 处理 .. 省略其余字段
                if (check(TokenKind::DotDot)) {
                    consume();
                    break;
                }

                if (!check(TokenKind::Identifier)) {
                    reportError(DiagID::err_expected_identifier);
                    return ParseResult<Pattern>::error();
                }

                Token fieldTok = consume();
                std::string fieldName = std::string(fieldTok.getText());

                Pattern* fieldPat = nullptr;
                if (match(TokenKind::Colon)) {
                    auto patResult = parsePattern();
                    if (patResult.isError()) {
                        return ParseResult<Pattern>::error();
                    }
                    fieldPat = patResult.get();
                } else {
                    SourceRange fieldRange(fieldTok.getLocation(), fieldTok.getRange().getEnd());
                    fieldPat = Ctx.create<IdentifierPattern>(fieldRange, fieldName, false);
                }

                payload.push_back(fieldPat);

                if (match(TokenKind::Comma)) {
                    if (check(TokenKind::RBrace)) {
                        break;
                    }
                } else if (!check(TokenKind::RBrace) && !check(TokenKind::DotDot)) {
                    Diag.report(DiagID::err_expected_comma_or_close, CurTok.getLocation(), CurTok.getRange())
                        << "}";
                    return ParseResult<Pattern>::error();
                }
            }

            if (!check(TokenKind::RBrace)) {
                reportExpectedError(TokenKind::RBrace);
                return ParseResult<Pattern>::error();
            }

            Token endTok = consume(); // 消费 '}'
            endLoc = endTok.getRange().getEnd();
        }
    }
    
    SourceRange range(startLoc, endLoc);
    
    return ParseResult<Pattern>(
        Ctx.create<EnumPattern>(range, enumName, variantName, std::move(payload)));
}

ParseResult<Pattern> Parser::parseRangePattern(Expr* start) {
    SourceLocation startLoc = start->getRange().getBegin();

    // 检查范围操作符
    bool isInclusive = false;
    Token rangeOp;
    if (check(TokenKind::DotDotEqual)) {
        rangeOp = consume();
        isInclusive = true;
    } else if (check(TokenKind::DotDot)) {
        rangeOp = consume();
        isInclusive = false;
    } else {
        Diag.report(DiagID::err_expected_token, CurTok.getLocation())
            << ".. or ..="
            << CurTok.getText();
        return ParseResult<Pattern>::error();
    }

    // 解析结束值（可选，支持开放式范围如 10..）
    Expr* end = nullptr;
    SourceLocation endLoc = rangeOp.getRange().getEnd();

    // 如果下一个 token 可以开始一个表达式，则解析结束值
    // 支持字面量和一元表达式（如 -5）
    if (check(TokenKind::IntegerLiteral) || check(TokenKind::FloatLiteral) ||
        check(TokenKind::CharLiteral) || check(TokenKind::StringLiteral) ||
        check(TokenKind::KW_true) || check(TokenKind::KW_false) ||
        check(TokenKind::KW_None) || check(TokenKind::Minus) ||
        check(TokenKind::Plus) || check(TokenKind::Exclaim)) {
        // 直接使用 parsePrimaryExpr，不解析后缀
        auto endResult = parsePrimaryExpr();
        if (endResult.isSuccess()) {
            end = endResult.get();
            endLoc = end->getRange().getEnd();
        }
    }

    SourceRange range(startLoc, endLoc);

    return ParseResult<Pattern>(
        Ctx.create<RangePattern>(range, start, end, isInclusive));
}

} // namespace yuan
