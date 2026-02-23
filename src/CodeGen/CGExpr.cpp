/// \file CGExpr.cpp
/// \brief Implementation of expression code generation.

#include "yuan/CodeGen/CodeGen.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/Pattern.h"
#include "yuan/Sema/Type.h"
#include "yuan/Builtin/BuiltinRegistry.h"
#include "yuan/Builtin/BuiltinHandler.h"
#include <algorithm>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Intrinsics.h>

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

static Type* unwrapTypeAlias(Type* type) {
    Type* current = type;
    while (current && current->isTypeAlias()) {
        current = static_cast<TypeAlias*>(current)->getAliasedType();
    }
    return current;
}

static llvm::Type* normalizeFirstClassType(llvm::Type* type) {
    if (!type) {
        return nullptr;
    }
    if (type->isFunctionTy()) {
        return llvm::PointerType::get(type, 0);
    }
    return type;
}

} // namespace

// ============================================================================
// Main expression dispatcher
// ============================================================================

llvm::Value* CodeGen::generateExpr(Expr* expr) {
    if (!expr) {
        return nullptr;
    }

    switch (expr->getKind()) {
        // Literals
        case ASTNode::Kind::IntegerLiteralExpr:
            return generateIntegerLiteral(static_cast<IntegerLiteralExpr*>(expr));

        case ASTNode::Kind::FloatLiteralExpr:
            return generateFloatLiteral(static_cast<FloatLiteralExpr*>(expr));

        case ASTNode::Kind::BoolLiteralExpr:
            return generateBoolLiteral(static_cast<BoolLiteralExpr*>(expr));

        case ASTNode::Kind::CharLiteralExpr:
            return generateCharLiteral(static_cast<CharLiteralExpr*>(expr));

        case ASTNode::Kind::StringLiteralExpr:
            return generateStringLiteral(static_cast<StringLiteralExpr*>(expr));

        case ASTNode::Kind::NoneLiteralExpr: {
            // None 字面量：在语义分析时类型为 ?void，但需要根据上下文确定实际类型
            Type* type = expr->getType();
            if (!type) {
                return nullptr;
            }
            
            // 如果类型是 Optional，使用它；否则查找上下文中期望的 Optional 类型
            if (!type->isOptional()) {
                // 尝试从父节点获取期望的类型
                return nullptr;
            }
            
            auto* optType = static_cast<OptionalType*>(type);
            Type* innerType = optType->getInnerType();
            
            // 对于 None，内部类型可能是 void（语义分析的默认值）
            // 我们需要使用正确的内部类型来构建 Optional 值
            llvm::Type* llvmOptType = getLLVMType(type);
            if (!llvmOptType) {
                return nullptr;
            }
            
            // 如果内部类型是 void，使用 i8 作为占位符
            llvm::Type* llvmInnerType = nullptr;
            if (innerType->isVoid()) {
                llvmInnerType = llvm::Type::getInt8Ty(*Context);
            } else {
                llvmInnerType = getLLVMType(innerType);
            }
            
            if (!llvmInnerType) {
                return nullptr;
            }
            llvmInnerType = normalizeFirstClassType(llvmInnerType);

            llvm::Value* result = llvm::UndefValue::get(llvmOptType);
            llvm::Value* hasValue = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*Context), 0);
            llvm::Value* innerZero = llvm::Constant::getNullValue(llvmInnerType);
            result = Builder->CreateInsertValue(result, hasValue, 0, "none.has");
            result = Builder->CreateInsertValue(result, innerZero, 1, "none.val");
            return result;
        }

        // Identifiers and member access
        case ASTNode::Kind::IdentifierExpr:
            return generateIdentifierExpr(static_cast<IdentifierExpr*>(expr));

        case ASTNode::Kind::MemberExpr:
            return generateMemberExpr(static_cast<MemberExpr*>(expr));

        // Operators
        case ASTNode::Kind::BinaryExpr:
            return generateBinaryExpr(static_cast<BinaryExpr*>(expr));

        case ASTNode::Kind::UnaryExpr:
            return generateUnaryExpr(static_cast<UnaryExpr*>(expr));

        case ASTNode::Kind::CastExpr:
            return generateCastExpr(static_cast<CastExpr*>(expr));

        // Assignment
        case ASTNode::Kind::AssignExpr:
            return generateAssignExpr(static_cast<AssignExpr*>(expr));

        // Function calls
        case ASTNode::Kind::CallExpr:
            return generateCallExpr(static_cast<CallExpr*>(expr));

        // Indexing
        case ASTNode::Kind::IndexExpr:
            return generateIndexExpr(static_cast<IndexExpr*>(expr));

        case ASTNode::Kind::SliceExpr:
            return generateSliceExpr(static_cast<SliceExpr*>(expr));

        // Struct literal
        case ASTNode::Kind::StructExpr:
            return generateStructExpr(static_cast<StructExpr*>(expr));

        // Array literal
        case ASTNode::Kind::ArrayExpr:
            return generateArrayExpr(static_cast<ArrayExpr*>(expr));

        // Tuple literal
        case ASTNode::Kind::TupleExpr:
            return generateTupleExpr(static_cast<TupleExpr*>(expr));

        case ASTNode::Kind::ClosureExpr:
            return generateClosureExpr(static_cast<ClosureExpr*>(expr));

        case ASTNode::Kind::AwaitExpr:
            return generateAwaitExpr(static_cast<AwaitExpr*>(expr));

        // Control flow expressions
        case ASTNode::Kind::IfExpr:
            return generateIfExpr(static_cast<IfExpr*>(expr));

        case ASTNode::Kind::MatchExpr:
            return generateMatchExpr(static_cast<MatchExpr*>(expr));

        case ASTNode::Kind::BlockExpr:
            return generateBlockExpr(static_cast<BlockExpr*>(expr));

        // Error handling
        case ASTNode::Kind::ErrorPropagateExpr:
            return generateErrorPropagateExpr(static_cast<ErrorPropagateExpr*>(expr));

        case ASTNode::Kind::ErrorHandleExpr:
            return generateErrorHandleExpr(static_cast<ErrorHandleExpr*>(expr));

        // Builtin function calls
        case ASTNode::Kind::BuiltinCallExpr:
            return generateBuiltinCallExpr(static_cast<BuiltinCallExpr*>(expr));

        // Range expression
        case ASTNode::Kind::RangeExpr:
            return generateRangeExpr(static_cast<RangeExpr*>(expr));

        default:
            // Other expression types not yet supported
            return nullptr;
    }
}

// ============================================================================
// Literal expressions
// ============================================================================

llvm::Value* CodeGen::generateIntegerLiteral(IntegerLiteralExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // Get the type from semantic analysis
    Type* type = unwrapTypeAlias(expr->getType());
    if (!type) {
        return nullptr;
    }

    // Contextual typing may lift integer literals to optional integers.
    if (type->isOptional()) {
        auto* optType = static_cast<OptionalType*>(type);
        Type* innerType = unwrapTypeAlias(optType->getInnerType());
        if (!innerType || !innerType->isInteger()) {
            return nullptr;
        }

        llvm::Type* llvmOptType = getLLVMType(type);
        llvm::Type* llvmInnerType = getLLVMType(innerType);
        if (!llvmOptType || !llvmInnerType || !llvmOptType->isStructTy()) {
            return nullptr;
        }

        llvm::Value* innerValue = llvm::ConstantInt::get(llvmInnerType, expr->getValue(), expr->isSigned());
        llvm::Value* wrapped = llvm::UndefValue::get(llvmOptType);
        wrapped = Builder->CreateInsertValue(
            wrapped,
            llvm::ConstantInt::get(llvm::Type::getInt1Ty(*Context), 1),
            0,
            "opt.int.has");
        wrapped = Builder->CreateInsertValue(wrapped, innerValue, 1, "opt.int.val");
        return wrapped;
    }

    if (!type->isInteger()) {
        return nullptr;
    }

    // Convert to LLVM type
    llvm::Type* llvmType = getLLVMType(type);
    if (!llvmType || !llvmType->isIntegerTy()) {
        return nullptr;
    }

    // Create constant integer
    uint64_t value = expr->getValue();
    return llvm::ConstantInt::get(llvmType, value, expr->isSigned());
}

llvm::Value* CodeGen::generateFloatLiteral(FloatLiteralExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // Get the type from semantic analysis
    Type* type = unwrapTypeAlias(expr->getType());
    if (!type || !type->isFloat()) {
        return nullptr;
    }

    // Convert to LLVM type
    llvm::Type* llvmType = getLLVMType(type);
    if (!llvmType || !llvmType->isFloatingPointTy()) {
        return nullptr;
    }

    // Create constant float
    double value = expr->getValue();
    return llvm::ConstantFP::get(llvmType, value);
}

llvm::Value* CodeGen::generateBoolLiteral(BoolLiteralExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // Bool is represented as i1
    llvm::Type* boolType = llvm::Type::getInt1Ty(*Context);
    return llvm::ConstantInt::get(boolType, expr->getValue() ? 1 : 0);
}

llvm::Value* CodeGen::generateCharLiteral(CharLiteralExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // Get the type from semantic analysis
    Type* type = unwrapTypeAlias(expr->getType());
    if (!type || !type->isChar()) {
        return nullptr;
    }

    // Convert to LLVM type
    llvm::Type* llvmType = getLLVMType(type);
    if (!llvmType || !llvmType->isIntegerTy()) {
        return nullptr;
    }

    // Get the Unicode codepoint
    uint32_t codepoint = expr->getCodepoint();

    // Create constant integer with the full codepoint value
    // The LLVM type should match what getLLVMType returns for char
    // (currently i8, but could be i32 for full Unicode support)
    unsigned bitWidth = llvmType->getIntegerBitWidth();

    if (bitWidth < 32 && codepoint > ((1u << bitWidth) - 1)) {
        // Codepoint doesn't fit in the target type, truncate
        // This happens when char is i8 but we have a Unicode character
        codepoint = codepoint & ((1u << bitWidth) - 1);
    }

    return llvm::ConstantInt::get(llvmType, codepoint);
}

llvm::Value* CodeGen::emitStringLiteralValue(const std::string& value) {
    // Create a global constant string
    llvm::Constant* strConstant = llvm::ConstantDataArray::getString(
        *Context, value, true  // AddNull = true
    );

    // Create a global variable to hold the string
    llvm::GlobalVariable* strGlobal = new llvm::GlobalVariable(
        *Module,
        strConstant->getType(),
        true,  // isConstant
        llvm::GlobalValue::PrivateLinkage,
        strConstant,
        ".str"
    );

    // String is represented as { i8*, i64 } (pointer + length)
    llvm::Type* i8PtrType = llvm::PointerType::get(llvm::Type::getInt8Ty(*Context), 0);
    llvm::Type* i64Type = llvm::Type::getInt64Ty(*Context);
    llvm::Type* stringType = llvm::StructType::get(i8PtrType, i64Type);

    // Get pointer to the string data (GEP to first element)
    llvm::Constant* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Context), 0);
    llvm::Constant* indices[] = {zero, zero};
    llvm::Constant* strPtr = llvm::ConstantExpr::getGetElementPtr(
        strGlobal->getValueType(), strGlobal, indices
    );

    // Create the string struct { ptr, len }
    llvm::Constant* len = llvm::ConstantInt::get(i64Type, value.length());
    llvm::Constant* stringValue = llvm::ConstantStruct::get(
        llvm::cast<llvm::StructType>(stringType),
        strPtr,
        len
    );

    return stringValue;
}

llvm::Value* CodeGen::generateStringLiteral(StringLiteralExpr* expr) {
    if (!expr) {
        return nullptr;
    }
    return emitStringLiteralValue(expr->getValue());
}

// ============================================================================
// Identifier and member access expressions
// ============================================================================

llvm::Value* CodeGen::generateIdentifierExpr(IdentifierExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // Get the resolved declaration from Sema
    Decl* decl = expr->getResolvedDecl();
    if (!decl) {
        // Declaration not resolved - Sema should have caught this
        return nullptr;
    }

    // Look up the value in the value map
    auto it = ValueMap.find(decl);
    if (it == ValueMap.end()) {
        // Try lazy materialization for constants/functions.
        if (decl->getKind() == ASTNode::Kind::ConstDecl) {
            auto* constDecl = static_cast<ConstDecl*>(decl);
            if (Expr* init = constDecl->getInit()) {
                llvm::Value* initValue = generateExpr(init);
                if (initValue) {
                    ValueMap[decl] = initValue;
                    it = ValueMap.find(decl);
                }
            }
        } else if (decl->getKind() == ASTNode::Kind::FuncDecl) {
            if (generateDecl(decl)) {
                it = ValueMap.find(decl);
            }
        }
        if (it == ValueMap.end()) {
            // Declaration not generated yet
            return nullptr;
        }
    }

    llvm::Value* value = it->second;

    const bool canEmitInstructions =
        Builder && Builder->GetInsertBlock() && CurrentFunction != nullptr;

    // If it's an alloca or global variable, load the value.
    // Note: in global constant initialization there is no active insertion point,
    // so we must not emit instructions such as `load`.
    if (llvm::isa<llvm::AllocaInst>(value) || llvm::isa<llvm::GlobalVariable>(value)) {
        if (!canEmitInstructions) {
            if (llvm::GlobalVariable* global = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
                if (global->isConstant() && global->hasInitializer()) {
                    return global->getInitializer();
                }
                return global;
            }
            return nullptr;
        }

        // Get the type to load
        llvm::Type* llvmType = nullptr;

        // Try to get type from the expression's semantic type
        Type* exprType = expr->getType();
        if (exprType && !GenericSubstStack.empty()) {
            exprType = substituteType(exprType);
        }
        if (exprType) {
            llvmType = getLLVMType(exprType);
            llvmType = normalizeFirstClassType(llvmType);
        }

        // If that fails, try to get it from the declaration
        if (!llvmType && decl->getKind() == ASTNode::Kind::ParamDecl) {
            ParamDecl* paramDecl = static_cast<ParamDecl*>(decl);
            Type* paramType = paramDecl->getSemanticType();
            if (paramType && !GenericSubstStack.empty()) {
                paramType = substituteType(paramType);
            }
            if (paramType) {
                llvmType = getLLVMType(paramType);
                llvmType = normalizeFirstClassType(llvmType);
            }
        }

        // If still no type, try to infer from the alloca's allocated type
        if (!llvmType) {
            if (llvm::AllocaInst* alloca = llvm::dyn_cast<llvm::AllocaInst>(value)) {
                llvmType = alloca->getAllocatedType();
            } else if (llvm::GlobalVariable* global = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
                llvmType = global->getValueType();
            }
        }

        if (!llvmType) {
            return nullptr;
        }

        llvm::Value* loaded = Builder->CreateLoad(llvmType, value, expr->getName());
        if (expr->isMoveConsumed()) {
            setDropFlag(decl, false);
        }
        return loaded;
    }

    // Otherwise it's already a value (function, parameter after load, etc.)
    return value;
}

llvm::Value* CodeGen::generateMemberExpr(MemberExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // Get the type of the base expression
    Type* baseType = expr->getBase()->getType();
    if (!baseType) {
        return nullptr;
    }
    if (baseType->isReference()) {
        baseType = static_cast<ReferenceType*>(baseType)->getPointeeType();
    }
    if (!GenericSubstStack.empty()) {
        baseType = substituteType(baseType);
    }

    // If base is a reference/pointer, use the pointee type for member lookup
    if (baseType->isReference()) {
        baseType = static_cast<ReferenceType*>(baseType)->getPointeeType();
    }
    if (baseType->isPointer()) {
        baseType = static_cast<PointerType*>(baseType)->getPointeeType();
    }

    // Preserve generic instance for layout; use base type only for field lookup.
    GenericInstanceType* genInst = nullptr;
    if (baseType->isGenericInstance()) {
        genInst = static_cast<GenericInstanceType*>(baseType);
    }

    // Handle module member access
    if (baseType->isModule()) {
        auto* moduleTy = static_cast<ModuleType*>(baseType);
        const ModuleType::Member* moduleMember = moduleTy->getMember(expr->getMember());
        if (!moduleMember) {
            return nullptr;
        }

        if (!moduleMember->LinkName.empty()) {
            Type* memberType = moduleMember->MemberType;
            if (memberType && memberType->isFunction()) {
                llvm::FunctionType* llvmFnTy = llvm::dyn_cast<llvm::FunctionType>(getLLVMType(memberType));
                if (!llvmFnTy) {
                    return nullptr;
                }
                llvm::Function* fn = Module->getFunction(moduleMember->LinkName);
                if (fn) {
                    if (fn->getFunctionType() != llvmFnTy) {
                        return nullptr;
                    }
                } else {
                    fn = llvm::Function::Create(
                        llvmFnTy,
                        llvm::Function::ExternalLinkage,
                        moduleMember->LinkName,
                        Module.get());
                }

                if (Decl* resolvedDecl = expr->getResolvedDecl()) {
                    ValueMap[resolvedDecl] = fn;
                }
                return fn;
            }

            llvm::Type* llvmMemberTy = getLLVMType(memberType);
            if (!llvmMemberTy) {
                return nullptr;
            }

            llvm::GlobalVariable* gv = Module->getGlobalVariable(moduleMember->LinkName);
            if (!gv) {
                gv = new llvm::GlobalVariable(
                    *Module,
                    llvmMemberTy,
                    false,
                    llvm::GlobalValue::ExternalLinkage,
                    nullptr,
                    moduleMember->LinkName);
            }

            if (Decl* resolvedDecl = expr->getResolvedDecl()) {
                ValueMap[resolvedDecl] = gv;
            }
            return Builder->CreateLoad(llvmMemberTy, gv, expr->getMember());
        }

        // 模块成员访问通常是编译时概念，但常量别名可能需要值
        Decl* resolvedDecl = expr->getResolvedDecl();
        if (!resolvedDecl) {
            return nullptr;
        }

        auto materializeDeclValue = [&](Decl* targetDecl) -> llvm::Value* {
            auto found = ValueMap.find(targetDecl);
            if (found != ValueMap.end()) {
                return found->second;
            }

            switch (targetDecl->getKind()) {
                case ASTNode::Kind::VarDecl:
                case ASTNode::Kind::ConstDecl:
                case ASTNode::Kind::FuncDecl:
                case ASTNode::Kind::StructDecl:
                case ASTNode::Kind::EnumDecl:
                case ASTNode::Kind::TraitDecl:
                case ASTNode::Kind::ImplDecl:
                    (void)generateDecl(static_cast<Decl*>(targetDecl));
                    break;
                default:
                    break;
            }

            found = ValueMap.find(targetDecl);
            if (found == ValueMap.end()) {
                return nullptr;
            }
            return found->second;
        };

        llvm::Value* value = materializeDeclValue(resolvedDecl);
        if (!value) {
            return nullptr;
        }
        if (llvm::isa<llvm::AllocaInst>(value) || llvm::isa<llvm::GlobalVariable>(value)) {
            llvm::Type* llvmType = nullptr;
            if (resolvedDecl->getKind() == ASTNode::Kind::VarDecl) {
                auto* varDecl = static_cast<VarDecl*>(resolvedDecl);
                if (Type* semType = varDecl->getSemanticType()) {
                    llvmType = getLLVMType(semType);
                }
            } else if (resolvedDecl->getKind() == ASTNode::Kind::ConstDecl) {
                auto* constDecl = static_cast<ConstDecl*>(resolvedDecl);
                if (Type* semType = constDecl->getSemanticType()) {
                    llvmType = getLLVMType(semType);
                }
            }

            if (!llvmType) {
                if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(value)) {
                    llvmType = alloca->getAllocatedType();
                } else if (auto* global = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
                    llvmType = global->getValueType();
                }
            }

            if (!llvmType) {
                return nullptr;
            }
            return Builder->CreateLoad(llvmType, value, expr->getMember());
        }

        // Function values are returned directly.
        return value;
    }

    // Handle enum type member access (unit variants)
    if (auto* identBase = dynamic_cast<IdentifierExpr*>(expr->getBase())) {
        Decl* baseDecl = identBase->getResolvedDecl();
        Type* enumType = nullptr;

        if (baseDecl && baseDecl->getKind() == ASTNode::Kind::EnumDecl) {
            enumType = static_cast<EnumDecl*>(baseDecl)->getSemanticType();
        } else if (!baseDecl) {
            // Builtin enum types may have no Decl; fall back to the identifier's type.
            enumType = baseType;
        }

        if (enumType && enumType->isGenericInstance()) {
            enumType = static_cast<GenericInstanceType*>(enumType)->getBaseType();
        }

        if (enumType && enumType->isEnum()) {
            auto* enumTy = static_cast<EnumType*>(enumType);
            const EnumType::Variant* variant = enumTy->getVariant(expr->getMember());
            if (!variant) {
                return nullptr;
            }
            if (!variant->Data.empty()) {
                // Non-unit variants are constructed via call or struct literal
                return nullptr;
            }

            llvm::Type* enumLLVMType = getLLVMType(enumTy);
            if (!enumLLVMType || !enumLLVMType->isStructTy()) {
                return nullptr;
            }

            llvm::Value* enumValue = llvm::UndefValue::get(enumLLVMType);
            llvm::Value* tagVal = llvm::ConstantInt::get(
                llvm::Type::getInt32Ty(*Context),
                static_cast<uint64_t>(variant->Tag));
            enumValue = Builder->CreateInsertValue(enumValue, tagVal, 0, "enum.tag");

            llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(*Context), 0);
            llvm::Value* dataPtr = llvm::ConstantPointerNull::get(
                static_cast<llvm::PointerType*>(i8PtrTy));
            enumValue = Builder->CreateInsertValue(enumValue, dataPtr, 1, "enum.data");
            return enumValue;
        }
    }

    // Generate code for the base expression
    llvm::Value* base = generateExpr(expr->getBase());
    if (!base) {
        return nullptr;
    }

    // SysError 运行时字段访问（err.func_name / err.file / err.line）
    if (baseType->isEnum()) {
        auto* enumType = static_cast<EnumType*>(baseType);
        if (enumType->getName() == "SysError") {
            const std::string& memberName = expr->getMember();
            if (memberName == "func_name") {
                return emitStringLiteralValue("<unknown>");
            }
            if (memberName == "file") {
                return emitStringLiteralValue("<unknown>");
            }
            if (memberName == "line") {
                return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Context), 0);
            }
        }
    }

    // Handle struct member access (including generic instances)
    if (baseType->isStruct() || (genInst && genInst->getBaseType()->isStruct())) {
        StructType* structType = baseType->isStruct()
            ? static_cast<StructType*>(baseType)
            : static_cast<StructType*>(genInst->getBaseType());
        Type* structValueType = baseType->isStruct() ? baseType : static_cast<Type*>(genInst);
        const std::string& memberName = expr->getMember();

        // Find the field in the struct
        const StructType::Field* field = structType->getField(memberName);
        if (!field) {
            // Field not found
            return nullptr;
        }

        // Get the field index
        size_t fieldIndex = 0;
        const auto& fields = structType->getFields();
        for (size_t i = 0; i < fields.size(); ++i) {
            if (fields[i].Name == memberName) {
                fieldIndex = i;
                break;
            }
        }

        // Convert field type to LLVM type (substitute generics if needed)
        llvm::Type* fieldType = nullptr;
        Type* fieldSemType = field->FieldType;
        bool pushedMapping = false;
        if (genInst && structType == genInst->getBaseType()) {
            GenericSubst mapping;
            if (buildStructGenericMapping(structType, genInst, mapping)) {
                GenericSubstStack.push_back(mapping);
                pushedMapping = true;
                fieldSemType = substituteType(fieldSemType);
            }
        }
        fieldType = normalizeFirstClassType(getLLVMType(fieldSemType));
        if (pushedMapping) {
            GenericSubstStack.pop_back();
        }
        if (!fieldType) {
            return nullptr;
        }

        // Check if base is a pointer (for struct values stored in memory)
        llvm::Value* structPtr = base;
        if (!base->getType()->isPointerTy()) {
            // If base is a struct value, we need to store it to memory first
            // This can happen with struct literals or returned values
            llvm::Type* structLLVMType = getLLVMType(structValueType);
            if (!structLLVMType) {
                return nullptr;
            }

            llvm::Value* tempAlloca = Builder->CreateAlloca(structLLVMType, nullptr, "temp.struct");
            Builder->CreateStore(base, tempAlloca);
            structPtr = tempAlloca;
        }

        // Use GEP to get pointer to the field
        llvm::Value* fieldPtr = Builder->CreateStructGEP(
            getLLVMType(structValueType),
            structPtr,
            fieldIndex,
            memberName
        );

        // Load the field value
        llvm::Value* fieldVal = Builder->CreateLoad(fieldType, fieldPtr, memberName);

        // Cast to the expression's semantic type if needed (e.g., generic fields)
        if (Type* exprType = expr->getType()) {
            Type* exprSemType = exprType;
            if (genInst && structType == genInst->getBaseType()) {
                GenericSubst mapping;
                if (buildStructGenericMapping(structType, genInst, mapping)) {
                    GenericSubstStack.push_back(mapping);
                    exprSemType = substituteType(exprSemType);
                    GenericSubstStack.pop_back();
                }
            }
            llvm::Type* llvmExprType = normalizeFirstClassType(getLLVMType(exprSemType));
            if (llvmExprType && fieldVal->getType() != llvmExprType) {
                if (llvmExprType->isPointerTy()) {
                    if (fieldVal->getType()->isPointerTy()) {
                        fieldVal = Builder->CreateBitCast(fieldVal, llvmExprType, "field.cast");
                    } else if (fieldVal->getType()->isIntegerTy()) {
                        fieldVal = Builder->CreateIntToPtr(fieldVal, llvmExprType, "field.inttoptr");
                    } else {
                        fieldVal = Builder->CreateBitCast(fieldVal, llvmExprType, "field.cast");
                    }
                } else if (llvmExprType->isIntegerTy()) {
                    if (fieldVal->getType()->isPointerTy()) {
                        fieldVal = Builder->CreatePtrToInt(fieldVal, llvmExprType, "field.ptrtoint");
                    } else if (fieldVal->getType()->isIntegerTy()) {
                        fieldVal = Builder->CreateSExtOrTrunc(fieldVal, llvmExprType, "field.int.cast");
                    } else {
                        fieldVal = Builder->CreateBitCast(fieldVal, llvmExprType, "field.cast");
                    }
                } else {
                    fieldVal = Builder->CreateBitCast(fieldVal, llvmExprType, "field.cast");
                }
            }
        }

        return fieldVal;
    }

    // Handle array member access (e.g., arr.len)
    if (baseType->isArray()) {
        ArrayType* arrayType = static_cast<ArrayType*>(baseType);
        const std::string& memberName = expr->getMember();

        if (memberName == "len") {
            // Return the array length as a constant
            uint64_t arraySize = arrayType->getArraySize();
            return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*Context), arraySize);
        }

        // Other array members not supported
        return nullptr;
    }

    // Handle slice member access (e.g., slice.len, slice.ptr)
    if (baseType->isSlice()) {
        const std::string& memberName = expr->getMember();

        if (memberName == "len") {
            // Slice is { ptr, len }, extract the length (index 1)
            return Builder->CreateExtractValue(base, 1, "slice.len");
        } else if (memberName == "ptr") {
            // Extract the pointer (index 0)
            return Builder->CreateExtractValue(base, 0, "slice.ptr");
        }

        // Other slice members not supported
        return nullptr;
    }

    // Handle VarArgs member access (e.g., args.len)
    if (baseType->isVarArgs()) {
        const std::string& memberName = expr->getMember();

        if (memberName == "len") {
            return Builder->CreateExtractValue(base, 0, "varargs.len");
        }

        return nullptr;
    }

    // Handle tuple member access (e.g., tuple.0, tuple.1)
    if (baseType->isTuple()) {
        TupleType* tupleType = static_cast<TupleType*>(baseType);
        const std::string& memberName = expr->getMember();

        // Check if member name is a number
        if (!memberName.empty() && std::isdigit(memberName[0])) {
            try {
                size_t index = std::stoull(memberName);
                const auto& elements = tupleType->getElements();

                if (index >= elements.size()) {
                    // Index out of bounds
                    return nullptr;
                }

                // Get the element type
                Type* elementType = elements[index];
                llvm::Type* llvmElementType = normalizeFirstClassType(getLLVMType(elementType));
                if (!llvmElementType) {
                    return nullptr;
                }

                // Check if base is a pointer
                llvm::Value* tuplePtr = base;
                if (!base->getType()->isPointerTy()) {
                    // Store tuple to memory first
                    llvm::Type* tupleLLVMType = getLLVMType(tupleType);
                    if (!tupleLLVMType) {
                        return nullptr;
                    }

                    llvm::Value* tempAlloca = Builder->CreateAlloca(tupleLLVMType, nullptr, "temp.tuple");
                    Builder->CreateStore(base, tempAlloca);
                    tuplePtr = tempAlloca;
                }

                // Use GEP to get pointer to the element
                llvm::Value* elementPtr = Builder->CreateStructGEP(
                    getLLVMType(tupleType),
                    tuplePtr,
                    index,
                    "tuple." + memberName
                );

                // Load the element value
                return Builder->CreateLoad(llvmElementType, elementPtr, "tuple." + memberName);
            } catch (...) {
                // Invalid number format
                return nullptr;
            }
        }

        // Non-numeric member name for tuple
        return nullptr;
    }

    // Handle string member access (e.g., str.len, str.ptr)
    if (baseType->isString()) {
        const std::string& memberName = expr->getMember();

        if (memberName == "len") {
            // String is { ptr, len }, extract the length (index 1)
            llvm::Value* lenVal = Builder->CreateExtractValue(base, 1, "str.len");
            Type* semType = expr->getType();
            if (semType && semType->isFunction()) {
                semType = static_cast<FunctionType*>(semType)->getReturnType();
            }
            if (semType) {
                llvm::Type* desired = getLLVMType(semType);
                if (desired && desired->isIntegerTy() && desired != lenVal->getType()) {
                    lenVal = Builder->CreateSExtOrTrunc(lenVal, desired, "str.len.cast");
                }
            }
            return lenVal;
        } else if (memberName == "ptr") {
            // Extract the pointer (index 0)
            return Builder->CreateExtractValue(base, 0, "str.ptr");
        }

        // Other string members not supported
        return nullptr;
    }

    return nullptr;
}

// ============================================================================
// Binary expressions
// ============================================================================

llvm::Value* CodeGen::generateBinaryExpr(BinaryExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    BinaryExpr::Op op = expr->getOp();
    auto getLoweringOperandType = [&]() -> Type* {
        Type* type = expr->getLHS() ? expr->getLHS()->getType() : nullptr;
        if (!type) {
            return nullptr;
        }
        if (!GenericSubstStack.empty()) {
            type = substituteType(type);
        }
        if (type->isReference()) {
            type = static_cast<ReferenceType*>(type)->getPointeeType();
        }
        type = unwrapTypeAlias(type);
        if (type && type->isTypeVar()) {
            auto* typeVar = static_cast<TypeVariable*>(type);
            if (typeVar->isResolved()) {
                type = unwrapTypeAlias(typeVar->getResolvedType());
            }
        }
        return type;
    };
    auto isBuiltinArithmeticOperand = [](Type* type) -> bool {
        Type* normalized = unwrapTypeAlias(type);
        return normalized && normalized->isNumeric();
    };
    auto isBuiltinComparisonOperand = [](Type* type) -> bool {
        Type* normalized = unwrapTypeAlias(type);
        return normalized && (normalized->isInteger() || normalized->isFloat() ||
                              normalized->isBool() || normalized->isChar() ||
                              normalized->isString() || normalized->isPointer());
    };

    bool preferBuiltinLowering = false;
    if (Type* loweringType = getLoweringOperandType()) {
        switch (op) {
            case BinaryExpr::Op::Add:
            case BinaryExpr::Op::Sub:
            case BinaryExpr::Op::Mul:
            case BinaryExpr::Op::Div:
            case BinaryExpr::Op::Mod:
                preferBuiltinLowering = isBuiltinArithmeticOperand(loweringType);
                break;
            case BinaryExpr::Op::Eq:
            case BinaryExpr::Op::Ne:
            case BinaryExpr::Op::Lt:
            case BinaryExpr::Op::Le:
            case BinaryExpr::Op::Gt:
            case BinaryExpr::Op::Ge:
                preferBuiltinLowering = isBuiltinComparisonOperand(loweringType);
                break;
            default:
                break;
        }
    }

    if (FuncDecl* resolvedMethod = expr->getResolvedOpMethod();
        resolvedMethod && !preferBuiltinLowering) {
        MemberExpr opMethodExpr(expr->getRange(), expr->getLHS(), resolvedMethod->getName());
        opMethodExpr.setResolvedDecl(resolvedMethod);
        CallExpr opCallExpr(
            expr->getRange(),
            &opMethodExpr,
            std::vector<Expr*>{expr->getRHS()}
        );
        opCallExpr.setType(expr->getType());
        return generateCallExpr(&opCallExpr);
    }

    // Handle short-circuit logical operators (&&, ||)
    if (op == BinaryExpr::Op::And || op == BinaryExpr::Op::Or) {
        return generateLogicalBinaryExpr(expr);
    }

    // Generate code for both operands
    llvm::Value* lhs = generateExpr(expr->getLHS());
    llvm::Value* rhs = generateExpr(expr->getRHS());

    if (!lhs || !rhs) {
        return nullptr;
    }

    auto materializeAutoDeref = [&](llvm::Value* value, Type* exprType) -> llvm::Value* {
        if (!value || !exprType) {
            return value;
        }
        if (exprType->isReference() && value->getType()->isPointerTy()) {
            Type* pointeeType = static_cast<ReferenceType*>(exprType)->getPointeeType();
            llvm::Type* llvmPointeeType = getLLVMType(pointeeType);
            if (!llvmPointeeType) {
                return nullptr;
            }
            return Builder->CreateLoad(llvmPointeeType, value, "autoderef");
        }
        return value;
    };

    Type* lhsExprType = expr->getLHS()->getType();
    Type* rhsExprType = expr->getRHS()->getType();
    lhs = materializeAutoDeref(lhs, lhsExprType);
    rhs = materializeAutoDeref(rhs, rhsExprType);
    if (!lhs || !rhs) {
        return nullptr;
    }

    // Get the result type from semantic analysis
    Type* resultType = expr->getType();
    if (!resultType) {
        return nullptr;
    }

    // Determine if operands are integer or floating-point
    // For comparison and logical operators, the result type is bool,
    // but we need to check the operand types
    Type* operandType = lhsExprType;
    if (!operandType) {
        return nullptr;
    }
    if (!GenericSubstStack.empty()) {
        operandType = substituteType(operandType);
    }
    if (operandType->isReference()) {
        operandType = static_cast<ReferenceType*>(operandType)->getPointeeType();
    }
    operandType = unwrapTypeAlias(operandType);
    if (operandType && operandType->isTypeVar()) {
        auto* typeVar = static_cast<TypeVariable*>(operandType);
        if (typeVar->isResolved()) {
            operandType = unwrapTypeAlias(typeVar->getResolvedType());
        }
    }

    bool isInt = operandType->isInteger() || operandType->isBool();
    bool isFloat = operandType->isFloat();
    bool isSigned = false;

    if (isInt && operandType->isInteger()) {
        IntegerType* intType = static_cast<IntegerType*>(operandType);
        isSigned = intType->isSigned();
    }

    auto isStringStructValue = [](llvm::Value* value) -> bool {
        if (!value) {
            return false;
        }
        auto* structTy = llvm::dyn_cast<llvm::StructType>(value->getType());
        if (!structTy || structTy->getNumElements() != 2) {
            return false;
        }
        llvm::Type* first = structTy->getElementType(0);
        llvm::Type* second = structTy->getElementType(1);
        if (!first || !second || !first->isPointerTy() || !second->isIntegerTy()) {
            return false;
        }
        return true;
    };

    bool useStringEquality =
        (operandType && unwrapTypeAlias(operandType)->isString()) ||
        (isStringStructValue(lhs) && isStringStructValue(rhs) && lhs->getType() == rhs->getType());

    // Generate appropriate instruction based on operator type
    switch (op) {
        // Arithmetic operators
        case BinaryExpr::Op::Add:
            return isFloat ? Builder->CreateFAdd(lhs, rhs, "fadd")
                          : Builder->CreateAdd(lhs, rhs, "add");

        case BinaryExpr::Op::Sub:
            return isFloat ? Builder->CreateFSub(lhs, rhs, "fsub")
                          : Builder->CreateSub(lhs, rhs, "sub");

        case BinaryExpr::Op::Mul:
            return isFloat ? Builder->CreateFMul(lhs, rhs, "fmul")
                          : Builder->CreateMul(lhs, rhs, "mul");

        case BinaryExpr::Op::Div:
            return isFloat ? Builder->CreateFDiv(lhs, rhs, "fdiv")
                          : (isSigned ? Builder->CreateSDiv(lhs, rhs, "sdiv")
                                     : Builder->CreateUDiv(lhs, rhs, "udiv"));

        case BinaryExpr::Op::Mod:
            return isFloat ? Builder->CreateFRem(lhs, rhs, "frem")
                          : (isSigned ? Builder->CreateSRem(lhs, rhs, "srem")
                                     : Builder->CreateURem(lhs, rhs, "urem"));

        // Bitwise operators
        case BinaryExpr::Op::BitAnd:
            return Builder->CreateAnd(lhs, rhs, "and");

        case BinaryExpr::Op::BitOr:
            return Builder->CreateOr(lhs, rhs, "or");

        case BinaryExpr::Op::BitXor:
            return Builder->CreateXor(lhs, rhs, "xor");

        case BinaryExpr::Op::Shl:
            return Builder->CreateShl(lhs, rhs, "shl");

        case BinaryExpr::Op::Shr:
            return isSigned ? Builder->CreateAShr(lhs, rhs, "ashr")
                           : Builder->CreateLShr(lhs, rhs, "lshr");

        // Comparison operators
        case BinaryExpr::Op::Eq:
            if (useStringEquality) {
                return emitStringEquality(lhs, rhs);
            }
            if (!lhs->getType()->isIntOrIntVectorTy() &&
                !lhs->getType()->isPtrOrPtrVectorTy() &&
                !lhs->getType()->isFPOrFPVectorTy()) {
                return nullptr;
            }
            return isFloat ? Builder->CreateFCmpOEQ(lhs, rhs, "fcmp.eq")
                          : Builder->CreateICmpEQ(lhs, rhs, "icmp.eq");

        case BinaryExpr::Op::Ne:
            if (useStringEquality) {
                llvm::Value* eqRes = emitStringEquality(lhs, rhs);
                return eqRes ? Builder->CreateNot(eqRes, "str.ne") : nullptr;
            }
            if (!lhs->getType()->isIntOrIntVectorTy() &&
                !lhs->getType()->isPtrOrPtrVectorTy() &&
                !lhs->getType()->isFPOrFPVectorTy()) {
                return nullptr;
            }
            return isFloat ? Builder->CreateFCmpONE(lhs, rhs, "fcmp.ne")
                          : Builder->CreateICmpNE(lhs, rhs, "icmp.ne");

        case BinaryExpr::Op::Lt:
            return isFloat ? Builder->CreateFCmpOLT(lhs, rhs, "fcmp.lt")
                          : (isSigned ? Builder->CreateICmpSLT(lhs, rhs, "icmp.slt")
                                     : Builder->CreateICmpULT(lhs, rhs, "icmp.ult"));

        case BinaryExpr::Op::Le:
            return isFloat ? Builder->CreateFCmpOLE(lhs, rhs, "fcmp.le")
                          : (isSigned ? Builder->CreateICmpSLE(lhs, rhs, "icmp.sle")
                                     : Builder->CreateICmpULE(lhs, rhs, "icmp.ule"));

        case BinaryExpr::Op::Gt:
            return isFloat ? Builder->CreateFCmpOGT(lhs, rhs, "fcmp.gt")
                          : (isSigned ? Builder->CreateICmpSGT(lhs, rhs, "icmp.sgt")
                                     : Builder->CreateICmpUGT(lhs, rhs, "icmp.ugt"));

        case BinaryExpr::Op::Ge:
            return isFloat ? Builder->CreateFCmpOGE(lhs, rhs, "fcmp.ge")
                          : (isSigned ? Builder->CreateICmpSGE(lhs, rhs, "icmp.sge")
                                     : Builder->CreateICmpUGE(lhs, rhs, "icmp.uge"));

        // Logical operators (handled above)
        case BinaryExpr::Op::And:
        case BinaryExpr::Op::Or:
            // Should be handled by generateLogicalBinaryExpr
            return nullptr;

        // Range operators and others
        case BinaryExpr::Op::Range:
        case BinaryExpr::Op::RangeInclusive:
        case BinaryExpr::Op::OrElse: {
            if (op == BinaryExpr::Op::OrElse) {
                // Optional default value: lhs ? lhs.value : rhs
                llvm::Value* lhsVal = generateExpr(expr->getLHS());
                if (!lhsVal) {
                    return nullptr;
                }

                llvm::Value* hasValue = Builder->CreateExtractValue(lhsVal, 0, "opt.has");
                llvm::Value* innerValue = Builder->CreateExtractValue(lhsVal, 1, "opt.value");

                llvm::Function* currentFunc = Builder->GetInsertBlock()->getParent();
                llvm::BasicBlock* hasBB = llvm::BasicBlock::Create(*Context, "orelse.has", currentFunc);
                llvm::BasicBlock* rhsBB = llvm::BasicBlock::Create(*Context, "orelse.rhs", currentFunc);
                llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*Context, "orelse.merge", currentFunc);

                Builder->CreateCondBr(hasValue, hasBB, rhsBB);

                // Has value
                Builder->SetInsertPoint(hasBB);
                llvm::Value* lhsResult = innerValue;
                Builder->CreateBr(mergeBB);
                llvm::BasicBlock* hasEndBB = Builder->GetInsertBlock();

                // Else (rhs)
                Builder->SetInsertPoint(rhsBB);
                llvm::Value* rhsValue = generateExpr(expr->getRHS());
                if (!rhsValue) {
                    return nullptr;
                }
                Builder->CreateBr(mergeBB);
                llvm::BasicBlock* rhsEndBB = Builder->GetInsertBlock();

                // Merge
                Builder->SetInsertPoint(mergeBB);
                llvm::Type* resultType = getLLVMType(expr->getType());
                if (!resultType) {
                    return nullptr;
                }
                llvm::PHINode* phi = Builder->CreatePHI(resultType, 2, "orelse.result");
                phi->addIncoming(lhsResult, hasEndBB);
                phi->addIncoming(rhsValue, rhsEndBB);
                return phi;
            }

            // Range operators: construct a Range struct
            Type* rangeType = expr->getType();
            if (!rangeType || !rangeType->isRange()) {
                return nullptr;
            }

            llvm::Value* startValue = generateExpr(expr->getLHS());
            llvm::Value* endValue = generateExpr(expr->getRHS());
            if (!startValue || !endValue) {
                return nullptr;
            }

            llvm::Type* llvmRangeType = getLLVMType(rangeType);
            if (!llvmRangeType) {
                return nullptr;
            }

            llvm::Value* rangeStruct = llvm::UndefValue::get(llvmRangeType);
            rangeStruct = Builder->CreateInsertValue(rangeStruct, startValue, 0, "range.start");
            rangeStruct = Builder->CreateInsertValue(rangeStruct, endValue, 1, "range.end");
            llvm::Value* inclusiveValue = llvm::ConstantInt::get(
                llvm::Type::getInt1Ty(*Context),
                op == BinaryExpr::Op::RangeInclusive ? 1 : 0
            );
            rangeStruct = Builder->CreateInsertValue(rangeStruct, inclusiveValue, 2, "range.inclusive");
            return rangeStruct;
        }

        default:
            return nullptr;
    }
}

llvm::Value* CodeGen::emitStringEquality(llvm::Value* lhsVal, llvm::Value* rhsVal) {
    if (!lhsVal || !rhsVal) {
        return nullptr;
    }
    if (!lhsVal->getType()->isStructTy() || !rhsVal->getType()->isStructTy()) {
        return nullptr;
    }

    auto* lhsStruct = llvm::dyn_cast<llvm::StructType>(lhsVal->getType());
    auto* rhsStruct = llvm::dyn_cast<llvm::StructType>(rhsVal->getType());
    if (!lhsStruct || !rhsStruct || lhsStruct->getNumElements() != 2 || rhsStruct->getNumElements() != 2) {
        return nullptr;
    }
    if (!lhsStruct->getElementType(0)->isPointerTy() || !rhsStruct->getElementType(0)->isPointerTy()) {
        return nullptr;
    }
    if (!lhsStruct->getElementType(1)->isIntegerTy() || !rhsStruct->getElementType(1)->isIntegerTy()) {
        return nullptr;
    }

    llvm::Value* lhsPtr = Builder->CreateExtractValue(lhsVal, 0, "str.lhs.ptr");
    llvm::Value* lhsLen = Builder->CreateExtractValue(lhsVal, 1, "str.lhs.len");
    llvm::Value* rhsPtr = Builder->CreateExtractValue(rhsVal, 0, "str.rhs.ptr");
    llvm::Value* rhsLen = Builder->CreateExtractValue(rhsVal, 1, "str.rhs.len");

    llvm::Type* i64Ty = llvm::Type::getInt64Ty(*Context);
    if (!lhsLen->getType()->isIntegerTy(64)) {
        lhsLen = Builder->CreateSExtOrTrunc(lhsLen, i64Ty, "str.lhs.len.i64");
    }
    if (!rhsLen->getType()->isIntegerTy(64)) {
        rhsLen = Builder->CreateSExtOrTrunc(rhsLen, i64Ty, "str.rhs.len.i64");
    }

    llvm::Value* sameLen = Builder->CreateICmpEQ(lhsLen, rhsLen, "str.len.eq");

    llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(*Context), 0);
    if (lhsPtr->getType() != i8PtrTy) {
        lhsPtr = Builder->CreateBitCast(lhsPtr, i8PtrTy, "str.lhs.ptr.cast");
    }
    if (rhsPtr->getType() != i8PtrTy) {
        rhsPtr = Builder->CreateBitCast(rhsPtr, i8PtrTy, "str.rhs.ptr.cast");
    }

    llvm::FunctionCallee memcmpFn = Module->getOrInsertFunction(
        "memcmp",
        llvm::FunctionType::get(llvm::Type::getInt32Ty(*Context), {i8PtrTy, i8PtrTy, i64Ty}, false));
    llvm::Value* cmpVal = Builder->CreateCall(memcmpFn, {lhsPtr, rhsPtr, lhsLen}, "str.memcmp");
    llvm::Value* cmpEqZero = Builder->CreateICmpEQ(
        cmpVal,
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Context), 0),
        "str.memcmp.eq");
    return Builder->CreateAnd(sameLen, cmpEqZero, "str.eq");
}

llvm::Value* CodeGen::generateLogicalBinaryExpr(BinaryExpr* expr) {
    // Logical operators require short-circuit evaluation
    // For && : if LHS is false, don't evaluate RHS
    // For || : if LHS is true, don't evaluate RHS

    llvm::Function* currentFunc = Builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* rhsBB = llvm::BasicBlock::Create(*Context, "logical.rhs", currentFunc);
    llvm::BasicBlock* endBB = llvm::BasicBlock::Create(*Context, "logical.end", currentFunc);

    // Generate LHS
    llvm::Value* lhs = generateExpr(expr->getLHS());
    if (!lhs) {
        return nullptr;
    }
    if (Type* lhsType = expr->getLHS()->getType()) {
        if (lhsType->isReference() && lhs->getType()->isPointerTy()) {
            Type* pointee = static_cast<ReferenceType*>(lhsType)->getPointeeType();
            llvm::Type* llvmPointee = getLLVMType(pointee);
            if (!llvmPointee) {
                return nullptr;
            }
            lhs = Builder->CreateLoad(llvmPointee, lhs, "logical.lhs.autoderef");
        }
    }

    // Create the conditional branch
    BinaryExpr::Op op = expr->getOp();
    if (op == BinaryExpr::Op::And) {
        // For &&: jump to RHS if LHS is true, otherwise jump to end
        Builder->CreateCondBr(lhs, rhsBB, endBB);
    } else {
        // For ||: jump to RHS if LHS is false, otherwise jump to end
        Builder->CreateCondBr(lhs, endBB, rhsBB);
    }

    // Remember the LHS block for PHI
    llvm::BasicBlock* lhsBB = Builder->GetInsertBlock();

    // Generate RHS
    Builder->SetInsertPoint(rhsBB);
    llvm::Value* rhs = generateExpr(expr->getRHS());
    if (!rhs) {
        return nullptr;
    }
    if (Type* rhsType = expr->getRHS()->getType()) {
        if (rhsType->isReference() && rhs->getType()->isPointerTy()) {
            Type* pointee = static_cast<ReferenceType*>(rhsType)->getPointeeType();
            llvm::Type* llvmPointee = getLLVMType(pointee);
            if (!llvmPointee) {
                return nullptr;
            }
            rhs = Builder->CreateLoad(llvmPointee, rhs, "logical.rhs.autoderef");
        }
    }

    // Branch to end block
    Builder->CreateBr(endBB);
    llvm::BasicBlock* rhsEndBB = Builder->GetInsertBlock();

    // Generate PHI node in end block
    Builder->SetInsertPoint(endBB);
    llvm::PHINode* phi = Builder->CreatePHI(llvm::Type::getInt1Ty(*Context), 2, "logical.result");

    if (op == BinaryExpr::Op::And) {
        // For &&: if LHS is false, result is false; otherwise use RHS
        phi->addIncoming(llvm::ConstantInt::getFalse(*Context), lhsBB);
        phi->addIncoming(rhs, rhsEndBB);
    } else {
        // For ||: if LHS is true, result is true; otherwise use RHS
        phi->addIncoming(llvm::ConstantInt::getTrue(*Context), lhsBB);
        phi->addIncoming(rhs, rhsEndBB);
    }

    return phi;
}

// ============================================================================
// Unary expressions
// ============================================================================

llvm::Value* CodeGen::generateUnaryExpr(UnaryExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    UnaryExpr::Op op = expr->getOp();
    auto getLoweringOperandType = [&]() -> Type* {
        Type* type = expr->getOperand() ? expr->getOperand()->getType() : nullptr;
        if (!type) {
            return nullptr;
        }
        if (!GenericSubstStack.empty()) {
            type = substituteType(type);
        }
        if (type->isReference()) {
            type = static_cast<ReferenceType*>(type)->getPointeeType();
        }
        type = unwrapTypeAlias(type);
        if (type && type->isTypeVar()) {
            auto* typeVar = static_cast<TypeVariable*>(type);
            if (typeVar->isResolved()) {
                type = unwrapTypeAlias(typeVar->getResolvedType());
            }
        }
        return type;
    };

    bool preferBuiltinLowering = false;
    if (Type* loweringType = getLoweringOperandType()) {
        switch (op) {
            case UnaryExpr::Op::Neg:
                preferBuiltinLowering = loweringType->isNumeric();
                break;
            case UnaryExpr::Op::Not:
                preferBuiltinLowering = loweringType->isBool();
                break;
            case UnaryExpr::Op::BitNot:
                preferBuiltinLowering = loweringType->isInteger();
                break;
            default:
                break;
        }
    }

    if (FuncDecl* resolvedMethod = expr->getResolvedOpMethod();
        resolvedMethod && !preferBuiltinLowering) {
        MemberExpr opMethodExpr(expr->getRange(), expr->getOperand(), resolvedMethod->getName());
        opMethodExpr.setResolvedDecl(resolvedMethod);
        CallExpr opCallExpr(expr->getRange(), &opMethodExpr, std::vector<Expr*>{});
        opCallExpr.setType(expr->getType());
        return generateCallExpr(&opCallExpr);
    }

    // For reference operators, we need to return the address, not the value
    if (op == UnaryExpr::Op::Ref || op == UnaryExpr::Op::RefMut) {
        // Sema may normalize `&arr[1..3]` to a slice value (not reference-to-slice).
        // In that case, emit the operand value directly instead of taking lvalue address.
        if (Type* resultType = expr->getType()) {
            if (!resultType->isReference()) {
                return generateExpr(expr->getOperand());
            }
        }

        // Get the address of the operand (lvalue)
        llvm::Value* addr = generateLValueAddress(expr->getOperand());
        if (!addr) {
            return nullptr;
        }

        // Return the address directly (references are pointers in LLVM)
        return addr;
    }

    // Generate code for the operand
    llvm::Value* operand = generateExpr(expr->getOperand());
    if (!operand) {
        return nullptr;
    }

    // Get the type from semantic analysis
    Type* operandType = expr->getOperand()->getType();
    if (!operandType) {
        return nullptr;
    }
    if (operandType->isReference() &&
        op != UnaryExpr::Op::Ref &&
        op != UnaryExpr::Op::RefMut &&
        op != UnaryExpr::Op::Deref) {
        Type* pointeeType = static_cast<ReferenceType*>(operandType)->getPointeeType();
        llvm::Type* llvmPointeeType = getLLVMType(pointeeType);
        if (!llvmPointeeType) {
            return nullptr;
        }
        if (!operand->getType()->isPointerTy()) {
            return nullptr;
        }
        operand = Builder->CreateLoad(llvmPointeeType, operand, "unary.autoderef");
        operandType = pointeeType;
    }

    bool isFloat = operandType->isFloat();

    // Generate appropriate instruction based on operator
    switch (op) {
        case UnaryExpr::Op::Neg:
            // Negate: -x
            return isFloat ? Builder->CreateFNeg(operand, "fneg")
                          : Builder->CreateNeg(operand, "neg");

        case UnaryExpr::Op::Not:
            // Logical NOT: !x
            // In LLVM, this is XOR with true (for i1 type)
            return Builder->CreateNot(operand, "not");

        case UnaryExpr::Op::BitNot:
            // Bitwise NOT: ~x
            return Builder->CreateNot(operand, "bitnot");

        case UnaryExpr::Op::Deref: {
            // Dereference: *ptr
            // The operand should be a pointer type
            if (!operand->getType()->isPointerTy()) {
                return nullptr;
            }

            // Get the pointee type
            Type* resultType = expr->getType();
            if (!resultType) {
                return nullptr;
            }

            llvm::Type* llvmResultType = getLLVMType(resultType);
            if (!llvmResultType) {
                return nullptr;
            }

            // Load the value from the pointer
            return Builder->CreateLoad(llvmResultType, operand, "deref");
        }

        case UnaryExpr::Op::Ref:
        case UnaryExpr::Op::RefMut:
            // Reference operators handled above
            return nullptr;

        default:
            return nullptr;
    }
}

llvm::Value* CodeGen::generateCastExpr(CastExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    llvm::Value* value = generateExpr(expr->getExpr());
    if (!value) {
        return nullptr;
    }

    Type* srcType = expr->getExpr()->getType();
    Type* dstType = expr->getType();
    if (!srcType || !dstType) {
        return nullptr;
    }

    // References are represented as pointers in LLVM.
    if (srcType->isReference()) {
        srcType = static_cast<ReferenceType*>(srcType)->getPointeeType();
    }

    if (dstType->isReference()) {
        dstType = Ctx.getPointerType(static_cast<ReferenceType*>(dstType)->getPointeeType(),
                                     static_cast<ReferenceType*>(dstType)->isMutable());
    }

    llvm::Type* llvmDstType = getLLVMType(dstType);
    if (!llvmDstType) {
        return nullptr;
    }

    if (value->getType() == llvmDstType) {
        return value;
    }

    auto isSignedIntegerType = [](Type* type) -> bool {
        if (!type || !type->isInteger()) {
            return false;
        }
        return static_cast<IntegerType*>(type)->isSigned();
    };

    if (value->getType()->isIntegerTy() && llvmDstType->isIntegerTy()) {
        return Builder->CreateSExtOrTrunc(value, llvmDstType, "cast.int");
    }

    if (value->getType()->isFloatingPointTy() && llvmDstType->isFloatingPointTy()) {
        unsigned srcBits = value->getType()->getPrimitiveSizeInBits();
        unsigned dstBits = llvmDstType->getPrimitiveSizeInBits();
        if (srcBits < dstBits) {
            return Builder->CreateFPExt(value, llvmDstType, "cast.fpext");
        }
        return Builder->CreateFPTrunc(value, llvmDstType, "cast.fptrunc");
    }

    if (value->getType()->isIntegerTy() && llvmDstType->isFloatingPointTy()) {
        if (isSignedIntegerType(srcType)) {
            return Builder->CreateSIToFP(value, llvmDstType, "cast.sitofp");
        }
        return Builder->CreateUIToFP(value, llvmDstType, "cast.uitofp");
    }

    if (value->getType()->isFloatingPointTy() && llvmDstType->isIntegerTy()) {
        if (isSignedIntegerType(dstType)) {
            return Builder->CreateFPToSI(value, llvmDstType, "cast.fptosi");
        }
        return Builder->CreateFPToUI(value, llvmDstType, "cast.fptoui");
    }

    if (value->getType()->isPointerTy() && llvmDstType->isPointerTy()) {
        return Builder->CreateBitCast(value, llvmDstType, "cast.ptr");
    }

    if (value->getType()->isIntegerTy() && llvmDstType->isPointerTy()) {
        return Builder->CreateIntToPtr(value, llvmDstType, "cast.inttoptr");
    }

    if (value->getType()->isPointerTy() && llvmDstType->isIntegerTy()) {
        return Builder->CreatePtrToInt(value, llvmDstType, "cast.ptrtoint");
    }

    return Builder->CreateBitCast(value, llvmDstType, "cast");
}

// ============================================================================
// Assignment expressions
// ============================================================================

llvm::Value* CodeGen::generateAssignExpr(AssignExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    if (auto* identTarget = dynamic_cast<IdentifierExpr*>(expr->getTarget())) {
        if (identTarget->getName() == "_") {
            // Discard assignment: just evaluate value for side effects.
            return generateExpr(expr->getValue());
        }
    }

    // Get the target lvalue address
    llvm::Value* targetAddr = generateLValueAddress(expr->getTarget());
    if (!targetAddr) {
        return nullptr;
    }

    AssignExpr::Op op = expr->getOp();
    const Decl* targetDecl = nullptr;
    if (auto* identTarget = dynamic_cast<IdentifierExpr*>(expr->getTarget())) {
        targetDecl = identTarget->getResolvedDecl();
    }

    // Generate the value to assign
    llvm::Value* value = generateExpr(expr->getValue());
    if (!value) {
        return nullptr;
    }

    Type* targetType = expr->getTarget()->getType();
    if (!targetType) {
        return nullptr;
    }
    if (targetType->isReference()) {
        targetType = static_cast<ReferenceType*>(targetType)->getPointeeType();
    }

    if (Type* valueType = expr->getValue()->getType()) {
        if (valueType->isReference() &&
            !targetType->isReference() &&
            !targetType->isPointer() &&
            value->getType()->isPointerTy()) {
            llvm::Type* llvmTargetType = getLLVMType(targetType);
            if (!llvmTargetType) {
                return nullptr;
            }
            value = Builder->CreateLoad(llvmTargetType, value, "assign.autoderef");
        }
    }

    auto castValueIfNeeded = [&](llvm::Value* source, llvm::Type* targetLLVMType) -> llvm::Value* {
        if (!source || !targetLLVMType) {
            return nullptr;
        }
        if (source->getType() == targetLLVMType) {
            return source;
        }
        llvm::Type* sourceLLVMType = source->getType();
        if (sourceLLVMType->isIntegerTy() && targetLLVMType->isIntegerTy()) {
            return Builder->CreateSExtOrTrunc(source, targetLLVMType, "assign.int.cast");
        }
        if (sourceLLVMType->isFloatingPointTy() && targetLLVMType->isFloatingPointTy()) {
            unsigned srcBits = sourceLLVMType->getPrimitiveSizeInBits();
            unsigned dstBits = targetLLVMType->getPrimitiveSizeInBits();
            if (srcBits < dstBits) {
                return Builder->CreateFPExt(source, targetLLVMType, "assign.fp.ext");
            }
            return Builder->CreateFPTrunc(source, targetLLVMType, "assign.fp.trunc");
        }
        if (sourceLLVMType->isPointerTy() && targetLLVMType->isPointerTy()) {
            return Builder->CreateBitCast(source, targetLLVMType, "assign.ptr.cast");
        }
        if (sourceLLVMType->isPointerTy() && targetLLVMType->isIntegerTy()) {
            return Builder->CreatePtrToInt(source, targetLLVMType, "assign.ptrtoint");
        }
        if (sourceLLVMType->isIntegerTy() && targetLLVMType->isPointerTy()) {
            return Builder->CreateIntToPtr(source, targetLLVMType, "assign.inttoptr");
        }
        return nullptr;
    };

    // Handle simple assignment (=)
    if (op == AssignExpr::Op::Assign) {
        if (targetDecl) {
            emitDropForDecl(targetDecl);
        } else {
            (void)emitDropForAddress(targetAddr, targetType);
        }
        llvm::Type* llvmTargetType = getLLVMType(targetType);
        if (!llvmTargetType) {
            return nullptr;
        }
        value = castValueIfNeeded(value, llvmTargetType);
        if (!value) {
            return nullptr;
        }
        Builder->CreateStore(value, targetAddr);
        if (targetDecl) {
            setDropFlag(targetDecl, true);
        }
        return value;  // Assignment expression evaluates to the assigned value
    }

    // Handle compound assignments (+=, -=, etc.)
    // First, load the current value
    llvm::Type* llvmTargetType = getLLVMType(targetType);
    if (!llvmTargetType) {
        return nullptr;
    }

    llvm::Value* currentValue = Builder->CreateLoad(llvmTargetType, targetAddr, "current");

    // Determine if we're dealing with integer or float
    bool isFloat = targetType->isFloat();
    bool isSigned = false;

    if (targetType->isInteger()) {
        IntegerType* intType = static_cast<IntegerType*>(targetType);
        isSigned = intType->isSigned();
    }

    // Perform the compound operation
    llvm::Value* newValue = nullptr;

    switch (op) {
        case AssignExpr::Op::AddAssign:
            newValue = isFloat ? Builder->CreateFAdd(currentValue, value, "add.assign")
                              : Builder->CreateAdd(currentValue, value, "add.assign");
            break;

        case AssignExpr::Op::SubAssign:
            newValue = isFloat ? Builder->CreateFSub(currentValue, value, "sub.assign")
                              : Builder->CreateSub(currentValue, value, "sub.assign");
            break;

        case AssignExpr::Op::MulAssign:
            newValue = isFloat ? Builder->CreateFMul(currentValue, value, "mul.assign")
                              : Builder->CreateMul(currentValue, value, "mul.assign");
            break;

        case AssignExpr::Op::DivAssign:
            newValue = isFloat ? Builder->CreateFDiv(currentValue, value, "div.assign")
                              : (isSigned ? Builder->CreateSDiv(currentValue, value, "sdiv.assign")
                                         : Builder->CreateUDiv(currentValue, value, "udiv.assign"));
            break;

        case AssignExpr::Op::ModAssign:
            newValue = isFloat ? Builder->CreateFRem(currentValue, value, "rem.assign")
                              : (isSigned ? Builder->CreateSRem(currentValue, value, "srem.assign")
                                         : Builder->CreateURem(currentValue, value, "urem.assign"));
            break;

        case AssignExpr::Op::BitAndAssign:
            newValue = Builder->CreateAnd(currentValue, value, "and.assign");
            break;

        case AssignExpr::Op::BitOrAssign:
            newValue = Builder->CreateOr(currentValue, value, "or.assign");
            break;

        case AssignExpr::Op::BitXorAssign:
            newValue = Builder->CreateXor(currentValue, value, "xor.assign");
            break;

        case AssignExpr::Op::ShlAssign:
            newValue = Builder->CreateShl(currentValue, value, "shl.assign");
            break;

        case AssignExpr::Op::ShrAssign:
            newValue = isSigned ? Builder->CreateAShr(currentValue, value, "ashr.assign")
                               : Builder->CreateLShr(currentValue, value, "lshr.assign");
            break;

        case AssignExpr::Op::Assign:
            // Already handled above
            return nullptr;

        default:
            return nullptr;
    }

    if (!newValue) {
        return nullptr;
    }

    // Store the new value
    Builder->CreateStore(newValue, targetAddr);
    if (targetDecl) {
        setDropFlag(targetDecl, true);
    }

    return newValue;  // Compound assignment evaluates to the new value
}

// ============================================================================
// Helper methods for lvalue handling
// ============================================================================

llvm::Value* CodeGen::generateLValueAddress(Expr* expr) {
    if (!expr) {
        return nullptr;
    }

    // Check if the expression is an lvalue
    if (!expr->isLValue()) {
        return nullptr;
    }

    switch (expr->getKind()) {
        case ASTNode::Kind::IdentifierExpr: {
            // Look up the variable in the value map
            IdentifierExpr* identExpr = static_cast<IdentifierExpr*>(expr);

            // Get the resolved declaration from Sema
            Decl* decl = identExpr->getResolvedDecl();
            if (!decl) {
                return nullptr;
            }

            // Look up the value in the value map
            auto it = ValueMap.find(decl);
            if (it == ValueMap.end()) {
                return nullptr;
            }

            llvm::Value* value = it->second;

            // Value should be an alloca or global variable (pointer)
            if (!value->getType()->isPointerTy()) {
                return nullptr;
            }

            // 引用绑定作为左值时，赋值目标是被引用对象而非绑定本身。
            Type* identType = identExpr->getType();
            if (identType && identType->isReference()) {
                llvm::Type* refLLVMType = getLLVMType(identType);
                if (!refLLVMType) {
                    return nullptr;
                }
                return Builder->CreateLoad(refLLVMType, value, identExpr->getName() + ".ref.addr");
            }

            return value;
        }

        case ASTNode::Kind::MemberExpr: {
            // Generate address of struct field
            MemberExpr* memberExpr = static_cast<MemberExpr*>(expr);

            // Prefer lvalue base address so assignments like `obj.field = ...`
            // write back to the original object instead of a temporary copy.
            llvm::Value* base = nullptr;
            if (memberExpr->getBase() && memberExpr->getBase()->isLValue()) {
                base = generateLValueAddress(memberExpr->getBase());
            }
            if (!base) {
                base = generateExpr(memberExpr->getBase());
                if (!base) {
                    return nullptr;
                }
            }

            Type* baseType = memberExpr->getBase()->getType();
            if (!baseType) {
                return nullptr;
            }
            if (baseType->isReference()) {
                baseType = static_cast<ReferenceType*>(baseType)->getPointeeType();
            }
            if (baseType->isPointer()) {
                baseType = static_cast<PointerType*>(baseType)->getPointeeType();
            }

            GenericInstanceType* genInst = nullptr;
            if (baseType->isGenericInstance()) {
                genInst = static_cast<GenericInstanceType*>(baseType);
                baseType = genInst->getBaseType();
            }

            if (!baseType || !baseType->isStruct()) {
                return nullptr;
            }

            StructType* structType = static_cast<StructType*>(baseType);
            Type* structValueType = genInst ? static_cast<Type*>(genInst) : baseType;
            const std::string& memberName = memberExpr->getMember();

            const StructType::Field* field = structType->getField(memberName);
            if (!field) {
                return nullptr;
            }

            // Get field index
            size_t fieldIndex = 0;
            const auto& fields = structType->getFields();
            for (size_t i = 0; i < fields.size(); ++i) {
                if (fields[i].Name == memberName) {
                    fieldIndex = i;
                    break;
                }
            }

            // Ensure base is a pointer
            llvm::Value* structPtr = base;
            if (!structPtr->getType()->isPointerTy()) {
                llvm::Type* structLLVMType = getLLVMType(structValueType);
                if (!structLLVMType) {
                    return nullptr;
                }

                llvm::Value* tempAlloca = Builder->CreateAlloca(structLLVMType, nullptr, "temp.struct");
                Builder->CreateStore(structPtr, tempAlloca);
                structPtr = tempAlloca;
            }

            llvm::Type* llvmStructType = getLLVMType(structValueType);
            if (!llvmStructType) {
                return nullptr;
            }
            llvm::Type* llvmStructPtrType = llvm::PointerType::get(llvmStructType, 0);
            if (structPtr->getType() != llvmStructPtrType) {
                structPtr = Builder->CreateBitCast(structPtr, llvmStructPtrType, "struct.addr.cast");
            }

            // Return pointer to the field (don't load)
            return Builder->CreateStructGEP(
                llvmStructType,
                structPtr,
                fieldIndex,
                memberName + ".addr"
            );
        }

        case ASTNode::Kind::IndexExpr: {
            // Generate address of array/slice element
            IndexExpr* indexExpr = static_cast<IndexExpr*>(expr);

            llvm::Value* base = generateExpr(indexExpr->getBase());
            if (!base) {
                return nullptr;
            }

            llvm::Value* index = generateExpr(indexExpr->getIndex());
            if (!index) {
                return nullptr;
            }

            Type* baseType = indexExpr->getBase()->getType();
            if (!baseType) {
                return nullptr;
            }
            if (baseType->isReference()) {
                Type* pointeeType = static_cast<ReferenceType*>(baseType)->getPointeeType();
                if (!pointeeType->isArray()) {
                    llvm::Type* llvmPointeeType = getLLVMType(pointeeType);
                    if (!llvmPointeeType || !base->getType()->isPointerTy()) {
                        return nullptr;
                    }
                    base = Builder->CreateLoad(llvmPointeeType, base, "index.base.autoderef");
                }
                baseType = pointeeType;
            }

            if (baseType->isArray()) {
                ArrayType* arrayType = static_cast<ArrayType*>(baseType);

                llvm::Value* arrayPtr = base;
                if (!base->getType()->isPointerTy()) {
                    llvm::Type* arrayLLVMType = getLLVMType(arrayType);
                    if (!arrayLLVMType) {
                        return nullptr;
                    }

                    llvm::Value* tempAlloca = Builder->CreateAlloca(arrayLLVMType, nullptr, "temp.array");
                    Builder->CreateStore(base, tempAlloca);
                    arrayPtr = tempAlloca;
                }

                llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Context), 0);
                llvm::Value* indices[] = {zero, index};

                // Return pointer to element (don't load)
                return Builder->CreateGEP(
                    getLLVMType(arrayType),
                    arrayPtr,
                    indices,
                    "arrayidx.addr"
                );
            }

            if (baseType->isSlice()) {
                SliceType* sliceType = static_cast<SliceType*>(baseType);
                Type* elementType = sliceType->getElementType();

                llvm::Type* llvmElementType = getLLVMType(elementType);
                if (!llvmElementType) {
                    return nullptr;
                }

                llvm::Value* slicePtr = Builder->CreateExtractValue(base, 0, "slice.ptr");

                // Return pointer to element (don't load)
                return Builder->CreateGEP(
                    llvmElementType,
                    slicePtr,
                    index,
                    "sliceidx.addr"
                );
            }

            return nullptr;
        }

        case ASTNode::Kind::UnaryExpr: {
            // Handle dereference (*ptr)
            UnaryExpr* unaryExpr = static_cast<UnaryExpr*>(expr);
            if (unaryExpr->getOp() == UnaryExpr::Op::Deref) {
                // For dereference, just generate the pointer value
                return generateExpr(unaryExpr->getOperand());
            }
            return nullptr;
        }

        default:
            return nullptr;
    }
}

// ============================================================================
// Function call expressions
// ============================================================================

llvm::Value* CodeGen::generateCallExpr(CallExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // Generate the callee expression
    // This could be:
    // - IdentifierExpr (direct function call)
    // - MemberExpr (method call or module function call)
    // - Other expressions that evaluate to function pointers
    Expr* callee = expr->getCallee();
    if (!callee) {
        return nullptr;
    }

    auto unwrapMemberBaseType = [](Type* type) -> Type* {
        while (type) {
            if (type->isReference()) {
                type = static_cast<ReferenceType*>(type)->getPointeeType();
                continue;
            }
            if (type->isPointer()) {
                type = static_cast<PointerType*>(type)->getPointeeType();
                continue;
            }
            if (type->isGenericInstance()) {
                type = static_cast<GenericInstanceType*>(type)->getBaseType();
                continue;
            }
            break;
        }
        return type;
    };

    // Special-case: len()/iter() as built-in member calls.
    if (auto* memberExpr = dynamic_cast<MemberExpr*>(callee)) {
        if (expr->getArgCount() == 0) {
            Type* baseType = unwrapMemberBaseType(memberExpr->getBase()->getType());
            const std::string& memberName = memberExpr->getMember();

            if (baseType && memberName == "len") {
                if (baseType->isString() || baseType->isSlice() || baseType->isArray()) {
                    return generateMemberExpr(memberExpr);
                }
            }

            if (baseType && memberName == "iter") {
                if (baseType->isString() || baseType->isSlice() || baseType->isArray() ||
                    baseType->isTuple() || baseType->isVarArgs() || baseType->isRange()) {
                    llvm::Value* iterValue = generateExpr(memberExpr->getBase());
                    if (!iterValue) {
                        return nullptr;
                    }

                    Type* iterType = expr->getType();
                    if (iterType && iterValue->getType()->isPointerTy() &&
                        !iterType->isReference() && !iterType->isPointer()) {
                        llvm::Type* llvmIterType = getLLVMType(iterType);
                        if (!llvmIterType) {
                            return nullptr;
                        }
                        iterValue = Builder->CreateLoad(llvmIterType, iterValue, "iter.autoderef");
                    }

                    return iterValue;
                }
            }
        }
    }

    // Special-case: SysError.message()
    if (auto* memberExpr = dynamic_cast<MemberExpr*>(callee)) {
        if (expr->getArgCount() == 0) {
            Type* baseType = memberExpr->getBase()->getType();
            if (baseType) {
                if (baseType->isReference()) {
                    baseType = static_cast<ReferenceType*>(baseType)->getPointeeType();
                } else if (baseType->isPointer()) {
                    baseType = static_cast<PointerType*>(baseType)->getPointeeType();
                }
                if (baseType->isGenericInstance()) {
                    baseType = static_cast<GenericInstanceType*>(baseType)->getBaseType();
                }
            }
            if (baseType && baseType->isEnum()) {
                auto* enumType = static_cast<EnumType*>(baseType);
                if (enumType->getName() == "SysError" && memberExpr->getMember() == "full_trace") {
                    return emitStringLiteralValue("trace unavailable");
                }
                if (enumType->getName() == "SysError" && memberExpr->getMember() == "message") {
                    const EnumType::Variant* divVar = enumType->getVariant("DivisionByZero");
                    const EnumType::Variant* parseVar = enumType->getVariant("ParseError");

                    llvm::Value* baseVal = generateExpr(memberExpr->getBase());
                    if (!baseVal) {
                        return nullptr;
                    }
                    llvm::Type* enumLLVMType = getLLVMType(enumType);
                    if (baseVal->getType()->isPointerTy() && enumLLVMType) {
                        baseVal = Builder->CreateLoad(enumLLVMType, baseVal, "sys_error.load");
                    }

                    llvm::Value* tagValue = Builder->CreateExtractValue(baseVal, 0, "sys_error.tag");

                    llvm::Function* currentFunc = Builder->GetInsertBlock()->getParent();
                    llvm::BasicBlock* divBB = llvm::BasicBlock::Create(*Context, "sys_error.div0", currentFunc);
                    llvm::BasicBlock* parseBB = llvm::BasicBlock::Create(*Context, "sys_error.parse", currentFunc);
                    llvm::BasicBlock* defaultBB = llvm::BasicBlock::Create(*Context, "sys_error.default", currentFunc);
                    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*Context, "sys_error.merge", currentFunc);

                    llvm::SwitchInst* switchInst = Builder->CreateSwitch(tagValue, defaultBB, 2);
                    auto* tagIntTy = llvm::cast<llvm::IntegerType>(tagValue->getType());
                    if (divVar) {
                        switchInst->addCase(
                            llvm::ConstantInt::get(tagIntTy, static_cast<uint64_t>(divVar->Tag)),
                            divBB
                        );
                    }
                    if (parseVar) {
                        switchInst->addCase(
                            llvm::ConstantInt::get(tagIntTy, static_cast<uint64_t>(parseVar->Tag)),
                            parseBB
                        );
                    }

                    llvm::Type* strLLVMType = getLLVMType(Ctx.getStrType());
                    llvm::Value* divVal = nullptr;
                    llvm::Value* parseVal = nullptr;
                    llvm::Value* defaultVal = nullptr;

                    Builder->SetInsertPoint(divBB);
                    divVal = emitStringLiteralValue("Division by zero");
                    Builder->CreateBr(mergeBB);

                    Builder->SetInsertPoint(parseBB);
                    if (parseVar && !parseVar->Data.empty() && parseVar->Data[0]->isStruct()) {
                        Type* payloadType = parseVar->Data[0];
                        llvm::Type* payloadLLVMType = getLLVMType(payloadType);
                        llvm::Value* dataPtr = Builder->CreateExtractValue(baseVal, 1, "sys_error.data");
                        llvm::Value* payloadPtr = Builder->CreateBitCast(
                            dataPtr,
                            llvm::PointerType::get(payloadLLVMType, 0),
                            "sys_error.payload.ptr"
                        );
                        llvm::Value* payloadValue = Builder->CreateLoad(payloadLLVMType, payloadPtr, "sys_error.payload");
                        parseVal = Builder->CreateExtractValue(payloadValue, 0, "sys_error.message");
                    } else {
                        parseVal = emitStringLiteralValue("Unknown error");
                    }
                    Builder->CreateBr(mergeBB);

                    Builder->SetInsertPoint(defaultBB);
                    defaultVal = emitStringLiteralValue("Unknown error");
                    Builder->CreateBr(mergeBB);

                    Builder->SetInsertPoint(mergeBB);
                    llvm::PHINode* phi = Builder->CreatePHI(strLLVMType, 3, "sys_error.msg");
                    if (divVal) {
                        phi->addIncoming(divVal, divBB);
                    }
                    if (parseVal) {
                        phi->addIncoming(parseVal, parseBB);
                    }
                    phi->addIncoming(defaultVal, defaultBB);
                    return phi;
                }
            }
        }
    }

    std::string funcName;
    FuncDecl* funcDecl = nullptr;
    FunctionType* semaFuncType = nullptr;
    MemberExpr* memberExpr = nullptr;
    bool preferExternalSymbol = false;

    // Handle different callee types
    if (callee->getKind() == ASTNode::Kind::IdentifierExpr) {
        // Direct function call
        IdentifierExpr* funcIdent = static_cast<IdentifierExpr*>(callee);
        if (Decl* decl = funcIdent->getResolvedDecl()) {
            funcDecl = dynamic_cast<FuncDecl*>(decl);
        }
        if (funcDecl) {
            if (!funcDecl->getLinkName().empty()) {
                funcName = funcDecl->getLinkName();
                preferExternalSymbol = true;
            } else {
                funcName = getFunctionSymbolName(funcDecl);
            }
        } else {
            funcName = funcIdent->getName();
        }
    } else if (callee->getKind() == ASTNode::Kind::MemberExpr) {
        // Member function call (e.g., std.io.println)
        memberExpr = static_cast<MemberExpr*>(callee);

        // 如果是模块成员并带外部链接名，优先使用接口提供的符号名
        Type* baseType = memberExpr->getBase() ? memberExpr->getBase()->getType() : nullptr;
        if (baseType && baseType->isReference()) {
            baseType = static_cast<ReferenceType*>(baseType)->getPointeeType();
        }
        if (baseType && baseType->isPointer()) {
            baseType = static_cast<PointerType*>(baseType)->getPointeeType();
        }
        if (baseType && baseType->isGenericInstance()) {
            baseType = static_cast<GenericInstanceType*>(baseType)->getBaseType();
        }
        if (baseType && baseType->isModule()) {
            auto* moduleTy = static_cast<ModuleType*>(baseType);
            if (const ModuleType::Member* moduleMember = moduleTy->getMember(memberExpr->getMember())) {
                if (!moduleMember->LinkName.empty()) {
                    funcName = moduleMember->LinkName;
                    preferExternalSymbol = true;
                }
            }
        }

        // 从 MemberExpr 的 ResolvedDecl 获取函数声明
        Decl* resolvedDecl = memberExpr->getResolvedDecl();
        if (resolvedDecl && resolvedDecl->getKind() == ASTNode::Kind::FuncDecl) {
            FuncDecl* resolvedFunc = static_cast<FuncDecl*>(resolvedDecl);
            funcDecl = resolvedFunc;
            if (!resolvedFunc->getLinkName().empty()) {
                funcName = resolvedFunc->getLinkName();
                preferExternalSymbol = true;
            }

            // Trait-bound method calls in generic code may still resolve to trait
            // declarations after Sema. Prefer a concrete impl method once the
            // receiver type is known in codegen (e.g. monomorphized generic).
            Type* callBaseType = memberExpr->getBase() ? memberExpr->getBase()->getType() : nullptr;
            if (callBaseType && !GenericSubstStack.empty()) {
                callBaseType = substituteType(callBaseType);
            }
            while (callBaseType && callBaseType->isReference()) {
                callBaseType = static_cast<ReferenceType*>(callBaseType)->getPointeeType();
            }
            while (callBaseType && callBaseType->isPointer()) {
                callBaseType = static_cast<PointerType*>(callBaseType)->getPointeeType();
            }
            if (callBaseType && (!resolvedFunc->hasBody() || !resolvedFunc->getSemanticType())) {
                FuncDecl* implMethod = Ctx.getImplMethod(callBaseType, memberExpr->getMember());
                if (!implMethod && callBaseType->isGenericInstance()) {
                    auto* genInst = static_cast<GenericInstanceType*>(callBaseType);
                    implMethod = Ctx.getImplMethod(genInst->getBaseType(), memberExpr->getMember());
                }
                if (implMethod) {
                    funcDecl = implMethod;
                    if (!implMethod->getLinkName().empty()) {
                        funcName = implMethod->getLinkName();
                        preferExternalSymbol = true;
                    }
                }
            }

            if (!preferExternalSymbol) {
                funcName = getFunctionSymbolName(funcDecl);
            }
        } else {
            // 如果没有 ResolvedDecl，回退到使用成员名称
            if (funcName.empty()) {
                funcName = memberExpr->getMember();
            }
        }
    } else {
        // Other expression types not yet supported
        return nullptr;
    }

    const auto& args = expr->getArgs();
    std::vector<Expr*> plainArgs;
    plainArgs.reserve(args.size());
    bool hasSpreadArg = false;
    for (const auto& arg : args) {
        if (arg.IsSpread) {
            hasSpreadArg = true;
        }
        plainArgs.push_back(arg.Value);
    }

    // Enum variant constructor calls (Enum.Variant(...) or Variant(...))
    auto buildEnumValue = [&](Type* enumSemanticType,
                              EnumType* enumBaseType,
                              const EnumType::Variant* variant,
                              const std::vector<Expr*>& args) -> llvm::Value* {
        if (!enumSemanticType || !enumBaseType || !variant) {
            return nullptr;
        }

        llvm::Type* enumLLVMType = getLLVMType(enumSemanticType);
        if (!enumLLVMType || !enumLLVMType->isStructTy()) {
            return nullptr;
        }
        auto* enumStructTy = llvm::dyn_cast<llvm::StructType>(enumLLVMType);
        if (!enumStructTy) {
            return nullptr;
        }

        GenericSubst enumMapping;
        if (enumSemanticType->isGenericInstance()) {
            auto* enumInst = static_cast<GenericInstanceType*>(enumSemanticType);
            Type* enumBase = enumInst->getBaseType();
            if (enumBase == enumBaseType) {
                auto it = EnumGenericParams.find(enumBaseType);
                if (it == EnumGenericParams.end()) {
                    for (const auto& entry : EnumGenericParams) {
                        if (entry.first && entry.first->getName() == enumBaseType->getName()) {
                            it = EnumGenericParams.find(entry.first);
                            break;
                        }
                    }
                }
                if (it != EnumGenericParams.end() && it->second.size() == enumInst->getTypeArgCount()) {
                    for (size_t i = 0; i < it->second.size(); ++i) {
                        enumMapping[it->second[i]] = enumInst->getTypeArg(i);
                    }
                }
            }
        }

        struct ScopedSubstPush {
            std::vector<GenericSubst>& Stack;
            bool Active = false;
            ScopedSubstPush(std::vector<GenericSubst>& stack, GenericSubst mapping)
                : Stack(stack) {
                if (!mapping.empty()) {
                    Stack.push_back(std::move(mapping));
                    Active = true;
                }
            }
            ~ScopedSubstPush() {
                if (Active) {
                    Stack.pop_back();
                }
            }
        };
        ScopedSubstPush enumSubst(GenericSubstStack, std::move(enumMapping));

        llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(*Context), 0);
        if (enumStructTy->isOpaque()) {
            enumStructTy->setBody({llvm::Type::getInt32Ty(*Context), i8PtrTy});
        }

        llvm::Value* enumValue = llvm::UndefValue::get(enumStructTy);
        llvm::Value* tagVal = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Context),
                                                     static_cast<uint64_t>(variant->Tag));
        enumValue = Builder->CreateInsertValue(enumValue, tagVal, 0, "enum.tag");

        llvm::Value* dataPtr = llvm::ConstantPointerNull::get(static_cast<llvm::PointerType*>(i8PtrTy));

        if (!variant->Data.empty()) {
            llvm::Value* payloadValue = nullptr;
            llvm::Type* payloadLLVMType = nullptr;

            if (variant->Data.size() == 1) {
                Type* payloadType = variant->Data[0];
                payloadLLVMType = getLLVMType(payloadType);
                if (!payloadLLVMType) {
                    return nullptr;
                }

                if (payloadType->isTuple()) {
                    auto* tupleType = static_cast<TupleType*>(payloadType);
                    if (args.size() == tupleType->getElementCount()) {
                        payloadValue = llvm::UndefValue::get(payloadLLVMType);
                        for (size_t i = 0; i < args.size(); ++i) {
                            llvm::Value* argVal = generateExpr(args[i]);
                            if (!argVal) {
                                return nullptr;
                            }
                            payloadValue = Builder->CreateInsertValue(
                                payloadValue, argVal, {static_cast<unsigned>(i)}, "tuple.insert");
                        }
                    } else if (args.size() == 1) {
                        payloadValue = generateExpr(args[0]);
                        if (!payloadValue) {
                            return nullptr;
                        }
                    }
                } else {
                    if (args.empty()) {
                        return nullptr;
                    }
                    payloadValue = generateExpr(args[0]);
                    if (!payloadValue) {
                        return nullptr;
                    }
                }
            } else {
                std::vector<llvm::Type*> payloadFields;
                payloadFields.reserve(variant->Data.size());
                for (Type* fieldType : variant->Data) {
                    llvm::Type* llvmFieldType = getLLVMType(fieldType);
                    if (!llvmFieldType) {
                        return nullptr;
                    }
                    payloadFields.push_back(llvmFieldType);
                }
                payloadLLVMType = llvm::StructType::get(*Context, payloadFields);
                payloadValue = llvm::UndefValue::get(payloadLLVMType);
                for (size_t i = 0; i < args.size(); ++i) {
                    llvm::Value* argVal = generateExpr(args[i]);
                    if (!argVal) {
                        return nullptr;
                    }
                    payloadValue = Builder->CreateInsertValue(
                        payloadValue, argVal, {static_cast<unsigned>(i)}, "payload.insert");
                }
            }

            if (payloadValue && payloadLLVMType) {
                if (payloadValue->getType() != payloadLLVMType) {
                    if (payloadLLVMType->isPointerTy()) {
                        if (payloadValue->getType()->isPointerTy()) {
                            payloadValue = Builder->CreateBitCast(payloadValue, payloadLLVMType, "payload.cast");
                        } else if (payloadValue->getType()->isIntegerTy()) {
                            unsigned ptrBits = Module->getDataLayout().getPointerSizeInBits();
                            llvm::Type* intPtrTy = llvm::Type::getIntNTy(*Context, ptrBits);
                            llvm::Value* intVal = payloadValue;
                            if (payloadValue->getType() != intPtrTy) {
                                intVal = Builder->CreateZExtOrTrunc(payloadValue, intPtrTy, "payload.int");
                            }
                            payloadValue = Builder->CreateIntToPtr(intVal, payloadLLVMType, "payload.inttoptr");
                        } else {
                            llvm::AllocaInst* tempAlloca = Builder->CreateAlloca(
                                payloadValue->getType(), nullptr, "payload.tmp");
                            Builder->CreateStore(payloadValue, tempAlloca);
                            payloadValue = Builder->CreateBitCast(tempAlloca, payloadLLVMType, "payload.ptr");
                        }
                    } else if (payloadLLVMType->isIntegerTy()) {
                        if (payloadValue->getType()->isPointerTy()) {
                            payloadValue = Builder->CreatePtrToInt(payloadValue, payloadLLVMType, "payload.ptrtoint");
                        } else if (payloadValue->getType()->isIntegerTy()) {
                            payloadValue = Builder->CreateZExtOrTrunc(payloadValue, payloadLLVMType, "payload.int");
                        } else {
                            payloadValue = Builder->CreateBitCast(payloadValue, payloadLLVMType, "payload.cast");
                        }
                    } else {
                        payloadValue = Builder->CreateBitCast(payloadValue, payloadLLVMType, "payload.cast");
                    }
                }
                llvm::AllocaInst* payloadAlloca = Builder->CreateAlloca(payloadLLVMType, nullptr, "enum.payload");
                Builder->CreateStore(payloadValue, payloadAlloca);
                dataPtr = Builder->CreateBitCast(payloadAlloca, i8PtrTy, "enum.data.ptr");
            }
        }

        enumValue = Builder->CreateInsertValue(enumValue, dataPtr, 1, "enum.data");
        return enumValue;
    };

    if (auto* identCallee = dynamic_cast<IdentifierExpr*>(callee)) {
        if (Decl* resolved = identCallee->getResolvedDecl()) {
            if (resolved->getKind() == ASTNode::Kind::EnumVariantDecl) {
                Type* enumSemanticType = expr->getType() ? expr->getType() : identCallee->getType();
                Type* enumBaseType = enumSemanticType;
                if (enumBaseType && enumBaseType->isGenericInstance()) {
                    enumBaseType = static_cast<GenericInstanceType*>(enumBaseType)->getBaseType();
                }
                if (enumBaseType && enumBaseType->isEnum()) {
                    auto* enumBase = static_cast<EnumType*>(enumBaseType);
                    const auto* variant = enumBase->getVariant(identCallee->getName());
                    if (hasSpreadArg) {
                        return nullptr;
                    }
                    return buildEnumValue(enumSemanticType, enumBase, variant, plainArgs);
                }
            }
        }
    }

    if (memberExpr) {
        bool baseIsType = false;
        if (auto* identBase = dynamic_cast<IdentifierExpr*>(memberExpr->getBase())) {
            if (Decl* baseDecl = identBase->getResolvedDecl()) {
                if (baseDecl->getKind() == ASTNode::Kind::EnumDecl) {
                    baseIsType = true;
                }
            }
        }
        if (baseIsType) {
            Type* enumSemanticType = expr->getType() ? expr->getType() : memberExpr->getBase()->getType();
            Type* enumBaseType = enumSemanticType;
            if (enumBaseType && enumBaseType->isGenericInstance()) {
                enumBaseType = static_cast<GenericInstanceType*>(enumBaseType)->getBaseType();
            }
            if (enumBaseType && enumBaseType->isEnum()) {
                auto* enumBase = static_cast<EnumType*>(enumBaseType);
                const auto* variant = enumBase->getVariant(memberExpr->getMember());
                if (hasSpreadArg) {
                    return nullptr;
                }
                return buildEnumValue(enumSemanticType, enumBase, variant, plainArgs);
            }
        }
    }

    // Determine semantic function type (for variadic packing)
    if (funcDecl) {
        if (Type* semaType = funcDecl->getSemanticType()) {
            if (semaType->isFunction()) {
                semaFuncType = static_cast<FunctionType*>(semaType);
            }
        }
    } else if (callee->getType() && callee->getType()->isFunction()) {
        semaFuncType = static_cast<FunctionType*>(callee->getType());
    }

    // Generate code for arguments
    std::vector<llvm::Value*> argValues;
    if (hasSpreadArg) {
        // Spread 参数仅支持可变参数调用，非可变参数在语义阶段已拒绝。
        if (!semaFuncType || !semaFuncType->isVariadic()) {
            return nullptr;
        }
    }

    // Determine if we need to inject implicit self
    bool injectSelf = false;
    if (memberExpr && funcDecl && !funcDecl->getParams().empty() && funcDecl->getParams()[0]->isSelf()) {
        bool baseIsType = false;
        if (auto* identBase = dynamic_cast<IdentifierExpr*>(memberExpr->getBase())) {
            if (Decl* baseDecl = identBase->getResolvedDecl()) {
                if (baseDecl->getKind() == ASTNode::Kind::StructDecl ||
                    baseDecl->getKind() == ASTNode::Kind::EnumDecl ||
                    baseDecl->getKind() == ASTNode::Kind::TraitDecl ||
                    baseDecl->getKind() == ASTNode::Kind::TypeAliasDecl) {
                    baseIsType = true;
                }
            }
        }
        injectSelf = !baseIsType;
    }

    Type* selfParamType = nullptr;
    Type* selfActualType = nullptr;
    if (injectSelf && memberExpr) {
        selfActualType = memberExpr->getBase()->getType();
        if (funcDecl && !funcDecl->getParams().empty()) {
            selfParamType = funcDecl->getParams()[0]->getSemanticType();
        }
        if (!selfParamType && semaFuncType && semaFuncType->getParamCount() > 0) {
            selfParamType = semaFuncType->getParam(0);
        }
    }

    // Build generic mapping from arguments
    GenericSubst mapping;
    bool mappingValid = true;
    auto unifyForMapping = [&](Type* expected, Type* actual) {
        if (!expected || !actual) {
            return;
        }
        Type* expResolved = substituteType(expected);
        Type* actResolved = substituteType(actual);
        if (!expResolved || !actResolved) {
            return;
        }
        if (!typeHasGenericParam(expResolved)) {
            return;
        }
        if (unifyGenericTypes(expResolved, actResolved, mapping)) {
            return;
        }
        if (expResolved->isReference()) {
            auto* expRef = static_cast<ReferenceType*>(expResolved);
            if (unifyGenericTypes(expRef->getPointeeType(), actResolved, mapping)) {
                return;
            }
        }
        if (expResolved->isPointer()) {
            auto* expPtr = static_cast<PointerType*>(expResolved);
            if (unifyGenericTypes(expPtr->getPointeeType(), actResolved, mapping)) {
                return;
            }
        }
        if (actResolved->isReference()) {
            auto* actRef = static_cast<ReferenceType*>(actResolved);
            if (unifyGenericTypes(expResolved, actRef->getPointeeType(), mapping)) {
                return;
            }
        }
        if (actResolved->isPointer()) {
            auto* actPtr = static_cast<PointerType*>(actResolved);
            if (unifyGenericTypes(expResolved, actPtr->getPointeeType(), mapping)) {
                return;
            }
        }
        mappingValid = false;
    };

    if (semaFuncType) {
        size_t paramIndex = 0;
        if (injectSelf) {
            unifyForMapping(selfParamType, selfActualType);
            paramIndex = 1;
        }

        if (!semaFuncType->isVariadic()) {
            for (size_t i = 0; i < plainArgs.size(); ++i) {
                size_t paramIdx = paramIndex + i;
                if (paramIdx < semaFuncType->getParamCount()) {
                    unifyForMapping(semaFuncType->getParam(paramIdx), plainArgs[i]->getType());
                }
            }
        } else {
            size_t totalParams = semaFuncType->getParamCount();
            size_t fixedParams = totalParams > 0 ? (totalParams - 1) : 0;
            size_t userFixed = fixedParams;
            if (injectSelf && userFixed > 0) {
                userFixed -= 1;
            }
            size_t fixedCount = std::min(userFixed, plainArgs.size());
            for (size_t i = 0; i < fixedCount; ++i) {
                size_t paramIdx = injectSelf ? (i + 1) : i;
                if (paramIdx < fixedParams) {
                    unifyForMapping(semaFuncType->getParam(paramIdx), plainArgs[i]->getType());
                }
            }
            if (totalParams > 0) {
                Type* varParamType = semaFuncType->getParam(totalParams - 1);
                Type* varElemType = varParamType;
                if (varParamType && varParamType->isVarArgs()) {
                    varElemType = static_cast<VarArgsType*>(varParamType)->getElementType();
                }
                for (size_t i = userFixed; i < plainArgs.size(); ++i) {
                    unifyForMapping(varElemType, plainArgs[i]->getType());
                }
            }
        }
    }

    // 对无参泛型工厂函数（如 Vec.new）可通过期望返回类型反推泛型实参。
    if (semaFuncType && expr->getType()) {
        unifyForMapping(semaFuncType->getReturnType(), expr->getType());
    }

    if (!mappingValid) {
        return nullptr;
    }

    GenericSubst combinedMapping;
    if (!GenericSubstStack.empty()) {
        combinedMapping = GenericSubstStack.back();
    }
    for (const auto& entry : mapping) {
        combinedMapping[entry.first] = entry.second;
    }

    auto resolveParamType = [&](Type* type) -> Type* {
        if (!type) {
            return type;
        }
        if (combinedMapping.empty()) {
            return substituteType(type);
        }
        GenericSubstStack.push_back(combinedMapping);
        Type* resolved = substituteType(type);
        GenericSubstStack.pop_back();
        return resolved;
    };

    // Resolve the LLVM callee value (specialize if needed)
    llvm::Value* calleeValue = nullptr;
    llvm::Function* func = nullptr;
    auto getOrCreateExternalFunction = [&](const std::string& name,
                                           FunctionType* fnType) -> llvm::Function* {
        if (name.empty() || !fnType) {
            return nullptr;
        }
        llvm::FunctionType* llvmFnTy =
            llvm::dyn_cast<llvm::FunctionType>(getLLVMType(fnType));
        if (!llvmFnTy) {
            return nullptr;
        }

        llvm::Function* existing = Module->getFunction(name);
        if (existing) {
            if (existing->getFunctionType() != llvmFnTy) {
                return nullptr;
            }
            return existing;
        }

        return llvm::Function::Create(
            llvmFnTy,
            llvm::Function::ExternalLinkage,
            name,
            Module.get());
    };

    auto materializeFuncDecl = [&]() -> llvm::Function* {
        if (!funcDecl) {
            return nullptr;
        }

        // Reuse already generated value first.
        auto it = ValueMap.find(funcDecl);
        if (it != ValueMap.end()) {
            if (auto* fn = llvm::dyn_cast<llvm::Function>(it->second)) {
                return fn;
            }
        }

        if (!generateDecl(funcDecl)) {
            return nullptr;
        }

        it = ValueMap.find(funcDecl);
        if (it != ValueMap.end()) {
            if (auto* fn = llvm::dyn_cast<llvm::Function>(it->second)) {
                return fn;
            }
        }

        std::string symbolName = getFunctionSymbolName(funcDecl);
        if (llvm::Function* fn = Module->getFunction(symbolName)) {
            return fn;
        }

        if (!funcName.empty()) {
            return Module->getFunction(funcName);
        }

        return nullptr;
    };

    GenericSubst specializationMapping = mapping;
    if (specializationMapping.empty() && !combinedMapping.empty()) {
        specializationMapping = combinedMapping;
    }
    if (funcDecl && !typeHasGenericParam(funcDecl->getSemanticType())) {
        specializationMapping.clear();
    }
    if (funcDecl && !funcDecl->hasBody()) {
        specializationMapping.clear();
    }
    if (!specializationMapping.empty() && funcDecl) {
        func = getOrCreateSpecializedFunction(funcDecl, specializationMapping);
        if (!func) {
            return nullptr;
        }
        calleeValue = func;
    }
    if (!calleeValue && preferExternalSymbol && !funcName.empty()) {
        func = getOrCreateExternalFunction(funcName, semaFuncType);
        if (!func) {
            return nullptr;
        }
        calleeValue = func;
    }
    if (!calleeValue && !funcName.empty()) {
        func = Module->getFunction(funcName);
        if (func) {
            calleeValue = func;
        }
    }
    if (!calleeValue && funcDecl && !preferExternalSymbol) {
        func = materializeFuncDecl();
        if (func) {
            calleeValue = func;
        }
    }
    if (!calleeValue) {
        calleeValue = generateExpr(callee);
    }
    if (!calleeValue) {
        // Function not found
        return nullptr;
    }

    llvm::Value* selfArgValue = nullptr;
    if (injectSelf && memberExpr) {
        Type* resolvedSelfParamType = resolveParamType(selfParamType);
        if (!resolvedSelfParamType) {
            return nullptr;
        }

        llvm::Type* llvmSelfType = getLLVMType(resolvedSelfParamType);
        if (!llvmSelfType) {
            return nullptr;
        }

        if (resolvedSelfParamType->isReference() || resolvedSelfParamType->isPointer()) {
            // For mut/ref self, preserve aliasing by passing the real lvalue address.
            selfArgValue = generateLValueAddress(memberExpr->getBase());
            if (!selfArgValue) {
                llvm::Value* baseValue = generateExpr(memberExpr->getBase());
                if (!baseValue) {
                    return nullptr;
                }
                llvm::AllocaInst* tempAlloca = Builder->CreateAlloca(
                    baseValue->getType(),
                    nullptr,
                    "self.addr"
                );
                Builder->CreateStore(baseValue, tempAlloca);
                selfArgValue = tempAlloca;
            }
            if (selfArgValue->getType() != llvmSelfType) {
                selfArgValue = Builder->CreateBitCast(selfArgValue, llvmSelfType, "self.cast");
            }
        } else {
            llvm::Value* baseValue = generateExpr(memberExpr->getBase());
            if (!baseValue) {
                return nullptr;
            }
            selfArgValue = baseValue;
            if (baseValue->getType()->isPointerTy()) {
                selfArgValue = Builder->CreateLoad(llvmSelfType, baseValue, "self.load");
            } else if (baseValue->getType() != llvmSelfType) {
                selfArgValue = Builder->CreateBitCast(baseValue, llvmSelfType, "self.cast");
            }
        }
    }

    bool isVariadic = (semaFuncType && semaFuncType->isVariadic());
    size_t fixedCount = 0;
    if (isVariadic) {
        fixedCount = semaFuncType->getParamCount() - 1;
        size_t minArgs = fixedCount;
        if (injectSelf && minArgs > 0) {
            minArgs -= 1;
        }
        if (plainArgs.size() < minArgs) {
            return nullptr;
        }
    }

    auto castToParamType = [&](llvm::Value* value, Type* paramType, Type* sourceType) -> llvm::Value* {
        if (!value || !paramType) {
            return value;
        }
        Type* resolvedParamType = resolveParamType(paramType);
        if (!resolvedParamType) {
            return value;
        }
        llvm::Type* llvmParamType = getLLVMType(resolvedParamType);
        if (!llvmParamType) {
            return value;
        }
        llvmParamType = normalizeFirstClassType(llvmParamType);

        if (sourceType && sourceType->isReference() &&
            !resolvedParamType->isReference() &&
            !resolvedParamType->isPointer() &&
            value->getType()->isPointerTy()) {
            value = Builder->CreateLoad(llvmParamType, value, "arg.autoderef");
        }

        if (value->getType() == llvmParamType) {
            return value;
        }

        if (llvmParamType->isPointerTy()) {
            if (value->getType()->isPointerTy()) {
                return Builder->CreateBitCast(value, llvmParamType, "arg.cast");
            }
            if (value->getType()->isIntegerTy()) {
                return Builder->CreateIntToPtr(value, llvmParamType, "arg.inttoptr");
            }
            llvm::AllocaInst* tempAlloca = Builder->CreateAlloca(
                value->getType(), nullptr, "arg.tmp");
            Builder->CreateStore(value, tempAlloca);
            return Builder->CreateBitCast(tempAlloca, llvmParamType, "arg.ptr");
        }

        if (llvmParamType->isIntegerTy()) {
            if (value->getType()->isPointerTy()) {
                return Builder->CreatePtrToInt(value, llvmParamType, "arg.ptrtoint");
            }
            if (value->getType()->isIntegerTy()) {
                return Builder->CreateSExtOrTrunc(value, llvmParamType, "arg.int.cast");
            }
            return Builder->CreateBitCast(value, llvmParamType, "arg.cast");
        }

        return Builder->CreateBitCast(value, llvmParamType, "arg.cast");
    };

    if (!isVariadic) {
        if (injectSelf) {
            argValues.push_back(selfArgValue);
        }
        size_t paramIndex = injectSelf ? 1 : 0;
        for (Expr* arg : plainArgs) {
            llvm::Value* argValue = generateExpr(arg);
            if (!argValue) {
                return nullptr;
            }
            if (semaFuncType && paramIndex < semaFuncType->getParamCount()) {
                argValue = castToParamType(argValue, semaFuncType->getParam(paramIndex), arg->getType());
            }
            ++paramIndex;
            argValues.push_back(argValue);
        }
    } else {
        size_t userFixedCount = fixedCount;
        if (injectSelf) {
            if (userFixedCount == 0) {
                return nullptr;
            }
            userFixedCount -= 1;
        }
        if (plainArgs.size() < userFixedCount) {
            return nullptr;
        }

        if (injectSelf) {
            argValues.push_back(selfArgValue);
        }
        // Fixed arguments
        for (size_t i = 0; i < userFixedCount; ++i) {
            llvm::Value* argValue = generateExpr(plainArgs[i]);
            if (!argValue) {
                return nullptr;
            }
            if (semaFuncType) {
                size_t paramIndex = injectSelf ? (i + 1) : i;
                if (paramIndex < semaFuncType->getParamCount()) {
                    argValue = castToParamType(argValue, semaFuncType->getParam(paramIndex), plainArgs[i]->getType());
                }
            }
            argValues.push_back(argValue);
        }

        // Pack variadic arguments into VarArgs (supports a trailing spread argument).
        llvm::Type* i64Ty = llvm::Type::getInt64Ty(*Context);
        Type* varParamType = semaFuncType->getParam(semaFuncType->getParamCount() - 1);
        Type* resolvedVarParamType = resolveParamType(varParamType);
        llvm::Type* varArgsLLVMType = getLLVMType(resolvedVarParamType ? resolvedVarParamType : varParamType);
        if (!varArgsLLVMType) {
            return nullptr;
        }

        llvm::Type* valueLLVMType = getLLVMType(Ctx.getValueType());
        if (!valueLLVMType) {
            return nullptr;
        }

        Type* varElemType = nullptr;
        if (resolvedVarParamType && resolvedVarParamType->isVarArgs()) {
            varElemType = static_cast<VarArgsType*>(resolvedVarParamType)->getElementType();
        } else if (varParamType && varParamType->isVarArgs()) {
            varElemType = static_cast<VarArgsType*>(varParamType)->getElementType();
        }

        size_t spreadIndex = plainArgs.size();
        for (size_t i = userFixedCount; i < args.size(); ++i) {
            if (args[i].IsSpread) {
                spreadIndex = i;
                break;
            }
        }
        if (spreadIndex != plainArgs.size() && spreadIndex + 1 != plainArgs.size()) {
            return nullptr;
        }

        size_t plainVarCount = plainArgs.size() - userFixedCount;
        size_t beforeSpreadCount = plainVarCount;
        if (spreadIndex != plainArgs.size()) {
            beforeSpreadCount = spreadIndex - userFixedCount;
        }

        llvm::Value* spreadVarArgsValue = nullptr;
        llvm::Value* spreadLenValue = llvm::ConstantInt::get(i64Ty, 0);
        if (spreadIndex != plainArgs.size()) {
            spreadVarArgsValue = generateExpr(plainArgs[spreadIndex]);
            if (!spreadVarArgsValue) {
                return nullptr;
            }
            spreadLenValue = Builder->CreateExtractValue(spreadVarArgsValue, 0, "spread.len");
            if (!spreadLenValue->getType()->isIntegerTy(64)) {
                spreadLenValue = Builder->CreateSExtOrTrunc(spreadLenValue, i64Ty, "spread.len.i64");
            }
        }

        llvm::Value* lenValue = llvm::ConstantInt::get(i64Ty, beforeSpreadCount);
        if (spreadIndex != plainArgs.size()) {
            lenValue = Builder->CreateAdd(lenValue, spreadLenValue, "varargs.total.len");
        }

        llvm::Type* valuePtrTy = llvm::PointerType::get(valueLLVMType, 0);
        llvm::Value* valuesPtr = llvm::ConstantPointerNull::get(static_cast<llvm::PointerType*>(valuePtrTy));

        if (beforeSpreadCount > 0 || spreadIndex != plainArgs.size()) {
            llvm::AllocaInst* valuesAlloca = Builder->CreateAlloca(valueLLVMType, lenValue, "varargs.values");
            valuesPtr = valuesAlloca;

            for (size_t i = 0; i < beforeSpreadCount; ++i) {
                Expr* argExpr = plainArgs[userFixedCount + i];
                llvm::Value* argValue = generateExpr(argExpr);
                if (!argValue) {
                    return nullptr;
                }
                llvm::Value* valueObj = buildValueFrom(argExpr->getType(), argValue, varElemType);
                if (!valueObj) {
                    return nullptr;
                }
                llvm::Value* index = llvm::ConstantInt::get(i64Ty, i);
                llvm::Value* elementPtr = Builder->CreateGEP(valueLLVMType, valuesAlloca, index, "varargs.elem.ptr");
                Builder->CreateStore(valueObj, elementPtr);
            }

            if (spreadIndex != plainArgs.size()) {
                llvm::Function* currentFunc = Builder->GetInsertBlock()->getParent();
                llvm::BasicBlock* preLoopBB = Builder->GetInsertBlock();
                llvm::BasicBlock* loopCondBB = llvm::BasicBlock::Create(*Context, "varargs.spread.cond", currentFunc);
                llvm::BasicBlock* loopBodyBB = llvm::BasicBlock::Create(*Context, "varargs.spread.body", currentFunc);
                llvm::BasicBlock* loopEndBB = llvm::BasicBlock::Create(*Context, "varargs.spread.end", currentFunc);

                Builder->CreateBr(loopCondBB);
                Builder->SetInsertPoint(loopCondBB);
                llvm::PHINode* idxPhi = Builder->CreatePHI(i64Ty, 2, "varargs.spread.idx");
                idxPhi->addIncoming(llvm::ConstantInt::get(i64Ty, 0), preLoopBB);
                llvm::Value* cond = Builder->CreateICmpULT(idxPhi, spreadLenValue, "varargs.spread.has_next");
                Builder->CreateCondBr(cond, loopBodyBB, loopEndBB);

                Builder->SetInsertPoint(loopBodyBB);
                llvm::Value* spreadValueObj = callVarArgsGet(spreadVarArgsValue, idxPhi);
                if (!spreadValueObj) {
                    return nullptr;
                }
                llvm::Value* destIndex = Builder->CreateAdd(
                    idxPhi, llvm::ConstantInt::get(i64Ty, beforeSpreadCount), "varargs.spread.dest");
                llvm::Value* elementPtr = Builder->CreateGEP(
                    valueLLVMType, valuesAlloca, destIndex, "varargs.spread.elem.ptr");
                Builder->CreateStore(spreadValueObj, elementPtr);
                llvm::Value* nextIdx = Builder->CreateAdd(
                    idxPhi, llvm::ConstantInt::get(i64Ty, 1), "varargs.spread.next");
                Builder->CreateBr(loopCondBB);
                idxPhi->addIncoming(nextIdx, loopBodyBB);

                Builder->SetInsertPoint(loopEndBB);
            }
        }

        llvm::Value* varArgsValue = llvm::UndefValue::get(varArgsLLVMType);
        varArgsValue = Builder->CreateInsertValue(varArgsValue, lenValue, 0, "varargs.len");

        llvm::Type* expectedPtrTy = nullptr;
        if (auto* structTy = llvm::dyn_cast<llvm::StructType>(varArgsLLVMType)) {
            expectedPtrTy = structTy->getElementType(1);
        }
        if (expectedPtrTy && valuesPtr->getType() != expectedPtrTy) {
            valuesPtr = Builder->CreateBitCast(valuesPtr, expectedPtrTy, "varargs.ptr.cast");
        }

        varArgsValue = Builder->CreateInsertValue(varArgsValue, valuesPtr, 1, "varargs.ptr");
        argValues.push_back(varArgsValue);
    }

    // Verify argument count matches when possible
    if (semaFuncType) {
        if (argValues.size() != semaFuncType->getParamCount()) {
            return nullptr;
        }
    } else if (func && argValues.size() != func->arg_size()) {
        return nullptr;
    }

    // Create the call instruction
    // Only name the result if the function returns a non-void value
    llvm::Value* result;
    llvm::Type* retTy = nullptr;
    if (Type* exprType = expr->getType()) {
        retTy = getLLVMType(exprType);
    } else if (func) {
        retTy = func->getReturnType();
    } else if (semaFuncType) {
        Type* semRet = semaFuncType->getReturnType();
        if (semaFuncType->canError()) {
            semRet = Ctx.getErrorType(semRet);
        }
        retTy = getLLVMType(semRet);
    }
    bool isVoidRet = (retTy && retTy->isVoidTy());
    if (func) {
        result = isVoidRet ? Builder->CreateCall(func, argValues)
                           : Builder->CreateCall(func, argValues, "call");
    } else {
        llvm::FunctionType* llvmFuncTy = nullptr;
        if (semaFuncType) {
            llvmFuncTy = llvm::dyn_cast<llvm::FunctionType>(getLLVMType(semaFuncType));
        }
        if (!llvmFuncTy) {
            return nullptr;
        }
        result = isVoidRet ? Builder->CreateCall(llvmFuncTy, calleeValue, argValues)
                           : Builder->CreateCall(llvmFuncTy, calleeValue, argValues, "call");
    }

    return result;
}

// ============================================================================
// Index expressions
// ============================================================================

llvm::Value* CodeGen::generateSliceExpr(SliceExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    llvm::Value* base = generateExpr(expr->getBase());
    if (!base) {
        return nullptr;
    }

    Type* baseType = expr->getBase()->getType();
    if (!baseType) {
        return nullptr;
    }

    if (baseType->isReference()) {
        Type* pointee = static_cast<ReferenceType*>(baseType)->getPointeeType();
        if (pointee->isArray()) {
            baseType = pointee;
        } else {
            llvm::Type* llvmPointee = getLLVMType(pointee);
            if (!llvmPointee || !base->getType()->isPointerTy()) {
                return nullptr;
            }
            base = Builder->CreateLoad(llvmPointee, base, "slice.base.autoderef");
            baseType = pointee;
        }
    }

    llvm::Type* llvmResultType = getLLVMType(expr->getType());
    if (!llvmResultType || !llvmResultType->isStructTy()) {
        return nullptr;
    }

    auto* resultStructTy = llvm::cast<llvm::StructType>(llvmResultType);
    if (resultStructTy->getNumElements() < 2) {
        return nullptr;
    }
    llvm::Type* resultPtrTy = resultStructTy->getElementType(0);
    llvm::Type* resultLenTy = resultStructTy->getElementType(1);
    llvm::Type* i64Ty = llvm::Type::getInt64Ty(*Context);

    auto normalizeIndex = [&](llvm::Value* index) -> llvm::Value* {
        if (!index) {
            return nullptr;
        }
        if (!index->getType()->isIntegerTy()) {
            return nullptr;
        }
        if (!index->getType()->isIntegerTy(64)) {
            index = Builder->CreateSExtOrTrunc(index, i64Ty, "slice.idx.i64");
        }
        return index;
    };

    auto buildSliceValue = [&](llvm::Value* ptr, llvm::Value* len) -> llvm::Value* {
        if (!ptr || !len) {
            return nullptr;
        }
        if (!len->getType()->isIntegerTy()) {
            return nullptr;
        }
        if (len->getType() != resultLenTy && resultLenTy->isIntegerTy()) {
            len = Builder->CreateSExtOrTrunc(len, resultLenTy, "slice.len.cast");
        }
        if (ptr->getType() != resultPtrTy) {
            if (!ptr->getType()->isPointerTy() || !resultPtrTy->isPointerTy()) {
                return nullptr;
            }
            ptr = Builder->CreateBitCast(ptr, resultPtrTy, "slice.ptr.cast");
        }
        llvm::Value* sliceVal = llvm::UndefValue::get(resultStructTy);
        sliceVal = Builder->CreateInsertValue(sliceVal, ptr, 0, "slice.ptr");
        sliceVal = Builder->CreateInsertValue(sliceVal, len, 1, "slice.len");
        return sliceVal;
    };

    if (baseType->isArray()) {
        auto* arrayType = static_cast<ArrayType*>(baseType);
        Type* elemType = arrayType->getElementType();
        llvm::Type* llvmElemTy = getLLVMType(elemType);
        if (!llvmElemTy) {
            return nullptr;
        }
        llvmElemTy = normalizeFirstClassType(llvmElemTy);

        llvm::Value* startVal = expr->hasStart() ? generateExpr(expr->getStart())
                                                 : llvm::ConstantInt::get(i64Ty, 0);
        llvm::Value* endVal = expr->hasEnd() ? generateExpr(expr->getEnd())
                                             : llvm::ConstantInt::get(i64Ty, arrayType->getArraySize());
        startVal = normalizeIndex(startVal);
        endVal = normalizeIndex(endVal);
        if (!startVal || !endVal) {
            return nullptr;
        }
        if (expr->isInclusive() && expr->hasEnd()) {
            endVal = Builder->CreateAdd(endVal, llvm::ConstantInt::get(i64Ty, 1), "slice.end.inclusive");
        }

        llvm::Value* arrayPtr = base;
        if (!arrayPtr->getType()->isPointerTy()) {
            llvm::Type* llvmArrayTy = getLLVMType(arrayType);
            if (!llvmArrayTy) {
                return nullptr;
            }
            llvm::Value* tmp = Builder->CreateAlloca(llvmArrayTy, nullptr, "slice.array.tmp");
            Builder->CreateStore(arrayPtr, tmp);
            arrayPtr = tmp;
        }

        llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Context), 0);
        llvm::Value* indices[] = {zero, startVal};
        llvm::Value* dataPtr = Builder->CreateGEP(
            getLLVMType(arrayType),
            arrayPtr,
            indices,
            "slice.data.ptr"
        );
        llvm::Value* lenVal = Builder->CreateSub(endVal, startVal, "slice.len.calc");
        return buildSliceValue(dataPtr, lenVal);
    }

    if (baseType->isSlice() || baseType->isString()) {
        Type* elemType = nullptr;
        if (baseType->isSlice()) {
            elemType = static_cast<SliceType*>(baseType)->getElementType();
        } else {
            elemType = Ctx.getU8Type();
        }

        llvm::Type* llvmElemTy = getLLVMType(elemType);
        if (!llvmElemTy) {
            return nullptr;
        }
        llvmElemTy = normalizeFirstClassType(llvmElemTy);

        llvm::Value* dataPtr = Builder->CreateExtractValue(base, 0, "slice.base.ptr");
        llvm::Value* baseLen = Builder->CreateExtractValue(base, 1, "slice.base.len");
        if (!baseLen->getType()->isIntegerTy(64)) {
            baseLen = Builder->CreateSExtOrTrunc(baseLen, i64Ty, "slice.base.len.i64");
        }

        llvm::Value* startVal = expr->hasStart() ? generateExpr(expr->getStart())
                                                 : llvm::ConstantInt::get(i64Ty, 0);
        llvm::Value* endVal = expr->hasEnd() ? generateExpr(expr->getEnd()) : baseLen;
        startVal = normalizeIndex(startVal);
        endVal = normalizeIndex(endVal);
        if (!startVal || !endVal) {
            return nullptr;
        }
        if (expr->isInclusive() && expr->hasEnd()) {
            endVal = Builder->CreateAdd(endVal, llvm::ConstantInt::get(i64Ty, 1), "slice.end.inclusive");
        }

        llvm::Value* slicedPtr = Builder->CreateGEP(llvmElemTy, dataPtr, startVal, "slice.ptr.offset");
        llvm::Value* lenVal = Builder->CreateSub(endVal, startVal, "slice.len.calc");
        return buildSliceValue(slicedPtr, lenVal);
    }

    return nullptr;
}

llvm::Value* CodeGen::generateIndexExpr(IndexExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // Generate the base expression
    llvm::Value* base = generateExpr(expr->getBase());
    if (!base) {
        return nullptr;
    }

    // Generate the index expression
    llvm::Value* index = generateExpr(expr->getIndex());
    if (!index) {
        return nullptr;
    }

    // Get the type of the base expression
    Type* baseType = expr->getBase()->getType();
    if (!baseType) {
        return nullptr;
    }

    // Handle VarArgs indexing
    if (baseType->isVarArgs()) {
        auto* varArgsType = static_cast<VarArgsType*>(baseType);
        llvm::Value* varArgsValue = base;
        llvm::Value* indexValue = index;
        llvm::Value* valueObj = callVarArgsGet(varArgsValue, indexValue);
        if (!valueObj) {
            return nullptr;
        }
        return convertValueToType(valueObj, varArgsType->getElementType());
    }

    // Handle array indexing
    if (baseType->isArray()) {
        ArrayType* arrayType = static_cast<ArrayType*>(baseType);
        Type* elementType = arrayType->getElementType();

        llvm::Type* llvmElementType = getLLVMType(elementType);
        if (!llvmElementType) {
            return nullptr;
        }
        llvmElementType = normalizeFirstClassType(llvmElementType);

        // Check if base is a pointer (array stored in memory)
        llvm::Value* arrayPtr = base;
        if (!base->getType()->isPointerTy()) {
            // If base is an array value, store it to memory first
            llvm::Type* arrayLLVMType = getLLVMType(arrayType);
            if (!arrayLLVMType) {
                return nullptr;
            }

            llvm::Value* tempAlloca = Builder->CreateAlloca(arrayLLVMType, nullptr, "temp.array");
            Builder->CreateStore(base, tempAlloca);
            arrayPtr = tempAlloca;
        }

        // Use GEP to get pointer to the element
        // GEP indices: [0, index]
        // First 0: dereference the pointer to array
        // Second index: index into the array
        llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Context), 0);
        llvm::Value* indices[] = {zero, index};
        llvm::Value* elementPtr = Builder->CreateGEP(
            getLLVMType(arrayType),
            arrayPtr,
            indices,
            "arrayidx"
        );

        // Load the element value
        return Builder->CreateLoad(llvmElementType, elementPtr, "arrayelem");
    }

    // Handle slice indexing
    if (baseType->isSlice()) {
        // Slice is { ptr, len }
        // We need to extract the pointer and index into it
        SliceType* sliceType = static_cast<SliceType*>(baseType);
        Type* elementType = sliceType->getElementType();

        llvm::Type* llvmElementType = getLLVMType(elementType);
        if (!llvmElementType) {
            return nullptr;
        }
        llvmElementType = normalizeFirstClassType(llvmElementType);

        // Extract the pointer from the slice struct
        // Slice struct: { i8*, i64 } or { element_type*, i64 }
        llvm::Value* slicePtr = Builder->CreateExtractValue(base, 0, "slice.ptr");

        // Use GEP to get pointer to the element
        llvm::Value* elementPtr = Builder->CreateGEP(
            llvmElementType,
            slicePtr,
            index,
            "sliceidx"
        );

        // Load the element value
        return Builder->CreateLoad(llvmElementType, elementPtr, "sliceelem");
    }

    // Handle pointer indexing (pointer arithmetic)
    if (baseType->isPointer()) {
        PointerType* ptrType = static_cast<PointerType*>(baseType);
        Type* pointeeType = ptrType->getPointeeType();

        llvm::Type* llvmPointeeType = getLLVMType(pointeeType);
        if (!llvmPointeeType) {
            return nullptr;
        }
        llvmPointeeType = normalizeFirstClassType(llvmPointeeType);

        // Use GEP for pointer arithmetic
        llvm::Value* elementPtr = Builder->CreateGEP(
            llvmPointeeType,
            base,
            index,
            "ptridx"
        );

        // Load the element value
        return Builder->CreateLoad(llvmPointeeType, elementPtr, "ptrelem");
    }

    // Handle tuple indexing with constant indices
    if (baseType->isTuple()) {
        TupleType* tupleType = static_cast<TupleType*>(baseType);

        // Check if index is a constant integer
        if (llvm::ConstantInt* constIndex = llvm::dyn_cast<llvm::ConstantInt>(index)) {
            uint64_t indexValue = constIndex->getZExtValue();
            const auto& elements = tupleType->getElements();

            if (indexValue >= elements.size()) {
                // Index out of bounds
                return nullptr;
            }

            // Get the element type
            Type* elementType = elements[indexValue];
            llvm::Type* llvmElementType = getLLVMType(elementType);
            if (!llvmElementType) {
                return nullptr;
            }
            llvmElementType = normalizeFirstClassType(llvmElementType);

            // Check if base is a pointer
            llvm::Value* tuplePtr = base;
            if (!base->getType()->isPointerTy()) {
                // Store tuple to memory first
                llvm::Type* tupleLLVMType = getLLVMType(tupleType);
                if (!tupleLLVMType) {
                    return nullptr;
                }

                llvm::Value* tempAlloca = Builder->CreateAlloca(tupleLLVMType, nullptr, "temp.tuple");
                Builder->CreateStore(base, tempAlloca);
                tuplePtr = tempAlloca;
            }

            // Use GEP to get pointer to the element
            llvm::Value* elementPtr = Builder->CreateStructGEP(
                getLLVMType(tupleType),
                tuplePtr,
                indexValue,
                "tupleidx"
            );

            // Load the element value
            return Builder->CreateLoad(llvmElementType, elementPtr, "tupleelem");
        }

        // Non-constant index for tuple is not supported
        return nullptr;
    }

    return nullptr;
}

// ============================================================================
// Struct literal expressions
// ============================================================================

llvm::Value* CodeGen::generateStructExpr(StructExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    Type* exprType = expr->getType();
    if (!exprType) {
        return nullptr;
    }

    Type* resolvedExprType = substituteType(exprType);
    if (!resolvedExprType) {
        resolvedExprType = exprType;
    }

    Type* baseType = resolvedExprType;
    if (baseType->isGenericInstance()) {
        baseType = static_cast<GenericInstanceType*>(baseType)->getBaseType();
    }

    // Enum struct-variant literal: Enum.Variant { ... }
    if (baseType->isEnum()) {
        auto* enumType = static_cast<EnumType*>(baseType);

        auto splitEnumVariant = [](const std::string& name,
                                   std::string& enumName,
                                   std::string& variantName) -> bool {
            size_t pos = name.rfind("::");
            size_t dotPos = name.rfind('.');
            if (pos == std::string::npos || (dotPos != std::string::npos && dotPos > pos)) {
                pos = dotPos;
                if (pos == std::string::npos) {
                    return false;
                }
                enumName = name.substr(0, pos);
                variantName = name.substr(pos + 1);
                return !enumName.empty() && !variantName.empty();
            }
            enumName = name.substr(0, pos);
            variantName = name.substr(pos + 2);
            return !enumName.empty() && !variantName.empty();
        };

        std::string enumName;
        std::string variantName;
        if (!splitEnumVariant(expr->getTypeName(), enumName, variantName)) {
            return nullptr;
        }

        const EnumType::Variant* variant = enumType->getVariant(variantName);
        if (!variant || variant->Data.size() != 1 || !variant->Data[0]->isStruct()) {
            return nullptr;
        }

        auto* payloadStructType = static_cast<StructType*>(variant->Data[0]);
        llvm::Type* llvmPayloadType = getLLVMType(payloadStructType);
        if (!llvmPayloadType) {
            return nullptr;
        }

        // Build payload struct value
        llvm::Value* payloadValue = nullptr;
        if (expr->hasBase()) {
            llvm::Value* baseValue = generateExpr(expr->getBase());
            if (!baseValue) {
                return nullptr;
            }
            if (baseValue->getType()->isPointerTy()) {
                baseValue = Builder->CreateLoad(llvmPayloadType, baseValue, "payload.base");
            }
            payloadValue = baseValue;
        } else {
            payloadValue = llvm::Constant::getNullValue(llvmPayloadType);
        }

        const auto& fields = payloadStructType->getFields();
        for (const auto& fieldInit : expr->getFields()) {
            size_t fieldIndex = fields.size();
            for (size_t i = 0; i < fields.size(); ++i) {
                if (fields[i].Name == fieldInit.Name) {
                    fieldIndex = i;
                    break;
                }
            }
            if (fieldIndex >= fields.size()) {
                return nullptr;
            }

            llvm::Value* fieldValue = generateExpr(fieldInit.Value);
            if (!fieldValue) {
                return nullptr;
            }

            llvm::Type* fieldLLVMType = getLLVMType(fields[fieldIndex].FieldType);
            if (!fieldLLVMType) {
                return nullptr;
            }
            if (fieldValue->getType() != fieldLLVMType) {
                if (fieldLLVMType->isPointerTy()) {
                    if (fieldValue->getType()->isPointerTy()) {
                        fieldValue = Builder->CreateBitCast(fieldValue, fieldLLVMType, "field.cast");
                    } else if (fieldValue->getType()->isIntegerTy()) {
                        fieldValue = Builder->CreateIntToPtr(fieldValue, fieldLLVMType, "field.inttoptr");
                    } else {
                        llvm::AllocaInst* tempAlloca = Builder->CreateAlloca(
                            fieldValue->getType(), nullptr, "field.tmp");
                        Builder->CreateStore(fieldValue, tempAlloca);
                        fieldValue = Builder->CreateBitCast(tempAlloca, fieldLLVMType, "field.ptr");
                    }
                } else if (fieldLLVMType->isIntegerTy()) {
                    if (fieldValue->getType()->isPointerTy()) {
                        fieldValue = Builder->CreatePtrToInt(fieldValue, fieldLLVMType, "field.ptrtoint");
                    } else if (fieldValue->getType()->isIntegerTy()) {
                        fieldValue = Builder->CreateSExtOrTrunc(fieldValue, fieldLLVMType, "field.int.cast");
                    } else {
                        fieldValue = Builder->CreateBitCast(fieldValue, fieldLLVMType, "field.cast");
                    }
                } else {
                    fieldValue = Builder->CreateBitCast(fieldValue, fieldLLVMType, "field.cast");
                }
            }

            payloadValue = Builder->CreateInsertValue(
                payloadValue,
                fieldValue,
                {static_cast<unsigned>(fieldIndex)},
                fieldInit.Name
            );
        }

        // Construct enum value { tag, data_ptr }
        llvm::Type* enumLLVMType = getLLVMType(enumType);
        if (!enumLLVMType || !enumLLVMType->isStructTy()) {
            return nullptr;
        }

        llvm::Value* enumValue = llvm::UndefValue::get(enumLLVMType);
        llvm::Value* tagVal = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Context),
                                                     static_cast<uint64_t>(variant->Tag));
        enumValue = Builder->CreateInsertValue(enumValue, tagVal, 0, "enum.tag");

        llvm::AllocaInst* payloadAlloca = Builder->CreateAlloca(llvmPayloadType, nullptr, "enum.payload");
        Builder->CreateStore(payloadValue, payloadAlloca);

        llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(*Context), 0);
        llvm::Value* dataPtr = Builder->CreateBitCast(payloadAlloca, i8PtrTy, "enum.data.ptr");
        enumValue = Builder->CreateInsertValue(enumValue, dataPtr, 1, "enum.data");

        return enumValue;
    }

    if (!baseType->isStruct()) {
        return nullptr;
    }

    auto* structType = static_cast<StructType*>(baseType);
    llvm::Type* llvmStructType = getLLVMType(resolvedExprType);
    if (!llvmStructType) {
        return nullptr;
    }

    llvm::Value* value = nullptr;
    if (expr->hasBase()) {
        llvm::Value* baseValue = generateExpr(expr->getBase());
        if (!baseValue) {
            return nullptr;
        }
        if (baseValue->getType()->isPointerTy()) {
            baseValue = Builder->CreateLoad(llvmStructType, baseValue, "struct.base");
        }
        value = baseValue;
    } else {
        value = llvm::Constant::getNullValue(llvmStructType);
    }

    GenericSubst structMapping;
    GenericSubst combinedMapping;
    if (resolvedExprType->isGenericInstance()) {
        auto* genInst = static_cast<GenericInstanceType*>(resolvedExprType);
        auto it = StructGenericParams.find(structType);
        if (it != StructGenericParams.end()) {
            const auto& params = it->second;
            if (params.size() == genInst->getTypeArgCount()) {
                for (size_t i = 0; i < params.size(); ++i) {
                    structMapping[params[i]] = genInst->getTypeArg(i);
                }
            }
        }
    }

    if (!structMapping.empty()) {
        if (!GenericSubstStack.empty()) {
            combinedMapping = GenericSubstStack.back();
        }
        for (const auto& entry : structMapping) {
            combinedMapping[entry.first] = entry.second;
        }
    }

    auto resolveFieldType = [&](Type* fieldType) -> Type* {
        if (!fieldType) {
            return fieldType;
        }
        if (structMapping.empty()) {
            return substituteType(fieldType);
        }
        GenericSubstStack.push_back(combinedMapping);
        Type* resolved = substituteType(fieldType);
        GenericSubstStack.pop_back();
        return resolved;
    };

    const auto& fields = structType->getFields();
    for (const auto& fieldInit : expr->getFields()) {
        // Find the field index by name
        size_t fieldIndex = fields.size();
        for (size_t i = 0; i < fields.size(); ++i) {
            if (fields[i].Name == fieldInit.Name) {
                fieldIndex = i;
                break;
            }
        }
        if (fieldIndex >= fields.size()) {
            return nullptr;
        }

        llvm::Value* fieldValue = generateExpr(fieldInit.Value);
        if (!fieldValue) {
            return nullptr;
        }

        Type* resolvedFieldType = resolveFieldType(fields[fieldIndex].FieldType);
        llvm::Type* fieldLLVMType = getLLVMType(resolvedFieldType);
        if (!fieldLLVMType) {
            return nullptr;
        }
        fieldLLVMType = normalizeFirstClassType(fieldLLVMType);
        if (fieldValue->getType() != fieldLLVMType) {
            if (fieldLLVMType->isPointerTy()) {
                if (fieldValue->getType()->isPointerTy()) {
                    fieldValue = Builder->CreateBitCast(fieldValue, fieldLLVMType, "field.cast");
                } else if (fieldValue->getType()->isIntegerTy()) {
                    fieldValue = Builder->CreateIntToPtr(fieldValue, fieldLLVMType, "field.inttoptr");
                } else {
                    llvm::AllocaInst* tempAlloca = Builder->CreateAlloca(
                        fieldValue->getType(), nullptr, "field.tmp");
                    Builder->CreateStore(fieldValue, tempAlloca);
                    fieldValue = Builder->CreateBitCast(tempAlloca, fieldLLVMType, "field.ptr");
                }
            } else if (fieldLLVMType->isIntegerTy()) {
                if (fieldValue->getType()->isPointerTy()) {
                    fieldValue = Builder->CreatePtrToInt(fieldValue, fieldLLVMType, "field.ptrtoint");
                } else if (fieldValue->getType()->isIntegerTy()) {
                    fieldValue = Builder->CreateSExtOrTrunc(fieldValue, fieldLLVMType, "field.int.cast");
                } else {
                    fieldValue = Builder->CreateBitCast(fieldValue, fieldLLVMType, "field.cast");
                }
            } else {
                fieldValue = Builder->CreateBitCast(fieldValue, fieldLLVMType, "field.cast");
            }
        }

        value = Builder->CreateInsertValue(
            value,
            fieldValue,
            {static_cast<unsigned>(fieldIndex)},
            fieldInit.Name
        );
    }

    return value;
}

llvm::Value* CodeGen::generateArrayExpr(ArrayExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    Type* exprType = expr->getType();
    if (!exprType || !exprType->isArray()) {
        return nullptr;
    }

    auto* arrayType = static_cast<ArrayType*>(exprType);
    llvm::Type* llvmArrayType = getLLVMType(arrayType);
    if (!llvmArrayType) {
        return nullptr;
    }

    Type* elemType = arrayType->getElementType();
    llvm::Type* llvmElemType = getLLVMType(elemType);
    if (!llvmElemType) {
        return nullptr;
    }
    llvmElemType = normalizeFirstClassType(llvmElemType);

    llvm::Value* arrayValue = llvm::UndefValue::get(llvmArrayType);

    if (expr->isRepeat()) {
        Expr* countExpr = expr->getRepeatCount();
        if (!countExpr) {
            return nullptr;
        }

        llvm::Value* countVal = generateExpr(countExpr);
        if (!countVal || !llvm::isa<llvm::ConstantInt>(countVal)) {
            return nullptr;
        }

        uint64_t count = llvm::cast<llvm::ConstantInt>(countVal)->getZExtValue();
        if (count != arrayType->getArraySize()) {
            return nullptr;
        }

        if (expr->getElements().empty()) {
            return nullptr;
        }
        llvm::Value* elemValue = generateExpr(expr->getElements().front());
        if (!elemValue) {
            return nullptr;
        }

        if (elemValue->getType() != llvmElemType) {
            if (llvmElemType->isPointerTy()) {
                if (elemValue->getType()->isPointerTy()) {
                    elemValue = Builder->CreateBitCast(elemValue, llvmElemType, "array.cast");
                } else if (elemValue->getType()->isIntegerTy()) {
                    elemValue = Builder->CreateIntToPtr(elemValue, llvmElemType, "array.inttoptr");
                } else {
                    llvm::AllocaInst* tempAlloca = Builder->CreateAlloca(
                        elemValue->getType(), nullptr, "array.tmp");
                    Builder->CreateStore(elemValue, tempAlloca);
                    elemValue = Builder->CreateBitCast(tempAlloca, llvmElemType, "array.ptr");
                }
            } else if (llvmElemType->isIntegerTy()) {
                if (elemValue->getType()->isPointerTy()) {
                    elemValue = Builder->CreatePtrToInt(elemValue, llvmElemType, "array.ptrtoint");
                } else if (elemValue->getType()->isIntegerTy()) {
                    elemValue = Builder->CreateSExtOrTrunc(elemValue, llvmElemType, "array.int.cast");
                } else {
                    elemValue = Builder->CreateBitCast(elemValue, llvmElemType, "array.cast");
                }
            } else {
                elemValue = Builder->CreateBitCast(elemValue, llvmElemType, "array.cast");
            }
        }

        for (uint64_t i = 0; i < count; ++i) {
            arrayValue = Builder->CreateInsertValue(
                arrayValue,
                elemValue,
                {static_cast<unsigned>(i)},
                "array.elem"
            );
        }
        return arrayValue;
    }

    const auto& elements = expr->getElements();
    if (elements.size() != arrayType->getArraySize()) {
        return nullptr;
    }

    for (size_t i = 0; i < elements.size(); ++i) {
        llvm::Value* elemValue = generateExpr(elements[i]);
        if (!elemValue) {
            return nullptr;
        }

        if (elemValue->getType() != llvmElemType) {
            if (llvmElemType->isPointerTy()) {
                if (elemValue->getType()->isPointerTy()) {
                    elemValue = Builder->CreateBitCast(elemValue, llvmElemType, "array.cast");
                } else if (elemValue->getType()->isIntegerTy()) {
                    elemValue = Builder->CreateIntToPtr(elemValue, llvmElemType, "array.inttoptr");
                } else {
                    llvm::AllocaInst* tempAlloca = Builder->CreateAlloca(
                        elemValue->getType(), nullptr, "array.tmp");
                    Builder->CreateStore(elemValue, tempAlloca);
                    elemValue = Builder->CreateBitCast(tempAlloca, llvmElemType, "array.ptr");
                }
            } else if (llvmElemType->isIntegerTy()) {
                if (elemValue->getType()->isPointerTy()) {
                    elemValue = Builder->CreatePtrToInt(elemValue, llvmElemType, "array.ptrtoint");
                } else if (elemValue->getType()->isIntegerTy()) {
                    elemValue = Builder->CreateSExtOrTrunc(elemValue, llvmElemType, "array.int.cast");
                } else {
                    elemValue = Builder->CreateBitCast(elemValue, llvmElemType, "array.cast");
                }
            } else {
                elemValue = Builder->CreateBitCast(elemValue, llvmElemType, "array.cast");
            }
        }

        arrayValue = Builder->CreateInsertValue(
            arrayValue,
            elemValue,
            {static_cast<unsigned>(i)},
            "array.elem"
        );
    }

    return arrayValue;
}

llvm::Value* CodeGen::generateTupleExpr(TupleExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    Type* exprType = expr->getType();
    if (!exprType || !exprType->isTuple()) {
        return nullptr;
    }

    auto* tupleType = static_cast<TupleType*>(exprType);
    llvm::Type* llvmTupleType = getLLVMType(tupleType);
    if (!llvmTupleType) {
        return nullptr;
    }

    const auto& elements = expr->getElements();
    if (elements.size() != tupleType->getElementCount()) {
        return nullptr;
    }

    llvm::Value* tupleValue = llvm::UndefValue::get(llvmTupleType);
    for (size_t i = 0; i < elements.size(); ++i) {
        llvm::Value* elemValue = generateExpr(elements[i]);
        if (!elemValue) {
            return nullptr;
        }

        Type* elemType = tupleType->getElement(i);
        llvm::Type* llvmElemType = getLLVMType(elemType);
        if (!llvmElemType) {
            return nullptr;
        }
        llvmElemType = normalizeFirstClassType(llvmElemType);

        if (elemValue->getType() != llvmElemType) {
            if (llvmElemType->isPointerTy()) {
                if (elemValue->getType()->isPointerTy()) {
                    elemValue = Builder->CreateBitCast(elemValue, llvmElemType, "tuple.cast");
                } else if (elemValue->getType()->isIntegerTy()) {
                    elemValue = Builder->CreateIntToPtr(elemValue, llvmElemType, "tuple.inttoptr");
                } else {
                    llvm::AllocaInst* tempAlloca = Builder->CreateAlloca(
                        elemValue->getType(), nullptr, "tuple.tmp");
                    Builder->CreateStore(elemValue, tempAlloca);
                    elemValue = Builder->CreateBitCast(tempAlloca, llvmElemType, "tuple.ptr");
                }
            } else if (llvmElemType->isIntegerTy()) {
                if (elemValue->getType()->isPointerTy()) {
                    elemValue = Builder->CreatePtrToInt(elemValue, llvmElemType, "tuple.ptrtoint");
                } else if (elemValue->getType()->isIntegerTy()) {
                    elemValue = Builder->CreateSExtOrTrunc(elemValue, llvmElemType, "tuple.int.cast");
                } else {
                    elemValue = Builder->CreateBitCast(elemValue, llvmElemType, "tuple.cast");
                }
            } else {
                elemValue = Builder->CreateBitCast(elemValue, llvmElemType, "tuple.cast");
            }
        }

        tupleValue = Builder->CreateInsertValue(
            tupleValue,
            elemValue,
            {static_cast<unsigned>(i)},
            "tuple.elem"
        );
    }

    return tupleValue;
}

// ============================================================================
// Control flow expressions
// ============================================================================

llvm::Value* CodeGen::generateIfExpr(IfExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // If expressions must have a result type (not void)
    Type* resultType = expr->getType();
    if (!resultType) {
        return nullptr;
    }

    llvm::Type* llvmResultType = getLLVMType(resultType);
    if (!llvmResultType) {
        return nullptr;
    }

    llvm::Function* currentFunc = Builder->GetInsertBlock()->getParent();

    // Create a temporary alloca to store the result
    llvm::AllocaInst* resultAlloca = Builder->CreateAlloca(llvmResultType, nullptr, "if.result");

    // Get branches
    const auto& branches = expr->getBranches();
    if (branches.empty()) {
        return nullptr;
    }

    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*Context, "if.merge", currentFunc);
    llvm::BasicBlock* currentBB = Builder->GetInsertBlock();

    // Generate branches
    for (size_t i = 0; i < branches.size(); ++i) {
        const IfExpr::Branch& branch = branches[i];

        Builder->SetInsertPoint(currentBB);

        if (branch.Condition) {
            // If or elif branch - generate condition
            llvm::Value* cond = generateExpr(branch.Condition);
            if (!cond) {
                return nullptr;
            }

            // Create blocks for then and next branch
            llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(*Context, "if.then", currentFunc);
            llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(*Context, "if.else", currentFunc);

            Builder->CreateCondBr(cond, thenBB, elseBB);

            // Generate then body
            Builder->SetInsertPoint(thenBB);
            llvm::Value* thenValue = generateExpr(branch.Body);
            if (!thenValue) {
                return nullptr;
            }

            // Store the result
            Builder->CreateStore(thenValue, resultAlloca);
            Builder->CreateBr(mergeBB);

            // Continue with else block
            currentBB = elseBB;
        } else {
            // Else branch
            Builder->SetInsertPoint(currentBB);
            llvm::Value* elseValue = generateExpr(branch.Body);
            if (!elseValue) {
                return nullptr;
            }

            // Store the result
            Builder->CreateStore(elseValue, resultAlloca);
            Builder->CreateBr(mergeBB);
            currentBB = nullptr; // No more branches
        }
    }

    // If no else branch, this is an error for if expressions
    // (if expressions must have an else branch to produce a value)
    if (currentBB) {
        // This shouldn't happen if Sema did its job correctly
        return nullptr;
    }

    // Continue with merge block
    Builder->SetInsertPoint(mergeBB);

    // Load and return the result
    return Builder->CreateLoad(llvmResultType, resultAlloca, "if.result.load");
}

llvm::Value* CodeGen::generateBlockExpr(BlockExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // Generate all statements in the block
    for (Stmt* stmt : expr->getStatements()) {
        if (!generateStmt(stmt)) {
            return nullptr;
        }
        if (Builder->GetInsertBlock()->getTerminator()) {
            return nullptr;
        }
    }

    if (!expr->hasResult()) {
        return nullptr;
    }

    llvm::Value* result = generateExpr(expr->getResultExpr());
    return result;
}

llvm::Value* CodeGen::generateClosureExpr(ClosureExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    Type* closureType = expr->getType();
    if (!closureType || !closureType->isFunction()) {
        return nullptr;
    }
    auto* semFuncType = static_cast<FunctionType*>(closureType);

    llvm::Type* llvmClosureType = getLLVMType(closureType);
    auto* llvmFuncType = llvm::dyn_cast_or_null<llvm::FunctionType>(llvmClosureType);
    if (!llvmFuncType) {
        return nullptr;
    }

    static uint64_t closureCounter = 0;
    std::string closureName = "__yuan_closure_" + std::to_string(++closureCounter);

    llvm::Function* fn = llvm::Function::Create(
        llvmFuncType,
        llvm::Function::InternalLinkage,
        closureName,
        Module.get());

    auto savedIP = Builder->saveIP();
    llvm::Function* savedCurrentFunction = CurrentFunction;
    std::string savedFunctionName = CurrentFunctionName;
    FuncDecl* savedCurrentFuncDecl = CurrentFuncDecl;
    std::vector<Stmt*> savedDeferStack = DeferStack;

    CurrentFunction = fn;
    CurrentFunctionName = closureName;
    CurrentFuncDecl = nullptr;
    DeferStack.clear();

    llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(*Context, "entry", fn);
    Builder->SetInsertPoint(entryBB);

    // Bind closure parameters.
    size_t paramIndex = 0;
    for (llvm::Argument& arg : fn->args()) {
        Type* semParamType = semFuncType->getParam(paramIndex);
        llvm::Type* llvmParamType = getLLVMType(semParamType);
        if (!llvmParamType) {
            return nullptr;
        }
        llvmParamType = normalizeFirstClassType(llvmParamType);

        std::string paramName = "arg" + std::to_string(paramIndex);
        if (paramIndex < expr->getParams().size() && expr->getParams()[paramIndex]) {
            paramName = expr->getParams()[paramIndex]->getName();
        }
        arg.setName(paramName);

        llvm::IRBuilder<> entryBuilder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
        llvm::AllocaInst* alloca = entryBuilder.CreateAlloca(llvmParamType, nullptr, paramName);
        Builder->CreateStore(&arg, alloca);

        if (paramIndex < expr->getParams().size() && expr->getParams()[paramIndex]) {
            ValueMap[expr->getParams()[paramIndex]] = alloca;
        }
        ++paramIndex;
    }

    auto emitReturn = [&](llvm::Value* value) -> bool {
        Type* semRetType = semFuncType->getReturnType();
        llvm::Type* llvmRetType = getLLVMType(semRetType);
        if (!llvmRetType) {
            return false;
        }
        llvmRetType = normalizeFirstClassType(llvmRetType);

        if (llvmRetType->isVoidTy()) {
            Builder->CreateRetVoid();
            return true;
        }

        if (!value) {
            Builder->CreateRet(llvm::Constant::getNullValue(llvmRetType));
            return true;
        }

        if (value->getType() != llvmRetType) {
            if (value->getType()->isIntegerTy() && llvmRetType->isIntegerTy()) {
                value = Builder->CreateSExtOrTrunc(value, llvmRetType, "closure.ret.int.cast");
            } else if (value->getType()->isPointerTy() && llvmRetType->isPointerTy()) {
                value = Builder->CreateBitCast(value, llvmRetType, "closure.ret.ptr.cast");
            } else if (value->getType()->isPointerTy() && llvmRetType->isIntegerTy()) {
                value = Builder->CreatePtrToInt(value, llvmRetType, "closure.ret.ptrtoint");
            } else if (value->getType()->isIntegerTy() && llvmRetType->isPointerTy()) {
                value = Builder->CreateIntToPtr(value, llvmRetType, "closure.ret.inttoptr");
            } else {
                value = Builder->CreateBitCast(value, llvmRetType, "closure.ret.cast");
            }
        }

        Builder->CreateRet(value);
        return true;
    };

    bool bodyOk = true;
    if (auto* blockBody = dynamic_cast<BlockExpr*>(expr->getBody())) {
        for (Stmt* stmt : blockBody->getStatements()) {
            if (!generateStmt(stmt)) {
                bodyOk = false;
                break;
            }
            if (Builder->GetInsertBlock()->getTerminator()) {
                break;
            }
        }

        if (bodyOk && !Builder->GetInsertBlock()->getTerminator()) {
            llvm::Value* result = nullptr;
            if (blockBody->hasResult()) {
                result = generateExpr(blockBody->getResultExpr());
                if (!result) {
                    bodyOk = false;
                }
            }
            if (bodyOk && !emitReturn(result)) {
                bodyOk = false;
            }
        }
    } else {
        llvm::Value* result = generateExpr(expr->getBody());
        if (!result) {
            bodyOk = false;
        } else if (!emitReturn(result)) {
            bodyOk = false;
        }
    }

    if (!bodyOk) {
        return nullptr;
    }

    if (!Builder->GetInsertBlock()->getTerminator()) {
        if (!emitReturn(nullptr)) {
            return nullptr;
        }
    }

    CurrentFunction = savedCurrentFunction;
    CurrentFunctionName = savedFunctionName;
    CurrentFuncDecl = savedCurrentFuncDecl;
    DeferStack = std::move(savedDeferStack);
    Builder->restoreIP(savedIP);

    return fn;
}

llvm::Value* CodeGen::generateAwaitExpr(AwaitExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    llvm::FunctionCallee suspendFn = Module->getOrInsertFunction(
        "yuan_async_suspend_point",
        llvm::FunctionType::get(llvm::Type::getVoidTy(*Context), {}, false)
    );
    Builder->CreateCall(suspendFn, {});

    return generateExpr(expr->getInner());
}

llvm::Value* CodeGen::generateMatchExpr(MatchExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    Expr* scrutinee = expr->getScrutinee();
    if (!scrutinee) {
        return nullptr;
    }

    Type* scrutineeType = scrutinee->getType();
    if (!scrutineeType) {
        return nullptr;
    }

    Type* resultType = expr->getType();
    if (!resultType) {
        return nullptr;
    }

    llvm::Type* llvmScrutineeType = getLLVMType(scrutineeType);
    llvm::Type* llvmResultType = getLLVMType(resultType);
    if (!llvmScrutineeType || !llvmResultType) {
        return nullptr;
    }

    llvm::Value* scrutineeValue = generateExpr(scrutinee);
    if (!scrutineeValue) {
        return nullptr;
    }

    llvm::Function* currentFunc = Builder->GetInsertBlock()->getParent();

    llvm::AllocaInst* scrutineeAlloca = Builder->CreateAlloca(llvmScrutineeType, nullptr, "match.scrutinee");
    Builder->CreateStore(scrutineeValue, scrutineeAlloca);

    llvm::AllocaInst* resultAlloca = Builder->CreateAlloca(llvmResultType, nullptr, "match.result");

    llvm::BasicBlock* endBB = llvm::BasicBlock::Create(*Context, "match.end", currentFunc);

    struct ArmInstance {
        Pattern* Pat;
        Expr* Guard;
        Expr* Body;
    };

    std::vector<ArmInstance> instances;
    for (const auto& arm : expr->getArms()) {
        if (!arm.Pat) {
            continue;
        }
        if (auto* orPat = dynamic_cast<OrPattern*>(arm.Pat)) {
            for (auto* alt : orPat->getPatterns()) {
                instances.push_back({alt, arm.Guard, arm.Body});
            }
        } else {
            instances.push_back({arm.Pat, arm.Guard, arm.Body});
        }
    }

    llvm::BasicBlock* nextBB = Builder->GetInsertBlock();

    for (size_t i = 0; i < instances.size(); ++i) {
        Builder->SetInsertPoint(nextBB);

        llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*Context, "match.body", currentFunc);
        llvm::BasicBlock* fallthroughBB = llvm::BasicBlock::Create(*Context, "match.next", currentFunc);

        llvm::Value* currentValue = Builder->CreateLoad(llvmScrutineeType, scrutineeAlloca, "match.val");
        llvm::Value* cond = generatePatternCondition(instances[i].Pat, currentValue, scrutineeType);
        if (!cond) {
            return nullptr;
        }

        if (instances[i].Guard) {
            llvm::BasicBlock* guardBB = llvm::BasicBlock::Create(*Context, "match.guard", currentFunc);
            Builder->CreateCondBr(cond, guardBB, fallthroughBB);

            Builder->SetInsertPoint(guardBB);
            llvm::Value* bindValue = Builder->CreateLoad(llvmScrutineeType, scrutineeAlloca, "match.val");
            if (!bindPattern(instances[i].Pat, bindValue, scrutineeType)) {
                return nullptr;
            }
            llvm::Value* guardValue = generateExpr(instances[i].Guard);
            if (!guardValue) {
                return nullptr;
            }
            Builder->CreateCondBr(guardValue, bodyBB, fallthroughBB);
        } else {
            Builder->CreateCondBr(cond, bodyBB, fallthroughBB);
        }

        Builder->SetInsertPoint(bodyBB);
        if (!instances[i].Guard) {
            llvm::Value* bindValue = Builder->CreateLoad(llvmScrutineeType, scrutineeAlloca, "match.val");
            if (!bindPattern(instances[i].Pat, bindValue, scrutineeType)) {
                return nullptr;
            }
        }

        llvm::Value* bodyValue = generateExpr(instances[i].Body);
        if (!bodyValue) {
            return nullptr;
        }

        Builder->CreateStore(bodyValue, resultAlloca);
        Builder->CreateBr(endBB);

        nextBB = fallthroughBB;
    }

    Builder->SetInsertPoint(nextBB);
    if (!Builder->GetInsertBlock()->getTerminator()) {
        Builder->CreateBr(endBB);
    }

    if (endBB->hasNPredecessorsOrMore(1)) {
        Builder->SetInsertPoint(endBB);
    } else {
        endBB->eraseFromParent();
        llvm::BasicBlock* unreachableBB = llvm::BasicBlock::Create(*Context, "unreachable", currentFunc);
        Builder->SetInsertPoint(unreachableBB);
        Builder->CreateUnreachable();
        return nullptr;
    }

    return Builder->CreateLoad(llvmResultType, resultAlloca, "match.result.load");
}

// ============================================================================
// Error handling expressions
// ============================================================================

llvm::Value* CodeGen::generateErrorPropagateExpr(ErrorPropagateExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // Generate the inner expression (which should return a Result<T, E>)
    Expr* innerExpr = expr->getInner();
    if (auto* propagate = dynamic_cast<ErrorPropagateExpr*>(innerExpr)) {
        innerExpr = propagate->getInner();
    }

    llvm::Value* result = generateExpr(innerExpr);
    if (!result) {
        return nullptr;
    }

    // Get the result type
    Type* innerType = innerExpr->getType();
    if (!innerType || !innerType->isError()) {
        // Error: ! operator used on non-error type
        return nullptr;
    }

    ErrorType* errorType = static_cast<ErrorType*>(innerType);
    Type* successType = errorType->getSuccessType();

    llvm::Type* llvmSuccessType = getLLVMType(successType);
    if (!llvmSuccessType) {
        return nullptr;
    }
    llvmSuccessType = normalizeFirstClassType(llvmSuccessType);
    if (llvmSuccessType->isVoidTy()) {
        llvmSuccessType = llvm::Type::getInt8Ty(*Context);
    }

    auto coerceValue = [&](llvm::Value* value,
                           llvm::Type* targetType,
                           const char* name) -> llvm::Value* {
        if (!value || !targetType) {
            return nullptr;
        }
        if (value->getType() == targetType) {
            return value;
        }
        if (value->getType()->isIntegerTy() && targetType->isIntegerTy()) {
            return Builder->CreateSExtOrTrunc(value, targetType, name);
        }
        if (value->getType()->isPointerTy() && targetType->isPointerTy()) {
            return Builder->CreateBitCast(value, targetType, name);
        }
        if (value->getType()->isPointerTy() && targetType->isIntegerTy()) {
            return Builder->CreatePtrToInt(value, targetType, name);
        }
        if (value->getType()->isIntegerTy() && targetType->isPointerTy()) {
            return Builder->CreateIntToPtr(value, targetType, name);
        }
        if (value->getType()->isFloatingPointTy() && targetType->isFloatingPointTy()) {
            if (value->getType()->getPrimitiveSizeInBits() <
                targetType->getPrimitiveSizeInBits()) {
                return Builder->CreateFPExt(value, targetType, name);
            }
            return Builder->CreateFPTrunc(value, targetType, name);
        }

        // Aggregate/mixed casts must go through memory and require size match.
        const llvm::DataLayout& DL = Module->getDataLayout();
        if (value->getType()->isSized() && targetType->isSized()) {
            uint64_t srcSize = DL.getTypeAllocSize(value->getType());
            uint64_t dstSize = DL.getTypeAllocSize(targetType);
            if (srcSize == dstSize) {
                llvm::AllocaInst* tmp = Builder->CreateAlloca(value->getType(), nullptr, "err.cast.tmp");
                Builder->CreateStore(value, tmp);
                llvm::Value* castPtr = Builder->CreateBitCast(
                    tmp,
                    llvm::PointerType::get(targetType, 0),
                    "err.cast.ptr");
                return Builder->CreateLoad(targetType, castPtr, name);
            }
        }
        return nullptr;
    };

    // Result layout: { i8 tag, ok, err_ptr }.
    llvm::Value* tag = Builder->CreateExtractValue(result, 0, "result.tag");

    // Compare tag with 0 (Ok)
    llvm::Value* isOk = Builder->CreateICmpEQ(
        tag,
        llvm::ConstantInt::get(llvm::Type::getInt8Ty(*Context), 0),
        "is_ok"
    );

    // Create basic blocks for Ok and Err paths
    llvm::Function* currentFunc = Builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* okBB = llvm::BasicBlock::Create(*Context, "ok", currentFunc);
    llvm::BasicBlock* errBB = llvm::BasicBlock::Create(*Context, "err", currentFunc);
    llvm::BasicBlock* contBB = llvm::BasicBlock::Create(*Context, "ok.cont", currentFunc);

    Builder->CreateCondBr(isOk, okBB, errBB);

    // Ok path: extract the value
    Builder->SetInsertPoint(okBB);
    llvm::Value* okValue = Builder->CreateExtractValue(result, 1, "ok.value");
    okValue = coerceValue(okValue, llvmSuccessType, "ok.value.cast");
    if (!okValue) {
        return nullptr;
    }

    // Continue after ok path
    Builder->CreateBr(contBB);

    // Err path: propagate or trap
    Builder->SetInsertPoint(errBB);

    // Execute deferred statements before returning
    executeDeferredStatements(0);

    // 如果当前函数返回 ErrorType，沿调用链传播错误；
    // 否则将 `expr!` 视为强制解包失败并直接终止。
    if (currentFunc->getReturnType() == result->getType()) {
        emitDropForScopeRange(0);
        Builder->CreateRet(result);
    } else {
        llvm::Function* trapFn =
            llvm::Intrinsic::getDeclaration(Module.get(), llvm::Intrinsic::trap);
        Builder->CreateCall(trapFn, {});
        Builder->CreateUnreachable();
    }

    // Continue after ok path
    Builder->SetInsertPoint(contBB);

    return okValue;
}

llvm::Value* CodeGen::generateErrorHandleExpr(ErrorHandleExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // Generate the inner expression (which should return a Result<T, E>)
    Expr* innerExpr = expr->getInner();
    if (auto* propagate = dynamic_cast<ErrorPropagateExpr*>(innerExpr)) {
        innerExpr = propagate->getInner();
    }

    llvm::Value* result = generateExpr(innerExpr);
    if (!result) {
        return nullptr;
    }

    // Get the result type
    Type* innerType = innerExpr->getType();
    if (!innerType || !innerType->isError()) {
        // Error: -> err used on non-error type
        return nullptr;
    }

    ErrorType* errorType = static_cast<ErrorType*>(innerType);
    Type* successType = errorType->getSuccessType();

    llvm::Type* llvmSuccessType = getLLVMType(successType);
    if (!llvmSuccessType) {
        return nullptr;
    }
    llvmSuccessType = normalizeFirstClassType(llvmSuccessType);
    if (llvmSuccessType->isVoidTy()) {
        llvmSuccessType = llvm::Type::getInt8Ty(*Context);
    }

    auto coerceValue = [&](llvm::Value* value,
                           llvm::Type* targetType,
                           const char* name) -> llvm::Value* {
        if (!value || !targetType) {
            return nullptr;
        }
        if (value->getType() == targetType) {
            return value;
        }
        if (value->getType()->isIntegerTy() && targetType->isIntegerTy()) {
            return Builder->CreateSExtOrTrunc(value, targetType, name);
        }
        if (value->getType()->isPointerTy() && targetType->isPointerTy()) {
            return Builder->CreateBitCast(value, targetType, name);
        }
        if (value->getType()->isPointerTy() && targetType->isIntegerTy()) {
            return Builder->CreatePtrToInt(value, targetType, name);
        }
        if (value->getType()->isIntegerTy() && targetType->isPointerTy()) {
            return Builder->CreateIntToPtr(value, targetType, name);
        }
        if (value->getType()->isFloatingPointTy() && targetType->isFloatingPointTy()) {
            if (value->getType()->getPrimitiveSizeInBits() <
                targetType->getPrimitiveSizeInBits()) {
                return Builder->CreateFPExt(value, targetType, name);
            }
            return Builder->CreateFPTrunc(value, targetType, name);
        }

        const llvm::DataLayout& DL = Module->getDataLayout();
        if (value->getType()->isSized() && targetType->isSized()) {
            uint64_t srcSize = DL.getTypeAllocSize(value->getType());
            uint64_t dstSize = DL.getTypeAllocSize(targetType);
            if (srcSize == dstSize) {
                llvm::AllocaInst* tmp = Builder->CreateAlloca(value->getType(), nullptr, "err.cast.tmp");
                Builder->CreateStore(value, tmp);
                llvm::Value* castPtr = Builder->CreateBitCast(
                    tmp,
                    llvm::PointerType::get(targetType, 0),
                    "err.cast.ptr");
                return Builder->CreateLoad(targetType, castPtr, name);
            }
        }
        return nullptr;
    };

    // Result layout: { i8 tag, ok, err_ptr }.
    llvm::Value* tag = Builder->CreateExtractValue(result, 0, "result.tag");

    // Compare tag with 0 (Ok)
    llvm::Value* isOk = Builder->CreateICmpEQ(
        tag,
        llvm::ConstantInt::get(llvm::Type::getInt8Ty(*Context), 0),
        "is_ok"
    );

    // Create basic blocks for Ok and Err paths
    llvm::Function* currentFunc = Builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* okBB = llvm::BasicBlock::Create(*Context, "ok", currentFunc);
    llvm::BasicBlock* errBB = llvm::BasicBlock::Create(*Context, "err_handle", currentFunc);
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*Context, "merge", currentFunc);

    Builder->CreateCondBr(isOk, okBB, errBB);

    // Ok path: extract and return the value
    Builder->SetInsertPoint(okBB);
    llvm::Value* okValue = Builder->CreateExtractValue(result, 1, "ok.value");
    okValue = coerceValue(okValue, llvmSuccessType, "ok.value.cast");
    if (!okValue) {
        return nullptr;
    }

    Builder->CreateBr(mergeBB);

    // Err path: execute the error handler
    Builder->SetInsertPoint(errBB);

    // Bind the error value to the error variable
    const std::string& errorVarName = expr->getErrorVar();
    if (!errorVarName.empty()) {
        // Extract the error payload pointer (index 2).
        llvm::Value* errorData = Builder->CreateExtractValue(result, 2, "err.data");

        Type* errSemType = nullptr;
        if (VarDecl* errDecl = expr->getErrorVarDecl()) {
            errSemType = errDecl->getSemanticType();
        }

        if (errSemType) {
            llvm::Type* errLLVMType = getLLVMType(errSemType);
            if (errLLVMType) {
                llvm::Type* i8PtrType = llvm::PointerType::get(llvm::Type::getInt8Ty(*Context), 0);
                llvm::Value* errorPtr = errorData;
                if (errorPtr->getType() != i8PtrType) {
                    errorPtr = coerceValue(errorPtr, i8PtrType, "err.ptr");
                    if (!errorPtr) {
                        return nullptr;
                    }
                }

                if (!errLLVMType->isPointerTy()) {
                    llvm::Value* typedPtr = Builder->CreateBitCast(
                        errorPtr,
                        llvm::PointerType::get(errLLVMType, 0),
                        "err.payload.ptr"
                    );
                    llvm::Value* errValue = Builder->CreateLoad(errLLVMType, typedPtr, "err.payload");
                    llvm::AllocaInst* errorAlloca = Builder->CreateAlloca(errLLVMType, nullptr, errorVarName);
                    Builder->CreateStore(errValue, errorAlloca);
                    if (VarDecl* errDecl = expr->getErrorVarDecl()) {
                        ValueMap[errDecl] = errorAlloca;
                    }
                    goto err_bound;
                }
            }
        }

        // Fallback: create a string value { i8*, i64 } for the error variable
        {
            llvm::Type* i8PtrType = llvm::PointerType::get(llvm::Type::getInt8Ty(*Context), 0);
            llvm::Type* i64Type = llvm::Type::getInt64Ty(*Context);
            llvm::StructType* strType = llvm::StructType::get(*Context, {i8PtrType, i64Type});

            llvm::Value* errorPtr = errorData;
            if (errorPtr->getType() != i8PtrType) {
                errorPtr = coerceValue(errorPtr, i8PtrType, "err.ptr");
                if (!errorPtr) {
                    return nullptr;
                }
            }

            llvm::Value* errStr = llvm::UndefValue::get(strType);
            errStr = Builder->CreateInsertValue(errStr, errorPtr, 0, "err.str.ptr");
            errStr = Builder->CreateInsertValue(errStr, llvm::ConstantInt::get(i64Type, 0), 1, "err.str.len");

            llvm::AllocaInst* errorAlloca = Builder->CreateAlloca(strType, nullptr, errorVarName);
            Builder->CreateStore(errStr, errorAlloca);

            if (VarDecl* errDecl = expr->getErrorVarDecl()) {
                ValueMap[errDecl] = errorAlloca;
            }
        }
err_bound:
        (void)0;
    }

    // Generate the error handler block and capture last expression value
    llvm::Value* handlerValue = generateBlockStmtWithResult(expr->getHandler());

    // If the handler doesn't have a terminator, branch to merge
    llvm::Value* errValue = nullptr;
    if (!Builder->GetInsertBlock()->getTerminator()) {
        errValue = handlerValue;
        if (!errValue) {
            errValue = llvm::Constant::getNullValue(llvmSuccessType);
        } else if (errValue->getType() != llvmSuccessType) {
            errValue = coerceValue(errValue, llvmSuccessType, "err.value.cast");
            if (!errValue) {
                return nullptr;
            }
        }
        Builder->CreateBr(mergeBB);
    }
    llvm::BasicBlock* errEndBB = Builder->GetInsertBlock();

    // Merge block: PHI node to select between ok value and handler result
    Builder->SetInsertPoint(mergeBB);

    // Create PHI node to merge the results
    llvm::PHINode* phi = Builder->CreatePHI(llvmSuccessType, 2, "result");
    phi->addIncoming(okValue, okBB);

    if (errValue) {
        phi->addIncoming(errValue, errEndBB);
    } else {
        // Handler terminated (returned), so no incoming value from err path
        // This is fine, the PHI will only have one incoming edge
    }

    return phi;
}

// ============================================================================
// Helper methods for literal generation
// ============================================================================

llvm::Value* CodeGen::generateLiteralExpr(Expr* expr) {
    if (!expr) {
        return nullptr;
    }

    switch (expr->getKind()) {
        case ASTNode::Kind::IntegerLiteralExpr:
            return generateIntegerLiteral(static_cast<IntegerLiteralExpr*>(expr));

        case ASTNode::Kind::FloatLiteralExpr:
            return generateFloatLiteral(static_cast<FloatLiteralExpr*>(expr));

        case ASTNode::Kind::BoolLiteralExpr:
            return generateBoolLiteral(static_cast<BoolLiteralExpr*>(expr));

        case ASTNode::Kind::CharLiteralExpr:
            return generateCharLiteral(static_cast<CharLiteralExpr*>(expr));

        case ASTNode::Kind::StringLiteralExpr:
            return generateStringLiteral(static_cast<StringLiteralExpr*>(expr));

        default:
            return nullptr;
    }
}

// ============================================================================
// Builtin function calls
// ============================================================================

llvm::Value* CodeGen::generateBuiltinCallExpr(BuiltinCallExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // Get the builtin handler from registry
    BuiltinHandler* handler = BuiltinRegistry::instance().getHandler(expr->getBuiltinKind());
    if (!handler) {
        return nullptr;
    }

    // Call the handler's generate method
    return handler->generate(expr, *this);
}

// ============================================================================
// Range expression
// ============================================================================

llvm::Value* CodeGen::generateRangeExpr(RangeExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // 获取 Range 表达式的类型
    Type* rangeType = expr->getType();
    if (!rangeType || !rangeType->isRange()) {
        return nullptr;
    }

    auto* rangeTy = static_cast<RangeType*>(rangeType);
    Type* elementType = rangeTy->getElementType();

    // 获取 LLVM 类型
    llvm::Type* llvmRangeType = getLLVMType(rangeType);
    llvm::Type* llvmElementType = getLLVMType(elementType);
    if (!llvmRangeType || !llvmElementType) {
        return nullptr;
    }

    // 生成起始值和结束值
    llvm::Value* startValue = nullptr;
    llvm::Value* endValue = nullptr;

    if (expr->hasStart()) {
        startValue = generateExpr(expr->getStart());
        if (!startValue) {
            return nullptr;
        }
    } else {
        // 如果没有起始值，使用类型的最小值
        // 对于整数类型，使用 0
        if (elementType->isInteger()) {
            auto* intType = static_cast<IntegerType*>(elementType);
            if (intType->isSigned()) {
                // 有符号整数：使用最小值
                int64_t minVal = -(1LL << (intType->getBitWidth() - 1));
                startValue = llvm::ConstantInt::get(llvmElementType, minVal, true);
            } else {
                // 无符号整数：使用 0
                startValue = llvm::ConstantInt::get(llvmElementType, 0, false);
            }
        } else {
            return nullptr; // 不支持的类型
        }
    }

    if (expr->hasEnd()) {
        endValue = generateExpr(expr->getEnd());
        if (!endValue) {
            return nullptr;
        }
    } else {
        // 如果没有结束值，使用类型的最大值
        if (elementType->isInteger()) {
            auto* intType = static_cast<IntegerType*>(elementType);
            if (intType->isSigned()) {
                // 有符号整数：使用最大值
                int64_t maxVal = (1LL << (intType->getBitWidth() - 1)) - 1;
                endValue = llvm::ConstantInt::get(llvmElementType, maxVal, true);
            } else {
                // 无符号整数：使用最大值
                uint64_t maxVal = (1ULL << intType->getBitWidth()) - 1;
                endValue = llvm::ConstantInt::get(llvmElementType, maxVal, false);
            }
        } else {
            return nullptr; // 不支持的类型
        }
    }

    // 创建 Range 结构体
    // Range 结构体布局: { T start, T end, i1 inclusive }
    llvm::Value* rangeStruct = llvm::UndefValue::get(llvmRangeType);

    // 设置 start 字段 (索引 0)
    rangeStruct = Builder->CreateInsertValue(rangeStruct, startValue, 0, "range.start");

    // 设置 end 字段 (索引 1)
    rangeStruct = Builder->CreateInsertValue(rangeStruct, endValue, 1, "range.end");

    // 设置 inclusive 字段 (索引 2)
    llvm::Value* inclusiveValue = llvm::ConstantInt::get(
        llvm::Type::getInt1Ty(*Context),
        expr->isInclusive() ? 1 : 0
    );
    rangeStruct = Builder->CreateInsertValue(rangeStruct, inclusiveValue, 2, "range.inclusive");

    return rangeStruct;
}

// ============================================================================
// VarArgs/Value helpers
// ============================================================================

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

llvm::Value* CodeGen::buildValueFrom(Type* type, llvm::Value* value, Type* expectedElementType) {
    if (!type || !value) {
        return nullptr;
    }

    // 解引用引用类型
    bool wasReference = false;
    if (type->isReference()) {
        type = static_cast<ReferenceType*>(type)->getPointeeType();
        wasReference = true;
    }

    if (wasReference && value->getType()->isPointerTy()) {
        llvm::Type* llvmPointee = getLLVMType(type);
        if (!llvmPointee) {
            return nullptr;
        }
        value = Builder->CreateLoad(llvmPointee, value, "value.autoderef");
    }

    Type* resolvedType = substituteType(type);
    Type* baseType = unwrapDisplayBaseType(resolvedType ? resolvedType : type);

    if (resolvedType && resolvedType->isValue()) {
        return value;
    }

    llvm::Type* valueTy = getLLVMType(Ctx.getValueType());
    if (!valueTy) {
        return nullptr;
    }

    llvm::Type* i32Ty = llvm::Type::getInt32Ty(*Context);
    llvm::Type* i64Ty = llvm::Type::getInt64Ty(*Context);

    enum Tag {
        TAG_STRING = 0,
        TAG_I32 = 1,
        TAG_I64 = 2,
        TAG_F32 = 3,
        TAG_F64 = 4,
        TAG_BOOL = 5,
        TAG_CHAR = 6,
    };

    llvm::Value* tagValue = llvm::ConstantInt::get(i32Ty, TAG_I64);
    llvm::Value* data0 = llvm::ConstantInt::get(i64Ty, 0);
    llvm::Value* data1 = llvm::ConstantInt::get(i64Ty, 0);
    bool supported = true;

    if (resolvedType && resolvedType->isString()) {
        value = coerceGenericValue(value, resolvedType);
        tagValue = llvm::ConstantInt::get(i32Ty, TAG_STRING);
        llvm::Value* strPtr = nullptr;
        llvm::Value* strLen = nullptr;
        if (value->getType()->isStructTy()) {
            strPtr = Builder->CreateExtractValue(value, 0, "value.str.ptr");
            strLen = Builder->CreateExtractValue(value, 1, "value.str.len");
        } else {
            strPtr = value;
            strLen = llvm::ConstantInt::get(i64Ty, 0);
        }
        if (!strLen->getType()->isIntegerTy(64)) {
            strLen = Builder->CreateSExtOrTrunc(strLen, i64Ty, "value.str.len64");
        }
        data0 = Builder->CreatePtrToInt(strPtr, i64Ty, "value.str.ptr.int");
        data1 = strLen;
    } else if (resolvedType && resolvedType->isInteger()) {
        value = coerceGenericValue(value, resolvedType);
        auto* intType = static_cast<IntegerType*>(resolvedType);
        if (intType->getBitWidth() <= 32) {
            tagValue = llvm::ConstantInt::get(i32Ty, TAG_I32);
            data0 = intType->isSigned()
                ? Builder->CreateSExt(value, i64Ty, "value.i32.sext")
                : Builder->CreateZExt(value, i64Ty, "value.i32.zext");
        } else {
            tagValue = llvm::ConstantInt::get(i32Ty, TAG_I64);
            data0 = intType->isSigned()
                ? Builder->CreateSExtOrTrunc(value, i64Ty, "value.i64.sext")
                : Builder->CreateZExtOrTrunc(value, i64Ty, "value.i64.zext");
        }
    } else if (resolvedType && resolvedType->isFloat()) {
        value = coerceGenericValue(value, resolvedType);
        auto* floatType = static_cast<FloatType*>(resolvedType);
        llvm::Type* doubleTy = llvm::Type::getDoubleTy(*Context);
        llvm::Value* doubleVal = value;
        if (value->getType()->isFloatTy()) {
            doubleVal = Builder->CreateFPExt(value, doubleTy, "value.f32.to.f64");
        }
        data0 = Builder->CreateBitCast(doubleVal, i64Ty, "value.float.bits");
        tagValue = llvm::ConstantInt::get(
            i32Ty,
            floatType->getBitWidth() == 32 ? TAG_F32 : TAG_F64
        );
    } else if (resolvedType && resolvedType->isBool()) {
        value = coerceGenericValue(value, resolvedType);
        tagValue = llvm::ConstantInt::get(i32Ty, TAG_BOOL);
        data0 = Builder->CreateZExt(value, i64Ty, "value.bool");
    } else if (resolvedType && resolvedType->isChar()) {
        value = coerceGenericValue(value, resolvedType);
        tagValue = llvm::ConstantInt::get(i32Ty, TAG_CHAR);
        data0 = Builder->CreateZExt(value, i64Ty, "value.char");
    } else {
        supported = false;
    }

    // 支持通过 Display/Debug 将结构体/枚举转为字符串（仅用于 VarArgs<Value>）
    if (!supported && baseType && (baseType->isStruct() || baseType->isEnum())) {
        bool allowDisplay = !expectedElementType || expectedElementType->isValue();
        if (allowDisplay) {
            FuncDecl* method = Ctx.getDisplayImpl(baseType);
            if (!method) {
                method = Ctx.getDebugImpl(baseType);
            }

            llvm::Function* specializedFunc = nullptr;
            if (method) {
                Type* semaType = method->getSemanticType();
                if (semaType && semaType->isFunction()) {
                    auto* funcType = static_cast<FunctionType*>(semaType);
                    if (funcType->getParamCount() > 0) {
                        Type* selfType = funcType->getParam(0);
                        Type* actualTypeForMap = type;
                        if (selfType && actualTypeForMap) {
                            if (selfType->isReference() && !actualTypeForMap->isReference()) {
                                auto* ref = static_cast<ReferenceType*>(selfType);
                                actualTypeForMap = Ctx.getReferenceType(actualTypeForMap, ref->isMutable());
                            } else if (selfType->isPointer() && !actualTypeForMap->isPointer()) {
                                auto* ptr = static_cast<PointerType*>(selfType);
                                actualTypeForMap = Ctx.getPointerType(actualTypeForMap, ptr->isMutable());
                            }
                            GenericSubst mapping;
                            bool unified = unifyGenericTypes(selfType, actualTypeForMap, mapping);
                            if (!unified || mapping.empty()) {
                                auto tryCollect = [&](auto&& self, Type* expected, Type* actual) -> void {
                                    if (!expected || !actual) {
                                        return;
                                    }
                                    if (expected->isReference() && actual->isReference()) {
                                        auto* expRef = static_cast<ReferenceType*>(expected);
                                        auto* actRef = static_cast<ReferenceType*>(actual);
                                        self(self, expRef->getPointeeType(), actRef->getPointeeType());
                                        return;
                                    }
                                    if (expected->isPointer() && actual->isPointer()) {
                                        auto* expPtr = static_cast<PointerType*>(expected);
                                        auto* actPtr = static_cast<PointerType*>(actual);
                                        self(self, expPtr->getPointeeType(), actPtr->getPointeeType());
                                        return;
                                    }
                                    if (expected->isGenericInstance() && actual->isGenericInstance()) {
                                        auto* expInst = static_cast<GenericInstanceType*>(expected);
                                        auto* actInst = static_cast<GenericInstanceType*>(actual);
                                        if (!expInst->getBaseType()->isEqual(actInst->getBaseType()) ||
                                            expInst->getTypeArgCount() != actInst->getTypeArgCount()) {
                                            return;
                                        }
                                        for (size_t i = 0; i < expInst->getTypeArgCount(); ++i) {
                                            Type* expArg = expInst->getTypeArg(i);
                                            Type* actArg = actInst->getTypeArg(i);
                                            if (!expArg || !actArg) {
                                                continue;
                                            }
                                            if (expArg->isGeneric()) {
                                                auto* gen = static_cast<GenericType*>(expArg);
                                                mapping[gen->getName()] = actArg;
                                            } else if (expArg->isTypeVar()) {
                                                auto* tv = static_cast<TypeVariable*>(expArg);
                                                mapping["#tv" + std::to_string(tv->getID())] = actArg;
                                            } else if (expArg->isGenericInstance()) {
                                                self(self, expArg, actArg);
                                            }
                                        }
                                    }
                                };
                                tryCollect(tryCollect, selfType, actualTypeForMap);
                            }
                            if (mapping.empty() && actualTypeForMap && actualTypeForMap->isGenericInstance()) {
                                auto* genInst = static_cast<GenericInstanceType*>(actualTypeForMap);
                                if (Type* base = genInst->getBaseType()) {
                                    if (base->isStruct()) {
                                        buildStructGenericMapping(
                                            static_cast<StructType*>(base),
                                            genInst,
                                            mapping
                                        );
                                    }
                                }
                            }
                            if (!mapping.empty()) {
                                specializedFunc = getOrCreateSpecializedFunction(method, mapping);
                            }
                        }
                    }
                }
            }

            auto callDisplay = [&](FuncDecl* displayMethod) -> llvm::Value* {
                if (!displayMethod) {
                    return nullptr;
                }
                Type* semaType = displayMethod->getSemanticType();
                if (!semaType || !semaType->isFunction()) {
                    return nullptr;
                }
                auto* funcType = static_cast<FunctionType*>(semaType);
                if (funcType->getParamCount() == 0) {
                    return nullptr;
                }
                Type* selfType = funcType->getParam(0);
                llvm::Type* llvmSelfType = getLLVMType(selfType);
                if (!llvmSelfType) {
                    return nullptr;
                }

                llvm::Value* selfArg = value;
                if (selfType->isReference() || selfType->isPointer()) {
                    if (!value->getType()->isPointerTy()) {
                        llvm::AllocaInst* tempAlloca = Builder->CreateAlloca(
                            value->getType(),
                            nullptr,
                            "display.self"
                        );
                        Builder->CreateStore(value, tempAlloca);
                        selfArg = tempAlloca;
                    }
                    if (selfArg->getType() != llvmSelfType) {
                        selfArg = Builder->CreateBitCast(selfArg, llvmSelfType, "display.self.cast");
                    }
                } else {
                    if (value->getType()->isPointerTy()) {
                        selfArg = Builder->CreateLoad(llvmSelfType, value, "display.self.load");
                    } else if (value->getType() != llvmSelfType) {
                        selfArg = Builder->CreateBitCast(value, llvmSelfType, "display.self.cast");
                    }
                }

                llvm::Function* func = specializedFunc;
                if (!func) {
                    std::string funcName = getFunctionSymbolName(displayMethod);
                    func = Module->getFunction(funcName);
                    if (!func) {
                        llvm::Type* funcLLVMType = getLLVMType(funcType);
                        auto* llvmFuncType = llvm::dyn_cast<llvm::FunctionType>(funcLLVMType);
                        if (!llvmFuncType) {
                            return nullptr;
                        }
                        func = llvm::Function::Create(
                            llvmFuncType,
                            llvm::Function::ExternalLinkage,
                            funcName,
                            Module.get()
                        );
                    }
                }

                return Builder->CreateCall(func, {selfArg}, "display.call");
            };

            llvm::Value* strValue = callDisplay(method);

            if (strValue) {
                tagValue = llvm::ConstantInt::get(i32Ty, TAG_STRING);
                llvm::Value* strPtr = nullptr;
                llvm::Value* strLen = nullptr;
                if (strValue->getType()->isStructTy()) {
                    strPtr = Builder->CreateExtractValue(strValue, 0, "display.str.ptr");
                    strLen = Builder->CreateExtractValue(strValue, 1, "display.str.len");
                } else if (strValue->getType()->isPointerTy()) {
                    strPtr = strValue;
                    strLen = llvm::ConstantInt::get(i64Ty, 0);
                }

                if (strPtr && strLen) {
                    if (!strLen->getType()->isIntegerTy(64)) {
                        strLen = Builder->CreateSExtOrTrunc(strLen, i64Ty, "display.str.len64");
                    }
                    data0 = Builder->CreatePtrToInt(strPtr, i64Ty, "display.str.ptr.int");
                    data1 = strLen;
                    supported = true;
                }
            }
        }
    }

    if (!supported) {
        return nullptr;
    }

    llvm::Value* result = llvm::UndefValue::get(valueTy);
    result = Builder->CreateInsertValue(result, tagValue, 0, "value.tag");
    result = Builder->CreateInsertValue(result, llvm::ConstantInt::get(i32Ty, 0), 1, "value.pad");
    result = Builder->CreateInsertValue(result, data0, 2, "value.data0");
    result = Builder->CreateInsertValue(result, data1, 3, "value.data1");
    return result;
}

llvm::Value* CodeGen::convertValueToType(llvm::Value* value, Type* targetType) {
    if (!value || !targetType) {
        return nullptr;
    }

    if (targetType->isReference()) {
        targetType = static_cast<ReferenceType*>(targetType)->getPointeeType();
    }

    if (targetType->isValue()) {
        return value;
    }

    llvm::Type* i64Ty = llvm::Type::getInt64Ty(*Context);
    llvm::Value* data0 = Builder->CreateExtractValue(value, 2, "value.data0");
    llvm::Value* data1 = Builder->CreateExtractValue(value, 3, "value.data1");

    if (targetType->isString()) {
        llvm::Type* strTy = getLLVMType(targetType);
        if (!strTy) {
            return nullptr;
        }
        llvm::Value* ptr = Builder->CreateIntToPtr(
            data0,
            llvm::PointerType::get(llvm::Type::getInt8Ty(*Context), 0),
            "value.str.ptr"
        );
        llvm::Value* len = data1;
        if (!len->getType()->isIntegerTy(64)) {
            len = Builder->CreateSExtOrTrunc(len, i64Ty, "value.str.len64");
        }
        llvm::Value* result = llvm::UndefValue::get(strTy);
        result = Builder->CreateInsertValue(result, ptr, 0, "value.str.ptr");
        result = Builder->CreateInsertValue(result, len, 1, "value.str.len");
        return result;
    }

    if (targetType->isInteger()) {
        auto* intType = static_cast<IntegerType*>(targetType);
        llvm::Type* llvmInt = getLLVMType(targetType);
        if (!llvmInt) {
            return nullptr;
        }
        if (intType->isSigned()) {
            return Builder->CreateTruncOrBitCast(data0, llvmInt, "value.int");
        }
        return Builder->CreateTruncOrBitCast(data0, llvmInt, "value.uint");
    }

    if (targetType->isFloat()) {
        llvm::Type* doubleTy = llvm::Type::getDoubleTy(*Context);
        llvm::Value* doubleVal = Builder->CreateBitCast(data0, doubleTy, "value.float");
        if (targetType->getKind() == Type::Kind::Float) {
            auto* floatType = static_cast<FloatType*>(targetType);
            if (floatType->getBitWidth() == 32) {
                return Builder->CreateFPTrunc(doubleVal, llvm::Type::getFloatTy(*Context), "value.f32");
            }
        }
        return doubleVal;
    }

    if (targetType->isBool()) {
        return Builder->CreateTrunc(data0, llvm::Type::getInt1Ty(*Context), "value.bool");
    }

    if (targetType->isChar()) {
        return Builder->CreateTrunc(data0, llvm::Type::getInt8Ty(*Context), "value.char");
    }

    return nullptr;
}

llvm::Value* CodeGen::callVarArgsGet(llvm::Value* varArgsValue, llvm::Value* index) {
    if (!varArgsValue || !index) {
        return nullptr;
    }

    llvm::Type* varArgsStructTy = getLLVMType(Ctx.getVarArgsType(Ctx.getValueType()));
    if (!varArgsStructTy) {
        return nullptr;
    }

    if (varArgsValue->getType()->isPointerTy()) {
        llvm::Type* expectedPtrTy = llvm::PointerType::get(varArgsStructTy, 0);
        if (varArgsValue->getType() != expectedPtrTy) {
            varArgsValue = Builder->CreateBitCast(varArgsValue, expectedPtrTy, "varargs.ptr.cast");
        }
        varArgsValue = Builder->CreateLoad(varArgsStructTy, varArgsValue, "varargs.load");
    }

    llvm::Type* i64Ty = llvm::Type::getInt64Ty(*Context);
    if (!index->getType()->isIntegerTy(64)) {
        index = Builder->CreateSExtOrTrunc(index, i64Ty, "varargs.idx64");
    }

    llvm::Type* valueTy = getLLVMType(Ctx.getValueType());
    if (!valueTy) {
        return nullptr;
    }

    llvm::Type* valuePtrTy = llvm::PointerType::get(valueTy, 0);
    llvm::Value* lenValue = Builder->CreateExtractValue(varArgsValue, 0, "varargs.len");
    llvm::Value* valuesPtr = Builder->CreateExtractValue(varArgsValue, 1, "varargs.ptr");
    if (valuesPtr->getType() != valuePtrTy) {
        valuesPtr = Builder->CreateBitCast(valuesPtr, valuePtrTy, "varargs.values.cast");
    }
    llvm::Function* func = Module->getFunction("yuan_varargs_get");
    if (!func) {
        llvm::FunctionType* fnType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(*Context),
            {valuePtrTy, i64Ty, valuePtrTy, i64Ty},
            false
        );
        func = llvm::Function::Create(
            fnType,
            llvm::Function::ExternalLinkage,
            "yuan_varargs_get",
            Module.get()
        );
        func->addParamAttr(0, llvm::Attribute::StructRet);
    }

    llvm::AllocaInst* resultAlloca = Builder->CreateAlloca(valueTy, nullptr, "varargs.result");
    Builder->CreateCall(func, {resultAlloca, lenValue, valuesPtr, index});
    return Builder->CreateLoad(valueTy, resultAlloca, "varargs.get");
}

} // namespace yuan
