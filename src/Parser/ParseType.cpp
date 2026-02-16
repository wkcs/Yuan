/// \file ParseType.cpp
/// \brief 类型解析实现。
///
/// 本文件实现了 Parser 类中与类型解析相关的方法，
/// 包括内置类型、数组、元组、引用、指针、函数类型等。

#include "yuan/Parser/Parser.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Type.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Lexer/Lexer.h"

namespace yuan {

// ============================================================================
// 类型解析
// ============================================================================

ParseResult<TypeNode> Parser::parseType() {
    SourceLocation startLoc = CurTok.getLocation();

    // 处理 trait object 语法：dyn Trait
    if (check(TokenKind::Identifier) && CurTok.getText() == "dyn") {
        consume(); // 消费 dyn
        auto innerResult = parseType();
        if (innerResult.isError()) {
            return ParseResult<TypeNode>::error();
        }
        return ParseResult<TypeNode>(innerResult.get());
    }
    
    switch (CurTok.getKind()) {
        // 内置类型
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
        case TokenKind::KW_f64:
            return parseBuiltinType();
        
        // 数组类型 [T; N] 或切片类型 [T]
        case TokenKind::LBracket:
            return parseArrayType();
        
        // 元组类型 (T1, T2, ...)
        case TokenKind::LParen:
            return parseTupleType();
        
        // 引用类型 &T 或 &mut T
        case TokenKind::Amp:
            return parseReferenceType();
        
        // 指针类型 *T 或 *mut T
        case TokenKind::Star:
            return parsePointerType();
        
        // 函数类型 func(T1, T2) -> R
        case TokenKind::KW_func:
            return parseFunctionType();

        // impl Trait 类型（暂时降级为内部类型）
        case TokenKind::KW_impl: {
            consume(); // 消费 impl
            auto innerResult = parseType();
            if (innerResult.isError()) {
                return ParseResult<TypeNode>::error();
            }
            return ParseResult<TypeNode>(innerResult.get());
        }
        
        // Optional 类型 ?T
        case TokenKind::Question:
            return parseOptionalType();
        
        // 错误类型 !T
        case TokenKind::Exclaim:
            return parseErrorType();
        
        // Self 类型
        case TokenKind::KW_Self: {
            Token tok = consume();
            SourceLocation startLoc = tok.getLocation();

            // 检查是否有关联类型 Self.Type
            if (check(TokenKind::Dot)) {
                consume(); // 消费 '.'
                if (!check(TokenKind::Identifier)) {
                    reportError(DiagID::err_expected_identifier);
                    return ParseResult<TypeNode>::error();
                }
                Token memberTok = consume();
                std::string memberName = std::string(memberTok.getText());
                SourceRange range(startLoc, memberTok.getRange().getEnd());

                // 创建关联类型节点 (使用 IdentifierTypeNode，名称为 "Self.TypeName")
                return ParseResult<TypeNode>(
                    Ctx.create<IdentifierTypeNode>(range, "Self." + memberName));
            }

            // 处理 Self 的泛型参数 Self<T>
            if (check(TokenKind::Less)) {
                std::vector<TypeNode*> typeArgs = parseGenericArgs();
                SourceLocation endLoc = PrevTok.getRange().getEnd();
                SourceRange range(startLoc, endLoc);
                return ParseResult<TypeNode>(
                    Ctx.create<GenericTypeNode>(range, "Self", std::move(typeArgs)));
            }

            SourceRange range(tok.getLocation(), tok.getRange().getEnd());
            return ParseResult<TypeNode>(
                Ctx.create<IdentifierTypeNode>(range, "Self"));
        }
        
        // 标识符类型（用户定义类型或泛型）
        case TokenKind::Identifier:
            return parseIdentifierType();
        
        default:
            reportError(DiagID::err_expected_type);
            return ParseResult<TypeNode>::error();
    }
}

ParseResult<TypeNode> Parser::parseBuiltinType() {
    Token tok = consume();
    SourceRange range(tok.getLocation(), tok.getRange().getEnd());
    
    BuiltinTypeNode::BuiltinKind kind;
    switch (tok.getKind()) {
        case TokenKind::KW_void:  kind = BuiltinTypeNode::BuiltinKind::Void; break;
        case TokenKind::KW_bool:  kind = BuiltinTypeNode::BuiltinKind::Bool; break;
        case TokenKind::KW_char:  kind = BuiltinTypeNode::BuiltinKind::Char; break;
        case TokenKind::KW_str:   kind = BuiltinTypeNode::BuiltinKind::Str; break;
        case TokenKind::KW_i8:    kind = BuiltinTypeNode::BuiltinKind::I8; break;
        case TokenKind::KW_i16:   kind = BuiltinTypeNode::BuiltinKind::I16; break;
        case TokenKind::KW_i32:   kind = BuiltinTypeNode::BuiltinKind::I32; break;
        case TokenKind::KW_i64:   kind = BuiltinTypeNode::BuiltinKind::I64; break;
        case TokenKind::KW_i128:  kind = BuiltinTypeNode::BuiltinKind::I128; break;
        case TokenKind::KW_isize: kind = BuiltinTypeNode::BuiltinKind::ISize; break;
        case TokenKind::KW_u8:    kind = BuiltinTypeNode::BuiltinKind::U8; break;
        case TokenKind::KW_u16:   kind = BuiltinTypeNode::BuiltinKind::U16; break;
        case TokenKind::KW_u32:   kind = BuiltinTypeNode::BuiltinKind::U32; break;
        case TokenKind::KW_u64:   kind = BuiltinTypeNode::BuiltinKind::U64; break;
        case TokenKind::KW_u128:  kind = BuiltinTypeNode::BuiltinKind::U128; break;
        case TokenKind::KW_usize: kind = BuiltinTypeNode::BuiltinKind::USize; break;
        case TokenKind::KW_f32:   kind = BuiltinTypeNode::BuiltinKind::F32; break;
        case TokenKind::KW_f64:   kind = BuiltinTypeNode::BuiltinKind::F64; break;
        default:
            reportError(DiagID::err_expected_type);
            return ParseResult<TypeNode>::error();
    }
    
    return ParseResult<TypeNode>(
        Ctx.create<BuiltinTypeNode>(range, kind));
}

ParseResult<TypeNode> Parser::parseArrayType() {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 '['
    if (!expectAndConsume(TokenKind::LBracket)) {
        return ParseResult<TypeNode>::error();
    }
    
    // 解析元素类型
    auto elementResult = parseType();
    if (elementResult.isError()) {
        return ParseResult<TypeNode>::error();
    }
    
    TypeNode* elementType = elementResult.get();
    
    // 检查是否是数组 [T; N] / [T, N]，或兼容 [T \n N] 形式
    bool implicitArraySize = !check(TokenKind::RBracket) &&
                             Lex.isNewLineBetween(elementType->getEndLoc(), CurTok.getLocation());
    if (match(TokenKind::Semicolon) || match(TokenKind::Comma) || implicitArraySize) {
        // 数组类型 [T; N]
        auto sizeResult = parseExpr();
        if (sizeResult.isError()) {
            return ParseResult<TypeNode>::error();
        }
        
        if (!expectAndConsume(TokenKind::RBracket)) {
            return ParseResult<TypeNode>::error();
        }
        
        SourceLocation endLoc = PrevTok.getRange().getEnd();
        SourceRange range(startLoc, endLoc);
        
        return ParseResult<TypeNode>(
            Ctx.create<ArrayTypeNode>(range, elementType, sizeResult.get()));
    } else {
        // 切片类型 [T] - 实际上应该是 &[T]，但这里我们创建一个切片类型
        if (!expectAndConsume(TokenKind::RBracket)) {
            return ParseResult<TypeNode>::error();
        }
        
        SourceLocation endLoc = PrevTok.getRange().getEnd();
        SourceRange range(startLoc, endLoc);
        
        // 创建不可变切片类型
        return ParseResult<TypeNode>(
            Ctx.create<SliceTypeNode>(range, elementType, false));
    }
}

ParseResult<TypeNode> Parser::parseTupleType() {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 '('
    if (!expectAndConsume(TokenKind::LParen)) {
        return ParseResult<TypeNode>::error();
    }
    
    std::vector<TypeNode*> elements;
    
    // 空元组 ()
    if (check(TokenKind::RParen)) {
        consume();
        SourceLocation endLoc = PrevTok.getRange().getEnd();
        SourceRange range(startLoc, endLoc);
        return ParseResult<TypeNode>(
            Ctx.create<TupleTypeNode>(range, std::move(elements)));
    }
    
    // 解析第一个元素类型
    auto firstResult = parseType();
    if (firstResult.isError()) {
        return ParseResult<TypeNode>::error();
    }
    elements.push_back(firstResult.get());
    
    // 检查是否是单元素元组（需要逗号）或多元素元组
    if (match(TokenKind::Comma)) {
        // 多元素元组或单元素元组（带逗号）
        while (!check(TokenKind::RParen)) {
            if (check(TokenKind::RParen)) {
                break;  // 允许尾随逗号
            }
            
            auto elementResult = parseType();
            if (elementResult.isError()) {
                return ParseResult<TypeNode>::error();
            }
            elements.push_back(elementResult.get());
            
            if (!match(TokenKind::Comma)) {
                break;
            }
        }
    } else if (check(TokenKind::RParen)) {
        // 单个类型在括号中，这不是元组，而是括号表达式
        // 但在类型上下文中，我们将其视为单元素元组
        // 实际上，在 Yuan 中，(T) 应该等同于 T，而不是单元素元组
        // 单元素元组需要尾随逗号：(T,)
        consume(); // 消费 ')'
        SourceLocation endLoc = PrevTok.getRange().getEnd();
        SourceRange range(startLoc, endLoc);
        
        // 返回原始类型，而不是元组
        return ParseResult<TypeNode>(firstResult.get());
    }
    
    if (!expectAndConsume(TokenKind::RParen)) {
        return ParseResult<TypeNode>::error();
    }
    
    SourceLocation endLoc = PrevTok.getRange().getEnd();
    SourceRange range(startLoc, endLoc);
    
    return ParseResult<TypeNode>(
        Ctx.create<TupleTypeNode>(range, std::move(elements)));
}

ParseResult<TypeNode> Parser::parseReferenceType() {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 '&'
    consume();
    
    // 检查是否有 'mut' 修饰符
    bool isMut = match(TokenKind::KW_mut);
    
    // 解析被引用的类型
    auto pointeeResult = parseType();
    if (pointeeResult.isError()) {
        return ParseResult<TypeNode>::error();
    }

    SourceLocation endLoc = PrevTok.getRange().getEnd();
    SourceRange range(startLoc, endLoc);

    // 特殊处理：&[T] 或 &mut [T] 解析为切片类型
    if (auto* sliceNode = dynamic_cast<SliceTypeNode*>(pointeeResult.get())) {
        bool isMutSlice = isMut || sliceNode->isMutable();
        return ParseResult<TypeNode>(
            Ctx.create<SliceTypeNode>(range, sliceNode->getElementType(), isMutSlice));
    }
    
    return ParseResult<TypeNode>(
        Ctx.create<ReferenceTypeNode>(range, pointeeResult.get(), isMut));
}

ParseResult<TypeNode> Parser::parsePointerType() {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 '*'
    if (!expectAndConsume(TokenKind::Star)) {
        return ParseResult<TypeNode>::error();
    }
    
    // 检查是否有 'mut' 修饰符
    bool isMut = match(TokenKind::KW_mut);
    
    // 解析被指向的类型
    auto pointeeResult = parseType();
    if (pointeeResult.isError()) {
        return ParseResult<TypeNode>::error();
    }
    
    SourceLocation endLoc = PrevTok.getRange().getEnd();
    SourceRange range(startLoc, endLoc);
    
    return ParseResult<TypeNode>(
        Ctx.create<PointerTypeNode>(range, pointeeResult.get(), isMut));
}

ParseResult<TypeNode> Parser::parseFunctionType() {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 'func'
    if (!expectAndConsume(TokenKind::KW_func)) {
        return ParseResult<TypeNode>::error();
    }

    // 解析可选的泛型参数 func<T, U>(...)
    if (check(TokenKind::Less)) {
        parseGenericParams();
    }
    
    // 解析参数列表 (T1, T2, ...)
    if (!expectAndConsume(TokenKind::LParen)) {
        return ParseResult<TypeNode>::error();
    }
    
    std::vector<TypeNode*> paramTypes;
    
    // 空参数列表
    if (!check(TokenKind::RParen)) {
        // 解析第一个参数类型
        auto firstParam = parseType();
        if (firstParam.isError()) {
            return ParseResult<TypeNode>::error();
        }
        paramTypes.push_back(firstParam.get());
        
        // 解析后续参数类型
        while (match(TokenKind::Comma)) {
            if (check(TokenKind::RParen)) {
                break;  // 允许尾随逗号
            }
            
            auto param = parseType();
            if (param.isError()) {
                return ParseResult<TypeNode>::error();
            }
            paramTypes.push_back(param.get());
        }
    }
    
    if (!expectAndConsume(TokenKind::RParen)) {
        return ParseResult<TypeNode>::error();
    }
    
    // 解析返回类型
    TypeNode* returnType = nullptr;
    bool canError = false;
    
    if (match(TokenKind::Arrow)) {
        // 检查是否是错误返回类型 -> !T
        if (check(TokenKind::Exclaim)) {
            canError = true;
            consume(); // 消费 '!'
        }
        
        auto returnResult = parseType();
        if (returnResult.isError()) {
            return ParseResult<TypeNode>::error();
        }
        returnType = returnResult.get();
    } else {
        // 没有返回类型，默认为 void
        SourceRange voidRange(PrevTok.getRange().getEnd(), PrevTok.getRange().getEnd());
        returnType = Ctx.create<BuiltinTypeNode>(voidRange, BuiltinTypeNode::BuiltinKind::Void);
    }
    
    SourceLocation endLoc = PrevTok.getRange().getEnd();
    SourceRange range(startLoc, endLoc);
    
    return ParseResult<TypeNode>(
        Ctx.create<FunctionTypeNode>(range, std::move(paramTypes), returnType, canError));
}

ParseResult<TypeNode> Parser::parseOptionalType() {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 '?'
    if (!expectAndConsume(TokenKind::Question)) {
        return ParseResult<TypeNode>::error();
    }
    
    // 解析内部类型
    auto innerResult = parseType();
    if (innerResult.isError()) {
        return ParseResult<TypeNode>::error();
    }
    
    SourceLocation endLoc = PrevTok.getRange().getEnd();
    SourceRange range(startLoc, endLoc);
    
    return ParseResult<TypeNode>(
        Ctx.create<OptionalTypeNode>(range, innerResult.get()));
}

ParseResult<TypeNode> Parser::parseErrorType() {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 '!'
    if (!expectAndConsume(TokenKind::Exclaim)) {
        return ParseResult<TypeNode>::error();
    }
    
    // 解析成功时的类型
    auto successResult = parseType();
    if (successResult.isError()) {
        return ParseResult<TypeNode>::error();
    }
    
    SourceLocation endLoc = PrevTok.getRange().getEnd();
    SourceRange range(startLoc, endLoc);
    
    return ParseResult<TypeNode>(
        Ctx.create<ErrorTypeNode>(range, successResult.get()));
}

ParseResult<TypeNode> Parser::parseIdentifierType() {
    // 解析标识符类型
    // Name 或 Name<T1, T2>
    Token tok = consume();
    SourceLocation startLoc = tok.getLocation();
    std::string name = std::string(tok.getText());
    
    // 解析路径访问 (e.g., std.collections.HashMap 或 std::collections::HashMap)
    while (check(TokenKind::Dot) || check(TokenKind::ColonColon)) {
        consume();  // 消费 . 或 ::
        if (!check(TokenKind::Identifier)) {
            reportError(DiagID::err_expected_identifier);
            return ParseResult<TypeNode>::error();
        }
        name += ".";
        name += std::string(CurTok.getText());
        consume();
    }
    
    // 检查是否有泛型参数
    if (check(TokenKind::Less)) {
        std::vector<TypeNode*> typeArgs = parseGenericArgs();
        SourceLocation endLoc = PrevTok.getRange().getEnd();
        SourceRange range(startLoc, endLoc);
        
        return ParseResult<TypeNode>(
            Ctx.create<GenericTypeNode>(range, name, std::move(typeArgs)));
    }
    
    SourceRange range(startLoc, tok.getRange().getEnd());
    return ParseResult<TypeNode>(
        Ctx.create<IdentifierTypeNode>(range, name));
}

} // namespace yuan
