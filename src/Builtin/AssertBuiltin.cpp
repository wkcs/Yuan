/// \file AssertBuiltin.cpp
/// \brief @assert 内置函数实现。
///
/// @assert 用于运行时断言，如果条件为假则触发 panic，例如：
/// - @assert(x > 0)
/// - @assert(ptr != nullptr, "pointer is null")

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

/// \brief @assert 内置函数处理器
///
/// 运行时断言，如果条件为假则触发 panic。
/// 参数：
/// - 第一个参数：布尔条件表达式
/// - 第二个参数（可选）：错误消息字符串
class AssertBuiltin : public BuiltinHandler {
public:
    const char* getName() const override { return "assert"; }
    
    BuiltinKind getKind() const override { return BuiltinKind::Assert; }
    
    int getExpectedArgCount() const override { 
        return -1;  // 可变参数：1 或 2 个参数
    }
    
    std::string getArgDescription() const override {
        return "条件表达式 [, 错误消息字符串]";
    }
    
    Type* analyze(BuiltinCallExpr* expr, Sema& sema) override {
        // 检查参数数量（1 或 2 个）
        size_t argCount = expr->getArgCount();
        if (argCount < 1 || argCount > 2) {
            sema.getDiagnostics()
                .report(DiagID::err_wrong_builtin_argument_count, expr->getBeginLoc(), expr->getRange())
                << "1 or 2" << static_cast<unsigned>(argCount);
            return nullptr;
        }

        const auto& condArg = expr->getArgs()[0];
        if (!condArg.isExpr()) {
            sema.getDiagnostics()
                .report(DiagID::err_expected_expression, expr->getBeginLoc(), expr->getRange());
            return nullptr;
        }

        // 检查第一个参数类型（必须是布尔）
        Expr* conditionExpr = condArg.getExpr();
        Type* condType = sema.analyzeExpr(conditionExpr);
        if (!condType || !condType->isBool()) {
            sema.getDiagnostics()
                .report(DiagID::err_type_mismatch, conditionExpr->getBeginLoc(), conditionExpr->getRange())
                << "bool"
                << (condType ? condType->toString() : "unknown");
            return nullptr;
        }

        // 如果有第二个参数，检查是否为字符串
        if (argCount == 2) {
            const auto& msgArg = expr->getArgs()[1];
            if (!msgArg.isExpr()) {
                sema.getDiagnostics()
                    .report(DiagID::err_expected_expression, expr->getBeginLoc(), expr->getRange());
                return nullptr;
            }
            Expr* msgExpr = msgArg.getExpr();
            Type* msgType = sema.analyzeExpr(msgExpr);
            if (!msgType || !msgType->isString()) {
                sema.getDiagnostics()
                    .report(DiagID::err_type_mismatch, msgExpr->getBeginLoc(), msgExpr->getRange())
                    << "str"
                    << (msgType ? msgType->toString() : "unknown");
                return nullptr;
            }
        }

        // @assert 返回 void 类型
        return sema.getContext().getVoidType();
    }
    
    llvm::Value* generate(BuiltinCallExpr* expr, CodeGen& codegen) override {
        if (expr->getArgCount() < 1 || expr->getArgCount() > 2) {
            return nullptr;
        }

        const auto& condArg = expr->getArgs()[0];
        if (!condArg.isExpr()) {
            return nullptr;
        }

        Expr* conditionExpr = condArg.getExpr();
        llvm::Value* condition = codegen.generateExprPublic(conditionExpr);
        if (!condition) {
            return nullptr;
        }

        llvm::IRBuilder<>& builder = codegen.getBuilder();
        llvm::LLVMContext& context = codegen.getContext();
        llvm::Module* module = codegen.getModule();

        llvm::BasicBlock* currentBB = builder.GetInsertBlock();
        if (!currentBB) {
            return nullptr;
        }
        llvm::Function* func = currentBB->getParent();
        if (!func) {
            return nullptr;
        }

        llvm::BasicBlock* failBB = llvm::BasicBlock::Create(context, "assert.fail", func);
        llvm::BasicBlock* contBB = llvm::BasicBlock::Create(context, "assert.cont", func);

        builder.CreateCondBr(condition, contBB, failBB);

        // 失败分支
        builder.SetInsertPoint(failBB);

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

        if (expr->getArgCount() == 2) {
            const auto& msgArg = expr->getArgs()[1];
            if (msgArg.isExpr()) {
                llvm::Value* msgValue = codegen.generateExprPublic(msgArg.getExpr());
                if (msgValue) {
                    llvm::Value* msgPtr = nullptr;
                    if (msgValue->getType()->isStructTy()) {
                        msgPtr = builder.CreateExtractValue(msgValue, 0, "assert.msg.ptr");
                    } else if (msgValue->getType()->isPointerTy()) {
                        msgPtr = msgValue;
                    }

                    if (msgPtr) {
                        const std::string fmt = "assertion failed: %s\n";
                        llvm::Constant* fmtConst = llvm::ConstantDataArray::getString(context, fmt, true);
                        llvm::GlobalVariable* fmtGlobal = new llvm::GlobalVariable(
                            *module,
                            fmtConst->getType(),
                            true,
                            llvm::GlobalValue::PrivateLinkage,
                            fmtConst,
                            ".assert.fmt"
                        );
                        llvm::Constant* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
                        llvm::Constant* indices[] = {zero, zero};
                        llvm::Constant* fmtPtr = llvm::ConstantExpr::getGetElementPtr(
                            fmtGlobal->getValueType(), fmtGlobal, indices
                        );
                        builder.CreateCall(printfFunc, {fmtPtr, msgPtr});
                    }
                }
            }
        } else {
            const std::string msg = "assertion failed\n";
            llvm::Constant* msgConst = llvm::ConstantDataArray::getString(context, msg, true);
            llvm::GlobalVariable* msgGlobal = new llvm::GlobalVariable(
                *module,
                msgConst->getType(),
                true,
                llvm::GlobalValue::PrivateLinkage,
                msgConst,
                ".assert.msg"
            );
            llvm::Constant* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
            llvm::Constant* indices[] = {zero, zero};
            llvm::Constant* msgPtr = llvm::ConstantExpr::getGetElementPtr(
                msgGlobal->getValueType(), msgGlobal, indices
            );
            builder.CreateCall(printfFunc, {msgPtr});
        }

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
        builder.CreateCall(abortFunc);
        builder.CreateUnreachable();

        // 继续分支
        builder.SetInsertPoint(contBB);

        return nullptr;
    }
};

/// \brief 创建 @assert 内置函数处理器
std::unique_ptr<BuiltinHandler> createAssertBuiltin() {
    return std::make_unique<AssertBuiltin>();
}

} // namespace yuan
