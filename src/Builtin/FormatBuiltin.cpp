/// \file FormatBuiltin.cpp
/// \brief @format 内置函数实现（带类型标记）
///
/// @format 用于格式化字符串，类似于 C++ 的 std::format，例如：
/// - @format("Hello, {}!", "World")
/// - @format("x = {}, y = {}", 10, 20)
/// - @format("{0} {1} {0}", "a", "b")
///
/// 实现策略：
/// 在调用 yuan_format 时，为每个参数传递类型标记
/// 调用格式：yuan_format(format, argc, type1, value1, type2, value2, ...)

#include "yuan/Builtin/BuiltinHandler.h"
#include "yuan/Sema/Sema.h"
#include "yuan/Sema/Type.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/Expr.h"
#include "yuan/CodeGen/CodeGen.h"
#include "yuan/Basic/Diagnostic.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>

namespace yuan {

/// \brief 参数类型枚举（与运行时库保持一致）
enum class YuanArgType : int32_t {
    String = 0,
    I32 = 1,
    I64 = 2,
    F32 = 3,
    F64 = 4,
    Bool = 5,
    Char = 6,
};

static Type* unwrapDisplayBaseType(Type* type) {
    Type* base = type;
    while (base) {
        if (base->isReference()) {
            base = static_cast<ReferenceType*>(base)->getPointeeType();
            continue;
        }
        if (base->isPointer()) {
            base = static_cast<PointerType*>(base)->getPointeeType();
            continue;
        }
        if (base->isGenericInstance()) {
            base = static_cast<GenericInstanceType*>(base)->getBaseType();
            continue;
        }
        break;
    }
    return base;
}

static llvm::Value* emitDisplayOrDebugString(CodeGen& codegen, Type* argType, llvm::Value* argValue) {
    if (!argType || !argValue) {
        return nullptr;
    }

    Type* baseType = unwrapDisplayBaseType(argType);
    if (!baseType || (!baseType->isStruct() && !baseType->isEnum())) {
        return nullptr;
    }

    ASTContext& astCtx = codegen.getASTContext();
    FuncDecl* method = astCtx.getDisplayImpl(baseType);
    if (!method) {
        method = astCtx.getDebugImpl(baseType);
    }
    if (!method) {
        return nullptr;
    }

    Type* semaType = method->getSemanticType();
    if (!semaType || !semaType->isFunction()) {
        return nullptr;
    }
    auto* funcType = static_cast<FunctionType*>(semaType);
    if (funcType->getParamCount() == 0) {
        return nullptr;
    }

    Type* selfType = funcType->getParam(0);
    llvm::Type* llvmSelfType = codegen.getLLVMType(selfType);
    if (!llvmSelfType) {
        return nullptr;
    }

    llvm::IRBuilder<>& builder = codegen.getBuilder();
    llvm::Value* selfArg = argValue;
    if (selfType->isReference() || selfType->isPointer()) {
        if (!argValue->getType()->isPointerTy()) {
            llvm::AllocaInst* tempAlloca = builder.CreateAlloca(
                argValue->getType(),
                nullptr,
                "display.self"
            );
            builder.CreateStore(argValue, tempAlloca);
            selfArg = tempAlloca;
        }
        if (selfArg->getType() != llvmSelfType) {
            selfArg = builder.CreateBitCast(selfArg, llvmSelfType, "display.self.cast");
        }
    } else {
        if (argValue->getType()->isPointerTy()) {
            selfArg = builder.CreateLoad(llvmSelfType, argValue, "display.self.load");
        } else if (argValue->getType() != llvmSelfType) {
            selfArg = builder.CreateBitCast(argValue, llvmSelfType, "display.self.cast");
        }
    }

    llvm::Module* module = codegen.getModule();
    llvm::Function* func = nullptr;

    // Try to specialize generic Display/Debug methods based on the argument type.
    CodeGen::GenericSubst mapping;
    Type* actualTypeForMap = argType;
    if (selfType && actualTypeForMap) {
        if (selfType->isReference() && !actualTypeForMap->isReference()) {
            auto* ref = static_cast<ReferenceType*>(selfType);
            actualTypeForMap = codegen.getASTContext().getReferenceType(actualTypeForMap, ref->isMutable());
        } else if (selfType->isPointer() && !actualTypeForMap->isPointer()) {
            auto* ptr = static_cast<PointerType*>(selfType);
            actualTypeForMap = codegen.getASTContext().getPointerType(actualTypeForMap, ptr->isMutable());
        }
        if (codegen.unifyGenericTypes(selfType, actualTypeForMap, mapping) && !mapping.empty()) {
            func = codegen.getOrCreateSpecializedFunction(method, mapping);
        } else if (mapping.empty() && actualTypeForMap->isGenericInstance()) {
            auto* genInst = static_cast<GenericInstanceType*>(actualTypeForMap);
            if (Type* base = genInst->getBaseType()) {
                if (base->isStruct()) {
                    if (codegen.buildStructGenericMapping(static_cast<StructType*>(base), genInst, mapping)) {
                        func = codegen.getOrCreateSpecializedFunction(method, mapping);
                    }
                }
            }
        }
    }

    std::string funcName = codegen.getFunctionSymbolName(method);
    if (!func) {
        func = module->getFunction(funcName);
    }
    if (!func) {
        llvm::Type* funcLLVMType = codegen.getLLVMType(funcType);
        auto* llvmFuncType = llvm::dyn_cast<llvm::FunctionType>(funcLLVMType);
        if (!llvmFuncType) {
            return nullptr;
        }
        func = llvm::Function::Create(
            llvmFuncType,
            llvm::Function::ExternalLinkage,
            funcName,
            module
        );
    }

    return builder.CreateCall(func, {selfArg}, "display.call");
}

/// \brief @format 内置函数处理器
class FormatBuiltin : public BuiltinHandler {
public:
    const char* getName() const override { return "format"; }

    BuiltinKind getKind() const override { return BuiltinKind::Format; }

    int getExpectedArgCount() const override { return -1; }

    std::string getArgDescription() const override {
        return "格式化字符串和可变参数";
    }

    Type* analyze(BuiltinCallExpr* expr, Sema& sema) override {
        if (expr->getArgCount() < 1) {
            return nullptr;
        }

        // 支持两种特殊形式：
        // 1) @format(args) : args 为 VarArgs<Value>，第一个参数作为格式字符串
        // 2) @format(format, args) : args 为 VarArgs<Value>
        if (expr->getArgCount() == 1) {
            const auto& onlyArg = expr->getArgs()[0];
            if (!onlyArg.isExpr()) {
                return nullptr;
            }

            Expr* argExpr = onlyArg.getExpr();
            Type* argType = sema.analyzeExpr(argExpr);
            if (!argType) {
                return nullptr;
            }

            if (argType->isVarArgs()) {
                auto* varType = static_cast<VarArgsType*>(argType);
                if (!varType->getElementType()->isValue()) {
                    return nullptr;
                }
                return sema.getContext().getStrType();
            }
        }

        const auto& formatArg = expr->getArgs()[0];
        if (!formatArg.isExpr()) {
            return nullptr;
        }

        Expr* formatExpr = formatArg.getExpr();
        Type* formatType = sema.analyzeExpr(formatExpr);
        if (!formatType || !formatType->isString()) {
            return nullptr;
        }

        if (expr->getArgCount() == 2) {
            const auto& maybeArgs = expr->getArgs()[1];
            if (maybeArgs.isExpr()) {
                Expr* argsExpr = maybeArgs.getExpr();
                Type* argsType = sema.analyzeExpr(argsExpr);
                if (argsType && argsType->isVarArgs()) {
                    auto* varType = static_cast<VarArgsType*>(argsType);
                    if (!varType->getElementType()->isValue()) {
                        return nullptr;
                    }
                    return formatType;
                }
            }
        }

        // 分析所有参数
        for (size_t i = 1; i < expr->getArgCount(); ++i) {
            const auto& arg = expr->getArgs()[i];
            if (!arg.isExpr()) {
                continue;
            }

            Expr* argExpr = arg.getExpr();
            Type* argType = sema.analyzeExpr(argExpr);
            if (!argType) {
                return nullptr;
            }

            // 验证类型
            if (!argType->isInteger() && !argType->isFloat() &&
                !argType->isString() && !argType->isBool() &&
                !argType->isChar() && !argType->isValue()) {
                // 允许带 Display/Debug 的结构体
                Type* baseType = unwrapDisplayBaseType(argType);

                if (!baseType || (!baseType->isStruct() && !baseType->isEnum()) ||
                    (!sema.getContext().getDisplayImpl(baseType) &&
                     !sema.getContext().getDebugImpl(baseType))) {
                    sema.getDiagnostics()
                        .report(DiagID::err_trait_not_implemented,
                                argExpr->getBeginLoc(), argExpr->getRange())
                        << "Display"
                        << argType->toString();
                    return nullptr;
                }
            }
        }

        return formatType;
    }

    llvm::Value* generate(BuiltinCallExpr* expr, CodeGen& codegen) override {
        if (expr->getArgCount() < 1) {
            return nullptr;
        }

        llvm::LLVMContext& context = codegen.getContext();
        llvm::Module* module = codegen.getModule();
#if defined(_WIN32)
        bool useWindowsSRet = true;
#else
        bool useWindowsSRet = false;
#endif

        // @format(args) 形式：args 为 VarArgs<Value>
        if (expr->getArgCount() == 1) {
            const auto& onlyArg = expr->getArgs()[0];
            if (!onlyArg.isExpr()) {
                return nullptr;
            }
            Expr* argExpr = onlyArg.getExpr();
            Type* argType = argExpr->getType();
            if (argType && argType->isVarArgs()) {
                auto* varType = static_cast<VarArgsType*>(argType);
                if (!varType->getElementType()->isValue()) {
                    return nullptr;
                }

                llvm::Value* varArgsValue = codegen.generateExprPublic(argExpr);
                if (!varArgsValue) {
                    return nullptr;
                }

                llvm::Type* varArgsLLVMType = codegen.getLLVMType(argType);
                if (!varArgsLLVMType) {
                    return nullptr;
                }
                auto* varArgsStructTy = llvm::dyn_cast<llvm::StructType>(varArgsLLVMType);
                if (!varArgsStructTy || varArgsStructTy->getNumElements() != 2) {
                    return nullptr;
                }
                llvm::Value* varArgsStructValue = varArgsValue;
                if (varArgsStructValue->getType()->isPointerTy()) {
                    llvm::Type* expectedPtrTy = llvm::PointerType::get(varArgsStructTy, 0);
                    if (varArgsStructValue->getType() != expectedPtrTy) {
                        varArgsStructValue = codegen.getBuilder().CreateBitCast(varArgsStructValue, expectedPtrTy, "fmt.varargs.ptr.cast");
                    }
                    varArgsStructValue = codegen.getBuilder().CreateLoad(varArgsStructTy, varArgsStructValue, "fmt.varargs.load");
                }
                llvm::Value* varArgsLen = codegen.getBuilder().CreateExtractValue(varArgsStructValue, 0, "fmt.varargs.len");
                llvm::Value* varArgsPtr = codegen.getBuilder().CreateExtractValue(varArgsStructValue, 1, "fmt.varargs.ptr");

                llvm::Function* formatAllFunc = module->getFunction("yuan_format_all");
                if (useWindowsSRet) {
                    llvm::Type* i64Ty = llvm::Type::getInt64Ty(context);
                    llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
                    llvm::StructType* stringStructTy = llvm::StructType::get(context, {i8PtrTy, i64Ty});
                    llvm::Type* stringStructPtrTy = llvm::PointerType::get(stringStructTy, 0);

                    if (!varArgsLen->getType()->isIntegerTy(64)) {
                        varArgsLen = codegen.getBuilder().CreateSExtOrTrunc(varArgsLen, i64Ty, "fmt.varargs.len64");
                    }

                    if (!formatAllFunc) {
                        llvm::FunctionType* fnType = llvm::FunctionType::get(
                            llvm::Type::getVoidTy(context),
                            {stringStructPtrTy, i64Ty, varArgsPtr->getType()},
                            false
                        );
                        formatAllFunc = llvm::Function::Create(
                            fnType,
                            llvm::Function::ExternalLinkage,
                            "yuan_format_all",
                            module
                        );
                        formatAllFunc->addParamAttr(0, llvm::Attribute::StructRet);
                    }

                    llvm::AllocaInst* outAlloca = codegen.getBuilder().CreateAlloca(stringStructTy, nullptr, "fmt.all.out");
                    codegen.getBuilder().CreateCall(formatAllFunc, {outAlloca, varArgsLen, varArgsPtr});
                    return codegen.getBuilder().CreateLoad(stringStructTy, outAlloca, "fmt.all.ret");
                }
                if (!formatAllFunc) {
                    llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
                    llvm::FunctionType* fnType = llvm::FunctionType::get(
                        llvm::StructType::get(i8PtrTy, llvm::Type::getInt64Ty(context)),
                        {varArgsLen->getType(), varArgsPtr->getType()},
                        false
                    );
                    formatAllFunc = llvm::Function::Create(
                        fnType,
                        llvm::Function::ExternalLinkage,
                        "yuan_format_all",
                        module
                    );
                }
                return codegen.getBuilder().CreateCall(formatAllFunc, {varArgsLen, varArgsPtr});
            }
        }

        // 获取格式化字符串
        const auto& formatArg = expr->getArgs()[0];
        if (!formatArg.isExpr()) {
            return nullptr;
        }

        Expr* formatExpr = formatArg.getExpr();
        llvm::Value* formatValue = codegen.generateExprPublic(formatExpr);
        if (!formatValue) {
            return nullptr;
        }

        // 提取格式化字符串指针
        llvm::Value* formatPtr = nullptr;
        if (formatValue->getType()->isStructTy()) {
            formatPtr = codegen.getBuilder().CreateExtractValue(formatValue, 0, "format.ptr");
        } else if (formatValue->getType()->isPointerTy()) {
            formatPtr = formatValue;
        } else {
            return nullptr;
        }

        // @format(format, args) 形式：args 为 VarArgs<Value>
        if (expr->getArgCount() == 2) {
            const auto& maybeArgs = expr->getArgs()[1];
            if (maybeArgs.isExpr()) {
                Expr* argsExpr = maybeArgs.getExpr();
                Type* argsType = argsExpr->getType();
                if (argsType && argsType->isVarArgs()) {
                    auto* varType = static_cast<VarArgsType*>(argsType);
                    if (!varType->getElementType()->isValue()) {
                        return nullptr;
                    }

                    llvm::Value* varArgsValue = codegen.generateExprPublic(argsExpr);
                    if (!varArgsValue) {
                        return nullptr;
                    }

                    llvm::Type* varArgsLLVMType = codegen.getLLVMType(argsType);
                    if (!varArgsLLVMType) {
                        return nullptr;
                    }
                    // Build a new VarArgs with format string inserted at index 0,
                    // then reuse yuan_format_all for consistent handling.
                    llvm::Type* valueLLVMType = codegen.getLLVMType(codegen.getASTContext().getValueType());
                    if (!valueLLVMType) {
                        return nullptr;
                    }

                    auto* varArgsStructTy = llvm::dyn_cast<llvm::StructType>(varArgsLLVMType);
                    if (!varArgsStructTy || varArgsStructTy->getNumElements() != 2) {
                        return nullptr;
                    }

                    llvm::IRBuilder<>& builder = codegen.getBuilder();
                    llvm::Type* i32Ty = llvm::Type::getInt32Ty(context);
                    llvm::Type* i64Ty = llvm::Type::getInt64Ty(context);
                    llvm::Type* valuePtrTy = llvm::PointerType::get(valueLLVMType, 0);

                    llvm::Value* oldLen = builder.CreateExtractValue(varArgsValue, 0, "varargs.len");
                    llvm::Value* oldPtr = builder.CreateExtractValue(varArgsValue, 1, "varargs.ptr");
                    if (oldPtr->getType() != valuePtrTy) {
                        oldPtr = builder.CreateBitCast(oldPtr, valuePtrTy, "varargs.ptr.cast");
                    }

                    llvm::Value* one = llvm::ConstantInt::get(i64Ty, 1);
                    llvm::Value* newLen = builder.CreateAdd(oldLen, one, "varargs.len.new");

                    llvm::Value* newValues = builder.CreateAlloca(valueLLVMType, newLen, "varargs.values.new");

                    // Build Value for format string (tag=String, data0=ptr, data1=len)
                    llvm::Value* formatLen = nullptr;
                    if (formatValue->getType()->isStructTy()) {
                        formatLen = builder.CreateExtractValue(formatValue, 1, "format.len");
                    } else {
                        formatLen = llvm::ConstantInt::get(i64Ty, 0);
                    }
                    if (!formatLen->getType()->isIntegerTy(64)) {
                        formatLen = builder.CreateSExtOrTrunc(formatLen, i64Ty, "format.len64");
                    }

                    llvm::Value* formatPtrInt = builder.CreatePtrToInt(formatPtr, i64Ty, "format.ptr.int");
                    llvm::Value* valueObj = llvm::UndefValue::get(valueLLVMType);
                    valueObj = builder.CreateInsertValue(
                        valueObj, llvm::ConstantInt::get(i32Ty, static_cast<int32_t>(YuanArgType::String)), 0, "value.tag");
                    valueObj = builder.CreateInsertValue(
                        valueObj, llvm::ConstantInt::get(i32Ty, 0), 1, "value.pad");
                    valueObj = builder.CreateInsertValue(valueObj, formatPtrInt, 2, "value.data0");
                    valueObj = builder.CreateInsertValue(valueObj, formatLen, 3, "value.data1");

                    llvm::Value* zero = llvm::ConstantInt::get(i64Ty, 0);
                    llvm::Value* firstPtr = builder.CreateGEP(valueLLVMType, newValues, zero, "varargs.values.first");
                    builder.CreateStore(valueObj, firstPtr);

                    // Copy existing values to new array starting at index 1 (safe for len==0)
                    llvm::Value* destPtr = builder.CreateGEP(valueLLVMType, newValues, one, "varargs.values.dest");
                    uint64_t valueSize = module->getDataLayout().getTypeAllocSize(valueLLVMType);
                    llvm::Value* copySize = builder.CreateMul(oldLen, llvm::ConstantInt::get(i64Ty, valueSize), "varargs.copy.size");
                    llvm::Value* hasValues = builder.CreateICmpSGT(oldLen, llvm::ConstantInt::get(i64Ty, 0));
                    llvm::Value* safeSrcPtr = builder.CreateSelect(hasValues, oldPtr, destPtr, "varargs.src");
                    builder.CreateMemCpy(destPtr, llvm::MaybeAlign(8), safeSrcPtr, llvm::MaybeAlign(8), copySize);

                    llvm::Value* newVarArgs = llvm::UndefValue::get(varArgsLLVMType);
                    newVarArgs = builder.CreateInsertValue(newVarArgs, newLen, 0, "varargs.len.new");
                    llvm::Value* newPtrCast = newValues;
                    if (newPtrCast->getType() != varArgsStructTy->getElementType(1)) {
                        newPtrCast = builder.CreateBitCast(newValues, varArgsStructTy->getElementType(1), "varargs.ptr.new.cast");
                    }
                    newVarArgs = builder.CreateInsertValue(newVarArgs, newPtrCast, 1, "varargs.ptr.new");

                    llvm::Function* formatAllFunc = module->getFunction("yuan_format_all");
                    if (useWindowsSRet) {
                        llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
                        llvm::StructType* stringStructTy = llvm::StructType::get(context, {i8PtrTy, i64Ty});
                        llvm::Type* stringStructPtrTy = llvm::PointerType::get(stringStructTy, 0);
                        if (!formatAllFunc) {
                            llvm::FunctionType* fnType = llvm::FunctionType::get(
                                llvm::Type::getVoidTy(context),
                                {stringStructPtrTy, i64Ty, newPtrCast->getType()},
                                false
                            );
                            formatAllFunc = llvm::Function::Create(
                                fnType,
                                llvm::Function::ExternalLinkage,
                                "yuan_format_all",
                                module
                            );
                            formatAllFunc->addParamAttr(0, llvm::Attribute::StructRet);
                        }

                        llvm::AllocaInst* outAlloca = builder.CreateAlloca(stringStructTy, nullptr, "fmt.all.out");
                        builder.CreateCall(formatAllFunc, {outAlloca, newLen, newPtrCast});
                        return builder.CreateLoad(stringStructTy, outAlloca, "fmt.all.ret");
                    }
                    if (!formatAllFunc) {
                        llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
                        llvm::FunctionType* fnType = llvm::FunctionType::get(
                            llvm::StructType::get(i8PtrTy, llvm::Type::getInt64Ty(context)),
                            {newLen->getType(), newPtrCast->getType()},
                            false
                        );
                        formatAllFunc = llvm::Function::Create(
                            fnType,
                            llvm::Function::ExternalLinkage,
                            "yuan_format_all",
                            module
                        );
                    }
                    return builder.CreateCall(formatAllFunc, {newLen, newPtrCast});
                }
            }
        }

        // 准备参数：format, argc, type1, value1, type2, value2, ...
        std::vector<llvm::Value*> args;
        args.push_back(formatPtr);

        // 参数数量
        llvm::Value* argCount = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(context),
            expr->getArgCount() - 1
        );
        args.push_back(argCount);

        // 处理每个参数：添加类型标记和值
        for (size_t i = 1; i < expr->getArgCount(); ++i) {
            const auto& arg = expr->getArgs()[i];
            if (!arg.isExpr()) {
                continue;
            }

            Expr* argExpr = arg.getExpr();
            llvm::Value* argValue = codegen.generateExprPublic(argExpr);
            if (!argValue) {
                return nullptr;
            }

            Type* argType = argExpr->getType();
            Type* resolvedType = codegen.substituteType(argType);
            YuanArgType typeTag;
            llvm::Value* valueToPass = nullptr;

            if (resolvedType && resolvedType->isString()) {
                argValue = codegen.coerceGenericValue(argValue, resolvedType);
                typeTag = YuanArgType::String;
                // 提取字符串指针
                if (argValue->getType()->isStructTy()) {
                    valueToPass = codegen.getBuilder().CreateExtractValue(argValue, 0, "str.ptr");
                } else {
                    valueToPass = argValue;
                }
            } else if (resolvedType && resolvedType->isInteger()) {
                argValue = codegen.coerceGenericValue(argValue, resolvedType);
                // 判断是 i32 还是 i64
                if (argValue->getType()->isIntegerTy(32)) {
                    typeTag = YuanArgType::I32;
                    valueToPass = argValue;
                } else {
                    typeTag = YuanArgType::I64;
                    // 如果不是 i64，转换为 i64
                    if (!argValue->getType()->isIntegerTy(64)) {
                        valueToPass = codegen.getBuilder().CreateSExtOrTrunc(
                            argValue,
                            llvm::Type::getInt64Ty(context),
                            "to_i64"
                        );
                    } else {
                        valueToPass = argValue;
                    }
                }
            } else if (resolvedType && resolvedType->isFloat()) {
                argValue = codegen.coerceGenericValue(argValue, resolvedType);
                // 判断是 f32 还是 f64
                if (argValue->getType()->isFloatTy()) {
                    typeTag = YuanArgType::F32;
                    // float 在可变参数中会被提升为 double
                    valueToPass = codegen.getBuilder().CreateFPExt(
                        argValue,
                        llvm::Type::getDoubleTy(context),
                        "to_double"
                    );
                } else {
                    typeTag = YuanArgType::F64;
                    valueToPass = argValue;
                }
            } else if (resolvedType && resolvedType->isBool()) {
                argValue = codegen.coerceGenericValue(argValue, resolvedType);
                typeTag = YuanArgType::Bool;
                // bool 转换为 i32
                valueToPass = codegen.getBuilder().CreateZExt(
                    argValue,
                    llvm::Type::getInt32Ty(context),
                    "bool_to_i32"
                );
            } else if (resolvedType && resolvedType->isChar()) {
                argValue = codegen.coerceGenericValue(argValue, resolvedType);
                typeTag = YuanArgType::Char;
                valueToPass = codegen.getBuilder().CreateZExt(
                    argValue,
                    llvm::Type::getInt32Ty(context),
                    "char_to_i32"
                );
            } else if (resolvedType && resolvedType->isValue()) {
                argValue = codegen.coerceGenericValue(argValue, resolvedType);
                // Value 转换为字符串
                llvm::Function* valueToStringFunc = module->getFunction("yuan_value_to_string");
                llvm::Value* strValue = nullptr;
                if (useWindowsSRet) {
                    if (!valueToStringFunc) {
                        llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
                        llvm::Type* i64Ty = llvm::Type::getInt64Ty(context);
                        llvm::Type* stringStructTy = llvm::StructType::get(context, {i8PtrTy, i64Ty});
                        llvm::Type* i32Ty = llvm::Type::getInt32Ty(context);
                        llvm::StructType* valueStructTy = llvm::StructType::getTypeByName(context, "YuanValue");
                        if (!valueStructTy) {
                            valueStructTy = llvm::StructType::create(context, "YuanValue");
                        }
                        if (valueStructTy->isOpaque()) {
                            valueStructTy->setBody({i32Ty, i32Ty, i64Ty, i64Ty});
                        }
                        llvm::Type* stringStructPtrTy = llvm::PointerType::get(stringStructTy, 0);
                        llvm::FunctionType* valueToStringType = llvm::FunctionType::get(
                            llvm::Type::getVoidTy(context),
                            {stringStructPtrTy, valueStructTy},
                            false
                        );
                        valueToStringFunc = llvm::Function::Create(
                            valueToStringType,
                            llvm::Function::ExternalLinkage,
                            "yuan_value_to_string",
                            module
                        );
                        valueToStringFunc->addParamAttr(0, llvm::Attribute::StructRet);
                    }

                    llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
                    llvm::Type* i64Ty = llvm::Type::getInt64Ty(context);
                    llvm::StructType* stringStructTy = llvm::StructType::get(context, {i8PtrTy, i64Ty});
                    llvm::AllocaInst* outAlloca = codegen.getBuilder().CreateAlloca(stringStructTy, nullptr, "value.str.out");
                    codegen.getBuilder().CreateCall(valueToStringFunc, {outAlloca, argValue});
                    strValue = codegen.getBuilder().CreateLoad(stringStructTy, outAlloca, "value.str.ret");
                } else {
                    if (!valueToStringFunc) {
                        llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
                        llvm::Type* i64Ty = llvm::Type::getInt64Ty(context);
                        llvm::Type* stringStructTy = llvm::StructType::get(context, {i8PtrTy, i64Ty});
                        llvm::Type* i32Ty = llvm::Type::getInt32Ty(context);
                        llvm::StructType* valueStructTy = llvm::StructType::getTypeByName(context, "YuanValue");
                        if (!valueStructTy) {
                            valueStructTy = llvm::StructType::create(context, "YuanValue");
                        }
                        if (valueStructTy->isOpaque()) {
                            valueStructTy->setBody({i32Ty, i32Ty, i64Ty, i64Ty});
                        }

                        llvm::FunctionType* valueToStringType = llvm::FunctionType::get(
                            stringStructTy,
                            {valueStructTy},
                            false
                        );
                        valueToStringFunc = llvm::Function::Create(
                            valueToStringType,
                            llvm::Function::ExternalLinkage,
                            "yuan_value_to_string",
                            module
                        );
                    }
                    strValue = codegen.getBuilder().CreateCall(valueToStringFunc, {argValue});
                }
                llvm::Value* strPtr = codegen.getBuilder().CreateExtractValue(strValue, 0, "value.str.ptr");
                typeTag = YuanArgType::String;
                valueToPass = strPtr;
            } else {
                llvm::Value* strValue = emitDisplayOrDebugString(codegen, resolvedType ? resolvedType : argType, argValue);
                if (!strValue) {
                    return nullptr;
                }
                typeTag = YuanArgType::String;
                if (strValue->getType()->isStructTy()) {
                    valueToPass = codegen.getBuilder().CreateExtractValue(strValue, 0, "display.str.ptr");
                } else {
                    valueToPass = strValue;
                }
            }

            // 添加类型标记
            llvm::Value* typeValue = llvm::ConstantInt::get(
                llvm::Type::getInt32Ty(context),
                static_cast<int32_t>(typeTag)
            );
            args.push_back(typeValue);

            // 添加值
            args.push_back(valueToPass);
        }

        // 声明 yuan_format 函数
        llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
        llvm::Type* i64Ty = llvm::Type::getInt64Ty(context);
        llvm::StructType* stringStructTy = llvm::StructType::get(context, {i8PtrTy, i64Ty});
        llvm::Type* i32Ty = llvm::Type::getInt32Ty(context);
        if (useWindowsSRet) {
            llvm::Type* stringStructPtrTy = llvm::PointerType::get(stringStructTy, 0);
            llvm::FunctionType* formatFuncType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context),
                {stringStructPtrTy, i8PtrTy, i32Ty},
                true
            );
            llvm::FunctionCallee callee = module->getOrInsertFunction("yuan_format", formatFuncType);
            if (auto* fn = llvm::dyn_cast<llvm::Function>(callee.getCallee())) {
                fn->addParamAttr(0, llvm::Attribute::StructRet);
            }

            llvm::AllocaInst* outAlloca = codegen.getBuilder().CreateAlloca(stringStructTy, nullptr, "fmt.out");
            std::vector<llvm::Value*> callArgs;
            callArgs.reserve(args.size() + 1);
            callArgs.push_back(outAlloca);
            callArgs.insert(callArgs.end(), args.begin(), args.end());
            codegen.getBuilder().CreateCall(callee, callArgs);
            return codegen.getBuilder().CreateLoad(stringStructTy, outAlloca, "fmt.ret");
        }

        std::vector<llvm::Type*> paramTypes = {i8PtrTy, i32Ty};
        llvm::FunctionType* formatFuncType = llvm::FunctionType::get(
            stringStructTy,
            paramTypes,
            true
        );
        llvm::FunctionCallee callee = module->getOrInsertFunction("yuan_format", formatFuncType);
        return codegen.getBuilder().CreateCall(callee, args);
    }
};

/// \brief 创建 @format 内置函数处理器
std::unique_ptr<BuiltinHandler> createFormatBuiltin() {
    return std::make_unique<FormatBuiltin>();
}

} // namespace yuan
