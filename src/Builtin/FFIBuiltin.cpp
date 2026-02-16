/// \file FFIBuiltin.cpp
/// \brief C FFI 相关内置函数实现。

#include "yuan/Builtin/BuiltinHandler.h"
#include "yuan/Sema/Sema.h"
#include "yuan/Sema/Type.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/CodeGen/CodeGen.h"
#include "yuan/Basic/Diagnostic.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>

namespace yuan {

namespace {

static Type* unwrapAliases(Type* type) {
    Type* current = type;
    while (current && current->isTypeAlias()) {
        current = static_cast<TypeAlias*>(current)->getAliasedType();
    }
    return current;
}

static bool isStringType(Type* type) {
    type = unwrapAliases(type);
    return type && type->isString();
}

static bool isIntegerType(Type* type) {
    type = unwrapAliases(type);
    return type && type->isInteger();
}

static llvm::Value* castToUSize(llvm::Value* value,
                                llvm::Type* usizeTy,
                                llvm::IRBuilder<>& builder,
                                const char* name) {
    if (!value || !usizeTy || value->getType() == usizeTy) {
        return value;
    }

    if (value->getType()->isIntegerTy()) {
        return builder.CreateZExtOrTrunc(value, usizeTy, name);
    }
    if (value->getType()->isPointerTy()) {
        return builder.CreatePtrToInt(value, usizeTy, name);
    }
    return builder.CreateBitCast(value, usizeTy, name);
}

static llvm::Value* castFromUSize(llvm::Value* value,
                                  llvm::Type* targetTy,
                                  llvm::IRBuilder<>& builder,
                                  const char* name) {
    if (!value || !targetTy || value->getType() == targetTy) {
        return value;
    }

    if (value->getType()->isIntegerTy() && targetTy->isIntegerTy()) {
        return builder.CreateZExtOrTrunc(value, targetTy, name);
    }
    if (value->getType()->isIntegerTy() && targetTy->isPointerTy()) {
        return builder.CreateIntToPtr(value, targetTy, name);
    }
    if (value->getType()->isPointerTy() && targetTy->isIntegerTy()) {
        return builder.CreatePtrToInt(value, targetTy, name);
    }
    return builder.CreateBitCast(value, targetTy, name);
}

class FFIBuiltin : public BuiltinHandler {
public:
    explicit FFIBuiltin(BuiltinKind kind) : Kind(kind) {}

    const char* getName() const override {
        switch (Kind) {
            case BuiltinKind::FfiOpen: return "ffi_open";
            case BuiltinKind::FfiOpenSelf: return "ffi_open_self";
            case BuiltinKind::FfiSym: return "ffi_sym";
            case BuiltinKind::FfiClose: return "ffi_close";
            case BuiltinKind::FfiLastError: return "ffi_last_error";
            case BuiltinKind::FfiCStrLen: return "ffi_cstr_len";
            case BuiltinKind::FfiCall0: return "ffi_call0";
            case BuiltinKind::FfiCall1: return "ffi_call1";
            case BuiltinKind::FfiCall2: return "ffi_call2";
            case BuiltinKind::FfiCall3: return "ffi_call3";
            case BuiltinKind::FfiCall4: return "ffi_call4";
            case BuiltinKind::FfiCall5: return "ffi_call5";
            case BuiltinKind::FfiCall6: return "ffi_call6";
            default: return "ffi";
        }
    }

    BuiltinKind getKind() const override { return Kind; }

    int getExpectedArgCount() const override {
        switch (Kind) {
            case BuiltinKind::FfiOpen: return 1;
            case BuiltinKind::FfiOpenSelf: return 0;
            case BuiltinKind::FfiSym: return 2;
            case BuiltinKind::FfiClose: return 1;
            case BuiltinKind::FfiLastError: return 0;
            case BuiltinKind::FfiCStrLen: return 1;
            case BuiltinKind::FfiCall0: return 1;
            case BuiltinKind::FfiCall1: return 2;
            case BuiltinKind::FfiCall2: return 3;
            case BuiltinKind::FfiCall3: return 4;
            case BuiltinKind::FfiCall4: return 5;
            case BuiltinKind::FfiCall5: return 6;
            case BuiltinKind::FfiCall6: return 7;
            default: return -1;
        }
    }

    std::string getArgDescription() const override {
        switch (Kind) {
            case BuiltinKind::FfiOpen: return "library_path";
            case BuiltinKind::FfiOpenSelf: return "";
            case BuiltinKind::FfiSym: return "handle, symbol_name";
            case BuiltinKind::FfiClose: return "handle";
            case BuiltinKind::FfiLastError: return "";
            case BuiltinKind::FfiCStrLen: return "cstr_ptr";
            case BuiltinKind::FfiCall0: return "func_ptr";
            case BuiltinKind::FfiCall1: return "func_ptr, arg0";
            case BuiltinKind::FfiCall2: return "func_ptr, arg0, arg1";
            case BuiltinKind::FfiCall3: return "func_ptr, arg0, arg1, arg2";
            case BuiltinKind::FfiCall4: return "func_ptr, arg0, arg1, arg2, arg3";
            case BuiltinKind::FfiCall5: return "func_ptr, arg0, arg1, arg2, arg3, arg4";
            case BuiltinKind::FfiCall6: return "func_ptr, arg0, arg1, arg2, arg3, arg4, arg5";
            default: return "";
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

        auto requireStringArg = [&](unsigned index) -> bool {
            const auto& arg = expr->getArgs()[index];
            if (!arg.isExpr()) {
                return false;
            }
            Type* argType = sema.analyzeExpr(arg.getExpr());
            if (!isStringType(argType)) {
                sema.getDiagnostics()
                    .report(DiagID::err_type_mismatch, arg.getExpr()->getBeginLoc(), arg.getExpr()->getRange())
                    << "str"
                    << (argType ? argType->toString() : "unknown");
                return false;
            }
            return true;
        };

        auto requireIntegerArg = [&](unsigned index) -> bool {
            const auto& arg = expr->getArgs()[index];
            if (!arg.isExpr()) {
                return false;
            }
            Type* argType = sema.analyzeExpr(arg.getExpr());
            if (!isIntegerType(argType)) {
                sema.getDiagnostics()
                    .report(DiagID::err_type_mismatch, arg.getExpr()->getBeginLoc(), arg.getExpr()->getRange())
                    << "integer"
                    << (argType ? argType->toString() : "unknown");
                return false;
            }
            return true;
        };

        switch (Kind) {
            case BuiltinKind::FfiOpen:
                if (!requireStringArg(0)) return nullptr;
                break;
            case BuiltinKind::FfiSym:
                if (!requireIntegerArg(0) || !requireStringArg(1)) return nullptr;
                break;
            case BuiltinKind::FfiClose:
            case BuiltinKind::FfiCStrLen:
            case BuiltinKind::FfiCall0:
                if (!requireIntegerArg(0)) return nullptr;
                break;
            case BuiltinKind::FfiCall1:
            case BuiltinKind::FfiCall2:
            case BuiltinKind::FfiCall3:
            case BuiltinKind::FfiCall4:
            case BuiltinKind::FfiCall5:
            case BuiltinKind::FfiCall6: {
                for (unsigned i = 0; i < expr->getArgCount(); ++i) {
                    if (!requireIntegerArg(i)) return nullptr;
                }
                break;
            }
            default:
                break;
        }

        ASTContext& ctx = sema.getContext();
        switch (Kind) {
            case BuiltinKind::FfiClose:
                return ctx.getBoolType();
            case BuiltinKind::FfiLastError:
                return ctx.getStrType();
            case BuiltinKind::FfiOpen:
            case BuiltinKind::FfiOpenSelf:
            case BuiltinKind::FfiSym:
            case BuiltinKind::FfiCStrLen:
            case BuiltinKind::FfiCall0:
            case BuiltinKind::FfiCall1:
            case BuiltinKind::FfiCall2:
            case BuiltinKind::FfiCall3:
            case BuiltinKind::FfiCall4:
            case BuiltinKind::FfiCall5:
            case BuiltinKind::FfiCall6:
                return ctx.getIntegerType(ctx.getPointerBitWidth(), false);
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
        llvm::Type* i32Ty = llvm::Type::getInt32Ty(context);
        llvm::Type* usizeTy = module->getDataLayout().getIntPtrType(context);
#if defined(_WIN32)
        bool useWindowsSRet = true;
#else
        bool useWindowsSRet = false;
#endif

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

        auto getOrInsert = [&](const char* fnName,
                               llvm::Type* retTy,
                               const std::vector<llvm::Type*>& paramTys) -> llvm::FunctionCallee {
            llvm::FunctionType* fnTy = llvm::FunctionType::get(retTy, paramTys, false);
            return module->getOrInsertFunction(fnName, fnTy);
        };

        auto castResultToExprType = [&](llvm::Value* raw, const char* name) -> llvm::Value* {
            llvm::Type* targetTy = codegen.getLLVMType(expr->getType());
            return castFromUSize(raw, targetTy ? targetTy : usizeTy, builder, name);
        };

        switch (Kind) {
            case BuiltinKind::FfiOpen: {
                llvm::Value* path = genExprArg(0);
                if (!path) return nullptr;
                llvm::Value* pathData = builder.CreateExtractValue(path, {0}, "ffi.path.data");
                llvm::Value* pathLen = builder.CreateExtractValue(path, {1}, "ffi.path.len");
                llvm::FunctionCallee fn = getOrInsert("yuan_ffi_open", usizeTy, {pathData->getType(), pathLen->getType()});
                llvm::Value* raw = builder.CreateCall(fn, {pathData, pathLen}, "ffi.open");
                return castResultToExprType(raw, "ffi.open.cast");
            }
            case BuiltinKind::FfiOpenSelf: {
                llvm::FunctionCallee fn = getOrInsert("yuan_ffi_open_self", usizeTy, {});
                llvm::Value* raw = builder.CreateCall(fn, {}, "ffi.open.self");
                return castResultToExprType(raw, "ffi.open.self.cast");
            }
            case BuiltinKind::FfiSym: {
                llvm::Value* handle = genExprArg(0);
                llvm::Value* symbol = genExprArg(1);
                if (!handle || !symbol) return nullptr;
                handle = castToUSize(handle, usizeTy, builder, "ffi.handle");
                llvm::Value* symbolData = builder.CreateExtractValue(symbol, {0}, "ffi.symbol.data");
                llvm::Value* symbolLen = builder.CreateExtractValue(symbol, {1}, "ffi.symbol.len");
                llvm::FunctionCallee fn =
                    getOrInsert("yuan_ffi_symbol", usizeTy, {usizeTy, symbolData->getType(), symbolLen->getType()});
                llvm::Value* raw = builder.CreateCall(fn, {handle, symbolData, symbolLen}, "ffi.sym");
                return castResultToExprType(raw, "ffi.sym.cast");
            }
            case BuiltinKind::FfiClose: {
                llvm::Value* handle = genExprArg(0);
                if (!handle) return nullptr;
                handle = castToUSize(handle, usizeTy, builder, "ffi.handle");
                llvm::FunctionCallee fn = getOrInsert("yuan_ffi_close", i32Ty, {usizeTy});
                llvm::Value* raw = builder.CreateCall(fn, {handle}, "ffi.close");
                return builder.CreateICmpNE(raw, llvm::ConstantInt::get(i32Ty, 0), "ffi.close.ok");
            }
            case BuiltinKind::FfiLastError: {
                llvm::Type* retTy = codegen.getLLVMType(expr->getType());
                if (!retTy) return nullptr;
                if (useWindowsSRet) {
                    llvm::Type* retPtrTy = llvm::PointerType::get(retTy, 0);
                    llvm::FunctionType* fnTy = llvm::FunctionType::get(
                        llvm::Type::getVoidTy(context),
                        {retPtrTy},
                        false
                    );
                    llvm::FunctionCallee callee = module->getOrInsertFunction("yuan_ffi_last_error", fnTy);
                    if (auto* fn = llvm::dyn_cast<llvm::Function>(callee.getCallee())) {
                        fn->addParamAttr(0, llvm::Attribute::StructRet);
                    }
                    llvm::AllocaInst* out = builder.CreateAlloca(retTy, nullptr, "ffi.last_error.out");
                    builder.CreateCall(callee, {out});
                    return builder.CreateLoad(retTy, out, "ffi.last_error");
                }
                llvm::FunctionCallee fn = getOrInsert("yuan_ffi_last_error", retTy, {});
                return builder.CreateCall(fn, {}, "ffi.last_error");
            }
            case BuiltinKind::FfiCStrLen: {
                llvm::Value* ptr = genExprArg(0);
                if (!ptr) return nullptr;
                ptr = castToUSize(ptr, usizeTy, builder, "ffi.cstr.ptr");
                llvm::FunctionCallee fn = getOrInsert("yuan_ffi_cstr_len", usizeTy, {usizeTy});
                llvm::Value* raw = builder.CreateCall(fn, {ptr}, "ffi.cstr_len");
                return castResultToExprType(raw, "ffi.cstr_len.cast");
            }
            case BuiltinKind::FfiCall0:
            case BuiltinKind::FfiCall1:
            case BuiltinKind::FfiCall2:
            case BuiltinKind::FfiCall3:
            case BuiltinKind::FfiCall4:
            case BuiltinKind::FfiCall5:
            case BuiltinKind::FfiCall6: {
                unsigned argc = static_cast<unsigned>(expr->getArgCount());
                std::vector<llvm::Value*> callArgs;
                std::vector<llvm::Type*> paramTys;
                callArgs.reserve(argc);
                paramTys.reserve(argc);

                for (unsigned i = 0; i < argc; ++i) {
                    llvm::Value* arg = genExprArg(i);
                    if (!arg) return nullptr;
                    arg = castToUSize(arg, usizeTy, builder, "ffi.call.arg");
                    callArgs.push_back(arg);
                    paramTys.push_back(usizeTy);
                }

                const char* fnName = nullptr;
                switch (Kind) {
                    case BuiltinKind::FfiCall0: fnName = "yuan_ffi_call0"; break;
                    case BuiltinKind::FfiCall1: fnName = "yuan_ffi_call1"; break;
                    case BuiltinKind::FfiCall2: fnName = "yuan_ffi_call2"; break;
                    case BuiltinKind::FfiCall3: fnName = "yuan_ffi_call3"; break;
                    case BuiltinKind::FfiCall4: fnName = "yuan_ffi_call4"; break;
                    case BuiltinKind::FfiCall5: fnName = "yuan_ffi_call5"; break;
                    case BuiltinKind::FfiCall6: fnName = "yuan_ffi_call6"; break;
                    default: break;
                }

                llvm::FunctionCallee fn = getOrInsert(fnName, usizeTy, paramTys);
                llvm::Value* raw = builder.CreateCall(fn, callArgs, "ffi.call");
                return castResultToExprType(raw, "ffi.call.cast");
            }
            default:
                return nullptr;
        }
    }

private:
    BuiltinKind Kind;
};

} // namespace

std::unique_ptr<BuiltinHandler> createFfiOpenBuiltin() {
    return std::make_unique<FFIBuiltin>(BuiltinKind::FfiOpen);
}

std::unique_ptr<BuiltinHandler> createFfiOpenSelfBuiltin() {
    return std::make_unique<FFIBuiltin>(BuiltinKind::FfiOpenSelf);
}

std::unique_ptr<BuiltinHandler> createFfiSymBuiltin() {
    return std::make_unique<FFIBuiltin>(BuiltinKind::FfiSym);
}

std::unique_ptr<BuiltinHandler> createFfiCloseBuiltin() {
    return std::make_unique<FFIBuiltin>(BuiltinKind::FfiClose);
}

std::unique_ptr<BuiltinHandler> createFfiLastErrorBuiltin() {
    return std::make_unique<FFIBuiltin>(BuiltinKind::FfiLastError);
}

std::unique_ptr<BuiltinHandler> createFfiCStrLenBuiltin() {
    return std::make_unique<FFIBuiltin>(BuiltinKind::FfiCStrLen);
}

std::unique_ptr<BuiltinHandler> createFfiCall0Builtin() {
    return std::make_unique<FFIBuiltin>(BuiltinKind::FfiCall0);
}

std::unique_ptr<BuiltinHandler> createFfiCall1Builtin() {
    return std::make_unique<FFIBuiltin>(BuiltinKind::FfiCall1);
}

std::unique_ptr<BuiltinHandler> createFfiCall2Builtin() {
    return std::make_unique<FFIBuiltin>(BuiltinKind::FfiCall2);
}

std::unique_ptr<BuiltinHandler> createFfiCall3Builtin() {
    return std::make_unique<FFIBuiltin>(BuiltinKind::FfiCall3);
}

std::unique_ptr<BuiltinHandler> createFfiCall4Builtin() {
    return std::make_unique<FFIBuiltin>(BuiltinKind::FfiCall4);
}

std::unique_ptr<BuiltinHandler> createFfiCall5Builtin() {
    return std::make_unique<FFIBuiltin>(BuiltinKind::FfiCall5);
}

std::unique_ptr<BuiltinHandler> createFfiCall6Builtin() {
    return std::make_unique<FFIBuiltin>(BuiltinKind::FfiCall6);
}

} // namespace yuan
