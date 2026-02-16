//===--- Scope.cpp - 作用域实现 ----------------------------------------===//
//
// Yuan 编译器
//
//===----------------------------------------------------------------------===//

#include "yuan/Sema/Scope.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/Sema/Type.h"
#include "llvm/ADT/StringRef.h"

namespace yuan {

Scope::Scope(Kind kind, Scope* parent)
    : ScopeKind(kind), Parent(parent) {
    // 如果父作用域有当前函数，继承它
    if (parent && parent->getCurrentFunction()) {
        CurrentFunc = parent->getCurrentFunction();
    }
}

bool Scope::addSymbol(Symbol* sym) {
    if (!sym) {
        return false;
    }

    std::string name = sym->getName().str();

    // 检查是否已存在同名符号
    if (Symbols.find(name) != Symbols.end()) {
        return false;
    }

    Symbols[name] = sym;
    return true;
}

Symbol* Scope::lookupLocal(llvm::StringRef name) const {
    auto it = Symbols.find(name.str());
    if (it != Symbols.end()) {
        return it->second;
    }
    return nullptr;
}

Symbol* Scope::lookup(llvm::StringRef name) const {
    // 先在当前作用域查找
    Symbol* sym = lookupLocal(name);
    if (sym) {
        return sym;
    }

    // 在父作用域中递归查找
    if (Parent) {
        return Parent->lookup(name);
    }

    return nullptr;
}

bool Scope::isInLoop() const {
    // 检查当前作用域或任何父作用域是否为循环作用域
    const Scope* current = this;
    while (current) {
        if (current->getKind() == Kind::Loop) {
            return true;
        }
        current = current->getParent();
    }
    return false;
}

bool Scope::isInFunction() const {
    // 检查当前作用域或任何父作用域是否为函数作用域
    const Scope* current = this;
    while (current) {
        if (current->getKind() == Kind::Function) {
            return true;
        }
        current = current->getParent();
    }
    return false;
}

FuncDecl* Scope::getCurrentFunction() const {
    // 向上查找函数作用域
    const Scope* current = this;
    while (current) {
        if (current->CurrentFunc) {
            return current->CurrentFunc;
        }
        current = current->getParent();
    }
    return nullptr;
}

const char* Scope::getKindName(Kind kind) {
    switch (kind) {
        case Kind::Global:
            return "global";
        case Kind::Module:
            return "module";
        case Kind::Function:
            return "function";
        case Kind::Block:
            return "block";
        case Kind::Struct:
            return "struct";
        case Kind::Enum:
            return "enum";
        case Kind::Trait:
            return "trait";
        case Kind::Impl:
            return "impl";
        case Kind::Loop:
            return "loop";
    }
    return "unknown";
}

//===----------------------------------------------------------------------===//
// SymbolTable 实现
//===----------------------------------------------------------------------===//

SymbolTable::SymbolTable(ASTContext& ctx) : Ctx(ctx) {
    // 创建全局作用域
    auto globalScope = std::make_unique<Scope>(Scope::Kind::Global);
    GlobalScope = globalScope.get();
    CurrentScope = GlobalScope;
    AllScopes.push_back(std::move(globalScope));

    // 注册内置类型
    registerBuiltinTypes();
}

SymbolTable::~SymbolTable() = default;

void SymbolTable::enterScope(Scope::Kind kind, const std::string& label) {
    auto newScope = std::make_unique<Scope>(kind, CurrentScope);
    if (kind == Scope::Kind::Loop && !label.empty()) {
        newScope->setLoopLabel(label);
    }
    CurrentScope = newScope.get();
    AllScopes.push_back(std::move(newScope));
}

void SymbolTable::exitScope() {
    if (CurrentScope && CurrentScope != GlobalScope) {
        CurrentScope = CurrentScope->getParent();
    }
}

bool SymbolTable::addSymbol(Symbol* sym) {
    if (!CurrentScope) {
        return false;
    }
    return CurrentScope->addSymbol(sym);
}

Symbol* SymbolTable::lookup(llvm::StringRef name) const {
    if (!CurrentScope) {
        return nullptr;
    }
    return CurrentScope->lookup(name);
}

Symbol* SymbolTable::lookupType(llvm::StringRef name) const {
    Symbol* sym = lookup(name);
    if (sym && sym->isType()) {
        return sym;
    }
    return nullptr;
}

size_t SymbolTable::getScopeDepth() const {
    size_t depth = 0;
    const Scope* current = CurrentScope;
    while (current) {
        depth++;
        current = current->getParent();
    }
    return depth;
}

void SymbolTable::registerBuiltinTypes() {
    auto addType = [&](SymbolKind kind, const char* name, Type* type) {
        auto* sym = new Symbol(kind, name, type, SourceLocation(), Visibility::Public);
        if (!GlobalScope->addSymbol(sym)) {
            delete sym;
        }
    };

    // 基本类型
    addType(SymbolKind::TypeAlias, "void", Ctx.getVoidType());
    addType(SymbolKind::TypeAlias, "bool", Ctx.getBoolType());
    addType(SymbolKind::TypeAlias, "char", Ctx.getCharType());
    addType(SymbolKind::TypeAlias, "str", Ctx.getStrType());

    // 整数类型
    addType(SymbolKind::TypeAlias, "i8", Ctx.getI8Type());
    addType(SymbolKind::TypeAlias, "i16", Ctx.getI16Type());
    addType(SymbolKind::TypeAlias, "i32", Ctx.getI32Type());
    addType(SymbolKind::TypeAlias, "i64", Ctx.getI64Type());
    addType(SymbolKind::TypeAlias, "i128", Ctx.getIntegerType(128, true));
    addType(SymbolKind::TypeAlias, "isize", Ctx.getIntegerType(Ctx.getPointerBitWidth(), true));

    addType(SymbolKind::TypeAlias, "u8", Ctx.getU8Type());
    addType(SymbolKind::TypeAlias, "u16", Ctx.getU16Type());
    addType(SymbolKind::TypeAlias, "u32", Ctx.getU32Type());
    addType(SymbolKind::TypeAlias, "u64", Ctx.getU64Type());
    addType(SymbolKind::TypeAlias, "u128", Ctx.getIntegerType(128, false));
    addType(SymbolKind::TypeAlias, "usize", Ctx.getIntegerType(Ctx.getPointerBitWidth(), false));

    // 浮点类型
    addType(SymbolKind::TypeAlias, "f32", Ctx.getF32Type());
    addType(SymbolKind::TypeAlias, "f64", Ctx.getF64Type());

    // 系统内置错误类型 SysError
    {
        std::vector<Type*> variantData;
        std::vector<std::string> variantNames;

        // DivisionByZero (unit)
        variantData.push_back(nullptr);
        variantNames.emplace_back("DivisionByZero");

        // ParseError { message: str }
        std::vector<Type*> parseFieldTypes = {Ctx.getStrType()};
        std::vector<std::string> parseFieldNames = {"message"};
        Type* parseStruct = Ctx.getStructType("SysError::ParseError",
                                              std::move(parseFieldTypes),
                                              std::move(parseFieldNames));
        variantData.push_back(parseStruct);
        variantNames.emplace_back("ParseError");

        // PermissionDenied { path: str }
        std::vector<Type*> permFieldTypes = {Ctx.getStrType()};
        std::vector<std::string> permFieldNames = {"path"};
        Type* permStruct = Ctx.getStructType("SysError::PermissionDenied",
                                             std::move(permFieldTypes),
                                             std::move(permFieldNames));
        variantData.push_back(permStruct);
        variantNames.emplace_back("PermissionDenied");

        // FileNotFound { path: str }
        std::vector<Type*> notFoundFieldTypes = {Ctx.getStrType()};
        std::vector<std::string> notFoundFieldNames = {"path"};
        Type* notFoundStruct = Ctx.getStructType("SysError::FileNotFound",
                                                 std::move(notFoundFieldTypes),
                                                 std::move(notFoundFieldNames));
        variantData.push_back(notFoundStruct);
        variantNames.emplace_back("FileNotFound");

        Type* sysErrorType = Ctx.getEnumType("SysError", std::move(variantData), std::move(variantNames));
        addType(SymbolKind::Enum, "SysError", sysErrorType);
    }
}

} // namespace yuan
