/// \file ASTPrinterTest.cpp
/// \brief ASTPrinter 单元测试。

#include "yuan/AST/ASTPrinter.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Stmt.h"
#include "yuan/AST/Type.h"
#include "yuan/AST/Pattern.h"
#include <gtest/gtest.h>
#include <sstream>

using namespace yuan;

// ============================================================================
// 测试辅助
// ============================================================================

class ASTPrinterTest : public ::testing::Test {
protected:
    SourceRange makeRange() {
        return SourceRange(SourceLocation(1), SourceLocation(10));
    }
    
    std::string print(const ASTNode* node) {
        std::ostringstream oss;
        ASTPrinter printer(oss);
        printer.print(node);
        return oss.str();
    }
};

// ============================================================================
// 字面量表达式打印测试
// ============================================================================

TEST_F(ASTPrinterTest, PrintIntegerLiteral) {
    auto range = makeRange();
    IntegerLiteralExpr expr(range, 42, true, 0);
    
    EXPECT_EQ(print(&expr), "42");
}

TEST_F(ASTPrinterTest, PrintIntegerLiteralWithSuffix) {
    auto range = makeRange();
    IntegerLiteralExpr expr(range, 100, true, 32);
    
    EXPECT_EQ(print(&expr), "100i32");
}

TEST_F(ASTPrinterTest, PrintIntegerLiteralUnsigned) {
    auto range = makeRange();
    IntegerLiteralExpr expr(range, 255, false, 8);
    
    EXPECT_EQ(print(&expr), "255u8");
}

TEST_F(ASTPrinterTest, PrintFloatLiteral) {
    auto range = makeRange();
    FloatLiteralExpr expr(range, 3.14, 0);
    
    std::string result = print(&expr);
    EXPECT_TRUE(result.find("3.14") != std::string::npos);
}

TEST_F(ASTPrinterTest, PrintFloatLiteralWithSuffix) {
    auto range = makeRange();
    FloatLiteralExpr expr(range, 2.5, 32);
    
    std::string result = print(&expr);
    EXPECT_TRUE(result.find("f32") != std::string::npos);
}

TEST_F(ASTPrinterTest, PrintBoolLiteralTrue) {
    auto range = makeRange();
    BoolLiteralExpr expr(range, true);
    
    EXPECT_EQ(print(&expr), "true");
}

TEST_F(ASTPrinterTest, PrintBoolLiteralFalse) {
    auto range = makeRange();
    BoolLiteralExpr expr(range, false);
    
    EXPECT_EQ(print(&expr), "false");
}

TEST_F(ASTPrinterTest, PrintCharLiteral) {
    auto range = makeRange();
    CharLiteralExpr expr(range, 'A');
    
    EXPECT_EQ(print(&expr), "'A'");
}

TEST_F(ASTPrinterTest, PrintCharLiteralEscape) {
    auto range = makeRange();
    CharLiteralExpr expr(range, '\n');
    
    EXPECT_EQ(print(&expr), "'\\n'");
}

TEST_F(ASTPrinterTest, PrintStringLiteral) {
    auto range = makeRange();
    StringLiteralExpr expr(range, "hello", StringLiteralExpr::StringKind::Normal);
    
    EXPECT_EQ(print(&expr), "\"hello\"");
}

TEST_F(ASTPrinterTest, PrintStringLiteralWithEscape) {
    auto range = makeRange();
    StringLiteralExpr expr(range, "hello\nworld", StringLiteralExpr::StringKind::Normal);
    
    EXPECT_EQ(print(&expr), "\"hello\\nworld\"");
}

TEST_F(ASTPrinterTest, PrintRawStringLiteral) {
    auto range = makeRange();
    StringLiteralExpr expr(range, "raw\\nstring", StringLiteralExpr::StringKind::Raw);
    
    EXPECT_EQ(print(&expr), "r\"raw\\nstring\"");
}

TEST_F(ASTPrinterTest, PrintNoneLiteral) {
    auto range = makeRange();
    NoneLiteralExpr expr(range);
    
    EXPECT_EQ(print(&expr), "None");
}

// ============================================================================
// 标识符和成员访问打印测试
// ============================================================================

TEST_F(ASTPrinterTest, PrintIdentifier) {
    auto range = makeRange();
    IdentifierExpr expr(range, "foo");
    
    EXPECT_EQ(print(&expr), "foo");
}

TEST_F(ASTPrinterTest, PrintMemberExpr) {
    auto range = makeRange();
    IdentifierExpr* base = new IdentifierExpr(range, "obj");
    MemberExpr expr(range, base, "field");
    
    EXPECT_EQ(print(&expr), "obj.field");
    
    delete base;
}

// ============================================================================
// 运算符表达式打印测试
// ============================================================================

TEST_F(ASTPrinterTest, PrintBinaryExprAdd) {
    auto range = makeRange();
    IntegerLiteralExpr* lhs = new IntegerLiteralExpr(range, 1, true, 0);
    IntegerLiteralExpr* rhs = new IntegerLiteralExpr(range, 2, true, 0);
    BinaryExpr expr(range, BinaryExpr::Op::Add, lhs, rhs);
    
    EXPECT_EQ(print(&expr), "(1 + 2)");
    
    delete lhs;
    delete rhs;
}

TEST_F(ASTPrinterTest, PrintBinaryExprMul) {
    auto range = makeRange();
    IntegerLiteralExpr* lhs = new IntegerLiteralExpr(range, 3, true, 0);
    IntegerLiteralExpr* rhs = new IntegerLiteralExpr(range, 4, true, 0);
    BinaryExpr expr(range, BinaryExpr::Op::Mul, lhs, rhs);
    
    EXPECT_EQ(print(&expr), "(3 * 4)");
    
    delete lhs;
    delete rhs;
}

TEST_F(ASTPrinterTest, PrintBinaryExprComparison) {
    auto range = makeRange();
    IdentifierExpr* lhs = new IdentifierExpr(range, "x");
    IntegerLiteralExpr* rhs = new IntegerLiteralExpr(range, 0, true, 0);
    BinaryExpr expr(range, BinaryExpr::Op::Gt, lhs, rhs);
    
    EXPECT_EQ(print(&expr), "(x > 0)");
    
    delete lhs;
    delete rhs;
}

TEST_F(ASTPrinterTest, PrintUnaryExprNeg) {
    auto range = makeRange();
    IntegerLiteralExpr* operand = new IntegerLiteralExpr(range, 5, true, 0);
    UnaryExpr expr(range, UnaryExpr::Op::Neg, operand);
    
    EXPECT_EQ(print(&expr), "-5");
    
    delete operand;
}

TEST_F(ASTPrinterTest, PrintUnaryExprNot) {
    auto range = makeRange();
    BoolLiteralExpr* operand = new BoolLiteralExpr(range, true);
    UnaryExpr expr(range, UnaryExpr::Op::Not, operand);
    
    EXPECT_EQ(print(&expr), "!true");
    
    delete operand;
}

TEST_F(ASTPrinterTest, PrintUnaryExprRef) {
    auto range = makeRange();
    IdentifierExpr* operand = new IdentifierExpr(range, "x");
    UnaryExpr expr(range, UnaryExpr::Op::Ref, operand);
    
    EXPECT_EQ(print(&expr), "&x");
    
    delete operand;
}

TEST_F(ASTPrinterTest, PrintAssignExpr) {
    auto range = makeRange();
    IdentifierExpr* target = new IdentifierExpr(range, "x");
    IntegerLiteralExpr* value = new IntegerLiteralExpr(range, 10, true, 0);
    AssignExpr expr(range, AssignExpr::Op::Assign, target, value);
    
    EXPECT_EQ(print(&expr), "x = 10");
    
    delete target;
    delete value;
}

TEST_F(ASTPrinterTest, PrintAssignExprCompound) {
    auto range = makeRange();
    IdentifierExpr* target = new IdentifierExpr(range, "x");
    IntegerLiteralExpr* value = new IntegerLiteralExpr(range, 1, true, 0);
    AssignExpr expr(range, AssignExpr::Op::AddAssign, target, value);
    
    EXPECT_EQ(print(&expr), "x += 1");
    
    delete target;
    delete value;
}

// ============================================================================
// 调用和索引表达式打印测试
// ============================================================================

TEST_F(ASTPrinterTest, PrintCallExprNoArgs) {
    auto range = makeRange();
    IdentifierExpr* callee = new IdentifierExpr(range, "foo");
    std::vector<Expr*> args;
    CallExpr expr(range, callee, args);
    
    EXPECT_EQ(print(&expr), "foo()");
    
    delete callee;
}

TEST_F(ASTPrinterTest, PrintCallExprWithArgs) {
    auto range = makeRange();
    IdentifierExpr* callee = new IdentifierExpr(range, "add");
    std::vector<Expr*> args;
    args.push_back(new IntegerLiteralExpr(range, 1, true, 0));
    args.push_back(new IntegerLiteralExpr(range, 2, true, 0));
    CallExpr expr(range, callee, args);
    
    EXPECT_EQ(print(&expr), "add(1, 2)");
    
    delete callee;
    for (auto* arg : args) delete arg;
}

TEST_F(ASTPrinterTest, PrintIndexExpr) {
    auto range = makeRange();
    IdentifierExpr* base = new IdentifierExpr(range, "arr");
    IntegerLiteralExpr* index = new IntegerLiteralExpr(range, 0, true, 0);
    IndexExpr expr(range, base, index);
    
    EXPECT_EQ(print(&expr), "arr[0]");
    
    delete base;
    delete index;
}

// ============================================================================
// 复合表达式打印测试
// ============================================================================

TEST_F(ASTPrinterTest, PrintArrayExpr) {
    auto range = makeRange();
    std::vector<Expr*> elements;
    elements.push_back(new IntegerLiteralExpr(range, 1, true, 0));
    elements.push_back(new IntegerLiteralExpr(range, 2, true, 0));
    elements.push_back(new IntegerLiteralExpr(range, 3, true, 0));
    ArrayExpr expr(range, elements);
    
    EXPECT_EQ(print(&expr), "[1, 2, 3]");
    
    for (auto* elem : elements) delete elem;
}

TEST_F(ASTPrinterTest, PrintTupleExpr) {
    auto range = makeRange();
    std::vector<Expr*> elements;
    elements.push_back(new IntegerLiteralExpr(range, 1, true, 0));
    elements.push_back(new StringLiteralExpr(range, "hello", StringLiteralExpr::StringKind::Normal));
    TupleExpr expr(range, elements);
    
    EXPECT_EQ(print(&expr), "(1, \"hello\")");
    
    for (auto* elem : elements) delete elem;
}

TEST_F(ASTPrinterTest, PrintTupleExprSingle) {
    auto range = makeRange();
    std::vector<Expr*> elements;
    elements.push_back(new IntegerLiteralExpr(range, 42, true, 0));
    TupleExpr expr(range, elements);
    
    // 单元素元组需要尾随逗号
    EXPECT_EQ(print(&expr), "(42,)");
    
    for (auto* elem : elements) delete elem;
}

TEST_F(ASTPrinterTest, PrintRangeExpr) {
    auto range = makeRange();
    IntegerLiteralExpr* start = new IntegerLiteralExpr(range, 0, true, 0);
    IntegerLiteralExpr* end = new IntegerLiteralExpr(range, 10, true, 0);
    RangeExpr expr(range, start, end, false);
    
    EXPECT_EQ(print(&expr), "0..10");
    
    delete start;
    delete end;
}

TEST_F(ASTPrinterTest, PrintRangeExprInclusive) {
    auto range = makeRange();
    IntegerLiteralExpr* start = new IntegerLiteralExpr(range, 0, true, 0);
    IntegerLiteralExpr* end = new IntegerLiteralExpr(range, 10, true, 0);
    RangeExpr expr(range, start, end, true);
    
    EXPECT_EQ(print(&expr), "0..=10");
    
    delete start;
    delete end;
}

// ============================================================================
// 类型打印测试
// ============================================================================

TEST_F(ASTPrinterTest, PrintBuiltinTypeI32) {
    auto range = makeRange();
    BuiltinTypeNode type(range, BuiltinTypeNode::BuiltinKind::I32);
    
    EXPECT_EQ(print(&type), "i32");
}

TEST_F(ASTPrinterTest, PrintBuiltinTypeBool) {
    auto range = makeRange();
    BuiltinTypeNode type(range, BuiltinTypeNode::BuiltinKind::Bool);
    
    EXPECT_EQ(print(&type), "bool");
}

TEST_F(ASTPrinterTest, PrintIdentifierType) {
    auto range = makeRange();
    IdentifierTypeNode type(range, "MyStruct");
    
    EXPECT_EQ(print(&type), "MyStruct");
}

TEST_F(ASTPrinterTest, PrintOptionalType) {
    auto range = makeRange();
    BuiltinTypeNode* inner = new BuiltinTypeNode(range, BuiltinTypeNode::BuiltinKind::I32);
    OptionalTypeNode type(range, inner);
    
    EXPECT_EQ(print(&type), "?i32");
    
    delete inner;
}

TEST_F(ASTPrinterTest, PrintReferenceType) {
    auto range = makeRange();
    BuiltinTypeNode* pointee = new BuiltinTypeNode(range, BuiltinTypeNode::BuiltinKind::I32);
    ReferenceTypeNode type(range, pointee, false);
    
    EXPECT_EQ(print(&type), "&i32");
    
    delete pointee;
}

TEST_F(ASTPrinterTest, PrintReferenceTypeMut) {
    auto range = makeRange();
    BuiltinTypeNode* pointee = new BuiltinTypeNode(range, BuiltinTypeNode::BuiltinKind::I32);
    ReferenceTypeNode type(range, pointee, true);
    
    EXPECT_EQ(print(&type), "&mut i32");
    
    delete pointee;
}

TEST_F(ASTPrinterTest, PrintPointerType) {
    auto range = makeRange();
    BuiltinTypeNode* pointee = new BuiltinTypeNode(range, BuiltinTypeNode::BuiltinKind::U8);
    PointerTypeNode type(range, pointee, false);
    
    EXPECT_EQ(print(&type), "*u8");
    
    delete pointee;
}

TEST_F(ASTPrinterTest, PrintTupleType) {
    auto range = makeRange();
    std::vector<TypeNode*> elements;
    elements.push_back(new BuiltinTypeNode(range, BuiltinTypeNode::BuiltinKind::I32));
    elements.push_back(new BuiltinTypeNode(range, BuiltinTypeNode::BuiltinKind::Str));
    TupleTypeNode type(range, elements);
    
    EXPECT_EQ(print(&type), "(i32, str)");
    
    for (auto* elem : elements) delete elem;
}

TEST_F(ASTPrinterTest, PrintGenericType) {
    auto range = makeRange();
    std::vector<TypeNode*> typeArgs;
    typeArgs.push_back(new BuiltinTypeNode(range, BuiltinTypeNode::BuiltinKind::I32));
    GenericTypeNode type(range, "Vec", typeArgs);
    
    EXPECT_EQ(print(&type), "Vec<i32>");
    
    for (auto* arg : typeArgs) delete arg;
}

// ============================================================================
// 模式打印测试
// ============================================================================

TEST_F(ASTPrinterTest, PrintWildcardPattern) {
    auto range = makeRange();
    WildcardPattern pattern(range);
    
    EXPECT_EQ(print(&pattern), "_");
}

TEST_F(ASTPrinterTest, PrintIdentifierPattern) {
    auto range = makeRange();
    IdentifierPattern pattern(range, "x", false, nullptr);
    
    EXPECT_EQ(print(&pattern), "x");
}

TEST_F(ASTPrinterTest, PrintIdentifierPatternMut) {
    auto range = makeRange();
    IdentifierPattern pattern(range, "y", true, nullptr);
    
    EXPECT_EQ(print(&pattern), "mut y");
}

TEST_F(ASTPrinterTest, PrintTuplePattern) {
    auto range = makeRange();
    std::vector<Pattern*> elements;
    elements.push_back(new IdentifierPattern(range, "x", false, nullptr));
    elements.push_back(new IdentifierPattern(range, "y", false, nullptr));
    TuplePattern pattern(range, elements);
    
    EXPECT_EQ(print(&pattern), "(x, y)");
    
    for (auto* elem : elements) delete elem;
}

TEST_F(ASTPrinterTest, PrintEnumPattern) {
    auto range = makeRange();
    std::vector<Pattern*> payload;
    payload.push_back(new IdentifierPattern(range, "value", false, nullptr));
    EnumPattern pattern(range, "Option", "Some", payload);
    
    EXPECT_EQ(print(&pattern), "Option.Some(value)");
    
    for (auto* p : payload) delete p;
}

// ============================================================================
// 声明打印测试
// ============================================================================

TEST_F(ASTPrinterTest, PrintVarDecl) {
    auto range = makeRange();
    VarDecl decl(range, "x", nullptr, nullptr, false);
    
    EXPECT_EQ(print(&decl), "var x");
}

TEST_F(ASTPrinterTest, PrintVarDeclMut) {
    auto range = makeRange();
    VarDecl decl(range, "y", nullptr, nullptr, true);
    
    // var 声明默认可变，不需要 mut 关键字
    EXPECT_EQ(print(&decl), "var y");
}

TEST_F(ASTPrinterTest, PrintVarDeclWithType) {
    auto range = makeRange();
    BuiltinTypeNode* type = new BuiltinTypeNode(range, BuiltinTypeNode::BuiltinKind::I32);
    VarDecl decl(range, "x", type, nullptr, false);
    
    EXPECT_EQ(print(&decl), "var x: i32");
    
    delete type;
}

TEST_F(ASTPrinterTest, PrintVarDeclWithInit) {
    auto range = makeRange();
    IntegerLiteralExpr* init = new IntegerLiteralExpr(range, 42, true, 0);
    VarDecl decl(range, "x", nullptr, init, false);
    
    EXPECT_EQ(print(&decl), "var x = 42");
    
    delete init;
}

TEST_F(ASTPrinterTest, PrintConstDecl) {
    auto range = makeRange();
    IntegerLiteralExpr* init = new IntegerLiteralExpr(range, 100, true, 0);
    ConstDecl decl(range, "MAX", nullptr, init);
    
    EXPECT_EQ(print(&decl), "const MAX = 100");
    
    delete init;
}

TEST_F(ASTPrinterTest, PrintFuncDeclSimple) {
    auto range = makeRange();
    std::vector<ParamDecl*> params;
    FuncDecl decl(range, "foo", params, nullptr, nullptr, false, false, Visibility::Private);
    
    std::string result = print(&decl);
    EXPECT_TRUE(result.find("func foo()") != std::string::npos);
}

TEST_F(ASTPrinterTest, PrintFuncDeclPublic) {
    auto range = makeRange();
    std::vector<ParamDecl*> params;
    FuncDecl decl(range, "bar", params, nullptr, nullptr, false, false, Visibility::Public);
    
    std::string result = print(&decl);
    EXPECT_TRUE(result.find("pub func bar()") != std::string::npos);
}

TEST_F(ASTPrinterTest, PrintFuncDeclAsync) {
    auto range = makeRange();
    std::vector<ParamDecl*> params;
    FuncDecl decl(range, "fetch", params, nullptr, nullptr, true, false, Visibility::Private);
    
    std::string result = print(&decl);
    EXPECT_TRUE(result.find("async func fetch()") != std::string::npos);
}

// ============================================================================
// 语句打印测试
// ============================================================================

TEST_F(ASTPrinterTest, PrintReturnStmt) {
    auto range = makeRange();
    ReturnStmt stmt(range, nullptr);
    
    EXPECT_EQ(print(&stmt), "return");
}

TEST_F(ASTPrinterTest, PrintReturnStmtWithValue) {
    auto range = makeRange();
    IntegerLiteralExpr* value = new IntegerLiteralExpr(range, 42, true, 0);
    ReturnStmt stmt(range, value);
    
    EXPECT_EQ(print(&stmt), "return 42");
    
    delete value;
}

TEST_F(ASTPrinterTest, PrintBreakStmt) {
    auto range = makeRange();
    BreakStmt stmt(range);
    
    EXPECT_EQ(print(&stmt), "break");
}

TEST_F(ASTPrinterTest, PrintBreakStmtWithLabel) {
    auto range = makeRange();
    BreakStmt stmt(range, "outer");
    
    EXPECT_EQ(print(&stmt), "break 'outer");
}

TEST_F(ASTPrinterTest, PrintContinueStmt) {
    auto range = makeRange();
    ContinueStmt stmt(range);
    
    EXPECT_EQ(print(&stmt), "continue");
}

TEST_F(ASTPrinterTest, PrintBlockStmtEmpty) {
    auto range = makeRange();
    std::vector<Stmt*> stmts;
    BlockStmt stmt(range, stmts);
    
    std::string result = print(&stmt);
    EXPECT_TRUE(result.find("{") != std::string::npos);
    EXPECT_TRUE(result.find("}") != std::string::npos);
}
