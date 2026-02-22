/// \file ParseDecl.cpp
/// \brief 声明解析实现。
///
/// 本文件实现了 Parser 类中与声明解析相关的方法，
/// 包括变量、常量、函数、结构体、枚举、Trait 和 Impl 的解析。

#include "yuan/Parser/Parser.h"
#include "yuan/Lexer/Lexer.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/Stmt.h"
#include "yuan/AST/Type.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Pattern.h"
#include <algorithm>

namespace yuan {

// ============================================================================
// 变量和常量声明解析
// ============================================================================

/// \brief 解析变量声明
/// 语法: var name [: Type] [= init]
ParseResult<Decl> Parser::parseVarDecl(Visibility vis) {
    std::string docComment = CurTok.getDocComment();
    SourceLocation startLoc = CurTok.getLocation();
    
    // 暂时跳过 var 关键字检查
    if (check(TokenKind::KW_var)) {
        advance(); // 手动消费 var
    }
    
    // var 声明的变量默认是可变的
    bool isMutable = true;
    
    // 解析变量模式（支持解构）
    auto patternResult = parsePattern();
    if (patternResult.isError()) {
        reportError(DiagID::err_expected_identifier);
        synchronizeToStatement();
        return ParseResult<Decl>::error();
    }

    Pattern* pattern = patternResult.get();
    std::string name;
    TypeNode* type = nullptr;

    if (pattern && pattern->getKind() == ASTNode::Kind::IdentifierPattern) {
        auto* identPat = static_cast<IdentifierPattern*>(pattern);
        name = identPat->getName();
        if (identPat->hasType()) {
            type = identPat->getType();
        }
    } else {
        // 解构模式暂时降级为匿名变量名
        name = "_pattern$" + std::to_string(startLoc.getOffset());
    }

    // 解析可选的类型注解（用于非标识符模式）
    if (match(TokenKind::Colon)) {
        auto typeResult = parseType();
        if (typeResult.isError()) {
            return ParseResult<Decl>::error();
        }
        type = typeResult.get();
    }
    
    // 解析可选的初始化表达式
    Expr* init = nullptr;
    if (match(TokenKind::Equal)) {
        auto initResult = parseExpr();
        if (initResult.isError()) {
            synchronizeToStatement();
            return ParseResult<Decl>::error();
        }
        init = initResult.get();
    }
    
    // 如果没有类型注解也没有初始化表达式，报错
    if (type == nullptr && init == nullptr) {
        reportError(DiagID::err_expected_type);
        synchronizeToStatement();
        return ParseResult<Decl>::error();
    }
    
    SourceLocation endLoc = PrevTok.getRange().getEnd();
    SourceRange range(startLoc, endLoc);
    
    auto* decl = Ctx.create<VarDecl>(range, name, type, init, isMutable, vis, pattern);
    if (!docComment.empty()) {
        decl->setDocComment(docComment);
    }
    return ParseResult<Decl>(decl);
}

/// \brief 解析常量声明
/// 语法: const name [: Type] = init
ParseResult<Decl> Parser::parseConstDecl(Visibility vis) {
    std::string docComment = CurTok.getDocComment();
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 'const' 关键字
    if (!expectAndConsume(TokenKind::KW_const)) {
        synchronizeToStatement();
        return ParseResult<Decl>::error();
    }
    
    // 解析常量名
    if (!check(TokenKind::Identifier)) {
        reportError(DiagID::err_expected_identifier);
        synchronizeToStatement();
        return ParseResult<Decl>::error();
    }
    
    std::string name = std::string(CurTok.getText());
    consume();
    
    // 解析可选的类型注解
    TypeNode* type = nullptr;
    if (match(TokenKind::Colon)) {
        auto typeResult = parseType();
        if (typeResult.isError()) {
            // 尝试恢复到 = 或语句结束
            skipUntil({TokenKind::Equal, TokenKind::Semicolon, TokenKind::RBrace});
            if (!check(TokenKind::Equal)) {
                synchronizeToStatement();
                return ParseResult<Decl>::error();
            }
        } else {
            type = typeResult.get();
        }
    }
    
    // 常量必须有初始化表达式
    if (!expectAndConsume(TokenKind::Equal)) {
        synchronizeToStatement();
        return ParseResult<Decl>::error();
    }
    
    auto initResult = parseExpr();
    if (initResult.isError()) {
        synchronizeToStatement();
        return ParseResult<Decl>::error();
    }
    
    SourceLocation endLoc = PrevTok.getRange().getEnd();
    SourceRange range(startLoc, endLoc);
    
    auto* decl = Ctx.create<ConstDecl>(range, name, type, initResult.get(), vis);
    if (!docComment.empty()) {
        decl->setDocComment(docComment);
    }
    return ParseResult<Decl>(decl);
}

// ============================================================================
// 函数声明解析
// ============================================================================

/// \brief 解析函数声明
/// 语法: [async] func name [<GenericParams>] (params) [-> RetType] { body }
ParseResult<Decl> Parser::parseFuncDecl(Visibility vis) {
    std::string docComment = CurTok.getDocComment();
    SourceLocation startLoc = CurTok.getLocation();
    
    // 检查是否有 'async' 修饰符
    bool isAsync = match(TokenKind::KW_async);
    
    // 消费 'func' 关键字
    if (!expectAndConsume(TokenKind::KW_func)) {
        return ParseResult<Decl>::error();
    }
    
    // 解析函数名
    if (!check(TokenKind::Identifier)) {
        reportError(DiagID::err_expected_identifier);
        return ParseResult<Decl>::error();
    }
    
    std::string name = std::string(CurTok.getText());
    consume();
    
    // 解析可选的泛型参数
    std::vector<GenericParam> genericParams;
    if (check(TokenKind::Less)) {
        genericParams = parseGenericParams();
    }
    
    // 解析参数列表
    if (!expectAndConsume(TokenKind::LParen)) {
        return ParseResult<Decl>::error();
    }
    
    std::vector<ParamDecl*> params = parseParamList();
    
    if (!expectAndConsume(TokenKind::RParen)) {
        return ParseResult<Decl>::error();
    }
    
    // 解析可选的返回类型
    TypeNode* returnType = nullptr;
    bool canError = false;
    
    if (match(TokenKind::Arrow)) {
        // 检查是否为错误返回类型 !T
        if (check(TokenKind::Exclaim)) {
            canError = true;
            consume();
        }
        
        auto typeResult = parseType();
        if (typeResult.isError()) {
            return ParseResult<Decl>::error();
        }
        returnType = typeResult.get();
    }
    
    // 解析函数体（可选，如果没有则是声明）
    BlockStmt* body = nullptr;
    if (check(TokenKind::LBrace)) {
        auto bodyResult = parseBlockStmt();
        if (bodyResult.isError()) {
            return ParseResult<Decl>::error();
        }
        body = static_cast<BlockStmt*>(bodyResult.get());
    }
    
    SourceLocation endLoc = PrevTok.getRange().getEnd();
    SourceRange range(startLoc, endLoc);
    
    auto* funcDecl = Ctx.create<FuncDecl>(
        range, name, std::move(params), returnType, body,
        isAsync, canError, vis);
    
    if (!genericParams.empty()) {
        funcDecl->setGenericParams(std::move(genericParams));
    }

    if (!docComment.empty()) {
        funcDecl->setDocComment(docComment);
    }
    
    return ParseResult<Decl>(funcDecl);
}

/// \brief 解析参数列表
std::vector<ParamDecl*> Parser::parseParamList() {
    std::vector<ParamDecl*> params;

    // 空参数列表
    if (check(TokenKind::RParen)) {
        return params;
    }

    // 解析第一个参数
    auto firstParam = parseParam();
    if (firstParam.isSuccess()) {
        params.push_back(firstParam.get());

        // 检查第一个参数不能是可变参数（除非它是唯一的参数）
        if (firstParam.get()->isVariadic() && !check(TokenKind::RParen)) {
            reportError(DiagID::err_variadic_param_must_be_last);
            return params;
        }
    } else {
        return params;
    }

    bool seenDefaultParam = firstParam.get()->hasDefaultValue();

    // 解析后续参数
    while (match(TokenKind::Comma)) {
        if (check(TokenKind::RParen)) {
            break;  // 允许尾随逗号
        }

        // 检查前一个参数是否为可变参数
        if (!params.empty() && params.back()->isVariadic()) {
            reportError(DiagID::err_variadic_param_must_be_last);
            break;
        }

        auto param = parseParam();
        if (param.isSuccess()) {
            if (seenDefaultParam && !param.get()->hasDefaultValue()) {
                // 默认参数之后不允许出现非默认参数
                reportError(DiagID::err_expected_type);
            }
            if (param.get()->hasDefaultValue()) {
                seenDefaultParam = true;
            }
            params.push_back(param.get());
        } else {
            break;
        }
    }

    return params;
}

/// \brief 解析单个参数
/// 语法: [mut] name: Type | self | &self | &mut self | ...args[: Type]
ParseResult<ParamDecl> Parser::parseParam() {
    SourceLocation startLoc = CurTok.getLocation();

    // 检查可变参数 ...name[: Type]
    if (match(TokenKind::Ellipsis)) {
        if (!check(TokenKind::Identifier)) {
            reportError(DiagID::err_expected_identifier);
            return ParseResult<ParamDecl>::error();
        }

        std::string name = std::string(CurTok.getText());
        consume();

        // 解析可选的元素类型约束
        TypeNode* elementType = nullptr;
        if (match(TokenKind::Colon)) {
            auto typeResult = parseType();
            if (typeResult.isError()) {
                return ParseResult<ParamDecl>::error();
            }
            elementType = typeResult.get();
        }

        SourceLocation endLoc = PrevTok.getRange().getEnd();
        SourceRange range(startLoc, endLoc);

        return ParseResult<ParamDecl>(
            ParamDecl::createVariadic(range, name, elementType));
    }

    // 检查 &self 或 &mut self 或 &param（必须在 self 之前检查）
    bool isReference = false;
    if (check(TokenKind::Amp)) {
        consume();
        isReference = true;

        if (match(TokenKind::KW_mut)) {
            if (check(TokenKind::KW_self)) {
                consume();
                SourceRange range(startLoc, PrevTok.getRange().getEnd());
                return ParseResult<ParamDecl>(
                    ParamDecl::createSelf(range, ParamDecl::ParamKind::MutRefSelf));
            }
            // 否则是 &mut param，继续解析
        } else if (check(TokenKind::KW_self)) {
            consume();
            SourceRange range(startLoc, PrevTok.getRange().getEnd());
            return ParseResult<ParamDecl>(
                ParamDecl::createSelf(range, ParamDecl::ParamKind::RefSelf));
        }
        // 否则是 &param，继续解析普通参数
    }

    // 检查 self 参数（不带类型注解）
    if (check(TokenKind::KW_self)) {
        // 向前看，检查是否有 `: Type`
        Token next = peekAhead(1);
        if (next.isNot(TokenKind::Colon)) {
            // 纯 self 参数
            consume();
            SourceRange range(startLoc, PrevTok.getRange().getEnd());
            return ParseResult<ParamDecl>(
                ParamDecl::createSelf(range, ParamDecl::ParamKind::Self));
        }
        // 否则，self: Type 形式，作为普通参数处理
    }

    // 检查是否有 'mut' 修饰符
    bool isMutable = match(TokenKind::KW_mut);

    // 解析参数名
    if (!check(TokenKind::Identifier) && !check(TokenKind::KW_self)) {
        reportError(DiagID::err_expected_identifier);
        return ParseResult<ParamDecl>::error();
    }

    std::string name = std::string(CurTok.getText());
    consume();

    // 解析可选的类型注解
    TypeNode* type = nullptr;
    if (match(TokenKind::Colon)) {
        auto typeResult = parseType();
        if (typeResult.isError()) {
            return ParseResult<ParamDecl>::error();
        }
        type = typeResult.get();
    } else if (isReference) {
        // 如果有 & 但没有类型注解，创建一个推断的引用类型（nullptr 表示类型推断）
        // 在闭包参数中允许类型推断
        type = nullptr;  // 类型推断
    }

    Expr* defaultValue = nullptr;
    if (match(TokenKind::Equal)) {
        auto defaultResult = parseExpr();
        if (defaultResult.isError()) {
            return ParseResult<ParamDecl>::error();
        }
        defaultValue = defaultResult.get();
    }

    SourceLocation endLoc = PrevTok.getRange().getEnd();
    SourceRange range(startLoc, endLoc);

    // 注意：isReference 信息目前没有传递给 ParamDecl
    // 在闭包中，&x 会被处理为类型推断的引用参数
    return ParseResult<ParamDecl>(
        Ctx.create<ParamDecl>(range, name, type, defaultValue, isMutable));
}

/// \brief 解析泛型参数列表
/// 语法: <T, U: Trait, V: Trait1 + Trait2>
std::vector<GenericParam> Parser::parseGenericParams() {
    std::vector<GenericParam> params;
    
    if (!expectAndConsume(TokenKind::Less)) {
        return params;
    }
    
    // 空泛型参数列表
    if (check(TokenKind::Greater)) {
        consume();
        return params;
    }
    
    // 解析第一个泛型参数
    if (check(TokenKind::Identifier)) {
        SourceLocation loc = CurTok.getLocation();
        std::string name = std::string(CurTok.getText());
        consume();
        
        std::vector<std::string> bounds;
        
        // 解析可选的 Trait 约束
        if (match(TokenKind::Colon)) {
            // 解析第一个约束
            if (check(TokenKind::Identifier)) {
                bounds.push_back(std::string(CurTok.getText()));
                consume();
            }
            
            // 解析后续约束 (+ Trait)
            while (match(TokenKind::Plus)) {
                if (check(TokenKind::Identifier)) {
                    bounds.push_back(std::string(CurTok.getText()));
                    consume();
                }
            }
        }
        
        params.emplace_back(name, bounds, loc);
    }
    
    // 解析后续泛型参数
    while (match(TokenKind::Comma)) {
        if (check(TokenKind::Greater)) {
            break;  // 允许尾随逗号
        }
        
        if (check(TokenKind::Identifier)) {
            SourceLocation loc = CurTok.getLocation();
            std::string name = std::string(CurTok.getText());
            consume();
            
            std::vector<std::string> bounds;
            
            if (match(TokenKind::Colon)) {
                if (check(TokenKind::Identifier)) {
                    bounds.push_back(std::string(CurTok.getText()));
                    consume();
                }
                
                while (match(TokenKind::Plus)) {
                    if (check(TokenKind::Identifier)) {
                        bounds.push_back(std::string(CurTok.getText()));
                        consume();
                    }
                }
            }
            
            params.emplace_back(name, bounds, loc);
        }
    }
    
    expectAndConsume(TokenKind::Greater);
    
    return params;
}

/// \brief 解析泛型参数（类型参数）
/// 语法: <T1, T2, ...>
/// 注意：需要特殊处理 >> 被词法分析器解析为单个 token 的情况
std::vector<TypeNode*> Parser::parseGenericArgs() {
    std::vector<TypeNode*> args;

    if (!expectAndConsume(TokenKind::Less)) {
        return args;
    }

    auto parseArg = [&]() -> TypeNode* {
        // 支持关联类型约束：Item = i32
        if (check(TokenKind::Identifier) && peekAhead(1).is(TokenKind::Equal)) {
            consume(); // 消费关联类型名
            consume(); // 消费 '='
            auto rhsResult = parseType();
            if (rhsResult.isSuccess()) {
                return rhsResult.get();
            }
            return nullptr;
        }

        auto typeResult = parseType();
        if (typeResult.isSuccess()) {
            return typeResult.get();
        }
        return nullptr;
    };

    // 空泛型参数列表
    if (check(TokenKind::Greater)) {
        consume();
        return args;
    }

    // 解析第一个类型参数
    if (TypeNode* firstType = parseArg()) {
        args.push_back(firstType);
    } else {
        return args;
    }

    // 解析后续类型参数
    while (match(TokenKind::Comma)) {
        if (check(TokenKind::Greater) || check(TokenKind::GreaterGreater)) {
            break;  // 允许尾随逗号
        }

        if (TypeNode* type = parseArg()) {
            args.push_back(type);
        } else {
            break;
        }
    }

    // 处理 > 或 >> (嵌套泛型的情况)
    if (check(TokenKind::Greater)) {
        consume();
    } else if (check(TokenKind::GreaterGreater)) {
        // >> 需要拆分：消费一个 > 并保留另一个
        // 通过将 >> token 替换为单个 > token 来实现
        // 这里我们设置一个标志让词法分析器下次返回 >
        Lex.splitGreaterGreater();
        consume(); // 消费第一个 >，剩余的 > 作为下一个 token
    } else {
        reportExpectedError(TokenKind::Greater);
    }

    return args;
}

/// \brief 解析 where 子句
/// 语法: where T: Trait1 + Trait2, U: Trait3
std::vector<std::pair<std::string, std::vector<std::string>>> Parser::parseWhereClause() {
    std::vector<std::pair<std::string, std::vector<std::string>>> constraints;

    if (!match(TokenKind::KW_where)) {
        return constraints;
    }

    // 解析第一个约束 (支持 Self 关键字)
    if (!check(TokenKind::Identifier) && !check(TokenKind::KW_Self)) {
        reportError(DiagID::err_expected_identifier);
        return constraints;
    }

    std::string typeName = std::string(CurTok.getText());
    consume();

    if (!expectAndConsume(TokenKind::Colon)) {
        return constraints;
    }

    std::vector<std::string> bounds;
    if (check(TokenKind::Identifier)) {
        bounds.push_back(std::string(CurTok.getText()));
        consume();
    }

    while (match(TokenKind::Plus)) {
        if (check(TokenKind::Identifier)) {
            bounds.push_back(std::string(CurTok.getText()));
            consume();
        }
    }

    constraints.emplace_back(typeName, bounds);

    // 解析后续约束
    while (match(TokenKind::Comma)) {
        if (!check(TokenKind::Identifier) && !check(TokenKind::KW_Self)) {
            break;
        }

        typeName = std::string(CurTok.getText());
        consume();

        if (!expectAndConsume(TokenKind::Colon)) {
            break;
        }

        bounds.clear();
        if (check(TokenKind::Identifier)) {
            bounds.push_back(std::string(CurTok.getText()));
            consume();
        }

        while (match(TokenKind::Plus)) {
            if (check(TokenKind::Identifier)) {
                bounds.push_back(std::string(CurTok.getText()));
                consume();
            }
        }

        constraints.emplace_back(typeName, bounds);
    }

    return constraints;
}

static void applyWhereConstraints(
    std::vector<GenericParam>& params,
    const std::vector<std::pair<std::string, std::vector<std::string>>>& constraints) {
    if (params.empty() || constraints.empty()) {
        return;
    }

    for (const auto& constraint : constraints) {
        const std::string& typeName = constraint.first;
        const auto& bounds = constraint.second;

        auto it = std::find_if(params.begin(), params.end(),
                               [&](const GenericParam& param) {
                                   return param.Name == typeName;
                               });

        if (it == params.end()) {
            continue;
        }

        for (const auto& bound : bounds) {
            if (std::find(it->Bounds.begin(), it->Bounds.end(), bound) == it->Bounds.end()) {
                it->Bounds.push_back(bound);
            }
        }
    }
}


// ============================================================================
// 结构体声明解析
// ============================================================================

/// \brief 解析结构体声明
/// 语法: struct Name [<GenericParams>] [where ...] { fields }
ParseResult<Decl> Parser::parseStructDecl(Visibility vis) {
    std::string docComment = CurTok.getDocComment();
    SourceLocation startLoc = CurTok.getLocation();

    // 消费 'struct' 关键字
    if (!expectAndConsume(TokenKind::KW_struct)) {
        return ParseResult<Decl>::error();
    }

    // 解析结构体名
    if (!check(TokenKind::Identifier)) {
        reportError(DiagID::err_expected_identifier);
        return ParseResult<Decl>::error();
    }

    std::string name = std::string(CurTok.getText());
    consume();

    // 解析可选的泛型参数
    std::vector<GenericParam> genericParams;
    if (check(TokenKind::Less)) {
        genericParams = parseGenericParams();
    }

    // 解析可选的 where 子句
    auto whereConstraints = parseWhereClause();
    applyWhereConstraints(genericParams, whereConstraints);
    if (genericParams.empty() && !whereConstraints.empty()) {
        for (const auto& constraint : whereConstraints) {
            genericParams.emplace_back(constraint.first, constraint.second, startLoc);
        }
    }
    if (genericParams.empty() && !whereConstraints.empty()) {
        for (const auto& constraint : whereConstraints) {
            genericParams.emplace_back(constraint.first, constraint.second, startLoc);
        }
    }

    // 解析字段列表
    if (!expectAndConsume(TokenKind::LBrace)) {
        return ParseResult<Decl>::error();
    }
    
    std::vector<FieldDecl*> fields;
    
    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        auto fieldResult = parseFieldDecl();
        if (fieldResult.isSuccess()) {
            fields.push_back(fieldResult.get());
        } else {
            // 错误恢复：跳到下一个字段或结构体结束
            synchronizeTo(TokenKind::RBrace);
            break;
        }
        
        // 字段之间可以用逗号或换行分隔
        match(TokenKind::Comma);
    }
    
    if (!expectAndConsume(TokenKind::RBrace)) {
        return ParseResult<Decl>::error();
    }
    
    SourceLocation endLoc = PrevTok.getRange().getEnd();
    SourceRange range(startLoc, endLoc);
    
    auto* structDecl = Ctx.create<StructDecl>(
        range, name, std::move(fields), vis);
    
    if (!genericParams.empty()) {
        structDecl->setGenericParams(std::move(genericParams));
    }

    if (!docComment.empty()) {
        structDecl->setDocComment(docComment);
    }
    
    return ParseResult<Decl>(structDecl);
}

/// \brief 解析结构体字段
/// 语法: [pub|priv] name: Type [= default]
ParseResult<FieldDecl> Parser::parseFieldDecl() {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 解析可见性修饰符
    Visibility vis = parseVisibility();
    
    // 解析字段名（允许某些关键字作为字段名，如 internal）
    if (!check(TokenKind::Identifier) &&
        !check(TokenKind::KW_internal) &&
        !check(TokenKind::KW_type)) {
        reportError(DiagID::err_expected_identifier);
        return ParseResult<FieldDecl>::error();
    }

    std::string name = std::string(CurTok.getText());
    consume();
    
    // 解析类型注解
    if (!expectAndConsume(TokenKind::Colon)) {
        return ParseResult<FieldDecl>::error();
    }
    
    auto typeResult = parseType();
    if (typeResult.isError()) {
        return ParseResult<FieldDecl>::error();
    }
    
    // 解析可选的默认值
    Expr* defaultValue = nullptr;
    if (match(TokenKind::Equal)) {
        auto defaultResult = parseExpr();
        if (defaultResult.isError()) {
            return ParseResult<FieldDecl>::error();
        }
        defaultValue = defaultResult.get();
    }
    
    SourceLocation endLoc = PrevTok.getRange().getEnd();
    SourceRange range(startLoc, endLoc);
    
    return ParseResult<FieldDecl>(
        Ctx.create<FieldDecl>(range, name, typeResult.get(), defaultValue, vis));
}

// ============================================================================
// 枚举声明解析
// ============================================================================

/// \brief 解析枚举声明
/// 语法: enum Name [<GenericParams>] { variants }
ParseResult<Decl> Parser::parseEnumDecl(Visibility vis) {
    std::string docComment = CurTok.getDocComment();
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 'enum' 关键字
    if (!expectAndConsume(TokenKind::KW_enum)) {
        return ParseResult<Decl>::error();
    }
    
    // 解析枚举名
    if (!check(TokenKind::Identifier)) {
        reportError(DiagID::err_expected_identifier);
        return ParseResult<Decl>::error();
    }
    
    std::string name = std::string(CurTok.getText());
    consume();
    
    // 解析可选的泛型参数
    std::vector<GenericParam> genericParams;
    if (check(TokenKind::Less)) {
        genericParams = parseGenericParams();
    }
    
    // 解析变体列表
    if (!expectAndConsume(TokenKind::LBrace)) {
        return ParseResult<Decl>::error();
    }
    
    std::vector<EnumVariantDecl*> variants;
    
    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        auto variantResult = parseEnumVariant();
        if (variantResult.isSuccess()) {
            variants.push_back(variantResult.get());
        } else {
            // 错误恢复：跳到下一个变体或枚举结束
            synchronizeTo(TokenKind::RBrace);
            break;
        }
        
        // 变体之间可以用逗号分隔
        match(TokenKind::Comma);
    }
    
    if (!expectAndConsume(TokenKind::RBrace)) {
        return ParseResult<Decl>::error();
    }
    
    SourceLocation endLoc = PrevTok.getRange().getEnd();
    SourceRange range(startLoc, endLoc);
    
    auto* enumDecl = Ctx.create<EnumDecl>(
        range, name, std::move(variants), vis);
    
    if (!genericParams.empty()) {
        enumDecl->setGenericParams(std::move(genericParams));
    }

    if (!docComment.empty()) {
        enumDecl->setDocComment(docComment);
    }
    
    return ParseResult<Decl>(enumDecl);
}

/// \brief 解析枚举变体
/// 语法: Name | Name(T1, T2) | Name { field1: T1, field2: T2 }
ParseResult<EnumVariantDecl> Parser::parseEnumVariant() {
    SourceLocation startLoc = CurTok.getLocation();
    
    // 解析变体名 - 可以是标识符或某些关键字
    std::string name;
    if (check(TokenKind::Identifier)) {
        name = std::string(CurTok.getText());
        consume();
    } else if (check(TokenKind::KW_None)) {
        // None 可以作为枚举变体名
        name = "None";
        consume();
    } else {
        reportError(DiagID::err_expected_identifier);
        return ParseResult<EnumVariantDecl>::error();
    }
    
    SourceLocation endLoc = PrevTok.getRange().getEnd();
    
    // 检查变体类型
    if (check(TokenKind::LParen)) {
        // 元组变体: Name(T1, T2, ...)
        consume();
        
        std::vector<TypeNode*> types;
        
        if (!check(TokenKind::RParen)) {
            auto firstType = parseType();
            if (firstType.isSuccess()) {
                types.push_back(firstType.get());
            }
            
            while (match(TokenKind::Comma)) {
                if (check(TokenKind::RParen)) {
                    break;
                }
                
                auto type = parseType();
                if (type.isSuccess()) {
                    types.push_back(type.get());
                }
            }
        }
        
        if (!expectAndConsume(TokenKind::RParen)) {
            return ParseResult<EnumVariantDecl>::error();
        }
        
        endLoc = PrevTok.getRange().getEnd();
        SourceRange range(startLoc, endLoc);
        
        return ParseResult<EnumVariantDecl>(
            EnumVariantDecl::createTuple(range, name, std::move(types)));
        
    } else if (check(TokenKind::LBrace)) {
        // 结构体变体: Name { field1: T1, field2: T2 }
        consume();
        
        std::vector<FieldDecl*> fields;
        
        while (!check(TokenKind::RBrace) && !isAtEnd()) {
            auto fieldResult = parseFieldDecl();
            if (fieldResult.isSuccess()) {
                fields.push_back(fieldResult.get());
            } else {
                break;
            }
            
            match(TokenKind::Comma);
        }
        
        if (!expectAndConsume(TokenKind::RBrace)) {
            return ParseResult<EnumVariantDecl>::error();
        }
        
        endLoc = PrevTok.getRange().getEnd();
        SourceRange range(startLoc, endLoc);
        
        return ParseResult<EnumVariantDecl>(
            EnumVariantDecl::createStruct(range, name, std::move(fields)));
        
    } else {
        // 单元变体: Name
        SourceRange range(startLoc, endLoc);
        
        return ParseResult<EnumVariantDecl>(
            EnumVariantDecl::createUnit(range, name));
    }
}


// ============================================================================
// Trait 和 Impl 声明解析
// ============================================================================

/// \brief 解析 Trait 声明
/// 语法: trait Name [<GenericParams>] [: SuperTraits] [where ...] { methods }
ParseResult<Decl> Parser::parseTraitDecl(Visibility vis) {
    std::string docComment = CurTok.getDocComment();
    SourceLocation startLoc = CurTok.getLocation();

    // 消费 'trait' 关键字
    if (!expectAndConsume(TokenKind::KW_trait)) {
        return ParseResult<Decl>::error();
    }

    // 解析 Trait 名
    if (!check(TokenKind::Identifier)) {
        reportError(DiagID::err_expected_identifier);
        return ParseResult<Decl>::error();
    }

    std::string name = std::string(CurTok.getText());
    consume();

    // 解析可选的泛型参数
    std::vector<GenericParam> genericParams;
    if (check(TokenKind::Less)) {
        genericParams = parseGenericParams();
    }

    // 解析可选的父 Trait 列表
    std::vector<std::string> superTraits;
    if (match(TokenKind::Colon)) {
        // 解析第一个父 Trait
        if (check(TokenKind::Identifier)) {
            superTraits.push_back(std::string(CurTok.getText()));
            consume();
        }

        // 解析后续父 Trait (+ Trait)
        while (match(TokenKind::Plus)) {
            if (check(TokenKind::Identifier)) {
                superTraits.push_back(std::string(CurTok.getText()));
                consume();
            }
        }
    }

    // 解析可选的 where 子句
    auto whereConstraints = parseWhereClause();
    applyWhereConstraints(genericParams, whereConstraints);
    if (genericParams.empty() && !whereConstraints.empty()) {
        for (const auto& constraint : whereConstraints) {
            genericParams.emplace_back(constraint.first, constraint.second, startLoc);
        }
    }

    // 解析方法列表
    if (!expectAndConsume(TokenKind::LBrace)) {
        return ParseResult<Decl>::error();
    }
    
    std::vector<FuncDecl*> methods;
    std::vector<TypeAliasDecl*> associatedTypes;
    
    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        // 解析可见性修饰符
        Visibility vis = parseVisibility();
        (void)vis;  // Trait 方法默认公开
        
        if (check(TokenKind::KW_type)) {
            // 关联类型
            auto typeAliasResult = parseTypeAlias();
            if (typeAliasResult.isSuccess()) {
                associatedTypes.push_back(
                    static_cast<TypeAliasDecl*>(typeAliasResult.get()));
            }
        } else if (check(TokenKind::KW_func) || check(TokenKind::KW_async)) {
            // 方法
            auto funcResult = parseFuncDecl(vis);
            if (funcResult.isSuccess()) {
                methods.push_back(static_cast<FuncDecl*>(funcResult.get()));
            }
        } else {
            reportError(DiagID::err_expected_declaration);
            synchronize();
        }
    }
    
    if (!expectAndConsume(TokenKind::RBrace)) {
        return ParseResult<Decl>::error();
    }
    
    SourceLocation endLoc = PrevTok.getRange().getEnd();
    SourceRange range(startLoc, endLoc);
    
    auto* traitDecl = Ctx.create<TraitDecl>(
        range, name, std::move(methods), std::move(associatedTypes),
        vis);
    
    if (!genericParams.empty()) {
        traitDecl->setGenericParams(std::move(genericParams));
    }
    
    if (!superTraits.empty()) {
        traitDecl->setSuperTraits(std::move(superTraits));
    }

    if (!docComment.empty()) {
        traitDecl->setDocComment(docComment);
    }
    
    return ParseResult<Decl>(traitDecl);
}

/// \brief 解析 Impl 块
/// 语法: impl [<GenericParams>] [Trait for] Type [where ...] { methods }
ParseResult<Decl> Parser::parseImplDecl() {
    std::string docComment = CurTok.getDocComment();
    SourceLocation startLoc = CurTok.getLocation();

    // 消费 'impl' 关键字
    if (!expectAndConsume(TokenKind::KW_impl)) {
        return ParseResult<Decl>::error();
    }

    // 解析可选的泛型参数
    std::vector<GenericParam> genericParams;
    if (check(TokenKind::Less)) {
        genericParams = parseGenericParams();
    }

    // 解析类型或 Trait 名
    // 需要向前看来确定是 "impl Type" 还是 "impl Trait for Type"
    std::string traitName;
    TypeNode* traitRefType = nullptr;
    std::vector<TypeNode*> traitTypeArgs;
    TypeNode* targetType = nullptr;

    auto firstTypeResult = parseType();
    if (firstTypeResult.isError()) {
        return ParseResult<Decl>::error();
    }

    if (match(TokenKind::KW_for)) {
        // impl Trait for Type
        // 第一个类型是 Trait 名
        traitRefType = firstTypeResult.get();
        if (auto* identType = dynamic_cast<IdentifierTypeNode*>(firstTypeResult.get())) {
            traitName = identType->getName();
        } else if (auto* genericType = dynamic_cast<GenericTypeNode*>(firstTypeResult.get())) {
            traitName = genericType->getBaseName();
            traitTypeArgs = genericType->getTypeArgs();
        } else {
            reportError(DiagID::err_expected_identifier);
            return ParseResult<Decl>::error();
        }

        // 解析目标类型
        auto targetTypeResult = parseType();
        if (targetTypeResult.isError()) {
            return ParseResult<Decl>::error();
        }
        targetType = targetTypeResult.get();
    } else {
        // impl Type
        targetType = firstTypeResult.get();
    }

    // 解析可选的 where 子句
    auto whereConstraints = parseWhereClause();
    applyWhereConstraints(genericParams, whereConstraints);
    if (genericParams.empty() && !whereConstraints.empty()) {
        for (const auto& constraint : whereConstraints) {
            genericParams.emplace_back(constraint.first, constraint.second, startLoc);
        }
    }

    // 解析方法列表
    if (!expectAndConsume(TokenKind::LBrace)) {
        return ParseResult<Decl>::error();
    }
    
    std::vector<FuncDecl*> methods;
    std::vector<TypeAliasDecl*> associatedTypes;
    
    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        // 解析可见性修饰符
        Visibility vis = parseVisibility();
        (void)vis;
        
        if (check(TokenKind::KW_type)) {
            // 关联类型实现
            auto typeAliasResult = parseTypeAlias();
            if (typeAliasResult.isSuccess()) {
                associatedTypes.push_back(
                    static_cast<TypeAliasDecl*>(typeAliasResult.get()));
            }
        } else if (check(TokenKind::KW_func) || check(TokenKind::KW_async)) {
            // 方法
            auto funcResult = parseFuncDecl(vis);
            if (funcResult.isSuccess()) {
                methods.push_back(static_cast<FuncDecl*>(funcResult.get()));
            }
        } else {
            reportError(DiagID::err_expected_declaration);
            synchronize();
        }
    }
    
    if (!expectAndConsume(TokenKind::RBrace)) {
        return ParseResult<Decl>::error();
    }
    
    SourceLocation endLoc = PrevTok.getRange().getEnd();
    SourceRange range(startLoc, endLoc);
    
    auto* implDecl = Ctx.create<ImplDecl>(
        range, targetType, traitName, traitRefType, std::move(methods));
    
    if (!genericParams.empty()) {
        implDecl->setGenericParams(std::move(genericParams));
    }

    if (!traitTypeArgs.empty()) {
        implDecl->setTraitTypeArgs(std::move(traitTypeArgs));
    }
    
    if (!associatedTypes.empty()) {
        implDecl->setAssociatedTypes(std::move(associatedTypes));
    }

    if (!docComment.empty()) {
        implDecl->setDocComment(docComment);
    }
    
    return ParseResult<Decl>(implDecl);
}

/// \brief 解析类型别名
/// 语法: type Name [<GenericParams>] [: Trait + Trait] [= Type]
ParseResult<Decl> Parser::parseTypeAlias(Visibility vis) {
    std::string docComment = CurTok.getDocComment();
    SourceLocation startLoc = CurTok.getLocation();
    
    // 消费 'type' 关键字
    if (!expectAndConsume(TokenKind::KW_type)) {
        return ParseResult<Decl>::error();
    }
    
    // 解析类型别名名称
    if (!check(TokenKind::Identifier)) {
        reportError(DiagID::err_expected_identifier);
        return ParseResult<Decl>::error();
    }
    
    std::string name = std::string(CurTok.getText());
    consume();
    
    // 解析可选的泛型参数
    std::vector<GenericParam> genericParams;
    if (check(TokenKind::Less)) {
        genericParams = parseGenericParams();
    }
    
    // 解析可选的 Trait 约束 (用于关联类型)
    // 语法: type Associated: Trait1 + Trait2
    std::vector<std::string> traitBounds;
    if (match(TokenKind::Colon)) {
        // 解析第一个 Trait 约束
        if (check(TokenKind::Identifier)) {
            traitBounds.push_back(std::string(CurTok.getText()));
            consume();
        } else {
            reportError(DiagID::err_expected_identifier);
            return ParseResult<Decl>::error();
        }
        
        // 解析后续 Trait 约束 (+ Trait)
        while (match(TokenKind::Plus)) {
            if (check(TokenKind::Identifier)) {
                traitBounds.push_back(std::string(CurTok.getText()));
                consume();
            } else {
                reportError(DiagID::err_expected_identifier);
                return ParseResult<Decl>::error();
            }
        }
    }
    
    // 解析可选的类型定义
    TypeNode* aliasedType = nullptr;
    if (match(TokenKind::Equal)) {
        auto typeResult = parseType();
        if (typeResult.isError()) {
            return ParseResult<Decl>::error();
        }
        aliasedType = typeResult.get();
    }
    
    SourceLocation endLoc = PrevTok.getRange().getEnd();
    SourceRange range(startLoc, endLoc);
    
    auto* typeAliasDecl = Ctx.create<TypeAliasDecl>(
        range, name, aliasedType, vis);
    
    if (!genericParams.empty()) {
        typeAliasDecl->setGenericParams(std::move(genericParams));
    }
    
    if (!traitBounds.empty()) {
        typeAliasDecl->setTraitBounds(std::move(traitBounds));
    }

    if (!docComment.empty()) {
        typeAliasDecl->setDocComment(docComment);
    }
    
    return ParseResult<Decl>(typeAliasDecl);
}


} // namespace yuan
