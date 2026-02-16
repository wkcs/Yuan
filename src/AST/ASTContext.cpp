/// \file ASTContext.cpp
/// \brief AST 上下文和内存管理实现。

#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Decl.h"
#include "yuan/Sema/Type.h"

namespace yuan {

ASTContext::ASTContext(SourceManager& sm)
    : SM(sm) {
    // 预留一些空间以减少重新分配
    Nodes.reserve(1024);
}

ASTContext::~ASTContext() {
    // unique_ptr 会自动清理所有分配的节点
}

// 类型工厂方法实现
VoidType* ASTContext::getVoidType() {
    if (!VoidTy) {
        VoidTy = std::make_unique<VoidType>();
    }
    return VoidTy.get();
}

BoolType* ASTContext::getBoolType() {
    if (!BoolTy) {
        BoolTy = std::make_unique<BoolType>();
    }
    return BoolTy.get();
}

CharType* ASTContext::getCharType() {
    if (!CharTy) {
        CharTy = std::make_unique<CharType>();
    }
    return CharTy.get();
}

StringType* ASTContext::getStrType() {
    if (!StrTy) {
        StrTy = std::make_unique<StringType>();
    }
    return StrTy.get();
}

ValueType* ASTContext::getValueType() {
    if (!ValueTy) {
        ValueTy = std::make_unique<ValueType>();
    }
    return ValueTy.get();
}

IntegerType* ASTContext::getIntegerType(unsigned bitWidth, bool isSigned) {
    IntegerTypeKey key{bitWidth, isSigned};
    auto it = IntegerTypes.find(key);
    if (it == IntegerTypes.end()) {
        auto type = std::make_unique<IntegerType>(bitWidth, isSigned);
        IntegerType* ptr = type.get();
        IntegerTypes[key] = std::move(type);
        return ptr;
    }
    return it->second.get();
}

FloatType* ASTContext::getFloatType(unsigned bitWidth) {
    auto it = FloatTypes.find(bitWidth);
    if (it == FloatTypes.end()) {
        auto type = std::make_unique<FloatType>(bitWidth);
        FloatType* ptr = type.get();
        FloatTypes[bitWidth] = std::move(type);
        return ptr;
    }
    return it->second.get();
}

ArrayType* ASTContext::getArrayType(Type* element, uint64_t size) {
    ArrayTypeKey key{element, size};
    auto it = ArrayTypes.find(key);
    if (it == ArrayTypes.end()) {
        auto type = std::make_unique<ArrayType>(element, size);
        ArrayType* ptr = type.get();
        ArrayTypes[key] = std::move(type);
        return ptr;
    }
    return it->second.get();
}

SliceType* ASTContext::getSliceType(Type* element, bool isMut) {
    SliceTypeKey key{element, isMut};
    auto it = SliceTypes.find(key);
    if (it == SliceTypes.end()) {
        auto type = std::make_unique<SliceType>(element, isMut);
        SliceType* ptr = type.get();
        SliceTypes[key] = std::move(type);
        return ptr;
    }
    return it->second.get();
}

RangeType* ASTContext::getRangeType(Type* elementType, bool inclusive) {
    RangeTypeKey key{elementType, inclusive};
    auto it = RangeTypes.find(key);
    if (it == RangeTypes.end()) {
        auto type = std::make_unique<RangeType>(elementType, inclusive);
        RangeType* ptr = type.get();
        RangeTypes[key] = std::move(type);
        return ptr;
    }
    return it->second.get();
}

// ============================================================================
// Display/Debug trait impl registry
// ============================================================================

void ASTContext::registerDisplayImpl(Type* type, FuncDecl* method) {
    if (!type || !method) {
        return;
    }
    DisplayImpls[type] = method;
}

void ASTContext::registerDebugImpl(Type* type, FuncDecl* method) {
    if (!type || !method) {
        return;
    }
    DebugImpls[type] = method;
}

FuncDecl* ASTContext::getDisplayImpl(Type* type) const {
    if (!type) {
        return nullptr;
    }

    auto it = DisplayImpls.find(type);
    if (it != DisplayImpls.end()) {
        return it->second;
    }

    if (type->isGenericInstance()) {
        auto* gen = static_cast<GenericInstanceType*>(type);
        Type* base = gen->getBaseType();
        it = DisplayImpls.find(base);
        if (it != DisplayImpls.end()) {
            return it->second;
        }
    }

    // Fallback: match any generic impl with the same base type.
    for (const auto& entry : DisplayImpls) {
        const Type* keyType = entry.first;
        if (!keyType || !keyType->isGenericInstance()) {
            continue;
        }
        auto* gen = static_cast<const GenericInstanceType*>(keyType);
        if (gen->getBaseType() == type) {
            return entry.second;
        }
    }

    return nullptr;
}

FuncDecl* ASTContext::getDebugImpl(Type* type) const {
    if (!type) {
        return nullptr;
    }

    auto it = DebugImpls.find(type);
    if (it != DebugImpls.end()) {
        return it->second;
    }

    if (type->isGenericInstance()) {
        auto* gen = static_cast<GenericInstanceType*>(type);
        Type* base = gen->getBaseType();
        it = DebugImpls.find(base);
        if (it != DebugImpls.end()) {
            return it->second;
        }
    }

    for (const auto& entry : DebugImpls) {
        const Type* keyType = entry.first;
        if (!keyType || !keyType->isGenericInstance()) {
            continue;
        }
        auto* gen = static_cast<const GenericInstanceType*>(keyType);
        if (gen->getBaseType() == type) {
            return entry.second;
        }
    }

    return nullptr;
}

void ASTContext::registerImplMethod(Type* type, FuncDecl* method) {
    if (!type || !method) {
        return;
    }
    ImplMethods[type][method->getName()] = method;
}

FuncDecl* ASTContext::getImplMethod(Type* type, const std::string& name) const {
    auto it = ImplMethods.find(type);
    if (it == ImplMethods.end()) {
        // Try generic instance methods whose base type matches
        for (const auto& entry : ImplMethods) {
            const Type* keyType = entry.first;
            if (!keyType || !keyType->isGenericInstance()) {
                continue;
            }
            auto* gen = static_cast<const GenericInstanceType*>(keyType);
            if (gen->getBaseType() != type) {
                continue;
            }
            const auto& methods = entry.second;
            auto mit = methods.find(name);
            if (mit != methods.end()) {
                return mit->second;
            }
        }
        return nullptr;
    }
    const auto& methods = it->second;
    auto mit = methods.find(name);
    return mit == methods.end() ? nullptr : mit->second;
}

TupleType* ASTContext::getTupleType(std::vector<Type*> elements) {
    auto it = TupleTypes.find(elements);
    if (it == TupleTypes.end()) {
        auto type = std::make_unique<TupleType>(elements);
        TupleType* ptr = type.get();
        TupleTypes[elements] = std::move(type);
        return ptr;
    }
    return it->second.get();
}

VarArgsType* ASTContext::getVarArgsType(Type* elementType) {
    if (!elementType) {
        return nullptr;
    }
    auto it = VarArgsTypes.find(elementType);
    if (it == VarArgsTypes.end()) {
        auto type = std::make_unique<VarArgsType>(elementType);
        VarArgsType* ptr = type.get();
        VarArgsTypes[elementType] = std::move(type);
        return ptr;
    }
    return it->second.get();
}

OptionalType* ASTContext::getOptionalType(Type* inner) {
    auto it = OptionalTypes.find(inner);
    if (it == OptionalTypes.end()) {
        auto type = std::make_unique<OptionalType>(inner);
        OptionalType* ptr = type.get();
        OptionalTypes[inner] = std::move(type);
        return ptr;
    }
    return it->second.get();
}

ReferenceType* ASTContext::getReferenceType(Type* pointee, bool isMut) {
    RefPtrTypeKey key{pointee, isMut};
    auto it = ReferenceTypes.find(key);
    if (it == ReferenceTypes.end()) {
        auto type = std::make_unique<ReferenceType>(pointee, isMut);
        ReferenceType* ptr = type.get();
        ReferenceTypes[key] = std::move(type);
        return ptr;
    }
    return it->second.get();
}

PointerType* ASTContext::getPointerType(Type* pointee, bool isMut) {
    RefPtrTypeKey key{pointee, isMut};
    auto it = PointerTypes.find(key);
    if (it == PointerTypes.end()) {
        auto type = std::make_unique<PointerType>(pointee, isMut);
        PointerType* ptr = type.get();
        PointerTypes[key] = std::move(type);
        return ptr;
    }
    return it->second.get();
}

FunctionType* ASTContext::getFunctionType(std::vector<Type*> params, Type* returnType, bool canError, bool isVariadic) {
    FunctionTypeKey key{params, returnType, canError, isVariadic};
    auto it = FunctionTypes.find(key);
    if (it == FunctionTypes.end()) {
        auto type = std::make_unique<FunctionType>(params, returnType, canError, isVariadic);
        FunctionType* ptr = type.get();
        FunctionTypes[key] = std::move(type);
        return ptr;
    }
    return it->second.get();
}

StructType* ASTContext::getStructType(std::string name, std::vector<Type*> fieldTypes, std::vector<std::string> fieldNames) {
    auto it = StructTypes.find(name);
    if (it == StructTypes.end()) {
        // 构建字段列表
        std::vector<StructType::Field> fields;
        for (size_t i = 0; i < fieldTypes.size() && i < fieldNames.size(); ++i) {
            fields.emplace_back(fieldNames[i], fieldTypes[i], 0); // 偏移将在构造函数中计算
        }
        
        auto type = std::make_unique<StructType>(name, std::move(fields));
        StructType* ptr = type.get();
        StructTypes[name] = std::move(type);
        return ptr;
    }
    if (!fieldTypes.empty() && !fieldNames.empty()) {
        std::vector<StructType::Field> fields;
        for (size_t i = 0; i < fieldTypes.size() && i < fieldNames.size(); ++i) {
            fields.emplace_back(fieldNames[i], fieldTypes[i], 0);
        }
        it->second->populateFieldsIfEmpty(std::move(fields));
    }
    return it->second.get();
}

EnumType* ASTContext::getEnumType(std::string name, std::vector<Type*> variantDataTypes, std::vector<std::string> variantNames) {
    auto it = EnumTypes.find(name);
    if (it == EnumTypes.end()) {
        // 构建变体列表
        std::vector<EnumType::Variant> variants;
        for (size_t i = 0; i < variantNames.size(); ++i) {
            std::vector<Type*> data;
            if (i < variantDataTypes.size() && variantDataTypes[i] != nullptr) {
                data.push_back(variantDataTypes[i]);
            }
            variants.emplace_back(variantNames[i], std::move(data), i);
        }
        
        auto type = std::make_unique<EnumType>(name, std::move(variants));
        EnumType* ptr = type.get();
        EnumTypes[name] = std::move(type);
        return ptr;
    }
    if (!variantNames.empty()) {
        std::vector<EnumType::Variant> variants;
        for (size_t i = 0; i < variantNames.size(); ++i) {
            std::vector<Type*> data;
            if (i < variantDataTypes.size() && variantDataTypes[i] != nullptr) {
                data.push_back(variantDataTypes[i]);
            }
            variants.emplace_back(variantNames[i], std::move(data), i);
        }
        it->second->populateVariantsIfEmpty(std::move(variants));
    }
    return it->second.get();
}

TraitType* ASTContext::getTraitType(std::string name) {
    auto it = TraitTypes.find(name);
    if (it == TraitTypes.end()) {
        auto type = std::make_unique<TraitType>(name);
        TraitType* ptr = type.get();
        TraitTypes[name] = std::move(type);
        return ptr;
    }
    return it->second.get();
}

GenericType* ASTContext::getGenericType(std::string name, std::vector<TraitType*> constraints) {
    auto it = GenericTypes.find(name);
    if (it == GenericTypes.end()) {
        auto type = std::make_unique<GenericType>(name, std::move(constraints));
        GenericType* ptr = type.get();
        GenericTypes[name] = std::move(type);
        return ptr;
    }
    return it->second.get();
}

GenericInstanceType* ASTContext::getGenericInstanceType(Type* baseType, std::vector<Type*> typeArgs) {
    GenericInstanceTypeKey key{baseType, typeArgs};
    auto it = GenericInstanceTypes.find(key);
    if (it == GenericInstanceTypes.end()) {
        auto type = std::make_unique<GenericInstanceType>(baseType, std::move(typeArgs));
        GenericInstanceType* ptr = type.get();
        GenericInstanceTypes[key] = std::move(type);
        return ptr;
    }
    return it->second.get();
}

TypeVariable* ASTContext::getTypeVariable(size_t id) {
    auto it = TypeVariables.find(id);
    if (it == TypeVariables.end()) {
        auto type = std::make_unique<TypeVariable>(id);
        TypeVariable* ptr = type.get();
        TypeVariables[id] = std::move(type);
        return ptr;
    }
    return it->second.get();
}

TypeVariable* ASTContext::createTypeVariable() {
    size_t id = NextTypeVarID++;
    return getTypeVariable(id);
}

ErrorType* ASTContext::getErrorType(Type* successType) {
    auto it = ErrorTypes.find(successType);
    if (it == ErrorTypes.end()) {
        auto type = std::make_unique<ErrorType>(successType);
        ErrorType* ptr = type.get();
        ErrorTypes[successType] = std::move(type);
        return ptr;
    }
    return it->second.get();
}

TypeAlias* ASTContext::getTypeAlias(std::string name, Type* aliasedType) {
    auto it = TypeAliases.find(name);
    if (it == TypeAliases.end()) {
        auto type = std::make_unique<TypeAlias>(name, aliasedType);
        TypeAlias* ptr = type.get();
        TypeAliases[name] = std::move(type);
        return ptr;
    }
    return it->second.get();
}

ModuleType* ASTContext::getModuleType(std::string name, std::vector<ModuleType::Member> members) {
    // 每次都创建新的模块类型实例（模块是唯一的）
    auto type = std::make_unique<ModuleType>(std::move(name), std::move(members));
    ModuleType* ptr = type.get();
    Types.push_back(std::move(type));
    return ptr;
}

} // namespace yuan
