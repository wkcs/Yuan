/// \file ASTNodeTest.cpp
/// \brief AST 节点创建单元测试。

#include "yuan/AST/AST.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Stmt.h"
#include "yuan/AST/Type.h"
#include "yuan/AST/Pattern.h"
#include "yuan/AST/ASTVisitor.h"
#include <gtest/gtest.h>
#include <sstream>

using namespace yuan;

// ============================================================================
// 测试辅助
// ============================================================================

class ASTNodeTest : public ::testing::Test {
protected:
    SourceRange makeRange() {
        return SourceRange(SourceLocation(1), SourceLocation(10));
    }
};

// ============================================================================
// 声明节点测试
// ============================================================================

TEST_F(ASTNodeTest, VarDeclCreation) {
    auto range = makeRange();
    VarDecl decl(range, "x", nullptr, nullptr, false);
    
    EXPECT_EQ(decl.getName(), "x");
    EXPECT_EQ(decl.getType(), nullptr);
    EXPECT_EQ(decl.getInit(), nullptr);
    EXPECT_FALSE(decl.isMutable());
    EXPECT_EQ(decl.getKind(), ASTNode::Kind::VarDecl);
    EXPECT_TRUE(decl.isDecl());
}

TEST_F(ASTNodeTest, VarDeclMutable) {
    auto range = makeRange();
    VarDecl decl(range, "y", nullptr, nullptr, true);
    
    EXPECT_EQ(decl.getName(), "y");
    EXPECT_TRUE(decl.isMutable());
}

TEST_F(ASTNodeTest, ConstDeclCreation) {
    auto range = makeRange();
    IntegerLiteralExpr* init = new IntegerLiteralExpr(range, 42, true, 32);
    ConstDecl decl(range, "PI", nullptr, init);
    
    EXPECT_EQ(decl.getName(), "PI");
    EXPECT_EQ(decl.getInit(), init);
    EXPECT_EQ(decl.getKind(), ASTNode::Kind::ConstDecl);
    
    delete init;
}

TEST_F(ASTNodeTest, ParamDeclNormal) {
    auto range = makeRange();
    ParamDecl decl(range, "x", nullptr, false);
    
    EXPECT_EQ(decl.getName(), "x");
    EXPECT_FALSE(decl.isMutable());
    EXPECT_FALSE(decl.isSelf());
    EXPECT_EQ(decl.getParamKind(), ParamDecl::ParamKind::Normal);
}

TEST_F(ASTNodeTest, ParamDeclSelf) {
    auto range = makeRange();
    ParamDecl* decl = ParamDecl::createSelf(range, ParamDecl::ParamKind::Self);
    
    EXPECT_TRUE(decl->isSelf());
    EXPECT_EQ(decl->getParamKind(), ParamDecl::ParamKind::Self);
    
    delete decl;
}

TEST_F(ASTNodeTest, ParamDeclRefSelf) {
    auto range = makeRange();
    ParamDecl* decl = ParamDecl::createSelf(range, ParamDecl::ParamKind::RefSelf);
    
    EXPECT_TRUE(decl->isSelf());
    EXPECT_EQ(decl->getParamKind(), ParamDecl::ParamKind::RefSelf);
    
    delete decl;
}

TEST_F(ASTNodeTest, FuncDeclCreation) {
    auto range = makeRange();
    std::vector<ParamDecl*> params;
    FuncDecl decl(range, "foo", params, nullptr, nullptr, false, false, Visibility::Public);
    
    EXPECT_EQ(decl.getName(), "foo");
    EXPECT_TRUE(decl.getParams().empty());
    EXPECT_FALSE(decl.isAsync());
    EXPECT_FALSE(decl.canError());
    EXPECT_EQ(decl.getVisibility(), Visibility::Public);
    EXPECT_FALSE(decl.hasBody());
}

TEST_F(ASTNodeTest, FuncDeclAsync) {
    auto range = makeRange();
    std::vector<ParamDecl*> params;
    FuncDecl decl(range, "fetch", params, nullptr, nullptr, true, true, Visibility::Private);
    
    EXPECT_TRUE(decl.isAsync());
    EXPECT_TRUE(decl.canError());
}

TEST_F(ASTNodeTest, FuncDeclGeneric) {
    auto range = makeRange();
    std::vector<ParamDecl*> params;
    FuncDecl decl(range, "identity", params, nullptr, nullptr, false, false, Visibility::Public);
    
    std::vector<GenericParam> genericParams;
    genericParams.emplace_back("T", SourceLocation(1));
    decl.setGenericParams(genericParams);
    
    EXPECT_TRUE(decl.isGeneric());
    EXPECT_EQ(decl.getGenericParams().size(), 1);
    EXPECT_EQ(decl.getGenericParams()[0].Name, "T");
}

TEST_F(ASTNodeTest, StructDeclCreation) {
    auto range = makeRange();
    std::vector<FieldDecl*> fields;
    StructDecl decl(range, "Point", fields, Visibility::Public);
    
    EXPECT_EQ(decl.getName(), "Point");
    EXPECT_TRUE(decl.getFields().empty());
    EXPECT_EQ(decl.getVisibility(), Visibility::Public);
}

TEST_F(ASTNodeTest, EnumVariantUnit) {
    auto range = makeRange();
    EnumVariantDecl* decl = EnumVariantDecl::createUnit(range, "None");
    
    EXPECT_EQ(decl->getName(), "None");
    EXPECT_TRUE(decl->isUnit());
    EXPECT_FALSE(decl->isTuple());
    EXPECT_FALSE(decl->isStruct());
    
    delete decl;
}

TEST_F(ASTNodeTest, EnumVariantTuple) {
    auto range = makeRange();
    std::vector<TypeNode*> types;
    EnumVariantDecl* decl = EnumVariantDecl::createTuple(range, "Some", types);
    
    EXPECT_EQ(decl->getName(), "Some");
    EXPECT_TRUE(decl->isTuple());
    
    delete decl;
}

TEST_F(ASTNodeTest, EnumDeclCreation) {
    auto range = makeRange();
    std::vector<EnumVariantDecl*> variants;
    EnumDecl decl(range, "Option", variants, Visibility::Public);
    
    EXPECT_EQ(decl.getName(), "Option");
    EXPECT_TRUE(decl.getVariants().empty());
}

// ============================================================================
// 表达式节点测试
// ============================================================================

TEST_F(ASTNodeTest, IntegerLiteralExpr) {
    auto range = makeRange();
    IntegerLiteralExpr expr(range, 42, true, 32);
    
    EXPECT_EQ(expr.getValue(), 42);
    EXPECT_TRUE(expr.isSigned());
    EXPECT_EQ(expr.getBitWidth(), 32);
    EXPECT_TRUE(expr.hasTypeSuffix());
    EXPECT_EQ(expr.getKind(), ASTNode::Kind::IntegerLiteralExpr);
    EXPECT_TRUE(expr.isExpr());
}

TEST_F(ASTNodeTest, IntegerLiteralExprNoSuffix) {
    auto range = makeRange();
    IntegerLiteralExpr expr(range, 100, true, 0);
    
    EXPECT_EQ(expr.getValue(), 100);
    EXPECT_FALSE(expr.hasTypeSuffix());
}

TEST_F(ASTNodeTest, FloatLiteralExpr) {
    auto range = makeRange();
    FloatLiteralExpr expr(range, 3.14, 64);
    
    EXPECT_DOUBLE_EQ(expr.getValue(), 3.14);
    EXPECT_EQ(expr.getBitWidth(), 64);
    EXPECT_TRUE(expr.hasTypeSuffix());
}

TEST_F(ASTNodeTest, BoolLiteralExpr) {
    auto range = makeRange();
    BoolLiteralExpr exprTrue(range, true);
    BoolLiteralExpr exprFalse(range, false);
    
    EXPECT_TRUE(exprTrue.getValue());
    EXPECT_FALSE(exprFalse.getValue());
}

TEST_F(ASTNodeTest, CharLiteralExpr) {
    auto range = makeRange();
    CharLiteralExpr expr(range, 'A');
    
    EXPECT_EQ(expr.getCodepoint(), 'A');
}

TEST_F(ASTNodeTest, StringLiteralExpr) {
    auto range = makeRange();
    StringLiteralExpr expr(range, "hello", StringLiteralExpr::StringKind::Normal);
    
    EXPECT_EQ(expr.getValue(), "hello");
    EXPECT_EQ(expr.getStringKind(), StringLiteralExpr::StringKind::Normal);
    EXPECT_FALSE(expr.isRaw());
    EXPECT_FALSE(expr.isMultiline());
}

TEST_F(ASTNodeTest, StringLiteralExprRaw) {
    auto range = makeRange();
    StringLiteralExpr expr(range, "raw\\nstring", StringLiteralExpr::StringKind::Raw);
    
    EXPECT_TRUE(expr.isRaw());
}

TEST_F(ASTNodeTest, NoneLiteralExpr) {
    auto range = makeRange();
    NoneLiteralExpr expr(range);
    
    EXPECT_EQ(expr.getKind(), ASTNode::Kind::NoneLiteralExpr);
}

TEST_F(ASTNodeTest, IdentifierExpr) {
    auto range = makeRange();
    IdentifierExpr expr(range, "foo");
    
    EXPECT_EQ(expr.getName(), "foo");
    EXPECT_TRUE(expr.isLValue());
}

TEST_F(ASTNodeTest, BinaryExpr) {
    auto range = makeRange();
    IntegerLiteralExpr* lhs = new IntegerLiteralExpr(range, 1, true, 0);
    IntegerLiteralExpr* rhs = new IntegerLiteralExpr(range, 2, true, 0);
    BinaryExpr expr(range, BinaryExpr::Op::Add, lhs, rhs);
    
    EXPECT_EQ(expr.getOp(), BinaryExpr::Op::Add);
    EXPECT_EQ(expr.getLHS(), lhs);
    EXPECT_EQ(expr.getRHS(), rhs);
    EXPECT_STREQ(BinaryExpr::getOpSpelling(BinaryExpr::Op::Add), "+");
    
    delete lhs;
    delete rhs;
}

TEST_F(ASTNodeTest, UnaryExpr) {
    auto range = makeRange();
    IntegerLiteralExpr* operand = new IntegerLiteralExpr(range, 5, true, 0);
    UnaryExpr expr(range, UnaryExpr::Op::Neg, operand);
    
    EXPECT_EQ(expr.getOp(), UnaryExpr::Op::Neg);
    EXPECT_EQ(expr.getOperand(), operand);
    EXPECT_STREQ(UnaryExpr::getOpSpelling(UnaryExpr::Op::Neg), "-");
    
    delete operand;
}

TEST_F(ASTNodeTest, CallExpr) {
    auto range = makeRange();
    IdentifierExpr* callee = new IdentifierExpr(range, "foo");
    std::vector<Expr*> args;
    CallExpr expr(range, callee, args);
    
    EXPECT_EQ(expr.getCallee(), callee);
    EXPECT_TRUE(expr.getArgs().empty());
    EXPECT_EQ(expr.getArgCount(), 0);
    
    delete callee;
}

TEST_F(ASTNodeTest, IndexExpr) {
    auto range = makeRange();
    IdentifierExpr* base = new IdentifierExpr(range, "arr");
    IntegerLiteralExpr* index = new IntegerLiteralExpr(range, 0, true, 0);
    IndexExpr expr(range, base, index);
    
    EXPECT_EQ(expr.getBase(), base);
    EXPECT_EQ(expr.getIndex(), index);
    EXPECT_TRUE(expr.isLValue());
    
    delete base;
    delete index;
}

TEST_F(ASTNodeTest, ArrayExpr) {
    auto range = makeRange();
    std::vector<Expr*> elements;
    ArrayExpr expr(range, elements);
    
    EXPECT_TRUE(expr.getElements().empty());
    EXPECT_FALSE(expr.isRepeat());
}

TEST_F(ASTNodeTest, TupleExpr) {
    auto range = makeRange();
    std::vector<Expr*> elements;
    TupleExpr expr(range, elements);
    
    EXPECT_TRUE(expr.isEmpty());
}

TEST_F(ASTNodeTest, RangeExpr) {
    auto range = makeRange();
    IntegerLiteralExpr* start = new IntegerLiteralExpr(range, 0, true, 0);
    IntegerLiteralExpr* end = new IntegerLiteralExpr(range, 10, true, 0);
    RangeExpr expr(range, start, end, false);
    
    EXPECT_EQ(expr.getStart(), start);
    EXPECT_EQ(expr.getEnd(), end);
    EXPECT_FALSE(expr.isInclusive());
    EXPECT_TRUE(expr.hasStart());
    EXPECT_TRUE(expr.hasEnd());
    
    delete start;
    delete end;
}

// ============================================================================
// 语句节点测试
// ============================================================================

TEST_F(ASTNodeTest, BlockStmt) {
    auto range = makeRange();
    std::vector<Stmt*> stmts;
    BlockStmt stmt(range, stmts);
    
    EXPECT_TRUE(stmt.isEmpty());
    EXPECT_EQ(stmt.getStatementCount(), 0);
    EXPECT_EQ(stmt.getKind(), ASTNode::Kind::BlockStmt);
    EXPECT_TRUE(stmt.isStmt());
}

TEST_F(ASTNodeTest, ReturnStmt) {
    auto range = makeRange();
    ReturnStmt stmt(range, nullptr);
    
    EXPECT_FALSE(stmt.hasValue());
    EXPECT_EQ(stmt.getValue(), nullptr);
}

TEST_F(ASTNodeTest, ReturnStmtWithValue) {
    auto range = makeRange();
    IntegerLiteralExpr* value = new IntegerLiteralExpr(range, 42, true, 0);
    ReturnStmt stmt(range, value);
    
    EXPECT_TRUE(stmt.hasValue());
    EXPECT_EQ(stmt.getValue(), value);
    
    delete value;
}

TEST_F(ASTNodeTest, BreakStmt) {
    auto range = makeRange();
    BreakStmt stmt(range);
    
    EXPECT_FALSE(stmt.hasLabel());
}

TEST_F(ASTNodeTest, BreakStmtWithLabel) {
    auto range = makeRange();
    BreakStmt stmt(range, "outer");
    
    EXPECT_TRUE(stmt.hasLabel());
    EXPECT_EQ(stmt.getLabel(), "outer");
}

TEST_F(ASTNodeTest, ContinueStmt) {
    auto range = makeRange();
    ContinueStmt stmt(range);
    
    EXPECT_FALSE(stmt.hasLabel());
}

// ============================================================================
// 类型节点测试
// ============================================================================

TEST_F(ASTNodeTest, BuiltinTypeNode) {
    auto range = makeRange();
    BuiltinTypeNode type(range, BuiltinTypeNode::BuiltinKind::I32);
    
    EXPECT_EQ(type.getBuiltinKind(), BuiltinTypeNode::BuiltinKind::I32);
    EXPECT_TRUE(type.isInteger());
    EXPECT_TRUE(type.isSignedInteger());
    EXPECT_FALSE(type.isUnsignedInteger());
    EXPECT_FALSE(type.isFloatingPoint());
    EXPECT_EQ(type.getKind(), ASTNode::Kind::BuiltinTypeNode);
    EXPECT_TRUE(type.isTypeNode());
}

TEST_F(ASTNodeTest, BuiltinTypeNodeFloat) {
    auto range = makeRange();
    BuiltinTypeNode type(range, BuiltinTypeNode::BuiltinKind::F64);
    
    EXPECT_TRUE(type.isFloatingPoint());
    EXPECT_FALSE(type.isInteger());
}

TEST_F(ASTNodeTest, IdentifierTypeNode) {
    auto range = makeRange();
    IdentifierTypeNode type(range, "MyStruct");
    
    EXPECT_EQ(type.getName(), "MyStruct");
}

TEST_F(ASTNodeTest, TupleTypeNode) {
    auto range = makeRange();
    std::vector<TypeNode*> elements;
    TupleTypeNode type(range, elements);
    
    EXPECT_TRUE(type.isUnit());
    EXPECT_EQ(type.getElementCount(), 0);
}

TEST_F(ASTNodeTest, OptionalTypeNode) {
    auto range = makeRange();
    BuiltinTypeNode* inner = new BuiltinTypeNode(range, BuiltinTypeNode::BuiltinKind::I32);
    OptionalTypeNode type(range, inner);
    
    EXPECT_EQ(type.getInnerType(), inner);
    
    delete inner;
}

TEST_F(ASTNodeTest, ReferenceTypeNode) {
    auto range = makeRange();
    BuiltinTypeNode* pointee = new BuiltinTypeNode(range, BuiltinTypeNode::BuiltinKind::I32);
    ReferenceTypeNode type(range, pointee, false);
    
    EXPECT_EQ(type.getPointeeType(), pointee);
    EXPECT_FALSE(type.isMutable());
    
    delete pointee;
}

TEST_F(ASTNodeTest, ReferenceTypeNodeMut) {
    auto range = makeRange();
    BuiltinTypeNode* pointee = new BuiltinTypeNode(range, BuiltinTypeNode::BuiltinKind::I32);
    ReferenceTypeNode type(range, pointee, true);
    
    EXPECT_TRUE(type.isMutable());
    
    delete pointee;
}

// ============================================================================
// 模式节点测试
// ============================================================================

TEST_F(ASTNodeTest, WildcardPattern) {
    auto range = makeRange();
    WildcardPattern pattern(range);
    
    EXPECT_EQ(pattern.getKind(), ASTNode::Kind::WildcardPattern);
    EXPECT_TRUE(pattern.isPattern());
}

TEST_F(ASTNodeTest, IdentifierPattern) {
    auto range = makeRange();
    IdentifierPattern pattern(range, "x", false, nullptr);
    
    EXPECT_EQ(pattern.getName(), "x");
    EXPECT_FALSE(pattern.isMutable());
    EXPECT_FALSE(pattern.hasType());
}

TEST_F(ASTNodeTest, IdentifierPatternMut) {
    auto range = makeRange();
    IdentifierPattern pattern(range, "y", true, nullptr);
    
    EXPECT_TRUE(pattern.isMutable());
}

TEST_F(ASTNodeTest, TuplePattern) {
    auto range = makeRange();
    std::vector<Pattern*> elements;
    TuplePattern pattern(range, elements);
    
    EXPECT_TRUE(pattern.isEmpty());
    EXPECT_EQ(pattern.getElementCount(), 0);
}

TEST_F(ASTNodeTest, EnumPattern) {
    auto range = makeRange();
    EnumPattern pattern(range, "Option", "Some", {});
    
    EXPECT_EQ(pattern.getEnumName(), "Option");
    EXPECT_EQ(pattern.getVariantName(), "Some");
    EXPECT_FALSE(pattern.hasPayload());
    EXPECT_TRUE(pattern.hasEnumName());
}

// ============================================================================
// ASTVisitor 测试
// ============================================================================

/// \brief 计数访问者，用于测试 ASTVisitor
class CountingVisitor : public ASTVisitor<CountingVisitor, int> {
public:
    int IntegerCount = 0;
    int BinaryCount = 0;
    int IdentifierCount = 0;
    
    int visitIntegerLiteralExpr(IntegerLiteralExpr* /*expr*/) {
        ++IntegerCount;
        return IntegerCount;
    }
    
    int visitBinaryExpr(BinaryExpr* expr) {
        ++BinaryCount;
        // 继续访问子节点
        if (expr->getLHS()) visitExpr(expr->getLHS());
        if (expr->getRHS()) visitExpr(expr->getRHS());
        return BinaryCount;
    }
    
    int visitIdentifierExpr(IdentifierExpr* /*expr*/) {
        ++IdentifierCount;
        return IdentifierCount;
    }
};

TEST_F(ASTNodeTest, ASTVisitorBasic) {
    auto range = makeRange();
    IntegerLiteralExpr expr(range, 42, true, 0);
    
    CountingVisitor visitor;
    visitor.visit(&expr);
    
    EXPECT_EQ(visitor.IntegerCount, 1);
}

TEST_F(ASTNodeTest, ASTVisitorBinaryExpr) {
    auto range = makeRange();
    IntegerLiteralExpr* lhs = new IntegerLiteralExpr(range, 1, true, 0);
    IntegerLiteralExpr* rhs = new IntegerLiteralExpr(range, 2, true, 0);
    BinaryExpr expr(range, BinaryExpr::Op::Add, lhs, rhs);
    
    CountingVisitor visitor;
    visitor.visit(&expr);
    
    EXPECT_EQ(visitor.BinaryCount, 1);
    EXPECT_EQ(visitor.IntegerCount, 2);
    
    delete lhs;
    delete rhs;
}

// ============================================================================
// Visibility 测试
// ============================================================================

TEST_F(ASTNodeTest, VisibilityNames) {
    EXPECT_STREQ(getVisibilityName(Visibility::Private), "priv");
    EXPECT_STREQ(getVisibilityName(Visibility::Public), "pub");
    EXPECT_STREQ(getVisibilityName(Visibility::Internal), "internal");
}

// ============================================================================
// GenericParam 测试
// ============================================================================

TEST_F(ASTNodeTest, GenericParamBasic) {
    GenericParam param("T", SourceLocation(1));
    
    EXPECT_EQ(param.Name, "T");
    EXPECT_TRUE(param.Bounds.empty());
}

TEST_F(ASTNodeTest, GenericParamWithBounds) {
    std::vector<std::string> bounds = {"Display", "Clone"};
    GenericParam param("T", bounds, SourceLocation(1));
    
    EXPECT_EQ(param.Name, "T");
    EXPECT_EQ(param.Bounds.size(), 2);
    EXPECT_EQ(param.Bounds[0], "Display");
    EXPECT_EQ(param.Bounds[1], "Clone");
}
