/// \file Pattern.cpp
/// \brief 模式 AST 节点实现。

#include "yuan/AST/Pattern.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Type.h"

namespace yuan {

// ============================================================================
// WildcardPattern 实现
// ============================================================================

WildcardPattern::WildcardPattern(SourceRange range)
    : Pattern(Kind::WildcardPattern, range) {}


// ============================================================================
// IdentifierPattern 实现
// ============================================================================

IdentifierPattern::IdentifierPattern(SourceRange range,
                                     const std::string& name,
                                     bool isMutable,
                                     TypeNode* type)
    : Pattern(Kind::IdentifierPattern, range)
    , Name(name)
    , IsMutable(isMutable)
    , Type(type)
    , DeclNode(nullptr) {}


// ============================================================================
// LiteralPattern 实现
// ============================================================================

LiteralPattern::LiteralPattern(SourceRange range, Expr* literal)
    : Pattern(Kind::LiteralPattern, range)
    , Literal(literal) {}


// ============================================================================
// TuplePattern 实现
// ============================================================================

TuplePattern::TuplePattern(SourceRange range, std::vector<Pattern*> elements)
    : Pattern(Kind::TuplePattern, range)
    , Elements(std::move(elements)) {}


// ============================================================================
// StructPattern 实现
// ============================================================================

StructPattern::StructPattern(SourceRange range,
                             const std::string& typeName,
                             std::vector<StructPatternField> fields,
                             bool hasRest)
    : Pattern(Kind::StructPattern, range)
    , TypeName(typeName)
    , Fields(std::move(fields))
    , HasRest(hasRest) {}


// ============================================================================
// EnumPattern 实现
// ============================================================================

EnumPattern::EnumPattern(SourceRange range,
                         const std::string& enumName,
                         const std::string& variantName,
                         std::vector<Pattern*> payload)
    : Pattern(Kind::EnumPattern, range)
    , EnumName(enumName)
    , VariantName(variantName)
    , Payload(std::move(payload)) {}


// ============================================================================
// RangePattern 实现
// ============================================================================

RangePattern::RangePattern(SourceRange range,
                           Expr* start,
                           Expr* end,
                           bool isInclusive)
    : Pattern(Kind::RangePattern, range)
    , Start(start)
    , End(end)
    , IsInclusive(isInclusive) {}

// ============================================================================
// OrPattern 实现
// ============================================================================

OrPattern::OrPattern(SourceRange range, std::vector<Pattern*> patterns)
    : Pattern(Kind::OrPattern, range)
    , Patterns(std::move(patterns)) {}

// ============================================================================
// BindPattern 实现
// ============================================================================

BindPattern::BindPattern(SourceRange range,
                         const std::string& name,
                         Pattern* inner,
                         bool isMutable,
                         TypeNode* type)
    : Pattern(Kind::BindPattern, range)
    , Name(name)
    , Inner(inner)
    , IsMutable(isMutable)
    , Type(type)
    , DeclNode(nullptr) {}

} // namespace yuan
