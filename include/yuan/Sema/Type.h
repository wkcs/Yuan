/// \file
/// \brief Yuan 语义分析类型系统
///
/// 定义了语义分析阶段使用的类型系统，区别于 AST 中的 TypeNode。
/// 这些类型用于类型检查、类型推断和代码生成。

#ifndef YUAN_SEMA_TYPE_H
#define YUAN_SEMA_TYPE_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace llvm {
class StringRef;
}

namespace yuan {

class ASTContext;

/// \brief 类型基类
///
/// 所有语义类型的基类，提供类型比较、字符串表示、大小和对齐信息。
/// 与 AST 中的 TypeNode 不同，这些类型用于语义分析和代码生成。
class Type {
public:
    /// 类型种类枚举
    enum class Kind {
        Void,            ///< void 类型
        Bool,            ///< bool 类型
        Integer,         ///< 整数类型 (i8, i16, i32, i64, u8, u16, u32, u64 等)
        Float,           ///< 浮点类型 (f32, f64)
        Char,            ///< char 类型
        String,          ///< str 类型
        Value,           ///< Value 动态值类型
        Array,           ///< 数组类型 [T; N]
        Slice,           ///< 切片类型 &[T]
        Tuple,           ///< 元组类型 (T1, T2, ...)
        VarArgs,         ///< 可变参数类型 VarArgs<T>
        Optional,        ///< 可选类型 ?T
        Reference,       ///< 引用类型 &T, &mut T
        Pointer,         ///< 指针类型 *T, *mut T
        Function,        ///< 函数类型 func(T1, T2) -> R
        Struct,          ///< 结构体类型
        Enum,            ///< 枚举类型
        Trait,           ///< Trait 类型
        Generic,         ///< 泛型类型参数 (T, U)
        GenericInstance, ///< 泛型类型实例 (Vec<i32>, HashMap<String, i32>)
        Error,           ///< 错误类型 !T
        TypeVar,         ///< 类型变量（用于类型推断）
        TypeAlias,       ///< 类型别名
        Module,          ///< 模块类型
        Range,           ///< 范围类型 (start..end, start..=end)
    };
    
    /// 构造函数
    /// \param kind 类型种类
    explicit Type(Kind kind) : TypeKind(kind) {}
    
    /// 虚析构函数
    virtual ~Type() = default;
    
    /// 获取类型种类
    Kind getKind() const { return TypeKind; }
    
    /// 检查类型是否相等
    /// \param other 要比较的类型
    /// \return 如果类型相等返回 true
    virtual bool isEqual(const Type* other) const = 0;
    
    /// 获取类型的字符串表示
    /// \return 类型的字符串表示
    virtual std::string toString() const = 0;
    
    /// 获取类型的大小（字节）
    /// \return 类型大小
    virtual size_t getSize() const = 0;
    
    /// 获取类型的对齐要求（字节）
    /// \return 对齐要求
    virtual size_t getAlignment() const = 0;
    
    // 类型检查便利方法
    bool isVoid() const { return TypeKind == Kind::Void; }
    bool isBool() const { return TypeKind == Kind::Bool; }
    bool isInteger() const { return TypeKind == Kind::Integer; }
    bool isFloat() const { return TypeKind == Kind::Float; }
    bool isChar() const { return TypeKind == Kind::Char; }
    bool isString() const { return TypeKind == Kind::String; }
    bool isValue() const { return TypeKind == Kind::Value; }
    bool isArray() const { return TypeKind == Kind::Array; }
    bool isSlice() const { return TypeKind == Kind::Slice; }
    bool isTuple() const { return TypeKind == Kind::Tuple; }
    bool isVarArgs() const { return TypeKind == Kind::VarArgs; }
    bool isOptional() const { return TypeKind == Kind::Optional; }
    bool isReference() const { return TypeKind == Kind::Reference; }
    bool isPointer() const { return TypeKind == Kind::Pointer; }
    bool isFunction() const { return TypeKind == Kind::Function; }
    bool isStruct() const { return TypeKind == Kind::Struct; }
    bool isEnum() const { return TypeKind == Kind::Enum; }
    bool isTrait() const { return TypeKind == Kind::Trait; }
    bool isGeneric() const { return TypeKind == Kind::Generic; }
    bool isGenericInstance() const { return TypeKind == Kind::GenericInstance; }
    bool isError() const { return TypeKind == Kind::Error; }
    bool isTypeVar() const { return TypeKind == Kind::TypeVar; }
    bool isTypeAlias() const { return TypeKind == Kind::TypeAlias; }
    bool isModule() const { return TypeKind == Kind::Module; }
    bool isRange() const { return TypeKind == Kind::Range; }

    /// 检查是否为数值类型
    bool isNumeric() const { return isInteger() || isFloat(); }

    /// 检查是否为指针类型（包括引用和指针）
    bool isPointerLike() const { return isReference() || isPointer(); }
    
protected:
    Kind TypeKind;
};

/// \brief void 类型
class VoidType : public Type {
public:
    VoidType() : Type(Kind::Void) {}
    
    bool isEqual(const Type* other) const override {
        return other && other->isVoid();
    }
    
    std::string toString() const override {
        return "void";
    }
    
    size_t getSize() const override {
        return 0;
    }
    
    size_t getAlignment() const override {
        return 1;
    }
    
    /// 获取单例实例
    static VoidType* get(ASTContext& ctx);
};

/// \brief bool 类型
class BoolType : public Type {
public:
    BoolType() : Type(Kind::Bool) {}
    
    bool isEqual(const Type* other) const override {
        return other && other->isBool();
    }
    
    std::string toString() const override {
        return "bool";
    }
    
    size_t getSize() const override {
        return 1;
    }
    
    size_t getAlignment() const override {
        return 1;
    }
    
    /// 获取单例实例
    static BoolType* get(ASTContext& ctx);
};

/// \brief char 类型
class CharType : public Type {
public:
    CharType() : Type(Kind::Char) {}
    
    bool isEqual(const Type* other) const override {
        return other && other->isChar();
    }
    
    std::string toString() const override {
        return "char";
    }
    
    size_t getSize() const override {
        return 4; // UTF-32 字符
    }
    
    size_t getAlignment() const override {
        return 4;
    }
    
    /// 获取单例实例
    static CharType* get(ASTContext& ctx);
};

/// \brief str 类型
class StringType : public Type {
public:
    StringType() : Type(Kind::String) {}
    
    bool isEqual(const Type* other) const override {
        return other && other->isString();
    }
    
    std::string toString() const override {
        return "str";
    }
    
    size_t getSize() const override {
        return sizeof(void*) + sizeof(size_t); // 指针 + 长度
    }
    
    size_t getAlignment() const override {
        return alignof(void*);
    }
    
    /// 获取单例实例
    static StringType* get(ASTContext& ctx);
};

/// \brief Value 动态值类型
class ValueType : public Type {
public:
    ValueType() : Type(Kind::Value) {}

    bool isEqual(const Type* other) const override {
        return other && other->isValue();
    }

    std::string toString() const override {
        return "Value";
    }

    size_t getSize() const override {
        // { i32 tag, i32 padding, i64 data0, i64 data1 }
        return 24;
    }

    size_t getAlignment() const override {
        return 8;
    }

    /// 获取 Value 类型实例
    static ValueType* get(ASTContext& ctx);
};

/// \brief 整数类型
class IntegerType : public Type {
public:
    /// 构造函数
    /// \param bitWidth 位宽 (8, 16, 32, 64, 128)
    /// \param isSigned 是否有符号
    IntegerType(unsigned bitWidth, bool isSigned)
        : Type(Kind::Integer), BitWidth(bitWidth), IsSigned(isSigned) {}
    
    /// 获取位宽
    unsigned getBitWidth() const { return BitWidth; }
    
    /// 是否有符号
    bool isSigned() const { return IsSigned; }
    
    bool isEqual(const Type* other) const override {
        if (!other || !other->isInteger()) {
            return false;
        }
        auto* intTy = static_cast<const IntegerType*>(other);
        return BitWidth == intTy->BitWidth && IsSigned == intTy->IsSigned;
    }
    
    std::string toString() const override {
        if (IsSigned) {
            switch (BitWidth) {
                case 8: return "i8";
                case 16: return "i16";
                case 32: return "i32";
                case 64: return "i64";
                case 128: return "i128";
                default: return "isize"; // 假设其他情况为 isize
            }
        } else {
            switch (BitWidth) {
                case 8: return "u8";
                case 16: return "u16";
                case 32: return "u32";
                case 64: return "u64";
                case 128: return "u128";
                default: return "usize"; // 假设其他情况为 usize
            }
        }
    }
    
    size_t getSize() const override {
        return BitWidth / 8;
    }
    
    size_t getAlignment() const override {
        // 对齐要求通常等于大小，但不超过指针大小
        size_t size = getSize();
        size_t ptrSize = sizeof(void*);
        return size > ptrSize ? ptrSize : size;
    }
    
    /// 获取整数类型实例
    /// \param ctx AST 上下文
    /// \param bitWidth 位宽
    /// \param isSigned 是否有符号
    /// \return 整数类型实例
    static IntegerType* get(ASTContext& ctx, unsigned bitWidth, bool isSigned);
    
private:
    unsigned BitWidth;
    bool IsSigned;
};

/// \brief 浮点类型
class FloatType : public Type {
public:
    /// 构造函数
    /// \param bitWidth 位宽 (32 或 64)
    explicit FloatType(unsigned bitWidth)
        : Type(Kind::Float), BitWidth(bitWidth) {}
    
    /// 获取位宽
    unsigned getBitWidth() const { return BitWidth; }
    
    bool isEqual(const Type* other) const override {
        if (!other || !other->isFloat()) {
            return false;
        }
        auto* floatTy = static_cast<const FloatType*>(other);
        return BitWidth == floatTy->BitWidth;
    }
    
    std::string toString() const override {
        switch (BitWidth) {
            case 32: return "f32";
            case 64: return "f64";
            default: return "f" + std::to_string(BitWidth);
        }
    }
    
    size_t getSize() const override {
        return BitWidth / 8;
    }
    
    size_t getAlignment() const override {
        return BitWidth / 8;
    }
    
    /// 获取浮点类型实例
    /// \param ctx AST 上下文
    /// \param bitWidth 位宽
    /// \return 浮点类型实例
    static FloatType* get(ASTContext& ctx, unsigned bitWidth);
    
private:
    unsigned BitWidth;
};

/// \brief 数组类型 [T; N]
class ArrayType : public Type {
public:
    /// 构造函数
    /// \param element 元素类型
    /// \param size 数组大小
    ArrayType(Type* element, uint64_t size)
        : Type(Kind::Array), Element(element), Size(size) {}
    
    /// 获取元素类型
    Type* getElementType() const { return Element; }
    
    /// 获取数组大小
    uint64_t getArraySize() const { return Size; }
    
    bool isEqual(const Type* other) const override {
        if (!other || !other->isArray()) {
            return false;
        }
        auto* arrayTy = static_cast<const ArrayType*>(other);
        return Size == arrayTy->Size && Element->isEqual(arrayTy->Element);
    }
    
    std::string toString() const override {
        return "[" + Element->toString() + "; " + std::to_string(Size) + "]";
    }
    
    size_t getSize() const override {
        return Element->getSize() * Size;
    }
    
    size_t getAlignment() const override {
        return Element->getAlignment();
    }
    
    /// 获取数组类型实例
    /// \param ctx AST 上下文
    /// \param element 元素类型
    /// \param size 数组大小
    /// \return 数组类型实例
    static ArrayType* get(ASTContext& ctx, Type* element, uint64_t size);
    
private:
    Type* Element;
    uint64_t Size;
};

/// \brief 切片类型 &[T] 或 &mut [T]
class SliceType : public Type {
public:
    /// 构造函数
    /// \param element 元素类型
    /// \param isMut 是否可变
    SliceType(Type* element, bool isMut)
        : Type(Kind::Slice), Element(element), IsMut(isMut) {}
    
    /// 获取元素类型
    Type* getElementType() const { return Element; }
    
    /// 是否可变
    bool isMutable() const { return IsMut; }
    
    bool isEqual(const Type* other) const override {
        if (!other || !other->isSlice()) {
            return false;
        }
        auto* sliceTy = static_cast<const SliceType*>(other);
        return IsMut == sliceTy->IsMut && Element->isEqual(sliceTy->Element);
    }
    
    std::string toString() const override {
        return "&" + std::string(IsMut ? "mut " : "") + "[" + Element->toString() + "]";
    }
    
    size_t getSize() const override {
        return sizeof(void*) + sizeof(size_t); // 指针 + 长度
    }
    
    size_t getAlignment() const override {
        return alignof(void*);
    }
    
    /// 获取切片类型实例
    /// \param ctx AST 上下文
    /// \param element 元素类型
    /// \param isMut 是否可变
    /// \return 切片类型实例
    static SliceType* get(ASTContext& ctx, Type* element, bool isMut);
    
private:
    Type* Element;
    bool IsMut;
};

/// \brief 元组类型 (T1, T2, ...)
class TupleType : public Type {
public:
    /// 构造函数
    /// \param elements 元素类型列表
    explicit TupleType(std::vector<Type*> elements)
        : Type(Kind::Tuple), Elements(std::move(elements)) {}
    
    /// 获取元素类型列表
    const std::vector<Type*>& getElements() const { return Elements; }
    
    /// 获取元素数量
    size_t getElementCount() const { return Elements.size(); }
    
    /// 获取指定索引的元素类型
    /// \param index 元素索引
    /// \return 元素类型，如果索引无效返回 nullptr
    Type* getElement(size_t index) const {
        return index < Elements.size() ? Elements[index] : nullptr;
    }
    
    bool isEqual(const Type* other) const override {
        if (!other || !other->isTuple()) {
            return false;
        }
        auto* tupleTy = static_cast<const TupleType*>(other);
        if (Elements.size() != tupleTy->Elements.size()) {
            return false;
        }
        for (size_t i = 0; i < Elements.size(); ++i) {
            if (!Elements[i]->isEqual(tupleTy->Elements[i])) {
                return false;
            }
        }
        return true;
    }
    
    std::string toString() const override {
        if (Elements.empty()) {
            return "()";
        }
        std::string result = "(";
        for (size_t i = 0; i < Elements.size(); ++i) {
            if (i > 0) result += ", ";
            result += Elements[i]->toString();
        }
        result += ")";
        return result;
    }
    
    size_t getSize() const override {
        size_t totalSize = 0;
        size_t maxAlign = 1;
        
        // 计算每个元素的对齐和大小
        for (Type* elem : Elements) {
            size_t elemAlign = elem->getAlignment();
            maxAlign = std::max(maxAlign, elemAlign);
            
            // 对齐当前偏移
            totalSize = (totalSize + elemAlign - 1) & ~(elemAlign - 1);
            totalSize += elem->getSize();
        }
        
        // 最终对齐到最大对齐要求
        totalSize = (totalSize + maxAlign - 1) & ~(maxAlign - 1);
        return totalSize;
    }
    
    size_t getAlignment() const override {
        size_t maxAlign = 1;
        for (Type* elem : Elements) {
            maxAlign = std::max(maxAlign, elem->getAlignment());
        }
        return maxAlign;
    }
    
    /// 获取元组类型实例
    /// \param ctx AST 上下文
    /// \param elements 元素类型列表
    /// \return 元组类型实例
    static TupleType* get(ASTContext& ctx, std::vector<Type*> elements);
    
private:
    std::vector<Type*> Elements;
};

/// \brief 可变参数类型 VarArgs<T>
class VarArgsType : public Type {
public:
    explicit VarArgsType(Type* elementType)
        : Type(Kind::VarArgs), ElementType(elementType) {}

    Type* getElementType() const { return ElementType; }

    bool isEqual(const Type* other) const override {
        if (!other || !other->isVarArgs()) {
            return false;
        }
        auto* varTy = static_cast<const VarArgsType*>(other);
        return ElementType->isEqual(varTy->ElementType);
    }

    std::string toString() const override {
        return "VarArgs<" + ElementType->toString() + ">";
    }

    size_t getSize() const override {
        // { i64 len, Value* values }
        return 16;
    }

    size_t getAlignment() const override {
        return 8;
    }

    /// 获取 VarArgs 类型实例
    static VarArgsType* get(ASTContext& ctx, Type* elementType);

private:
    Type* ElementType;
};

/// \brief 可选类型 ?T
class OptionalType : public Type {
public:
    /// 构造函数
    /// \param inner 内部类型
    explicit OptionalType(Type* inner)
        : Type(Kind::Optional), Inner(inner) {}
    
    /// 获取内部类型
    Type* getInnerType() const { return Inner; }
    
    bool isEqual(const Type* other) const override {
        if (!other || !other->isOptional()) {
            return false;
        }
        auto* optTy = static_cast<const OptionalType*>(other);
        return Inner->isEqual(optTy->Inner);
    }
    
    std::string toString() const override {
        return "?" + Inner->toString();
    }
    
    size_t getSize() const override {
        // Optional 类型通常实现为带标签的联合
        // 1 字节标签 + 内部类型大小（对齐）
        size_t innerSize = Inner->getSize();
        size_t innerAlign = Inner->getAlignment();
        
        // 标签占用 1 字节，然后对齐到内部类型的对齐要求
        size_t tagSize = 1;
        size_t alignedTagSize = (tagSize + innerAlign - 1) & ~(innerAlign - 1);
        
        return alignedTagSize + innerSize;
    }
    
    size_t getAlignment() const override {
        // 对齐要求是内部类型的对齐要求
        return Inner->getAlignment();
    }
    
    /// 获取可选类型实例
    /// \param ctx AST 上下文
    /// \param inner 内部类型
    /// \return 可选类型实例
    static OptionalType* get(ASTContext& ctx, Type* inner);
    
private:
    Type* Inner;
};

/// \brief 引用类型 &T 或 &mut T
class ReferenceType : public Type {
public:
    /// 构造函数
    /// \param pointee 指向的类型
    /// \param isMut 是否可变引用
    ReferenceType(Type* pointee, bool isMut)
        : Type(Kind::Reference), Pointee(pointee), IsMut(isMut) {}
    
    /// 获取指向的类型
    Type* getPointeeType() const { return Pointee; }
    
    /// 是否可变引用
    bool isMutable() const { return IsMut; }
    
    bool isEqual(const Type* other) const override {
        if (!other || !other->isReference()) {
            return false;
        }
        auto* refTy = static_cast<const ReferenceType*>(other);
        return IsMut == refTy->IsMut && Pointee->isEqual(refTy->Pointee);
    }
    
    std::string toString() const override {
        return "&" + std::string(IsMut ? "mut " : "") + Pointee->toString();
    }
    
    size_t getSize() const override {
        return sizeof(void*); // 引用在运行时是指针
    }
    
    size_t getAlignment() const override {
        return alignof(void*);
    }
    
    /// 获取引用类型实例
    /// \param ctx AST 上下文
    /// \param pointee 指向的类型
    /// \param isMut 是否可变引用
    /// \return 引用类型实例
    static ReferenceType* get(ASTContext& ctx, Type* pointee, bool isMut);
    
private:
    Type* Pointee;
    bool IsMut;
};

/// \brief 指针类型 *T 或 *mut T
class PointerType : public Type {
public:
    /// 构造函数
    /// \param pointee 指向的类型
    /// \param isMut 是否可变指针
    PointerType(Type* pointee, bool isMut)
        : Type(Kind::Pointer), Pointee(pointee), IsMut(isMut) {}
    
    /// 获取指向的类型
    Type* getPointeeType() const { return Pointee; }
    
    /// 是否可变指针
    bool isMutable() const { return IsMut; }
    
    bool isEqual(const Type* other) const override {
        if (!other || !other->isPointer()) {
            return false;
        }
        auto* ptrTy = static_cast<const PointerType*>(other);
        return IsMut == ptrTy->IsMut && Pointee->isEqual(ptrTy->Pointee);
    }
    
    std::string toString() const override {
        return "*" + std::string(IsMut ? "mut " : "") + Pointee->toString();
    }
    
    size_t getSize() const override {
        return sizeof(void*);
    }
    
    size_t getAlignment() const override {
        return alignof(void*);
    }
    
    /// 获取指针类型实例
    /// \param ctx AST 上下文
    /// \param pointee 指向的类型
    /// \param isMut 是否可变指针
    /// \return 指针类型实例
    static PointerType* get(ASTContext& ctx, Type* pointee, bool isMut);
    
private:
    Type* Pointee;
    bool IsMut;
};

/// \brief 函数类型 func(T1, T2) -> R
class FunctionType : public Type {
public:
    /// 构造函数
    /// \param params 参数类型列表
    /// \param returnType 返回类型
    /// \param canError 是否可能返回错误
    /// \param isVariadic 是否有可变参数
    FunctionType(std::vector<Type*> params, Type* returnType, bool canError, bool isVariadic = false)
        : Type(Kind::Function), Params(std::move(params)),
          ReturnType(returnType), CanError(canError), IsVariadic(isVariadic) {}

    /// 获取参数类型列表
    const std::vector<Type*>& getParamTypes() const { return Params; }

    /// 获取返回类型
    Type* getReturnType() const { return ReturnType; }

    /// 是否可能返回错误
    bool canError() const { return CanError; }

    /// 是否有可变参数
    bool isVariadic() const { return IsVariadic; }

    /// 获取参数数量
    size_t getParamCount() const { return Params.size(); }

    /// 获取指定索引的参数类型
    /// \param index 参数索引
    /// \return 参数类型，如果索引无效返回 nullptr
    Type* getParam(size_t index) const {
        return index < Params.size() ? Params[index] : nullptr;
    }
    
    bool isEqual(const Type* other) const override {
        if (!other || !other->isFunction()) {
            return false;
        }
        auto* funcTy = static_cast<const FunctionType*>(other);
        if (CanError != funcTy->CanError ||
            IsVariadic != funcTy->IsVariadic ||
            !ReturnType->isEqual(funcTy->ReturnType) ||
            Params.size() != funcTy->Params.size()) {
            return false;
        }
        for (size_t i = 0; i < Params.size(); ++i) {
            if (!Params[i]->isEqual(funcTy->Params[i])) {
                return false;
            }
        }
        return true;
    }
    
    std::string toString() const override {
        std::string result = "func(";
        for (size_t i = 0; i < Params.size(); ++i) {
            if (i > 0) result += ", ";
            result += Params[i]->toString();
        }
        result += ") -> ";
        if (CanError) result += "!";
        result += ReturnType->toString();
        return result;
    }
    
    size_t getSize() const override {
        return sizeof(void*); // 函数指针大小
    }
    
    size_t getAlignment() const override {
        return alignof(void*);
    }
    
    /// 获取函数类型实例
    /// \param ctx AST 上下文
    /// \param params 参数类型列表
    /// \param returnType 返回类型
    /// \param canError 是否可能返回错误
    /// \return 函数类型实例
    static FunctionType* get(ASTContext& ctx, std::vector<Type*> params,
                             Type* returnType, bool canError, bool isVariadic = false);
    
private:
    std::vector<Type*> Params;
    Type* ReturnType;
    bool CanError;
    bool IsVariadic;
};

/// \brief 结构体类型
class StructType : public Type {
public:
    /// 结构体字段
    struct Field {
        std::string Name;       ///< 字段名
        Type* FieldType;        ///< 字段类型
        size_t Offset;          ///< 字段在结构体中的偏移
        
        Field(std::string name, Type* type, size_t offset)
            : Name(std::move(name)), FieldType(type), Offset(offset) {}
    };
    
    /// 构造函数
    /// \param name 结构体名称
    /// \param fields 字段列表
    StructType(std::string name, std::vector<Field> fields)
        : Type(Kind::Struct), Name(std::move(name)), Fields(std::move(fields)) {
        computeLayout();
    }
    
    /// 获取结构体名称
    const std::string& getName() const { return Name; }
    
    /// 获取字段列表
    const std::vector<Field>& getFields() const { return Fields; }
    
    /// 获取字段数量
    size_t getFieldCount() const { return Fields.size(); }

    /// 在前向声明/占位类型场景下补全字段定义（仅允许从空定义补全）
    void populateFieldsIfEmpty(std::vector<Field> fields) {
        if (!Fields.empty() || fields.empty()) {
            return;
        }
        Fields = std::move(fields);
        computeLayout();
    }
    
    /// 根据名称查找字段
    /// \param name 字段名
    /// \return 字段指针，如果未找到返回 nullptr
    const Field* getField(const std::string& name) const {
        for (const auto& field : Fields) {
            if (field.Name == name) {
                return &field;
            }
        }
        return nullptr;
    }
    
    /// 根据索引获取字段
    /// \param index 字段索引
    /// \return 字段指针，如果索引无效返回 nullptr
    const Field* getField(size_t index) const {
        return index < Fields.size() ? &Fields[index] : nullptr;
    }
    
    bool isEqual(const Type* other) const override {
        if (!other || !other->isStruct()) {
            return false;
        }
        auto* structTy = static_cast<const StructType*>(other);
        return Name == structTy->Name; // 结构体按名称比较
    }
    
    std::string toString() const override {
        return Name;
    }
    
    size_t getSize() const override {
        return CachedSize;
    }
    
    size_t getAlignment() const override {
        return CachedAlign;
    }
    
    /// 获取结构体类型实例
    /// \param ctx AST 上下文
    /// \param name 结构体名称
    /// \param fields 字段列表
    /// \return 结构体类型实例
    static StructType* get(ASTContext& ctx, std::string name, std::vector<Field> fields);
    
private:
    std::string Name;
    std::vector<Field> Fields;
    size_t CachedSize = 0;
    size_t CachedAlign = 1;
    
    /// 计算结构体布局
    void computeLayout();
};

/// \brief 枚举类型
class EnumType : public Type {
public:
    /// 枚举变体
    struct Variant {
        std::string Name;           ///< 变体名称
        std::vector<Type*> Data;    ///< 变体数据类型（如果有）
        size_t Tag;                 ///< 变体标签值
        
        Variant(std::string name, std::vector<Type*> data, size_t tag)
            : Name(std::move(name)), Data(std::move(data)), Tag(tag) {}
    };
    
    /// 构造函数
    /// \param name 枚举名称
    /// \param variants 变体列表
    EnumType(std::string name, std::vector<Variant> variants)
        : Type(Kind::Enum), Name(std::move(name)), Variants(std::move(variants)) {
        computeLayout();
    }
    
    /// 获取枚举名称
    const std::string& getName() const { return Name; }
    
    /// 获取变体列表
    const std::vector<Variant>& getVariants() const { return Variants; }
    
    /// 获取变体数量
    size_t getVariantCount() const { return Variants.size(); }

    /// 在前向声明/占位类型场景下补全变体定义（仅允许从空定义补全）
    void populateVariantsIfEmpty(std::vector<Variant> variants) {
        if (!Variants.empty() || variants.empty()) {
            return;
        }
        Variants = std::move(variants);
        computeLayout();
    }
    
    /// 根据名称查找变体
    /// \param name 变体名
    /// \return 变体指针，如果未找到返回 nullptr
    const Variant* getVariant(const std::string& name) const {
        for (const auto& variant : Variants) {
            if (variant.Name == name) {
                return &variant;
            }
        }
        return nullptr;
    }
    
    /// 根据索引获取变体
    /// \param index 变体索引
    /// \return 变体指针，如果索引无效返回 nullptr
    const Variant* getVariant(size_t index) const {
        return index < Variants.size() ? &Variants[index] : nullptr;
    }
    
    bool isEqual(const Type* other) const override {
        if (!other || !other->isEnum()) {
            return false;
        }
        auto* enumTy = static_cast<const EnumType*>(other);
        return Name == enumTy->Name; // 枚举按名称比较
    }
    
    std::string toString() const override {
        return Name;
    }
    
    size_t getSize() const override {
        return CachedSize;
    }
    
    size_t getAlignment() const override {
        return CachedAlign;
    }
    
    /// 获取枚举类型实例
    /// \param ctx AST 上下文
    /// \param name 枚举名称
    /// \param variants 变体列表
    /// \return 枚举类型实例
    static EnumType* get(ASTContext& ctx, std::string name, std::vector<Variant> variants);
    
private:
    std::string Name;
    std::vector<Variant> Variants;
    size_t CachedSize = 0;
    size_t CachedAlign = 1;
    
    /// 计算枚举布局
    void computeLayout();
};

/// \brief Trait 类型
class TraitType : public Type {
public:
    /// 构造函数
    /// \param name Trait 名称
    explicit TraitType(std::string name)
        : Type(Kind::Trait), Name(std::move(name)) {}
    
    /// 获取 Trait 名称
    const std::string& getName() const { return Name; }
    
    bool isEqual(const Type* other) const override {
        if (!other || !other->isTrait()) {
            return false;
        }
        auto* traitTy = static_cast<const TraitType*>(other);
        return Name == traitTy->Name; // Trait 按名称比较
    }
    
    std::string toString() const override {
        return Name;
    }
    
    size_t getSize() const override {
        // Trait 对象通常是 fat pointer (指针 + vtable)
        return sizeof(void*) * 2;
    }
    
    size_t getAlignment() const override {
        return alignof(void*);
    }
    
    /// 获取 Trait 类型实例
    /// \param ctx AST 上下文
    /// \param name Trait 名称
    /// \return Trait 类型实例
    static TraitType* get(ASTContext& ctx, std::string name);
    
private:
    std::string Name;
};

/// \brief 泛型类型
class GenericType : public Type {
public:
    /// 构造函数
    /// \param name 泛型参数名称
    /// \param constraints Trait 约束列表
    GenericType(std::string name, std::vector<TraitType*> constraints = {})
        : Type(Kind::Generic), Name(std::move(name)), Constraints(std::move(constraints)) {}
    
    /// 获取泛型参数名称
    const std::string& getName() const { return Name; }
    
    /// 获取 Trait 约束列表
    const std::vector<TraitType*>& getConstraints() const { return Constraints; }
    
    /// 添加 Trait 约束
    void addConstraint(TraitType* trait) {
        Constraints.push_back(trait);
    }
    
    bool isEqual(const Type* other) const override {
        if (!other || !other->isGeneric()) {
            return false;
        }
        auto* genericTy = static_cast<const GenericType*>(other);
        return Name == genericTy->Name; // 泛型类型按名称比较
    }
    
    std::string toString() const override {
        std::string result = Name;
        if (!Constraints.empty()) {
            result += ": ";
            for (size_t i = 0; i < Constraints.size(); ++i) {
                if (i > 0) result += " + ";
                result += Constraints[i]->toString();
            }
        }
        return result;
    }
    
    size_t getSize() const override {
        // 泛型类型的大小在实例化时确定
        return 0;
    }
    
    size_t getAlignment() const override {
        // 泛型类型的对齐在实例化时确定
        return 1;
    }
    
    /// 获取泛型类型实例
    /// \param ctx AST 上下文
    /// \param name 泛型参数名称
    /// \param constraints Trait 约束列表
    /// \return 泛型类型实例
    static GenericType* get(ASTContext& ctx, std::string name, 
                            std::vector<TraitType*> constraints = {});
    
private:
    std::string Name;
    std::vector<TraitType*> Constraints;
};

/// \brief 泛型类型实例
///
/// 表示泛型类型的实例化，例如：
/// - Vec<i32>
/// - HashMap<String, i32>
/// - Result<T, SysError>
class GenericInstanceType : public Type {
public:
    /// 构造函数
    /// \param baseType 基础类型（可以是 StructType, EnumType 等）
    /// \param typeArgs 类型参数列表
    GenericInstanceType(Type* baseType, std::vector<Type*> typeArgs)
        : Type(Kind::GenericInstance), BaseType(baseType), TypeArgs(std::move(typeArgs)) {}

    /// 获取基础类型
    Type* getBaseType() const { return BaseType; }

    /// 获取类型参数列表
    const std::vector<Type*>& getTypeArgs() const { return TypeArgs; }

    /// 获取类型参数数量
    size_t getTypeArgCount() const { return TypeArgs.size(); }

    /// 获取指定索引的类型参数
    /// \param index 参数索引
    /// \return 类型参数，如果索引无效返回 nullptr
    Type* getTypeArg(size_t index) const {
        return index < TypeArgs.size() ? TypeArgs[index] : nullptr;
    }

    bool isEqual(const Type* other) const override {
        if (!other || !other->isGenericInstance()) {
            return false;
        }
        auto* genInst = static_cast<const GenericInstanceType*>(other);
        if (!BaseType->isEqual(genInst->BaseType) ||
            TypeArgs.size() != genInst->TypeArgs.size()) {
            return false;
        }
        for (size_t i = 0; i < TypeArgs.size(); ++i) {
            if (!TypeArgs[i]->isEqual(genInst->TypeArgs[i])) {
                return false;
            }
        }
        return true;
    }

    std::string toString() const override {
        std::string result = BaseType->toString();
        result += "<";
        for (size_t i = 0; i < TypeArgs.size(); ++i) {
            if (i > 0) result += ", ";
            result += TypeArgs[i]->toString();
        }
        result += ">";
        return result;
    }

    size_t getSize() const override {
        // 泛型实例的大小取决于基础类型
        // 这里需要根据类型参数实例化后的实际大小
        // 简化处理：返回基础类型大小
        return BaseType->getSize();
    }

    size_t getAlignment() const override {
        // 对齐要求同样取决于基础类型
        return BaseType->getAlignment();
    }

    /// 获取泛型实例类型
    /// \param ctx AST 上下文
    /// \param baseType 基础类型
    /// \param typeArgs 类型参数列表
    /// \return 泛型实例类型
    static GenericInstanceType* get(ASTContext& ctx, Type* baseType,
                                     std::vector<Type*> typeArgs);

private:
    Type* BaseType;                  ///< 基础类型
    std::vector<Type*> TypeArgs;     ///< 类型参数列表
};

/// \brief 类型变量（用于类型推断）
class TypeVariable : public Type {
public:
    /// 构造函数
    /// \param id 类型变量唯一标识符
    explicit TypeVariable(size_t id)
        : Type(Kind::TypeVar), ID(id), ResolvedType(nullptr) {}
    
    /// 获取类型变量 ID
    size_t getID() const { return ID; }
    
    /// 是否已解析
    bool isResolved() const { return ResolvedType != nullptr; }
    
    /// 获取解析后的类型
    Type* getResolvedType() const { return ResolvedType; }
    
    /// 设置解析后的类型
    void setResolvedType(Type* type) { ResolvedType = type; }
    
    bool isEqual(const Type* other) const override {
        if (!other || !other->isTypeVar()) {
            return false;
        }
        auto* typeVar = static_cast<const TypeVariable*>(other);
        return ID == typeVar->ID;
    }
    
    std::string toString() const override {
        if (ResolvedType) {
            return ResolvedType->toString();
        }
        return "?" + std::to_string(ID);
    }
    
    size_t getSize() const override {
        if (ResolvedType) {
            return ResolvedType->getSize();
        }
        return 0; // 未解析的类型变量大小未知
    }
    
    size_t getAlignment() const override {
        if (ResolvedType) {
            return ResolvedType->getAlignment();
        }
        return 1; // 未解析的类型变量对齐未知
    }
    
    /// 获取类型变量实例
    /// \param ctx AST 上下文
    /// \param id 类型变量 ID
    /// \return 类型变量实例
    static TypeVariable* get(ASTContext& ctx, size_t id);
    
private:
    size_t ID;
    Type* ResolvedType;
};

/// \brief 错误类型 !T
class ErrorType : public Type {
public:
    /// 构造函数
    /// \param successType 成功时的类型
    explicit ErrorType(Type* successType)
        : Type(Kind::Error), SuccessType(successType) {}
    
    /// 获取成功时的类型
    Type* getSuccessType() const { return SuccessType; }
    
    bool isEqual(const Type* other) const override {
        if (!other || !other->isError()) {
            return false;
        }
        auto* errorTy = static_cast<const ErrorType*>(other);
        return SuccessType->isEqual(errorTy->SuccessType);
    }
    
    std::string toString() const override {
        return "!" + SuccessType->toString();
    }
    
    size_t getSize() const override {
        // 错误类型通常实现为 Result<T, E>
        // 1 字节标签 + max(T, E) 的大小（对齐）
        // 这里简化为成功类型大小 + 1 字节标签 + 错误信息指针
        size_t successSize = SuccessType->getSize();
        size_t successAlign = SuccessType->getAlignment();
        size_t errorPtrSize = sizeof(void*);
        size_t errorPtrAlign = alignof(void*);
        
        // 计算最大数据大小和对齐
        size_t maxDataSize = std::max(successSize, errorPtrSize);
        size_t maxAlign = std::max(successAlign, errorPtrAlign);
        
        // 标签 + 数据（对齐）
        size_t tagSize = 1;
        size_t alignedTagSize = (tagSize + maxAlign - 1) & ~(maxAlign - 1);
        
        return alignedTagSize + maxDataSize;
    }
    
    size_t getAlignment() const override {
        // 对齐要求是成功类型和错误指针的最大对齐
        return std::max(SuccessType->getAlignment(), alignof(void*));
    }
    
    /// 获取错误类型实例
    /// \param ctx AST 上下文
    /// \param successType 成功时的类型
    /// \return 错误类型实例
    static ErrorType* get(ASTContext& ctx, Type* successType);
    
private:
    Type* SuccessType;
};

/// \brief 模块类型
///
/// 表示一个导入的模块，包含模块中导出的符号
class ModuleType : public Type {
public:
    /// 模块成员（导出的符号）
    struct Member {
        std::string Name;     ///< 成员名称
        Type* MemberType;     ///< 成员类型
        void* Decl;           ///< 指向声明的指针（Decl*）
        std::string LinkName; ///< 可选的外部链接符号（函数/全局）
    };

    /// 构造函数
    /// \param name 模块名称
    /// \param members 模块成员
    ModuleType(std::string name, std::vector<Member> members)
        : Type(Kind::Module), Name(std::move(name)), Members(std::move(members)) {}

    /// 获取模块名称
    const std::string& getName() const { return Name; }

    /// 获取所有成员
    const std::vector<Member>& getMembers() const { return Members; }

    /// 查找成员
    /// \param name 成员名称
    /// \return 成员指针，未找到返回 nullptr
    const Member* getMember(const std::string& name) const {
        for (const auto& member : Members) {
            if (member.Name == name) {
                return &member;
            }
        }
        return nullptr;
    }

    bool isEqual(const Type* other) const override {
        if (!other || !other->isModule()) {
            return false;
        }
        auto* moduleTy = static_cast<const ModuleType*>(other);
        return Name == moduleTy->Name;
    }

    std::string toString() const override {
        return "module(" + Name + ")";
    }

    size_t getSize() const override {
        // 模块类型在运行时不占用空间（编译时概念）
        return 0;
    }

    size_t getAlignment() const override {
        return 1;
    }

    /// 获取模块类型实例
    /// \param ctx AST 上下文
    /// \param name 模块名称
    /// \param members 模块成员
    /// \return 模块类型实例
    static ModuleType* get(ASTContext& ctx, std::string name, std::vector<Member> members);

private:
    std::string Name;
    std::vector<Member> Members;
};

/// \brief 类型别名
class TypeAlias : public Type {
public:
    /// 构造函数
    /// \param name 别名名称
    /// \param aliasedType 被别名的类型
    TypeAlias(std::string name, Type* aliasedType)
        : Type(Kind::TypeAlias), Name(std::move(name)), AliasedType(aliasedType) {}

    /// 获取别名名称
    const std::string& getName() const { return Name; }

    /// 获取被别名的类型
    Type* getAliasedType() const { return AliasedType; }

    /// 解析别名，返回最终的类型
    Type* resolve() const {
        Type* current = AliasedType;
        while (auto* alias = dynamic_cast<TypeAlias*>(current)) {
            current = alias->AliasedType;
        }
        return current;
    }

    bool isEqual(const Type* other) const override {
        // 类型别名比较时需要解析到最终类型
        const Type* rhs = other;
        while (rhs && rhs->isTypeAlias()) {
            rhs = static_cast<const TypeAlias*>(rhs)->getAliasedType();
        }
        return rhs && resolve()->isEqual(rhs);
    }

    std::string toString() const override {
        return Name;
    }

    size_t getSize() const override {
        return resolve()->getSize();
    }

    size_t getAlignment() const override {
        return resolve()->getAlignment();
    }

    /// 获取类型别名实例
    /// \param ctx AST 上下文
    /// \param name 别名名称
    /// \param aliasedType 被别名的类型
    /// \return 类型别名实例
    static TypeAlias* get(ASTContext& ctx, std::string name, Type* aliasedType);

private:
    std::string Name;
    Type* AliasedType;
};

/// \brief 范围类型 (Range)
///
/// 表示范围类型，用于迭代。范围类型包含起始值和结束值，
/// 可以是包含结束值的 (start..=end) 或不包含结束值的 (start..end)。
///
/// 设计决策：
/// - Range 类型在 LLVM IR 中表示为结构体 { T start, T end, i1 inclusive }
/// - 支持整数类型的范围迭代
/// - 实现 Iterator trait，提供 next() 方法
class RangeType : public Type {
public:
    /// 构造函数
    /// \param elementType 范围元素类型（必须是整数类型）
    /// \param isInclusive 是否包含结束值
    RangeType(Type* elementType, bool isInclusive)
        : Type(Kind::Range), ElementType(elementType), IsInclusive(isInclusive) {}

    /// 获取元素类型
    Type* getElementType() const { return ElementType; }

    /// 是否包含结束值
    bool isInclusive() const { return IsInclusive; }

    bool isEqual(const Type* other) const override {
        if (!other || !other->isRange()) {
            return false;
        }
        auto* rangeTy = static_cast<const RangeType*>(other);
        return IsInclusive == rangeTy->IsInclusive &&
               ElementType->isEqual(rangeTy->ElementType);
    }

    std::string toString() const override {
        return "Range<" + ElementType->toString() +
               (IsInclusive ? ", inclusive" : "") + ">";
    }

    size_t getSize() const override {
        // Range 结构体包含：start (T), end (T), inclusive (i1)
        // 布局：{ T, T, i1 } 需要考虑对齐
        size_t elemSize = ElementType->getSize();
        size_t elemAlign = ElementType->getAlignment();

        // start: elemSize
        // end: elemSize (对齐到 elemAlign)
        // inclusive: 1 byte (对齐到 1)
        size_t totalSize = elemSize * 2 + 1;

        // 最终对齐到元素对齐要求
        totalSize = (totalSize + elemAlign - 1) & ~(elemAlign - 1);
        return totalSize;
    }

    size_t getAlignment() const override {
        // 对齐要求与元素类型相同
        return ElementType->getAlignment();
    }

    /// 获取范围类型实例
    /// \param ctx AST 上下文
    /// \param elementType 元素类型
    /// \param isInclusive 是否包含结束值
    /// \return 范围类型实例
    static RangeType* get(ASTContext& ctx, Type* elementType, bool isInclusive);

private:
    Type* ElementType;    ///< 范围元素类型
    bool IsInclusive;     ///< 是否包含结束值
};

} // namespace yuan

#endif // YUAN_SEMA_TYPE_H
