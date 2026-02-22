/// \file GenericInstantiationTest.cpp
/// \brief Sema tests for generic impl selection and instantiation checks.

#include "yuan/Parser/Parser.h"
#include "yuan/Lexer/Lexer.h"
#include "yuan/Sema/Sema.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/SourceManager.h"
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

namespace yuan {

class GenericInstantiationTest : public ::testing::Test {
protected:
    struct AnalyzeResult {
        bool Parsed = false;
        bool SemaOK = false;
        std::vector<DiagID> Diagnostics;
    };

    AnalyzeResult analyzeSource(const std::string& source) {
        AnalyzeResult result;

        SourceManager sm;
        DiagnosticEngine diag(sm);
        auto stored = std::make_unique<StoredDiagnosticConsumer>();
        StoredDiagnosticConsumer* storedPtr = stored.get();
        diag.setConsumer(std::move(stored));

        auto fileID = sm.createBuffer(source, "generic_instantiation_test.yu");
        ASTContext ctx(sm);
        Lexer lexer(sm, diag, fileID);
        Parser parser(lexer, diag, ctx);

        auto decls = parser.parseCompilationUnit();
        result.Parsed = !diag.hasErrors();

        CompilationUnit unit(fileID);
        for (Decl* decl : decls) {
            unit.addDecl(decl);
        }

        Sema sema(ctx, diag);
        result.SemaOK = sema.analyze(&unit) && !diag.hasErrors();

        for (const auto& d : storedPtr->getDiagnostics()) {
            result.Diagnostics.push_back(d.getID());
        }

        return result;
    }

    static bool hasDiag(const AnalyzeResult& result, DiagID id) {
        for (DiagID diag : result.Diagnostics) {
            if (diag == id) {
                return true;
            }
        }
        return false;
    }
};

TEST_F(GenericInstantiationTest, RejectsImplBoundWhenReceiverTypeDoesNotSatisfyTrait) {
    const std::string source = R"(
trait RenderX {
    func render(&self) -> str
}

struct Bad {}
struct Wrap<T> { value: T }

impl<T: RenderX> RenderX for Wrap<T> {
    func render(&self) -> str { return "wrap" }
}

func use_bad(x: Wrap<Bad>) -> str {
    return x.render()
}
)";

    AnalyzeResult result = analyzeSource(source);
    EXPECT_FALSE(result.SemaOK);
    EXPECT_TRUE(hasDiag(result, DiagID::err_field_not_found) ||
                hasDiag(result, DiagID::err_trait_not_implemented));
}

TEST_F(GenericInstantiationTest, GenericParamNamesInDifferentScopesDoNotPolluteConstraints) {
    const std::string source = R"(
trait A {
    func a(&self) -> str
}

trait B {
    func b(&self) -> str
}

struct OnlyB {}

impl B for OnlyB {
    func b(&self) -> str { return "b" }
}

func f<T: A>(x: T) -> str {
    return x.a()
}

func g<T: B>(x: T) -> str {
    return x.b()
}

func test(v: OnlyB) -> str {
    return g(v)
}
)";

    AnalyzeResult result = analyzeSource(source);
    EXPECT_TRUE(result.Parsed);
    EXPECT_TRUE(result.SemaOK);
}

TEST_F(GenericInstantiationTest, TraitGenericParameterIsResolvedInImplAndDoesNotCrash) {
    const std::string source = R"(
trait From<T> {
    func from(value: T) -> Self
}

struct S {}

impl From<i32> for S {
    func from(value: i32) -> Self { return S {} }
}

func make() -> S {
    return S.from(1)
}
)";

    AnalyzeResult result = analyzeSource(source);
    EXPECT_TRUE(result.Parsed);
    EXPECT_TRUE(result.SemaOK);
}

TEST_F(GenericInstantiationTest, GenericImplMethodTypeSubstitutionWorksForConcreteCall) {
    const std::string source = R"(
struct Wrap<T> { value: T }

impl<T> Wrap<T> {
    func get(&self) -> T { return self.value }
}

func call_i32(w: Wrap<i32>) -> i32 {
    return w.get()
}
)";

    AnalyzeResult result = analyzeSource(source);
    EXPECT_TRUE(result.Parsed);
    EXPECT_TRUE(result.SemaOK);
}

TEST_F(GenericInstantiationTest, OperatorAddUsesTraitImpl) {
    const std::string source = R"(
struct Box {
    value: i32,
}

impl Add for Box {
    func add(&self, other: &Self) -> Self {
        return Box { value: self.value + other.value }
    }
}

func combine(a: Box, b: Box) -> Box {
    return a + b
}
)";

    AnalyzeResult result = analyzeSource(source);
    EXPECT_TRUE(result.Parsed);
    EXPECT_TRUE(result.SemaOK);
}

TEST_F(GenericInstantiationTest, OperatorAddDoesNotFallbackToInherentMethod) {
    const std::string source = R"(
struct Counter {
    value: i32,
}

impl Counter {
    func add(&self, other: &Self) -> Self {
        return Counter { value: self.value + other.value }
    }
}

func combine(a: Counter, b: Counter) -> Counter {
    return a + b
}
)";

    AnalyzeResult result = analyzeSource(source);
    EXPECT_FALSE(result.SemaOK);
    EXPECT_TRUE(hasDiag(result, DiagID::err_trait_not_implemented));
}

TEST_F(GenericInstantiationTest, ComparisonOperatorsUseIndependentTraits) {
    const std::string source = R"(
struct Score {
    value: i32,
}

impl Eq for Score {
    func eq(&self, other: &Self) -> bool { return self.value == other.value }
}

impl Ne for Score {
    func ne(&self, other: &Self) -> bool { return self.value != other.value }
}

impl Lt for Score {
    func lt(&self, other: &Self) -> bool { return self.value < other.value }
}

impl Le for Score {
    func le(&self, other: &Self) -> bool { return self.value <= other.value }
}

impl Gt for Score {
    func gt(&self, other: &Self) -> bool { return self.value > other.value }
}

impl Ge for Score {
    func ge(&self, other: &Self) -> bool { return self.value >= other.value }
}

func compare_all(a: Score, b: Score) -> bool {
    return a != b && a < b && a <= b && a > b && a >= b && a == b
}
)";

    AnalyzeResult result = analyzeSource(source);
    EXPECT_TRUE(result.Parsed);
    EXPECT_TRUE(result.SemaOK);
}

TEST_F(GenericInstantiationTest, UnaryOperatorsUseTraitImpls) {
    const std::string source = R"(
struct Vec2 {
    x: i32,
}

impl Neg for Vec2 {
    func neg(&self) -> Self {
        return Vec2 { x: 0 - self.x }
    }
}

struct Flag {
    set: bool,
}

impl Not for Flag {
    func not(&self) -> bool {
        return self.set
    }
}

struct Mask {
    bits: i32,
}

impl BitNot for Mask {
    func bit_not(&self) -> Self {
        return Mask { bits: ~self.bits }
    }
}

func use_neg(v: Vec2) -> Vec2 { return -v }
func use_not(v: Flag) -> bool { return !v }
func use_bit_not(v: Mask) -> Mask { return ~v }
)";

    AnalyzeResult result = analyzeSource(source);
    EXPECT_TRUE(result.Parsed);
    EXPECT_TRUE(result.SemaOK);
}

TEST_F(GenericInstantiationTest, RejectsOperatorTraitImplForBuiltinType) {
    const std::string source = R"(
impl Add for i32 {
    func add(&self, other: &Self) -> Self {
        return 0
    }
}
)";

    AnalyzeResult result = analyzeSource(source);
    EXPECT_FALSE(result.SemaOK);
    EXPECT_TRUE(hasDiag(result, DiagID::err_builtin_operator_overload_forbidden));
}

TEST_F(GenericInstantiationTest, GenericBoundSupportsOperatorTraitOnlyResolution) {
    const std::string source = R"(
func add_values<T: Add>(a: T, b: T) -> T {
    return a + b
}
)";

    AnalyzeResult result = analyzeSource(source);
    EXPECT_TRUE(result.Parsed);
    EXPECT_TRUE(result.SemaOK);
}

} // namespace yuan
