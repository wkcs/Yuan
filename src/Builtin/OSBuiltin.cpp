/// \file OSBuiltin.cpp
/// \brief OS 运行时内置函数实现。

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
#include <utility>

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

static bool isBoolType(Type* type) {
    type = unwrapAliases(type);
    return type && type->isBool();
}

static llvm::Value* castIntegerValue(llvm::Value* value,
                                     llvm::Type* targetType,
                                     llvm::IRBuilder<>& builder,
                                     const char* name) {
    if (!value || !targetType || value->getType() == targetType) {
        return value;
    }

    if (value->getType()->isIntegerTy() && targetType->isIntegerTy()) {
        return builder.CreateSExtOrTrunc(value, targetType, name);
    }

    if (value->getType()->isPointerTy() && targetType->isIntegerTy()) {
        return builder.CreatePtrToInt(value, targetType, name);
    }

    if (value->getType()->isIntegerTy() && targetType->isPointerTy()) {
        return builder.CreateIntToPtr(value, targetType, name);
    }

    return builder.CreateBitCast(value, targetType, name);
}

class OSBuiltin : public BuiltinHandler {
public:
    explicit OSBuiltin(BuiltinKind kind) : Kind(kind) {}

    const char* getName() const override {
        switch (Kind) {
            case BuiltinKind::OsTimeUnixNanos: return "os_time_unix_nanos";
            case BuiltinKind::OsSleepNanos: return "os_sleep_nanos";
            case BuiltinKind::OsYield: return "os_yield";
            case BuiltinKind::OsThreadSpawn: return "os_thread_spawn";
            case BuiltinKind::OsThreadIsFinished: return "os_thread_is_finished";
            case BuiltinKind::OsThreadJoin: return "os_thread_join";
            case BuiltinKind::OsReadFile: return "os_read_file";
            case BuiltinKind::OsWriteFile: return "os_write_file";
            case BuiltinKind::OsExists: return "os_exists";
            case BuiltinKind::OsIsFile: return "os_is_file";
            case BuiltinKind::OsIsDir: return "os_is_dir";
            case BuiltinKind::OsCreateDir: return "os_create_dir";
            case BuiltinKind::OsCreateDirAll: return "os_create_dir_all";
            case BuiltinKind::OsRemoveDir: return "os_remove_dir";
            case BuiltinKind::OsRemoveFile: return "os_remove_file";
            case BuiltinKind::OsReadDirOpen: return "os_read_dir_open";
            case BuiltinKind::OsReadDirNext: return "os_read_dir_next";
            case BuiltinKind::OsReadDirEntryPath: return "os_read_dir_entry_path";
            case BuiltinKind::OsReadDirEntryName: return "os_read_dir_entry_name";
            case BuiltinKind::OsReadDirEntryIsFile: return "os_read_dir_entry_is_file";
            case BuiltinKind::OsReadDirEntryIsDir: return "os_read_dir_entry_is_dir";
            case BuiltinKind::OsReadDirClose: return "os_read_dir_close";
            case BuiltinKind::OsStdinReadLine: return "os_stdin_read_line";
            case BuiltinKind::OsHttpGetStatus: return "os_http_get_status";
            case BuiltinKind::OsHttpGetBody: return "os_http_get_body";
            case BuiltinKind::OsHttpPostStatus: return "os_http_post_status";
            case BuiltinKind::OsHttpPostBody: return "os_http_post_body";
            default: return "os";
        }
    }

    BuiltinKind getKind() const override { return Kind; }

    int getExpectedArgCount() const override {
        switch (Kind) {
            case BuiltinKind::OsTimeUnixNanos:
            case BuiltinKind::OsYield:
            case BuiltinKind::OsStdinReadLine:
                return 0;
            case BuiltinKind::OsThreadIsFinished:
            case BuiltinKind::OsThreadJoin:
                return 1;
            case BuiltinKind::OsThreadSpawn:
                return 2;
            case BuiltinKind::OsSleepNanos:
            case BuiltinKind::OsReadFile:
            case BuiltinKind::OsExists:
            case BuiltinKind::OsIsFile:
            case BuiltinKind::OsIsDir:
            case BuiltinKind::OsCreateDir:
            case BuiltinKind::OsCreateDirAll:
            case BuiltinKind::OsRemoveDir:
            case BuiltinKind::OsRemoveFile:
            case BuiltinKind::OsReadDirOpen:
            case BuiltinKind::OsReadDirNext:
            case BuiltinKind::OsReadDirEntryPath:
            case BuiltinKind::OsReadDirEntryName:
            case BuiltinKind::OsReadDirEntryIsFile:
            case BuiltinKind::OsReadDirEntryIsDir:
            case BuiltinKind::OsReadDirClose:
                return 1;
            case BuiltinKind::OsWriteFile:
                return 2;
            case BuiltinKind::OsHttpGetStatus:
            case BuiltinKind::OsHttpGetBody:
            case BuiltinKind::OsHttpPostStatus:
            case BuiltinKind::OsHttpPostBody:
                return -1;
            default:
                return -1;
        }
    }

    std::string getArgDescription() const override {
        switch (Kind) {
            case BuiltinKind::OsTimeUnixNanos:
            case BuiltinKind::OsYield:
            case BuiltinKind::OsStdinReadLine:
                return "";
            case BuiltinKind::OsThreadSpawn:
                return "entry, context";
            case BuiltinKind::OsThreadIsFinished:
            case BuiltinKind::OsThreadJoin:
                return "handle";
            case BuiltinKind::OsSleepNanos:
                return "nanos";
            case BuiltinKind::OsReadFile:
            case BuiltinKind::OsExists:
            case BuiltinKind::OsIsFile:
            case BuiltinKind::OsIsDir:
            case BuiltinKind::OsCreateDir:
            case BuiltinKind::OsCreateDirAll:
            case BuiltinKind::OsRemoveDir:
            case BuiltinKind::OsRemoveFile:
            case BuiltinKind::OsReadDirOpen:
                return "path";
            case BuiltinKind::OsReadDirNext:
            case BuiltinKind::OsReadDirEntryPath:
            case BuiltinKind::OsReadDirEntryName:
            case BuiltinKind::OsReadDirEntryIsFile:
            case BuiltinKind::OsReadDirEntryIsDir:
            case BuiltinKind::OsReadDirClose:
                return "handle";
            case BuiltinKind::OsWriteFile:
                return "path, content";
            case BuiltinKind::OsHttpGetStatus:
            case BuiltinKind::OsHttpGetBody:
                return "url[, timeout_ms[, headers]]";
            case BuiltinKind::OsHttpPostStatus:
            case BuiltinKind::OsHttpPostBody:
                return "url, body[, timeout_ms[, headers[, stream]]]";
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

        if ((Kind == BuiltinKind::OsHttpGetStatus || Kind == BuiltinKind::OsHttpGetBody) &&
            !(expr->getArgCount() == 1 || expr->getArgCount() == 2 || expr->getArgCount() == 3)) {
            sema.getDiagnostics()
                .report(DiagID::err_wrong_builtin_argument_count, expr->getBeginLoc(), expr->getRange())
                << static_cast<unsigned>(3)
                << static_cast<unsigned>(expr->getArgCount());
            return nullptr;
        }
        if ((Kind == BuiltinKind::OsHttpPostStatus || Kind == BuiltinKind::OsHttpPostBody) &&
            !(expr->getArgCount() == 2 || expr->getArgCount() == 3 ||
              expr->getArgCount() == 4 || expr->getArgCount() == 5)) {
            sema.getDiagnostics()
                .report(DiagID::err_wrong_builtin_argument_count, expr->getBeginLoc(), expr->getRange())
                << static_cast<unsigned>(5)
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

        auto requireBoolArg = [&](unsigned index) -> bool {
            const auto& arg = expr->getArgs()[index];
            if (!arg.isExpr()) {
                return false;
            }
            Type* argType = sema.analyzeExpr(arg.getExpr());
            if (!isBoolType(argType)) {
                sema.getDiagnostics()
                    .report(DiagID::err_type_mismatch, arg.getExpr()->getBeginLoc(), arg.getExpr()->getRange())
                    << "bool"
                    << (argType ? argType->toString() : "unknown");
                return false;
            }
            return true;
        };

        auto requireThreadEntryArg = [&](unsigned index) -> bool {
            const auto& arg = expr->getArgs()[index];
            if (!arg.isExpr()) {
                return false;
            }
            Type* argType = unwrapAliases(sema.analyzeExpr(arg.getExpr()));
            if (!argType || !argType->isFunction()) {
                sema.getDiagnostics()
                    .report(DiagID::err_type_mismatch, arg.getExpr()->getBeginLoc(), arg.getExpr()->getRange())
                    << "func(usize) -> void"
                    << (argType ? argType->toString() : "unknown");
                return false;
            }

            auto* fnType = static_cast<FunctionType*>(argType);
            if (fnType->canError() || fnType->isVariadic() || fnType->getParamCount() != 1) {
                sema.getDiagnostics()
                    .report(DiagID::err_type_mismatch, arg.getExpr()->getBeginLoc(), arg.getExpr()->getRange())
                    << "func(usize) -> void"
                    << fnType->toString();
                return false;
            }

            Type* paramType = unwrapAliases(fnType->getParam(0));
            Type* retType = unwrapAliases(fnType->getReturnType());
            if (!paramType || !paramType->isInteger() || !retType || !retType->isVoid()) {
                sema.getDiagnostics()
                    .report(DiagID::err_type_mismatch, arg.getExpr()->getBeginLoc(), arg.getExpr()->getRange())
                    << "func(usize) -> void"
                    << fnType->toString();
                return false;
            }

            auto* paramIntType = static_cast<IntegerType*>(paramType);
            if (paramIntType->getBitWidth() != sema.getContext().getPointerBitWidth()) {
                sema.getDiagnostics()
                    .report(DiagID::err_type_mismatch, arg.getExpr()->getBeginLoc(), arg.getExpr()->getRange())
                    << "func(usize) -> void"
                    << fnType->toString();
                return false;
            }

            return true;
        };

        switch (Kind) {
            case BuiltinKind::OsSleepNanos:
                if (!requireIntegerArg(0)) return nullptr;
                break;
            case BuiltinKind::OsThreadSpawn:
                if (!requireThreadEntryArg(0) || !requireIntegerArg(1)) return nullptr;
                break;
            case BuiltinKind::OsThreadIsFinished:
            case BuiltinKind::OsThreadJoin:
                if (!requireIntegerArg(0)) return nullptr;
                break;
            case BuiltinKind::OsReadFile:
            case BuiltinKind::OsExists:
            case BuiltinKind::OsIsFile:
            case BuiltinKind::OsIsDir:
            case BuiltinKind::OsCreateDir:
            case BuiltinKind::OsCreateDirAll:
            case BuiltinKind::OsRemoveDir:
            case BuiltinKind::OsRemoveFile:
            case BuiltinKind::OsReadDirOpen:
                if (!requireStringArg(0)) return nullptr;
                break;
            case BuiltinKind::OsWriteFile:
                if (!requireStringArg(0) || !requireStringArg(1)) return nullptr;
                break;
            case BuiltinKind::OsReadDirNext:
            case BuiltinKind::OsReadDirEntryPath:
            case BuiltinKind::OsReadDirEntryName:
            case BuiltinKind::OsReadDirEntryIsFile:
            case BuiltinKind::OsReadDirEntryIsDir:
            case BuiltinKind::OsReadDirClose:
                if (!requireIntegerArg(0)) return nullptr;
                break;
            case BuiltinKind::OsHttpGetStatus:
            case BuiltinKind::OsHttpGetBody:
                if (!requireStringArg(0)) return nullptr;
                if (expr->getArgCount() > 1 && !requireIntegerArg(1)) return nullptr;
                if (expr->getArgCount() > 2 && !requireStringArg(2)) return nullptr;
                break;
            case BuiltinKind::OsHttpPostStatus:
            case BuiltinKind::OsHttpPostBody:
                if (!requireStringArg(0) || !requireStringArg(1)) return nullptr;
                if (expr->getArgCount() > 2 && !requireIntegerArg(2)) return nullptr;
                if (expr->getArgCount() > 3 && !requireStringArg(3)) return nullptr;
                if (expr->getArgCount() > 4 && !requireBoolArg(4)) return nullptr;
                break;
            default:
                break;
        }

        ASTContext& ctx = sema.getContext();
        switch (Kind) {
            case BuiltinKind::OsTimeUnixNanos:
                return ctx.getI64Type();
            case BuiltinKind::OsThreadSpawn:
                return ctx.getIntegerType(ctx.getPointerBitWidth(), false);
            case BuiltinKind::OsReadFile:
            case BuiltinKind::OsReadDirEntryPath:
            case BuiltinKind::OsReadDirEntryName:
            case BuiltinKind::OsStdinReadLine:
            case BuiltinKind::OsHttpGetBody:
            case BuiltinKind::OsHttpPostBody:
                return ctx.getStrType();
            case BuiltinKind::OsExists:
            case BuiltinKind::OsIsFile:
            case BuiltinKind::OsIsDir:
            case BuiltinKind::OsWriteFile:
            case BuiltinKind::OsCreateDir:
            case BuiltinKind::OsCreateDirAll:
            case BuiltinKind::OsRemoveDir:
            case BuiltinKind::OsRemoveFile:
            case BuiltinKind::OsReadDirNext:
            case BuiltinKind::OsReadDirEntryIsFile:
            case BuiltinKind::OsReadDirEntryIsDir:
            case BuiltinKind::OsThreadIsFinished:
                return ctx.getBoolType();
            case BuiltinKind::OsReadDirOpen:
                return ctx.getIntegerType(ctx.getPointerBitWidth(), false);
            case BuiltinKind::OsHttpGetStatus:
            case BuiltinKind::OsHttpPostStatus:
                return ctx.getI32Type();
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
        llvm::Type* voidTy = llvm::Type::getVoidTy(context);
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

        auto callBoolI32Runtime = [&](const char* fnName,
                                      const std::vector<llvm::Type*>& paramTys,
                                      const std::vector<llvm::Value*>& args,
                                      const char* label) -> llvm::Value* {
            llvm::FunctionCallee fn = getOrInsert(fnName, i32Ty, paramTys);
            llvm::Value* raw = builder.CreateCall(fn, args, label);
            return builder.CreateICmpNE(raw, llvm::ConstantInt::get(i32Ty, 0), "os.bool");
        };

        auto extractStringParts = [&](llvm::Value* strValue,
                                      const char* ptrName,
                                      const char* lenName) -> std::pair<llvm::Value*, llvm::Value*> {
            if (!strValue) {
                return {nullptr, nullptr};
            }
            if (strValue->getType()->isPointerTy()) {
                llvm::Type* strTy = codegen.getLLVMType(codegen.getASTContext().getStrType());
                if (strTy && strTy->isStructTy()) {
                    strValue = builder.CreateLoad(strTy, strValue, "os.str.load");
                }
            }
            if (!strValue->getType()->isStructTy()) {
                return {nullptr, nullptr};
            }
            llvm::Value* data = builder.CreateExtractValue(strValue, 0, ptrName);
            llvm::Value* len = builder.CreateExtractValue(strValue, 1, lenName);
            if (data->getType() != i8PtrTy) {
                data = castIntegerValue(data, i8PtrTy, builder, "os.str.ptr.cast");
            }
            if (!len->getType()->isIntegerTy(64)) {
                len = castIntegerValue(len, i64Ty, builder, "os.str.len.cast");
            }
            return {data, len};
        };

        auto callStringRuntime = [&](const char* fnName,
                                     const std::vector<llvm::Type*>& paramTys,
                                     const std::vector<llvm::Value*>& args,
                                     const char* label) -> llvm::Value* {
            llvm::Type* retTy = codegen.getLLVMType(expr->getType());
            if (!retTy) {
                return nullptr;
            }
            if (useWindowsSRet) {
                llvm::Type* retPtrTy = llvm::PointerType::get(retTy, 0);
                llvm::FunctionType* fnTy = llvm::FunctionType::get(
                    voidTy,
                    [&]() {
                        std::vector<llvm::Type*> tys;
                        tys.reserve(paramTys.size() + 1);
                        tys.push_back(retPtrTy);
                        tys.insert(tys.end(), paramTys.begin(), paramTys.end());
                        return tys;
                    }(),
                    false
                );
                llvm::FunctionCallee callee = module->getOrInsertFunction(fnName, fnTy);
                if (auto* fn = llvm::dyn_cast<llvm::Function>(callee.getCallee())) {
                    fn->addParamAttr(0, llvm::Attribute::StructRet);
                }
                llvm::AllocaInst* out = builder.CreateAlloca(retTy, nullptr, "os.str.out");
                std::vector<llvm::Value*> callArgs;
                callArgs.reserve(args.size() + 1);
                callArgs.push_back(out);
                callArgs.insert(callArgs.end(), args.begin(), args.end());
                builder.CreateCall(callee, callArgs);
                return builder.CreateLoad(retTy, out, label);
            }
            llvm::FunctionCallee callee = getOrInsert(fnName, retTy, paramTys);
            return builder.CreateCall(callee, args, label);
        };

        switch (Kind) {
            case BuiltinKind::OsTimeUnixNanos: {
                llvm::FunctionCallee fn = getOrInsert("yuan_os_time_unix_nanos", i64Ty, {});
                llvm::Value* raw = builder.CreateCall(fn, {}, "os.time.nanos");
                llvm::Type* targetTy = codegen.getLLVMType(expr->getType());
                return castIntegerValue(raw, targetTy ? targetTy : i64Ty, builder, "os.time.cast");
            }
            case BuiltinKind::OsSleepNanos: {
                llvm::Value* ns = genExprArg(0);
                if (!ns) return nullptr;
                ns = castIntegerValue(ns, i64Ty, builder, "os.sleep.nanos");
                llvm::FunctionCallee fn = getOrInsert("yuan_os_sleep_nanos", voidTy, {i64Ty});
                builder.CreateCall(fn, {ns});
                return nullptr;
            }
            case BuiltinKind::OsYield: {
                llvm::FunctionCallee fn = getOrInsert("yuan_os_yield", voidTy, {});
                builder.CreateCall(fn, {});
                return nullptr;
            }
            case BuiltinKind::OsThreadSpawn: {
                llvm::Value* entry = genExprArg(0);
                llvm::Value* ctx = genExprArg(1);
                if (!entry || !ctx) return nullptr;

                entry = castIntegerValue(entry, i8PtrTy, builder, "os.thread.entry");
                ctx = castIntegerValue(ctx, usizeTy, builder, "os.thread.ctx");

                llvm::FunctionCallee fn =
                    getOrInsert("yuan_os_thread_spawn", usizeTy, {i8PtrTy, usizeTy});
                llvm::Value* raw = builder.CreateCall(fn, {entry, ctx}, "os.thread.spawn");
                llvm::Type* targetTy = codegen.getLLVMType(expr->getType());
                return castIntegerValue(raw, targetTy ? targetTy : usizeTy, builder, "os.thread.handle.cast");
            }
            case BuiltinKind::OsThreadIsFinished: {
                llvm::Value* handle = genExprArg(0);
                if (!handle) return nullptr;
                handle = castIntegerValue(handle, usizeTy, builder, "os.thread.handle");
                return callBoolI32Runtime("yuan_os_thread_is_finished",
                                          {usizeTy},
                                          {handle},
                                          "os.thread.is_finished");
            }
            case BuiltinKind::OsThreadJoin: {
                llvm::Value* handle = genExprArg(0);
                if (!handle) return nullptr;
                handle = castIntegerValue(handle, usizeTy, builder, "os.thread.handle");
                llvm::FunctionCallee fn = getOrInsert("yuan_os_thread_join", voidTy, {usizeTy});
                builder.CreateCall(fn, {handle});
                return nullptr;
            }
            case BuiltinKind::OsReadFile: {
                llvm::Value* path = genExprArg(0);
                if (!path) return nullptr;
                auto [pathData, pathLen] = extractStringParts(path, "os.path.data", "os.path.len");
                if (!pathData || !pathLen) return nullptr;
                return callStringRuntime("yuan_os_read_file",
                                         {i8PtrTy, i64Ty},
                                         {pathData, pathLen},
                                         "os.read_file");
            }
            case BuiltinKind::OsWriteFile: {
                llvm::Value* path = genExprArg(0);
                llvm::Value* content = genExprArg(1);
                if (!path || !content) return nullptr;
                auto [pathData, pathLen] = extractStringParts(path, "os.path.data", "os.path.len");
                auto [contentData, contentLen] = extractStringParts(content, "os.content.data", "os.content.len");
                if (!pathData || !pathLen || !contentData || !contentLen) return nullptr;
                return callBoolI32Runtime("yuan_os_write_file",
                                          {i8PtrTy, i64Ty, i8PtrTy, i64Ty},
                                          {pathData, pathLen, contentData, contentLen},
                                          "os.write_file");
            }
            case BuiltinKind::OsExists:
            case BuiltinKind::OsIsFile:
            case BuiltinKind::OsIsDir:
            case BuiltinKind::OsCreateDir:
            case BuiltinKind::OsCreateDirAll:
            case BuiltinKind::OsRemoveDir:
            case BuiltinKind::OsRemoveFile: {
                llvm::Value* path = genExprArg(0);
                if (!path) return nullptr;
                auto [pathData, pathLen] = extractStringParts(path, "os.path.data", "os.path.len");
                if (!pathData || !pathLen) return nullptr;
                const char* runtimeName = nullptr;
                switch (Kind) {
                    case BuiltinKind::OsExists: runtimeName = "yuan_os_exists"; break;
                    case BuiltinKind::OsIsFile: runtimeName = "yuan_os_is_file"; break;
                    case BuiltinKind::OsIsDir: runtimeName = "yuan_os_is_dir"; break;
                    case BuiltinKind::OsCreateDir: runtimeName = "yuan_os_create_dir"; break;
                    case BuiltinKind::OsCreateDirAll: runtimeName = "yuan_os_create_dir_all"; break;
                    case BuiltinKind::OsRemoveDir: runtimeName = "yuan_os_remove_dir"; break;
                    case BuiltinKind::OsRemoveFile: runtimeName = "yuan_os_remove_file"; break;
                    default: break;
                }
                return callBoolI32Runtime(runtimeName,
                                          {i8PtrTy, i64Ty},
                                          {pathData, pathLen},
                                          "os.path.op");
            }
            case BuiltinKind::OsReadDirOpen: {
                llvm::Value* path = genExprArg(0);
                if (!path) return nullptr;
                auto [pathData, pathLen] = extractStringParts(path, "os.path.data", "os.path.len");
                if (!pathData || !pathLen) return nullptr;
                llvm::FunctionCallee fn = getOrInsert("yuan_os_read_dir_open", usizeTy, {i8PtrTy, i64Ty});
                llvm::Value* raw = builder.CreateCall(fn, {pathData, pathLen}, "os.dir.open");
                llvm::Type* targetTy = codegen.getLLVMType(expr->getType());
                return castIntegerValue(raw, targetTy ? targetTy : usizeTy, builder, "os.dir.handle.cast");
            }
            case BuiltinKind::OsReadDirNext:
            case BuiltinKind::OsReadDirEntryIsFile:
            case BuiltinKind::OsReadDirEntryIsDir: {
                llvm::Value* handle = genExprArg(0);
                if (!handle) return nullptr;
                handle = castIntegerValue(handle, usizeTy, builder, "os.dir.handle");
                const char* runtimeName = nullptr;
                switch (Kind) {
                    case BuiltinKind::OsReadDirNext: runtimeName = "yuan_os_read_dir_next"; break;
                    case BuiltinKind::OsReadDirEntryIsFile: runtimeName = "yuan_os_read_dir_entry_is_file"; break;
                    case BuiltinKind::OsReadDirEntryIsDir: runtimeName = "yuan_os_read_dir_entry_is_dir"; break;
                    default: break;
                }
                return callBoolI32Runtime(runtimeName, {usizeTy}, {handle}, "os.dir.bool");
            }
            case BuiltinKind::OsReadDirEntryPath:
            case BuiltinKind::OsReadDirEntryName: {
                llvm::Value* handle = genExprArg(0);
                if (!handle) return nullptr;
                handle = castIntegerValue(handle, usizeTy, builder, "os.dir.handle");
                const char* runtimeName =
                    (Kind == BuiltinKind::OsReadDirEntryPath)
                        ? "yuan_os_read_dir_entry_path"
                        : "yuan_os_read_dir_entry_name";
                return callStringRuntime(runtimeName, {usizeTy}, {handle}, "os.dir.str");
            }
            case BuiltinKind::OsReadDirClose: {
                llvm::Value* handle = genExprArg(0);
                if (!handle) return nullptr;
                handle = castIntegerValue(handle, usizeTy, builder, "os.dir.handle");
                llvm::FunctionCallee fn = getOrInsert("yuan_os_read_dir_close", voidTy, {usizeTy});
                builder.CreateCall(fn, {handle});
                return nullptr;
            }
            case BuiltinKind::OsStdinReadLine: {
                return callStringRuntime("yuan_os_stdin_read_line", {}, {}, "os.stdin.read_line");
            }
            case BuiltinKind::OsHttpGetStatus:
            case BuiltinKind::OsHttpGetBody: {
                llvm::Value* url = genExprArg(0);
                if (!url) return nullptr;
                auto [urlData, urlLen] = extractStringParts(url, "os.url.data", "os.url.len");
                if (!urlData || !urlLen) return nullptr;
                llvm::Value* timeout = llvm::ConstantInt::get(i64Ty, 30000);
                llvm::Value* headers = nullptr;
                llvm::Value* headersData = nullptr;
                llvm::Value* headersLen = nullptr;
                if (expr->getArgCount() > 1) {
                    timeout = genExprArg(1);
                    if (!timeout) return nullptr;
                    timeout = castIntegerValue(timeout, i64Ty, builder, "os.http.timeout");
                }
                if (expr->getArgCount() > 2) {
                    headers = genExprArg(2);
                    if (!headers) return nullptr;
                    auto parts = extractStringParts(headers, "os.headers.data", "os.headers.len");
                    headersData = parts.first;
                    headersLen = parts.second;
                    if (!headersData || !headersLen) return nullptr;
                }

                if (Kind == BuiltinKind::OsHttpGetStatus) {
                    if (headers) {
                        llvm::FunctionCallee fn = getOrInsert("yuan_os_http_get_status_ex", i32Ty,
                                                              {i8PtrTy, i64Ty, i8PtrTy, i64Ty, i64Ty});
                        return builder.CreateCall(
                            fn, {urlData, urlLen, headersData, headersLen, timeout}, "os.http.get.status");
                    }
                    llvm::FunctionCallee fn = getOrInsert("yuan_os_http_get_status", i32Ty,
                                                          {i8PtrTy, i64Ty, i64Ty});
                    return builder.CreateCall(fn, {urlData, urlLen, timeout}, "os.http.get.status");
                }

                if (headers) {
                    return callStringRuntime("yuan_os_http_get_body_ex",
                                             {i8PtrTy, i64Ty, i8PtrTy, i64Ty, i64Ty},
                                             {urlData, urlLen, headersData, headersLen, timeout},
                                             "os.http.get.body");
                }
                return callStringRuntime("yuan_os_http_get_body",
                                         {i8PtrTy, i64Ty, i64Ty},
                                         {urlData, urlLen, timeout},
                                         "os.http.get.body");
            }
            case BuiltinKind::OsHttpPostStatus:
            case BuiltinKind::OsHttpPostBody: {
                llvm::Value* url = genExprArg(0);
                llvm::Value* body = genExprArg(1);
                if (!url || !body) return nullptr;
                auto [urlData, urlLen] = extractStringParts(url, "os.url.data", "os.url.len");
                auto [bodyData, bodyLen] = extractStringParts(body, "os.body.data", "os.body.len");
                if (!urlData || !urlLen || !bodyData || !bodyLen) return nullptr;
                llvm::Value* timeout = llvm::ConstantInt::get(i64Ty, 30000);
                llvm::Value* headers = nullptr;
                llvm::Value* headersData = nullptr;
                llvm::Value* headersLen = nullptr;
                llvm::Value* stream = llvm::ConstantInt::get(i32Ty, 0);
                if (expr->getArgCount() > 2) {
                    timeout = genExprArg(2);
                    if (!timeout) return nullptr;
                    timeout = castIntegerValue(timeout, i64Ty, builder, "os.http.timeout");
                }
                if (expr->getArgCount() > 3) {
                    headers = genExprArg(3);
                    if (!headers) return nullptr;
                    auto parts = extractStringParts(headers, "os.headers.data", "os.headers.len");
                    headersData = parts.first;
                    headersLen = parts.second;
                    if (!headersData || !headersLen) return nullptr;
                }
                if (expr->getArgCount() > 4) {
                    stream = genExprArg(4);
                    if (!stream) return nullptr;
                    stream = castIntegerValue(stream, i32Ty, builder, "os.http.stream");
                }

                if (Kind == BuiltinKind::OsHttpPostStatus) {
                    if (headers && expr->getArgCount() > 4) {
                        llvm::FunctionCallee fn = getOrInsert("yuan_os_http_post_status_ex2", i32Ty,
                                                              {i8PtrTy, i64Ty, i8PtrTy, i64Ty, i8PtrTy, i64Ty, i64Ty, i32Ty});
                        return builder.CreateCall(fn, {urlData, urlLen, bodyData, bodyLen,
                                                       headersData, headersLen, timeout, stream},
                                                  "os.http.post.status");
                    }
                    if (headers) {
                        llvm::FunctionCallee fn = getOrInsert("yuan_os_http_post_status_ex", i32Ty,
                                                              {i8PtrTy, i64Ty, i8PtrTy, i64Ty, i8PtrTy, i64Ty, i64Ty});
                        return builder.CreateCall(fn, {urlData, urlLen, bodyData, bodyLen,
                                                       headersData, headersLen, timeout},
                                                  "os.http.post.status");
                    }
                    llvm::FunctionCallee fn = getOrInsert("yuan_os_http_post_status", i32Ty,
                                                          {i8PtrTy, i64Ty, i8PtrTy, i64Ty, i64Ty});
                    return builder.CreateCall(fn, {urlData, urlLen, bodyData, bodyLen, timeout},
                                              "os.http.post.status");
                }

                if (headers && expr->getArgCount() > 4) {
                    return callStringRuntime("yuan_os_http_post_body_ex2",
                                             {i8PtrTy, i64Ty, i8PtrTy, i64Ty, i8PtrTy, i64Ty, i64Ty, i32Ty},
                                             {urlData, urlLen, bodyData, bodyLen, headersData, headersLen, timeout, stream},
                                             "os.http.post.body");
                }
                if (headers) {
                    return callStringRuntime("yuan_os_http_post_body_ex",
                                             {i8PtrTy, i64Ty, i8PtrTy, i64Ty, i8PtrTy, i64Ty, i64Ty},
                                             {urlData, urlLen, bodyData, bodyLen, headersData, headersLen, timeout},
                                             "os.http.post.body");
                }
                return callStringRuntime("yuan_os_http_post_body",
                                         {i8PtrTy, i64Ty, i8PtrTy, i64Ty, i64Ty},
                                         {urlData, urlLen, bodyData, bodyLen, timeout},
                                         "os.http.post.body");
            }
            default:
                return nullptr;
        }
    }

private:
    BuiltinKind Kind;
};

} // namespace

std::unique_ptr<BuiltinHandler> createOsTimeUnixNanosBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsTimeUnixNanos);
}
std::unique_ptr<BuiltinHandler> createOsSleepNanosBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsSleepNanos);
}
std::unique_ptr<BuiltinHandler> createOsYieldBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsYield);
}
std::unique_ptr<BuiltinHandler> createOsThreadSpawnBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsThreadSpawn);
}
std::unique_ptr<BuiltinHandler> createOsThreadIsFinishedBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsThreadIsFinished);
}
std::unique_ptr<BuiltinHandler> createOsThreadJoinBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsThreadJoin);
}
std::unique_ptr<BuiltinHandler> createOsReadFileBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsReadFile);
}
std::unique_ptr<BuiltinHandler> createOsWriteFileBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsWriteFile);
}
std::unique_ptr<BuiltinHandler> createOsExistsBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsExists);
}
std::unique_ptr<BuiltinHandler> createOsIsFileBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsIsFile);
}
std::unique_ptr<BuiltinHandler> createOsIsDirBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsIsDir);
}
std::unique_ptr<BuiltinHandler> createOsCreateDirBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsCreateDir);
}
std::unique_ptr<BuiltinHandler> createOsCreateDirAllBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsCreateDirAll);
}
std::unique_ptr<BuiltinHandler> createOsRemoveDirBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsRemoveDir);
}
std::unique_ptr<BuiltinHandler> createOsRemoveFileBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsRemoveFile);
}
std::unique_ptr<BuiltinHandler> createOsReadDirOpenBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsReadDirOpen);
}
std::unique_ptr<BuiltinHandler> createOsReadDirNextBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsReadDirNext);
}
std::unique_ptr<BuiltinHandler> createOsReadDirEntryPathBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsReadDirEntryPath);
}
std::unique_ptr<BuiltinHandler> createOsReadDirEntryNameBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsReadDirEntryName);
}
std::unique_ptr<BuiltinHandler> createOsReadDirEntryIsFileBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsReadDirEntryIsFile);
}
std::unique_ptr<BuiltinHandler> createOsReadDirEntryIsDirBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsReadDirEntryIsDir);
}
std::unique_ptr<BuiltinHandler> createOsReadDirCloseBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsReadDirClose);
}
std::unique_ptr<BuiltinHandler> createOsStdinReadLineBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsStdinReadLine);
}
std::unique_ptr<BuiltinHandler> createOsHttpGetStatusBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsHttpGetStatus);
}
std::unique_ptr<BuiltinHandler> createOsHttpGetBodyBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsHttpGetBody);
}
std::unique_ptr<BuiltinHandler> createOsHttpPostStatusBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsHttpPostStatus);
}
std::unique_ptr<BuiltinHandler> createOsHttpPostBodyBuiltin() {
    return std::make_unique<OSBuiltin>(BuiltinKind::OsHttpPostBody);
}

} // namespace yuan
