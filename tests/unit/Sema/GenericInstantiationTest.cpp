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

} // namespace yuan

