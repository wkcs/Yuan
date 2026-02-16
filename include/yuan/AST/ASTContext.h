/// \file ASTContext.h
/// \brief AST 上下文和内存管理。

#ifndef YUAN_AST_ASTCONTEXT_H
#define YUAN_AST_ASTCONTEXT_H

#include "yuan/AST/AST.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Sema/Type.h"
#include <memory>
#include <vector>
#include <unordered_map>

namespace yuan {

class FuncDecl;

/// \brief AST 上下文 - 管理 AST 节点的内存
class ASTContext {
public:
    explicit ASTContext(SourceManager& sm);
    ~ASTContext();
    
    ASTContext(const ASTContext&) = delete;
    ASTContext& operator=(const ASTContext&) = delete;
    
    SourceManager& getSourceManager() { return SM; }
    const SourceManager& getSourceManager() const { return SM; }
    
    template<typename T, typename... Args>
    T* create(Args&&... args) {
        static_assert(std::is_base_of<ASTNode, T>::value,
                      "T must be derived from ASTNode");
        auto node = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = node.get();
        Nodes.push_back(std::move(node));
        return ptr;
    }
    
    size_t getNodeCount() const { return Nodes.size(); }
    void clear() { Nodes.clear(); }
    
    unsigned getPointerBitWidth() const { return PointerBitWidth; }
    void setPointerBitWidth(unsigned width) { PointerBitWidth = width; }
    
    // 类型工厂方法
    VoidType* getVoidType();
    BoolType* getBoolType();
    CharType* getCharType();
    StringType* getStrType();
    ValueType* getValueType();
    IntegerType* getIntegerType(unsigned bitWidth, bool isSigned);
    FloatType* getFloatType(unsigned bitWidth);
    ArrayType* getArrayType(Type* element, uint64_t size);
    SliceType* getSliceType(Type* element, bool isMut);
    TupleType* getTupleType(std::vector<Type*> elements);
    VarArgsType* getVarArgsType(Type* elementType);
    OptionalType* getOptionalType(Type* inner);
    ReferenceType* getReferenceType(Type* pointee, bool isMut);
    PointerType* getPointerType(Type* pointee, bool isMut);
    FunctionType* getFunctionType(std::vector<Type*> params, Type* returnType, bool canError, bool isVariadic = false);
    StructType* getStructType(std::string name, std::vector<Type*> fieldTypes, std::vector<std::string> fieldNames);
    EnumType* getEnumType(std::string name, std::vector<Type*> variantDataTypes, std::vector<std::string> variantNames);
    TraitType* getTraitType(std::string name);
    GenericType* getGenericType(std::string name, std::vector<TraitType*> constraints = {});
    GenericInstanceType* getGenericInstanceType(Type* baseType, std::vector<Type*> typeArgs);
    TypeVariable* getTypeVariable(size_t id);
    TypeVariable* createTypeVariable(); // 创建新的类型变量
    ErrorType* getErrorType(Type* successType);
    TypeAlias* getTypeAlias(std::string name, Type* aliasedType);
    ModuleType* getModuleType(std::string name, std::vector<ModuleType::Member> members);
    RangeType* getRangeType(Type* elementType, bool inclusive);

    // Display/Debug trait impl registry (for formatting)
    void registerDisplayImpl(Type* type, FuncDecl* method);
    void registerDebugImpl(Type* type, FuncDecl* method);
    FuncDecl* getDisplayImpl(Type* type) const;
    FuncDecl* getDebugImpl(Type* type) const;
    void registerImplMethod(Type* type, FuncDecl* method);
    FuncDecl* getImplMethod(Type* type, const std::string& name) const;

    // 常用类型快捷方式
    IntegerType* getI8Type() { return getIntegerType(8, true); }
    IntegerType* getI16Type() { return getIntegerType(16, true); }
    IntegerType* getI32Type() { return getIntegerType(32, true); }
    IntegerType* getI64Type() { return getIntegerType(64, true); }
    IntegerType* getU8Type() { return getIntegerType(8, false); }
    IntegerType* getU16Type() { return getIntegerType(16, false); }
    IntegerType* getU32Type() { return getIntegerType(32, false); }
    IntegerType* getU64Type() { return getIntegerType(64, false); }
    FloatType* getF32Type() { return getFloatType(32); }
    FloatType* getF64Type() { return getFloatType(64); }
    
private:
    SourceManager& SM;
    std::vector<std::unique_ptr<ASTNode>> Nodes;
    unsigned PointerBitWidth = static_cast<unsigned>(sizeof(void*) * 8);
    
    // 类型缓存
    std::unique_ptr<VoidType> VoidTy;
    std::unique_ptr<BoolType> BoolTy;
    std::unique_ptr<CharType> CharTy;
    std::unique_ptr<StringType> StrTy;
    std::unique_ptr<ValueType> ValueTy;
    
    // 整数类型缓存的键类型
    struct IntegerTypeKey {
        unsigned bitWidth;
        bool isSigned;
        
        bool operator==(const IntegerTypeKey& other) const {
            return bitWidth == other.bitWidth && isSigned == other.isSigned;
        }
    };
    
    // 整数类型键的哈希函数
    struct IntegerTypeKeyHash {
        size_t operator()(const IntegerTypeKey& key) const {
            return std::hash<unsigned>()(key.bitWidth) ^ 
                   (std::hash<bool>()(key.isSigned) << 1);
        }
    };
    
    std::unordered_map<IntegerTypeKey, std::unique_ptr<IntegerType>, IntegerTypeKeyHash> IntegerTypes;
    std::unordered_map<unsigned, std::unique_ptr<FloatType>> FloatTypes;

    // Display/Debug impls by concrete type
    std::unordered_map<const Type*, FuncDecl*> DisplayImpls;
    std::unordered_map<const Type*, FuncDecl*> DebugImpls;
    std::unordered_map<const Type*, std::unordered_map<std::string, FuncDecl*>> ImplMethods;
    
    // 复合类型缓存
    struct ArrayTypeKey {
        Type* element;
        uint64_t size;
        
        bool operator==(const ArrayTypeKey& other) const {
            return element == other.element && size == other.size;
        }
    };
    
    struct ArrayTypeKeyHash {
        size_t operator()(const ArrayTypeKey& key) const {
            return std::hash<Type*>()(key.element) ^ 
                   (std::hash<uint64_t>()(key.size) << 1);
        }
    };
    
    struct SliceTypeKey {
        Type* element;
        bool isMut;
        
        bool operator==(const SliceTypeKey& other) const {
            return element == other.element && isMut == other.isMut;
        }
    };
    
    struct SliceTypeKeyHash {
        size_t operator()(const SliceTypeKey& key) const {
            return std::hash<Type*>()(key.element) ^ 
                   (std::hash<bool>()(key.isMut) << 1);
        }
    };
    
    std::unordered_map<ArrayTypeKey, std::unique_ptr<ArrayType>, ArrayTypeKeyHash> ArrayTypes;
    std::unordered_map<SliceTypeKey, std::unique_ptr<SliceType>, SliceTypeKeyHash> SliceTypes;

    // Range 类型缓存
    struct RangeTypeKey {
        Type* element;
        bool inclusive;

        bool operator==(const RangeTypeKey& other) const {
            return element == other.element && inclusive == other.inclusive;
        }
    };

    struct RangeTypeKeyHash {
        size_t operator()(const RangeTypeKey& key) const {
            return std::hash<Type*>()(key.element) ^
                   (std::hash<bool>()(key.inclusive) << 1);
        }
    };

    std::unordered_map<RangeTypeKey, std::unique_ptr<RangeType>, RangeTypeKeyHash> RangeTypes;

    // 元组类型键的哈希函数
    struct TupleTypeKeyHash {
        size_t operator()(const std::vector<Type*>& elements) const {
            size_t hash = 0;
            for (size_t i = 0; i < elements.size(); ++i) {
                hash ^= std::hash<Type*>()(elements[i]) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            return hash;
        }
    };
    
    std::unordered_map<std::vector<Type*>, std::unique_ptr<TupleType>, TupleTypeKeyHash> TupleTypes;
    std::unordered_map<Type*, std::unique_ptr<VarArgsType>> VarArgsTypes;
    std::unordered_map<Type*, std::unique_ptr<OptionalType>> OptionalTypes;
    
    // 引用和指针类型缓存
    struct RefPtrTypeKey {
        Type* pointee;
        bool isMut;
        
        bool operator==(const RefPtrTypeKey& other) const {
            return pointee == other.pointee && isMut == other.isMut;
        }
    };
    
    struct RefPtrTypeKeyHash {
        size_t operator()(const RefPtrTypeKey& key) const {
            return std::hash<Type*>()(key.pointee) ^ 
                   (std::hash<bool>()(key.isMut) << 1);
        }
    };
    
    std::unordered_map<RefPtrTypeKey, std::unique_ptr<ReferenceType>, RefPtrTypeKeyHash> ReferenceTypes;
    std::unordered_map<RefPtrTypeKey, std::unique_ptr<PointerType>, RefPtrTypeKeyHash> PointerTypes;
    
    // 函数类型缓存
    struct FunctionTypeKey {
        std::vector<Type*> params;
        Type* returnType;
        bool canError;
        bool isVariadic;

        bool operator==(const FunctionTypeKey& other) const {
            return params == other.params &&
                   returnType == other.returnType &&
                   canError == other.canError &&
                   isVariadic == other.isVariadic;
        }
    };

    struct FunctionTypeKeyHash {
        size_t operator()(const FunctionTypeKey& key) const {
            size_t hash = std::hash<Type*>()(key.returnType);
            hash ^= std::hash<bool>()(key.canError) << 1;
            hash ^= std::hash<bool>()(key.isVariadic) << 2;
            for (size_t i = 0; i < key.params.size(); ++i) {
                hash ^= std::hash<Type*>()(key.params[i]) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            return hash;
        }
    };
    
    std::unordered_map<FunctionTypeKey, std::unique_ptr<FunctionType>, FunctionTypeKeyHash> FunctionTypes;
    
    // 用户定义类型缓存（按名称）
    std::unordered_map<std::string, std::unique_ptr<StructType>> StructTypes;
    std::unordered_map<std::string, std::unique_ptr<EnumType>> EnumTypes;
    std::unordered_map<std::string, std::unique_ptr<TraitType>> TraitTypes;
    std::unordered_map<std::string, std::unique_ptr<GenericType>> GenericTypes;

    // 泛型实例类型缓存
    struct GenericInstanceTypeKey {
        Type* baseType;
        std::vector<Type*> typeArgs;

        bool operator==(const GenericInstanceTypeKey& other) const {
            return baseType == other.baseType && typeArgs == other.typeArgs;
        }
    };

    struct GenericInstanceTypeKeyHash {
        size_t operator()(const GenericInstanceTypeKey& key) const {
            size_t hash = std::hash<Type*>()(key.baseType);
            for (size_t i = 0; i < key.typeArgs.size(); ++i) {
                hash ^= std::hash<Type*>()(key.typeArgs[i]) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            return hash;
        }
    };

    std::unordered_map<GenericInstanceTypeKey, std::unique_ptr<GenericInstanceType>, GenericInstanceTypeKeyHash> GenericInstanceTypes;
    
    // 类型变量缓存
    std::unordered_map<size_t, std::unique_ptr<TypeVariable>> TypeVariables;
    size_t NextTypeVarID = 0;
    
    // 错误类型缓存
    std::unordered_map<Type*, std::unique_ptr<ErrorType>> ErrorTypes;
    
    // 类型别名缓存
    std::unordered_map<std::string, std::unique_ptr<TypeAlias>> TypeAliases;

    // 通用类型存储（用于不需要缓存的类型，如 ModuleType）
    std::vector<std::unique_ptr<Type>> Types;
};

} // namespace yuan

#endif // YUAN_AST_ASTCONTEXT_H
