/// \file AST.cpp
/// \brief AST 基类实现。

#include "yuan/AST/AST.h"

namespace yuan {

const char* ASTNode::getKindName(Kind kind) {
    switch (kind) {
        // 声明节点
        case Kind::VarDecl:           return "VarDecl";
        case Kind::ConstDecl:         return "ConstDecl";
        case Kind::FuncDecl:          return "FuncDecl";
        case Kind::ParamDecl:         return "ParamDecl";
        case Kind::StructDecl:        return "StructDecl";
        case Kind::FieldDecl:         return "FieldDecl";
        case Kind::EnumDecl:          return "EnumDecl";
        case Kind::EnumVariantDecl:   return "EnumVariantDecl";
        case Kind::TraitDecl:         return "TraitDecl";
        case Kind::ImplDecl:          return "ImplDecl";
        case Kind::TypeAliasDecl:     return "TypeAliasDecl";
        
        // 语句节点
        case Kind::ExprStmt:          return "ExprStmt";
        case Kind::ReturnStmt:        return "ReturnStmt";
        case Kind::IfStmt:            return "IfStmt";
        case Kind::WhileStmt:         return "WhileStmt";
        case Kind::LoopStmt:          return "LoopStmt";
        case Kind::ForStmt:           return "ForStmt";
        case Kind::BreakStmt:         return "BreakStmt";
        case Kind::ContinueStmt:      return "ContinueStmt";
        case Kind::DeferStmt:         return "DeferStmt";
        case Kind::BlockStmt:         return "BlockStmt";
        case Kind::MatchStmt:         return "MatchStmt";
        
        // 表达式节点
        case Kind::IntegerLiteralExpr:    return "IntegerLiteralExpr";
        case Kind::FloatLiteralExpr:      return "FloatLiteralExpr";
        case Kind::BoolLiteralExpr:       return "BoolLiteralExpr";
        case Kind::CharLiteralExpr:       return "CharLiteralExpr";
        case Kind::StringLiteralExpr:     return "StringLiteralExpr";
        case Kind::NoneLiteralExpr:       return "NoneLiteralExpr";
        case Kind::IdentifierExpr:        return "IdentifierExpr";
        case Kind::BinaryExpr:            return "BinaryExpr";
        case Kind::UnaryExpr:             return "UnaryExpr";
        case Kind::CallExpr:              return "CallExpr";
        case Kind::BuiltinCallExpr:       return "BuiltinCallExpr";
        case Kind::MemberExpr:            return "MemberExpr";
        case Kind::IndexExpr:             return "IndexExpr";
        case Kind::SliceExpr:             return "SliceExpr";
        case Kind::CastExpr:              return "CastExpr";
        case Kind::IfExpr:                return "IfExpr";
        case Kind::MatchExpr:             return "MatchExpr";
        case Kind::ClosureExpr:           return "ClosureExpr";
        case Kind::ArrayExpr:             return "ArrayExpr";
        case Kind::TupleExpr:             return "TupleExpr";
        case Kind::StructExpr:            return "StructExpr";
        case Kind::RangeExpr:             return "RangeExpr";
        case Kind::AssignExpr:            return "AssignExpr";
        case Kind::AwaitExpr:             return "AwaitExpr";
        case Kind::ErrorPropagateExpr:    return "ErrorPropagateExpr";
        case Kind::ErrorHandleExpr:       return "ErrorHandleExpr";
        
        // 类型节点
        case Kind::BuiltinTypeNode:       return "BuiltinTypeNode";
        case Kind::IdentifierTypeNode:    return "IdentifierTypeNode";
        case Kind::ArrayTypeNode:         return "ArrayTypeNode";
        case Kind::SliceTypeNode:         return "SliceTypeNode";
        case Kind::TupleTypeNode:         return "TupleTypeNode";
        case Kind::OptionalTypeNode:      return "OptionalTypeNode";
        case Kind::ReferenceTypeNode:     return "ReferenceTypeNode";
        case Kind::PointerTypeNode:       return "PointerTypeNode";
        case Kind::FunctionTypeNode:      return "FunctionTypeNode";
        case Kind::GenericTypeNode:       return "GenericTypeNode";
        case Kind::ErrorTypeNode:         return "ErrorTypeNode";
        
        // 模式节点
        case Kind::WildcardPattern:       return "WildcardPattern";
        case Kind::IdentifierPattern:     return "IdentifierPattern";
        case Kind::LiteralPattern:        return "LiteralPattern";
        case Kind::TuplePattern:          return "TuplePattern";
        case Kind::StructPattern:         return "StructPattern";
        case Kind::EnumPattern:           return "EnumPattern";
        case Kind::RangePattern:          return "RangePattern";
        case Kind::OrPattern:             return "OrPattern";
        case Kind::BindPattern:           return "BindPattern";
    }
    return "Unknown";
}

} // namespace yuan
