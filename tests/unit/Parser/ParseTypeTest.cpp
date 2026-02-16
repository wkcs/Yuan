/// \file ParseTypeTest.cpp
/// \brief 类型解析单元测试。
///
/// 本文件测试 Parser 类中与类型解析相关的功能，
/// 包括内置类型、数组、元组、引用、指针、函数类型等。

#include <gtest/gtest.h>
#include "yuan/Parser/Parser.h"
#include "yuan/Lexer/Lexer.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Type.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/TextDiagnosticPrinter.h"
#include <sstream>

namespace yuan {

/// \brief 类型解析测试夹具
class ParseTypeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建诊断输出流
        diagStream = std::make_unique<std::ostringstream>();
        
        // 创建诊断系统
        diag = std::make_unique<DiagnosticEngine>(sm);
        printer = std::make_unique<TextDiagnosticPrinter>(*diagStream, sm, false);
        diag->setConsumer(std::move(printer));
        
        // 创建 AST 上下文
        ctx = std::make_unique<ASTContext>(sm);
    }
    
    /// \brief 解析类型字符串
    /// \param source 源代码字符串
    /// \return 解析结果
    ParseResult<TypeNode> parseTypeString(const std::string& source) {
        auto fileID = sm.createBuffer(source, "<test>");
        auto lexer = std::make_unique<Lexer>(sm, *diag, fileID);
        Parser parser(*lexer, *diag, *ctx);
        
        return parser.parseType();
    }
    
    /// \brief 检查是否有错误
    bool hasError() const {
        return diag->hasErrors();
    }
    
    /// \brief 获取错误信息
    std::string getErrors() const {
        return diagStream->str();
    }
    
    SourceManager sm;
    std::unique_ptr<std::ostringstream> diagStream;
    std::unique_ptr<DiagnosticEngine> diag;
    std::unique_ptr<TextDiagnosticPrinter> printer;
    std::unique_ptr<ASTContext> ctx;
};

// ============================================================================
// 内置类型测试
// ============================================================================

TEST_F(ParseTypeTest, ParseBuiltinTypes) {
    // 测试所有内置类型
    struct TestCase {
        std::string source;
        BuiltinTypeNode::BuiltinKind expectedKind;
    };
    
    std::vector<TestCase> testCases = {
        {"void", BuiltinTypeNode::BuiltinKind::Void},
        {"bool", BuiltinTypeNode::BuiltinKind::Bool},
        {"char", BuiltinTypeNode::BuiltinKind::Char},
        {"str", BuiltinTypeNode::BuiltinKind::Str},
        {"i8", BuiltinTypeNode::BuiltinKind::I8},
        {"i16", BuiltinTypeNode::BuiltinKind::I16},
        {"i32", BuiltinTypeNode::BuiltinKind::I32},
        {"i64", BuiltinTypeNode::BuiltinKind::I64},
        {"i128", BuiltinTypeNode::BuiltinKind::I128},
        {"isize", BuiltinTypeNode::BuiltinKind::ISize},
        {"u8", BuiltinTypeNode::BuiltinKind::U8},
        {"u16", BuiltinTypeNode::BuiltinKind::U16},
        {"u32", BuiltinTypeNode::BuiltinKind::U32},
        {"u64", BuiltinTypeNode::BuiltinKind::U64},
        {"u128", BuiltinTypeNode::BuiltinKind::U128},
        {"usize", BuiltinTypeNode::BuiltinKind::USize},
        {"f32", BuiltinTypeNode::BuiltinKind::F32},
        {"f64", BuiltinTypeNode::BuiltinKind::F64},
    };
    
    for (const auto& testCase : testCases) {
        auto result = parseTypeString(testCase.source);
        ASSERT_TRUE(result.isSuccess()) << "Failed to parse: " << testCase.source;
        ASSERT_FALSE(hasError()) << "Unexpected error: " << getErrors();
        
        auto* builtinType = dynamic_cast<BuiltinTypeNode*>(result.get());
        ASSERT_NE(builtinType, nullptr) << "Expected BuiltinTypeNode for: " << testCase.source;
        EXPECT_EQ(builtinType->getBuiltinKind(), testCase.expectedKind);
    }
}

// ============================================================================
// 标识符类型测试
// ============================================================================

TEST_F(ParseTypeTest, ParseIdentifierType) {
    auto result = parseTypeString("MyType");
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* identType = dynamic_cast<IdentifierTypeNode*>(result.get());
    ASSERT_NE(identType, nullptr);
    EXPECT_EQ(identType->getName(), "MyType");
}

TEST_F(ParseTypeTest, ParseSelfType) {
    auto result = parseTypeString("Self");
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* identType = dynamic_cast<IdentifierTypeNode*>(result.get());
    ASSERT_NE(identType, nullptr);
    EXPECT_EQ(identType->getName(), "Self");
}

// ============================================================================
// 引用类型测试
// ============================================================================

TEST_F(ParseTypeTest, ParseReferenceType) {
    // 不可变引用
    auto result1 = parseTypeString("&i32");
    ASSERT_TRUE(result1.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* refType1 = dynamic_cast<ReferenceTypeNode*>(result1.get());
    ASSERT_NE(refType1, nullptr);
    EXPECT_FALSE(refType1->isMutable());
    
    auto* pointeeType1 = dynamic_cast<BuiltinTypeNode*>(refType1->getPointeeType());
    ASSERT_NE(pointeeType1, nullptr);
    EXPECT_EQ(pointeeType1->getBuiltinKind(), BuiltinTypeNode::BuiltinKind::I32);
    
    // 可变引用
    auto result2 = parseTypeString("&mut str");
    ASSERT_TRUE(result2.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* refType2 = dynamic_cast<ReferenceTypeNode*>(result2.get());
    ASSERT_NE(refType2, nullptr);
    EXPECT_TRUE(refType2->isMutable());
    
    auto* pointeeType2 = dynamic_cast<BuiltinTypeNode*>(refType2->getPointeeType());
    ASSERT_NE(pointeeType2, nullptr);
    EXPECT_EQ(pointeeType2->getBuiltinKind(), BuiltinTypeNode::BuiltinKind::Str);
}

// ============================================================================
// 指针类型测试
// ============================================================================

TEST_F(ParseTypeTest, ParsePointerType) {
    // 不可变指针
    auto result1 = parseTypeString("*i32");
    ASSERT_TRUE(result1.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* ptrType1 = dynamic_cast<PointerTypeNode*>(result1.get());
    ASSERT_NE(ptrType1, nullptr);
    EXPECT_FALSE(ptrType1->isMutable());
    
    auto* pointeeType1 = dynamic_cast<BuiltinTypeNode*>(ptrType1->getPointeeType());
    ASSERT_NE(pointeeType1, nullptr);
    EXPECT_EQ(pointeeType1->getBuiltinKind(), BuiltinTypeNode::BuiltinKind::I32);
    
    // 可变指针
    auto result2 = parseTypeString("*mut u8");
    ASSERT_TRUE(result2.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* ptrType2 = dynamic_cast<PointerTypeNode*>(result2.get());
    ASSERT_NE(ptrType2, nullptr);
    EXPECT_TRUE(ptrType2->isMutable());
    
    auto* pointeeType2 = dynamic_cast<BuiltinTypeNode*>(ptrType2->getPointeeType());
    ASSERT_NE(pointeeType2, nullptr);
    EXPECT_EQ(pointeeType2->getBuiltinKind(), BuiltinTypeNode::BuiltinKind::U8);
}

// ============================================================================
// Optional 类型测试
// ============================================================================

TEST_F(ParseTypeTest, ParseOptionalType) {
    auto result = parseTypeString("?i32");
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* optType = dynamic_cast<OptionalTypeNode*>(result.get());
    ASSERT_NE(optType, nullptr);
    
    auto* innerType = dynamic_cast<BuiltinTypeNode*>(optType->getInnerType());
    ASSERT_NE(innerType, nullptr);
    EXPECT_EQ(innerType->getBuiltinKind(), BuiltinTypeNode::BuiltinKind::I32);
}

// ============================================================================
// 错误类型测试
// ============================================================================

TEST_F(ParseTypeTest, ParseErrorType) {
    auto result = parseTypeString("!str");
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* errType = dynamic_cast<ErrorTypeNode*>(result.get());
    ASSERT_NE(errType, nullptr);
    
    auto* successType = dynamic_cast<BuiltinTypeNode*>(errType->getSuccessType());
    ASSERT_NE(successType, nullptr);
    EXPECT_EQ(successType->getBuiltinKind(), BuiltinTypeNode::BuiltinKind::Str);
}

// ============================================================================
// 元组类型测试
// ============================================================================

TEST_F(ParseTypeTest, ParseTupleType) {
    // 空元组
    auto result1 = parseTypeString("()");
    ASSERT_TRUE(result1.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* tupleType1 = dynamic_cast<TupleTypeNode*>(result1.get());
    ASSERT_NE(tupleType1, nullptr);
    EXPECT_TRUE(tupleType1->isUnit());
    EXPECT_EQ(tupleType1->getElementCount(), 0);
    
    // 单元素元组（带逗号）
    auto result2 = parseTypeString("(i32,)");
    ASSERT_TRUE(result2.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* tupleType2 = dynamic_cast<TupleTypeNode*>(result2.get());
    ASSERT_NE(tupleType2, nullptr);
    EXPECT_FALSE(tupleType2->isUnit());
    EXPECT_EQ(tupleType2->getElementCount(), 1);
    
    // 多元素元组
    auto result3 = parseTypeString("(i32, str, bool)");
    ASSERT_TRUE(result3.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* tupleType3 = dynamic_cast<TupleTypeNode*>(result3.get());
    ASSERT_NE(tupleType3, nullptr);
    EXPECT_EQ(tupleType3->getElementCount(), 3);
    
    // 检查元素类型
    auto* elem0 = dynamic_cast<BuiltinTypeNode*>(tupleType3->getElements()[0]);
    ASSERT_NE(elem0, nullptr);
    EXPECT_EQ(elem0->getBuiltinKind(), BuiltinTypeNode::BuiltinKind::I32);
    
    auto* elem1 = dynamic_cast<BuiltinTypeNode*>(tupleType3->getElements()[1]);
    ASSERT_NE(elem1, nullptr);
    EXPECT_EQ(elem1->getBuiltinKind(), BuiltinTypeNode::BuiltinKind::Str);
    
    auto* elem2 = dynamic_cast<BuiltinTypeNode*>(tupleType3->getElements()[2]);
    ASSERT_NE(elem2, nullptr);
    EXPECT_EQ(elem2->getBuiltinKind(), BuiltinTypeNode::BuiltinKind::Bool);
}

TEST_F(ParseTypeTest, ParseParenthesizedType) {
    // 括号中的单个类型应该返回原始类型，而不是元组
    auto result = parseTypeString("(i32)");
    ASSERT_TRUE(result.isSuccess());
    ASSERT_FALSE(hasError());
    
    // 应该是 BuiltinTypeNode，而不是 TupleTypeNode
    auto* builtinType = dynamic_cast<BuiltinTypeNode*>(result.get());
    ASSERT_NE(builtinType, nullptr);
    EXPECT_EQ(builtinType->getBuiltinKind(), BuiltinTypeNode::BuiltinKind::I32);
}

// ============================================================================
// 函数类型测试
// ============================================================================

TEST_F(ParseTypeTest, ParseFunctionType) {
    // 无参数函数
    auto result1 = parseTypeString("func() -> i32");
    ASSERT_TRUE(result1.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* funcType1 = dynamic_cast<FunctionTypeNode*>(result1.get());
    ASSERT_NE(funcType1, nullptr);
    EXPECT_EQ(funcType1->getParamCount(), 0);
    EXPECT_FALSE(funcType1->canError());
    
    auto* returnType1 = dynamic_cast<BuiltinTypeNode*>(funcType1->getReturnType());
    ASSERT_NE(returnType1, nullptr);
    EXPECT_EQ(returnType1->getBuiltinKind(), BuiltinTypeNode::BuiltinKind::I32);
    
    // 有参数函数
    auto result2 = parseTypeString("func(i32, str) -> bool");
    ASSERT_TRUE(result2.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* funcType2 = dynamic_cast<FunctionTypeNode*>(result2.get());
    ASSERT_NE(funcType2, nullptr);
    EXPECT_EQ(funcType2->getParamCount(), 2);
    
    // 检查参数类型
    auto* param0 = dynamic_cast<BuiltinTypeNode*>(funcType2->getParamTypes()[0]);
    ASSERT_NE(param0, nullptr);
    EXPECT_EQ(param0->getBuiltinKind(), BuiltinTypeNode::BuiltinKind::I32);
    
    auto* param1 = dynamic_cast<BuiltinTypeNode*>(funcType2->getParamTypes()[1]);
    ASSERT_NE(param1, nullptr);
    EXPECT_EQ(param1->getBuiltinKind(), BuiltinTypeNode::BuiltinKind::Str);
    
    // 可能出错的函数
    auto result3 = parseTypeString("func(str) -> !i32");
    ASSERT_TRUE(result3.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* funcType3 = dynamic_cast<FunctionTypeNode*>(result3.get());
    ASSERT_NE(funcType3, nullptr);
    EXPECT_TRUE(funcType3->canError());
    
    // 无返回类型函数（默认 void）
    auto result4 = parseTypeString("func(i32)");
    ASSERT_TRUE(result4.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* funcType4 = dynamic_cast<FunctionTypeNode*>(result4.get());
    ASSERT_NE(funcType4, nullptr);
    EXPECT_EQ(funcType4->getParamCount(), 1);
    
    auto* returnType4 = dynamic_cast<BuiltinTypeNode*>(funcType4->getReturnType());
    ASSERT_NE(returnType4, nullptr);
    EXPECT_EQ(returnType4->getBuiltinKind(), BuiltinTypeNode::BuiltinKind::Void);
}

// ============================================================================
// 数组类型测试
// ============================================================================

TEST_F(ParseTypeTest, ParseArrayType) {
    // 固定大小数组 [T; N]
    auto result1 = parseTypeString("[i32; 5]");
    ASSERT_TRUE(result1.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* arrayType1 = dynamic_cast<ArrayTypeNode*>(result1.get());
    ASSERT_NE(arrayType1, nullptr);
    
    // 检查元素类型
    auto* elementType1 = dynamic_cast<BuiltinTypeNode*>(arrayType1->getElementType());
    ASSERT_NE(elementType1, nullptr);
    EXPECT_EQ(elementType1->getBuiltinKind(), BuiltinTypeNode::BuiltinKind::I32);
    
    // 检查大小表达式
    auto* sizeExpr1 = arrayType1->getSize();
    ASSERT_NE(sizeExpr1, nullptr);
    auto* sizeLiteral1 = dynamic_cast<IntegerLiteralExpr*>(sizeExpr1);
    ASSERT_NE(sizeLiteral1, nullptr);
    EXPECT_EQ(sizeLiteral1->getValue(), 5);
    
    // 不同类型的数组
    auto result2 = parseTypeString("[str; 10]");
    ASSERT_TRUE(result2.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* arrayType2 = dynamic_cast<ArrayTypeNode*>(result2.get());
    ASSERT_NE(arrayType2, nullptr);
    
    auto* elementType2 = dynamic_cast<BuiltinTypeNode*>(arrayType2->getElementType());
    ASSERT_NE(elementType2, nullptr);
    EXPECT_EQ(elementType2->getBuiltinKind(), BuiltinTypeNode::BuiltinKind::Str);
    
    // 嵌套数组 [[T; M]; N]
    auto result3 = parseTypeString("[[i32; 3]; 2]");
    ASSERT_TRUE(result3.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* outerArray = dynamic_cast<ArrayTypeNode*>(result3.get());
    ASSERT_NE(outerArray, nullptr);
    
    auto* innerArray = dynamic_cast<ArrayTypeNode*>(outerArray->getElementType());
    ASSERT_NE(innerArray, nullptr);
    
    auto* innerElementType = dynamic_cast<BuiltinTypeNode*>(innerArray->getElementType());
    ASSERT_NE(innerElementType, nullptr);
    EXPECT_EQ(innerElementType->getBuiltinKind(), BuiltinTypeNode::BuiltinKind::I32);
}

TEST_F(ParseTypeTest, ParseSliceType) {
    // 切片类型 [T]
    auto result1 = parseTypeString("[i32]");
    ASSERT_TRUE(result1.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* sliceType1 = dynamic_cast<SliceTypeNode*>(result1.get());
    ASSERT_NE(sliceType1, nullptr);
    EXPECT_FALSE(sliceType1->isMutable());
    
    auto* elementType1 = dynamic_cast<BuiltinTypeNode*>(sliceType1->getElementType());
    ASSERT_NE(elementType1, nullptr);
    EXPECT_EQ(elementType1->getBuiltinKind(), BuiltinTypeNode::BuiltinKind::I32);
    
    // 不同类型的切片
    auto result2 = parseTypeString("[str]");
    ASSERT_TRUE(result2.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* sliceType2 = dynamic_cast<SliceTypeNode*>(result2.get());
    ASSERT_NE(sliceType2, nullptr);
    
    auto* elementType2 = dynamic_cast<BuiltinTypeNode*>(sliceType2->getElementType());
    ASSERT_NE(elementType2, nullptr);
    EXPECT_EQ(elementType2->getBuiltinKind(), BuiltinTypeNode::BuiltinKind::Str);
}

TEST_F(ParseTypeTest, ParseArrayTypeWithComplexSize) {
    // 使用表达式作为数组大小
    auto result1 = parseTypeString("[i32; 2 + 3]");
    ASSERT_TRUE(result1.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* arrayType1 = dynamic_cast<ArrayTypeNode*>(result1.get());
    ASSERT_NE(arrayType1, nullptr);
    
    // 检查大小表达式是二元表达式
    auto* sizeExpr1 = arrayType1->getSize();
    ASSERT_NE(sizeExpr1, nullptr);
    auto* binaryExpr1 = dynamic_cast<BinaryExpr*>(sizeExpr1);
    ASSERT_NE(binaryExpr1, nullptr);
    EXPECT_EQ(binaryExpr1->getOp(), BinaryExpr::Op::Add);
    
    // 使用标识符作为数组大小
    auto result2 = parseTypeString("[f64; SIZE]");
    ASSERT_TRUE(result2.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* arrayType2 = dynamic_cast<ArrayTypeNode*>(result2.get());
    ASSERT_NE(arrayType2, nullptr);
    
    auto* sizeExpr2 = arrayType2->getSize();
    ASSERT_NE(sizeExpr2, nullptr);
    auto* identExpr2 = dynamic_cast<IdentifierExpr*>(sizeExpr2);
    ASSERT_NE(identExpr2, nullptr);
    EXPECT_EQ(identExpr2->getName(), "SIZE");
}

TEST_F(ParseTypeTest, ParseArrayTypeErrors) {
    // 缺少大小的数组类型应该被解析为切片
    auto result1 = parseTypeString("[i32]");
    ASSERT_TRUE(result1.isSuccess());
    ASSERT_FALSE(hasError());
    
    // 应该是切片类型，不是数组类型
    auto* sliceType = dynamic_cast<SliceTypeNode*>(result1.get());
    ASSERT_NE(sliceType, nullptr);
    
    // 无效的数组语法
    auto result2 = parseTypeString("[; 5]");
    EXPECT_TRUE(result2.isError());
    EXPECT_TRUE(hasError());
}

// ============================================================================
// 复杂类型测试
// ============================================================================

TEST_F(ParseTypeTest, ParseNestedTypes) {
    // 指针的引用类型
    auto result1 = parseTypeString("&*i32");
    ASSERT_TRUE(result1.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* refType = dynamic_cast<ReferenceTypeNode*>(result1.get());
    ASSERT_NE(refType, nullptr);
    EXPECT_FALSE(refType->isMutable());
    
    auto* ptrType = dynamic_cast<PointerTypeNode*>(refType->getPointeeType());
    ASSERT_NE(ptrType, nullptr);
    EXPECT_FALSE(ptrType->isMutable());
    
    // Optional 引用类型
    auto result2 = parseTypeString("?&str");
    ASSERT_TRUE(result2.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* optType = dynamic_cast<OptionalTypeNode*>(result2.get());
    ASSERT_NE(optType, nullptr);
    
    auto* refType2 = dynamic_cast<ReferenceTypeNode*>(optType->getInnerType());
    ASSERT_NE(refType2, nullptr);
    EXPECT_FALSE(refType2->isMutable());
    
    // 引用的 Optional 类型
    auto result3 = parseTypeString("&?i32");
    ASSERT_TRUE(result3.isSuccess());
    ASSERT_FALSE(hasError());
    
    auto* refType3 = dynamic_cast<ReferenceTypeNode*>(result3.get());
    ASSERT_NE(refType3, nullptr);
    EXPECT_FALSE(refType3->isMutable());
    
    auto* optType3 = dynamic_cast<OptionalTypeNode*>(refType3->getPointeeType());
    ASSERT_NE(optType3, nullptr);
}

// ============================================================================
// 错误情况测试
// ============================================================================

TEST_F(ParseTypeTest, ParseInvalidType) {
    // 无效的 Token
    auto result1 = parseTypeString("123");
    EXPECT_TRUE(result1.isError());
    EXPECT_TRUE(hasError());
}

} // namespace yuan