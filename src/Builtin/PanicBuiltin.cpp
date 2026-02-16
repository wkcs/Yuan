/// \file PanicBuiltin.cpp
/// \brief @panic 内置函数实现。
///
/// @panic 用于触发运行时 panic，终止程序执行，例如：
/// - @panic("unexpected error")
/// - @panic("index out of bounds")

#include "yuan/Builtin/BuiltinHandler.h"
#include "yuan/Builtin/BuiltinRegistry.h"
#include "yuan/AST/Expr.h"
#include "yuan/Sema/Sema.h"
#include "yuan/Sema/Type.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/CodeGen/CodeGen.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/GlobalVariable.h>

namespace yuan {

/// \brief @panic 内置函数处理器
///
/// 触发运行时 panic，终止程序执行。
/// 参数是错误消息字符串。
/// 返回 never 类型（函数不会正常返回）。
class PanicBuiltin : public BuiltinHandler {
public:
    const char* getName() const override { return "panic"; }
    
    BuiltinKind getKind() const override { return BuiltinKind::Panic; }
    
    int getExpectedArgCount() const override { return 1; }
    
    std::string getArgDescription() const override {
        return "错误消息字符串";
    }
    
    Type* analyze(BuiltinCallExpr* expr, Sema& sema) override {
        // 检查参数数量
        if (expr->getArgCount() != 1) {
            sema.getDiagnostics()
                .report(DiagID::err_wrong_builtin_argument_count, expr->getBeginLoc(), expr->getRange())
                << 1u << static_cast<unsigned>(expr->getArgCount());
            return nullptr;
        }

        const auto& arg = expr->getArgs()[0];
        if (!arg.isExpr()) {
            sema.getDiagnostics()
                .report(DiagID::err_expected_expression, expr->getBeginLoc(), expr->getRange());
            return nullptr;
        }

        // 检查参数类型（必须是字符串）
        Expr* argExpr = arg.getExpr();
        Type* argType = sema.analyzeExpr(argExpr);
        if (!argType || !argType->isString()) {
            sema.getDiagnostics()
                .report(DiagID::err_type_mismatch, argExpr->getBeginLoc(), argExpr->getRange())
                << "str"
                << (argType ? argType->toString() : "unknown");
            return nullptr;
        }

        // @panic 返回 void 类型（不正常返回）
        return sema.getContext().getVoidType();
    }
    
    llvm::Value* generate(BuiltinCallExpr* expr, CodeGen& codegen) override {
        if (expr->getArgCount() != 1) {
            return nullptr;
        }

        const auto& arg = expr->getArgs()[0];
        if (!arg.isExpr()) {
            return nullptr;
        }

        Expr* argExpr = arg.getExpr();
        llvm::Value* argValue = codegen.generateExprPublic(argExpr);
        if (!argValue) {
            return nullptr;
        }

        llvm::LLVMContext& context = codegen.getContext();
        llvm::Module* module = codegen.getModule();

        // 提取字符串指针
        llvm::Value* strPtr = nullptr;
        if (argValue->getType()->isStructTy()) {
            strPtr = codegen.getBuilder().CreateExtractValue(argValue, 0, "panic.str.ptr");
        } else if (argValue->getType()->isPointerTy()) {
            strPtr = argValue;
        } else {
            return nullptr;
        }

        // 声明 printf
        llvm::Function* printfFunc = module->getFunction("printf");
        if (!printfFunc) {
            llvm::FunctionType* printfType = llvm::FunctionType::get(
                llvm::Type::getInt32Ty(context),
                llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                true
            );
            printfFunc = llvm::Function::Create(
                printfType,
                llvm::Function::ExternalLinkage,
                "printf",
                module
            );
        }

        // 创建格式字符串 "panic: %s\n"
        const std::string fmt = "panic: %s\n";
        llvm::Constant* fmtConst = llvm::ConstantDataArray::getString(context, fmt, true);
        llvm::GlobalVariable* fmtGlobal = new llvm::GlobalVariable(
            *module,
            fmtConst->getType(),
            true,
            llvm::GlobalValue::PrivateLinkage,
            fmtConst,
            ".panic.fmt"
        );
        llvm::Constant* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
        llvm::Constant* indices[] = {zero, zero};
        llvm::Constant* fmtPtr = llvm::ConstantExpr::getGetElementPtr(
            fmtGlobal->getValueType(), fmtGlobal, indices
        );

        codegen.getBuilder().CreateCall(printfFunc, {fmtPtr, strPtr});

        // 调用 abort
        llvm::Function* abortFunc = module->getFunction("abort");
        if (!abortFunc) {
            llvm::FunctionType* abortType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context),
                false
            );
            abortFunc = llvm::Function::Create(
                abortType,
                llvm::Function::ExternalLinkage,
                "abort",
                module
            );
        }
        codegen.getBuilder().CreateCall(abortFunc);
        codegen.getBuilder().CreateUnreachable();

        return nullptr;
    }
};

/// \brief 创建 @panic 内置函数处理器
std::unique_ptr<BuiltinHandler> createPanicBuiltin() {
    return std::make_unique<PanicBuiltin>();
}

} // namespace yuan
