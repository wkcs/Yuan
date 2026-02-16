/// \file PlatformBuiltin.cpp
/// \brief 平台信息内置函数实现。

#include "yuan/Builtin/BuiltinHandler.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/Sema/Sema.h"
#include "yuan/Sema/Type.h"
#include "yuan/CodeGen/CodeGen.h"
#include "yuan/Basic/Diagnostic.h"
#include <llvm/IR/Constants.h>

namespace yuan {
namespace {

static const char* detectOS() {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#elif defined(__FreeBSD__)
    return "freebsd";
#else
    return "unknown";
#endif
}

static const char* detectArch() {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#elif defined(__riscv)
    return "riscv";
#else
    return "unknown";
#endif
}

class PlatformBuiltin final : public BuiltinHandler {
public:
    explicit PlatformBuiltin(BuiltinKind kind) : Kind(kind) {}

    const char* getName() const override {
        switch (Kind) {
            case BuiltinKind::PlatformOs: return "platform_os";
            case BuiltinKind::PlatformArch: return "platform_arch";
            case BuiltinKind::PlatformPointerBits: return "platform_pointer_bits";
            default: return "platform";
        }
    }

    BuiltinKind getKind() const override { return Kind; }

    int getExpectedArgCount() const override { return 0; }

    std::string getArgDescription() const override { return ""; }

    Type* analyze(BuiltinCallExpr* expr, Sema& sema) override {
        if (!expr) {
            return nullptr;
        }
        if (expr->getArgCount() != 0) {
            sema.getDiagnostics()
                .report(DiagID::err_wrong_builtin_argument_count, expr->getBeginLoc(), expr->getRange())
                << 0u << static_cast<unsigned>(expr->getArgCount());
            return nullptr;
        }

        ASTContext& ctx = sema.getContext();
        if (Kind == BuiltinKind::PlatformPointerBits) {
            return ctx.getIntegerType(ctx.getPointerBitWidth(), false);
        }
        return ctx.getStrType();
    }

    llvm::Value* generate(BuiltinCallExpr* expr, CodeGen& codegen) override {
        if (!expr || expr->getArgCount() != 0) {
            return nullptr;
        }

        if (Kind == BuiltinKind::PlatformPointerBits) {
            Type* ty = expr->getType();
            llvm::Type* llvmTy = codegen.getLLVMType(ty);
            if (!llvmTy || !llvmTy->isIntegerTy()) {
                return nullptr;
            }
            unsigned bits = codegen.getASTContext().getPointerBitWidth();
            return llvm::ConstantInt::get(llvmTy, bits);
        }

        const char* value = (Kind == BuiltinKind::PlatformOs) ? detectOS() : detectArch();
        return codegen.emitStringLiteralValue(value);
    }

private:
    BuiltinKind Kind;
};

} // namespace

std::unique_ptr<BuiltinHandler> createPlatformOsBuiltin() {
    return std::make_unique<PlatformBuiltin>(BuiltinKind::PlatformOs);
}

std::unique_ptr<BuiltinHandler> createPlatformArchBuiltin() {
    return std::make_unique<PlatformBuiltin>(BuiltinKind::PlatformArch);
}

std::unique_ptr<BuiltinHandler> createPlatformPointerBitsBuiltin() {
    return std::make_unique<PlatformBuiltin>(BuiltinKind::PlatformPointerBits);
}

} // namespace yuan
