/// \file ModuleManager.cpp
/// \brief 模块管理器实现

#include "yuan/Sema/ModuleManager.h"
#include "yuan/Sema/Sema.h"
#include "yuan/Sema/TypeCodec.h"
#include "yuan/CodeGen/CodeGen.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/Lexer/Lexer.h"
#include "yuan/Parser/Parser.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace yuan {
namespace {

static std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= line.size()) {
        size_t pos = line.find('\t', start);
        if (pos == std::string::npos) {
            parts.push_back(line.substr(start));
            break;
        }
        parts.push_back(line.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

static std::string modulePathToRelFile(const std::string& modulePath) {
    std::string rel = modulePath;
    if (rel.rfind("std.", 0) == 0) {
        rel = rel.substr(4);
    } else if (rel.rfind("std/", 0) == 0) {
        rel = rel.substr(4);
    }
    std::replace(rel.begin(), rel.end(), '.', '/');
    return rel;
}

static std::string fnv1a64Hex(const std::string& input) {
    constexpr uint64_t kOffset = 1469598103934665603ULL;
    constexpr uint64_t kPrime = 1099511628211ULL;
    uint64_t hash = kOffset;
    for (unsigned char c : input) {
        hash ^= static_cast<uint64_t>(c);
        hash *= kPrime;
    }

    constexpr char kHex[] = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i) {
        out[static_cast<size_t>(i)] = kHex[hash & 0x0F];
        hash >>= 4;
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
        return typeHasGenericParam(inst->getBaseType());
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
        auto* tup = static_cast<TupleType*>(type);
        for (size_t i = 0; i < tup->getElementCount(); ++i) {
            if (typeHasGenericParam(tup->getElement(i))) {
                return true;
            }
        }
        return false;
    }
    if (type->isFunction()) {
        auto* fn = static_cast<FunctionType*>(type);
        for (size_t i = 0; i < fn->getParamCount(); ++i) {
            if (typeHasGenericParam(fn->getParam(i))) {
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
    if (type->isTypeAlias()) {
        return typeHasGenericParam(static_cast<TypeAlias*>(type)->getAliasedType());
    }
    return false;
}

static bool isImportBuiltinExpr(Expr* expr, std::string& modulePathOut) {
    if (!expr || expr->getKind() != ASTNode::Kind::BuiltinCallExpr) {
        return false;
    }

    auto* builtin = static_cast<BuiltinCallExpr*>(expr);
    if (builtin->getBuiltinKind() != BuiltinKind::Import ||
        builtin->getArgCount() != 1) {
        return false;
    }

    const auto& arg = builtin->getArgs()[0];
    if (!arg.isExpr() || !arg.getExpr() ||
        arg.getExpr()->getKind() != ASTNode::Kind::StringLiteralExpr) {
        return false;
    }

    modulePathOut = static_cast<StringLiteralExpr*>(arg.getExpr())->getValue();
    return !modulePathOut.empty();
}

} // namespace

ModuleManager::ModuleManager(SourceManager& sourceMgr, DiagnosticEngine& diag,
                             ASTContext& ctx, Sema& sema)
    : SourceMgr(sourceMgr), Diag(diag), Ctx(ctx), Sema_(sema) {
    StdLibPath = "./stdlib";
}

void ModuleManager::addPackagePath(const std::string& path) {
    if (path.empty()) {
        return;
    }
    if (std::find(PackagePaths.begin(), PackagePaths.end(), path) == PackagePaths.end()) {
        PackagePaths.push_back(path);
    }
}

std::string ModuleManager::resolveModulePath(const std::string& modulePath,
                                             const std::string& currentFilePath) {
    if (modulePath.empty()) {
        return "";
    }

    if (modulePath[0] == '.' || modulePath[0] == '/') {
        return resolveRelativePath(modulePath, currentFilePath);
    }

    if (modulePath.find("std.") == 0 || modulePath.find("std/") == 0) {
        return resolveStdLibPath(modulePath);
    }

    // 优先尝试当前文件目录（用于局部无前缀模块名）
    if (!currentFilePath.empty()) {
        std::filesystem::path currentDir = std::filesystem::path(currentFilePath).parent_path();
        std::filesystem::path localPath = currentDir / modulePath;
        if (!localPath.has_extension()) {
            localPath += ".yu";
        }
        if (std::filesystem::exists(localPath)) {
            try {
                return std::filesystem::canonical(localPath).string();
            } catch (...) {
                return std::filesystem::weakly_canonical(localPath).string();
            }
        }
    }

    // 默认仍按标准库路径解析（兼容现有行为，例如 std.yu 内 import("io")）
    return resolveStdLibPath(modulePath);
}

std::string ModuleManager::resolveStdLibPath(const std::string& modulePath) {
    std::string path = modulePath;
    if (path.find("std.") == 0) {
        path = path.substr(4);
        std::replace(path.begin(), path.end(), '.', '/');
    } else if (path.find("std/") == 0) {
        path = path.substr(4);
    } else {
        std::replace(path.begin(), path.end(), '.', '/');
    }

    std::filesystem::path fullPath = std::filesystem::path(StdLibPath) / path;
    if (!fullPath.has_extension()) {
        fullPath += ".yu";
    }

    // 统一返回规范化绝对路径，避免接口阶段与依赖编译阶段的模块文件名不一致，
    // 导致符号 mangling 前缀不一致（相对路径 vs 绝对路径）。
    try {
        return std::filesystem::canonical(fullPath).string();
    } catch (...) {
        return std::filesystem::weakly_canonical(fullPath).string();
    }
}

std::string ModuleManager::resolveRelativePath(const std::string& modulePath,
                                               const std::string& currentFilePath) {
    std::filesystem::path base;
    if (!modulePath.empty() && modulePath[0] == '/') {
        base = std::filesystem::path(modulePath);
    } else {
        std::filesystem::path currentDir = std::filesystem::path(currentFilePath).parent_path();
        base = currentDir / modulePath;
    }

    if (!base.has_extension()) {
        base += ".yu";
    }

    try {
        return std::filesystem::canonical(base).string();
    } catch (...) {
        return std::filesystem::weakly_canonical(base).string();
    }
}

std::string ModuleManager::resolvePackageInterfacePath(const std::string& modulePath) const {
    if (modulePath.empty() || modulePath[0] == '.' || modulePath[0] == '/') {
        return "";
    }

    std::string rel = modulePathToRelFile(modulePath);
    for (const auto& pkgPath : PackagePaths) {
        std::filesystem::path p1 = std::filesystem::path(pkgPath) / "modules" / (rel + ".ymi");
        if (std::filesystem::exists(p1)) {
            return p1.string();
        }
        std::filesystem::path p2 = std::filesystem::path(pkgPath) / (rel + ".ymi");
        if (std::filesystem::exists(p2)) {
            return p2.string();
        }
    }
    return "";
}

std::string ModuleManager::normalizeModuleName(const std::string& modulePath) {
    if (modulePath.find("std/") == 0) {
        std::string out = modulePath;
        std::replace(out.begin(), out.end(), '/', '.');
        return out;
    }
    return modulePath;
}

std::string ModuleManager::buildCacheKey(const std::string& moduleFilePath) const {
    std::filesystem::path p(moduleFilePath);
    try {
        p = std::filesystem::weakly_canonical(p);
    } catch (...) {
        p = p.lexically_normal();
    }
    std::string keyInput = p.generic_string();
    return fnv1a64Hex(keyInput);
}

std::string ModuleManager::getInterfacePathForKey(const std::string& cacheKey) const {
    return (std::filesystem::path(ModuleCacheDir) / (cacheKey + ".ymi")).string();
}

std::string ModuleManager::getObjectPathForKey(const std::string& cacheKey) const {
    return (std::filesystem::path(ModuleCacheDir) / (cacheKey + ".o")).string();
}

void ModuleManager::buildModuleExports(ModuleInfo& moduleInfo) {
    moduleInfo.Exports.clear();
    moduleInfo.Dependencies.clear();

    CodeGen symbolGen(Ctx, moduleInfo.Name);
    std::unordered_set<std::string> depSet;

    auto addGenericParams = [](const std::vector<GenericParam>& params,
                               ModuleExport& exp) {
        exp.GenericParams.reserve(params.size());
        for (const auto& gp : params) {
            exp.GenericParams.push_back(gp.Name);
        }
    };

    auto addFunctionExport = [&](FuncDecl* funcDecl, Type* ownerType) {
        if (!funcDecl || funcDecl->getVisibility() != Visibility::Public) {
            return;
        }
        ModuleExport exp;
        exp.ExportKind = ModuleExport::Kind::Function;
        exp.Name = funcDecl->getName();
        exp.SemanticType = funcDecl->getSemanticType();
        exp.LinkName = symbolGen.getFunctionSymbolName(funcDecl);
        funcDecl->setLinkName(exp.LinkName);
        exp.DeclNode = funcDecl;
        exp.ImplOwnerType = ownerType;
        addGenericParams(funcDecl->getGenericParams(), exp);
        moduleInfo.Exports.push_back(std::move(exp));
    };

    for (Decl* decl : moduleInfo.Declarations) {
        if (!decl) {
            continue;
        }

        ModuleExport exp;
        exp.DeclNode = decl;

        switch (decl->getKind()) {
            case ASTNode::Kind::VarDecl: {
                auto* varDecl = static_cast<VarDecl*>(decl);
                if (varDecl->getVisibility() != Visibility::Public) {
                    continue;
                }
                exp.Name = varDecl->getName();
                exp.ExportKind = ModuleExport::Kind::Variable;
                exp.SemanticType = varDecl->getSemanticType();
                exp.LinkName = symbolGen.getGlobalSymbolName(varDecl, varDecl->getName(), 'V');
                break;
            }
            case ASTNode::Kind::ConstDecl: {
                auto* constDecl = static_cast<ConstDecl*>(decl);
                if (constDecl->getVisibility() != Visibility::Public) {
                    continue;
                }
                exp.Name = constDecl->getName();

                std::string depPath;
                if (isImportBuiltinExpr(constDecl->getInit(), depPath)) {
                    exp.ExportKind = ModuleExport::Kind::ModuleAlias;
                    exp.ModulePath = depPath;
                    exp.SemanticType = constDecl->getSemanticType();
                    if (!depPath.empty() && depSet.insert(depPath).second) {
                        moduleInfo.Dependencies.push_back(depPath);
                    }
                } else {
                    exp.ExportKind = ModuleExport::Kind::Constant;
                    exp.SemanticType = constDecl->getSemanticType();
                    exp.LinkName = symbolGen.getGlobalSymbolName(constDecl, constDecl->getName(), 'C');
                }
                break;
            }
            case ASTNode::Kind::FuncDecl: {
                auto* funcDecl = static_cast<FuncDecl*>(decl);
                addFunctionExport(funcDecl, nullptr);
                continue;
            }
            case ASTNode::Kind::StructDecl: {
                auto* structDecl = static_cast<StructDecl*>(decl);
                if (structDecl->getVisibility() != Visibility::Public) {
                    continue;
                }
                exp.Name = structDecl->getName();
                exp.ExportKind = ModuleExport::Kind::Struct;
                exp.SemanticType = structDecl->getSemanticType();
                addGenericParams(structDecl->getGenericParams(), exp);
                if (exp.SemanticType && exp.SemanticType->isStruct()) {
                    auto* structTy = static_cast<StructType*>(exp.SemanticType);
                    for (const auto& field : structTy->getFields()) {
                        exp.StructFields.emplace_back(field.Name, field.FieldType);
                    }
                }
                break;
            }
            case ASTNode::Kind::EnumDecl: {
                auto* enumDecl = static_cast<EnumDecl*>(decl);
                if (enumDecl->getVisibility() != Visibility::Public) {
                    continue;
                }
                exp.Name = enumDecl->getName();
                exp.ExportKind = ModuleExport::Kind::Enum;
                exp.SemanticType = enumDecl->getSemanticType();
                addGenericParams(enumDecl->getGenericParams(), exp);
                break;
            }
            case ASTNode::Kind::TraitDecl: {
                auto* traitDecl = static_cast<TraitDecl*>(decl);
                if (traitDecl->getVisibility() != Visibility::Public) {
                    continue;
                }
                exp.Name = traitDecl->getName();
                exp.ExportKind = ModuleExport::Kind::Trait;
                exp.SemanticType = traitDecl->getSemanticType();
                addGenericParams(traitDecl->getGenericParams(), exp);
                break;
            }
            case ASTNode::Kind::TypeAliasDecl: {
                auto* aliasDecl = static_cast<TypeAliasDecl*>(decl);
                if (aliasDecl->getVisibility() != Visibility::Public) {
                    continue;
                }
                exp.Name = aliasDecl->getName();
                exp.ExportKind = ModuleExport::Kind::TypeAlias;
                exp.SemanticType = aliasDecl->getSemanticType();
                addGenericParams(aliasDecl->getGenericParams(), exp);
                break;
            }
            case ASTNode::Kind::ImplDecl: {
                auto* implDecl = static_cast<ImplDecl*>(decl);
                Type* ownerType = implDecl->getSemanticTargetType();
                for (FuncDecl* method : implDecl->getMethods()) {
                    addFunctionExport(method, ownerType);
                }
                continue;
            }
            default:
                continue;
        }

        if (exp.Name.empty()) {
            continue;
        }
        moduleInfo.Exports.push_back(std::move(exp));
    }
}

bool ModuleManager::writeModuleInterface(ModuleInfo& moduleInfo) {
    if (moduleInfo.InterfacePath.empty()) {
        return false;
    }
    if (moduleInfo.Exports.empty()) {
        buildModuleExports(moduleInfo);
    }

    std::filesystem::path interfacePath(moduleInfo.InterfacePath);
    std::error_code ec;
    std::filesystem::create_directories(interfacePath.parent_path(), ec);
    if (ec) {
        return false;
    }

    std::ofstream out(moduleInfo.InterfacePath, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }

    out << "YMI2\n";
    out << "module\t" << moduleInfo.Name << "\n";
    out << "source\t" << moduleInfo.FilePath << "\n";
    out << "object\t" << moduleInfo.ObjectPath << "\n";
    for (const auto& dep : moduleInfo.Dependencies) {
        out << "dep\t" << dep << "\n";
    }

    for (const auto& exp : moduleInfo.Exports) {
        switch (exp.ExportKind) {
            case ModuleExport::Kind::Function: {
                std::string ownerEnc = exp.ImplOwnerType
                    ? typecodec::encode(exp.ImplOwnerType)
                    : "-";
                out << "export\tfunc\t" << exp.Name << "\t" << exp.LinkName
                    << "\t" << typecodec::encode(exp.SemanticType)
                    << "\t" << ownerEnc
                    << "\t" << exp.GenericParams.size();
                for (const auto& gp : exp.GenericParams) {
                    out << "\t" << gp;
                }
                out << "\n";
                break;
            }
            case ModuleExport::Kind::Variable: {
                out << "export\tvar\t" << exp.Name << "\t" << exp.LinkName
                    << "\t" << typecodec::encode(exp.SemanticType) << "\n";
                break;
            }
            case ModuleExport::Kind::Constant: {
                out << "export\tconst\t" << exp.Name << "\t" << exp.LinkName
                    << "\t" << typecodec::encode(exp.SemanticType) << "\n";
                break;
            }
            case ModuleExport::Kind::Struct: {
                out << "export\tstruct\t" << exp.Name
                    << "\t" << typecodec::encode(exp.SemanticType)
                    << "\t" << exp.GenericParams.size();
                for (const auto& gp : exp.GenericParams) {
                    out << "\t" << gp;
                }
                out << "\tF\t" << exp.StructFields.size();
                for (const auto& field : exp.StructFields) {
                    out << "\t" << field.first
                        << "\t" << typecodec::encode(field.second);
                }
                out << "\n";
                break;
            }
            case ModuleExport::Kind::Enum: {
                out << "export\tenum\t" << exp.Name
                    << "\t" << typecodec::encode(exp.SemanticType)
                    << "\t" << exp.GenericParams.size();
                for (const auto& gp : exp.GenericParams) {
                    out << "\t" << gp;
                }
                out << "\n";
                break;
            }
            case ModuleExport::Kind::Trait: {
                out << "export\ttrait\t" << exp.Name
                    << "\t" << typecodec::encode(exp.SemanticType)
                    << "\t" << exp.GenericParams.size();
                for (const auto& gp : exp.GenericParams) {
                    out << "\t" << gp;
                }
                out << "\n";
                break;
            }
            case ModuleExport::Kind::TypeAlias: {
                out << "export\talias\t" << exp.Name
                    << "\t" << typecodec::encode(exp.SemanticType)
                    << "\t" << exp.GenericParams.size();
                for (const auto& gp : exp.GenericParams) {
                    out << "\t" << gp;
                }
                out << "\n";
                break;
            }
            case ModuleExport::Kind::ModuleAlias: {
                out << "export\tmodule\t" << exp.Name << "\t" << exp.ModulePath << "\n";
                break;
            }
        }
    }

    return true;
}

bool ModuleManager::loadModuleInterface(ModuleInfo& moduleInfo,
                                        const std::string& interfacePath,
                                        std::vector<std::string>& importChain) {
    auto fail = [&](const std::string&) -> bool {
        return false;
    };

    std::ifstream in(interfacePath, std::ios::binary);
    if (!in) {
        return fail("open");
    }

    std::string line;
    if (!std::getline(in, line) || line != "YMI2") {
        return fail("magic");
    }

    std::vector<ModuleExport> exports;
    std::vector<std::string> deps;

    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }

        auto cols = splitTabs(line);
        if (cols.empty()) {
            continue;
        }

        if (cols[0] == "module" && cols.size() >= 2) {
            moduleInfo.Name = cols[1];
            continue;
        }
        if (cols[0] == "source" && cols.size() >= 2) {
            moduleInfo.FilePath = cols[1];
            continue;
        }
        if (cols[0] == "object" && cols.size() >= 2) {
            moduleInfo.ObjectPath = cols[1];
            continue;
        }
        if (cols[0] == "dep" && cols.size() >= 2) {
            deps.push_back(cols[1]);
            continue;
        }
        if (cols[0] != "export" || cols.size() < 3) {
            continue;
        }

        ModuleExport exp;
        const std::string& kind = cols[1];
        exp.Name = cols[2];

        if (kind == "func") {
            if (cols.size() < 7) return fail("func.cols");
            exp.ExportKind = ModuleExport::Kind::Function;
            exp.LinkName = cols[3];
            exp.SemanticType = typecodec::decode(cols[4], Ctx);
            if (!exp.SemanticType || !exp.SemanticType->isFunction()) {
                return fail("func.type");
            }

            if (cols[5] != "-") {
                exp.ImplOwnerType = typecodec::decode(cols[5], Ctx);
                if (!exp.ImplOwnerType) {
                    return fail("func.owner");
                }
            }

            uint64_t genericCount = 0;
            try {
                genericCount = static_cast<uint64_t>(std::stoull(cols[6]));
            } catch (...) {
                return fail("func.generic_count_parse");
            }
            if (cols.size() < 7 + genericCount) {
                return fail("func.generic_cols");
            }
            for (size_t i = 0; i < genericCount; ++i) {
                exp.GenericParams.push_back(cols[7 + i]);
            }

            auto* fnTy = static_cast<FunctionType*>(exp.SemanticType);
            std::vector<ParamDecl*> params;
            params.reserve(fnTy->getParamCount());
            auto isOwnerType = [&](Type* ty) -> bool {
                if (!ty || !exp.ImplOwnerType) {
                    return false;
                }
                if (ty->isGenericInstance()) {
                    ty = static_cast<GenericInstanceType*>(ty)->getBaseType();
                }
                Type* ownerTy = exp.ImplOwnerType;
                if (ownerTy->isGenericInstance()) {
                    ownerTy = static_cast<GenericInstanceType*>(ownerTy)->getBaseType();
                }
                return ty->isEqual(ownerTy);
            };
            for (size_t i = 0; i < fnTy->getParamCount(); ++i) {
                Type* paramTy = fnTy->getParam(i);
                ParamDecl* paramDecl = nullptr;

                if (i == 0 && exp.ImplOwnerType) {
                    if (paramTy->isReference()) {
                        auto* refTy = static_cast<ReferenceType*>(paramTy);
                        if (isOwnerType(refTy->getPointeeType())) {
                            paramDecl = ParamDecl::createSelf(
                                SourceRange(),
                                refTy->isMutable() ? ParamDecl::ParamKind::MutRefSelf
                                                   : ParamDecl::ParamKind::RefSelf);
                        }
                    } else if (isOwnerType(paramTy)) {
                        paramDecl = ParamDecl::createSelf(SourceRange(), ParamDecl::ParamKind::Self);
                    }
                }

                if (!paramDecl && fnTy->isVariadic() &&
                    i + 1 == fnTy->getParamCount() && paramTy->isVarArgs()) {
                    paramDecl = ParamDecl::createVariadic(SourceRange(), "args", nullptr);
                }

                if (!paramDecl) {
                    paramDecl = Ctx.create<ParamDecl>(
                        SourceRange(), "p" + std::to_string(i), nullptr, nullptr, false);
                }
                paramDecl->setSemanticType(paramTy);
                params.push_back(paramDecl);
            }

            Type* retType = fnTy->getReturnType();
            auto* stub = Ctx.create<FuncDecl>(
                SourceRange(),
                exp.Name,
                std::move(params),
                nullptr,
                nullptr,
                false,
                fnTy->canError(),
                Visibility::Public);
            std::vector<GenericParam> gp;
            gp.reserve(exp.GenericParams.size());
            for (const auto& g : exp.GenericParams) {
                gp.emplace_back(g, SourceLocation());
            }
            stub->setGenericParams(std::move(gp));
            (void)retType;
            stub->setSemanticType(fnTy);
            stub->setLinkName(exp.LinkName);
            exp.DeclNode = stub;
            moduleInfo.Declarations.push_back(stub);
            if (exp.ImplOwnerType) {
                Ctx.registerImplMethod(exp.ImplOwnerType, stub);
            }
        } else if (kind == "var" || kind == "const") {
            if (cols.size() < 5) return fail("global.cols");
            exp.ExportKind = (kind == "var") ? ModuleExport::Kind::Variable : ModuleExport::Kind::Constant;
            exp.LinkName = cols[3];
            exp.SemanticType = typecodec::decode(cols[4], Ctx);
            if (!exp.SemanticType) return fail("global.type");

            if (kind == "var") {
                auto* stub = Ctx.create<VarDecl>(
                    SourceRange(),
                    exp.Name,
                    nullptr,
                    nullptr,
                    true,
                    Visibility::Public,
                    nullptr);
                stub->setSemanticType(exp.SemanticType);
                exp.DeclNode = stub;
                moduleInfo.Declarations.push_back(stub);
            } else {
                auto* stub = Ctx.create<ConstDecl>(
                    SourceRange(),
                    exp.Name,
                    nullptr,
                    nullptr,
                    Visibility::Public);
                stub->setSemanticType(exp.SemanticType);
                exp.DeclNode = stub;
                moduleInfo.Declarations.push_back(stub);
            }
        } else if (kind == "module") {
            if (cols.size() < 4) return fail("module.cols");
            exp.ExportKind = ModuleExport::Kind::ModuleAlias;
            exp.ModulePath = cols[3];
            deps.push_back(exp.ModulePath);
            // 具体模块类型在 resolveModuleType 构建成员时再递归解析。
            auto* stub = Ctx.create<ConstDecl>(
                SourceRange(), exp.Name, nullptr, nullptr, Visibility::Public);
            exp.DeclNode = stub;
            moduleInfo.Declarations.push_back(stub);
        } else if (kind == "struct" || kind == "enum" || kind == "trait" || kind == "alias") {
            if (cols.size() < 5) return fail("type.cols");

            uint64_t genericCount = 0;
            try {
                genericCount = static_cast<uint64_t>(std::stoull(cols[4]));
            } catch (...) {
                return fail("type.generic_count_parse");
            }
            if (cols.size() < 5 + genericCount) {
                return fail("type.generic_cols");
            }
            for (size_t i = 0; i < genericCount; ++i) {
                exp.GenericParams.push_back(cols[5 + i]);
            }
            size_t fieldCursor = 5 + genericCount;
            if (kind == "struct" && cols.size() > fieldCursor) {
                if (cols[fieldCursor] != "F" || cols.size() < fieldCursor + 2) {
                    return fail("struct.field_header");
                }
                uint64_t fieldCount = 0;
                try {
                    fieldCount = static_cast<uint64_t>(std::stoull(cols[fieldCursor + 1]));
                } catch (...) {
                    return fail("struct.field_count_parse");
                }
                if (cols.size() < fieldCursor + 2 + fieldCount * 2) {
                    return fail("struct.field_cols");
                }
                for (size_t i = 0; i < fieldCount; ++i) {
                    size_t base = fieldCursor + 2 + i * 2;
                    Type* fieldTy = typecodec::decode(cols[base + 1], Ctx);
                    if (!fieldTy) {
                        return fail("struct.field_type_decode");
                    }
                    exp.StructFields.emplace_back(cols[base], fieldTy);
                }
            }

            if (kind == "struct" && !exp.StructFields.empty()) {
                std::vector<Type*> fieldTypes;
                std::vector<std::string> fieldNames;
                fieldTypes.reserve(exp.StructFields.size());
                fieldNames.reserve(exp.StructFields.size());
                for (const auto& field : exp.StructFields) {
                    fieldNames.push_back(field.first);
                    fieldTypes.push_back(field.second);
                }
                exp.SemanticType = Ctx.getStructType(exp.Name,
                                                     std::move(fieldTypes),
                                                     std::move(fieldNames));
            } else {
                exp.SemanticType = typecodec::decode(cols[3], Ctx);
                if (!exp.SemanticType) return fail("type.decode");
            }

            if (kind == "struct") {
                exp.ExportKind = ModuleExport::Kind::Struct;
                auto* stub = Ctx.create<StructDecl>(
                    SourceRange(), exp.Name, std::vector<FieldDecl*>{}, Visibility::Public);
                std::vector<GenericParam> gp;
                gp.reserve(exp.GenericParams.size());
                for (const auto& g : exp.GenericParams) {
                    gp.emplace_back(g, SourceLocation());
                }
                stub->setGenericParams(std::move(gp));
                stub->setSemanticType(exp.SemanticType);
                exp.DeclNode = stub;
                moduleInfo.Declarations.push_back(stub);
            } else if (kind == "enum") {
                exp.ExportKind = ModuleExport::Kind::Enum;
                auto* stub = Ctx.create<EnumDecl>(
                    SourceRange(), exp.Name, std::vector<EnumVariantDecl*>{}, Visibility::Public);
                std::vector<GenericParam> gp;
                gp.reserve(exp.GenericParams.size());
                for (const auto& g : exp.GenericParams) {
                    gp.emplace_back(g, SourceLocation());
                }
                stub->setGenericParams(std::move(gp));
                stub->setSemanticType(exp.SemanticType);
                exp.DeclNode = stub;
                moduleInfo.Declarations.push_back(stub);
            } else if (kind == "trait") {
                exp.ExportKind = ModuleExport::Kind::Trait;
                auto* stub = Ctx.create<TraitDecl>(
                    SourceRange(), exp.Name, std::vector<FuncDecl*>{},
                    std::vector<TypeAliasDecl*>{}, Visibility::Public);
                std::vector<GenericParam> gp;
                gp.reserve(exp.GenericParams.size());
                for (const auto& g : exp.GenericParams) {
                    gp.emplace_back(g, SourceLocation());
                }
                stub->setGenericParams(std::move(gp));
                stub->setSemanticType(exp.SemanticType);
                exp.DeclNode = stub;
                moduleInfo.Declarations.push_back(stub);
            } else {
                exp.ExportKind = ModuleExport::Kind::TypeAlias;
                auto* stub = Ctx.create<TypeAliasDecl>(
                    SourceRange(), exp.Name, nullptr, Visibility::Public);
                std::vector<GenericParam> gp;
                gp.reserve(exp.GenericParams.size());
                for (const auto& g : exp.GenericParams) {
                    gp.emplace_back(g, SourceLocation());
                }
                stub->setGenericParams(std::move(gp));
                stub->setSemanticType(exp.SemanticType);
                exp.DeclNode = stub;
                moduleInfo.Declarations.push_back(stub);
            }
        } else {
            return fail("unknown.export_kind");
        }

        exports.push_back(std::move(exp));
    }

    moduleInfo.Exports = std::move(exports);
    moduleInfo.Dependencies = std::move(deps);
    moduleInfo.InterfacePath = interfacePath;
    moduleInfo.IsFromInterface = true;
    moduleInfo.IsLoaded = true;

    // 包接口中 module alias 依赖在这里提前触发加载，确保后续成员类型可用。
    for (auto& exp : moduleInfo.Exports) {
        if (exp.ExportKind != ModuleExport::Kind::ModuleAlias) {
            continue;
        }

        SourceLocation loc;
        if (!moduleInfo.FilePath.empty()) {
            SourceManager::FileID fid = SourceMgr.loadFile(moduleInfo.FilePath);
            if (fid != SourceManager::InvalidFileID) {
                loc = SourceMgr.getLocation(fid, 0);
            }
        }
        Type* moduleTy = Sema_.resolveModuleType(exp.ModulePath, loc);
        if (moduleTy) {
            exp.SemanticType = moduleTy;
            if (exp.DeclNode) {
                exp.DeclNode->setSemanticType(moduleTy);
            }
        }
    }

    // 尽量让接口里记录的依赖也进入已加载集合（失败时延迟到真实访问再报错）。
    for (const auto& dep : moduleInfo.Dependencies) {
        (void)loadModule(dep, moduleInfo.FilePath, importChain);
    }

    return true;
}

ModuleInfo* ModuleManager::loadModule(const std::string& modulePath,
                                      const std::string& currentFilePath,
                                      std::vector<std::string>& importChain) {
    if (modulePath.empty()) {
        return nullptr;
    }

    std::string fullPath = resolveModulePath(modulePath, currentFilePath);
    std::string moduleName = normalizeModuleName(modulePath);
    // 对相对模块，使用规范化绝对路径（去扩展名）做唯一名，避免同名冲突。
    if (!modulePath.empty() && (modulePath[0] == '.' || modulePath[0] == '/')) {
        std::filesystem::path p(fullPath);
        try {
            p = std::filesystem::weakly_canonical(p);
        } catch (...) {
            p = p.lexically_normal();
        }
        if (p.has_extension()) {
            p.replace_extension();
        }
        moduleName = p.generic_string();
    }

    auto it = LoadedModules.find(moduleName);
    if (it != LoadedModules.end()) {
        return it->second.get();
    }

    if (isInImportChain(moduleName, importChain)) {
        return nullptr;
    }
    importChain.push_back(moduleName);

    bool isStdLib = (modulePath.find("std.") == 0 || modulePath.find("std/") == 0);
    auto moduleInfo = std::make_unique<ModuleInfo>(moduleName, fullPath, isStdLib);

    // 缓存路径（按源码路径生成）
    if (!fullPath.empty()) {
        std::string key = buildCacheKey(fullPath);
        moduleInfo->InterfacePath = getInterfacePathForKey(key);
        moduleInfo->ObjectPath = getObjectPathForKey(key);
    }

    // 包接口路径（按逻辑模块名）
    std::string packageInterface = resolvePackageInterfacePath(modulePath);
    if (!packageInterface.empty()) {
        std::filesystem::path ip(packageInterface);
        moduleInfo->InterfacePath = packageInterface;
        if (moduleInfo->ObjectPath.empty()) {
            std::string objPath = packageInterface;
            size_t pos = objPath.rfind("/modules/");
            if (pos != std::string::npos) {
                objPath.replace(pos, 9, "/objects/");
                std::filesystem::path objP(objPath);
                objP.replace_extension(".o");
                moduleInfo->ObjectPath = objP.string();
            } else {
                std::filesystem::path objP(packageInterface);
                objP.replace_extension(".o");
                moduleInfo->ObjectPath = objP.string();
            }
        }
    }

    auto tryLoadInterface = [&](const std::string& interfacePath) -> bool {
        if (interfacePath.empty() || !std::filesystem::exists(interfacePath)) {
            return false;
        }

        // 若源码存在，只有接口不旧于源码时才使用。
        if (!moduleInfo->FilePath.empty() && std::filesystem::exists(moduleInfo->FilePath)) {
            std::error_code ec1, ec2;
            auto srcTime = std::filesystem::last_write_time(moduleInfo->FilePath, ec1);
            auto ifcTime = std::filesystem::last_write_time(interfacePath, ec2);
            if (!ec1 && !ec2 && ifcTime < srcTime) {
                return false;
            }
        }

        return loadModuleInterface(*moduleInfo, interfacePath, importChain);
    };

    if (tryLoadInterface(packageInterface) || tryLoadInterface(moduleInfo->InterfacePath)) {
        bool hasGenericFunctionExport = false;
        for (const auto& exp : moduleInfo->Exports) {
            if (exp.ExportKind == ModuleExport::Kind::Function &&
                (!exp.GenericParams.empty() ||
                 typeHasGenericParam(exp.SemanticType) ||
                 typeHasGenericParam(exp.ImplOwnerType))) {
                hasGenericFunctionExport = true;
                break;
            }
        }

        bool canFallbackToSource =
            !moduleInfo->FilePath.empty() && std::filesystem::exists(moduleInfo->FilePath);
        if (!hasGenericFunctionExport || !canFallbackToSource) {
            ModuleInfo* result = moduleInfo.get();
            LoadedModules[moduleName] = std::move(moduleInfo);
            importChain.pop_back();
            return result;
        }

        // 泛型函数需要源码以支持跨模块实例化，接口仅用于缓存元数据。
        moduleInfo->Declarations.clear();
        moduleInfo->Exports.clear();
        moduleInfo->Dependencies.clear();
        moduleInfo->IsLoaded = false;
        moduleInfo->IsFromInterface = false;
    }

    if (moduleInfo->FilePath.empty() || !std::filesystem::exists(moduleInfo->FilePath)) {
        importChain.pop_back();
        return nullptr;
    }

    try {
        SourceManager::FileID fileID = SourceMgr.loadFile(moduleInfo->FilePath);
        if (fileID == SourceManager::InvalidFileID) {
            importChain.pop_back();
            return nullptr;
        }

        Lexer lexer(SourceMgr, Diag, fileID);
        Parser parser(lexer, Diag, Ctx);
        moduleInfo->Declarations = parser.parseCompilationUnit();

        if (Diag.hasErrors()) {
            importChain.pop_back();
            return nullptr;
        }

        SymbolTable& symbols = Sema_.getSymbolTable();
        symbols.enterScope(Scope::Kind::Module);
        bool analyzeFailed = false;
        for (Decl* decl : moduleInfo->Declarations) {
            bool ok = Sema_.analyzeDecl(decl);
            if (!ok) {
                analyzeFailed = true;
                break;
            }
        }
        symbols.exitScope();

        if (analyzeFailed || Diag.hasErrors()) {
            importChain.pop_back();
            return nullptr;
        }

        moduleInfo->IsLoaded = true;
        moduleInfo->IsFromInterface = false;
        buildModuleExports(*moduleInfo);
        bool wroteInterface = writeModuleInterface(*moduleInfo);
        bool hasGenericExport = false;
        for (const auto& exp : moduleInfo->Exports) {
            if (exp.ExportKind == ModuleExport::Kind::Function &&
                (!exp.GenericParams.empty() ||
                 typeHasGenericParam(exp.SemanticType) ||
                 typeHasGenericParam(exp.ImplOwnerType))) {
                hasGenericExport = true;
                break;
            }
        }
        if (!hasGenericExport &&
            wroteInterface && std::filesystem::exists(moduleInfo->InterfacePath)) {
            // 即使首次从源码加载，也尽量切换到接口桩，避免把导入模块实现体带入当前编译单元。
            // 若接口回读失败，回退到源码结果，不让导入流程失败。
            std::vector<Decl*> sourceDecls = moduleInfo->Declarations;
            std::vector<ModuleExport> sourceExports = moduleInfo->Exports;
            std::vector<std::string> sourceDeps = moduleInfo->Dependencies;

            moduleInfo->Declarations.clear();
            moduleInfo->Exports.clear();
            moduleInfo->Dependencies.clear();
            if (!loadModuleInterface(*moduleInfo, moduleInfo->InterfacePath, importChain)) {
                moduleInfo->Declarations = std::move(sourceDecls);
                moduleInfo->Exports = std::move(sourceExports);
                moduleInfo->Dependencies = std::move(sourceDeps);
                moduleInfo->IsLoaded = true;
                moduleInfo->IsFromInterface = false;
            }
        }
    } catch (...) {
        importChain.pop_back();
        return nullptr;
    }

    ModuleInfo* result = moduleInfo.get();
    LoadedModules[moduleName] = std::move(moduleInfo);
    importChain.pop_back();
    return result;
}

ModuleInfo* ModuleManager::getLoadedModule(const std::string& moduleName) {
    auto it = LoadedModules.find(moduleName);
    if (it != LoadedModules.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool ModuleManager::isInImportChain(const std::string& moduleName,
                                    const std::vector<std::string>& importChain) const {
    return std::find(importChain.begin(), importChain.end(), moduleName) != importChain.end();
}

} // namespace yuan
