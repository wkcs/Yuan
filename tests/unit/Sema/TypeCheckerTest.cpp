/// \file
/// \brief 类型检查器单元测试

#include "yuan/Sema/TypeChecker.h"
#include "yuan/Sema/Scope.h"
#include "yuan/Sema/Symbol.h"
#include "yuan/Sema/Type.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/Expr.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/SourceManager.h"
#include <gtest/gtest.h>
#include <memory>

namespace yuan {

class TypeCheckerTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto consumer = std::make_unique<StoredDiagnosticConsumer>();
        StoredConsumer = consumer.get();
        Diag.setConsumer(std::move(consumer));
    }

    static SourceRange testRange() {
        return SourceRange(SourceLocation(1), SourceLocation(1));
    }

    SourceManager SM;
    ASTContext Ctx{SM};
    DiagnosticEngine Diag{SM};
    SymbolTable Symbols{Ctx};
    TypeChecker Checker{Symbols, Diag, Ctx};
    StoredDiagnosticConsumer* StoredConsumer = nullptr;
};

TEST_F(TypeCheckerTest, CheckTypeCompatible_BasicAndIntegerWidening) {
    Type* i32Ty = Ctx.getI32Type();
    Type* i64Ty = Ctx.getI64Type();
    Type* u32Ty = Ctx.getU32Type();
    Type* u64Ty = Ctx.getU64Type();

    EXPECT_TRUE(Checker.checkTypeCompatible(i32Ty, i32Ty, testRange()));
    EXPECT_TRUE(Checker.checkTypeCompatible(i64Ty, i32Ty, testRange()));
    EXPECT_TRUE(Checker.checkTypeCompatible(u64Ty, u32Ty, testRange()));
    EXPECT_FALSE(Checker.checkTypeCompatible(i64Ty, u32Ty, testRange()));
}

TEST_F(TypeCheckerTest, CheckTypeCompatible_OptionalAndReferenceValue) {
    Type* i32Ty = Ctx.getI32Type();
    Type* optI32 = OptionalType::get(Ctx, i32Ty);
    Type* optNone = OptionalType::get(Ctx, Ctx.getVoidType());
    Type* refI32 = ReferenceType::get(Ctx, i32Ty, false);

    EXPECT_TRUE(Checker.checkTypeCompatible(optI32, optNone, testRange()));
    EXPECT_TRUE(Checker.checkTypeCompatible(refI32, i32Ty, testRange()));
}

TEST_F(TypeCheckerTest, CheckAssignable_LValueAndVarArgsIndex) {
    SourceRange range = testRange();

    IdentifierExpr target(range, "x");
    EXPECT_TRUE(Checker.checkAssignable(&target, range.getBegin()));

    IntegerLiteralExpr literal(range, 42, true, 32);
    EXPECT_FALSE(Checker.checkAssignable(&literal, range.getBegin()));

    IdentifierExpr base(range, "args");
    base.setType(VarArgsType::get(Ctx, Ctx.getI32Type()));
    IntegerLiteralExpr index(range, 0, true, 32);
    IndexExpr indexExpr(range, &base, &index);
    EXPECT_FALSE(Checker.checkAssignable(&indexExpr, range.getBegin()));
}

TEST_F(TypeCheckerTest, CheckMutable_ImmutableAndMutableCases) {
    SourceRange range = testRange();

    Symbol immVar(SymbolKind::Variable, "imm_var", Ctx.getI32Type(), range.getBegin(),
                  Visibility::Private);
    immVar.setMutable(false);
    ASSERT_TRUE(Symbols.addSymbol(&immVar));

    IdentifierExpr immExpr(range, "imm_var");
    EXPECT_FALSE(Checker.checkMutable(&immExpr, range.getBegin()));

    Symbol constVar(SymbolKind::Constant, "const_val", Ctx.getI32Type(), range.getBegin(),
                    Visibility::Private);
    constVar.setMutable(false);
    ASSERT_TRUE(Symbols.addSymbol(&constVar));

    IdentifierExpr constExpr(range, "const_val");
    EXPECT_FALSE(Checker.checkMutable(&constExpr, range.getBegin()));

    Symbol mutVar(SymbolKind::Variable, "mut_var", Ctx.getI32Type(), range.getBegin(),
                  Visibility::Private);
    mutVar.setMutable(true);
    ASSERT_TRUE(Symbols.addSymbol(&mutVar));

    IdentifierExpr mutExpr(range, "mut_var");
    EXPECT_TRUE(Checker.checkMutable(&mutExpr, range.getBegin()));

    IdentifierExpr refBase(range, "ref_base");
    refBase.setType(ReferenceType::get(Ctx, Ctx.getI32Type(), false));
    UnaryExpr derefExpr(range, UnaryExpr::Op::Deref, &refBase);
    EXPECT_FALSE(Checker.checkMutable(&derefExpr, range.getBegin()));
}

TEST_F(TypeCheckerTest, GetCommonType_NumericAndOptional) {
    Type* i32Ty = Ctx.getI32Type();
    Type* i64Ty = Ctx.getI64Type();
    Type* f64Ty = Ctx.getF64Type();
    Type* u64Ty = Ctx.getU64Type();
    Type* optI32 = OptionalType::get(Ctx, i32Ty);

    EXPECT_EQ(Checker.getCommonType(i32Ty, f64Ty), f64Ty);
    EXPECT_EQ(Checker.getCommonType(i32Ty, i64Ty), i64Ty);
    EXPECT_EQ(Checker.getCommonType(i64Ty, u64Ty), nullptr);
    EXPECT_EQ(Checker.getCommonType(optI32, i32Ty), optI32);
}

TEST_F(TypeCheckerTest, EvaluateConstExpr_LiteralsUnaryBinaryAndConstIdentifier) {
    SourceRange range = testRange();

    IntegerLiteralExpr lit(range, 7, true, 32);
    int64_t result = 0;
    EXPECT_TRUE(Checker.evaluateConstExpr(&lit, result));
    EXPECT_EQ(result, 7);

    UnaryExpr negExpr(range, UnaryExpr::Op::Neg, &lit);
    EXPECT_TRUE(Checker.evaluateConstExpr(&negExpr, result));
    EXPECT_EQ(result, -7);

    IntegerLiteralExpr rhs(range, 5, true, 32);
    BinaryExpr addExpr(range, BinaryExpr::Op::Add, &lit, &rhs);
    EXPECT_TRUE(Checker.evaluateConstExpr(&addExpr, result));
    EXPECT_EQ(result, 12);

    IntegerLiteralExpr constInit(range, 9, true, 32);
    ConstDecl constDecl(range, "MY_CONST", nullptr, &constInit, Visibility::Private);
    Symbol constSym(SymbolKind::Constant, "MY_CONST", Ctx.getI32Type(), range.getBegin(),
                    Visibility::Private);
    constSym.setDecl(&constDecl);
    ASSERT_TRUE(Symbols.addSymbol(&constSym));

    IdentifierExpr constRef(range, "MY_CONST");
    EXPECT_TRUE(Checker.evaluateConstExpr(&constRef, result));
    EXPECT_EQ(result, 9);
}

TEST_F(TypeCheckerTest, EvaluateConstExpr_DivisionByZeroReportsDedicatedDiagnostic) {
    SourceRange range = testRange();
    IntegerLiteralExpr lhs(range, 10, true, 32);
    IntegerLiteralExpr rhsZero(range, 0, true, 32);
    int64_t result = 0;

    BinaryExpr divExpr(range, BinaryExpr::Op::Div, &lhs, &rhsZero);
    EXPECT_FALSE(Checker.evaluateConstExpr(&divExpr, result));
    ASSERT_FALSE(StoredConsumer->getDiagnostics().empty());
    EXPECT_EQ(StoredConsumer->getDiagnostics().back().getID(), DiagID::err_division_by_zero);

    StoredConsumer->clear();

    BinaryExpr modExpr(range, BinaryExpr::Op::Mod, &lhs, &rhsZero);
    EXPECT_FALSE(Checker.evaluateConstExpr(&modExpr, result));
    ASSERT_FALSE(StoredConsumer->getDiagnostics().empty());
    EXPECT_EQ(StoredConsumer->getDiagnostics().back().getID(), DiagID::err_division_by_zero);
}

TEST_F(TypeCheckerTest, IsCopyType_AggregatesWithRepeatedElementType) {
    Type* i32Ty = Ctx.getI32Type();
    Type* tupleTy = TupleType::get(Ctx, {i32Ty, i32Ty});
    EXPECT_TRUE(Checker.isCopyType(tupleTy));

    std::vector<StructType::Field> fields;
    fields.emplace_back("x", i32Ty, 0);
    fields.emplace_back("y", i32Ty, 4);
    Type* pairTy = StructType::get(Ctx, "PairI32", std::move(fields));
    EXPECT_TRUE(Checker.isCopyType(pairTy));
}

TEST_F(TypeCheckerTest, NeedsDrop_OnlyExplicitValidDropImpl) {
    SourceRange range = testRange();

    // 正确 Drop: drop(&mut self) -> void
    Type* resourceTy = StructType::get(Ctx, "Resource", {});
    ParamDecl* validSelf = ParamDecl::createSelf(range, ParamDecl::ParamKind::MutRefSelf);
    std::vector<ParamDecl*> validParams = {validSelf};
    FuncDecl validDrop(range, "drop", std::move(validParams), nullptr, nullptr, false, false,
                       Visibility::Public);
    Type* validSelfTy = ReferenceType::get(Ctx, resourceTy, true);
    validDrop.setSemanticType(FunctionType::get(Ctx, {validSelfTy}, Ctx.getVoidType(), false));
    Ctx.registerImplMethod(resourceTy, &validDrop);

    EXPECT_TRUE(Checker.needsDrop(resourceTy));
    EXPECT_FALSE(Checker.isCopyType(resourceTy));

    // 非法 Drop: drop(&self) -> void 不应触发 needsDrop
    Type* badDropTy = StructType::get(Ctx, "BadDrop", {});
    ParamDecl* badSelf = ParamDecl::createSelf(range, ParamDecl::ParamKind::RefSelf);
    std::vector<ParamDecl*> badParams = {badSelf};
    FuncDecl badDrop(range, "drop", std::move(badParams), nullptr, nullptr, false, false,
                     Visibility::Public);
    Type* badSelfTy = ReferenceType::get(Ctx, badDropTy, false);
    badDrop.setSemanticType(FunctionType::get(Ctx, {badSelfTy}, Ctx.getVoidType(), false));
    Ctx.registerImplMethod(badDropTy, &badDrop);

    EXPECT_FALSE(Checker.needsDrop(badDropTy));
}

} // namespace yuan
