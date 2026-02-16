/// \file ASTDumper.cpp
/// \brief 树形 AST 输出器实现。

#include "yuan/AST/ASTDumper.h"
#include "yuan/AST/AST.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Pattern.h"
#include "yuan/AST/Stmt.h"
#include "yuan/AST/Type.h"
#include <ostream>
#include <sstream>

namespace yuan {

namespace {

std::string quote(const std::string& text) {
    std::string out = "\"";
    for (char c : text) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

const char* boolText(bool value) {
    return value ? "true" : "false";
}

} // namespace

ASTDumper::ASTDumper(std::ostream& os) : OS(os) {}

void ASTDumper::dump(const ASTNode* node) {
    PrefixStack.clear();
    dumpNode(node, true, "");
}

void ASTDumper::dumpNode(const ASTNode* node, bool isLast, const std::string& edgeLabel) {
    if (!node) {
        return;
    }

    if (!PrefixStack.empty()) {
        printPrefix(isLast, edgeLabel);
    }
    OS << formatNodeLabel(node) << "\n";

    std::vector<Child> children;
    collectChildren(node, children);
    if (children.empty()) {
        return;
    }

    PrefixStack.push_back(!isLast);
    for (size_t i = 0; i < children.size(); ++i) {
        const bool childIsLast = (i + 1 == children.size());
        dumpNode(children[i].Node, childIsLast, children[i].Label);
    }
    PrefixStack.pop_back();
}

void ASTDumper::printPrefix(bool isLast, const std::string& edgeLabel) {
    for (size_t i = 0; i + 1 < PrefixStack.size(); ++i) {
        OS << (PrefixStack[i] ? "│   " : "    ");
    }
    OS << (isLast ? "└── " : "├── ");
    if (!edgeLabel.empty()) {
        OS << edgeLabel << ": ";
    }
}

std::string ASTDumper::formatNodeLabel(const ASTNode* node) const {
    if (!node) {
        return "<null>";
    }

    std::ostringstream oss;
    oss << ASTNode::getKindName(node->getKind());

    switch (node->getKind()) {
        case ASTNode::Kind::VarDecl: {
            auto* n = static_cast<const VarDecl*>(node);
            oss << "(name=" << quote(n->getName())
                << ", mutable=" << boolText(n->isMutable())
                << ", vis=" << getVisibilityName(n->getVisibility()) << ")";
            break;
        }
        case ASTNode::Kind::ConstDecl: {
            auto* n = static_cast<const ConstDecl*>(node);
            oss << "(name=" << quote(n->getName())
                << ", vis=" << getVisibilityName(n->getVisibility()) << ")";
            break;
        }
        case ASTNode::Kind::ParamDecl: {
            auto* n = static_cast<const ParamDecl*>(node);
            oss << "(name=" << quote(n->getName())
                << ", kind=";
            switch (n->getParamKind()) {
                case ParamDecl::ParamKind::Normal: oss << "normal"; break;
                case ParamDecl::ParamKind::Self: oss << "self"; break;
                case ParamDecl::ParamKind::RefSelf: oss << "&self"; break;
                case ParamDecl::ParamKind::MutRefSelf: oss << "&mut self"; break;
                case ParamDecl::ParamKind::Variadic: oss << "variadic"; break;
            }
            oss << ", mutable=" << boolText(n->isMutable()) << ")";
            break;
        }
        case ASTNode::Kind::FuncDecl: {
            auto* n = static_cast<const FuncDecl*>(node);
            oss << "(name=" << quote(n->getName())
                << ", async=" << boolText(n->isAsync())
                << ", canError=" << boolText(n->canError())
                << ", generic=" << boolText(n->isGeneric())
                << ", vis=" << getVisibilityName(n->getVisibility()) << ")";
            break;
        }
        case ASTNode::Kind::FieldDecl: {
            auto* n = static_cast<const FieldDecl*>(node);
            oss << "(name=" << quote(n->getName())
                << ", vis=" << getVisibilityName(n->getVisibility()) << ")";
            break;
        }
        case ASTNode::Kind::StructDecl: {
            auto* n = static_cast<const StructDecl*>(node);
            oss << "(name=" << quote(n->getName())
                << ", generic=" << boolText(n->isGeneric())
                << ", vis=" << getVisibilityName(n->getVisibility()) << ")";
            break;
        }
        case ASTNode::Kind::EnumVariantDecl: {
            auto* n = static_cast<const EnumVariantDecl*>(node);
            oss << "(name=" << quote(n->getName()) << ", kind=";
            switch (n->getVariantKind()) {
                case EnumVariantDecl::VariantKind::Unit: oss << "unit"; break;
                case EnumVariantDecl::VariantKind::Tuple: oss << "tuple"; break;
                case EnumVariantDecl::VariantKind::Struct: oss << "struct"; break;
            }
            if (n->hasDiscriminant()) {
                oss << ", discr=" << n->getDiscriminant();
            }
            oss << ")";
            break;
        }
        case ASTNode::Kind::EnumDecl: {
            auto* n = static_cast<const EnumDecl*>(node);
            oss << "(name=" << quote(n->getName())
                << ", generic=" << boolText(n->isGeneric())
                << ", vis=" << getVisibilityName(n->getVisibility()) << ")";
            break;
        }
        case ASTNode::Kind::TypeAliasDecl: {
            auto* n = static_cast<const TypeAliasDecl*>(node);
            oss << "(name=" << quote(n->getName())
                << ", generic=" << boolText(n->isGeneric())
                << ", associated=" << boolText(n->isAssociatedType())
                << ", vis=" << getVisibilityName(n->getVisibility()) << ")";
            break;
        }
        case ASTNode::Kind::TraitDecl: {
            auto* n = static_cast<const TraitDecl*>(node);
            oss << "(name=" << quote(n->getName())
                << ", generic=" << boolText(n->isGeneric())
                << ", vis=" << getVisibilityName(n->getVisibility()) << ")";
            break;
        }
        case ASTNode::Kind::ImplDecl: {
            auto* n = static_cast<const ImplDecl*>(node);
            oss << "(traitImpl=" << boolText(n->isTraitImpl());
            if (n->isTraitImpl()) {
                oss << ", trait=" << quote(n->getTraitName());
            }
            oss << ", generic=" << boolText(n->isGeneric()) << ")";
            break;
        }
        case ASTNode::Kind::WhileStmt: {
            auto* n = static_cast<const WhileStmt*>(node);
            if (n->hasLabel()) {
                oss << "(label=" << quote(n->getLabel()) << ")";
            }
            break;
        }
        case ASTNode::Kind::LoopStmt: {
            auto* n = static_cast<const LoopStmt*>(node);
            if (n->hasLabel()) {
                oss << "(label=" << quote(n->getLabel()) << ")";
            }
            break;
        }
        case ASTNode::Kind::ForStmt: {
            auto* n = static_cast<const ForStmt*>(node);
            if (n->hasLabel()) {
                oss << "(label=" << quote(n->getLabel()) << ")";
            }
            break;
        }
        case ASTNode::Kind::BreakStmt: {
            auto* n = static_cast<const BreakStmt*>(node);
            if (n->hasLabel()) {
                oss << "(label=" << quote(n->getLabel()) << ")";
            }
            break;
        }
        case ASTNode::Kind::ContinueStmt: {
            auto* n = static_cast<const ContinueStmt*>(node);
            if (n->hasLabel()) {
                oss << "(label=" << quote(n->getLabel()) << ")";
            }
            break;
        }
        case ASTNode::Kind::IdentifierExpr: {
            auto* n = static_cast<const IdentifierExpr*>(node);
            oss << "(name=" << quote(n->getName()) << ")";
            break;
        }
        case ASTNode::Kind::MemberExpr: {
            auto* n = static_cast<const MemberExpr*>(node);
            oss << "(member=" << quote(n->getMember()) << ")";
            break;
        }
        case ASTNode::Kind::OptionalChainingExpr: {
            auto* n = static_cast<const OptionalChainingExpr*>(node);
            oss << "(member=" << quote(n->getMember()) << ")";
            break;
        }
        case ASTNode::Kind::BinaryExpr: {
            auto* n = static_cast<const BinaryExpr*>(node);
            oss << "(op=" << quote(BinaryExpr::getOpSpelling(n->getOp())) << ")";
            break;
        }
        case ASTNode::Kind::UnaryExpr: {
            auto* n = static_cast<const UnaryExpr*>(node);
            oss << "(op=" << quote(UnaryExpr::getOpSpelling(n->getOp())) << ")";
            break;
        }
        case ASTNode::Kind::AssignExpr: {
            auto* n = static_cast<const AssignExpr*>(node);
            oss << "(op=" << quote(AssignExpr::getOpSpelling(n->getOp())) << ")";
            break;
        }
        case ASTNode::Kind::CallExpr: {
            auto* n = static_cast<const CallExpr*>(node);
            oss << "(args=" << n->getArgCount() << ", typeArgs=" << n->getTypeArgCount() << ")";
            break;
        }
        case ASTNode::Kind::BuiltinCallExpr: {
            auto* n = static_cast<const BuiltinCallExpr*>(node);
            oss << "(name=" << quote(BuiltinCallExpr::getBuiltinName(n->getBuiltinKind()))
                << ", args=" << n->getArgCount() << ")";
            break;
        }
        case ASTNode::Kind::SliceExpr: {
            auto* n = static_cast<const SliceExpr*>(node);
            oss << "(inclusive=" << boolText(n->isInclusive()) << ")";
            break;
        }
        case ASTNode::Kind::StructExpr: {
            auto* n = static_cast<const StructExpr*>(node);
            oss << "(type=" << quote(n->getTypeName())
                << ", fields=" << n->getFields().size()
                << ", hasBase=" << boolText(n->hasBase()) << ")";
            break;
        }
        case ASTNode::Kind::RangeExpr: {
            auto* n = static_cast<const RangeExpr*>(node);
            oss << "(inclusive=" << boolText(n->isInclusive()) << ")";
            break;
        }
        case ASTNode::Kind::AwaitExpr: {
            break;
        }
        case ASTNode::Kind::ErrorHandleExpr: {
            auto* n = static_cast<const ErrorHandleExpr*>(node);
            oss << "(errorVar=" << quote(n->getErrorVar()) << ")";
            break;
        }
        case ASTNode::Kind::BuiltinTypeNode: {
            auto* n = static_cast<const BuiltinTypeNode*>(node);
            oss << "(name=" << quote(BuiltinTypeNode::getBuiltinKindName(n->getBuiltinKind())) << ")";
            break;
        }
        case ASTNode::Kind::IdentifierTypeNode: {
            auto* n = static_cast<const IdentifierTypeNode*>(node);
            oss << "(name=" << quote(n->getName()) << ")";
            break;
        }
        case ASTNode::Kind::SliceTypeNode: {
            auto* n = static_cast<const SliceTypeNode*>(node);
            oss << "(mutable=" << boolText(n->isMutable()) << ")";
            break;
        }
        case ASTNode::Kind::ReferenceTypeNode: {
            auto* n = static_cast<const ReferenceTypeNode*>(node);
            oss << "(mutable=" << boolText(n->isMutable()) << ")";
            break;
        }
        case ASTNode::Kind::PointerTypeNode: {
            auto* n = static_cast<const PointerTypeNode*>(node);
            oss << "(mutable=" << boolText(n->isMutable()) << ")";
            break;
        }
        case ASTNode::Kind::FunctionTypeNode: {
            auto* n = static_cast<const FunctionTypeNode*>(node);
            oss << "(params=" << n->getParamCount()
                << ", canError=" << boolText(n->canError()) << ")";
            break;
        }
        case ASTNode::Kind::GenericTypeNode: {
            auto* n = static_cast<const GenericTypeNode*>(node);
            oss << "(base=" << quote(n->getBaseName())
                << ", typeArgs=" << n->getTypeArgCount() << ")";
            break;
        }
        case ASTNode::Kind::StructPattern: {
            auto* n = static_cast<const StructPattern*>(node);
            oss << "(type=" << quote(n->getTypeName())
                << ", fields=" << n->getFieldCount()
                << ", hasRest=" << boolText(n->hasRest()) << ")";
            break;
        }
        case ASTNode::Kind::EnumPattern: {
            auto* n = static_cast<const EnumPattern*>(node);
            oss << "(variant=" << quote(n->getVariantName())
                << ", hasEnumName=" << boolText(n->hasEnumName())
                << ", payload=" << n->getPayloadCount() << ")";
            break;
        }
        case ASTNode::Kind::RangePattern: {
            auto* n = static_cast<const RangePattern*>(node);
            oss << "(inclusive=" << boolText(n->isInclusive()) << ")";
            break;
        }
        case ASTNode::Kind::IdentifierPattern: {
            auto* n = static_cast<const IdentifierPattern*>(node);
            oss << "(name=" << quote(n->getName())
                << ", mutable=" << boolText(n->isMutable()) << ")";
            break;
        }
        case ASTNode::Kind::BindPattern: {
            auto* n = static_cast<const BindPattern*>(node);
            oss << "(name=" << quote(n->getName())
                << ", mutable=" << boolText(n->isMutable()) << ")";
            break;
        }
        default:
            break;
    }

    return oss.str();
}

void ASTDumper::collectChildren(const ASTNode* node, std::vector<Child>& out) const {
    if (!node) {
        return;
    }

    switch (node->getKind()) {
        case ASTNode::Kind::VarDecl: {
            auto* n = static_cast<const VarDecl*>(node);
            addChild(out, "pattern", n->getPattern());
            addChild(out, "type", n->getType());
            addChild(out, "init", n->getInit());
            break;
        }
        case ASTNode::Kind::ConstDecl: {
            auto* n = static_cast<const ConstDecl*>(node);
            addChild(out, "type", n->getType());
            addChild(out, "init", n->getInit());
            break;
        }
        case ASTNode::Kind::ParamDecl: {
            auto* n = static_cast<const ParamDecl*>(node);
            addChild(out, "type", n->getType());
            break;
        }
        case ASTNode::Kind::FuncDecl: {
            auto* n = static_cast<const FuncDecl*>(node);
            for (size_t i = 0; i < n->getParams().size(); ++i) {
                addChild(out, "param[" + std::to_string(i) + "]", n->getParams()[i]);
            }
            addChild(out, "returnType", n->getReturnType());
            addChild(out, "body", n->getBody());
            break;
        }
        case ASTNode::Kind::FieldDecl: {
            auto* n = static_cast<const FieldDecl*>(node);
            addChild(out, "type", n->getType());
            addChild(out, "default", n->getDefaultValue());
            break;
        }
        case ASTNode::Kind::StructDecl: {
            auto* n = static_cast<const StructDecl*>(node);
            for (size_t i = 0; i < n->getFields().size(); ++i) {
                addChild(out, "field[" + std::to_string(i) + "]", n->getFields()[i]);
            }
            break;
        }
        case ASTNode::Kind::EnumVariantDecl: {
            auto* n = static_cast<const EnumVariantDecl*>(node);
            if (n->isTuple()) {
                for (size_t i = 0; i < n->getTupleTypes().size(); ++i) {
                    addChild(out, "type[" + std::to_string(i) + "]", n->getTupleTypes()[i]);
                }
            } else if (n->isStruct()) {
                for (size_t i = 0; i < n->getFields().size(); ++i) {
                    addChild(out, "field[" + std::to_string(i) + "]", n->getFields()[i]);
                }
            }
            break;
        }
        case ASTNode::Kind::EnumDecl: {
            auto* n = static_cast<const EnumDecl*>(node);
            for (size_t i = 0; i < n->getVariants().size(); ++i) {
                addChild(out, "variant[" + std::to_string(i) + "]", n->getVariants()[i]);
            }
            break;
        }
        case ASTNode::Kind::TypeAliasDecl: {
            auto* n = static_cast<const TypeAliasDecl*>(node);
            addChild(out, "aliasedType", n->getAliasedType());
            break;
        }
        case ASTNode::Kind::TraitDecl: {
            auto* n = static_cast<const TraitDecl*>(node);
            for (size_t i = 0; i < n->getAssociatedTypes().size(); ++i) {
                addChild(out, "assocType[" + std::to_string(i) + "]", n->getAssociatedTypes()[i]);
            }
            for (size_t i = 0; i < n->getMethods().size(); ++i) {
                addChild(out, "method[" + std::to_string(i) + "]", n->getMethods()[i]);
            }
            break;
        }
        case ASTNode::Kind::ImplDecl: {
            auto* n = static_cast<const ImplDecl*>(node);
            addChild(out, "targetType", n->getTargetType());
            for (size_t i = 0; i < n->getAssociatedTypes().size(); ++i) {
                addChild(out, "assocType[" + std::to_string(i) + "]", n->getAssociatedTypes()[i]);
            }
            for (size_t i = 0; i < n->getMethods().size(); ++i) {
                addChild(out, "method[" + std::to_string(i) + "]", n->getMethods()[i]);
            }
            break;
        }

        case ASTNode::Kind::DeclStmt: {
            auto* n = static_cast<const DeclStmt*>(node);
            addChild(out, "decl", n->getDecl());
            break;
        }
        case ASTNode::Kind::ExprStmt: {
            auto* n = static_cast<const ExprStmt*>(node);
            addChild(out, "expr", n->getExpr());
            break;
        }
        case ASTNode::Kind::BlockStmt: {
            auto* n = static_cast<const BlockStmt*>(node);
            for (size_t i = 0; i < n->getStatements().size(); ++i) {
                addChild(out, "stmt[" + std::to_string(i) + "]", n->getStatements()[i]);
            }
            break;
        }
        case ASTNode::Kind::ReturnStmt: {
            auto* n = static_cast<const ReturnStmt*>(node);
            addChild(out, "value", n->getValue());
            break;
        }
        case ASTNode::Kind::IfStmt: {
            auto* n = static_cast<const IfStmt*>(node);
            for (size_t i = 0; i < n->getBranches().size(); ++i) {
                const auto& b = n->getBranches()[i];
                if (b.Condition) {
                    addChild(out, "branch[" + std::to_string(i) + "].cond", b.Condition);
                }
                addChild(out, "branch[" + std::to_string(i) + "].body", b.Body);
            }
            break;
        }
        case ASTNode::Kind::WhileStmt: {
            auto* n = static_cast<const WhileStmt*>(node);
            addChild(out, "cond", n->getCondition());
            addChild(out, "body", n->getBody());
            break;
        }
        case ASTNode::Kind::LoopStmt: {
            auto* n = static_cast<const LoopStmt*>(node);
            addChild(out, "body", n->getBody());
            break;
        }
        case ASTNode::Kind::ForStmt: {
            auto* n = static_cast<const ForStmt*>(node);
            addChild(out, "pattern", n->getPattern());
            addChild(out, "iterable", n->getIterable());
            addChild(out, "body", n->getBody());
            break;
        }
        case ASTNode::Kind::MatchStmt: {
            auto* n = static_cast<const MatchStmt*>(node);
            addChild(out, "scrutinee", n->getScrutinee());
            for (size_t i = 0; i < n->getArms().size(); ++i) {
                const auto& arm = n->getArms()[i];
                addChild(out, "arm[" + std::to_string(i) + "].pattern", arm.Pat);
                addChild(out, "arm[" + std::to_string(i) + "].guard", arm.Guard);
                addChild(out, "arm[" + std::to_string(i) + "].body", arm.Body);
            }
            break;
        }
        case ASTNode::Kind::DeferStmt: {
            auto* n = static_cast<const DeferStmt*>(node);
            addChild(out, "body", n->getBody());
            break;
        }

        case ASTNode::Kind::MemberExpr: {
            auto* n = static_cast<const MemberExpr*>(node);
            addChild(out, "base", n->getBase());
            break;
        }
        case ASTNode::Kind::OptionalChainingExpr: {
            auto* n = static_cast<const OptionalChainingExpr*>(node);
            addChild(out, "base", n->getBase());
            break;
        }
        case ASTNode::Kind::BinaryExpr: {
            auto* n = static_cast<const BinaryExpr*>(node);
            addChild(out, "lhs", n->getLHS());
            addChild(out, "rhs", n->getRHS());
            break;
        }
        case ASTNode::Kind::UnaryExpr: {
            auto* n = static_cast<const UnaryExpr*>(node);
            addChild(out, "operand", n->getOperand());
            break;
        }
        case ASTNode::Kind::AssignExpr: {
            auto* n = static_cast<const AssignExpr*>(node);
            addChild(out, "target", n->getTarget());
            addChild(out, "value", n->getValue());
            break;
        }
        case ASTNode::Kind::CallExpr: {
            auto* n = static_cast<const CallExpr*>(node);
            addChild(out, "callee", n->getCallee());
            for (size_t i = 0; i < n->getTypeArgs().size(); ++i) {
                addChild(out, "typeArg[" + std::to_string(i) + "]", n->getTypeArgs()[i]);
            }
            for (size_t i = 0; i < n->getArgs().size(); ++i) {
                const auto& arg = n->getArgs()[i];
                std::string label = "arg[" + std::to_string(i) + "]";
                if (arg.IsSpread) {
                    label += "(spread)";
                }
                addChild(out, label, arg.Value);
            }
            break;
        }
        case ASTNode::Kind::IndexExpr: {
            auto* n = static_cast<const IndexExpr*>(node);
            addChild(out, "base", n->getBase());
            addChild(out, "index", n->getIndex());
            break;
        }
        case ASTNode::Kind::SliceExpr: {
            auto* n = static_cast<const SliceExpr*>(node);
            addChild(out, "base", n->getBase());
            addChild(out, "start", n->getStart());
            addChild(out, "end", n->getEnd());
            break;
        }
        case ASTNode::Kind::BuiltinCallExpr: {
            auto* n = static_cast<const BuiltinCallExpr*>(node);
            for (size_t i = 0; i < n->getArgs().size(); ++i) {
                const auto& arg = n->getArgs()[i];
                if (arg.isExpr()) {
                    addChild(out, "arg[" + std::to_string(i) + "]", arg.getExpr());
                } else if (arg.isType()) {
                    addChild(out, "argType[" + std::to_string(i) + "]", arg.getType());
                }
            }
            break;
        }
        case ASTNode::Kind::IfExpr: {
            auto* n = static_cast<const IfExpr*>(node);
            for (size_t i = 0; i < n->getBranches().size(); ++i) {
                const auto& b = n->getBranches()[i];
                if (b.Condition) {
                    addChild(out, "branch[" + std::to_string(i) + "].cond", b.Condition);
                }
                addChild(out, "branch[" + std::to_string(i) + "].body", b.Body);
            }
            break;
        }
        case ASTNode::Kind::MatchExpr: {
            auto* n = static_cast<const MatchExpr*>(node);
            addChild(out, "scrutinee", n->getScrutinee());
            for (size_t i = 0; i < n->getArms().size(); ++i) {
                const auto& arm = n->getArms()[i];
                addChild(out, "arm[" + std::to_string(i) + "].pattern", arm.Pat);
                addChild(out, "arm[" + std::to_string(i) + "].guard", arm.Guard);
                addChild(out, "arm[" + std::to_string(i) + "].body", arm.Body);
            }
            break;
        }
        case ASTNode::Kind::ClosureExpr: {
            auto* n = static_cast<const ClosureExpr*>(node);
            for (size_t i = 0; i < n->getParams().size(); ++i) {
                addChild(out, "param[" + std::to_string(i) + "]", n->getParams()[i]);
            }
            addChild(out, "returnType", n->getReturnType());
            addChild(out, "body", n->getBody());
            break;
        }
        case ASTNode::Kind::ArrayExpr: {
            auto* n = static_cast<const ArrayExpr*>(node);
            if (n->isRepeat()) {
                if (!n->getElements().empty()) {
                    addChild(out, "element", n->getElements()[0]);
                }
                addChild(out, "count", n->getRepeatCount());
            } else {
                for (size_t i = 0; i < n->getElements().size(); ++i) {
                    addChild(out, "element[" + std::to_string(i) + "]", n->getElements()[i]);
                }
            }
            break;
        }
        case ASTNode::Kind::TupleExpr: {
            auto* n = static_cast<const TupleExpr*>(node);
            for (size_t i = 0; i < n->getElements().size(); ++i) {
                addChild(out, "element[" + std::to_string(i) + "]", n->getElements()[i]);
            }
            break;
        }
        case ASTNode::Kind::StructExpr: {
            auto* n = static_cast<const StructExpr*>(node);
            for (size_t i = 0; i < n->getFields().size(); ++i) {
                addChild(out, "field[" + n->getFields()[i].Name + "]", n->getFields()[i].Value);
            }
            addChild(out, "base", n->getBase());
            break;
        }
        case ASTNode::Kind::RangeExpr: {
            auto* n = static_cast<const RangeExpr*>(node);
            addChild(out, "start", n->getStart());
            addChild(out, "end", n->getEnd());
            break;
        }
        case ASTNode::Kind::AwaitExpr: {
            auto* n = static_cast<const AwaitExpr*>(node);
            addChild(out, "inner", n->getInner());
            break;
        }
        case ASTNode::Kind::ErrorPropagateExpr: {
            auto* n = static_cast<const ErrorPropagateExpr*>(node);
            addChild(out, "inner", n->getInner());
            break;
        }
        case ASTNode::Kind::ErrorHandleExpr: {
            auto* n = static_cast<const ErrorHandleExpr*>(node);
            addChild(out, "inner", n->getInner());
            addChild(out, "handler", n->getHandler());
            break;
        }
        case ASTNode::Kind::CastExpr: {
            auto* n = static_cast<const CastExpr*>(node);
            addChild(out, "expr", n->getExpr());
            addChild(out, "targetType", n->getTargetType());
            break;
        }
        case ASTNode::Kind::LoopExpr: {
            auto* n = static_cast<const LoopExpr*>(node);
            addChild(out, "body", n->getBody());
            break;
        }
        case ASTNode::Kind::BlockExpr: {
            auto* n = static_cast<const BlockExpr*>(node);
            for (size_t i = 0; i < n->getStatements().size(); ++i) {
                addChild(out, "stmt[" + std::to_string(i) + "]", n->getStatements()[i]);
            }
            addChild(out, "result", n->getResultExpr());
            break;
        }

        case ASTNode::Kind::ArrayTypeNode: {
            auto* n = static_cast<const ArrayTypeNode*>(node);
            addChild(out, "element", n->getElementType());
            addChild(out, "size", n->getSize());
            break;
        }
        case ASTNode::Kind::SliceTypeNode: {
            auto* n = static_cast<const SliceTypeNode*>(node);
            addChild(out, "element", n->getElementType());
            break;
        }
        case ASTNode::Kind::TupleTypeNode: {
            auto* n = static_cast<const TupleTypeNode*>(node);
            for (size_t i = 0; i < n->getElements().size(); ++i) {
                addChild(out, "element[" + std::to_string(i) + "]", n->getElements()[i]);
            }
            break;
        }
        case ASTNode::Kind::OptionalTypeNode: {
            auto* n = static_cast<const OptionalTypeNode*>(node);
            addChild(out, "inner", n->getInnerType());
            break;
        }
        case ASTNode::Kind::ReferenceTypeNode: {
            auto* n = static_cast<const ReferenceTypeNode*>(node);
            addChild(out, "pointee", n->getPointeeType());
            break;
        }
        case ASTNode::Kind::PointerTypeNode: {
            auto* n = static_cast<const PointerTypeNode*>(node);
            addChild(out, "pointee", n->getPointeeType());
            break;
        }
        case ASTNode::Kind::FunctionTypeNode: {
            auto* n = static_cast<const FunctionTypeNode*>(node);
            for (size_t i = 0; i < n->getParamTypes().size(); ++i) {
                addChild(out, "paramType[" + std::to_string(i) + "]", n->getParamTypes()[i]);
            }
            addChild(out, "returnType", n->getReturnType());
            break;
        }
        case ASTNode::Kind::ErrorTypeNode: {
            auto* n = static_cast<const ErrorTypeNode*>(node);
            addChild(out, "successType", n->getSuccessType());
            break;
        }
        case ASTNode::Kind::GenericTypeNode: {
            auto* n = static_cast<const GenericTypeNode*>(node);
            for (size_t i = 0; i < n->getTypeArgs().size(); ++i) {
                addChild(out, "typeArg[" + std::to_string(i) + "]", n->getTypeArgs()[i]);
            }
            break;
        }

        case ASTNode::Kind::IdentifierPattern: {
            auto* n = static_cast<const IdentifierPattern*>(node);
            addChild(out, "type", n->getType());
            break;
        }
        case ASTNode::Kind::LiteralPattern: {
            auto* n = static_cast<const LiteralPattern*>(node);
            addChild(out, "literal", n->getLiteral());
            break;
        }
        case ASTNode::Kind::TuplePattern: {
            auto* n = static_cast<const TuplePattern*>(node);
            for (size_t i = 0; i < n->getElements().size(); ++i) {
                addChild(out, "element[" + std::to_string(i) + "]", n->getElements()[i]);
            }
            break;
        }
        case ASTNode::Kind::StructPattern: {
            auto* n = static_cast<const StructPattern*>(node);
            for (size_t i = 0; i < n->getFields().size(); ++i) {
                addChild(out, "field[" + n->getFields()[i].Name + "]", n->getFields()[i].Pat);
            }
            break;
        }
        case ASTNode::Kind::EnumPattern: {
            auto* n = static_cast<const EnumPattern*>(node);
            for (size_t i = 0; i < n->getPayload().size(); ++i) {
                addChild(out, "payload[" + std::to_string(i) + "]", n->getPayload()[i]);
            }
            break;
        }
        case ASTNode::Kind::RangePattern: {
            auto* n = static_cast<const RangePattern*>(node);
            addChild(out, "start", n->getStart());
            addChild(out, "end", n->getEnd());
            break;
        }
        case ASTNode::Kind::OrPattern: {
            auto* n = static_cast<const OrPattern*>(node);
            for (size_t i = 0; i < n->getPatterns().size(); ++i) {
                addChild(out, "pattern[" + std::to_string(i) + "]", n->getPatterns()[i]);
            }
            break;
        }
        case ASTNode::Kind::BindPattern: {
            auto* n = static_cast<const BindPattern*>(node);
            addChild(out, "inner", n->getInner());
            addChild(out, "type", n->getType());
            break;
        }

        default:
            break;
    }
}

void ASTDumper::addChild(std::vector<Child>& out, const std::string& label, const ASTNode* node) {
    if (node) {
        out.push_back(Child{label, node});
    }
}

} // namespace yuan
