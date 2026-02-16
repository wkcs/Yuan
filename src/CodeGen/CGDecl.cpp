/// \file CGDecl.cpp
/// \brief Implementation of declaration code generation.

#include "yuan/CodeGen/CodeGen.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/Pattern.h"
#include <iostream>
#include "yuan/AST/Expr.h"
#include "yuan/AST/Stmt.h"
#include "yuan/Sema/Type.h"
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Verifier.h>

namespace yuan {
namespace {

static bool typeHasGenericParam(Type* type) {
    if (!type) {
        return false;
    }
    if (type->isGeneric() || type->isTypeVar()) {
        return true;
    }
    if (type->isGenericInstance()) {
        auto* inst = static_cast<GenericInstanceType*>(type);
        for (Type* arg : inst->getTypeArgs()) {
            if (typeHasGenericParam(arg)) {
                return true;
            }
        }
        return false;
    }
    if (type->isReference()) {
        return typeHasGenericParam(static_cast<ReferenceType*>(type)->getPointeeType());
    }
    if (type->isPointer()) {
        return typeHasGenericParam(static_cast<PointerType*>(type)->getPointeeType());
    }
    if (type->isOptional()) {
        return typeHasGenericParam(static_cast<OptionalType*>(type)->getInnerType());
    }
    if (type->isArray()) {
        return typeHasGenericParam(static_cast<ArrayType*>(type)->getElementType());
    }
    if (type->isSlice()) {
        return typeHasGenericParam(static_cast<SliceType*>(type)->getElementType());
    }
    if (type->isTuple()) {
        auto* tuple = static_cast<TupleType*>(type);
        for (size_t i = 0; i < tuple->getElementCount(); ++i) {
            if (typeHasGenericParam(tuple->getElement(i))) {
                return true;
            }
        }
        return false;
    }
    if (type->isFunction()) {
        auto* fn = static_cast<FunctionType*>(type);
        for (Type* param : fn->getParamTypes()) {
            if (typeHasGenericParam(param)) {
                return true;
            }
        }
        return typeHasGenericParam(fn->getReturnType());
    }
    if (type->isError()) {
        return typeHasGenericParam(static_cast<ErrorType*>(type)->getSuccessType());
    }
    if (type->isRange()) {
        return typeHasGenericParam(static_cast<RangeType*>(type)->getElementType());
    }
    return false;
}

} // namespace

// ============================================================================
// Main declaration dispatcher
// ============================================================================

bool CodeGen::generateDecl(Decl* decl) {
    if (!decl) {
        return false;
    }

    bool ok = false;
    switch (decl->getKind()) {
        case ASTNode::Kind::VarDecl:
            ok = generateVarDecl(static_cast<VarDecl*>(decl));
            break;

        case ASTNode::Kind::ConstDecl:
            ok = generateConstDecl(static_cast<ConstDecl*>(decl));
            break;

        case ASTNode::Kind::FuncDecl:
            ok = generateFuncDecl(static_cast<FuncDecl*>(decl));
            break;

        case ASTNode::Kind::StructDecl:
            ok = generateStructDecl(static_cast<StructDecl*>(decl));
            break;

        case ASTNode::Kind::EnumDecl:
            ok = generateEnumDecl(static_cast<EnumDecl*>(decl));
            break;

        case ASTNode::Kind::TraitDecl:
            ok = generateTraitDecl(static_cast<TraitDecl*>(decl));
            break;

        case ASTNode::Kind::ImplDecl:
            ok = generateImplDecl(static_cast<ImplDecl*>(decl));
            break;

        case ASTNode::Kind::TypeAliasDecl:
            // 类型别名仅影响语义分析和符号解析，不产生 IR。
            ok = true;
            break;

        default:
            // Other declaration types not yet supported
            ok = false;
            break;
    }

    if (!ok) {
        std::cerr << "CodeGen failed for decl kind: " << static_cast<int>(decl->getKind()) << std::endl;
    }
    return ok;
}

// ============================================================================
// Variable and constant declarations
// ============================================================================

bool CodeGen::generateVarDecl(VarDecl* decl) {
    if (!decl) {
        return false;
    }

    const std::string& name = decl->getName();

    // 解构绑定（仅在函数内有效）
    if (decl->getPattern() && decl->getPattern()->getKind() != ASTNode::Kind::IdentifierPattern && CurrentFunction) {
        if (!decl->getInit()) {
            return false;
        }

        Type* semanticType = decl->getSemanticType();
        if (!semanticType) {
            return false;
        }

        llvm::Value* initValue = generateExpr(decl->getInit());
        if (!initValue) {
            return false;
        }

        if (!bindPattern(decl->getPattern(), initValue, semanticType)) {
            return false;
        }

        return true;
    }

    // Get the semantic type from Sema
    Type* semanticType = decl->getSemanticType();
    if (!semanticType) {
        // Type should have been set by Sema
        return false;
    }

    // Module values are compile-time only and do not require runtime storage.
    if (semanticType->isModule()) {
        return true;
    }

    // Get LLVM type from semantic type
    llvm::Type* llvmType = getLLVMType(semanticType);
    if (!llvmType) {
        return false;
    }
    if (semanticType->isFunction() && llvmType->isFunctionTy()) {
        llvmType = llvm::PointerType::get(llvmType, 0);
    }

    // Check if we're in a function (local variable) or at global scope
    if (CurrentFunction) {
        // Local variable - create alloca instruction
        llvm::IRBuilder<> TmpB(&CurrentFunction->getEntryBlock(),
                               CurrentFunction->getEntryBlock().begin());
        llvm::AllocaInst* alloca = TmpB.CreateAlloca(llvmType, nullptr, name);

        // Store the alloca in the value map
        ValueMap[decl] = alloca;

        // Generate initialization if present
        if (Expr* init = decl->getInit()) {
            llvm::Value* initValue = generateExpr(init);
            if (!initValue) {
                return false;
            }

            auto castValueIfNeeded = [&](llvm::Value* value, llvm::Type* targetType) -> llvm::Value* {
                if (!value || !targetType) {
                    return nullptr;
                }
                if (value->getType() == targetType) {
                    return value;
                }

                llvm::Type* sourceType = value->getType();
                if (sourceType->isIntegerTy() && targetType->isIntegerTy()) {
                    return Builder->CreateSExtOrTrunc(value, targetType, "var.init.int.cast");
                }
                if (sourceType->isFloatingPointTy() && targetType->isFloatingPointTy()) {
                    unsigned srcBits = sourceType->getPrimitiveSizeInBits();
                    unsigned dstBits = targetType->getPrimitiveSizeInBits();
                    if (srcBits < dstBits) {
                        return Builder->CreateFPExt(value, targetType, "var.init.fp.ext");
                    }
                    return Builder->CreateFPTrunc(value, targetType, "var.init.fp.trunc");
                }
                if (sourceType->isStructTy() && targetType->isStructTy()) {
                    const llvm::DataLayout& dl = Module->getDataLayout();
                    if (dl.getTypeAllocSize(sourceType) == dl.getTypeAllocSize(targetType)) {
                        llvm::AllocaInst* tmp = Builder->CreateAlloca(sourceType, nullptr,
                                                                      "var.init.struct.cast.tmp");
                        Builder->CreateStore(value, tmp);
                        llvm::Value* castPtr = Builder->CreateBitCast(
                            tmp, llvm::PointerType::get(targetType, 0),
                            "var.init.struct.cast.ptr");
                        return Builder->CreateLoad(targetType, castPtr, "var.init.struct.cast");
                    }
                }
                if (sourceType->isPointerTy() && targetType->isPointerTy()) {
                    return Builder->CreateBitCast(value, targetType, "var.init.ptr.cast");
                }
                if (sourceType->isPointerTy() && targetType->isIntegerTy()) {
                    return Builder->CreatePtrToInt(value, targetType, "var.init.ptrtoint");
                }
                if (sourceType->isIntegerTy() && targetType->isPointerTy()) {
                    return Builder->CreateIntToPtr(value, targetType, "var.init.inttoptr");
                }
                return nullptr;
            };

            auto coerceInitToTargetType = [&](llvm::Value* value, Type* initType, Type* targetType) -> llvm::Value* {
                if (!value || !targetType) {
                    return nullptr;
                }

                if (targetType->isOptional()) {
                    auto* expectedOptType = static_cast<OptionalType*>(targetType);
                    Type* expectedInnerType = expectedOptType->getInnerType();
                    llvm::Type* llvmExpectedOptType = getLLVMType(targetType);
                    llvm::Type* llvmExpectedInnerType = getLLVMType(expectedInnerType);
                    if (!llvmExpectedOptType || !llvmExpectedInnerType) {
                        return nullptr;
                    }

                    if (initType && initType->isOptional()) {
                        auto* actualOptType = static_cast<OptionalType*>(initType);
                        Type* actualInnerType = actualOptType->getInnerType();
                        llvm::Value* hasValue = Builder->CreateExtractValue(value, 0, "var.init.opt.has");

                        llvm::Value* innerValue = nullptr;
                        if (actualInnerType && actualInnerType->isVoid()) {
                            innerValue = llvm::Constant::getNullValue(llvmExpectedInnerType);
                        } else {
                            innerValue = Builder->CreateExtractValue(value, 1, "var.init.opt.value");
                            innerValue = castValueIfNeeded(innerValue, llvmExpectedInnerType);
                        }
                        if (!innerValue) {
                            return nullptr;
                        }

                        llvm::Value* normalized = llvm::UndefValue::get(llvmExpectedOptType);
                        normalized = Builder->CreateInsertValue(normalized, hasValue, 0, "var.init.opt.has");
                        normalized = Builder->CreateInsertValue(normalized, innerValue, 1, "var.init.opt.value");
                        return normalized;
                    }

                    llvm::Value* innerValue = value;
                    if (initType && initType->isReference() && innerValue->getType()->isPointerTy()) {
                        Type* pointeeType = static_cast<ReferenceType*>(initType)->getPointeeType();
                        llvm::Type* llvmPointeeType = getLLVMType(pointeeType);
                        if (!llvmPointeeType) {
                            return nullptr;
                        }
                        innerValue = Builder->CreateLoad(llvmPointeeType, innerValue, "var.init.autoderef");
                    }

                    innerValue = castValueIfNeeded(innerValue, llvmExpectedInnerType);
                    if (!innerValue) {
                        return nullptr;
                    }

                    llvm::Value* wrapped = llvm::UndefValue::get(llvmExpectedOptType);
                    wrapped = Builder->CreateInsertValue(
                        wrapped,
                        llvm::ConstantInt::get(llvm::Type::getInt1Ty(*Context), 1),
                        0,
                        "var.init.opt.has"
                    );
                    wrapped = Builder->CreateInsertValue(wrapped, innerValue, 1, "var.init.opt.value");
                    return wrapped;
                }

                if (initType && initType->isReference() &&
                    !targetType->isReference() &&
                    !targetType->isPointer() &&
                    value->getType()->isPointerTy()) {
                    llvm::Type* llvmTargetType = getLLVMType(targetType);
                    if (!llvmTargetType) {
                        return nullptr;
                    }
                    value = Builder->CreateLoad(llvmTargetType, value, "var.init.autoderef");
                }

                llvm::Type* llvmTargetType = getLLVMType(targetType);
                if (!llvmTargetType) {
                    return nullptr;
                }
                if (targetType->isFunction() && llvmTargetType->isFunctionTy()) {
                    llvmTargetType = llvm::PointerType::get(llvmTargetType, 0);
                }
                return castValueIfNeeded(value, llvmTargetType);
            };

            initValue = coerceInitToTargetType(initValue, init->getType(), semanticType);
            if (!initValue) {
                return false;
            }

            Builder->CreateStore(initValue, alloca);
        }

        return true;
    } else {
        // Global variable
        llvm::Constant* initializer = nullptr;

        if (decl->getInit()) {
            // Try to evaluate constant expression
            llvm::Value* initValue = generateExpr(decl->getInit());
            if (initValue && llvm::isa<llvm::Constant>(initValue)) {
                // Successfully evaluated as constant
                initializer = llvm::cast<llvm::Constant>(initValue);
            } else {
                // Not a constant expression, use zero initializer
                // Note: Sema should have caught this for global variables
                initializer = llvm::Constant::getNullValue(llvmType);
            }
        } else {
            // Zero initializer
            initializer = llvm::Constant::getNullValue(llvmType);
        }

        std::string symbolName = getGlobalSymbolName(decl, name, 'V');

        llvm::GlobalVariable* globalVar = new llvm::GlobalVariable(
            *Module,
            llvmType,
            !decl->isMutable(),  // isConstant if not mutable
            llvm::GlobalValue::InternalLinkage,
            initializer,
            symbolName
        );

        // Store in value map
        ValueMap[decl] = globalVar;

        return true;
    }
}

bool CodeGen::generateConstDecl(ConstDecl* decl) {
    if (!decl) {
        return false;
    }

    const std::string& name = decl->getName();

    // 获取初始化表达式的类型
    Expr* init = decl->getInit();
    if (init) {
        Type* initType = init->getType();
        // 如果初始化表达式是模块类型，不生成代码（模块是编译时概念）
        if (initType && initType->isModule()) {
            return true;  // 成功，但不生成任何代码
        }
    }

    // Get semantic type from the declaration
    Type* semanticType = decl->getSemanticType();
    if (!semanticType) {
        // If no semantic type, try to get it from the type node
        TypeNode* typeNode = decl->getType();
        if (!typeNode) {
            return false;
        }
        // Type node doesn't have semantic type directly, skip for now
        // This should have been set by Sema
        return false;
    }

    // Function-typed constants are compile-time aliases; don't emit a global.
    if (semanticType->isFunction()) {
        if (init) {
            llvm::Value* initValue = generateExpr(init);
            if (initValue) {
                ValueMap[decl] = initValue;
                return true;
            }
        }
        return false;
    }

    // Get LLVM type
    llvm::Type* llvmType = getLLVMType(semanticType);
    if (!llvmType) {
        return false;
    }

    // Constants must have initializers
    if (!init) {
        return false;
    }

    // Try to evaluate constant expression
    llvm::Value* initValue = generateExpr(init);
    llvm::Constant* initializer = nullptr;

    if (initValue && llvm::isa<llvm::Constant>(initValue)) {
        // Successfully evaluated as constant
        initializer = llvm::cast<llvm::Constant>(initValue);
    } else {
        // Not a constant expression, use zero initializer
        // Note: Sema should have caught this for constants
        initializer = llvm::Constant::getNullValue(llvmType);
    }

    // Create global constant
    std::string symbolName = getGlobalSymbolName(decl, name, 'C');

    llvm::GlobalVariable* global = new llvm::GlobalVariable(
        *Module,
        llvmType,
        true,  // isConstant = true for const
        llvm::GlobalValue::InternalLinkage,
        initializer,
        symbolName
    );

    // Store in value map
    ValueMap[decl] = global;

    return true;
}

// ============================================================================
// Function declarations
// ============================================================================

bool CodeGen::generateFuncDecl(FuncDecl* decl) {
    if (!decl) {
        return false;
    }

    struct ScopedInsertPointRestore {
        llvm::IRBuilder<>* Builder = nullptr;
        bool Active = false;
        llvm::IRBuilderBase::InsertPoint SavedIP;

        ScopedInsertPointRestore(llvm::IRBuilder<>* builder, bool active)
            : Builder(builder), Active(active) {
            if (Builder && Active) {
                SavedIP = Builder->saveIP();
            }
        }

        ~ScopedInsertPointRestore() {
            if (Builder && Active) {
                Builder->restoreIP(SavedIP);
            }
        }
    };

    // Lazy function materialization may happen while emitting another function.
    // Preserve caller insertion point so nested generation does not corrupt it.
    ScopedInsertPointRestore restoreIP(Builder.get(), CurrentFunction != nullptr);

    const std::string& name = decl->getName();

    // Special handling for main function:
    // Rename Yuan's main to yuan_main and create a C-style main wrapper
    bool isMainFunc = (name == "main" && decl->getParams().empty());
    std::string actualName = isMainFunc ? "yuan_main" : getFunctionSymbolName(decl);

    // Build parameter types
    std::vector<llvm::Type*> paramTypes;
    for (ParamDecl* param : decl->getParams()) {
        // Get semantic type from parameter declaration
        Type* paramSemanticType = param->getSemanticType();
        if (!paramSemanticType) {
            return false;
        }
        if (ActiveSpecializationDecl == decl && !GenericSubstStack.empty()) {
            paramSemanticType = substituteType(paramSemanticType);
        }

        llvm::Type* paramType = getLLVMType(paramSemanticType);
        if (!paramType) {
            return false;
        }
        if (paramType->isFunctionTy()) {
            paramType = llvm::PointerType::get(paramType, 0);
        }

        paramTypes.push_back(paramType);
    }

    // Get return type from semantic function type (handles canError)
    llvm::Type* returnType = nullptr;
    Type* returnSemanticType = decl->getSemanticType();
    if (!returnSemanticType || !returnSemanticType->isFunction()) {
        return false;
    }

    FunctionType* funcSemanticType = static_cast<FunctionType*>(returnSemanticType);
    Type* returnSemType = funcSemanticType->getReturnType();
    if (ActiveSpecializationDecl == decl && !GenericSubstStack.empty()) {
        returnSemType = substituteType(returnSemType);
    }
    if (funcSemanticType->canError()) {
        returnSemType = Ctx.getErrorType(returnSemType);
    }

    returnType = getLLVMType(returnSemType);
    if (!returnType) {
        return false;
    }
    if (returnType->isFunctionTy()) {
        returnType = llvm::PointerType::get(returnType, 0);
    }

    // Create function type
    llvm::FunctionType* funcType = llvm::FunctionType::get(
        returnType,
        paramTypes,
        false  // isVarArg
    );

    // Reuse existing declaration if present
    llvm::Function* func = Module->getFunction(actualName);
    if (func) {
        if (func->getFunctionType() != funcType) {
            return false;
        }
    } else {
        // Create function with actual name (yuan_main for main function)
        func = llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            actualName,
            Module.get()
        );
    }

    // Store in value map
    ValueMap[decl] = func;

    // If the function was already defined, skip regeneration
    if (!func->empty()) {
        return true;
    }

    // Set parameter names
    size_t idx = 0;
    for (llvm::Argument& arg : func->args()) {
        arg.setName(decl->getParams()[idx]->getName());
        ++idx;
    }

    // Generate function body if present
    if (BlockStmt* body = decl->getBody()) {
        // Skip generic bodies unless we're generating a specialization.
        bool skipGenericBody = typeHasGenericParam(decl->getSemanticType()) &&
                               (ActiveSpecializationDecl != decl);
        if (skipGenericBody) {
            return true;
        }

        // Create entry basic block
        llvm::BasicBlock* entry = llvm::BasicBlock::Create(*Context, "entry", func);
        Builder->SetInsertPoint(entry);

        // Save current function
        llvm::Function* savedFunc = CurrentFunction;
        std::string savedFuncName = CurrentFunctionName;
        FuncDecl* savedFuncDecl = CurrentFuncDecl;
        CurrentFunction = func;
        CurrentFunctionName = name;
        CurrentFuncDecl = decl;
        std::vector<Stmt*> savedDeferStack = std::move(DeferStack);
        DeferStack.clear();

        // Create allocas for parameters
        idx = 0;
        for (llvm::Argument& arg : func->args()) {
            ParamDecl* param = decl->getParams()[idx];

            // Create alloca for parameter
            llvm::AllocaInst* alloca = Builder->CreateAlloca(
                arg.getType(),
                nullptr,
                param->getName()
            );

            // Store parameter value
            Builder->CreateStore(&arg, alloca);

            // Map parameter decl to alloca
            ValueMap[param] = alloca;

            ++idx;
        }

        // Generate body with implicit return for last expression/match
        auto generateBodyWithImplicitReturn = [&](BlockStmt* block) -> bool {
            if (!block) {
                return false;
            }

            const auto& stmts = block->getStatements();
            for (size_t i = 0; i < stmts.size(); ++i) {
                Stmt* stmt = stmts[i];
                bool isLast = (i + 1 == stmts.size());

                if (isLast && funcSemanticType && funcSemanticType->getReturnType() &&
                    !funcSemanticType->getReturnType()->isVoid()) {
                    if (auto* exprStmt = dynamic_cast<ExprStmt*>(stmt)) {
                        ReturnStmt* ret = Ctx.create<ReturnStmt>(exprStmt->getRange(), exprStmt->getExpr());
                        return generateReturnStmt(ret);
                    }

                    if (auto* matchStmt = dynamic_cast<MatchStmt*>(stmt)) {
                        std::vector<MatchExpr::Arm> arms;
                        arms.reserve(matchStmt->getArms().size());
                        bool canConvert = true;
                        for (const auto& arm : matchStmt->getArms()) {
                            auto* exprStmt = dynamic_cast<ExprStmt*>(arm.Body);
                            if (!exprStmt) {
                                canConvert = false;
                                break;
                            }
                            arms.push_back({arm.Pat, arm.Guard, exprStmt->getExpr()});
                        }

                        if (canConvert) {
                            MatchExpr* matchExpr = Ctx.create<MatchExpr>(
                                matchStmt->getRange(),
                                matchStmt->getScrutinee(),
                                std::move(arms)
                            );
                            matchExpr->setType(funcSemanticType->getReturnType());
                            ReturnStmt* ret = Ctx.create<ReturnStmt>(matchExpr->getRange(), matchExpr);
                            return generateReturnStmt(ret);
                        }
                    }
                }

                if (!generateStmt(stmt)) {
                    return false;
                }

                if (Builder->GetInsertBlock()->getTerminator()) {
                    return true;
                }
            }

            return true;
        };

        bool success = generateBodyWithImplicitReturn(body);

        // Restore current function
        CurrentFunction = savedFunc;
        CurrentFunctionName = savedFuncName;
        CurrentFuncDecl = savedFuncDecl;

        if (!success) {
            DeferStack = std::move(savedDeferStack);
            std::cerr << "CodeGen failed in function body: " << name << std::endl;
            func->eraseFromParent();
            return false;
        }

        // Add implicit return for void functions
        if (returnType->isVoidTy()) {
            if (!Builder->GetInsertBlock()->getTerminator()) {
                executeDeferredStatements(0);
                Builder->CreateRetVoid();
            }
        }

        // Defer stack is per-function state and must not leak to other functions.
        DeferStack.clear();

        // Verify function
        if (llvm::verifyFunction(*func, &llvm::errs())) {
            DeferStack = std::move(savedDeferStack);
            func->eraseFromParent();
            return false;
        }

        DeferStack = std::move(savedDeferStack);
    }

    // If this is the main function, create a C-style main wrapper
    if (isMainFunc) {
        // Create C-style main: i32 main(i32 argc, i8** argv)
        llvm::Type* i32Type = llvm::Type::getInt32Ty(*Context);
        llvm::Type* i8PtrType = llvm::PointerType::get(llvm::Type::getInt8Ty(*Context), 0);
        llvm::Type* i8PtrPtrType = llvm::PointerType::get(i8PtrType, 0);

        std::vector<llvm::Type*> mainParamTypes = {i32Type, i8PtrPtrType};
        llvm::FunctionType* mainFuncType = llvm::FunctionType::get(
            i32Type,
            mainParamTypes,
            false
        );

        llvm::Function* mainFunc = llvm::Function::Create(
            mainFuncType,
            llvm::Function::ExternalLinkage,
            "main",
            Module.get()
        );

        // Set parameter names
        auto argIter = mainFunc->arg_begin();
        argIter->setName("argc");
        ++argIter;
        argIter->setName("argv");

        // Create entry block for main
        llvm::BasicBlock* mainEntry = llvm::BasicBlock::Create(*Context, "entry", mainFunc);
        Builder->SetInsertPoint(mainEntry);

        // For now, just call yuan_main() (no argv/argc forwarding)
        llvm::Value* yuanMainResult = nullptr;
        if (decl->isAsync()) {
            llvm::Type* voidType = llvm::Type::getVoidTy(*Context);
            llvm::FunctionType* asyncEntryType =
                llvm::FunctionType::get(voidType, {i8PtrType}, false);
            std::string asyncEntryName = actualName + ".async.entry";

            llvm::Function* asyncEntry = Module->getFunction(asyncEntryName);
            if (asyncEntry) {
                if (asyncEntry->getFunctionType() != asyncEntryType) {
                    return false;
                }
            } else {
                asyncEntry = llvm::Function::Create(
                    asyncEntryType,
                    llvm::GlobalValue::InternalLinkage,
                    asyncEntryName,
                    Module.get()
                );
                asyncEntry->arg_begin()->setName("out_slot");

                llvm::IRBuilderBase::InsertPoint savedIP = Builder->saveIP();
                llvm::BasicBlock* asyncEntryBB = llvm::BasicBlock::Create(*Context, "entry", asyncEntry);
                Builder->SetInsertPoint(asyncEntryBB);

                llvm::Value* callResult = Builder->CreateCall(func);
                if (func->getReturnType()->isVoidTy()) {
                    Builder->CreateRetVoid();
                } else {
                    llvm::Value* outSlot = asyncEntry->arg_begin();
                    llvm::Type* retType = func->getReturnType();
                    llvm::Type* retPtrType = llvm::PointerType::get(retType, 0);
                    llvm::Value* typedOutPtr = Builder->CreateBitCast(outSlot, retPtrType, "async.out.ptr");
                    Builder->CreateStore(callResult, typedOutPtr);
                    Builder->CreateRetVoid();
                }

                Builder->restoreIP(savedIP);
            }

            llvm::FunctionCallee asyncRun = Module->getOrInsertFunction(
                "yuan_async_run",
                llvm::FunctionType::get(voidType, {i8PtrType, i8PtrType}, false)
            );

            llvm::Value* entryPtr = Builder->CreateBitCast(asyncEntry, i8PtrType, "async.entry.ptr");
            auto* i8PtrTy = llvm::cast<llvm::PointerType>(i8PtrType);
            llvm::Value* nullPtr = llvm::ConstantPointerNull::get(i8PtrTy);

            if (func->getReturnType()->isVoidTy()) {
                Builder->CreateCall(asyncRun, {entryPtr, nullPtr});
            } else {
                llvm::AllocaInst* resultSlot = Builder->CreateAlloca(
                    func->getReturnType(),
                    nullptr,
                    "async.main.result"
                );
                llvm::Value* outPtr = Builder->CreateBitCast(resultSlot, i8PtrType, "async.out.slot");
                Builder->CreateCall(asyncRun, {entryPtr, outPtr});
                yuanMainResult = Builder->CreateLoad(func->getReturnType(), resultSlot, "async.main.value");
            }
        } else {
            yuanMainResult = Builder->CreateCall(func);
        }

        // Return the result from yuan_main
        if (func->getReturnType()->isVoidTy()) {
            Builder->CreateRet(llvm::ConstantInt::get(i32Type, 0));
        } else if (func->getReturnType()->isIntegerTy(32)) {
            Builder->CreateRet(yuanMainResult);
        } else if (func->getReturnType()->isIntegerTy()) {
            llvm::Value* casted = Builder->CreateSExtOrTrunc(yuanMainResult, i32Type, "main.ret.cast");
            Builder->CreateRet(casted);
        } else {
            // Fallback: return 0 for unsupported main return types
            Builder->CreateRet(llvm::ConstantInt::get(i32Type, 0));
        }

        // Verify main function
        if (llvm::verifyFunction(*mainFunc, &llvm::errs())) {
            mainFunc->eraseFromParent();
            return false;
        }
    }

    return true;
}

// ============================================================================
// Struct declarations
// ============================================================================

bool CodeGen::generateStructDecl(StructDecl* decl) {
    if (!decl) {
        return false;
    }

    // For struct declarations, the type itself is created during type resolution
    // by the Sema pass. Here we just need to ensure it's registered in LLVM.

    // The struct type should already exist in the type system
    // We don't need to generate code for the declaration itself,
    // just ensure the LLVM type is created (which happens via getLLVMType)

    if (Type* structType = decl->getSemanticType()) {
        if (structType->isStruct() && decl->isGeneric()) {
            auto* structTy = static_cast<StructType*>(structType);
            std::vector<std::string> params;
            params.reserve(decl->getGenericParams().size());
            for (const auto& param : decl->getGenericParams()) {
                params.push_back(param.Name);
            }
            StructGenericParams[structTy] = std::move(params);
        }
        (void)getLLVMType(structType);
    }

    return true;
}

// ============================================================================
// Enum declarations
// ============================================================================

bool CodeGen::generateEnumDecl(EnumDecl* decl) {
    if (!decl) {
        return false;
    }

    // Similar to structs, enum types are created during type resolution
    // The LLVM representation (tagged union) is handled by getLLVMType

    if (Type* enumType = decl->getSemanticType()) {
        (void)getLLVMType(enumType);
    }

    return true;
}

// ============================================================================
// Trait and Impl declarations
// ============================================================================

bool CodeGen::generateTraitDecl(TraitDecl* decl) {
    (void)decl;
    // Traits are purely compile-time constructs
    // No runtime code generation needed
    return true;
}

bool CodeGen::generateImplDecl(ImplDecl* decl) {
    if (!decl) {
        return false;
    }

    // For impl blocks, we need to generate code for each method
    const auto& methods = decl->getMethods();

    for (FuncDecl* method : methods) {
        if (!method) {
            continue;
        }

        // Generate code for the method
        // Methods are just functions with an implicit 'self' parameter
        if (!generateFuncDecl(method)) {
            return false;
        }
    }

    return true;
}

} // namespace yuan
