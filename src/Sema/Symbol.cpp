//===--- Symbol.cpp - 符号实现 -----------------------------------------===//
//
// Yuan 编译器
//
//===----------------------------------------------------------------------===//

#include "yuan/Sema/Symbol.h"
#include "yuan/Sema/Type.h"
#include "yuan/AST/Decl.h"
#include "llvm/ADT/StringRef.h"

namespace yuan {

Symbol::Symbol(SymbolKind kind, llvm::StringRef name, Type* type,
               SourceLocation loc, Visibility vis)
    : Kind(kind), Name(name.str()), SymType(type), Loc(loc), Vis(vis) {
}

const char* Symbol::getKindName(SymbolKind kind) {
    switch (kind) {
        case SymbolKind::Variable:
            return "variable";
        case SymbolKind::Constant:
            return "constant";
        case SymbolKind::Function:
            return "function";
        case SymbolKind::Parameter:
            return "parameter";
        case SymbolKind::Struct:
            return "struct";
        case SymbolKind::Enum:
            return "enum";
        case SymbolKind::EnumVariant:
            return "enum variant";
        case SymbolKind::Trait:
            return "trait";
        case SymbolKind::TypeAlias:
            return "type alias";
        case SymbolKind::Field:
            return "field";
        case SymbolKind::Method:
            return "method";
        case SymbolKind::GenericParam:
            return "generic parameter";
    }
    return "unknown";
}

} // namespace yuan