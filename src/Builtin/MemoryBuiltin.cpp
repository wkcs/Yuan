/// \file MemoryBuiltin.cpp
/// \brief 内存相关内置函数实现。
///
/// 提供 @alloc / @realloc / @free / @memcpy / @memmove / @memset /
/// @str_from_parts / @slice 等内置函数。

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

static bool isSizeInteger(Type* type) {
    return type && type->isInteger();
}

static bool isBytePointer(Type* type) {
    if (!type || !type->isPointer()) {
        return false;
    }
    auto* ptrType = static_cast<PointerType*>(type);
    Type* elem = ptrType->getPointeeType();
    if (!elem || !elem->isInteger()) {
        return false;
    }
    auto* intType = static_cast<IntegerType*>(elem);
    return intType->getBitWidth() == 8;
}

static llvm::Value* castToI8Ptr(llvm::Value* value, llvm::LLVMContext& context,
                                llvm::IRBuilder<>& builder) {
    llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
    if (value->getType() == i8PtrTy) {
        return value;
    }
    return builder.CreateBitCast(value, i8PtrTy, "i8ptr");
}

static llvm::Value* castToSize(llvm::Value* value, llvm::Type* sizeTy,
                               llvm::IRBuilder<>& builder) {
    if (value->getType() == sizeTy) {
        return value;
    }
    if (value->getType()->isIntegerTy()) {
        return builder.CreateZExtOrTrunc(value, sizeTy, "size.cast");
    }
    return builder.CreateBitCast(value, sizeTy, "size.cast");
}

class MemoryBuiltin : public BuiltinHandler {
public:
    explicit MemoryBuiltin(BuiltinKind kind) : Kind(kind) {}

    const char* getName() const override {
        switch (Kind) {
            case BuiltinKind::Alloc: return "alloc";
            case BuiltinKind::Realloc: return "realloc";
            case BuiltinKind::Free: return "free";
            case BuiltinKind::Memcpy: return "memcpy";
            case BuiltinKind::Memmove: return "memmove";
            case BuiltinKind::Memset: return "memset";
            case BuiltinKind::StrFromParts: return "str_from_parts";
            case BuiltinKind::Slice: return "slice";
            default: return "memory";
        }
    }

    BuiltinKind getKind() const override { return Kind; }

    int getExpectedArgCount() const override {
        switch (Kind) {
            case BuiltinKind::Alloc: return 1;
            case BuiltinKind::Free: return 1;
            case BuiltinKind::Realloc: return 2;
            case BuiltinKind::Memcpy: return 3;
            case BuiltinKind::Memmove: return 3;
            case BuiltinKind::Memset: return 3;
            case BuiltinKind::StrFromParts: return 2;
            case BuiltinKind::Slice: return 2;
            default: return -1;
        }
    }

    std::string getArgDescription() const override {
        switch (Kind) {
            case BuiltinKind::Alloc: return "size";
            case BuiltinKind::Free: return "ptr";
            case BuiltinKind::Realloc: return "ptr, size";
            case BuiltinKind::Memcpy: return "dest, src, size";
            case BuiltinKind::Memmove: return "dest, src, size";
            case BuiltinKind::Memset: return "dest, value, size";
            case BuiltinKind::StrFromParts: return "ptr, len";
            case BuiltinKind::Slice: return "ptr, len";
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

        auto& ctx = sema.getContext();

        auto analyzeArg = [&](unsigned index) -> Type* {
            auto& arg = expr->getArgs()[index];
            if (!arg.isExpr()) {
                return nullptr;
            }
            return sema.analyzeExpr(arg.getExpr());
        };

        switch (Kind) {
            case BuiltinKind::Alloc: {
                Type* sizeTy = analyzeArg(0);
                if (!isSizeInteger(sizeTy)) {
                    sema.getDiagnostics()
                        .report(DiagID::err_type_mismatch, expr->getArgs()[0].getExpr()->getBeginLoc(),
                                expr->getArgs()[0].getExpr()->getRange())
                        << "integer" << (sizeTy ? sizeTy->toString() : "unknown");
                    return nullptr;
                }
                return ctx.getPointerType(ctx.getU8Type(), true);
            }
            case BuiltinKind::Realloc: {
                Type* ptrTy = analyzeArg(0);
                Type* sizeTy = analyzeArg(1);
                if (!ptrTy || !ptrTy->isPointer()) {
                    sema.getDiagnostics()
                        .report(DiagID::err_type_mismatch, expr->getArgs()[0].getExpr()->getBeginLoc(),
                                expr->getArgs()[0].getExpr()->getRange())
                        << "pointer" << (ptrTy ? ptrTy->toString() : "unknown");
                    return nullptr;
                }
                if (!isSizeInteger(sizeTy)) {
                    sema.getDiagnostics()
                        .report(DiagID::err_type_mismatch, expr->getArgs()[1].getExpr()->getBeginLoc(),
                                expr->getArgs()[1].getExpr()->getRange())
                        << "integer" << (sizeTy ? sizeTy->toString() : "unknown");
                    return nullptr;
                }
                return ptrTy;
            }
            case BuiltinKind::Free: {
                Type* ptrTy = analyzeArg(0);
                if (!ptrTy || !ptrTy->isPointer()) {
                    sema.getDiagnostics()
                        .report(DiagID::err_type_mismatch, expr->getArgs()[0].getExpr()->getBeginLoc(),
                                expr->getArgs()[0].getExpr()->getRange())
                        << "pointer" << (ptrTy ? ptrTy->toString() : "unknown");
                    return nullptr;
                }
                return ctx.getVoidType();
            }
            case BuiltinKind::Memcpy:
            case BuiltinKind::Memmove: {
                Type* destTy = analyzeArg(0);
                Type* srcTy = analyzeArg(1);
                Type* sizeTy = analyzeArg(2);
                if (!destTy || !destTy->isPointer()) {
                    sema.getDiagnostics()
                        .report(DiagID::err_type_mismatch, expr->getArgs()[0].getExpr()->getBeginLoc(),
                                expr->getArgs()[0].getExpr()->getRange())
                        << "pointer" << (destTy ? destTy->toString() : "unknown");
                    return nullptr;
                }
                if (!srcTy || !srcTy->isPointer()) {
                    sema.getDiagnostics()
                        .report(DiagID::err_type_mismatch, expr->getArgs()[1].getExpr()->getBeginLoc(),
                                expr->getArgs()[1].getExpr()->getRange())
                        << "pointer" << (srcTy ? srcTy->toString() : "unknown");
                    return nullptr;
                }
                if (!isSizeInteger(sizeTy)) {
                    sema.getDiagnostics()
                        .report(DiagID::err_type_mismatch, expr->getArgs()[2].getExpr()->getBeginLoc(),
                                expr->getArgs()[2].getExpr()->getRange())
                        << "integer" << (sizeTy ? sizeTy->toString() : "unknown");
                    return nullptr;
                }
                return ctx.getVoidType();
            }
            case BuiltinKind::Memset: {
                Type* destTy = analyzeArg(0);
                Type* valueTy = analyzeArg(1);
                Type* sizeTy = analyzeArg(2);
                if (!destTy || !destTy->isPointer()) {
                    sema.getDiagnostics()
                        .report(DiagID::err_type_mismatch, expr->getArgs()[0].getExpr()->getBeginLoc(),
                                expr->getArgs()[0].getExpr()->getRange())
                        << "pointer" << (destTy ? destTy->toString() : "unknown");
                    return nullptr;
                }
                if (!valueTy || !valueTy->isInteger()) {
                    sema.getDiagnostics()
                        .report(DiagID::err_type_mismatch, expr->getArgs()[1].getExpr()->getBeginLoc(),
                                expr->getArgs()[1].getExpr()->getRange())
                        << "integer" << (valueTy ? valueTy->toString() : "unknown");
                    return nullptr;
                }
                if (!isSizeInteger(sizeTy)) {
                    sema.getDiagnostics()
                        .report(DiagID::err_type_mismatch, expr->getArgs()[2].getExpr()->getBeginLoc(),
                                expr->getArgs()[2].getExpr()->getRange())
                        << "integer" << (sizeTy ? sizeTy->toString() : "unknown");
                    return nullptr;
                }
                return ctx.getVoidType();
            }
            case BuiltinKind::StrFromParts: {
                Type* ptrTy = analyzeArg(0);
                Type* lenTy = analyzeArg(1);
                if (!ptrTy || !ptrTy->isPointer()) {
                    sema.getDiagnostics()
                        .report(DiagID::err_type_mismatch, expr->getArgs()[0].getExpr()->getBeginLoc(),
                                expr->getArgs()[0].getExpr()->getRange())
                        << "pointer" << (ptrTy ? ptrTy->toString() : "unknown");
                    return nullptr;
                }
                if (!isSizeInteger(lenTy)) {
                    sema.getDiagnostics()
                        .report(DiagID::err_type_mismatch, expr->getArgs()[1].getExpr()->getBeginLoc(),
                                expr->getArgs()[1].getExpr()->getRange())
                        << "integer" << (lenTy ? lenTy->toString() : "unknown");
                    return nullptr;
                }
                if (!isBytePointer(ptrTy)) {
                    sema.getDiagnostics()
                        .report(DiagID::err_type_mismatch, expr->getArgs()[0].getExpr()->getBeginLoc(),
                                expr->getArgs()[0].getExpr()->getRange())
                        << "*u8" << ptrTy->toString();
                    return nullptr;
                }
                return ctx.getStrType();
            }
            case BuiltinKind::Slice: {
                Type* ptrTy = analyzeArg(0);
                Type* lenTy = analyzeArg(1);
                if (!ptrTy || !ptrTy->isPointer()) {
                    sema.getDiagnostics()
                        .report(DiagID::err_type_mismatch, expr->getArgs()[0].getExpr()->getBeginLoc(),
                                expr->getArgs()[0].getExpr()->getRange())
                        << "pointer" << (ptrTy ? ptrTy->toString() : "unknown");
                    return nullptr;
                }
                if (!isSizeInteger(lenTy)) {
                    sema.getDiagnostics()
                        .report(DiagID::err_type_mismatch, expr->getArgs()[1].getExpr()->getBeginLoc(),
                                expr->getArgs()[1].getExpr()->getRange())
                        << "integer" << (lenTy ? lenTy->toString() : "unknown");
                    return nullptr;
                }
                auto* ptrType = static_cast<PointerType*>(ptrTy);
                return ctx.getSliceType(ptrType->getPointeeType(), ptrType->isMutable());
            }
            default:
                return nullptr;
        }
    }

    llvm::Value* generate(BuiltinCallExpr* expr, CodeGen& codegen) override {
        if (!expr) {
            return nullptr;
        }

        llvm::LLVMContext& context = codegen.getContext();
        llvm::Module* module = codegen.getModule();
        llvm::IRBuilder<>& builder = codegen.getBuilder();

        auto genArg = [&](unsigned index) -> llvm::Value* {
            if (index >= expr->getArgCount()) {
                return nullptr;
            }
            const auto& arg = expr->getArgs()[index];
            if (!arg.isExpr()) {
                return nullptr;
            }
            return codegen.generateExprPublic(arg.getExpr());
        };

        llvm::Type* sizeTy = nullptr;
        if (module) {
            sizeTy = module->getDataLayout().getIntPtrType(context);
        }
        if (!sizeTy) {
            sizeTy = llvm::Type::getInt64Ty(context);
        }

        switch (Kind) {
            case BuiltinKind::Alloc: {
                llvm::Value* sizeVal = genArg(0);
                if (!sizeVal) {
                    return nullptr;
                }
                sizeVal = castToSize(sizeVal, sizeTy, builder);
                llvm::FunctionCallee mallocFn = module->getOrInsertFunction(
                    "malloc",
                    llvm::FunctionType::get(
                        llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                        {sizeTy},
                        false
                    )
                );
                llvm::Value* rawPtr = builder.CreateCall(mallocFn, {sizeVal}, "alloc");
                llvm::Type* desired = codegen.getLLVMType(expr->getType());
                if (desired && desired->isPointerTy() && rawPtr->getType() != desired) {
                    return builder.CreateBitCast(rawPtr, desired, "alloc.cast");
                }
                return rawPtr;
            }
            case BuiltinKind::Realloc: {
                llvm::Value* ptrVal = genArg(0);
                llvm::Value* sizeVal = genArg(1);
                if (!ptrVal || !sizeVal) {
                    return nullptr;
                }
                ptrVal = castToI8Ptr(ptrVal, context, builder);
                sizeVal = castToSize(sizeVal, sizeTy, builder);
                llvm::FunctionCallee reallocFn = module->getOrInsertFunction(
                    "realloc",
                    llvm::FunctionType::get(
                        llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                        {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0), sizeTy},
                        false
                    )
                );
                llvm::Value* rawPtr = builder.CreateCall(reallocFn, {ptrVal, sizeVal}, "realloc");
                llvm::Type* desired = codegen.getLLVMType(expr->getType());
                if (desired && desired->isPointerTy() && rawPtr->getType() != desired) {
                    return builder.CreateBitCast(rawPtr, desired, "realloc.cast");
                }
                return rawPtr;
            }
            case BuiltinKind::Free: {
                llvm::Value* ptrVal = genArg(0);
                if (!ptrVal) {
                    return nullptr;
                }
                ptrVal = castToI8Ptr(ptrVal, context, builder);
                llvm::FunctionCallee freeFn = module->getOrInsertFunction(
                    "free",
                    llvm::FunctionType::get(
                        llvm::Type::getVoidTy(context),
                        {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)},
                        false
                    )
                );
                builder.CreateCall(freeFn, {ptrVal});
                return nullptr;
            }
            case BuiltinKind::Memcpy:
            case BuiltinKind::Memmove: {
                llvm::Value* destVal = genArg(0);
                llvm::Value* srcVal = genArg(1);
                llvm::Value* sizeVal = genArg(2);
                if (!destVal || !srcVal || !sizeVal) {
                    return nullptr;
                }
                destVal = castToI8Ptr(destVal, context, builder);
                srcVal = castToI8Ptr(srcVal, context, builder);
                sizeVal = castToSize(sizeVal, sizeTy, builder);

                const char* fnName = (Kind == BuiltinKind::Memcpy) ? "memcpy" : "memmove";
                llvm::FunctionCallee fn = module->getOrInsertFunction(
                    fnName,
                    llvm::FunctionType::get(
                        llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                        {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                         llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                         sizeTy},
                        false
                    )
                );
                builder.CreateCall(fn, {destVal, srcVal, sizeVal});
                return nullptr;
            }
            case BuiltinKind::Memset: {
                llvm::Value* destVal = genArg(0);
                llvm::Value* valueVal = genArg(1);
                llvm::Value* sizeVal = genArg(2);
                if (!destVal || !valueVal || !sizeVal) {
                    return nullptr;
                }
                destVal = castToI8Ptr(destVal, context, builder);
                sizeVal = castToSize(sizeVal, sizeTy, builder);
                llvm::Type* i32Ty = llvm::Type::getInt32Ty(context);
                if (valueVal->getType() != i32Ty) {
                    if (valueVal->getType()->isIntegerTy()) {
                        valueVal = builder.CreateZExtOrTrunc(valueVal, i32Ty, "memset.val");
                    } else {
                        valueVal = builder.CreateBitCast(valueVal, i32Ty, "memset.val");
                    }
                }
                llvm::FunctionCallee fn = module->getOrInsertFunction(
                    "memset",
                    llvm::FunctionType::get(
                        llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                        {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0), i32Ty, sizeTy},
                        false
                    )
                );
                builder.CreateCall(fn, {destVal, valueVal, sizeVal});
                return nullptr;
            }
            case BuiltinKind::StrFromParts: {
                llvm::Value* ptrVal = genArg(0);
                llvm::Value* lenVal = genArg(1);
                if (!ptrVal || !lenVal) {
                    return nullptr;
                }

                llvm::Type* strTy = codegen.getLLVMType(codegen.getASTContext().getStrType());
                if (!strTy) {
                    return nullptr;
                }

                llvm::Value* result = llvm::UndefValue::get(strTy);
                llvm::Value* castPtr = castToI8Ptr(ptrVal, context, builder);
                llvm::Value* lenI64 = castToSize(lenVal, llvm::Type::getInt64Ty(context), builder);
                result = builder.CreateInsertValue(result, castPtr, 0, "str.ptr");
                result = builder.CreateInsertValue(result, lenI64, 1, "str.len");
                return result;
            }
            case BuiltinKind::Slice: {
                llvm::Value* ptrVal = genArg(0);
                llvm::Value* lenVal = genArg(1);
                if (!ptrVal || !lenVal) {
                    return nullptr;
                }
                Type* semType = expr->getType();
                llvm::Type* sliceTy = semType ? codegen.getLLVMType(semType) : nullptr;
                if (!sliceTy) {
                    return nullptr;
                }

                llvm::Value* result = llvm::UndefValue::get(sliceTy);
                llvm::Type* elemPtrTy = llvm::cast<llvm::StructType>(sliceTy)->getElementType(0);
                if (ptrVal->getType() != elemPtrTy) {
                    ptrVal = builder.CreateBitCast(ptrVal, elemPtrTy, "slice.ptr.cast");
                }
                llvm::Value* lenI64 = castToSize(lenVal, llvm::Type::getInt64Ty(context), builder);
                result = builder.CreateInsertValue(result, ptrVal, 0, "slice.ptr");
                result = builder.CreateInsertValue(result, lenI64, 1, "slice.len");
                return result;
            }
            default:
                return nullptr;
        }
    }

private:
    BuiltinKind Kind;
};

} // namespace

std::unique_ptr<BuiltinHandler> createAllocBuiltin() {
    return std::make_unique<MemoryBuiltin>(BuiltinKind::Alloc);
}

std::unique_ptr<BuiltinHandler> createReallocBuiltin() {
    return std::make_unique<MemoryBuiltin>(BuiltinKind::Realloc);
}

std::unique_ptr<BuiltinHandler> createFreeBuiltin() {
    return std::make_unique<MemoryBuiltin>(BuiltinKind::Free);
}

std::unique_ptr<BuiltinHandler> createMemcpyBuiltin() {
    return std::make_unique<MemoryBuiltin>(BuiltinKind::Memcpy);
}

std::unique_ptr<BuiltinHandler> createMemmoveBuiltin() {
    return std::make_unique<MemoryBuiltin>(BuiltinKind::Memmove);
}

std::unique_ptr<BuiltinHandler> createMemsetBuiltin() {
    return std::make_unique<MemoryBuiltin>(BuiltinKind::Memset);
}

std::unique_ptr<BuiltinHandler> createStrFromPartsBuiltin() {
    return std::make_unique<MemoryBuiltin>(BuiltinKind::StrFromParts);
}

std::unique_ptr<BuiltinHandler> createSliceBuiltin() {
    return std::make_unique<MemoryBuiltin>(BuiltinKind::Slice);
}

} // namespace yuan
