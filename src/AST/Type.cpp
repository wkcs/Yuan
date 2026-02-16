/// \file Type.cpp
/// \brief 类型 AST 节点实现。
///
/// 本文件实现了所有类型相关的 AST 节点。

#include "yuan/AST/Type.h"

namespace yuan {

// ============================================================================
// BuiltinTypeNode 实现
// ============================================================================

BuiltinTypeNode::BuiltinTypeNode(SourceRange range, BuiltinKind kind)
    : TypeNode(Kind::BuiltinTypeNode, range), BKind(kind) {}

const char* BuiltinTypeNode::getBuiltinKindName(BuiltinKind kind) {
    switch (kind) {
        case BuiltinKind::Void:  return "void";
        case BuiltinKind::Bool:  return "bool";
        case BuiltinKind::Char:  return "char";
        case BuiltinKind::Str:   return "str";
        case BuiltinKind::I8:    return "i8";
        case BuiltinKind::I16:   return "i16";
        case BuiltinKind::I32:   return "i32";
        case BuiltinKind::I64:   return "i64";
        case BuiltinKind::I128:  return "i128";
        case BuiltinKind::ISize: return "isize";
        case BuiltinKind::U8:    return "u8";
        case BuiltinKind::U16:   return "u16";
        case BuiltinKind::U32:   return "u32";
        case BuiltinKind::U64:   return "u64";
        case BuiltinKind::U128:  return "u128";
        case BuiltinKind::USize: return "usize";
        case BuiltinKind::F32:   return "f32";
        case BuiltinKind::F64:   return "f64";
    }
    return "unknown";
}

bool BuiltinTypeNode::isInteger() const {
    return isSignedInteger() || isUnsignedInteger();
}

bool BuiltinTypeNode::isSignedInteger() const {
    switch (BKind) {
        case BuiltinKind::I8:
        case BuiltinKind::I16:
        case BuiltinKind::I32:
        case BuiltinKind::I64:
        case BuiltinKind::I128:
        case BuiltinKind::ISize:
            return true;
        default:
            return false;
    }
}

bool BuiltinTypeNode::isUnsignedInteger() const {
    switch (BKind) {
        case BuiltinKind::U8:
        case BuiltinKind::U16:
        case BuiltinKind::U32:
        case BuiltinKind::U64:
        case BuiltinKind::U128:
        case BuiltinKind::USize:
            return true;
        default:
            return false;
    }
}

bool BuiltinTypeNode::isFloatingPoint() const {
    return BKind == BuiltinKind::F32 || BKind == BuiltinKind::F64;
}


// ============================================================================
// IdentifierTypeNode 实现
// ============================================================================

IdentifierTypeNode::IdentifierTypeNode(SourceRange range, const std::string& name)
    : TypeNode(Kind::IdentifierTypeNode, range), Name(name) {}


// ============================================================================
// ArrayTypeNode 实现
// ============================================================================

ArrayTypeNode::ArrayTypeNode(SourceRange range, TypeNode* element, Expr* size)
    : TypeNode(Kind::ArrayTypeNode, range), Element(element), Size(size) {}


// ============================================================================
// SliceTypeNode 实现
// ============================================================================

SliceTypeNode::SliceTypeNode(SourceRange range, TypeNode* element, bool isMut)
    : TypeNode(Kind::SliceTypeNode, range), Element(element), IsMut(isMut) {}


// ============================================================================
// TupleTypeNode 实现
// ============================================================================

TupleTypeNode::TupleTypeNode(SourceRange range, std::vector<TypeNode*> elements)
    : TypeNode(Kind::TupleTypeNode, range), Elements(std::move(elements)) {}


// ============================================================================
// OptionalTypeNode 实现
// ============================================================================

OptionalTypeNode::OptionalTypeNode(SourceRange range, TypeNode* inner)
    : TypeNode(Kind::OptionalTypeNode, range), Inner(inner) {}


// ============================================================================
// ReferenceTypeNode 实现
// ============================================================================

ReferenceTypeNode::ReferenceTypeNode(SourceRange range, TypeNode* pointee, bool isMut)
    : TypeNode(Kind::ReferenceTypeNode, range), Pointee(pointee), IsMut(isMut) {}


// ============================================================================
// PointerTypeNode 实现
// ============================================================================

PointerTypeNode::PointerTypeNode(SourceRange range, TypeNode* pointee, bool isMut)
    : TypeNode(Kind::PointerTypeNode, range), Pointee(pointee), IsMut(isMut) {}


// ============================================================================
// FunctionTypeNode 实现
// ============================================================================

FunctionTypeNode::FunctionTypeNode(SourceRange range,
                                   std::vector<TypeNode*> params,
                                   TypeNode* returnType,
                                   bool canError)
    : TypeNode(Kind::FunctionTypeNode, range),
      Params(std::move(params)),
      ReturnType(returnType),
      CanError(canError) {}


// ============================================================================
// ErrorTypeNode 实现
// ============================================================================

ErrorTypeNode::ErrorTypeNode(SourceRange range, TypeNode* successType)
    : TypeNode(Kind::ErrorTypeNode, range), SuccessType(successType) {}


// ============================================================================
// GenericTypeNode 实现
// ============================================================================

GenericTypeNode::GenericTypeNode(SourceRange range,
                                 const std::string& base,
                                 std::vector<TypeNode*> typeArgs)
    : TypeNode(Kind::GenericTypeNode, range),
      BaseName(base),
      TypeArgs(std::move(typeArgs)) {}

} // namespace yuan
