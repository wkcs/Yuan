/// \file Decl.cpp
/// \brief 声明 AST 节点实现。

#include "yuan/AST/Decl.h"

namespace yuan {

//===----------------------------------------------------------------------===//
// VarDecl 实现
//===----------------------------------------------------------------------===//

VarDecl::VarDecl(SourceRange range,
                 const std::string& name,
                 TypeNode* type,
                 Expr* init,
                 bool isMutable,
                 Visibility vis,
                 Pattern* pattern)
    : Decl(Kind::VarDecl, range),
      Name(name),
      Type(type),
      Init(init),
      IsMutable(isMutable),
      Vis(vis),
      PatternNode(pattern) {}

//===----------------------------------------------------------------------===//
// ConstDecl 实现
//===----------------------------------------------------------------------===//

ConstDecl::ConstDecl(SourceRange range,
                     const std::string& name,
                     TypeNode* type,
                     Expr* init,
                     Visibility vis)
    : Decl(Kind::ConstDecl, range),
      Name(name),
      Type(type),
      Init(init),
      Vis(vis) {}

//===----------------------------------------------------------------------===//
// ParamDecl 实现
//===----------------------------------------------------------------------===//

ParamDecl::ParamDecl(SourceRange range,
                     const std::string& name,
                     TypeNode* type,
                     bool isMutable)
    : ParamDecl(range, name, type, nullptr, isMutable) {}

ParamDecl::ParamDecl(SourceRange range,
                     const std::string& name,
                     TypeNode* type,
                     Expr* defaultValue,
                     bool isMutable)
    : Decl(Kind::ParamDecl, range),
      Name(name),
      Type(type),
      DefaultValue(defaultValue),
      IsMutable(isMutable),
      PK(ParamKind::Normal) {}

ParamDecl::ParamDecl(SourceRange range, ParamKind kind)
    : Decl(Kind::ParamDecl, range),
      Name("self"),
      Type(nullptr),
      DefaultValue(nullptr),
      IsMutable(kind == ParamKind::MutRefSelf),
      PK(kind) {}

ParamDecl* ParamDecl::createSelf(SourceRange range, ParamKind kind) {
    return new ParamDecl(range, kind);
}

ParamDecl* ParamDecl::createVariadic(SourceRange range,
                                      const std::string& name,
                                      TypeNode* elementType) {
    auto* param = new ParamDecl(range, ParamKind::Variadic);
    param->Name = name;
    param->Type = elementType;  // 可以为 nullptr,表示无类型约束
    param->IsMutable = false;
    return param;
}

//===----------------------------------------------------------------------===//
// FuncDecl 实现
//===----------------------------------------------------------------------===//

FuncDecl::FuncDecl(SourceRange range,
                   const std::string& name,
                   std::vector<ParamDecl*> params,
                   TypeNode* returnType,
                   BlockStmt* body,
                   bool isAsync,
                   bool canError,
                   Visibility vis)
    : Decl(Kind::FuncDecl, range),
      Name(name),
      Params(std::move(params)),
      ReturnType(returnType),
      Body(body),
      IsAsync(isAsync),
      CanError(canError),
      Vis(vis) {}

//===----------------------------------------------------------------------===//
// FieldDecl 实现
//===----------------------------------------------------------------------===//

FieldDecl::FieldDecl(SourceRange range,
                     const std::string& name,
                     TypeNode* type,
                     Expr* defaultValue,
                     Visibility vis)
    : Decl(Kind::FieldDecl, range),
      Name(name),
      Type(type),
      DefaultValue(defaultValue),
      Vis(vis) {}

//===----------------------------------------------------------------------===//
// StructDecl 实现
//===----------------------------------------------------------------------===//

StructDecl::StructDecl(SourceRange range,
                       const std::string& name,
                       std::vector<FieldDecl*> fields,
                       Visibility vis)
    : Decl(Kind::StructDecl, range),
      Name(name),
      Fields(std::move(fields)),
      Vis(vis) {}

FieldDecl* StructDecl::findField(const std::string& name) const {
    for (auto* field : Fields) {
        if (field->getName() == name) {
            return field;
        }
    }
    return nullptr;
}


//===----------------------------------------------------------------------===//
// EnumVariantDecl 实现
//===----------------------------------------------------------------------===//

EnumVariantDecl::EnumVariantDecl(SourceRange range,
                                 const std::string& name,
                                 VariantKind kind)
    : Decl(Kind::EnumVariantDecl, range),
      Name(name),
      VK(kind) {}

EnumVariantDecl* EnumVariantDecl::createUnit(SourceRange range,
                                             const std::string& name) {
    return new EnumVariantDecl(range, name, VariantKind::Unit);
}

EnumVariantDecl* EnumVariantDecl::createTuple(SourceRange range,
                                              const std::string& name,
                                              std::vector<TypeNode*> types) {
    auto* variant = new EnumVariantDecl(range, name, VariantKind::Tuple);
    variant->TupleTypes = std::move(types);
    return variant;
}

EnumVariantDecl* EnumVariantDecl::createStruct(SourceRange range,
                                               const std::string& name,
                                               std::vector<FieldDecl*> fields) {
    auto* variant = new EnumVariantDecl(range, name, VariantKind::Struct);
    variant->Fields = std::move(fields);
    return variant;
}

//===----------------------------------------------------------------------===//
// EnumDecl 实现
//===----------------------------------------------------------------------===//

EnumDecl::EnumDecl(SourceRange range,
                   const std::string& name,
                   std::vector<EnumVariantDecl*> variants,
                   Visibility vis)
    : Decl(Kind::EnumDecl, range),
      Name(name),
      Variants(std::move(variants)),
      Vis(vis) {}

EnumVariantDecl* EnumDecl::findVariant(const std::string& name) const {
    for (auto* variant : Variants) {
        if (variant->getName() == name) {
            return variant;
        }
    }
    return nullptr;
}

//===----------------------------------------------------------------------===//
// TypeAliasDecl 实现
//===----------------------------------------------------------------------===//

TypeAliasDecl::TypeAliasDecl(SourceRange range,
                             const std::string& name,
                             TypeNode* aliasedType,
                             Visibility vis)
    : Decl(Kind::TypeAliasDecl, range),
      Name(name),
      AliasedType(aliasedType),
      Vis(vis) {}

//===----------------------------------------------------------------------===//
// TraitDecl 实现
//===----------------------------------------------------------------------===//

TraitDecl::TraitDecl(SourceRange range,
                     const std::string& name,
                     std::vector<FuncDecl*> methods,
                     std::vector<TypeAliasDecl*> associatedTypes,
                     Visibility vis)
    : Decl(Kind::TraitDecl, range),
      Name(name),
      Methods(std::move(methods)),
      AssociatedTypes(std::move(associatedTypes)),
      Vis(vis) {}

FuncDecl* TraitDecl::findMethod(const std::string& name) const {
    for (auto* method : Methods) {
        if (method->getName() == name) {
            return method;
        }
    }
    return nullptr;
}

TypeAliasDecl* TraitDecl::findAssociatedType(const std::string& name) const {
    for (auto* type : AssociatedTypes) {
        if (type->getName() == name) {
            return type;
        }
    }
    return nullptr;
}

//===----------------------------------------------------------------------===//
// ImplDecl 实现
//===----------------------------------------------------------------------===//

ImplDecl::ImplDecl(SourceRange range,
                   TypeNode* targetType,
                   const std::string& traitName,
                   TypeNode* traitRefType,
                   std::vector<FuncDecl*> methods)
    : Decl(Kind::ImplDecl, range),
      TargetType(targetType),
      TraitName(traitName),
      TraitRefType(traitRefType),
      Methods(std::move(methods)) {}

FuncDecl* ImplDecl::findMethod(const std::string& name) const {
    for (auto* method : Methods) {
        if (method->getName() == name) {
            return method;
        }
    }
    return nullptr;
}

} // namespace yuan
