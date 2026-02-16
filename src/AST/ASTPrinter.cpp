/// \file ASTPrinter.cpp
/// \brief AST 打印器实现。
///
/// 本文件实现了 ASTPrinter 类，将 AST 格式化输出为有效的 Yuan 源代码。

#include "yuan/AST/ASTPrinter.h"
#include <iomanip>
#include <sstream>

namespace yuan {

ASTPrinter::ASTPrinter(std::ostream& os, unsigned indentSize)
    : OS(os), IndentSize(indentSize), IndentLevel(0) {}

void ASTPrinter::indent() {
    for (unsigned i = 0; i < IndentLevel * IndentSize; ++i) {
        OS << ' ';
    }
}

void ASTPrinter::print(const ASTNode* node) {
    if (!node) return;
    
    if (node->isDecl()) {
        printDecl(static_cast<const Decl*>(node));
    } else if (node->isStmt()) {
        printStmt(static_cast<const Stmt*>(node));
    } else if (node->isExpr()) {
        printExpr(static_cast<const Expr*>(node));
    } else if (node->isTypeNode()) {
        printTypeNode(static_cast<const TypeNode*>(node));
    } else if (node->isPattern()) {
        printPattern(static_cast<const Pattern*>(node));
    }
}

// ============================================================================
// 声明打印
// ============================================================================

void ASTPrinter::printDecl(const Decl* decl) {
    if (!decl) return;
    
    switch (decl->getKind()) {
        case ASTNode::Kind::VarDecl:
            printVarDecl(static_cast<const VarDecl*>(decl));
            break;
        case ASTNode::Kind::ConstDecl:
            printConstDecl(static_cast<const ConstDecl*>(decl));
            break;
        case ASTNode::Kind::ParamDecl:
            printParamDecl(static_cast<const ParamDecl*>(decl));
            break;
        case ASTNode::Kind::FuncDecl:
            printFuncDecl(static_cast<const FuncDecl*>(decl));
            break;
        case ASTNode::Kind::FieldDecl:
            printFieldDecl(static_cast<const FieldDecl*>(decl));
            break;
        case ASTNode::Kind::StructDecl:
            printStructDecl(static_cast<const StructDecl*>(decl));
            break;
        case ASTNode::Kind::EnumVariantDecl:
            printEnumVariantDecl(static_cast<const EnumVariantDecl*>(decl));
            break;
        case ASTNode::Kind::EnumDecl:
            printEnumDecl(static_cast<const EnumDecl*>(decl));
            break;
        case ASTNode::Kind::TypeAliasDecl:
            printTypeAliasDecl(static_cast<const TypeAliasDecl*>(decl));
            break;
        case ASTNode::Kind::TraitDecl:
            printTraitDecl(static_cast<const TraitDecl*>(decl));
            break;
        case ASTNode::Kind::ImplDecl:
            printImplDecl(static_cast<const ImplDecl*>(decl));
            break;
        default:
            break;
    }
}

void ASTPrinter::printVarDecl(const VarDecl* decl) {
    if (!decl) return;

    printVisibility(decl->getVisibility());
    OS << "var ";
    if (decl->getPattern()) {
        printPattern(decl->getPattern());
    } else {
        OS << decl->getName();
    }
    
    if (decl->getType()) {
        OS << ": ";
        printTypeNode(decl->getType());
    }
    
    if (decl->getInit()) {
        OS << " = ";
        printExpr(decl->getInit());
    }
}

void ASTPrinter::printConstDecl(const ConstDecl* decl) {
    if (!decl) return;

    printVisibility(decl->getVisibility());
    OS << "const " << decl->getName();
    
    if (decl->getType()) {
        OS << ": ";
        printTypeNode(decl->getType());
    }
    
    if (decl->getInit()) {
        OS << " = ";
        printExpr(decl->getInit());
    }
}

void ASTPrinter::printParamDecl(const ParamDecl* decl) {
    if (!decl) return;

    switch (decl->getParamKind()) {
        case ParamDecl::ParamKind::Self:
            OS << "self";
            return;
        case ParamDecl::ParamKind::RefSelf:
            OS << "&self";
            return;
        case ParamDecl::ParamKind::MutRefSelf:
            OS << "&mut self";
            return;
        case ParamDecl::ParamKind::Variadic:
            OS << "..." << decl->getName();
            if (decl->getType()) {
                OS << ": ";
                printTypeNode(decl->getType());
            }
            return;
        case ParamDecl::ParamKind::Normal:
            break;
    }

    if (decl->isMutable()) {
        OS << "mut ";
    }
    OS << decl->getName();

    if (decl->getType()) {
        OS << ": ";
        printTypeNode(decl->getType());
    }
}

void ASTPrinter::printFuncDecl(const FuncDecl* decl) {
    if (!decl) return;
    
    printVisibility(decl->getVisibility());
    
    if (decl->isAsync()) {
        OS << "async ";
    }
    
    OS << "func " << decl->getName();
    
    // 泛型参数
    if (decl->isGeneric()) {
        printGenericParams(decl->getGenericParams());
    }
    
    // 参数列表
    OS << "(";
    const auto& params = decl->getParams();
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) OS << ", ";
        printParamDecl(params[i]);
    }
    OS << ")";
    
    // 返回类型
    if (decl->getReturnType()) {
        OS << " -> ";
        if (decl->canError()) {
            OS << "!";
        }
        printTypeNode(decl->getReturnType());
    }
    
    // 函数体
    if (decl->hasBody()) {
        OS << " ";
        printBlockStmt(decl->getBody());
    }
}

void ASTPrinter::printFieldDecl(const FieldDecl* decl) {
    if (!decl) return;
    
    printVisibility(decl->getVisibility());
    OS << decl->getName() << ": ";
    printTypeNode(decl->getType());

    if (decl->hasDefaultValue()) {
        OS << " = ";
        printExpr(decl->getDefaultValue());
    }
}

void ASTPrinter::printStructDecl(const StructDecl* decl) {
    if (!decl) return;
    
    printVisibility(decl->getVisibility());
    OS << "struct " << decl->getName();
    
    if (decl->isGeneric()) {
        printGenericParams(decl->getGenericParams());
    }
    
    OS << " {\n";
    increaseIndent();
    
    const auto& fields = decl->getFields();
    for (size_t i = 0; i < fields.size(); ++i) {
        indent();
        printFieldDecl(fields[i]);
        if (i < fields.size() - 1) {
            OS << ",";
        }
        OS << "\n";
    }
    
    decreaseIndent();
    indent();
    OS << "}";
}

void ASTPrinter::printEnumVariantDecl(const EnumVariantDecl* decl) {
    if (!decl) return;
    
    OS << decl->getName();
    
    switch (decl->getVariantKind()) {
        case EnumVariantDecl::VariantKind::Unit:
            // 无需额外输出
            break;
        case EnumVariantDecl::VariantKind::Tuple: {
            OS << "(";
            const auto& types = decl->getTupleTypes();
            for (size_t i = 0; i < types.size(); ++i) {
                if (i > 0) OS << ", ";
                printTypeNode(types[i]);
            }
            OS << ")";
            break;
        }
        case EnumVariantDecl::VariantKind::Struct: {
            OS << " {\n";
            increaseIndent();
            const auto& fields = decl->getFields();
            for (size_t i = 0; i < fields.size(); ++i) {
                indent();
                printFieldDecl(fields[i]);
                if (i < fields.size() - 1) {
                    OS << ",";
                }
                OS << "\n";
            }
            decreaseIndent();
            indent();
            OS << "}";
            break;
        }
    }
    
    if (decl->hasDiscriminant()) {
        OS << " = " << decl->getDiscriminant();
    }
}

void ASTPrinter::printEnumDecl(const EnumDecl* decl) {
    if (!decl) return;
    
    printVisibility(decl->getVisibility());
    OS << "enum " << decl->getName();
    
    if (decl->isGeneric()) {
        printGenericParams(decl->getGenericParams());
    }
    
    OS << " {\n";
    increaseIndent();
    
    const auto& variants = decl->getVariants();
    for (size_t i = 0; i < variants.size(); ++i) {
        indent();
        printEnumVariantDecl(variants[i]);
        if (i < variants.size() - 1) {
            OS << ",";
        }
        OS << "\n";
    }
    
    decreaseIndent();
    indent();
    OS << "}";
}

void ASTPrinter::printTypeAliasDecl(const TypeAliasDecl* decl) {
    if (!decl) return;
    
    printVisibility(decl->getVisibility());
    OS << "type " << decl->getName();
    
    if (decl->isGeneric()) {
        printGenericParams(decl->getGenericParams());
    }

    if (decl->hasTraitBounds()) {
        OS << ": ";
        const auto& bounds = decl->getTraitBounds();
        for (size_t i = 0; i < bounds.size(); ++i) {
            if (i > 0) OS << " + ";
            OS << bounds[i];
        }
    }
    
    if (decl->getAliasedType()) {
        OS << " = ";
        printTypeNode(decl->getAliasedType());
    }
}

void ASTPrinter::printTraitDecl(const TraitDecl* decl) {
    if (!decl) return;
    
    printVisibility(decl->getVisibility());
    OS << "trait " << decl->getName();
    
    if (decl->isGeneric()) {
        printGenericParams(decl->getGenericParams());
    }
    
    // 父 Trait
    const auto& superTraits = decl->getSuperTraits();
    if (!superTraits.empty()) {
        OS << ": ";
        for (size_t i = 0; i < superTraits.size(); ++i) {
            if (i > 0) OS << " + ";
            OS << superTraits[i];
        }
    }
    
    OS << " {\n";
    increaseIndent();
    
    // 关联类型
    for (const auto* assocType : decl->getAssociatedTypes()) {
        indent();
        printTypeAliasDecl(assocType);
        OS << "\n";
    }
    
    // 方法
    for (const auto* method : decl->getMethods()) {
        indent();
        printFuncDecl(method);
        OS << "\n";
    }
    
    decreaseIndent();
    indent();
    OS << "}";
}

void ASTPrinter::printImplDecl(const ImplDecl* decl) {
    if (!decl) return;
    
    OS << "impl";
    
    if (decl->isGeneric()) {
        printGenericParams(decl->getGenericParams());
    }
    
    OS << " ";
    
    if (decl->isTraitImpl()) {
        OS << decl->getTraitName() << " for ";
    }
    
    printTypeNode(decl->getTargetType());
    
    OS << " {\n";
    increaseIndent();
    
    // 关联类型实现
    for (const auto* assocType : decl->getAssociatedTypes()) {
        indent();
        printTypeAliasDecl(assocType);
        OS << "\n";
    }
    
    // 方法
    for (const auto* method : decl->getMethods()) {
        indent();
        printFuncDecl(method);
        OS << "\n";
    }
    
    decreaseIndent();
    indent();
    OS << "}";
}


// ============================================================================
// 语句打印
// ============================================================================

void ASTPrinter::printStmt(const Stmt* stmt) {
    if (!stmt) return;
    
    switch (stmt->getKind()) {
        case ASTNode::Kind::ExprStmt:
            printExprStmt(static_cast<const ExprStmt*>(stmt));
            break;
        case ASTNode::Kind::BlockStmt:
            printBlockStmt(static_cast<const BlockStmt*>(stmt));
            break;
        case ASTNode::Kind::ReturnStmt:
            printReturnStmt(static_cast<const ReturnStmt*>(stmt));
            break;
        case ASTNode::Kind::IfStmt:
            printIfStmt(static_cast<const IfStmt*>(stmt));
            break;
        case ASTNode::Kind::WhileStmt:
            printWhileStmt(static_cast<const WhileStmt*>(stmt));
            break;
        case ASTNode::Kind::LoopStmt:
            printLoopStmt(static_cast<const LoopStmt*>(stmt));
            break;
        case ASTNode::Kind::ForStmt:
            printForStmt(static_cast<const ForStmt*>(stmt));
            break;
        case ASTNode::Kind::MatchStmt:
            printMatchStmt(static_cast<const MatchStmt*>(stmt));
            break;
        case ASTNode::Kind::BreakStmt:
            printBreakStmt(static_cast<const BreakStmt*>(stmt));
            break;
        case ASTNode::Kind::ContinueStmt:
            printContinueStmt(static_cast<const ContinueStmt*>(stmt));
            break;
        case ASTNode::Kind::DeferStmt:
            printDeferStmt(static_cast<const DeferStmt*>(stmt));
            break;
        default:
            break;
    }
}

void ASTPrinter::printExprStmt(const ExprStmt* stmt) {
    if (!stmt) return;
    printExpr(stmt->getExpr());
}

void ASTPrinter::printBlockStmt(const BlockStmt* stmt) {
    if (!stmt) return;
    
    OS << "{\n";
    increaseIndent();
    
    for (const auto* s : stmt->getStatements()) {
        indent();
        printStmt(s);
        OS << "\n";
    }
    
    decreaseIndent();
    indent();
    OS << "}";
}

void ASTPrinter::printReturnStmt(const ReturnStmt* stmt) {
    if (!stmt) return;
    
    OS << "return";
    if (stmt->hasValue()) {
        OS << " ";
        printExpr(stmt->getValue());
    }
}

void ASTPrinter::printIfStmt(const IfStmt* stmt) {
    if (!stmt) return;
    
    const auto& branches = stmt->getBranches();
    for (size_t i = 0; i < branches.size(); ++i) {
        const auto& branch = branches[i];
        
        if (i == 0) {
            OS << "if ";
        } else if (branch.Condition) {
            OS << " elif ";
        } else {
            OS << " else ";
        }
        
        if (branch.Condition) {
            printExpr(branch.Condition);
            OS << " ";
        }
        
        printBlockStmt(branch.Body);
    }
}

void ASTPrinter::printWhileStmt(const WhileStmt* stmt) {
    if (!stmt) return;
    if (stmt->hasLabel()) {
        OS << stmt->getLabel() << ": ";
    }
    OS << "while ";
    printExpr(stmt->getCondition());
    OS << " ";
    printBlockStmt(stmt->getBody());
}

void ASTPrinter::printLoopStmt(const LoopStmt* stmt) {
    if (!stmt) return;
    if (stmt->hasLabel()) {
        OS << stmt->getLabel() << ": ";
    }
    OS << "loop ";
    printBlockStmt(stmt->getBody());
}

void ASTPrinter::printForStmt(const ForStmt* stmt) {
    if (!stmt) return;
    if (stmt->hasLabel()) {
        OS << stmt->getLabel() << ": ";
    }
    OS << "for ";
    printPattern(stmt->getPattern());
    OS << " in ";
    printExpr(stmt->getIterable());
    OS << " ";
    printBlockStmt(stmt->getBody());
}

void ASTPrinter::printMatchStmt(const MatchStmt* stmt) {
    if (!stmt) return;
    
    OS << "match ";
    printExpr(stmt->getScrutinee());
    OS << " {\n";
    increaseIndent();
    
    for (const auto& arm : stmt->getArms()) {
        indent();
        printPattern(arm.Pat);
        
        if (arm.Guard) {
            OS << " if ";
            printExpr(arm.Guard);
        }
        
        OS << " => ";
        printStmt(arm.Body);
        OS << "\n";
    }
    
    decreaseIndent();
    indent();
    OS << "}";
}

void ASTPrinter::printBreakStmt(const BreakStmt* stmt) {
    if (!stmt) return;
    
    OS << "break";
    if (stmt->hasLabel()) {
        OS << " '" << stmt->getLabel();
    }
}

void ASTPrinter::printContinueStmt(const ContinueStmt* stmt) {
    if (!stmt) return;
    
    OS << "continue";
    if (stmt->hasLabel()) {
        OS << " '" << stmt->getLabel();
    }
}

void ASTPrinter::printDeferStmt(const DeferStmt* stmt) {
    if (!stmt) return;
    
    OS << "defer ";
    printStmt(stmt->getBody());
}

// ============================================================================
// 表达式打印
// ============================================================================

void ASTPrinter::printExpr(const Expr* expr) {
    if (!expr) return;
    
    switch (expr->getKind()) {
        case ASTNode::Kind::IntegerLiteralExpr:
            printIntegerLiteralExpr(static_cast<const IntegerLiteralExpr*>(expr));
            break;
        case ASTNode::Kind::FloatLiteralExpr:
            printFloatLiteralExpr(static_cast<const FloatLiteralExpr*>(expr));
            break;
        case ASTNode::Kind::BoolLiteralExpr:
            printBoolLiteralExpr(static_cast<const BoolLiteralExpr*>(expr));
            break;
        case ASTNode::Kind::CharLiteralExpr:
            printCharLiteralExpr(static_cast<const CharLiteralExpr*>(expr));
            break;
        case ASTNode::Kind::StringLiteralExpr:
            printStringLiteralExpr(static_cast<const StringLiteralExpr*>(expr));
            break;
        case ASTNode::Kind::NoneLiteralExpr:
            printNoneLiteralExpr(static_cast<const NoneLiteralExpr*>(expr));
            break;
        case ASTNode::Kind::IdentifierExpr:
            printIdentifierExpr(static_cast<const IdentifierExpr*>(expr));
            break;
        case ASTNode::Kind::MemberExpr:
            printMemberExpr(static_cast<const MemberExpr*>(expr));
            break;
        case ASTNode::Kind::BinaryExpr:
            printBinaryExpr(static_cast<const BinaryExpr*>(expr));
            break;
        case ASTNode::Kind::UnaryExpr:
            printUnaryExpr(static_cast<const UnaryExpr*>(expr));
            break;
        case ASTNode::Kind::AssignExpr:
            printAssignExpr(static_cast<const AssignExpr*>(expr));
            break;
        case ASTNode::Kind::CallExpr:
            printCallExpr(static_cast<const CallExpr*>(expr));
            break;
        case ASTNode::Kind::IndexExpr:
            printIndexExpr(static_cast<const IndexExpr*>(expr));
            break;
        case ASTNode::Kind::SliceExpr:
            printSliceExpr(static_cast<const SliceExpr*>(expr));
            break;
        case ASTNode::Kind::BuiltinCallExpr:
            printBuiltinCallExpr(static_cast<const BuiltinCallExpr*>(expr));
            break;
        case ASTNode::Kind::IfExpr:
            printIfExpr(static_cast<const IfExpr*>(expr));
            break;
        case ASTNode::Kind::MatchExpr:
            printMatchExpr(static_cast<const MatchExpr*>(expr));
            break;
        case ASTNode::Kind::ClosureExpr:
            printClosureExpr(static_cast<const ClosureExpr*>(expr));
            break;
        case ASTNode::Kind::ArrayExpr:
            printArrayExpr(static_cast<const ArrayExpr*>(expr));
            break;
        case ASTNode::Kind::TupleExpr:
            printTupleExpr(static_cast<const TupleExpr*>(expr));
            break;
        case ASTNode::Kind::StructExpr:
            printStructExpr(static_cast<const StructExpr*>(expr));
            break;
        case ASTNode::Kind::RangeExpr:
            printRangeExpr(static_cast<const RangeExpr*>(expr));
            break;
        case ASTNode::Kind::AwaitExpr:
            printAwaitExpr(static_cast<const AwaitExpr*>(expr));
            break;
        case ASTNode::Kind::ErrorPropagateExpr:
            printErrorPropagateExpr(static_cast<const ErrorPropagateExpr*>(expr));
            break;
        case ASTNode::Kind::ErrorHandleExpr:
            printErrorHandleExpr(static_cast<const ErrorHandleExpr*>(expr));
            break;
        case ASTNode::Kind::CastExpr:
            printCastExpr(static_cast<const CastExpr*>(expr));
            break;
        default:
            break;
    }
}

void ASTPrinter::printIntegerLiteralExpr(const IntegerLiteralExpr* expr) {
    if (!expr) return;
    
    OS << expr->getValue();
    
    // 类型后缀
    if (expr->hasTypeSuffix()) {
        if (expr->isPointerSizedSuffix() || expr->getBitWidth() == 0) {
            OS << (expr->isSigned() ? "isize" : "usize");
        } else if (expr->isSigned()) {
            OS << "i" << expr->getBitWidth();
        } else {
            OS << "u" << expr->getBitWidth();
        }
    }
}

void ASTPrinter::printFloatLiteralExpr(const FloatLiteralExpr* expr) {
    if (!expr) return;
    
    OS << std::setprecision(17) << expr->getValue();
    
    // 类型后缀
    if (expr->hasTypeSuffix()) {
        OS << "f" << expr->getBitWidth();
    }
}

void ASTPrinter::printBoolLiteralExpr(const BoolLiteralExpr* expr) {
    if (!expr) return;
    OS << (expr->getValue() ? "true" : "false");
}

void ASTPrinter::printCharLiteralExpr(const CharLiteralExpr* expr) {
    if (!expr) return;
    OS << "'" << escapeChar(expr->getCodepoint()) << "'";
}

void ASTPrinter::printStringLiteralExpr(const StringLiteralExpr* expr) {
    if (!expr) return;
    
    switch (expr->getStringKind()) {
        case StringLiteralExpr::StringKind::Normal:
            OS << "\"" << escapeString(expr->getValue()) << "\"";
            break;
        case StringLiteralExpr::StringKind::Raw:
            OS << "r\"" << expr->getValue() << "\"";
            break;
        case StringLiteralExpr::StringKind::Multiline:
            OS << "\"\"\"" << expr->getValue() << "\"\"\"";
            break;
    }
}

void ASTPrinter::printNoneLiteralExpr(const NoneLiteralExpr* expr) {
    if (!expr) return;
    OS << "None";
}

void ASTPrinter::printIdentifierExpr(const IdentifierExpr* expr) {
    if (!expr) return;
    OS << expr->getName();
}

void ASTPrinter::printMemberExpr(const MemberExpr* expr) {
    if (!expr) return;
    
    printExpr(expr->getBase());
    OS << "." << expr->getMember();
}

void ASTPrinter::printBinaryExpr(const BinaryExpr* expr) {
    if (!expr) return;
    
    OS << "(";
    printExpr(expr->getLHS());
    OS << " " << BinaryExpr::getOpSpelling(expr->getOp()) << " ";
    printExpr(expr->getRHS());
    OS << ")";
}

void ASTPrinter::printUnaryExpr(const UnaryExpr* expr) {
    if (!expr) return;
    
    OS << UnaryExpr::getOpSpelling(expr->getOp());
    printExpr(expr->getOperand());
}

void ASTPrinter::printAssignExpr(const AssignExpr* expr) {
    if (!expr) return;
    
    printExpr(expr->getTarget());
    OS << " " << AssignExpr::getOpSpelling(expr->getOp()) << " ";
    printExpr(expr->getValue());
}

void ASTPrinter::printCallExpr(const CallExpr* expr) {
    if (!expr) return;
    
    printExpr(expr->getCallee());
    if (expr->hasTypeArgs()) {
        OS << "<";
        const auto& typeArgs = expr->getTypeArgs();
        for (size_t i = 0; i < typeArgs.size(); ++i) {
            if (i > 0) OS << ", ";
            printTypeNode(typeArgs[i]);
        }
        OS << ">";
    }
    OS << "(";
    
    const auto& args = expr->getArgs();
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) OS << ", ";
        if (args[i].IsSpread) {
            OS << "...";
        }
        printExpr(args[i].Value);
    }
    
    OS << ")";
}

void ASTPrinter::printIndexExpr(const IndexExpr* expr) {
    if (!expr) return;
    
    printExpr(expr->getBase());
    OS << "[";
    printExpr(expr->getIndex());
    OS << "]";
}

void ASTPrinter::printSliceExpr(const SliceExpr* expr) {
    if (!expr) return;
    
    printExpr(expr->getBase());
    OS << "[";
    
    if (expr->hasStart()) {
        printExpr(expr->getStart());
    }
    
    OS << (expr->isInclusive() ? "..=" : "..");
    
    if (expr->hasEnd()) {
        printExpr(expr->getEnd());
    }
    
    OS << "]";
}

void ASTPrinter::printBuiltinCallExpr(const BuiltinCallExpr* expr) {
    if (!expr) return;
    
    OS << "@" << BuiltinCallExpr::getBuiltinName(expr->getBuiltinKind()) << "(";
    
    const auto& args = expr->getArgs();
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) OS << ", ";
        
        const auto& arg = args[i];
        if (arg.isExpr()) {
            printExpr(arg.getExpr());
        } else if (arg.isType()) {
            printTypeNode(arg.getType());
        }
    }
    
    OS << ")";
}

void ASTPrinter::printIfExpr(const IfExpr* expr) {
    if (!expr) return;
    
    const auto& branches = expr->getBranches();
    for (size_t i = 0; i < branches.size(); ++i) {
        const auto& branch = branches[i];
        
        if (i == 0) {
            OS << "if ";
        } else if (branch.Condition) {
            OS << " elif ";
        } else {
            OS << " else ";
        }
        
        if (branch.Condition) {
            printExpr(branch.Condition);
            OS << " { ";
        } else {
            OS << "{ ";
        }
        
        printExpr(branch.Body);
        OS << " }";
    }
}

void ASTPrinter::printMatchExpr(const MatchExpr* expr) {
    if (!expr) return;
    
    OS << "match ";
    printExpr(expr->getScrutinee());
    OS << " {\n";
    increaseIndent();
    
    for (const auto& arm : expr->getArms()) {
        indent();
        printPattern(arm.Pat);
        
        if (arm.Guard) {
            OS << " if ";
            printExpr(arm.Guard);
        }
        
        OS << " => ";
        printExpr(arm.Body);
        OS << ",\n";
    }
    
    decreaseIndent();
    indent();
    OS << "}";
}

void ASTPrinter::printClosureExpr(const ClosureExpr* expr) {
    if (!expr) return;
    
    OS << "|";
    const auto& params = expr->getParams();
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) OS << ", ";
        printParamDecl(params[i]);
    }
    OS << "|";
    
    if (expr->getReturnType()) {
        OS << " -> ";
        printTypeNode(expr->getReturnType());
    }
    
    OS << " ";
    printExpr(expr->getBody());
}

void ASTPrinter::printArrayExpr(const ArrayExpr* expr) {
    if (!expr) return;
    
    OS << "[";
    
    if (expr->isRepeat()) {
        const auto& elements = expr->getElements();
        if (!elements.empty()) {
            printExpr(elements[0]);
        }
        OS << "; ";
        printExpr(expr->getRepeatCount());
    } else {
        const auto& elements = expr->getElements();
        for (size_t i = 0; i < elements.size(); ++i) {
            if (i > 0) OS << ", ";
            printExpr(elements[i]);
        }
    }
    
    OS << "]";
}

void ASTPrinter::printTupleExpr(const TupleExpr* expr) {
    if (!expr) return;
    
    OS << "(";
    const auto& elements = expr->getElements();
    for (size_t i = 0; i < elements.size(); ++i) {
        if (i > 0) OS << ", ";
        printExpr(elements[i]);
    }
    // 单元素元组需要尾随逗号
    if (elements.size() == 1) {
        OS << ",";
    }
    OS << ")";
}

void ASTPrinter::printStructExpr(const StructExpr* expr) {
    if (!expr) return;
    
    OS << expr->getTypeName() << " { ";
    
    const auto& fields = expr->getFields();
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) OS << ", ";
        OS << fields[i].Name << ": ";
        printExpr(fields[i].Value);
    }
    
    if (expr->hasBase()) {
        if (!fields.empty()) OS << ", ";
        OS << "..";
        printExpr(expr->getBase());
    }
    
    OS << " }";
}

void ASTPrinter::printRangeExpr(const RangeExpr* expr) {
    if (!expr) return;
    
    if (expr->hasStart()) {
        printExpr(expr->getStart());
    }
    
    OS << (expr->isInclusive() ? "..=" : "..");
    
    if (expr->hasEnd()) {
        printExpr(expr->getEnd());
    }
}

void ASTPrinter::printAwaitExpr(const AwaitExpr* expr) {
    if (!expr) return;

    OS << "await ";
    printExpr(expr->getInner());
}

void ASTPrinter::printErrorPropagateExpr(const ErrorPropagateExpr* expr) {
    if (!expr) return;
    
    printExpr(expr->getInner());
    OS << "!";
}

void ASTPrinter::printErrorHandleExpr(const ErrorHandleExpr* expr) {
    if (!expr) return;
    
    printExpr(expr->getInner());
    OS << "! -> " << expr->getErrorVar() << " ";
    printBlockStmt(expr->getHandler());
}

void ASTPrinter::printCastExpr(const CastExpr* expr) {
    if (!expr) return;
    
    printExpr(expr->getExpr());
    OS << " as ";
    printTypeNode(expr->getTargetType());
}


// ============================================================================
// 类型打印
// ============================================================================

void ASTPrinter::printTypeNode(const TypeNode* type) {
    if (!type) return;
    
    switch (type->getKind()) {
        case ASTNode::Kind::BuiltinTypeNode:
            printBuiltinTypeNode(static_cast<const BuiltinTypeNode*>(type));
            break;
        case ASTNode::Kind::IdentifierTypeNode:
            printIdentifierTypeNode(static_cast<const IdentifierTypeNode*>(type));
            break;
        case ASTNode::Kind::ArrayTypeNode:
            printArrayTypeNode(static_cast<const ArrayTypeNode*>(type));
            break;
        case ASTNode::Kind::SliceTypeNode:
            printSliceTypeNode(static_cast<const SliceTypeNode*>(type));
            break;
        case ASTNode::Kind::TupleTypeNode:
            printTupleTypeNode(static_cast<const TupleTypeNode*>(type));
            break;
        case ASTNode::Kind::OptionalTypeNode:
            printOptionalTypeNode(static_cast<const OptionalTypeNode*>(type));
            break;
        case ASTNode::Kind::ReferenceTypeNode:
            printReferenceTypeNode(static_cast<const ReferenceTypeNode*>(type));
            break;
        case ASTNode::Kind::PointerTypeNode:
            printPointerTypeNode(static_cast<const PointerTypeNode*>(type));
            break;
        case ASTNode::Kind::FunctionTypeNode:
            printFunctionTypeNode(static_cast<const FunctionTypeNode*>(type));
            break;
        case ASTNode::Kind::ErrorTypeNode:
            printErrorTypeNode(static_cast<const ErrorTypeNode*>(type));
            break;
        case ASTNode::Kind::GenericTypeNode:
            printGenericTypeNode(static_cast<const GenericTypeNode*>(type));
            break;
        default:
            break;
    }
}

void ASTPrinter::printBuiltinTypeNode(const BuiltinTypeNode* type) {
    if (!type) return;
    OS << BuiltinTypeNode::getBuiltinKindName(type->getBuiltinKind());
}

void ASTPrinter::printIdentifierTypeNode(const IdentifierTypeNode* type) {
    if (!type) return;
    OS << type->getName();
}

void ASTPrinter::printArrayTypeNode(const ArrayTypeNode* type) {
    if (!type) return;
    
    OS << "[";
    printTypeNode(type->getElementType());
    OS << "; ";
    printExpr(type->getSize());
    OS << "]";
}

void ASTPrinter::printSliceTypeNode(const SliceTypeNode* type) {
    if (!type) return;
    
    OS << "&";
    if (type->isMutable()) {
        OS << "mut ";
    }
    OS << "[";
    printTypeNode(type->getElementType());
    OS << "]";
}

void ASTPrinter::printTupleTypeNode(const TupleTypeNode* type) {
    if (!type) return;
    
    OS << "(";
    const auto& elements = type->getElements();
    for (size_t i = 0; i < elements.size(); ++i) {
        if (i > 0) OS << ", ";
        printTypeNode(elements[i]);
    }
    OS << ")";
}

void ASTPrinter::printOptionalTypeNode(const OptionalTypeNode* type) {
    if (!type) return;
    
    OS << "?";
    printTypeNode(type->getInnerType());
}

void ASTPrinter::printReferenceTypeNode(const ReferenceTypeNode* type) {
    if (!type) return;
    
    OS << "&";
    if (type->isMutable()) {
        OS << "mut ";
    }
    printTypeNode(type->getPointeeType());
}

void ASTPrinter::printPointerTypeNode(const PointerTypeNode* type) {
    if (!type) return;
    
    OS << "*";
    if (type->isMutable()) {
        OS << "mut ";
    }
    printTypeNode(type->getPointeeType());
}

void ASTPrinter::printFunctionTypeNode(const FunctionTypeNode* type) {
    if (!type) return;
    
    OS << "func(";
    const auto& params = type->getParamTypes();
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) OS << ", ";
        printTypeNode(params[i]);
    }
    OS << ")";
    
    if (type->getReturnType()) {
        OS << " -> ";
        if (type->canError()) {
            OS << "!";
        }
        printTypeNode(type->getReturnType());
    }
}

void ASTPrinter::printErrorTypeNode(const ErrorTypeNode* type) {
    if (!type) return;
    
    OS << "!";
    printTypeNode(type->getSuccessType());
}

void ASTPrinter::printGenericTypeNode(const GenericTypeNode* type) {
    if (!type) return;
    
    OS << type->getBaseName() << "<";
    const auto& typeArgs = type->getTypeArgs();
    for (size_t i = 0; i < typeArgs.size(); ++i) {
        if (i > 0) OS << ", ";
        printTypeNode(typeArgs[i]);
    }
    OS << ">";
}

// ============================================================================
// 模式打印
// ============================================================================

void ASTPrinter::printPattern(const Pattern* pattern) {
    if (!pattern) return;
    
    switch (pattern->getKind()) {
        case ASTNode::Kind::WildcardPattern:
            printWildcardPattern(static_cast<const WildcardPattern*>(pattern));
            break;
        case ASTNode::Kind::IdentifierPattern:
            printIdentifierPattern(static_cast<const IdentifierPattern*>(pattern));
            break;
        case ASTNode::Kind::LiteralPattern:
            printLiteralPattern(static_cast<const LiteralPattern*>(pattern));
            break;
        case ASTNode::Kind::TuplePattern:
            printTuplePattern(static_cast<const TuplePattern*>(pattern));
            break;
        case ASTNode::Kind::StructPattern:
            printStructPattern(static_cast<const StructPattern*>(pattern));
            break;
        case ASTNode::Kind::EnumPattern:
            printEnumPattern(static_cast<const EnumPattern*>(pattern));
            break;
        case ASTNode::Kind::RangePattern:
            printRangePattern(static_cast<const RangePattern*>(pattern));
            break;
        case ASTNode::Kind::OrPattern:
            printOrPattern(static_cast<const OrPattern*>(pattern));
            break;
        case ASTNode::Kind::BindPattern:
            printBindPattern(static_cast<const BindPattern*>(pattern));
            break;
        default:
            break;
    }
}

void ASTPrinter::printWildcardPattern(const WildcardPattern* pattern) {
    if (!pattern) return;
    OS << "_";
}

void ASTPrinter::printIdentifierPattern(const IdentifierPattern* pattern) {
    if (!pattern) return;
    
    if (pattern->isMutable()) {
        OS << "mut ";
    }
    OS << pattern->getName();
    
    if (pattern->hasType()) {
        OS << ": ";
        printTypeNode(pattern->getType());
    }
}

void ASTPrinter::printLiteralPattern(const LiteralPattern* pattern) {
    if (!pattern) return;
    printExpr(pattern->getLiteral());
}

void ASTPrinter::printTuplePattern(const TuplePattern* pattern) {
    if (!pattern) return;
    
    OS << "(";
    const auto& elements = pattern->getElements();
    for (size_t i = 0; i < elements.size(); ++i) {
        if (i > 0) OS << ", ";
        printPattern(elements[i]);
    }
    OS << ")";
}

void ASTPrinter::printStructPattern(const StructPattern* pattern) {
    if (!pattern) return;
    
    OS << pattern->getTypeName() << " { ";
    
    const auto& fields = pattern->getFields();
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) OS << ", ";
        OS << fields[i].Name;
        if (fields[i].Pat) {
            OS << ": ";
            printPattern(fields[i].Pat);
        }
    }
    
    if (pattern->hasRest()) {
        if (!fields.empty()) OS << ", ";
        OS << "..";
    }
    
    OS << " }";
}

void ASTPrinter::printEnumPattern(const EnumPattern* pattern) {
    if (!pattern) return;
    
    if (pattern->hasEnumName()) {
        OS << pattern->getEnumName() << ".";
    }
    OS << pattern->getVariantName();
    
    if (pattern->hasPayload()) {
        OS << "(";
        const auto& payload = pattern->getPayload();
        for (size_t i = 0; i < payload.size(); ++i) {
            if (i > 0) OS << ", ";
            printPattern(payload[i]);
        }
        OS << ")";
    }
}

void ASTPrinter::printRangePattern(const RangePattern* pattern) {
    if (!pattern) return;
    
    printExpr(pattern->getStart());
    OS << (pattern->isInclusive() ? "..=" : "..");
    printExpr(pattern->getEnd());
}

void ASTPrinter::printOrPattern(const OrPattern* pattern) {
    if (!pattern) return;

    const auto& patterns = pattern->getPatterns();
    for (size_t i = 0; i < patterns.size(); ++i) {
        if (i > 0) OS << " | ";
        printPattern(patterns[i]);
    }
}

void ASTPrinter::printBindPattern(const BindPattern* pattern) {
    if (!pattern) return;

    if (pattern->isMutable()) {
        OS << "mut ";
    }
    OS << pattern->getName();

    if (pattern->hasType()) {
        OS << ": ";
        printTypeNode(pattern->getType());
    }

    OS << " @ ";
    printPattern(pattern->getInner());
}

// ============================================================================
// 辅助方法
// ============================================================================

void ASTPrinter::printGenericParams(const std::vector<GenericParam>& params) {
    if (params.empty()) return;
    
    OS << "<";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) OS << ", ";
        OS << params[i].Name;
        
        if (!params[i].Bounds.empty()) {
            OS << ": ";
            for (size_t j = 0; j < params[i].Bounds.size(); ++j) {
                if (j > 0) OS << " + ";
                OS << params[i].Bounds[j];
            }
        }
    }
    OS << ">";
}

void ASTPrinter::printVisibility(Visibility vis) {
    switch (vis) {
        case Visibility::Public:
            OS << "pub ";
            break;
        case Visibility::Internal:
            OS << "internal ";
            break;
        case Visibility::Private:
            // 私有是默认的，不输出
            break;
    }
}

std::string ASTPrinter::escapeString(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    
    for (char c : str) {
        switch (c) {
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            case '\\': result += "\\\\"; break;
            case '"':  result += "\\\""; break;
            case '\0': result += "\\0"; break;
            default:
                if (static_cast<unsigned char>(c) < 32) {
                    // 控制字符使用 \xNN 格式
                    std::ostringstream oss;
                    oss << "\\x" << std::hex << std::setw(2) 
                        << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(c));
                    result += oss.str();
                } else {
                    result += c;
                }
                break;
        }
    }
    
    return result;
}

std::string ASTPrinter::escapeChar(uint32_t codepoint) {
    switch (codepoint) {
        case '\n': return "\\n";
        case '\r': return "\\r";
        case '\t': return "\\t";
        case '\\': return "\\\\";
        case '\'': return "\\'";
        case '\0': return "\\0";
        default:
            if (codepoint < 32) {
                // 控制字符使用 \xNN 格式
                std::ostringstream oss;
                oss << "\\x" << std::hex << std::setw(2) 
                    << std::setfill('0') << codepoint;
                return oss.str();
            } else if (codepoint < 128) {
                // ASCII 可打印字符
                return std::string(1, static_cast<char>(codepoint));
            } else if (codepoint <= 0xFFFF) {
                // BMP 字符使用 \u{XXXX} 格式
                std::ostringstream oss;
                oss << "\\u{" << std::hex << codepoint << "}";
                return oss.str();
            } else {
                // 补充平面字符使用 \u{XXXXXX} 格式
                std::ostringstream oss;
                oss << "\\u{" << std::hex << codepoint << "}";
                return oss.str();
            }
    }
}

} // namespace yuan
