/// \file AsyncBuiltin.cpp
/// \brief 异步运行时内置函数实现。

#include "yuan/Builtin/BuiltinHandler.h"
#include "yuan/Sema/Sema.h"
#include "yuan/Sema/Type.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/CodeGen/CodeGen.h"
#include "yuan/Basic/Diagnostic.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>

namespace yuan {

namespace {

static Type* getUSizeType(ASTContext& ctx) {
    return ctx.getIntegerType(ctx.getPointerBitWidth(), false);
}

static Type* unwrapAliases(Type* type) {
    Type* current = type;
    while (current && current->isTypeAlias()) {
        current = static_cast<TypeAlias*>(current)->getAliasedType();
    }
    return current;
}

static llvm::Type* getUSizeLLVMType(llvm::Module* module, llvm::LLVMContext& context) {
    if (module) {
        llvm::Type* ptrSized = module->getDataLayout().getIntPtrType(context);
        if (ptrSized && ptrSized->isIntegerTy()) {
            return ptrSized;
        }
    }
    return llvm::Type::getInt64Ty(context);
}

static llvm::Value* castIntegerValue(llvm::Value* value,
                                     llvm::Type* targetType,
                                     llvm::IRBuilder<>& builder,
                                     const char* name) {
    if (!value || !targetType || value->getType() == targetType) {
        return value;
    }

    if (value->getType()->isIntegerTy() && targetType->isIntegerTy()) {
        return builder.CreateZExtOrTrunc(value, targetType, name);
    }

    if (value->getType()->isPointerTy() && targetType->isIntegerTy()) {
        return builder.CreatePtrToInt(value, targetType, name);
    }

    if (value->getType()->isIntegerTy() && targetType->isPointerTy()) {
        return builder.CreateIntToPtr(value, targetType, name);
    }

    return builder.CreateBitCast(value, targetType, name);
}

static llvm::Value* castHandleToRuntimePtr(llvm::Value* handle,
                                           llvm::Type* runtimePtrTy,
                                           llvm::Type* usizeTy,
                                           llvm::IRBuilder<>& builder) {
    if (!handle) {
        return nullptr;
    }

    if (handle->getType() == runtimePtrTy) {
        return handle;
    }

    if (handle->getType()->isPointerTy()) {
        return builder.CreateBitCast(handle, runtimePtrTy, "async.handle.ptr.cast");
    }

    llvm::Value* asUSize = castIntegerValue(handle, usizeTy, builder, "async.handle.usize");
    return builder.CreateIntToPtr(asUSize, runtimePtrTy, "async.handle.ptr");
}

static llvm::Value* castRuntimePtrToHandle(llvm::Value* ptrValue,
                                           llvm::Type* handleTy,
                                           llvm::Type* usizeTy,
                                           llvm::IRBuilder<>& builder) {
    if (!ptrValue) {
        return nullptr;
    }

    llvm::Type* targetTy = (handleTy && handleTy->isIntegerTy()) ? handleTy : usizeTy;
    return castIntegerValue(ptrValue, targetTy, builder, "async.handle.int");
}

class AsyncBuiltin : public BuiltinHandler {
public:
    explicit AsyncBuiltin(BuiltinKind kind) : Kind(kind) {}

    const char* getName() const override {
        switch (Kind) {
            case BuiltinKind::AsyncSchedulerCreate: return "async_scheduler_create";
            case BuiltinKind::AsyncSchedulerDestroy: return "async_scheduler_destroy";
            case BuiltinKind::AsyncSchedulerSetCurrent: return "async_scheduler_set_current";
            case BuiltinKind::AsyncSchedulerCurrent: return "async_scheduler_current";
            case BuiltinKind::AsyncSchedulerRunOne: return "async_scheduler_run_one";
            case BuiltinKind::AsyncSchedulerRunUntilIdle: return "async_scheduler_run_until_idle";
            case BuiltinKind::AsyncPromiseCreate: return "async_promise_create";
            case BuiltinKind::AsyncPromiseRetain: return "async_promise_retain";
            case BuiltinKind::AsyncPromiseRelease: return "async_promise_release";
            case BuiltinKind::AsyncPromiseStatus: return "async_promise_status";
            case BuiltinKind::AsyncPromiseValue: return "async_promise_value";
            case BuiltinKind::AsyncPromiseError: return "async_promise_error";
            case BuiltinKind::AsyncPromiseResolve: return "async_promise_resolve";
            case BuiltinKind::AsyncPromiseReject: return "async_promise_reject";
            case BuiltinKind::AsyncPromiseAwait: return "async_promise_await";
            case BuiltinKind::AsyncStep: return "async_step";
            case BuiltinKind::AsyncStepCount: return "async_step_count";
            default: return "async";
        }
    }

    BuiltinKind getKind() const override { return Kind; }

    int getExpectedArgCount() const override {
        switch (Kind) {
            case BuiltinKind::AsyncSchedulerCreate:
            case BuiltinKind::AsyncSchedulerCurrent:
            case BuiltinKind::AsyncPromiseCreate:
            case BuiltinKind::AsyncStep:
            case BuiltinKind::AsyncStepCount:
                return 0;
            case BuiltinKind::AsyncSchedulerDestroy:
            case BuiltinKind::AsyncSchedulerSetCurrent:
            case BuiltinKind::AsyncSchedulerRunOne:
            case BuiltinKind::AsyncSchedulerRunUntilIdle:
            case BuiltinKind::AsyncPromiseRetain:
            case BuiltinKind::AsyncPromiseRelease:
            case BuiltinKind::AsyncPromiseStatus:
            case BuiltinKind::AsyncPromiseValue:
            case BuiltinKind::AsyncPromiseError:
            case BuiltinKind::AsyncPromiseAwait:
                return 1;
            case BuiltinKind::AsyncPromiseResolve:
            case BuiltinKind::AsyncPromiseReject:
                return 2;
            default:
                return -1;
        }
    }

    std::string getArgDescription() const override {
        switch (Kind) {
            case BuiltinKind::AsyncSchedulerCreate:
            case BuiltinKind::AsyncSchedulerCurrent:
            case BuiltinKind::AsyncPromiseCreate:
            case BuiltinKind::AsyncStep:
            case BuiltinKind::AsyncStepCount:
                return "";
            case BuiltinKind::AsyncSchedulerDestroy:
            case BuiltinKind::AsyncSchedulerSetCurrent:
            case BuiltinKind::AsyncSchedulerRunOne:
            case BuiltinKind::AsyncSchedulerRunUntilIdle:
                return "scheduler";
            case BuiltinKind::AsyncPromiseRetain:
            case BuiltinKind::AsyncPromiseRelease:
            case BuiltinKind::AsyncPromiseStatus:
            case BuiltinKind::AsyncPromiseValue:
            case BuiltinKind::AsyncPromiseError:
            case BuiltinKind::AsyncPromiseAwait:
                return "promise";
            case BuiltinKind::AsyncPromiseResolve:
                return "promise, value";
            case BuiltinKind::AsyncPromiseReject:
                return "promise, error";
            default:
                return "";
        }
    }

    Type* analyze(BuiltinCallExpr* expr, Sema& sema) override {
        if (!expr) {
            return nullptr;
        }

        int expected = getExpectedArgCount();
        if (expected >= 0 && static_cast<int>(expr->getArgCount()) != expected) {
            sema.getDiagnostics()
                .report(DiagID::err_wrong_builtin_argument_count, expr->getBeginLoc(), expr->getRange())
                << static_cast<unsigned>(expected)
                << static_cast<unsigned>(expr->getArgCount());
            return nullptr;
        }

        auto requireIntegerArg = [&](unsigned index) -> bool {
            if (index >= expr->getArgCount()) {
                return false;
            }
            const auto& arg = expr->getArgs()[index];
            if (!arg.isExpr()) {
                return false;
            }

            Expr* argExpr = arg.getExpr();
            Type* argType = sema.analyzeExpr(argExpr);
            if (!argType) {
                return false;
            }
            argType = unwrapAliases(argType);

            if (!argType->isInteger()) {
                sema.getDiagnostics()
                    .report(DiagID::err_type_mismatch, argExpr->getBeginLoc(), argExpr->getRange())
                    << "integer"
                    << argType->toString();
                return false;
            }
            return true;
        };

        switch (Kind) {
            case BuiltinKind::AsyncSchedulerDestroy:
            case BuiltinKind::AsyncSchedulerSetCurrent:
            case BuiltinKind::AsyncSchedulerRunOne:
            case BuiltinKind::AsyncSchedulerRunUntilIdle:
            case BuiltinKind::AsyncPromiseRetain:
            case BuiltinKind::AsyncPromiseRelease:
            case BuiltinKind::AsyncPromiseStatus:
            case BuiltinKind::AsyncPromiseValue:
            case BuiltinKind::AsyncPromiseError:
            case BuiltinKind::AsyncPromiseAwait:
                if (!requireIntegerArg(0)) {
                    return nullptr;
                }
                break;
            case BuiltinKind::AsyncPromiseResolve:
            case BuiltinKind::AsyncPromiseReject:
                if (!requireIntegerArg(0) || !requireIntegerArg(1)) {
                    return nullptr;
                }
                break;
            default:
                break;
        }

        auto& ctx = sema.getContext();
        switch (Kind) {
            case BuiltinKind::AsyncSchedulerCreate:
            case BuiltinKind::AsyncSchedulerCurrent:
            case BuiltinKind::AsyncPromiseCreate:
            case BuiltinKind::AsyncPromiseValue:
            case BuiltinKind::AsyncPromiseError:
                return getUSizeType(ctx);
            case BuiltinKind::AsyncSchedulerRunOne:
            case BuiltinKind::AsyncPromiseStatus:
            case BuiltinKind::AsyncPromiseAwait:
                return ctx.getI32Type();
            case BuiltinKind::AsyncStepCount:
                return ctx.getU64Type();
            default:
                return ctx.getVoidType();
        }
    }

    llvm::Value* generate(BuiltinCallExpr* expr, CodeGen& codegen) override {
        if (!expr) {
            return nullptr;
        }

        llvm::Module* module = codegen.getModule();
        if (!module) {
            return nullptr;
        }

        llvm::LLVMContext& context = codegen.getContext();
        llvm::IRBuilder<>& builder = codegen.getBuilder();

        llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
        llvm::Type* i32Ty = llvm::Type::getInt32Ty(context);
        llvm::Type* i64Ty = llvm::Type::getInt64Ty(context);
        llvm::Type* usizeTy = getUSizeLLVMType(module, context);

        auto getIntResultTy = [&]() -> llvm::Type* {
            llvm::Type* resultTy = codegen.getLLVMType(expr->getType());
            if (resultTy && resultTy->isIntegerTy()) {
                return resultTy;
            }
            return usizeTy;
        };

        auto genExprArg = [&](unsigned index) -> llvm::Value* {
            if (index >= expr->getArgCount()) {
                return nullptr;
            }
            const auto& arg = expr->getArgs()[index];
            if (!arg.isExpr()) {
                return nullptr;
            }
            return codegen.generateExprPublic(arg.getExpr());
        };

        auto genRuntimeHandlePtrArg = [&](unsigned index) -> llvm::Value* {
            llvm::Value* handle = genExprArg(index);
            return castHandleToRuntimePtr(handle, i8PtrTy, usizeTy, builder);
        };

        switch (Kind) {
            case BuiltinKind::AsyncSchedulerCreate: {
                llvm::FunctionCallee fn = module->getOrInsertFunction(
                    "yuan_async_scheduler_create",
                    llvm::FunctionType::get(i8PtrTy, {}, false)
                );
                llvm::Value* raw = builder.CreateCall(fn, {}, "async.scheduler.create");
                return castRuntimePtrToHandle(raw, getIntResultTy(), usizeTy, builder);
            }
            case BuiltinKind::AsyncSchedulerDestroy: {
                llvm::Value* scheduler = genRuntimeHandlePtrArg(0);
                if (!scheduler) {
                    return nullptr;
                }
                llvm::FunctionCallee fn = module->getOrInsertFunction(
                    "yuan_async_scheduler_destroy",
                    llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8PtrTy}, false)
                );
                builder.CreateCall(fn, {scheduler});
                return nullptr;
            }
            case BuiltinKind::AsyncSchedulerSetCurrent: {
                llvm::Value* scheduler = genRuntimeHandlePtrArg(0);
                if (!scheduler) {
                    return nullptr;
                }
                llvm::FunctionCallee fn = module->getOrInsertFunction(
                    "yuan_async_scheduler_set_current",
                    llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8PtrTy}, false)
                );
                builder.CreateCall(fn, {scheduler});
                return nullptr;
            }
            case BuiltinKind::AsyncSchedulerCurrent: {
                llvm::FunctionCallee fn = module->getOrInsertFunction(
                    "yuan_async_scheduler_current",
                    llvm::FunctionType::get(i8PtrTy, {}, false)
                );
                llvm::Value* raw = builder.CreateCall(fn, {}, "async.scheduler.current");
                return castRuntimePtrToHandle(raw, getIntResultTy(), usizeTy, builder);
            }
            case BuiltinKind::AsyncSchedulerRunOne: {
                llvm::Value* scheduler = genRuntimeHandlePtrArg(0);
                if (!scheduler) {
                    return nullptr;
                }
                llvm::FunctionCallee fn = module->getOrInsertFunction(
                    "yuan_async_scheduler_run_one",
                    llvm::FunctionType::get(i32Ty, {i8PtrTy}, false)
                );
                llvm::Value* ret = builder.CreateCall(fn, {scheduler}, "async.scheduler.run_one");
                return castIntegerValue(ret, getIntResultTy(), builder, "async.scheduler.run_one.cast");
            }
            case BuiltinKind::AsyncSchedulerRunUntilIdle: {
                llvm::Value* scheduler = genRuntimeHandlePtrArg(0);
                if (!scheduler) {
                    return nullptr;
                }
                llvm::FunctionCallee fn = module->getOrInsertFunction(
                    "yuan_async_scheduler_run_until_idle",
                    llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8PtrTy}, false)
                );
                builder.CreateCall(fn, {scheduler});
                return nullptr;
            }
            case BuiltinKind::AsyncPromiseCreate: {
                llvm::FunctionCallee fn = module->getOrInsertFunction(
                    "yuan_promise_create",
                    llvm::FunctionType::get(i8PtrTy, {}, false)
                );
                llvm::Value* raw = builder.CreateCall(fn, {}, "async.promise.create");
                return castRuntimePtrToHandle(raw, getIntResultTy(), usizeTy, builder);
            }
            case BuiltinKind::AsyncPromiseRetain: {
                llvm::Value* promise = genRuntimeHandlePtrArg(0);
                if (!promise) {
                    return nullptr;
                }
                llvm::FunctionCallee fn = module->getOrInsertFunction(
                    "yuan_promise_retain",
                    llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8PtrTy}, false)
                );
                builder.CreateCall(fn, {promise});
                return nullptr;
            }
            case BuiltinKind::AsyncPromiseRelease: {
                llvm::Value* promise = genRuntimeHandlePtrArg(0);
                if (!promise) {
                    return nullptr;
                }
                llvm::FunctionCallee fn = module->getOrInsertFunction(
                    "yuan_promise_release",
                    llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8PtrTy}, false)
                );
                builder.CreateCall(fn, {promise});
                return nullptr;
            }
            case BuiltinKind::AsyncPromiseStatus: {
                llvm::Value* promise = genRuntimeHandlePtrArg(0);
                if (!promise) {
                    return nullptr;
                }
                llvm::FunctionCallee fn = module->getOrInsertFunction(
                    "yuan_promise_status",
                    llvm::FunctionType::get(i32Ty, {i8PtrTy}, false)
                );
                llvm::Value* ret = builder.CreateCall(fn, {promise}, "async.promise.status");
                return castIntegerValue(ret, getIntResultTy(), builder, "async.promise.status.cast");
            }
            case BuiltinKind::AsyncPromiseValue: {
                llvm::Value* promise = genRuntimeHandlePtrArg(0);
                if (!promise) {
                    return nullptr;
                }
                llvm::FunctionCallee fn = module->getOrInsertFunction(
                    "yuan_promise_value",
                    llvm::FunctionType::get(usizeTy, {i8PtrTy}, false)
                );
                llvm::Value* ret = builder.CreateCall(fn, {promise}, "async.promise.value");
                return castIntegerValue(ret, getIntResultTy(), builder, "async.promise.value.cast");
            }
            case BuiltinKind::AsyncPromiseError: {
                llvm::Value* promise = genRuntimeHandlePtrArg(0);
                if (!promise) {
                    return nullptr;
                }
                llvm::FunctionCallee fn = module->getOrInsertFunction(
                    "yuan_promise_error",
                    llvm::FunctionType::get(usizeTy, {i8PtrTy}, false)
                );
                llvm::Value* ret = builder.CreateCall(fn, {promise}, "async.promise.error");
                return castIntegerValue(ret, getIntResultTy(), builder, "async.promise.error.cast");
            }
            case BuiltinKind::AsyncPromiseResolve:
            case BuiltinKind::AsyncPromiseReject: {
                llvm::Value* promise = genRuntimeHandlePtrArg(0);
                llvm::Value* payload = genExprArg(1);
                if (!promise || !payload) {
                    return nullptr;
                }
                payload = castIntegerValue(payload, usizeTy, builder, "async.promise.payload");

                const char* fnName =
                    (Kind == BuiltinKind::AsyncPromiseResolve) ? "yuan_promise_resolve" : "yuan_promise_reject";
                llvm::FunctionCallee fn = module->getOrInsertFunction(
                    fnName,
                    llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8PtrTy, usizeTy}, false)
                );
                builder.CreateCall(fn, {promise, payload});
                return nullptr;
            }
            case BuiltinKind::AsyncPromiseAwait: {
                llvm::Value* promise = genRuntimeHandlePtrArg(0);
                if (!promise) {
                    return nullptr;
                }

                llvm::Value* outValue = builder.CreateAlloca(usizeTy, nullptr, "async.await.value");
                llvm::Value* outError = builder.CreateAlloca(usizeTy, nullptr, "async.await.error");

                llvm::FunctionCallee fn = module->getOrInsertFunction(
                    "yuan_promise_await",
                    llvm::FunctionType::get(
                        i32Ty,
                        {i8PtrTy,
                         llvm::PointerType::get(usizeTy, 0),
                         llvm::PointerType::get(usizeTy, 0)},
                        false
                    )
                );
                llvm::Value* status = builder.CreateCall(
                    fn,
                    {promise, outValue, outError},
                    "async.promise.await"
                );
                return castIntegerValue(status, getIntResultTy(), builder, "async.promise.await.cast");
            }
            case BuiltinKind::AsyncStep: {
                llvm::FunctionCallee fn = module->getOrInsertFunction(
                    "yuan_async_step",
                    llvm::FunctionType::get(llvm::Type::getVoidTy(context), {}, false)
                );
                builder.CreateCall(fn, {});
                return nullptr;
            }
            case BuiltinKind::AsyncStepCount: {
                llvm::FunctionCallee fn = module->getOrInsertFunction(
                    "yuan_async_step_count",
                    llvm::FunctionType::get(i64Ty, {}, false)
                );
                llvm::Value* count = builder.CreateCall(fn, {}, "async.step.count");
                return castIntegerValue(count, getIntResultTy(), builder, "async.step.count.cast");
            }
            default:
                return nullptr;
        }
    }

private:
    BuiltinKind Kind;
};

} // namespace

std::unique_ptr<BuiltinHandler> createAsyncSchedulerCreateBuiltin() {
    return std::make_unique<AsyncBuiltin>(BuiltinKind::AsyncSchedulerCreate);
}

std::unique_ptr<BuiltinHandler> createAsyncSchedulerDestroyBuiltin() {
    return std::make_unique<AsyncBuiltin>(BuiltinKind::AsyncSchedulerDestroy);
}

std::unique_ptr<BuiltinHandler> createAsyncSchedulerSetCurrentBuiltin() {
    return std::make_unique<AsyncBuiltin>(BuiltinKind::AsyncSchedulerSetCurrent);
}

std::unique_ptr<BuiltinHandler> createAsyncSchedulerCurrentBuiltin() {
    return std::make_unique<AsyncBuiltin>(BuiltinKind::AsyncSchedulerCurrent);
}

std::unique_ptr<BuiltinHandler> createAsyncSchedulerRunOneBuiltin() {
    return std::make_unique<AsyncBuiltin>(BuiltinKind::AsyncSchedulerRunOne);
}

std::unique_ptr<BuiltinHandler> createAsyncSchedulerRunUntilIdleBuiltin() {
    return std::make_unique<AsyncBuiltin>(BuiltinKind::AsyncSchedulerRunUntilIdle);
}

std::unique_ptr<BuiltinHandler> createAsyncPromiseCreateBuiltin() {
    return std::make_unique<AsyncBuiltin>(BuiltinKind::AsyncPromiseCreate);
}

std::unique_ptr<BuiltinHandler> createAsyncPromiseRetainBuiltin() {
    return std::make_unique<AsyncBuiltin>(BuiltinKind::AsyncPromiseRetain);
}

std::unique_ptr<BuiltinHandler> createAsyncPromiseReleaseBuiltin() {
    return std::make_unique<AsyncBuiltin>(BuiltinKind::AsyncPromiseRelease);
}

std::unique_ptr<BuiltinHandler> createAsyncPromiseStatusBuiltin() {
    return std::make_unique<AsyncBuiltin>(BuiltinKind::AsyncPromiseStatus);
}

std::unique_ptr<BuiltinHandler> createAsyncPromiseValueBuiltin() {
    return std::make_unique<AsyncBuiltin>(BuiltinKind::AsyncPromiseValue);
}

std::unique_ptr<BuiltinHandler> createAsyncPromiseErrorBuiltin() {
    return std::make_unique<AsyncBuiltin>(BuiltinKind::AsyncPromiseError);
}

std::unique_ptr<BuiltinHandler> createAsyncPromiseResolveBuiltin() {
    return std::make_unique<AsyncBuiltin>(BuiltinKind::AsyncPromiseResolve);
}

std::unique_ptr<BuiltinHandler> createAsyncPromiseRejectBuiltin() {
    return std::make_unique<AsyncBuiltin>(BuiltinKind::AsyncPromiseReject);
}

std::unique_ptr<BuiltinHandler> createAsyncPromiseAwaitBuiltin() {
    return std::make_unique<AsyncBuiltin>(BuiltinKind::AsyncPromiseAwait);
}

std::unique_ptr<BuiltinHandler> createAsyncStepBuiltin() {
    return std::make_unique<AsyncBuiltin>(BuiltinKind::AsyncStep);
}

std::unique_ptr<BuiltinHandler> createAsyncStepCountBuiltin() {
    return std::make_unique<AsyncBuiltin>(BuiltinKind::AsyncStepCount);
}

} // namespace yuan
