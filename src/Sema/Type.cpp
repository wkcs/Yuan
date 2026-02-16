/// \file
/// \brief Yuan 语义分析类型系统实现

#include "yuan/Sema/Type.h"
#include "yuan/AST/ASTContext.h"
#include <cassert>

namespace yuan {

// VoidType 实现
VoidType* VoidType::get(ASTContext& ctx) {
    return ctx.getVoidType();
}

// BoolType 实现
BoolType* BoolType::get(ASTContext& ctx) {
    return ctx.getBoolType();
}

// CharType 实现
CharType* CharType::get(ASTContext& ctx) {
    return ctx.getCharType();
}

// StringType 实现
StringType* StringType::get(ASTContext& ctx) {
    return ctx.getStrType();
}

// ValueType 实现
ValueType* ValueType::get(ASTContext& ctx) {
    return ctx.getValueType();
}

// IntegerType 实现
IntegerType* IntegerType::get(ASTContext& ctx, unsigned bitWidth, bool isSigned) {
    return ctx.getIntegerType(bitWidth, isSigned);
}

// FloatType 实现
FloatType* FloatType::get(ASTContext& ctx, unsigned bitWidth) {
    return ctx.getFloatType(bitWidth);
}

// ArrayType 实现
ArrayType* ArrayType::get(ASTContext& ctx, Type* element, uint64_t size) {
    return ctx.getArrayType(element, size);
}

// SliceType 实现
SliceType* SliceType::get(ASTContext& ctx, Type* element, bool isMut) {
    return ctx.getSliceType(element, isMut);
}

// TupleType 实现
TupleType* TupleType::get(ASTContext& ctx, std::vector<Type*> elements) {
    return ctx.getTupleType(std::move(elements));
}

// VarArgsType 实现
VarArgsType* VarArgsType::get(ASTContext& ctx, Type* elementType) {
    return ctx.getVarArgsType(elementType);
}

// OptionalType 实现
OptionalType* OptionalType::get(ASTContext& ctx, Type* inner) {
    return ctx.getOptionalType(inner);
}

// ReferenceType 实现
ReferenceType* ReferenceType::get(ASTContext& ctx, Type* pointee, bool isMut) {
    return ctx.getReferenceType(pointee, isMut);
}

// PointerType 实现
PointerType* PointerType::get(ASTContext& ctx, Type* pointee, bool isMut) {
    return ctx.getPointerType(pointee, isMut);
}

// FunctionType 实现
FunctionType* FunctionType::get(ASTContext& ctx, std::vector<Type*> params,
                                Type* returnType, bool canError, bool isVariadic) {
    return ctx.getFunctionType(std::move(params), returnType, canError, isVariadic);
}

// StructType 实现
StructType* StructType::get(ASTContext& ctx, std::string name, std::vector<Field> fields) {
    // 提取字段类型和名称
    std::vector<Type*> fieldTypes;
    std::vector<std::string> fieldNames;
    for (const auto& field : fields) {
        fieldTypes.push_back(field.FieldType);
        fieldNames.push_back(field.Name);
    }
    return ctx.getStructType(std::move(name), std::move(fieldTypes), std::move(fieldNames));
}

void StructType::computeLayout() {
    if (Fields.empty()) {
        CachedSize = 0;
        CachedAlign = 1;
        return;
    }
    
    size_t offset = 0;
    size_t maxAlign = 1;
    
    // 计算每个字段的偏移和对齐
    for (auto& field : Fields) {
        size_t fieldAlign = field.FieldType->getAlignment();
        maxAlign = std::max(maxAlign, fieldAlign);
        
        // 对齐当前偏移
        offset = (offset + fieldAlign - 1) & ~(fieldAlign - 1);
        field.Offset = offset;
        offset += field.FieldType->getSize();
    }
    
    // 最终对齐到最大对齐要求
    CachedSize = (offset + maxAlign - 1) & ~(maxAlign - 1);
    CachedAlign = maxAlign;
}

// EnumType 实现
EnumType* EnumType::get(ASTContext& ctx, std::string name, std::vector<Variant> variants) {
    // 提取变体数据类型和名称
    std::vector<Type*> variantDataTypes;
    std::vector<std::string> variantNames;
    for (const auto& variant : variants) {
        variantNames.push_back(variant.Name);
        if (!variant.Data.empty()) {
            variantDataTypes.push_back(variant.Data[0]); // 简化：只取第一个数据类型
        } else {
            variantDataTypes.push_back(nullptr);
        }
    }
    return ctx.getEnumType(std::move(name), std::move(variantDataTypes), std::move(variantNames));
}

void EnumType::computeLayout() {
    if (Variants.empty()) {
        CachedSize = 1; // 至少需要一个字节存储标签
        CachedAlign = 1;
        return;
    }
    
    // 计算标签大小（根据变体数量）
    size_t tagSize = 1;
    if (Variants.size() > 255) {
        tagSize = 2;
    }
    if (Variants.size() > 65535) {
        tagSize = 4;
    }
    
    size_t maxDataSize = 0;
    size_t maxAlign = tagSize;
    
    // 计算每个变体的数据大小和对齐
    for (const auto& variant : Variants) {
        if (!variant.Data.empty()) {
            size_t dataSize = 0;
            size_t dataAlign = 1;
            
            // 计算变体数据的大小和对齐（类似元组）
            for (Type* dataType : variant.Data) {
                size_t typeAlign = dataType->getAlignment();
                dataAlign = std::max(dataAlign, typeAlign);
                
                // 对齐当前偏移
                dataSize = (dataSize + typeAlign - 1) & ~(typeAlign - 1);
                dataSize += dataType->getSize();
            }
            
            // 最终对齐
            dataSize = (dataSize + dataAlign - 1) & ~(dataAlign - 1);
            maxDataSize = std::max(maxDataSize, dataSize);
            maxAlign = std::max(maxAlign, dataAlign);
        }
    }
    
    // 枚举布局：标签 + 最大变体数据（对齐）
    size_t alignedTagSize = (tagSize + maxAlign - 1) & ~(maxAlign - 1);
    CachedSize = alignedTagSize + maxDataSize;
    CachedAlign = maxAlign;
}

// TraitType 实现
TraitType* TraitType::get(ASTContext& ctx, std::string name) {
    return ctx.getTraitType(std::move(name));
}

// GenericType 实现
GenericType* GenericType::get(ASTContext& ctx, std::string name,
                              std::vector<TraitType*> constraints) {
    return ctx.getGenericType(std::move(name), std::move(constraints));
}

// GenericInstanceType 实现
GenericInstanceType* GenericInstanceType::get(ASTContext& ctx, Type* baseType,
                                               std::vector<Type*> typeArgs) {
    return ctx.getGenericInstanceType(baseType, std::move(typeArgs));
}

// TypeVariable 实现
TypeVariable* TypeVariable::get(ASTContext& ctx, size_t id) {
    return ctx.getTypeVariable(id);
}

// ErrorType 实现
ErrorType* ErrorType::get(ASTContext& ctx, Type* successType) {
    return ctx.getErrorType(successType);
}

// ModuleType 实现
ModuleType* ModuleType::get(ASTContext& ctx, std::string name, std::vector<Member> members) {
    return ctx.getModuleType(std::move(name), std::move(members));
}

// TypeAlias 实现
TypeAlias* TypeAlias::get(ASTContext& ctx, std::string name, Type* aliasedType) {
    return ctx.getTypeAlias(std::move(name), aliasedType);
}

} // namespace yuan
