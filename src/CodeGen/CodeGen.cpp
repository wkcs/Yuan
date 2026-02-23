/// \file CodeGen.cpp
/// \brief Implementation of LLVM IR code generation.

#include "yuan/CodeGen/CodeGen.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/Expr.h"
#include "yuan/Sema/Type.h"
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/TargetParser/Host.h>
#include <system_error>
#include <cctype>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <cstdint>
#include <unordered_set>

namespace yuan {

namespace {

constexpr char kHexDigits[] = "0123456789abcdef";

static void appendHexByte(std::string& out, unsigned char value) {
    out.push_back(kHexDigits[(value >> 4) & 0x0F]);
    out.push_back(kHexDigits[value & 0x0F]);
}

static std::string hexEncode(const std::string& text) {
    std::string out;
    out.reserve(text.size() * 2);
    for (unsigned char ch : text) {
        appendHexByte(out, ch);
    }
    return out;
}

static std::string hexEncodeU64(uint64_t value) {
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i) {
        out[static_cast<size_t>(i)] = kHexDigits[value & 0x0F];
        value >>= 4;
    }
    return out;
}

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

static void collectGenericNames(Type* type,
                                std::vector<std::string>& out,
                                std::unordered_set<std::string>& seen) {
    if (!type) {
        return;
    }

    if (type->isGeneric()) {
        auto* genericTy = static_cast<GenericType*>(type);
        const std::string& name = genericTy->getName();
        if (seen.insert(name).second) {
            out.push_back(name);
        }
        return;
    }

    if (type->isGenericInstance()) {
        auto* inst = static_cast<GenericInstanceType*>(type);
        collectGenericNames(inst->getBaseType(), out, seen);
        for (Type* arg : inst->getTypeArgs()) {
            collectGenericNames(arg, out, seen);
        }
        return;
    }

    if (type->isPointer()) {
        collectGenericNames(static_cast<PointerType*>(type)->getPointeeType(), out, seen);
        return;
    }
    if (type->isReference()) {
        collectGenericNames(static_cast<ReferenceType*>(type)->getPointeeType(), out, seen);
        return;
    }
    if (type->isOptional()) {
        collectGenericNames(static_cast<OptionalType*>(type)->getInnerType(), out, seen);
        return;
    }
    if (type->isArray()) {
        collectGenericNames(static_cast<ArrayType*>(type)->getElementType(), out, seen);
        return;
    }
    if (type->isSlice()) {
        collectGenericNames(static_cast<SliceType*>(type)->getElementType(), out, seen);
        return;
    }
    if (type->isTuple()) {
        auto* tuple = static_cast<TupleType*>(type);
        for (size_t i = 0; i < tuple->getElementCount(); ++i) {
            collectGenericNames(tuple->getElement(i), out, seen);
        }
        return;
    }
    if (type->isFunction()) {
        auto* fn = static_cast<FunctionType*>(type);
        for (Type* p : fn->getParamTypes()) {
            collectGenericNames(p, out, seen);
        }
        collectGenericNames(fn->getReturnType(), out, seen);
        return;
    }
    if (type->isError()) {
        collectGenericNames(static_cast<ErrorType*>(type)->getSuccessType(), out, seen);
        return;
    }
    if (type->isRange()) {
        collectGenericNames(static_cast<RangeType*>(type)->getElementType(), out, seen);
        return;
    }
    if (type->isTypeAlias()) {
        collectGenericNames(static_cast<TypeAlias*>(type)->getAliasedType(), out, seen);
        return;
    }
}

static std::vector<std::string> inferStructGenericParams(const StructType* structType) {
    std::vector<std::string> params;
    if (!structType) {
        return params;
    }

    std::unordered_set<std::string> seen;
    for (const auto& field : structType->getFields()) {
        collectGenericNames(field.FieldType, params, seen);
    }
    return params;
}

static std::vector<std::string> inferEnumGenericParams(const EnumType* enumType) {
    std::vector<std::string> params;
    if (!enumType) {
        return params;
    }

    std::unordered_set<std::string> seen;
    for (const auto& variant : enumType->getVariants()) {
        for (Type* payloadTy : variant.Data) {
            collectGenericNames(payloadTy, params, seen);
        }
    }
    return params;
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

static Type* unwrapAliases(Type* type) {
    Type* current = type;
    while (current && current->isTypeAlias()) {
        current = static_cast<TypeAlias*>(current)->getAliasedType();
    }
    return current;
}

} // namespace

// ============================================================================
// Constructor and Destructor
// ============================================================================

CodeGen::CodeGen(ASTContext& ctx, const std::string& moduleName)
    : Ctx(ctx),
      Context(std::make_unique<llvm::LLVMContext>()),
      Module(std::make_unique<llvm::Module>(moduleName, *Context)),
      Builder(std::make_unique<llvm::IRBuilder<>>(*Context)) {
}

CodeGen::~CodeGen() = default;

bool CodeGen::typeNeedsAutoDrop(Type* type, FuncDecl** dropMethod) const {
    type = unwrapAliases(type);
    if (!type) {
        return false;
    }

    FuncDecl* method = Ctx.getImplMethod(type, "drop");
    if (!method) {
        return false;
    }
    if (method->getParams().empty()) {
        return false;
    }
    ParamDecl* selfParam = method->getParams()[0];
    if (!selfParam || !selfParam->isSelf() ||
        selfParam->getParamKind() != ParamDecl::ParamKind::MutRefSelf) {
        return false;
    }

    Type* methodType = method->getSemanticType();
    if (!methodType || !methodType->isFunction()) {
        return false;
    }
    auto* fnType = static_cast<FunctionType*>(methodType);
    if (!fnType->getReturnType() || !fnType->getReturnType()->isVoid()) {
        return false;
    }

    if (dropMethod) {
        *dropMethod = method;
    }
    return true;
}

void CodeGen::beginDropScope() {
    DropScopeStack.emplace_back();
}

void CodeGen::endDropScope(bool emitDrops) {
    if (DropScopeStack.empty()) {
        return;
    }
    size_t idx = DropScopeStack.size() - 1;
    if (emitDrops) {
        emitDropForScope(idx);
    }
    for (const Decl* decl : DropScopeStack[idx]) {
        DropLocals.erase(decl);
    }
    DropScopeStack.pop_back();
}

void CodeGen::registerDropLocal(const Decl* decl, llvm::Value* storage, Type* type,
                                bool isInitialized) {
    if (!decl || !storage || !type || !CurrentFunction) {
        return;
    }

    FuncDecl* dropMethod = nullptr;
    if (!typeNeedsAutoDrop(type, &dropMethod) || !dropMethod) {
        return;
    }

    llvm::IRBuilder<> entryBuilder(&CurrentFunction->getEntryBlock(),
                                   CurrentFunction->getEntryBlock().begin());
    llvm::AllocaInst* flagAlloca = entryBuilder.CreateAlloca(
        llvm::Type::getInt1Ty(*Context), nullptr,
        getGlobalSymbolName(decl, "drop_flag", 'V'));
    entryBuilder.CreateStore(
        llvm::ConstantInt::get(llvm::Type::getInt1Ty(*Context), isInitialized ? 1 : 0),
        flagAlloca);

    DropLocals[decl] = DropLocalInfo{storage, flagAlloca, type, dropMethod};
    if (!DropScopeStack.empty()) {
        DropScopeStack.back().push_back(decl);
    }
}

void CodeGen::setDropFlag(const Decl* decl, bool live) {
    auto it = DropLocals.find(decl);
    if (it == DropLocals.end() || !it->second.DropFlag ||
        !Builder || !Builder->GetInsertBlock() || Builder->GetInsertBlock()->getTerminator()) {
        return;
    }
    Builder->CreateStore(
        llvm::ConstantInt::get(llvm::Type::getInt1Ty(*Context), live ? 1 : 0),
        it->second.DropFlag);
}

void CodeGen::emitDropForDecl(const Decl* decl) {
    auto it = DropLocals.find(decl);
    if (it == DropLocals.end()) {
        return;
    }
    DropLocalInfo& info = it->second;
    if (!info.Storage || !info.DropFlag || !info.DropMethod ||
        !Builder || !Builder->GetInsertBlock() || Builder->GetInsertBlock()->getTerminator()) {
        return;
    }

    llvm::Value* shouldDrop = Builder->CreateLoad(
        llvm::Type::getInt1Ty(*Context), info.DropFlag, "drop.flag");
    llvm::Function* currentFunc = Builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* dropBB = llvm::BasicBlock::Create(*Context, "drop.do", currentFunc);
    llvm::BasicBlock* contBB = llvm::BasicBlock::Create(*Context, "drop.cont", currentFunc);
    Builder->CreateCondBr(shouldDrop, dropBB, contBB);

    Builder->SetInsertPoint(dropBB);
    if (!emitDropForAddress(info.Storage, info.ValueType)) {
        Builder->CreateBr(contBB);
        Builder->SetInsertPoint(contBB);
        return;
    }

    if (!Builder || !Builder->GetInsertBlock() || Builder->GetInsertBlock()->getTerminator()) {
        return;
    }

    Builder->CreateStore(llvm::ConstantInt::get(llvm::Type::getInt1Ty(*Context), 0), info.DropFlag);
    Builder->CreateBr(contBB);

    Builder->SetInsertPoint(contBB);
}

bool CodeGen::emitDropForAddress(llvm::Value* storage, Type* valueType) {
    if (!storage || !valueType || !Builder || !Builder->GetInsertBlock() ||
        Builder->GetInsertBlock()->getTerminator()) {
        return false;
    }

    FuncDecl* dropMethod = nullptr;
    if (!typeNeedsAutoDrop(valueType, &dropMethod) || !dropMethod) {
        return false;
    }

    GenericSubst dropMapping;
    Type* concreteValueType = unwrapAliases(valueType);
    Type* selfTypeForUnify = nullptr;

    if (!dropMethod->getParams().empty()) {
        if (ParamDecl* selfParam = dropMethod->getParams()[0]) {
            selfTypeForUnify = selfParam->getSemanticType();
        }
    }
    if (!selfTypeForUnify) {
        if (Type* dropFnType = dropMethod->getSemanticType()) {
            if (dropFnType->isFunction()) {
                auto* fnType = static_cast<FunctionType*>(dropFnType);
                if (fnType->getParamCount() > 0) {
                    selfTypeForUnify = fnType->getParam(0);
                }
            }
        }
    }

    selfTypeForUnify = unwrapAliases(selfTypeForUnify);
    if (selfTypeForUnify && selfTypeForUnify->isReference()) {
        selfTypeForUnify =
            unwrapAliases(static_cast<ReferenceType*>(selfTypeForUnify)->getPointeeType());
    }
    if (selfTypeForUnify && concreteValueType && typeHasGenericParam(selfTypeForUnify)) {
        (void)unifyGenericTypes(selfTypeForUnify, concreteValueType, dropMapping);
    }

    llvm::Function* dropFunc = nullptr;
    if (!dropMapping.empty() && dropMethod->hasBody()) {
        dropFunc = getOrCreateSpecializedFunction(dropMethod, dropMapping);
    }
    if (!dropFunc) {
        if (!generateDecl(dropMethod)) {
            return false;
        }
        auto vmIt = ValueMap.find(dropMethod);
        if (vmIt != ValueMap.end()) {
            dropFunc = llvm::dyn_cast<llvm::Function>(vmIt->second);
        }
        if (!dropFunc) {
            dropFunc = Module->getFunction(getFunctionSymbolName(dropMethod));
        }
    }
    if (!dropFunc || dropFunc->arg_size() < 1) {
        return false;
    }

    llvm::Value* selfArg = storage;
    llvm::Type* expectedSelfTy = dropFunc->getFunctionType()->getParamType(0);
    if (selfArg->getType() != expectedSelfTy) {
        if (selfArg->getType()->isPointerTy() && expectedSelfTy->isPointerTy()) {
            selfArg = Builder->CreateBitCast(selfArg, expectedSelfTy, "drop.self.cast");
        } else {
            return false;
        }
    }

    Builder->CreateCall(dropFunc, {selfArg});
    return true;
}

void CodeGen::emitDropForScope(size_t scopeIndex) {
    if (scopeIndex >= DropScopeStack.size()) {
        return;
    }
    const auto& decls = DropScopeStack[scopeIndex];
    for (auto it = decls.rbegin(); it != decls.rend(); ++it) {
        emitDropForDecl(*it);
    }
}

void CodeGen::emitDropForScopeRange(size_t fromDepth) {
    if (fromDepth >= DropScopeStack.size()) {
        return;
    }
    for (size_t idx = DropScopeStack.size(); idx > fromDepth; --idx) {
        emitDropForScope(idx - 1);
    }
}

const Decl* CodeGen::getDeclFromExprPlace(Expr* expr) const {
    if (!expr) {
        return nullptr;
    }
    if (auto* ident = dynamic_cast<IdentifierExpr*>(expr)) {
        return ident->getResolvedDecl();
    }
    if (auto* member = dynamic_cast<MemberExpr*>(expr)) {
        return getDeclFromExprPlace(member->getBase());
    }
    if (auto* index = dynamic_cast<IndexExpr*>(expr)) {
        return getDeclFromExprPlace(index->getBase());
    }
    return nullptr;
}

std::string CodeGen::mangleIdentifier(const std::string& text) const {
    return "I" + std::to_string(text.size()) + "_" + hexEncode(text);
}

std::string CodeGen::mangleDeclModule(const Decl* decl) const {
    if (!decl) {
        return mangleIdentifier(Module ? Module->getName().str() : "module");
    }

    std::string moduleKey;
    SourceLocation loc = decl->getBeginLoc();
    if (loc.isValid()) {
        const SourceManager& sm = Ctx.getSourceManager();
        SourceManager::FileID fid = sm.getFileID(loc);
        if (fid != SourceManager::InvalidFileID) {
            moduleKey = sm.getFilename(fid);
        }
    }

    if (moduleKey.empty()) {
        moduleKey = Module ? Module->getName().str() : "module";
    } else {
        std::replace(moduleKey.begin(), moduleKey.end(), '\\', '/');
        std::filesystem::path p(moduleKey);
        p = p.lexically_normal();
        if (p.has_extension()) {
            p.replace_extension();
        }
        moduleKey = p.generic_string();
    }

    return mangleIdentifier(moduleKey);
}

std::string CodeGen::mangleDeclDiscriminator(const Decl* decl) const {
    if (!decl) {
        return "Dnone";
    }

    SourceLocation loc = decl->getBeginLoc();
    if (loc.isValid()) {
        const SourceManager& sm = Ctx.getSourceManager();
        SourceManager::FileID fid = sm.getFileID(loc);
        if (fid != SourceManager::InvalidFileID) {
            auto [line, col] = sm.getLineAndColumn(loc);
            std::string out = "DL";
            out += std::to_string(static_cast<uint64_t>(line));
            out += "_";
            out += std::to_string(static_cast<uint64_t>(col));
            return out;
        }
    }

    uintptr_t ptr = reinterpret_cast<uintptr_t>(decl);
    return "DP" + hexEncodeU64(static_cast<uint64_t>(ptr));
}

std::string CodeGen::mangleTypeForSymbol(Type* type) const {
    if (!type) {
        return "Tn";
    }

    switch (type->getKind()) {
        case Type::Kind::Void:
            return "Tv";
        case Type::Kind::Bool:
            return "Tb";
        case Type::Kind::Char:
            return "Tc";
        case Type::Kind::String:
            return "Tstr";
        case Type::Kind::Value:
            return "Tval";
        case Type::Kind::Integer: {
            auto* intTy = static_cast<IntegerType*>(type);
            return (intTy->isSigned() ? "Ti" : "Tu") + std::to_string(intTy->getBitWidth());
        }
        case Type::Kind::Float: {
            auto* floatTy = static_cast<FloatType*>(type);
            return "Tf" + std::to_string(floatTy->getBitWidth());
        }
        case Type::Kind::Array: {
            auto* arrTy = static_cast<ArrayType*>(type);
            return "Ta" + std::to_string(arrTy->getArraySize()) + "_" +
                   mangleTypeForSymbol(arrTy->getElementType()) + "_E";
        }
        case Type::Kind::Slice: {
            auto* sliceTy = static_cast<SliceType*>(type);
            return "Ts" + std::string(sliceTy->isMutable() ? "m" : "i") + "_" +
                   mangleTypeForSymbol(sliceTy->getElementType()) + "_E";
        }
        case Type::Kind::Tuple: {
            auto* tupleTy = static_cast<TupleType*>(type);
            std::string out = "Tt" + std::to_string(tupleTy->getElementCount());
            for (size_t i = 0; i < tupleTy->getElementCount(); ++i) {
                out += "_";
                out += mangleTypeForSymbol(tupleTy->getElement(i));
            }
            out += "_E";
            return out;
        }
        case Type::Kind::VarArgs: {
            auto* varTy = static_cast<VarArgsType*>(type);
            return "Tvargs_" + mangleTypeForSymbol(varTy->getElementType()) + "_E";
        }
        case Type::Kind::Optional: {
            auto* optTy = static_cast<OptionalType*>(type);
            return "To_" + mangleTypeForSymbol(optTy->getInnerType()) + "_E";
        }
        case Type::Kind::Reference: {
            auto* refTy = static_cast<ReferenceType*>(type);
            return "Tr" + std::string(refTy->isMutable() ? "m" : "i") + "_" +
                   mangleTypeForSymbol(refTy->getPointeeType()) + "_E";
        }
        case Type::Kind::Pointer: {
            auto* ptrTy = static_cast<PointerType*>(type);
            return "Tp" + std::string(ptrTy->isMutable() ? "m" : "i") + "_" +
                   mangleTypeForSymbol(ptrTy->getPointeeType()) + "_E";
        }
        case Type::Kind::Function: {
            auto* fnTy = static_cast<FunctionType*>(type);
            std::string out = "Tfn" + std::to_string(fnTy->getParamCount());
            for (Type* paramTy : fnTy->getParamTypes()) {
                out += "_";
                out += mangleTypeForSymbol(paramTy);
            }
            out += "_R_";
            out += mangleTypeForSymbol(fnTy->getReturnType());
            out += "_Er";
            out += fnTy->canError() ? "1" : "0";
            out += "_Vr";
            out += fnTy->isVariadic() ? "1" : "0";
            out += "_E";
            return out;
        }
        case Type::Kind::Struct: {
            auto* structTy = static_cast<StructType*>(type);
            return "Tst_" + mangleIdentifier(structTy->getName());
        }
        case Type::Kind::Enum: {
            auto* enumTy = static_cast<EnumType*>(type);
            return "Ten_" + mangleIdentifier(enumTy->getName());
        }
        case Type::Kind::Trait: {
            auto* traitTy = static_cast<TraitType*>(type);
            return "Ttr_" + mangleIdentifier(traitTy->getName());
        }
        case Type::Kind::Generic: {
            auto* genericTy = static_cast<GenericType*>(type);
            return "Tg_" + mangleIdentifier(genericTy->getName());
        }
        case Type::Kind::GenericInstance: {
            auto* instTy = static_cast<GenericInstanceType*>(type);
            std::string out = "Tgi_";
            out += mangleTypeForSymbol(instTy->getBaseType());
            out += "_N";
            out += std::to_string(instTy->getTypeArgCount());
            for (Type* argTy : instTy->getTypeArgs()) {
                out += "_";
                out += mangleTypeForSymbol(argTy);
            }
            out += "_E";
            return out;
        }
        case Type::Kind::Error: {
            auto* errTy = static_cast<ErrorType*>(type);
            return "Terr_" + mangleTypeForSymbol(errTy->getSuccessType()) + "_E";
        }
        case Type::Kind::TypeVar: {
            auto* tvTy = static_cast<TypeVariable*>(type);
            if (tvTy->isResolved() && tvTy->getResolvedType()) {
                return "Ttv" + std::to_string(tvTy->getID()) + "_" +
                       mangleTypeForSymbol(tvTy->getResolvedType()) + "_E";
            }
            return "Ttv" + std::to_string(tvTy->getID());
        }
        case Type::Kind::TypeAlias: {
            auto* aliasTy = static_cast<TypeAlias*>(type);
            std::string out = "Tal_";
            out += mangleIdentifier(aliasTy->getName());
            out += "_";
            out += mangleTypeForSymbol(aliasTy->getAliasedType());
            out += "_E";
            return out;
        }
        case Type::Kind::Module: {
            auto* moduleTy = static_cast<ModuleType*>(type);
            return "Tmo_" + mangleIdentifier(moduleTy->getName());
        }
        case Type::Kind::Range: {
            auto* rangeTy = static_cast<RangeType*>(type);
            return "Tra" + std::string(rangeTy->isInclusive() ? "1" : "0") + "_" +
                   mangleTypeForSymbol(rangeTy->getElementType()) + "_E";
        }
    }

    return "Tn";
}

std::string CodeGen::buildFunctionSymbolBase(const FuncDecl* decl) const {
    std::string symbol = "_Y1";

    bool isMethod = false;
    if (decl && !decl->getParams().empty()) {
        ParamDecl* firstParam = decl->getParams().front();
        isMethod = firstParam && firstParam->isSelf();
    }
    symbol.push_back(isMethod ? 'M' : 'F');

    symbol += "M";
    symbol += mangleDeclModule(decl);

    symbol += "N";
    symbol += mangleIdentifier(decl ? decl->getName() : "");

    symbol += "P";
    size_t paramCount = decl ? decl->getParams().size() : 0;
    symbol += std::to_string(paramCount);
    for (size_t i = 0; i < paramCount; ++i) {
        ParamDecl* param = decl->getParams()[i];
        symbol += "_";
        symbol += mangleTypeForSymbol(param ? param->getSemanticType() : nullptr);
    }
    symbol += "_E";

    Type* returnType = nullptr;
    bool canError = false;
    bool isVariadic = false;
    if (decl && decl->getSemanticType() && decl->getSemanticType()->isFunction()) {
        auto* fnTy = static_cast<FunctionType*>(decl->getSemanticType());
        returnType = fnTy->getReturnType();
        canError = fnTy->canError();
        isVariadic = fnTy->isVariadic();
    }

    symbol += "R_";
    symbol += mangleTypeForSymbol(returnType);
    symbol += "_Er";
    symbol += canError ? "1" : "0";
    symbol += "_Vr";
    symbol += isVariadic ? "1" : "0";
    symbol += "_Ar";
    symbol += (decl && decl->isAsync()) ? "1" : "0";

    symbol += "G";
    size_t genericCount = decl ? decl->getGenericParams().size() : 0;
    symbol += std::to_string(genericCount);
    if (decl) {
        for (const auto& param : decl->getGenericParams()) {
            symbol += "_";
            symbol += mangleIdentifier(param.Name);
        }
    }
    symbol += "_E";

    symbol += "_";
    symbol += mangleDeclDiscriminator(decl);
    return symbol;
}

std::string CodeGen::buildSpecializationSuffix(const FuncDecl* decl, const GenericSubst& mapping) const {
    if (mapping.empty()) {
        return "";
    }

    std::vector<std::string> keys;
    keys.reserve(mapping.size());
    std::unordered_set<std::string> included;
    if (decl && decl->isGeneric()) {
        for (const auto& param : decl->getGenericParams()) {
            if (mapping.find(param.Name) != mapping.end() &&
                included.insert(param.Name).second) {
                keys.push_back(param.Name);
            }
        }
    }

    std::vector<std::string> extraKeys;
    extraKeys.reserve(mapping.size());
    for (const auto& entry : mapping) {
        if (included.insert(entry.first).second) {
            extraKeys.push_back(entry.first);
        }
    }

    std::sort(extraKeys.begin(), extraKeys.end());
    keys.insert(keys.end(), extraKeys.begin(), extraKeys.end());

    std::string suffix = "_S";
    suffix += std::to_string(keys.size());
    for (const std::string& key : keys) {
        auto it = mapping.find(key);
        if (it == mapping.end()) {
            continue;
        }
        suffix += "_";
        suffix += mangleIdentifier(key);
        suffix += "_";
        suffix += mangleTypeForSymbol(it->second);
    }
    suffix += "_E";
    return suffix;
}

std::string CodeGen::getFunctionSymbolName(const FuncDecl* decl) const {
    if (!decl) {
        return "";
    }
    if (ActiveSpecializationDecl == decl && !ActiveSpecializationName.empty()) {
        return ActiveSpecializationName;
    }
    if (decl->getName() == "main" && decl->getParams().empty()) {
        return "yuan_main";
    }
    if (!decl->getLinkName().empty()) {
        return decl->getLinkName();
    }

    auto it = FunctionSymbolCache.find(decl);
    if (it != FunctionSymbolCache.end()) {
        return it->second;
    }

    std::string symbol = buildFunctionSymbolBase(decl);
    FunctionSymbolCache[decl] = symbol;
    return symbol;
}

std::string CodeGen::getGlobalSymbolName(const Decl* decl,
                                         const std::string& baseName,
                                         char kind) const {
    if (!decl) {
        return "";
    }
    auto it = GlobalSymbolCache.find(decl);
    if (it != GlobalSymbolCache.end()) {
        return it->second;
    }

    char symbolKind = (kind == 'V' || kind == 'C') ? kind : 'X';
    std::string symbol = "_Y1";
    symbol.push_back(symbolKind);
    symbol += "M";
    symbol += mangleDeclModule(decl);
    symbol += "N";
    symbol += mangleIdentifier(baseName);
    symbol += "T_";
    symbol += mangleTypeForSymbol(decl->getSemanticType());
    symbol += "_";
    symbol += mangleDeclDiscriminator(decl);

    GlobalSymbolCache[decl] = symbol;
    return symbol;
}

Type* CodeGen::substituteType(Type* type) const {
    if (!type || GenericSubstStack.empty()) {
        return type;
    }
    const GenericSubst& mapping = GenericSubstStack.back();
    if (mapping.empty()) {
        return type;
    }

    if (type->isTypeVar()) {
        auto* typeVar = static_cast<TypeVariable*>(type);
        std::string key = "#tv" + std::to_string(typeVar->getID());
        auto it = mapping.find(key);
        if (it != mapping.end()) {
            return it->second;
        }
        if (typeVar->isResolved()) {
            return typeVar->getResolvedType();
        }
        return type;
    }

    if (type->isGeneric()) {
        auto* genericTy = static_cast<GenericType*>(type);
        auto it = mapping.find(genericTy->getName());
        if (it != mapping.end()) {
            return it->second;
        }
        return type;
    }

    if (type->isGenericInstance()) {
        auto* genInst = static_cast<GenericInstanceType*>(type);
        std::vector<Type*> newArgs;
        newArgs.reserve(genInst->getTypeArgCount());
        for (auto* arg : genInst->getTypeArgs()) {
            newArgs.push_back(substituteType(arg));
        }
        return Ctx.getGenericInstanceType(genInst->getBaseType(), std::move(newArgs));
    }

    if (type->isStruct()) {
        auto* structTy = static_cast<StructType*>(type);
        auto it = StructGenericParams.find(structTy);
        if (it == StructGenericParams.end()) {
            for (const auto& entry : StructGenericParams) {
                if (entry.first && entry.first->getName() == structTy->getName()) {
                    it = StructGenericParams.find(entry.first);
                    break;
                }
            }
        }

        std::vector<std::string> inferredParams;
        const std::vector<std::string>* paramsPtr = nullptr;
        if (it != StructGenericParams.end() && !it->second.empty()) {
            paramsPtr = &it->second;
        } else {
            inferredParams = inferStructGenericParams(structTy);
            if (!inferredParams.empty()) {
                paramsPtr = &inferredParams;
            }
        }

        if (paramsPtr && !paramsPtr->empty()) {
            std::vector<Type*> typeArgs;
            typeArgs.reserve(paramsPtr->size());
            for (const auto& paramName : *paramsPtr) {
                auto mit = mapping.find(paramName);
                if (mit == mapping.end() || !mit->second) {
                    return type;
                }
                typeArgs.push_back(substituteType(mit->second));
            }
            return Ctx.getGenericInstanceType(const_cast<Type*>(type), std::move(typeArgs));
        }
    }

    if (type->isEnum()) {
        auto* enumTy = static_cast<EnumType*>(type);
        auto it = EnumGenericParams.find(enumTy);
        if (it == EnumGenericParams.end()) {
            for (const auto& entry : EnumGenericParams) {
                if (entry.first && entry.first->getName() == enumTy->getName()) {
                    it = EnumGenericParams.find(entry.first);
                    break;
                }
            }
        }

        std::vector<std::string> inferredParams;
        const std::vector<std::string>* paramsPtr = nullptr;
        if (it != EnumGenericParams.end() && !it->second.empty()) {
            paramsPtr = &it->second;
        } else {
            inferredParams = inferEnumGenericParams(enumTy);
            if (!inferredParams.empty()) {
                paramsPtr = &inferredParams;
            }
        }

        if (paramsPtr && !paramsPtr->empty()) {
            std::vector<Type*> typeArgs;
            typeArgs.reserve(paramsPtr->size());
            for (const auto& paramName : *paramsPtr) {
                auto mit = mapping.find(paramName);
                if (mit == mapping.end() || !mit->second) {
                    return type;
                }
                typeArgs.push_back(substituteType(mit->second));
            }
            return Ctx.getGenericInstanceType(const_cast<Type*>(type), std::move(typeArgs));
        }
    }

    if (type->isOptional()) {
        auto* opt = static_cast<OptionalType*>(type);
        return Ctx.getOptionalType(substituteType(opt->getInnerType()));
    }

    if (type->isArray()) {
        auto* arr = static_cast<ArrayType*>(type);
        return Ctx.getArrayType(substituteType(arr->getElementType()), arr->getSize());
    }

    if (type->isSlice()) {
        auto* slice = static_cast<SliceType*>(type);
        return Ctx.getSliceType(substituteType(slice->getElementType()), slice->isMutable());
    }

    if (type->isTuple()) {
        auto* tuple = static_cast<TupleType*>(type);
        std::vector<Type*> elems;
        elems.reserve(tuple->getElementCount());
        for (size_t i = 0; i < tuple->getElementCount(); ++i) {
            elems.push_back(substituteType(tuple->getElement(i)));
        }
        return Ctx.getTupleType(std::move(elems));
    }

    if (type->isReference()) {
        auto* ref = static_cast<ReferenceType*>(type);
        return Ctx.getReferenceType(substituteType(ref->getPointeeType()), ref->isMutable());
    }

    if (type->isPointer()) {
        auto* ptr = static_cast<PointerType*>(type);
        return Ctx.getPointerType(substituteType(ptr->getPointeeType()), ptr->isMutable());
    }

    if (type->isVarArgs()) {
        auto* varargs = static_cast<VarArgsType*>(type);
        return Ctx.getVarArgsType(substituteType(varargs->getElementType()));
    }

    if (type->isFunction()) {
        auto* fn = static_cast<FunctionType*>(type);
        std::vector<Type*> params;
        params.reserve(fn->getParamCount());
        for (auto* p : fn->getParamTypes()) {
            params.push_back(substituteType(p));
        }
        Type* ret = substituteType(fn->getReturnType());
        return Ctx.getFunctionType(std::move(params), ret, fn->canError(), fn->isVariadic());
    }

    if (type->isError()) {
        auto* err = static_cast<ErrorType*>(type);
        return Ctx.getErrorType(substituteType(err->getSuccessType()));
    }

    if (type->isRange()) {
        auto* range = static_cast<RangeType*>(type);
        return Ctx.getRangeType(substituteType(range->getElementType()), range->isInclusive());
    }

    return type;
}

llvm::Value* CodeGen::coerceGenericValue(llvm::Value* value, Type* targetType) {
    if (!value || !targetType) {
        return value;
    }

    Type* resolvedType = substituteType(targetType);
    if (!resolvedType) {
        return value;
    }

    llvm::Type* llvmTarget = getLLVMType(resolvedType);
    if (!llvmTarget) {
        return value;
    }
    llvmTarget = normalizeFirstClassType(llvmTarget);

    if (value->getType() == llvmTarget) {
        return value;
    }

    if (resolvedType->isReference() || resolvedType->isPointer()) {
        if (value->getType()->isPointerTy()) {
            return Builder->CreateBitCast(value, llvmTarget, "gen.ptr.cast");
        }
        if (value->getType()->isIntegerTy()) {
            return Builder->CreateIntToPtr(value, llvmTarget, "gen.inttoptr");
        }
        llvm::AllocaInst* tmp = Builder->CreateAlloca(value->getType(), nullptr, "gen.ptr.tmp");
        Builder->CreateStore(value, tmp);
        return Builder->CreateBitCast(tmp, llvmTarget, "gen.ptr.cast");
    }

    if (resolvedType->isInteger() || resolvedType->isBool() || resolvedType->isChar()) {
        if (value->getType()->isPointerTy()) {
            return Builder->CreatePtrToInt(value, llvmTarget, "gen.ptrtoint");
        }
        if (value->getType()->isIntegerTy()) {
            return Builder->CreateSExtOrTrunc(value, llvmTarget, "gen.int.cast");
        }
    }

    if (resolvedType->isFloat()) {
        if (value->getType()->isPointerTy()) {
            llvm::Type* ptrTy = llvm::PointerType::get(llvmTarget, 0);
            llvm::Value* ptr = value;
            if (value->getType() != ptrTy) {
                ptr = Builder->CreateBitCast(value, ptrTy, "gen.float.ptr");
            }
            return Builder->CreateLoad(llvmTarget, ptr, "gen.float.load");
        }
        if (llvmTarget->isFloatTy() && value->getType()->isDoubleTy()) {
            return Builder->CreateFPTrunc(value, llvmTarget, "gen.ftrunc");
        }
        if (llvmTarget->isDoubleTy() && value->getType()->isFloatTy()) {
            return Builder->CreateFPExt(value, llvmTarget, "gen.fext");
        }
    }

    if (value->getType()->isPointerTy()) {
        llvm::Type* ptrTy = llvm::PointerType::get(llvmTarget, 0);
        llvm::Value* ptr = value;
        if (value->getType() != ptrTy) {
            ptr = Builder->CreateBitCast(value, ptrTy, "gen.obj.ptr");
        }
        return Builder->CreateLoad(llvmTarget, ptr, "gen.obj.load");
    }

    return Builder->CreateBitCast(value, llvmTarget, "gen.cast");
}

bool CodeGen::unifyGenericTypes(Type* expected, Type* actual, GenericSubst& mapping) const {
    if (!expected || !actual) {
        return false;
    }

    if (expected->isTypeVar()) {
        auto* typeVar = static_cast<TypeVariable*>(expected);
        std::string key = "#tv" + std::to_string(typeVar->getID());
        auto it = mapping.find(key);
        if (it != mapping.end()) {
            return it->second->isEqual(actual);
        }
        mapping[key] = actual;
        return true;
    }

    if (expected->isGeneric()) {
        auto* gen = static_cast<GenericType*>(expected);
        const std::string& name = gen->getName();
        auto it = mapping.find(name);
        if (it != mapping.end()) {
            return it->second->isEqual(actual);
        }
        mapping[name] = actual;
        return true;
    }

    if (expected->isReference() && actual->isReference()) {
        auto* expRef = static_cast<ReferenceType*>(expected);
        auto* actRef = static_cast<ReferenceType*>(actual);
        return unifyGenericTypes(expRef->getPointeeType(), actRef->getPointeeType(), mapping);
    }
    if (expected->isPointer() && actual->isPointer()) {
        auto* expPtr = static_cast<PointerType*>(expected);
        auto* actPtr = static_cast<PointerType*>(actual);
        return unifyGenericTypes(expPtr->getPointeeType(), actPtr->getPointeeType(), mapping);
    }
    if (expected->isOptional() && actual->isOptional()) {
        auto* expOpt = static_cast<OptionalType*>(expected);
        auto* actOpt = static_cast<OptionalType*>(actual);
        return unifyGenericTypes(expOpt->getInnerType(), actOpt->getInnerType(), mapping);
    }
    if (expected->isArray() && actual->isArray()) {
        auto* expArr = static_cast<ArrayType*>(expected);
        auto* actArr = static_cast<ArrayType*>(actual);
        if (expArr->getSize() != actArr->getSize()) {
            return false;
        }
        return unifyGenericTypes(expArr->getElementType(), actArr->getElementType(), mapping);
    }
    if (expected->isSlice() && actual->isSlice()) {
        auto* expSlice = static_cast<SliceType*>(expected);
        auto* actSlice = static_cast<SliceType*>(actual);
        return unifyGenericTypes(expSlice->getElementType(), actSlice->getElementType(), mapping);
    }
    if (expected->isVarArgs() && actual->isVarArgs()) {
        auto* expVar = static_cast<VarArgsType*>(expected);
        auto* actVar = static_cast<VarArgsType*>(actual);
        return unifyGenericTypes(expVar->getElementType(), actVar->getElementType(), mapping);
    }
    if (expected->isTuple() && actual->isTuple()) {
        auto* expTuple = static_cast<TupleType*>(expected);
        auto* actTuple = static_cast<TupleType*>(actual);
        if (expTuple->getElementCount() != actTuple->getElementCount()) {
            return false;
        }
        for (size_t i = 0; i < expTuple->getElementCount(); ++i) {
            if (!unifyGenericTypes(expTuple->getElement(i), actTuple->getElement(i), mapping)) {
                return false;
            }
        }
        return true;
    }
    if (expected->isFunction() && actual->isFunction()) {
        auto* expFn = static_cast<FunctionType*>(expected);
        auto* actFn = static_cast<FunctionType*>(actual);
        if (expFn->getParamCount() != actFn->getParamCount()) {
            return false;
        }
        for (size_t i = 0; i < expFn->getParamCount(); ++i) {
            if (!unifyGenericTypes(expFn->getParam(i), actFn->getParam(i), mapping)) {
                return false;
            }
        }
        return unifyGenericTypes(expFn->getReturnType(), actFn->getReturnType(), mapping);
    }
    if (expected->isError() && actual->isError()) {
        auto* expErr = static_cast<ErrorType*>(expected);
        auto* actErr = static_cast<ErrorType*>(actual);
        return unifyGenericTypes(expErr->getSuccessType(), actErr->getSuccessType(), mapping);
    }
    if (expected->isRange() && actual->isRange()) {
        auto* expRange = static_cast<RangeType*>(expected);
        auto* actRange = static_cast<RangeType*>(actual);
        return unifyGenericTypes(expRange->getElementType(), actRange->getElementType(), mapping);
    }
    if (expected->isGenericInstance() && actual->isGenericInstance()) {
        auto* expInst = static_cast<GenericInstanceType*>(expected);
        auto* actInst = static_cast<GenericInstanceType*>(actual);
        if (!expInst->getBaseType()->isEqual(actInst->getBaseType()) ||
            expInst->getTypeArgCount() != actInst->getTypeArgCount()) {
            return false;
        }
        for (size_t i = 0; i < expInst->getTypeArgCount(); ++i) {
            if (!unifyGenericTypes(expInst->getTypeArg(i), actInst->getTypeArg(i), mapping)) {
                return false;
            }
        }
        return true;
    }

    return expected->isEqual(actual);
}

bool CodeGen::buildStructGenericMapping(const StructType* baseStruct,
                                        const GenericInstanceType* genInst,
                                        GenericSubst& mapping) const {
    if (!baseStruct || !genInst) {
        return false;
    }

    auto it = StructGenericParams.find(baseStruct);
    if (it == StructGenericParams.end()) {
        for (const auto& entry : StructGenericParams) {
            if (entry.first && entry.first->getName() == baseStruct->getName()) {
                it = StructGenericParams.find(entry.first);
                break;
            }
        }
    }

    if (it == StructGenericParams.end()) {
        return false;
    }

    const auto& params = it->second;
    if (params.size() != genInst->getTypeArgCount()) {
        return false;
    }

    mapping.clear();
    for (size_t i = 0; i < params.size(); ++i) {
        mapping[params[i]] = genInst->getTypeArg(i);
    }
    return !mapping.empty();
}

llvm::Function* CodeGen::getOrCreateSpecializedFunction(FuncDecl* decl, const GenericSubst& mapping) {
    if (!decl) {
        return nullptr;
    }
    if (mapping.empty()) {
        return nullptr;
    }

    std::string baseName = getFunctionSymbolName(decl);
    std::string specName = baseName + buildSpecializationSuffix(decl, mapping);

    if (llvm::Function* existing = Module->getFunction(specName)) {
        return existing;
    }

    // Activate specialization context.
    const FuncDecl* savedDecl = ActiveSpecializationDecl;
    std::string savedName = ActiveSpecializationName;
    ActiveSpecializationDecl = decl;
    ActiveSpecializationName = specName;
    GenericSubstStack.push_back(mapping);

    auto savedIP = Builder->saveIP();
    bool ok = generateFuncDecl(decl);
    Builder->restoreIP(savedIP);

    GenericSubstStack.pop_back();
    ActiveSpecializationDecl = savedDecl;
    ActiveSpecializationName = savedName;

    if (!ok) {
        return nullptr;
    }

    return Module->getFunction(specName);
}

// ============================================================================
// Main Generation Entry Point
// ============================================================================

bool CodeGen::generate() {
    // Note: In the current architecture, code generation is driven by the Driver,
    // which calls generateDecl() for each declaration after semantic analysis.
    // This method is kept for potential future use where CodeGen might need to
    // perform module-level initialization or finalization.

    // Verify the generated module
    std::string errorMsg;
    if (!verifyModule(&errorMsg)) {
        return false;
    }

    return true;
}

// ============================================================================
// IR Emission
// ============================================================================

std::string CodeGen::emitIR() const {
    std::string str;
    llvm::raw_string_ostream os(str);
    Module->print(os, nullptr);
    return str;
}

bool CodeGen::emitIRToFile(const std::string& filename) const {
    std::error_code EC;
    llvm::raw_fd_ostream dest(filename, EC, llvm::sys::fs::OF_None);

    if (EC) {
        return false;
    }

    Module->print(dest, nullptr);
    return true;
}

bool CodeGen::verifyModule(std::string* errorMsg) const {
    std::string errors;
    llvm::raw_string_ostream errorStream(errors);

    // Use LLVM's verifyModule function
    // Returns true if module is broken (has errors)
    bool hasErrors = llvm::verifyModule(*Module, &errorStream);

    if (hasErrors && errorMsg) {
        *errorMsg = errors;
    }

    // Return true if verification succeeded (no errors)
    return !hasErrors;
}

bool CodeGen::emitObjectFile(const std::string& filename, unsigned optimizationLevel) {
    // Initialize targets
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    // Get the target triple
    std::string targetTriple = llvm::sys::getDefaultTargetTriple();
    Module->setTargetTriple(targetTriple);

    // Get the target
    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);

    if (!target) {
        return false;
    }

    // Configure target machine with optimization level
    auto CPU = "generic";
    auto features = "";
    llvm::TargetOptions opt;

    // Map optimization level to LLVM CodeGenOptLevel
    llvm::CodeGenOptLevel optLevel;
    switch (optimizationLevel) {
        case 0:
            optLevel = llvm::CodeGenOptLevel::None;
            break;
        case 1:
            optLevel = llvm::CodeGenOptLevel::Less;
            break;
        case 2:
            optLevel = llvm::CodeGenOptLevel::Default;
            break;
        case 3:
        default:
            optLevel = llvm::CodeGenOptLevel::Aggressive;
            break;
    }

    auto targetMachine = target->createTargetMachine(
        targetTriple, CPU, features, opt, llvm::Reloc::PIC_,
        llvm::CodeModel::Small, optLevel);

    if (!targetMachine) {
        return false;
    }

    Module->setDataLayout(targetMachine->createDataLayout());

    // Open output file
    std::error_code EC;
    llvm::raw_fd_ostream dest(filename, EC, llvm::sys::fs::OF_None);

    if (EC) {
        return false;
    }

    // Emit object code
    llvm::legacy::PassManager pass;
    auto fileType = llvm::CodeGenFileType::ObjectFile;

    if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, fileType)) {
        return false;
    }

    pass.run(*Module);
    dest.flush();

    return true;
}

bool CodeGen::linkExecutable(const std::string& objectFile, const std::string& executableFile) {
    // Use the system linker to create an executable
    // This is platform-specific

    // 获取运行时库路径
    // 假设运行时库在编译器可执行文件的相对路径
    std::string runtimeLib = "build/runtime/libyuan_runtime.a";

#if defined(__APPLE__)
    // macOS: use clang
    std::string linkerCmd = "clang++ -o \"" + executableFile + "\" \"" + objectFile + "\" \"" + runtimeLib + "\"";
#elif defined(__linux__)
    // Linux: use g++ or clang++
    std::string linkerCmd = "g++ -o \"" + executableFile + "\" \"" + objectFile + "\" \"" + runtimeLib + "\"";
#elif defined(_WIN32)
    // Windows: use link.exe or lld-link
    std::string linkerCmd = "lld-link /OUT:\"" + executableFile + "\" \"" + objectFile + "\" \"" + runtimeLib + "\"";
#else
    // Unsupported platform
    return false;
#endif

    // Execute the linker command
    int result = std::system(linkerCmd.c_str());

    // Check if linking succeeded (return code 0)
    return result == 0;
}

// ============================================================================
// Type Conversion
// ============================================================================

llvm::Type* CodeGen::getLLVMType(const Type* type) {
    if (!type) {
        return nullptr;
    }

    if (!GenericSubstStack.empty()) {
        if (Type* resolved = substituteType(const_cast<Type*>(type))) {
            type = resolved;
        }
    }

    // Check cache first
    auto it = TypeCache.find(type);
    if (it != TypeCache.end()) {
        return it->second;
    }

    // Convert the type
    llvm::Type* llvmType = nullptr;

    switch (type->getKind()) {
        case Type::Kind::Void:
        case Type::Kind::Bool:
        case Type::Kind::Char:
        case Type::Kind::String:
        case Type::Kind::Integer:
        case Type::Kind::Float:
            llvmType = convertBuiltinType(type);
            break;

        case Type::Kind::Array:
            llvmType = convertArrayType(type);
            break;

        case Type::Kind::Slice:
            llvmType = convertSliceType(type);
            break;

        case Type::Kind::Tuple:
            llvmType = convertTupleType(type);
            break;

        case Type::Kind::Value:
            llvmType = convertValueType(type);
            break;

        case Type::Kind::VarArgs:
            llvmType = convertVarArgsType(type);
            break;

        case Type::Kind::Pointer:
            llvmType = convertPointerType(type);
            break;

        case Type::Kind::Reference:
            llvmType = convertReferenceType(type);
            break;

        case Type::Kind::Function:
            llvmType = convertFunctionType(type);
            break;

        case Type::Kind::Struct:
            llvmType = convertStructType(type);
            break;

        case Type::Kind::Enum:
            llvmType = convertEnumType(type);
            break;

        case Type::Kind::Error:
            llvmType = convertErrorType(type);
            break;

        case Type::Kind::Range:
            llvmType = convertRangeType(type);
            break;

        case Type::Kind::Optional:
            llvmType = convertOptionalType(type);
            break;

        case Type::Kind::TypeAlias: {
            auto* aliasType = static_cast<const TypeAlias*>(type);
            llvmType = getLLVMType(aliasType->getAliasedType());
            break;
        }

        case Type::Kind::GenericInstance: {
            auto* genInst = static_cast<const GenericInstanceType*>(type);
            Type* baseType = genInst->getBaseType();
            if (baseType && baseType->isStruct()) {
                auto* baseStruct = static_cast<const StructType*>(baseType);
                auto it = StructGenericParams.find(baseStruct);
                if (it == StructGenericParams.end()) {
                    // Fallback by name in case the base struct pointer differs.
                    for (const auto& entry : StructGenericParams) {
                        if (entry.first && entry.first->getName() == baseStruct->getName()) {
                            it = StructGenericParams.find(entry.first);
                            break;
                        }
                    }
                }
                if (it == StructGenericParams.end()) {
                    std::vector<std::string> inferred = inferStructGenericParams(baseStruct);
                    if (!inferred.empty()) {
                        auto key = const_cast<StructType*>(baseStruct);
                        StructGenericParams[key] = std::move(inferred);
                        it = StructGenericParams.find(key);
                    }
                }
                if (it != StructGenericParams.end() && !it->second.empty()) {
                    const auto& params = it->second;
                    if (params.size() == genInst->getTypeArgCount()) {
                        std::string instName = "_YT_";
                        instName += mangleIdentifier(baseStruct->getName());
                        for (size_t i = 0; i < params.size(); ++i) {
                            instName += "__";
                            instName += mangleIdentifier(params[i]);
                            instName += "_";
                            instName += mangleTypeForSymbol(genInst->getTypeArg(i));
                        }
                        llvm::StructType* llvmStruct = llvm::StructType::getTypeByName(*Context, instName);
                        if (!llvmStruct) {
                            llvmStruct = llvm::StructType::create(*Context, instName);
                        }

                        if (llvmStruct->isOpaque()) {
                            GenericSubst mapping;
                            for (size_t i = 0; i < params.size(); ++i) {
                                mapping[params[i]] = genInst->getTypeArg(i);
                            }

                            GenericSubstStack.push_back(mapping);
                            const auto& fields = baseStruct->getFields();
                            std::vector<llvm::Type*> llvmFields;
                            llvmFields.reserve(fields.size());
                            for (const auto& field : fields) {
                                llvm::Type* fieldType = getLLVMType(field.FieldType);
                                if (!fieldType) {
                                    GenericSubstStack.pop_back();
                                    return nullptr;
                                }
                                fieldType = normalizeFirstClassType(fieldType);
                                llvmFields.push_back(fieldType);
                            }
                            GenericSubstStack.pop_back();
                            llvmStruct->setBody(llvmFields);
                        }

                        llvmType = llvmStruct;
                        break;
                    }
                }
            }
            if (baseType && baseType->isEnum()) {
                auto* baseEnum = static_cast<const EnumType*>(baseType);
                auto it = EnumGenericParams.find(baseEnum);
                if (it == EnumGenericParams.end()) {
                    for (const auto& entry : EnumGenericParams) {
                        if (entry.first && entry.first->getName() == baseEnum->getName()) {
                            it = EnumGenericParams.find(entry.first);
                            break;
                        }
                    }
                }
                if (it == EnumGenericParams.end()) {
                    std::vector<std::string> inferred = inferEnumGenericParams(baseEnum);
                    if (!inferred.empty()) {
                        auto key = const_cast<EnumType*>(baseEnum);
                        EnumGenericParams[key] = std::move(inferred);
                        it = EnumGenericParams.find(key);
                    }
                }
                if (it != EnumGenericParams.end() && !it->second.empty()) {
                    const auto& params = it->second;
                    if (params.size() == genInst->getTypeArgCount()) {
                        std::string instName = "_YE_";
                        instName += mangleIdentifier(baseEnum->getName());
                        for (size_t i = 0; i < params.size(); ++i) {
                            instName += "__";
                            instName += mangleIdentifier(params[i]);
                            instName += "_";
                            instName += mangleTypeForSymbol(genInst->getTypeArg(i));
                        }

                        llvm::StructType* llvmEnum = llvm::StructType::getTypeByName(*Context, instName);
                        if (!llvmEnum) {
                            llvmEnum = llvm::StructType::create(*Context, instName);
                        }

                        if (llvmEnum->isOpaque()) {
                            GenericSubst mapping;
                            for (size_t i = 0; i < params.size(); ++i) {
                                mapping[params[i]] = genInst->getTypeArg(i);
                            }

                            GenericSubstStack.push_back(mapping);
                            // Force payload type substitution/materialization under specialization mapping.
                            for (const auto& variant : baseEnum->getVariants()) {
                                for (Type* payloadTy : variant.Data) {
                                    (void)getLLVMType(payloadTy);
                                }
                            }
                            GenericSubstStack.pop_back();

                            llvmEnum->setBody({
                                llvm::Type::getInt32Ty(*Context),
                                llvm::PointerType::get(llvm::Type::getInt8Ty(*Context), 0)});
                        }

                        llvmType = llvmEnum;
                        break;
                    }
                }
            }
            llvmType = getLLVMType(baseType);
            break;
        }

        case Type::Kind::Trait:
        case Type::Kind::Generic:
        case Type::Kind::TypeVar:
            // 暂时将 trait/generic 作为不透明指针处理
            llvmType = llvm::PointerType::get(llvm::Type::getInt8Ty(*Context), 0);
            break;

        case Type::Kind::Module:
            // 模块类型是编译期概念，没有运行时表示
            llvmType = nullptr;
            break;

        default:
            // Unsupported or compile-time-only types
            break;
    }

    // Cache the result
    if (llvmType) {
        TypeCache[type] = llvmType;
    }

    return llvmType;
}

// ============================================================================
// Type Conversion Helpers
// ============================================================================

llvm::Type* CodeGen::convertBuiltinType(const Type* type) {
    switch (type->getKind()) {
        case Type::Kind::Void:
            return llvm::Type::getVoidTy(*Context);

        case Type::Kind::Bool:
            return llvm::Type::getInt1Ty(*Context);

        case Type::Kind::Char:
            return llvm::Type::getInt8Ty(*Context);

        case Type::Kind::String:
            // String is represented as { i8*, i64 } (pointer + length)
            return llvm::StructType::get(
                llvm::PointerType::get(llvm::Type::getInt8Ty(*Context), 0),
                llvm::Type::getInt64Ty(*Context)
            );

        case Type::Kind::Integer: {
            auto* intType = static_cast<const IntegerType*>(type);
            unsigned bitWidth = intType->getBitWidth();
            return llvm::IntegerType::get(*Context, bitWidth);
        }

        case Type::Kind::Float: {
            auto* floatType = static_cast<const FloatType*>(type);
            if (floatType->getBitWidth() == 32) {
                return llvm::Type::getFloatTy(*Context);
            } else {
                return llvm::Type::getDoubleTy(*Context);
            }
        }

        default:
            return nullptr;
    }
}

llvm::Type* CodeGen::convertArrayType(const Type* type) {
    auto* arrayType = static_cast<const ArrayType*>(type);
    llvm::Type* elemType = getLLVMType(arrayType->getElementType());
    if (!elemType) {
        return nullptr;
    }
    elemType = normalizeFirstClassType(elemType);

    uint64_t numElements = arrayType->getArraySize();
    return llvm::ArrayType::get(elemType, numElements);
}

llvm::Type* CodeGen::convertSliceType(const Type* type) {
    auto* sliceType = static_cast<const SliceType*>(type);
    llvm::Type* elemType = getLLVMType(sliceType->getElementType());
    if (!elemType) {
        return nullptr;
    }
    elemType = normalizeFirstClassType(elemType);

    // Slice is represented as { T*, i64 } (pointer + length)
    return llvm::StructType::get(
        llvm::PointerType::get(elemType, 0),
        llvm::Type::getInt64Ty(*Context)
    );
}

llvm::Type* CodeGen::convertTupleType(const Type* type) {
    auto* tupleType = static_cast<const TupleType*>(type);
    const auto& elements = tupleType->getElements();

    std::vector<llvm::Type*> llvmElements;
    llvmElements.reserve(elements.size());

    for (const auto* elemType : elements) {
        llvm::Type* llvmElem = getLLVMType(elemType);
        if (!llvmElem) {
            return nullptr;
        }
        llvmElem = normalizeFirstClassType(llvmElem);
        llvmElements.push_back(llvmElem);
    }

    return llvm::StructType::get(*Context, llvmElements);
}

llvm::Type* CodeGen::convertValueType(const Type* type) {
    (void)type;
    llvm::StructType* valueTy = llvm::StructType::getTypeByName(*Context, "YuanValue");
    if (!valueTy) {
        valueTy = llvm::StructType::create(*Context, "YuanValue");
    }
    if (valueTy->isOpaque()) {
        llvm::Type* i32Ty = llvm::Type::getInt32Ty(*Context);
        llvm::Type* i64Ty = llvm::Type::getInt64Ty(*Context);
        valueTy->setBody({i32Ty, i32Ty, i64Ty, i64Ty});
    }
    return valueTy;
}

llvm::Type* CodeGen::convertVarArgsType(const Type* type) {
    auto* varArgsType = static_cast<const VarArgsType*>(type);
    (void)varArgsType;

    llvm::StructType* varArgsTy = llvm::StructType::getTypeByName(*Context, "YuanVarArgs");
    if (!varArgsTy) {
        varArgsTy = llvm::StructType::create(*Context, "YuanVarArgs");
    }
    if (varArgsTy->isOpaque()) {
        llvm::Type* i64Ty = llvm::Type::getInt64Ty(*Context);
        llvm::Type* valueTy = convertValueType(nullptr);
        llvm::Type* valuePtrTy = llvm::PointerType::get(valueTy, 0);
        varArgsTy->setBody({i64Ty, valuePtrTy});
    }
    return varArgsTy;
}

llvm::Type* CodeGen::convertPointerType(const Type* type) {
    auto* ptrType = static_cast<const PointerType*>(type);
    llvm::Type* pointeeType = getLLVMType(ptrType->getPointeeType());
    if (!pointeeType) {
        return nullptr;
    }

    return llvm::PointerType::get(pointeeType, 0);
}

llvm::Type* CodeGen::convertReferenceType(const Type* type) {
    auto* refType = static_cast<const ReferenceType*>(type);
    llvm::Type* referencedType = getLLVMType(refType->getPointeeType());
    if (!referencedType) {
        return nullptr;
    }

    // References are implemented as pointers in LLVM
    return llvm::PointerType::get(referencedType, 0);
}

llvm::Type* CodeGen::convertFunctionType(const Type* type) {
    auto* funcType = static_cast<const FunctionType*>(type);

    // Convert return type (canError -> ErrorType)
    Type* semReturnType = funcType->getReturnType();
    if (funcType->canError()) {
        semReturnType = Ctx.getErrorType(semReturnType);
    }
    llvm::Type* returnType = getLLVMType(semReturnType);
    if (!returnType) {
        return nullptr;
    }
    returnType = normalizeFirstClassType(returnType);

    // Convert parameter types
    const auto& params = funcType->getParamTypes();
    std::vector<llvm::Type*> llvmParams;
    llvmParams.reserve(params.size());

    for (const auto* paramType : params) {
        llvm::Type* llvmParam = getLLVMType(paramType);
        if (!llvmParam) {
            return nullptr;
        }
        llvmParam = normalizeFirstClassType(llvmParam);
        llvmParams.push_back(llvmParam);
    }

    return llvm::FunctionType::get(returnType, llvmParams, false);
}

llvm::Type* CodeGen::convertStructType(const Type* type) {
    auto* structType = static_cast<const StructType*>(type);

    // Check if we already have a named struct type
    const std::string& name = structType->getName();
    llvm::StructType* llvmStruct = llvm::StructType::getTypeByName(*Context, name);

    if (llvmStruct) {
        return llvmStruct;
    }

    // Create opaque struct type first (for recursive types)
    llvmStruct = llvm::StructType::create(*Context, name);

    // Convert field types
    const auto& fields = structType->getFields();
    std::vector<llvm::Type*> llvmFields;
    llvmFields.reserve(fields.size());

    for (const auto& field : fields) {
        llvm::Type* fieldType = getLLVMType(field.FieldType);
        if (!fieldType) {
            return nullptr;
        }
        fieldType = normalizeFirstClassType(fieldType);
        llvmFields.push_back(fieldType);
    }

    // Set the body
    llvmStruct->setBody(llvmFields);

    return llvmStruct;
}

llvm::Type* CodeGen::convertEnumType(const Type* type) {
    auto* enumType = static_cast<const EnumType*>(type);

    // Enum is represented as { i32, union } where i32 is the tag
    // For now, use a simple representation: { i32, i8* }
    // The i8* can hold any variant data

    return llvm::StructType::get(
        llvm::Type::getInt32Ty(*Context),  // Tag
        llvm::PointerType::get(llvm::Type::getInt8Ty(*Context), 0)  // Data pointer
    );
}

llvm::Type* CodeGen::convertErrorType(const Type* type) {
    auto* errorType = static_cast<const ErrorType*>(type);

    // Error type (Result<T, E>) is represented as:
    // struct Result {
    //     i8  tag;      // 0 = Ok, 1 = Err
    //     T   ok_value; // valid when tag == 0
    //     i8* err_ptr;  // valid when tag == 1
    // }
    //
    // This avoids illegal LLVM bitcasts when T is an aggregate type
    // (e.g. struct payload in !Struct).

    Type* successType = errorType->getSuccessType();
    llvm::Type* llvmSuccessType = getLLVMType(successType);
    if (!llvmSuccessType) {
        return nullptr;
    }
    llvmSuccessType = normalizeFirstClassType(llvmSuccessType);
    if (llvmSuccessType->isVoidTy()) {
        // Keep Result first-class even for !void.
        llvmSuccessType = llvm::Type::getInt8Ty(*Context);
    }

    llvm::Type* errorPtrType = llvm::PointerType::get(llvm::Type::getInt8Ty(*Context), 0);

    // Result type: { i8 tag, ok_value, err_ptr }
    return llvm::StructType::get(
        llvm::Type::getInt8Ty(*Context),  // Tag (0=Ok, 1=Err)
        llvmSuccessType,                   // Ok payload
        errorPtrType                       // Err payload pointer
    );
}

llvm::Type* CodeGen::convertRangeType(const Type* type) {
    auto* rangeType = static_cast<const RangeType*>(type);

    // Range 类型在 LLVM IR 中表示为结构体：
    // struct Range {
    //     T start;        // 起始值
    //     T end;          // 结束值
    //     i1 inclusive;   // 是否包含结束值
    // }
    //
    // 设计决策：
    // - 使用结构体而不是类来简化内存布局
    // - inclusive 字段使用 i1 (bool) 类型
    // - 结构体按照元素类型对齐

    Type* elementType = rangeType->getElementType();
    llvm::Type* llvmElementType = getLLVMType(elementType);
    if (!llvmElementType) {
        return nullptr;
    }
    llvmElementType = normalizeFirstClassType(llvmElementType);

    // Range 结构体: { T start, T end, i1 inclusive }
    return llvm::StructType::get(
        llvmElementType,                    // start
        llvmElementType,                    // end
        llvm::Type::getInt1Ty(*Context)    // inclusive
    );
}

llvm::Type* CodeGen::convertOptionalType(const Type* type) {
    auto* optType = static_cast<const OptionalType*>(type);
    Type* innerType = optType->getInnerType();
    
    // ?void 使用 i8 作为占位符类型
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

    // Optional represented as { i1 hasValue, T value }
    return llvm::StructType::get(
        llvm::Type::getInt1Ty(*Context),
        llvmInnerType
    );
}

} // namespace yuan
