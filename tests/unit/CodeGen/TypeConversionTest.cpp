/// \file TypeConversionTest.cpp
/// \brief Unit tests for CodeGen type conversion.

#include "yuan/CodeGen/CodeGen.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/Sema/Type.h"
#include "yuan/Basic/SourceManager.h"
#include <gtest/gtest.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>

namespace yuan {

/// \brief Test fixture for type conversion tests.
class TypeConversionTest : public ::testing::Test {
protected:
    void SetUp() override {
        SourceMgr = std::make_unique<SourceManager>();
        Ctx = std::make_unique<ASTContext>(*SourceMgr);
        CodeGenerator = std::make_unique<CodeGen>(*Ctx, "test_module");
    }

    std::unique_ptr<SourceManager> SourceMgr;
    std::unique_ptr<ASTContext> Ctx;
    std::unique_ptr<CodeGen> CodeGenerator;
};

// ============================================================================
// Builtin Type Tests
// ============================================================================

TEST_F(TypeConversionTest, VoidType) {
    auto* voidType = Ctx->getVoidType();
    llvm::Type* llvmType = CodeGenerator->getLLVMType(voidType);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isVoidTy());
}

TEST_F(TypeConversionTest, BoolType) {
    Type* boolType = Ctx->getBoolType();
    llvm::Type* llvmType = CodeGenerator->getLLVMType(boolType);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isIntegerTy(1));
}

TEST_F(TypeConversionTest, CharType) {
    Type* charType = Ctx->getCharType();
    llvm::Type* llvmType = CodeGenerator->getLLVMType(charType);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isIntegerTy(8));
}

TEST_F(TypeConversionTest, StringType) {
    Type* stringType = Ctx->getStrType();
    llvm::Type* llvmType = CodeGenerator->getLLVMType(stringType);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isStructTy());

    auto* structType = llvm::cast<llvm::StructType>(llvmType);
    EXPECT_EQ(structType->getNumElements(), 2u);

    // First element: i8* (pointer to characters)
    EXPECT_TRUE(structType->getElementType(0)->isPointerTy());

    // Second element: i64 (length)
    EXPECT_TRUE(structType->getElementType(1)->isIntegerTy(64));
}

TEST_F(TypeConversionTest, IntegerTypes) {
    // Test i8
    Type* i8Type = Ctx->getI8Type();
    llvm::Type* llvmI8 = CodeGenerator->getLLVMType(i8Type);
    ASSERT_NE(llvmI8, nullptr);
    EXPECT_TRUE(llvmI8->isIntegerTy(8));

    // Test i16
    Type* i16Type = Ctx->getI16Type();
    llvm::Type* llvmI16 = CodeGenerator->getLLVMType(i16Type);
    ASSERT_NE(llvmI16, nullptr);
    EXPECT_TRUE(llvmI16->isIntegerTy(16));

    // Test i32
    Type* i32Type = Ctx->getI32Type();
    llvm::Type* llvmI32 = CodeGenerator->getLLVMType(i32Type);
    ASSERT_NE(llvmI32, nullptr);
    EXPECT_TRUE(llvmI32->isIntegerTy(32));

    // Test i64
    Type* i64Type = Ctx->getI64Type();
    llvm::Type* llvmI64 = CodeGenerator->getLLVMType(i64Type);
    ASSERT_NE(llvmI64, nullptr);
    EXPECT_TRUE(llvmI64->isIntegerTy(64));
}

TEST_F(TypeConversionTest, FloatTypes) {
    // Test f32
    Type* f32Type = Ctx->getF32Type();
    llvm::Type* llvmF32 = CodeGenerator->getLLVMType(f32Type);
    ASSERT_NE(llvmF32, nullptr);
    EXPECT_TRUE(llvmF32->isFloatTy());

    // Test f64
    Type* f64Type = Ctx->getF64Type();
    llvm::Type* llvmF64 = CodeGenerator->getLLVMType(f64Type);
    ASSERT_NE(llvmF64, nullptr);
    EXPECT_TRUE(llvmF64->isDoubleTy());
}

// ============================================================================
// Composite Type Tests
// ============================================================================

TEST_F(TypeConversionTest, ArrayType) {
    Type* elemType = Ctx->getI32Type();
    Type* arrayType = Ctx->getArrayType(elemType, 10);
    llvm::Type* llvmType = CodeGenerator->getLLVMType(arrayType);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isArrayTy());

    auto* llvmArrayType = llvm::cast<llvm::ArrayType>(llvmType);
    EXPECT_EQ(llvmArrayType->getNumElements(), 10u);
    EXPECT_TRUE(llvmArrayType->getElementType()->isIntegerTy(32));
}

TEST_F(TypeConversionTest, SliceType) {
    Type* elemType = Ctx->getI32Type();
    Type* sliceType = Ctx->getSliceType(elemType, false);
    llvm::Type* llvmType = CodeGenerator->getLLVMType(sliceType);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isStructTy());

    auto* structType = llvm::cast<llvm::StructType>(llvmType);
    EXPECT_EQ(structType->getNumElements(), 2u);

    // First element: T* (pointer to elements)
    EXPECT_TRUE(structType->getElementType(0)->isPointerTy());

    // Second element: i64 (length)
    EXPECT_TRUE(structType->getElementType(1)->isIntegerTy(64));
}

TEST_F(TypeConversionTest, TupleType) {
    std::vector<Type*> elemTypes = {
        Ctx->getI32Type(),
        Ctx->getBoolType(),
        Ctx->getF64Type()
    };

    Type* tupleType = Ctx->getTupleType(elemTypes);
    llvm::Type* llvmType = CodeGenerator->getLLVMType(tupleType);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isStructTy());

    auto* structType = llvm::cast<llvm::StructType>(llvmType);
    EXPECT_EQ(structType->getNumElements(), 3u);

    EXPECT_TRUE(structType->getElementType(0)->isIntegerTy(32));
    EXPECT_TRUE(structType->getElementType(1)->isIntegerTy(1));
    EXPECT_TRUE(structType->getElementType(2)->isDoubleTy());
}

TEST_F(TypeConversionTest, EmptyTuple) {
    std::vector<Type*> elemTypes;
    Type* emptyTuple = Ctx->getTupleType(elemTypes);
    llvm::Type* llvmType = CodeGenerator->getLLVMType(emptyTuple);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isStructTy());

    auto* structType = llvm::cast<llvm::StructType>(llvmType);
    EXPECT_EQ(structType->getNumElements(), 0u);
}

// ============================================================================
// Pointer and Reference Type Tests
// ============================================================================

TEST_F(TypeConversionTest, PointerType) {
    Type* pointeeType = Ctx->getI32Type();
    Type* ptrType = Ctx->getPointerType(pointeeType, false);
    llvm::Type* llvmType = CodeGenerator->getLLVMType(ptrType);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isPointerTy());
}

TEST_F(TypeConversionTest, ReferenceType) {
    Type* referencedType = Ctx->getI32Type();
    Type* refType = Ctx->getReferenceType(referencedType, false);
    llvm::Type* llvmType = CodeGenerator->getLLVMType(refType);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isPointerTy());
}

TEST_F(TypeConversionTest, MutableReferenceType) {
    Type* referencedType = Ctx->getI32Type();
    Type* mutRefType = Ctx->getReferenceType(referencedType, true);
    llvm::Type* llvmType = CodeGenerator->getLLVMType(mutRefType);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isPointerTy());
}

TEST_F(TypeConversionTest, PointerToPointer) {
    Type* baseType = Ctx->getI32Type();
    Type* ptrType = Ctx->getPointerType(baseType, false);
    Type* ptrPtrType = Ctx->getPointerType(ptrType, false);
    llvm::Type* llvmType = CodeGenerator->getLLVMType(ptrPtrType);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isPointerTy());
}

// ============================================================================
// Function Type Tests
// ============================================================================

TEST_F(TypeConversionTest, SimpleFunctionType) {
    Type* returnType = Ctx->getI32Type();
    std::vector<Type*> paramTypes = {
        Ctx->getI32Type(),
        Ctx->getBoolType()
    };

    Type* funcType = Ctx->getFunctionType(paramTypes, returnType, false);
    llvm::Type* llvmType = CodeGenerator->getLLVMType(funcType);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isFunctionTy());

    auto* llvmFuncType = llvm::cast<llvm::FunctionType>(llvmType);
    EXPECT_TRUE(llvmFuncType->getReturnType()->isIntegerTy(32));
    EXPECT_EQ(llvmFuncType->getNumParams(), 2u);
    EXPECT_TRUE(llvmFuncType->getParamType(0)->isIntegerTy(32));
    EXPECT_TRUE(llvmFuncType->getParamType(1)->isIntegerTy(1));
}

TEST_F(TypeConversionTest, VoidFunctionType) {
    Type* returnType = Ctx->getVoidType();
    std::vector<Type*> paramTypes;

    Type* funcType = Ctx->getFunctionType(paramTypes, returnType, false);
    llvm::Type* llvmType = CodeGenerator->getLLVMType(funcType);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isFunctionTy());

    auto* llvmFuncType = llvm::cast<llvm::FunctionType>(llvmType);
    EXPECT_TRUE(llvmFuncType->getReturnType()->isVoidTy());
    EXPECT_EQ(llvmFuncType->getNumParams(), 0u);
}

// ============================================================================
// Struct Type Tests
// ============================================================================

TEST_F(TypeConversionTest, SimpleStructType) {
    std::vector<Type*> fieldTypes = {
        Ctx->getI32Type(),
        Ctx->getI32Type(),
        Ctx->getBoolType()
    };
    std::vector<std::string> fieldNames = {"x", "y", "active"};

    Type* structType = Ctx->getStructType("Point", fieldTypes, fieldNames);
    llvm::Type* llvmType = CodeGenerator->getLLVMType(structType);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isStructTy());

    auto* llvmStructType = llvm::cast<llvm::StructType>(llvmType);
    EXPECT_EQ(llvmStructType->getName(), "Point");
    EXPECT_EQ(llvmStructType->getNumElements(), 3u);
    EXPECT_TRUE(llvmStructType->getElementType(0)->isIntegerTy(32));
    EXPECT_TRUE(llvmStructType->getElementType(1)->isIntegerTy(32));
    EXPECT_TRUE(llvmStructType->getElementType(2)->isIntegerTy(1));
}

TEST_F(TypeConversionTest, NestedStructType) {
    std::vector<Type*> innerFieldTypes = {
        Ctx->getI32Type(),
        Ctx->getI32Type()
    };
    std::vector<std::string> innerFieldNames = {"x", "y"};

    Type* innerStruct = Ctx->getStructType("Point", innerFieldTypes, innerFieldNames);

    std::vector<Type*> outerFieldTypes = {
        innerStruct,
        innerStruct
    };
    std::vector<std::string> outerFieldNames = {"topLeft", "bottomRight"};

    Type* rectStruct = Ctx->getStructType("Rectangle", outerFieldTypes, outerFieldNames);
    llvm::Type* llvmType = CodeGenerator->getLLVMType(rectStruct);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isStructTy());

    auto* llvmStructType = llvm::cast<llvm::StructType>(llvmType);
    EXPECT_EQ(llvmStructType->getNumElements(), 2u);
    EXPECT_TRUE(llvmStructType->getElementType(0)->isStructTy());
    EXPECT_TRUE(llvmStructType->getElementType(1)->isStructTy());
}

TEST_F(TypeConversionTest, EmptyStructType) {
    std::vector<Type*> fieldTypes;
    std::vector<std::string> fieldNames;
    Type* emptyStruct = Ctx->getStructType("Empty", fieldTypes, fieldNames);
    llvm::Type* llvmType = CodeGenerator->getLLVMType(emptyStruct);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isStructTy());

    auto* llvmStructType = llvm::cast<llvm::StructType>(llvmType);
    EXPECT_EQ(llvmStructType->getNumElements(), 0u);
}

// ============================================================================
// Enum Type Tests
// ============================================================================

TEST_F(TypeConversionTest, SimpleEnumType) {
    std::vector<Type*> variantDataTypes;
    std::vector<std::string> variantNames = {"Red", "Green", "Blue"};

    Type* enumType = Ctx->getEnumType("Color", variantDataTypes, variantNames);
    llvm::Type* llvmType = CodeGenerator->getLLVMType(enumType);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isStructTy());

    auto* structType = llvm::cast<llvm::StructType>(llvmType);
    EXPECT_EQ(structType->getNumElements(), 2u);

    // First element: i32 (tag)
    EXPECT_TRUE(structType->getElementType(0)->isIntegerTy(32));

    // Second element: i8* (data pointer)
    EXPECT_TRUE(structType->getElementType(1)->isPointerTy());
}

TEST_F(TypeConversionTest, EnumWithData) {
    std::vector<Type*> variantDataTypes = {
        Ctx->getI32Type()
    };
    std::vector<std::string> variantNames = {"None", "Some"};

    Type* optionType = Ctx->getEnumType("Option", variantDataTypes, variantNames);
    llvm::Type* llvmType = CodeGenerator->getLLVMType(optionType);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isStructTy());

    auto* structType = llvm::cast<llvm::StructType>(llvmType);
    EXPECT_EQ(structType->getNumElements(), 2u);
    EXPECT_TRUE(structType->getElementType(0)->isIntegerTy(32));
    EXPECT_TRUE(structType->getElementType(1)->isPointerTy());
}

// ============================================================================
// Type Caching Tests
// ============================================================================

TEST_F(TypeConversionTest, TypeCaching) {
    Type* intType = Ctx->getI32Type();

    // Convert the same type twice
    llvm::Type* llvmType1 = CodeGenerator->getLLVMType(intType);
    llvm::Type* llvmType2 = CodeGenerator->getLLVMType(intType);

    // Should return the same cached instance
    EXPECT_EQ(llvmType1, llvmType2);
}

TEST_F(TypeConversionTest, StructTypeCaching) {
    std::vector<Type*> fieldTypes = {Ctx->getI32Type()};
    std::vector<std::string> fieldNames = {"value"};

    Type* structType = Ctx->getStructType("TestStruct", fieldTypes, fieldNames);

    llvm::Type* llvmType1 = CodeGenerator->getLLVMType(structType);
    llvm::Type* llvmType2 = CodeGenerator->getLLVMType(structType);

    EXPECT_EQ(llvmType1, llvmType2);
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST_F(TypeConversionTest, NullType) {
    llvm::Type* llvmType = CodeGenerator->getLLVMType(nullptr);
    EXPECT_EQ(llvmType, nullptr);
}

TEST_F(TypeConversionTest, ComplexNestedType) {
    // Create a complex type: [10](*func(i32, bool) -> i64)
    Type* returnType = Ctx->getI64Type();
    std::vector<Type*> paramTypes = {
        Ctx->getI32Type(),
        Ctx->getBoolType()
    };

    Type* funcType = Ctx->getFunctionType(paramTypes, returnType, false);
    Type* ptrType = Ctx->getPointerType(funcType, false);
    Type* arrayType = Ctx->getArrayType(ptrType, 10);

    llvm::Type* llvmType = CodeGenerator->getLLVMType(arrayType);

    ASSERT_NE(llvmType, nullptr);
    EXPECT_TRUE(llvmType->isArrayTy());

    auto* llvmArrayType = llvm::cast<llvm::ArrayType>(llvmType);
    EXPECT_EQ(llvmArrayType->getNumElements(), 10u);
    EXPECT_TRUE(llvmArrayType->getElementType()->isPointerTy());
}

} // namespace yuan
