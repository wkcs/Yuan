/// \file GenericMonomorphTest.cpp
/// \brief Integration tests for generic monomorphization in codegen.

#include "yuan/CodeGen/CodeGen.h"
#include "yuan/Parser/Parser.h"
#include "yuan/Lexer/Lexer.h"
#include "yuan/Sema/Sema.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Decl.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/SourceManager.h"
#include <gtest/gtest.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <memory>
#include <set>
#include <string>

namespace yuan {

class GenericMonomorphTest : public ::testing::Test {
protected:
    bool compileSource(const std::string& source) {
        SourceMgr = std::make_unique<SourceManager>();
        Diag = std::make_unique<DiagnosticEngine>(*SourceMgr);
        auto stored = std::make_unique<StoredDiagnosticConsumer>();
        StoredConsumer = stored.get();
        Diag->setConsumer(std::move(stored));

        auto fileID = SourceMgr->createBuffer(source, "generic_monomorph_test.yu");
        Ctx = std::make_unique<ASTContext>(*SourceMgr);

        Lexer lexer(*SourceMgr, *Diag, fileID);
        Parser parser(lexer, *Diag, *Ctx);
        Decls = parser.parseCompilationUnit();
        if (Diag->hasErrors()) {
            return false;
        }

        CompilationUnit unit(fileID);
        for (Decl* decl : Decls) {
            unit.addDecl(decl);
        }

        Sema sema(*Ctx, *Diag);
        if (!sema.analyze(&unit) || Diag->hasErrors()) {
            return false;
        }

        CodeGenerator = std::make_unique<CodeGen>(*Ctx, "generic_monomorph_test");
        for (Decl* decl : Decls) {
            if (!CodeGenerator->generateDecl(decl)) {
                return false;
            }
        }

        return !Diag->hasErrors();
    }

    FuncDecl* findFunctionDecl(const std::string& name) const {
        for (Decl* decl : Decls) {
            auto* fn = dynamic_cast<FuncDecl*>(decl);
            if (fn && fn->getName() == name) {
                return fn;
            }
        }
        return nullptr;
    }

    ImplDecl* findImplForMethod(const std::string& methodName) const {
        for (Decl* decl : Decls) {
            auto* impl = dynamic_cast<ImplDecl*>(decl);
            if (!impl) {
                continue;
            }
            if (impl->findMethod(methodName)) {
                return impl;
            }
        }
        return nullptr;
    }

    static bool startsWith(const std::string& value, const std::string& prefix) {
        return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
    }

    std::unique_ptr<SourceManager> SourceMgr;
    std::unique_ptr<DiagnosticEngine> Diag;
    std::unique_ptr<ASTContext> Ctx;
    std::unique_ptr<CodeGen> CodeGenerator;
    StoredDiagnosticConsumer* StoredConsumer = nullptr;
    std::vector<Decl*> Decls;
};

TEST_F(GenericMonomorphTest, GenericFunctionGeneratesMultipleSpecializations) {
    const std::string source = R"(
func id<T>(x: T) -> T { return x }

func call_i32(v: i32) -> i32 { return id(v) }
func call_i64(v: i64) -> i64 { return id(v) }
)";

    ASSERT_TRUE(compileSource(source));

    FuncDecl* idDecl = findFunctionDecl("id");
    ASSERT_NE(idDecl, nullptr);
    std::string baseName = CodeGenerator->getFunctionSymbolName(idDecl);
    std::string prefix = baseName + "_S";

    unsigned specCount = 0;
    for (const llvm::Function& fn : CodeGenerator->getModule()->functions()) {
        if (startsWith(fn.getName().str(), prefix)) {
            ++specCount;
        }
    }

    EXPECT_GE(specCount, 2u);
}

TEST_F(GenericMonomorphTest, GenericEnumHasDistinctSpecializedLLVMTypes) {
    const std::string source = R"(
enum Option<T> {
    Some(T),
    None
}

func take_i32(v: Option<i32>) -> i32 { return 0 }
func take_i64(v: Option<i64>) -> i64 { return 0 }
)";

    ASSERT_TRUE(compileSource(source));

    std::set<std::string> enumTypeNames;
    for (llvm::StructType* ty : CodeGenerator->getModule()->getIdentifiedStructTypes()) {
        if (!ty || !ty->hasName()) {
            continue;
        }
        std::string name = ty->getName().str();
        if (startsWith(name, "_YE_")) {
            enumTypeNames.insert(name);
        }
    }

    EXPECT_GE(enumTypeNames.size(), 2u);
}

TEST_F(GenericMonomorphTest, GenericImplMethodCallTargetsSpecializedSymbol) {
    const std::string source = R"(
struct Wrap<T> { value: T }

impl<T> Wrap<T> {
    func get(&self) -> T { return self.value }
}

func call_i32(w: Wrap<i32>) -> i32 {
    return w.get()
}
)";

    ASSERT_TRUE(compileSource(source));

    ImplDecl* impl = findImplForMethod("get");
    ASSERT_NE(impl, nullptr);
    FuncDecl* getDecl = impl->findMethod("get");
    ASSERT_NE(getDecl, nullptr);

    FuncDecl* callerDecl = findFunctionDecl("call_i32");
    ASSERT_NE(callerDecl, nullptr);

    std::string methodBase = CodeGenerator->getFunctionSymbolName(getDecl);
    std::string specPrefix = methodBase + "_S";
    std::string callerName = CodeGenerator->getFunctionSymbolName(callerDecl);

    llvm::Function* callerFn = CodeGenerator->getModule()->getFunction(callerName);
    ASSERT_NE(callerFn, nullptr);

    bool calledSpecialized = false;
    bool calledUnspecialized = false;
    for (const llvm::BasicBlock& bb : *callerFn) {
        for (const llvm::Instruction& inst : bb) {
            auto* call = llvm::dyn_cast<llvm::CallBase>(&inst);
            if (!call) {
                continue;
            }
            llvm::Function* callee = call->getCalledFunction();
            if (!callee) {
                continue;
            }
            std::string calleeName = callee->getName().str();
            if (startsWith(calleeName, specPrefix)) {
                calledSpecialized = true;
            }
            if (calleeName == methodBase) {
                calledUnspecialized = true;
            }
        }
    }

    EXPECT_TRUE(calledSpecialized);
    EXPECT_FALSE(calledUnspecialized);
}

} // namespace yuan
