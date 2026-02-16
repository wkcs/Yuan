/// \file
/// \brief Yuan 类型系统单元测试

#include "yuan/Sema/Type.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/Basic/SourceManager.h"
#include <gtest/gtest.h>
#include <memory>

using namespace yuan;

class TypeTest : public ::testing::Test {
protected:
    void SetUp() override {
        SM = std::make_unique<SourceManager>();
        Ctx = std::make_unique<ASTContext>(*SM);
    }

    std::unique_ptr<SourceManager> SM;
    std::unique_ptr<ASTContext> Ctx;
};

// 基本类型测试
TEST_F(TypeTest, BasicTypes) {
    // 测试 void 类型
    auto* voidTy = VoidType::get(*Ctx);
    EXPECT_TRUE(voidTy->isVoid());
    EXPECT_EQ(voidTy->toString(), "void");
    EXPECT_EQ(voidTy->getSize(), 0);
    EXPECT_EQ(voidTy->getAlignment(), 1);
    
    // 测试 bool 类型
    auto* boolTy = BoolType::get(*Ctx);
    EXPECT_TRUE(boolTy->isBool());
    EXPECT_EQ(boolTy->toString(), "bool");
    EXPECT_EQ(boolTy->getSize(), 1);
    EXPECT_EQ(boolTy->getAlignment(), 1);
    
    // 测试 char 类型
    auto* charTy = CharType::get(*Ctx);
    EXPECT_TRUE(charTy->isChar());
    EXPECT_EQ(charTy->toString(), "char");
    EXPECT_EQ(charTy->getSize(), 4); // UTF-32
    EXPECT_EQ(charTy->getAlignment(), 4);
    
    // 测试 str 类型
    auto* strTy = StringType::get(*Ctx);
    EXPECT_TRUE(strTy->isString());
    EXPECT_EQ(strTy->toString(), "str");
    EXPECT_EQ(strTy->getSize(), sizeof(void*) + sizeof(size_t));
    EXPECT_EQ(strTy->getAlignment(), alignof(void*));
}

// 整数类型测试
TEST_F(TypeTest, IntegerTypes) {
    // 测试有符号整数
    auto* i32Ty = IntegerType::get(*Ctx, 32, true);
    EXPECT_TRUE(i32Ty->isInteger());
    EXPECT_TRUE(i32Ty->isSigned());
    EXPECT_EQ(i32Ty->getBitWidth(), 32);
    EXPECT_EQ(i32Ty->toString(), "i32");
    EXPECT_EQ(i32Ty->getSize(), 4);
    EXPECT_EQ(i32Ty->getAlignment(), 4);
    
    // 测试无符号整数
    auto* u64Ty = IntegerType::get(*Ctx, 64, false);
    EXPECT_TRUE(u64Ty->isInteger());
    EXPECT_FALSE(u64Ty->isSigned());
    EXPECT_EQ(u64Ty->getBitWidth(), 64);
    EXPECT_EQ(u64Ty->toString(), "u64");
    EXPECT_EQ(u64Ty->getSize(), 8);
    EXPECT_EQ(u64Ty->getAlignment(), 8);
    
    // 测试类型相等性
    auto* i32Ty2 = IntegerType::get(*Ctx, 32, true);
    EXPECT_TRUE(i32Ty->isEqual(i32Ty2));
    EXPECT_FALSE(i32Ty->isEqual(u64Ty));
    
    // 测试快捷方式
    auto* i8Ty = Ctx->getI8Type();
    EXPECT_EQ(i8Ty->toString(), "i8");
    auto* u32Ty = Ctx->getU32Type();
    EXPECT_EQ(u32Ty->toString(), "u32");
}

// 浮点类型测试
TEST_F(TypeTest, FloatTypes) {
    auto* f32Ty = FloatType::get(*Ctx, 32);
    EXPECT_TRUE(f32Ty->isFloat());
    EXPECT_EQ(f32Ty->getBitWidth(), 32);
    EXPECT_EQ(f32Ty->toString(), "f32");
    EXPECT_EQ(f32Ty->getSize(), 4);
    EXPECT_EQ(f32Ty->getAlignment(), 4);
    
    auto* f64Ty = FloatType::get(*Ctx, 64);
    EXPECT_TRUE(f64Ty->isFloat());
    EXPECT_EQ(f64Ty->getBitWidth(), 64);
    EXPECT_EQ(f64Ty->toString(), "f64");
    EXPECT_EQ(f64Ty->getSize(), 8);
    EXPECT_EQ(f64Ty->getAlignment(), 8);
    
    // 测试类型相等性
    auto* f32Ty2 = FloatType::get(*Ctx, 32);
    EXPECT_TRUE(f32Ty->isEqual(f32Ty2));
    EXPECT_FALSE(f32Ty->isEqual(f64Ty));
}

// 数组类型测试
TEST_F(TypeTest, ArrayTypes) {
    auto* i32Ty = Ctx->getI32Type();
    auto* arrayTy = ArrayType::get(*Ctx, i32Ty, 10);
    
    EXPECT_TRUE(arrayTy->isArray());
    EXPECT_EQ(arrayTy->getElementType(), i32Ty);
    EXPECT_EQ(arrayTy->getArraySize(), 10);
    EXPECT_EQ(arrayTy->toString(), "[i32; 10]");
    EXPECT_EQ(arrayTy->getSize(), 40); // 4 * 10
    EXPECT_EQ(arrayTy->getAlignment(), 4);
    
    // 测试类型相等性
    auto* arrayTy2 = ArrayType::get(*Ctx, i32Ty, 10);
    auto* arrayTy3 = ArrayType::get(*Ctx, i32Ty, 5);
    EXPECT_TRUE(arrayTy->isEqual(arrayTy2));
    EXPECT_FALSE(arrayTy->isEqual(arrayTy3));
}

// 切片类型测试
TEST_F(TypeTest, SliceTypes) {
    auto* i32Ty = Ctx->getI32Type();
    auto* sliceTy = SliceType::get(*Ctx, i32Ty, false);
    auto* mutSliceTy = SliceType::get(*Ctx, i32Ty, true);
    
    EXPECT_TRUE(sliceTy->isSlice());
    EXPECT_EQ(sliceTy->getElementType(), i32Ty);
    EXPECT_FALSE(sliceTy->isMutable());
    EXPECT_EQ(sliceTy->toString(), "&[i32]");
    EXPECT_EQ(sliceTy->getSize(), sizeof(void*) + sizeof(size_t));
    
    EXPECT_TRUE(mutSliceTy->isMutable());
    EXPECT_EQ(mutSliceTy->toString(), "&mut [i32]");
    
    // 测试类型相等性
    EXPECT_FALSE(sliceTy->isEqual(mutSliceTy));
}

// 元组类型测试
TEST_F(TypeTest, TupleTypes) {
    auto* i32Ty = Ctx->getI32Type();
    auto* f64Ty = Ctx->getF64Type();
    auto* boolTy = Ctx->getBoolType();
    
    // 空元组
    auto* emptyTupleTy = TupleType::get(*Ctx, {});
    EXPECT_TRUE(emptyTupleTy->isTuple());
    EXPECT_EQ(emptyTupleTy->getElementCount(), 0);
    EXPECT_EQ(emptyTupleTy->toString(), "()");
    
    // 非空元组
    std::vector<Type*> elements = {i32Ty, f64Ty, boolTy};
    auto* tupleTy = TupleType::get(*Ctx, elements);
    EXPECT_EQ(tupleTy->getElementCount(), 3);
    EXPECT_EQ(tupleTy->getElement(0), i32Ty);
    EXPECT_EQ(tupleTy->getElement(1), f64Ty);
    EXPECT_EQ(tupleTy->getElement(2), boolTy);
    EXPECT_EQ(tupleTy->toString(), "(i32, f64, bool)");
    
    // 测试大小和对齐（考虑填充）
    EXPECT_GT(tupleTy->getSize(), 0);
    EXPECT_GE(tupleTy->getAlignment(), 1);
}

// 可选类型测试
TEST_F(TypeTest, OptionalTypes) {
    auto* i32Ty = Ctx->getI32Type();
    auto* optTy = OptionalType::get(*Ctx, i32Ty);
    
    EXPECT_TRUE(optTy->isOptional());
    EXPECT_EQ(optTy->getInnerType(), i32Ty);
    EXPECT_EQ(optTy->toString(), "?i32");
    EXPECT_GT(optTy->getSize(), i32Ty->getSize()); // 包含标签
    
    // 测试类型相等性
    auto* optTy2 = OptionalType::get(*Ctx, i32Ty);
    auto* optF64Ty = OptionalType::get(*Ctx, Ctx->getF64Type());
    EXPECT_TRUE(optTy->isEqual(optTy2));
    EXPECT_FALSE(optTy->isEqual(optF64Ty));
}

// 引用类型测试
TEST_F(TypeTest, ReferenceTypes) {
    auto* i32Ty = Ctx->getI32Type();
    auto* refTy = ReferenceType::get(*Ctx, i32Ty, false);
    auto* mutRefTy = ReferenceType::get(*Ctx, i32Ty, true);
    
    EXPECT_TRUE(refTy->isReference());
    EXPECT_EQ(refTy->getPointeeType(), i32Ty);
    EXPECT_FALSE(refTy->isMutable());
    EXPECT_EQ(refTy->toString(), "&i32");
    EXPECT_EQ(refTy->getSize(), sizeof(void*));
    
    EXPECT_TRUE(mutRefTy->isMutable());
    EXPECT_EQ(mutRefTy->toString(), "&mut i32");
    
    // 测试类型相等性
    EXPECT_FALSE(refTy->isEqual(mutRefTy));
}

// 指针类型测试
TEST_F(TypeTest, PointerTypes) {
    auto* i32Ty = Ctx->getI32Type();
    auto* ptrTy = PointerType::get(*Ctx, i32Ty, false);
    auto* mutPtrTy = PointerType::get(*Ctx, i32Ty, true);
    
    EXPECT_TRUE(ptrTy->isPointer());
    EXPECT_EQ(ptrTy->getPointeeType(), i32Ty);
    EXPECT_FALSE(ptrTy->isMutable());
    EXPECT_EQ(ptrTy->toString(), "*i32");
    EXPECT_EQ(ptrTy->getSize(), sizeof(void*));
    
    EXPECT_TRUE(mutPtrTy->isMutable());
    EXPECT_EQ(mutPtrTy->toString(), "*mut i32");
    
    // 测试类型相等性
    EXPECT_FALSE(ptrTy->isEqual(mutPtrTy));
}

// 函数类型测试
TEST_F(TypeTest, FunctionTypes) {
    auto* i32Ty = Ctx->getI32Type();
    auto* f64Ty = Ctx->getF64Type();
    auto* voidTy = Ctx->getVoidType();
    
    // 无参数函数
    auto* funcTy1 = FunctionType::get(*Ctx, {}, voidTy, false);
    EXPECT_TRUE(funcTy1->isFunction());
    EXPECT_EQ(funcTy1->getParamCount(), 0);
    EXPECT_EQ(funcTy1->getReturnType(), voidTy);
    EXPECT_FALSE(funcTy1->canError());
    EXPECT_EQ(funcTy1->toString(), "func() -> void");
    
    // 有参数函数
    std::vector<Type*> params = {i32Ty, f64Ty};
    auto* funcTy2 = FunctionType::get(*Ctx, params, i32Ty, false);
    EXPECT_EQ(funcTy2->getParamCount(), 2);
    EXPECT_EQ(funcTy2->getParam(0), i32Ty);
    EXPECT_EQ(funcTy2->getParam(1), f64Ty);
    EXPECT_EQ(funcTy2->toString(), "func(i32, f64) -> i32");
    
    // 可能出错的函数
    auto* funcTy3 = FunctionType::get(*Ctx, {i32Ty}, i32Ty, true);
    EXPECT_TRUE(funcTy3->canError());
    EXPECT_EQ(funcTy3->toString(), "func(i32) -> !i32");
}

// 结构体类型测试
TEST_F(TypeTest, StructTypes) {
    auto* i32Ty = Ctx->getI32Type();
    auto* f64Ty = Ctx->getF64Type();
    
    std::vector<StructType::Field> fields;
    fields.emplace_back("x", i32Ty, 0);
    fields.emplace_back("y", f64Ty, 8); // 考虑对齐
    
    auto* structTy = StructType::get(*Ctx, "Point", std::move(fields));
    EXPECT_TRUE(structTy->isStruct());
    EXPECT_EQ(structTy->getName(), "Point");
    EXPECT_EQ(structTy->getFieldCount(), 2);
    EXPECT_EQ(structTy->toString(), "Point");
    
    // 测试字段查找
    auto* xField = structTy->getField("x");
    ASSERT_NE(xField, nullptr);
    EXPECT_EQ(xField->Name, "x");
    EXPECT_EQ(xField->FieldType, i32Ty);
    
    auto* yField = structTy->getField("y");
    ASSERT_NE(yField, nullptr);
    EXPECT_EQ(yField->Name, "y");
    EXPECT_EQ(yField->FieldType, f64Ty);
    
    // 测试大小和对齐
    EXPECT_GT(structTy->getSize(), 0);
    EXPECT_GE(structTy->getAlignment(), 1);
}

// 枚举类型测试
TEST_F(TypeTest, EnumTypes) {
    auto* i32Ty = Ctx->getI32Type();
    auto* strTy = Ctx->getStrType();
    
    std::vector<EnumType::Variant> variants;
    variants.emplace_back("None", std::vector<Type*>{}, 0);
    variants.emplace_back("Some", std::vector<Type*>{i32Ty}, 1);
    variants.emplace_back("Error", std::vector<Type*>{strTy}, 2);
    
    auto* enumTy = EnumType::get(*Ctx, "Option", std::move(variants));
    EXPECT_TRUE(enumTy->isEnum());
    EXPECT_EQ(enumTy->getName(), "Option");
    EXPECT_EQ(enumTy->getVariantCount(), 3);
    EXPECT_EQ(enumTy->toString(), "Option");
    
    // 测试变体查找
    auto* noneVariant = enumTy->getVariant("None");
    ASSERT_NE(noneVariant, nullptr);
    EXPECT_EQ(noneVariant->Name, "None");
    EXPECT_TRUE(noneVariant->Data.empty());
    
    auto* someVariant = enumTy->getVariant("Some");
    ASSERT_NE(someVariant, nullptr);
    EXPECT_EQ(someVariant->Name, "Some");
    EXPECT_EQ(someVariant->Data.size(), 1);
    EXPECT_EQ(someVariant->Data[0], i32Ty);
    
    // 测试大小和对齐
    EXPECT_GT(enumTy->getSize(), 0);
    EXPECT_GE(enumTy->getAlignment(), 1);
}

// Trait 类型测试
TEST_F(TypeTest, TraitTypes) {
    auto* traitTy = TraitType::get(*Ctx, "Display");
    EXPECT_TRUE(traitTy->isTrait());
    EXPECT_EQ(traitTy->getName(), "Display");
    EXPECT_EQ(traitTy->toString(), "Display");
    EXPECT_EQ(traitTy->getSize(), sizeof(void*) * 2); // fat pointer
    
    // 测试类型相等性
    auto* traitTy2 = TraitType::get(*Ctx, "Display");
    auto* traitTy3 = TraitType::get(*Ctx, "Debug");
    EXPECT_TRUE(traitTy->isEqual(traitTy2));
    EXPECT_FALSE(traitTy->isEqual(traitTy3));
}

// 泛型类型测试
TEST_F(TypeTest, GenericTypes) {
    auto* traitTy = TraitType::get(*Ctx, "Display");
    auto* genericTy = GenericType::get(*Ctx, "T", {traitTy});
    
    EXPECT_TRUE(genericTy->isGeneric());
    EXPECT_EQ(genericTy->getName(), "T");
    EXPECT_EQ(genericTy->getConstraints().size(), 1);
    EXPECT_EQ(genericTy->getConstraints()[0], traitTy);
    EXPECT_EQ(genericTy->toString(), "T: Display");
    
    // 无约束的泛型类型
    auto* genericTy2 = GenericType::get(*Ctx, "U");
    EXPECT_EQ(genericTy2->toString(), "U");
}

// 类型变量测试
TEST_F(TypeTest, TypeVariables) {
    auto* typeVar1 = TypeVariable::get(*Ctx, 0);
    auto* typeVar2 = TypeVariable::get(*Ctx, 1);
    
    EXPECT_TRUE(typeVar1->isTypeVar());
    EXPECT_EQ(typeVar1->getID(), 0);
    EXPECT_FALSE(typeVar1->isResolved());
    EXPECT_EQ(typeVar1->toString(), "?0");
    
    EXPECT_EQ(typeVar2->getID(), 1);
    EXPECT_EQ(typeVar2->toString(), "?1");
    
    // 测试解析
    auto* i32Ty = Ctx->getI32Type();
    typeVar1->setResolvedType(i32Ty);
    EXPECT_TRUE(typeVar1->isResolved());
    EXPECT_EQ(typeVar1->getResolvedType(), i32Ty);
    EXPECT_EQ(typeVar1->toString(), "i32");
    EXPECT_EQ(typeVar1->getSize(), i32Ty->getSize());
    
    // 测试自动创建类型变量
    auto* typeVar3 = Ctx->createTypeVariable();
    EXPECT_TRUE(typeVar3->isTypeVar());
    EXPECT_GE(typeVar3->getID(), 0);
}

// 错误类型测试
TEST_F(TypeTest, ErrorTypes) {
    auto* i32Ty = Ctx->getI32Type();
    auto* errorTy = ErrorType::get(*Ctx, i32Ty);
    
    EXPECT_TRUE(errorTy->isError());
    EXPECT_EQ(errorTy->getSuccessType(), i32Ty);
    EXPECT_EQ(errorTy->toString(), "!i32");
    EXPECT_GT(errorTy->getSize(), i32Ty->getSize()); // 包含标签和错误信息
    
    // 测试类型相等性
    auto* errorTy2 = ErrorType::get(*Ctx, i32Ty);
    auto* errorF64Ty = ErrorType::get(*Ctx, Ctx->getF64Type());
    EXPECT_TRUE(errorTy->isEqual(errorTy2));
    EXPECT_FALSE(errorTy->isEqual(errorF64Ty));
}

// 类型别名测试
TEST_F(TypeTest, TypeAliases) {
    auto* i32Ty = Ctx->getI32Type();
    auto* aliasTy = TypeAlias::get(*Ctx, "MyInt", i32Ty);
    
    EXPECT_TRUE(aliasTy->isTypeAlias());
    EXPECT_EQ(aliasTy->getName(), "MyInt");
    EXPECT_EQ(aliasTy->getAliasedType(), i32Ty);
    EXPECT_EQ(aliasTy->resolve(), i32Ty);
    EXPECT_EQ(aliasTy->toString(), "MyInt");
    EXPECT_EQ(aliasTy->getSize(), i32Ty->getSize());
    
    // 测试链式别名
    auto* aliasTy2 = TypeAlias::get(*Ctx, "MyInt2", aliasTy);
    EXPECT_EQ(aliasTy2->resolve(), i32Ty);
    
    // 测试类型相等性（解析后比较）
    EXPECT_TRUE(aliasTy->isEqual(i32Ty));
    EXPECT_TRUE(aliasTy2->isEqual(i32Ty));
}

// 类型缓存测试
TEST_F(TypeTest, TypeCaching) {
    // 测试相同参数返回相同实例
    auto* i32Ty1 = Ctx->getI32Type();
    auto* i32Ty2 = Ctx->getI32Type();
    EXPECT_EQ(i32Ty1, i32Ty2); // 应该是同一个实例
    
    auto* arrayTy1 = ArrayType::get(*Ctx, i32Ty1, 10);
    auto* arrayTy2 = ArrayType::get(*Ctx, i32Ty1, 10);
    EXPECT_EQ(arrayTy1, arrayTy2); // 应该是同一个实例
    
    auto* arrayTy3 = ArrayType::get(*Ctx, i32Ty1, 5);
    EXPECT_NE(arrayTy1, arrayTy3); // 不同大小，应该是不同实例
}