/// \file ASTVisitor.h
/// \brief AST 访问者模式定义。
///
/// 本文件定义了 ASTVisitor 模板类，用于遍历和处理 AST 节点。
/// 使用 CRTP（Curiously Recurring Template Pattern）实现静态多态。

#ifndef YUAN_AST_ASTVISITOR_H
#define YUAN_AST_ASTVISITOR_H

#include "yuan/AST/AST.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Stmt.h"
#include "yuan/AST/Type.h"
#include "yuan/AST/Pattern.h"

namespace yuan {

/// \brief AST 访问者基类模板
///
/// 使用 CRTP 模式实现静态多态的访问者。派生类可以重写特定节点的
/// visit 方法来实现自定义的遍历逻辑。
///
/// \tparam Derived 派生类类型（CRTP）
/// \tparam RetTy 访问方法的返回类型，默认为 void
///
/// 使用示例：
/// \code
/// class MyVisitor : public ASTVisitor<MyVisitor, int> {
/// public:
///     int visitIntegerLiteralExpr(IntegerLiteralExpr* expr) {
///         return expr->getValue();
///     }
/// };
/// \endcode
template<typename Derived, typename RetTy = void>
class ASTVisitor {
public:
    /// \brief 获取派生类引用
    Derived& getDerived() { return *static_cast<Derived*>(this); }
    
    /// \brief 访问任意 AST 节点
    /// \param node 要访问的节点
    /// \return 访问结果
    RetTy visit(ASTNode* node) {
        if (!node) return RetTy();
        
        if (node->isDecl()) {
            return visitDecl(static_cast<Decl*>(node));
        } else if (node->isStmt()) {
            return visitStmt(static_cast<Stmt*>(node));
        } else if (node->isExpr()) {
            return visitExpr(static_cast<Expr*>(node));
        } else if (node->isTypeNode()) {
            return visitTypeNode(static_cast<TypeNode*>(node));
        } else if (node->isPattern()) {
            return visitPattern(static_cast<Pattern*>(node));
        }
        
        return RetTy();
    }
    
    // =========================================================================
    // 声明访问
    // =========================================================================
    
    /// \brief 访问声明节点（分发到具体类型）
    RetTy visitDecl(Decl* decl) {
        if (!decl) return RetTy();
        
        switch (decl->getKind()) {
            case ASTNode::Kind::VarDecl:
                return getDerived().visitVarDecl(static_cast<VarDecl*>(decl));
            case ASTNode::Kind::ConstDecl:
                return getDerived().visitConstDecl(static_cast<ConstDecl*>(decl));
            case ASTNode::Kind::ParamDecl:
                return getDerived().visitParamDecl(static_cast<ParamDecl*>(decl));
            case ASTNode::Kind::FuncDecl:
                return getDerived().visitFuncDecl(static_cast<FuncDecl*>(decl));
            case ASTNode::Kind::FieldDecl:
                return getDerived().visitFieldDecl(static_cast<FieldDecl*>(decl));
            case ASTNode::Kind::StructDecl:
                return getDerived().visitStructDecl(static_cast<StructDecl*>(decl));
            case ASTNode::Kind::EnumVariantDecl:
                return getDerived().visitEnumVariantDecl(static_cast<EnumVariantDecl*>(decl));
            case ASTNode::Kind::EnumDecl:
                return getDerived().visitEnumDecl(static_cast<EnumDecl*>(decl));
            case ASTNode::Kind::TypeAliasDecl:
                return getDerived().visitTypeAliasDecl(static_cast<TypeAliasDecl*>(decl));
            case ASTNode::Kind::TraitDecl:
                return getDerived().visitTraitDecl(static_cast<TraitDecl*>(decl));
            case ASTNode::Kind::ImplDecl:
                return getDerived().visitImplDecl(static_cast<ImplDecl*>(decl));
            default:
                return RetTy();
        }
    }
    
    /// \brief 访问变量声明
    RetTy visitVarDecl(VarDecl* decl) {
        if (decl->getPattern()) {
            getDerived().visitPattern(decl->getPattern());
        }
        if (decl->getType()) {
            getDerived().visitTypeNode(decl->getType());
        }
        if (decl->getInit()) {
            getDerived().visitExpr(decl->getInit());
        }
        return RetTy();
    }
    
    /// \brief 访问常量声明
    RetTy visitConstDecl(ConstDecl* decl) {
        if (decl->getType()) {
            getDerived().visitTypeNode(decl->getType());
        }
        if (decl->getInit()) {
            getDerived().visitExpr(decl->getInit());
        }
        return RetTy();
    }
    
    /// \brief 访问参数声明
    RetTy visitParamDecl(ParamDecl* decl) {
        if (decl->getType()) {
            getDerived().visitTypeNode(decl->getType());
        }
        return RetTy();
    }
    
    /// \brief 访问函数声明
    RetTy visitFuncDecl(FuncDecl* decl) {
        for (auto* param : decl->getParams()) {
            getDerived().visitParamDecl(param);
        }
        if (decl->getReturnType()) {
            getDerived().visitTypeNode(decl->getReturnType());
        }
        if (decl->getBody()) {
            getDerived().visitBlockStmt(decl->getBody());
        }
        return RetTy();
    }
    
    /// \brief 访问字段声明
    RetTy visitFieldDecl(FieldDecl* decl) {
        if (decl->getType()) {
            getDerived().visitTypeNode(decl->getType());
        }
        if (decl->getDefaultValue()) {
            getDerived().visitExpr(decl->getDefaultValue());
        }
        return RetTy();
    }
    
    /// \brief 访问结构体声明
    RetTy visitStructDecl(StructDecl* decl) {
        for (auto* field : decl->getFields()) {
            getDerived().visitFieldDecl(field);
        }
        return RetTy();
    }
    
    /// \brief 访问枚举变体声明
    RetTy visitEnumVariantDecl(EnumVariantDecl* decl) {
        if (decl->isTuple()) {
            for (auto* type : decl->getTupleTypes()) {
                getDerived().visitTypeNode(type);
            }
        } else if (decl->isStruct()) {
            for (auto* field : decl->getFields()) {
                getDerived().visitFieldDecl(field);
            }
        }
        return RetTy();
    }
    
    /// \brief 访问枚举声明
    RetTy visitEnumDecl(EnumDecl* decl) {
        for (auto* variant : decl->getVariants()) {
            getDerived().visitEnumVariantDecl(variant);
        }
        return RetTy();
    }
    
    /// \brief 访问类型别名声明
    RetTy visitTypeAliasDecl(TypeAliasDecl* decl) {
        if (decl->getAliasedType()) {
            getDerived().visitTypeNode(decl->getAliasedType());
        }
        return RetTy();
    }
    
    /// \brief 访问 Trait 声明
    RetTy visitTraitDecl(TraitDecl* decl) {
        for (auto* assocType : decl->getAssociatedTypes()) {
            getDerived().visitTypeAliasDecl(assocType);
        }
        for (auto* method : decl->getMethods()) {
            getDerived().visitFuncDecl(method);
        }
        return RetTy();
    }
    
    /// \brief 访问 Impl 块
    RetTy visitImplDecl(ImplDecl* decl) {
        if (decl->getTargetType()) {
            getDerived().visitTypeNode(decl->getTargetType());
        }
        for (auto* assocType : decl->getAssociatedTypes()) {
            getDerived().visitTypeAliasDecl(assocType);
        }
        for (auto* method : decl->getMethods()) {
            getDerived().visitFuncDecl(method);
        }
        return RetTy();
    }
    
    // =========================================================================
    // 语句访问
    // =========================================================================
    
    /// \brief 访问语句节点（分发到具体类型）
    RetTy visitStmt(Stmt* stmt) {
        if (!stmt) return RetTy();
        
        switch (stmt->getKind()) {
            case ASTNode::Kind::ExprStmt:
                return getDerived().visitExprStmt(static_cast<ExprStmt*>(stmt));
            case ASTNode::Kind::BlockStmt:
                return getDerived().visitBlockStmt(static_cast<BlockStmt*>(stmt));
            case ASTNode::Kind::ReturnStmt:
                return getDerived().visitReturnStmt(static_cast<ReturnStmt*>(stmt));
            case ASTNode::Kind::IfStmt:
                return getDerived().visitIfStmt(static_cast<IfStmt*>(stmt));
            case ASTNode::Kind::WhileStmt:
                return getDerived().visitWhileStmt(static_cast<WhileStmt*>(stmt));
            case ASTNode::Kind::LoopStmt:
                return getDerived().visitLoopStmt(static_cast<LoopStmt*>(stmt));
            case ASTNode::Kind::ForStmt:
                return getDerived().visitForStmt(static_cast<ForStmt*>(stmt));
            case ASTNode::Kind::MatchStmt:
                return getDerived().visitMatchStmt(static_cast<MatchStmt*>(stmt));
            case ASTNode::Kind::BreakStmt:
                return getDerived().visitBreakStmt(static_cast<BreakStmt*>(stmt));
            case ASTNode::Kind::ContinueStmt:
                return getDerived().visitContinueStmt(static_cast<ContinueStmt*>(stmt));
            case ASTNode::Kind::DeferStmt:
                return getDerived().visitDeferStmt(static_cast<DeferStmt*>(stmt));
            default:
                return RetTy();
        }
    }
    
    /// \brief 访问表达式语句
    RetTy visitExprStmt(ExprStmt* stmt) {
        if (stmt->getExpr()) {
            getDerived().visitExpr(stmt->getExpr());
        }
        return RetTy();
    }
    
    /// \brief 访问块语句
    RetTy visitBlockStmt(BlockStmt* stmt) {
        for (auto* s : stmt->getStatements()) {
            getDerived().visitStmt(s);
        }
        return RetTy();
    }
    
    /// \brief 访问 return 语句
    RetTy visitReturnStmt(ReturnStmt* stmt) {
        if (stmt->getValue()) {
            getDerived().visitExpr(stmt->getValue());
        }
        return RetTy();
    }
    
    /// \brief 访问 if 语句
    RetTy visitIfStmt(IfStmt* stmt) {
        for (const auto& branch : stmt->getBranches()) {
            if (branch.Condition) {
                getDerived().visitExpr(branch.Condition);
            }
            if (branch.Body) {
                getDerived().visitBlockStmt(branch.Body);
            }
        }
        return RetTy();
    }
    
    /// \brief 访问 while 语句
    RetTy visitWhileStmt(WhileStmt* stmt) {
        if (stmt->getCondition()) {
            getDerived().visitExpr(stmt->getCondition());
        }
        if (stmt->getBody()) {
            getDerived().visitBlockStmt(stmt->getBody());
        }
        return RetTy();
    }
    
    /// \brief 访问 loop 语句
    RetTy visitLoopStmt(LoopStmt* stmt) {
        if (stmt->getBody()) {
            getDerived().visitBlockStmt(stmt->getBody());
        }
        return RetTy();
    }
    
    /// \brief 访问 for 语句
    RetTy visitForStmt(ForStmt* stmt) {
        if (stmt->getPattern()) {
            getDerived().visitPattern(stmt->getPattern());
        }
        if (stmt->getIterable()) {
            getDerived().visitExpr(stmt->getIterable());
        }
        if (stmt->getBody()) {
            getDerived().visitBlockStmt(stmt->getBody());
        }
        return RetTy();
    }
    
    /// \brief 访问 match 语句
    RetTy visitMatchStmt(MatchStmt* stmt) {
        if (stmt->getScrutinee()) {
            getDerived().visitExpr(stmt->getScrutinee());
        }
        for (const auto& arm : stmt->getArms()) {
            if (arm.Pat) {
                getDerived().visitPattern(arm.Pat);
            }
            if (arm.Guard) {
                getDerived().visitExpr(arm.Guard);
            }
            if (arm.Body) {
                getDerived().visitStmt(arm.Body);
            }
        }
        return RetTy();
    }
    
    /// \brief 访问 break 语句
    RetTy visitBreakStmt(BreakStmt* /*stmt*/) {
        return RetTy();
    }
    
    /// \brief 访问 continue 语句
    RetTy visitContinueStmt(ContinueStmt* /*stmt*/) {
        return RetTy();
    }
    
    /// \brief 访问 defer 语句
    RetTy visitDeferStmt(DeferStmt* stmt) {
        if (stmt->getBody()) {
            getDerived().visitStmt(stmt->getBody());
        }
        return RetTy();
    }

    
    // =========================================================================
    // 表达式访问
    // =========================================================================
    
    /// \brief 访问表达式节点（分发到具体类型）
    RetTy visitExpr(Expr* expr) {
        if (!expr) return RetTy();
        
        switch (expr->getKind()) {
            case ASTNode::Kind::IntegerLiteralExpr:
                return getDerived().visitIntegerLiteralExpr(static_cast<IntegerLiteralExpr*>(expr));
            case ASTNode::Kind::FloatLiteralExpr:
                return getDerived().visitFloatLiteralExpr(static_cast<FloatLiteralExpr*>(expr));
            case ASTNode::Kind::BoolLiteralExpr:
                return getDerived().visitBoolLiteralExpr(static_cast<BoolLiteralExpr*>(expr));
            case ASTNode::Kind::CharLiteralExpr:
                return getDerived().visitCharLiteralExpr(static_cast<CharLiteralExpr*>(expr));
            case ASTNode::Kind::StringLiteralExpr:
                return getDerived().visitStringLiteralExpr(static_cast<StringLiteralExpr*>(expr));
            case ASTNode::Kind::NoneLiteralExpr:
                return getDerived().visitNoneLiteralExpr(static_cast<NoneLiteralExpr*>(expr));
            case ASTNode::Kind::IdentifierExpr:
                return getDerived().visitIdentifierExpr(static_cast<IdentifierExpr*>(expr));
            case ASTNode::Kind::MemberExpr:
                return getDerived().visitMemberExpr(static_cast<MemberExpr*>(expr));
            case ASTNode::Kind::BinaryExpr:
                return getDerived().visitBinaryExpr(static_cast<BinaryExpr*>(expr));
            case ASTNode::Kind::UnaryExpr:
                return getDerived().visitUnaryExpr(static_cast<UnaryExpr*>(expr));
            case ASTNode::Kind::AssignExpr:
                return getDerived().visitAssignExpr(static_cast<AssignExpr*>(expr));
            case ASTNode::Kind::CallExpr:
                return getDerived().visitCallExpr(static_cast<CallExpr*>(expr));
            case ASTNode::Kind::IndexExpr:
                return getDerived().visitIndexExpr(static_cast<IndexExpr*>(expr));
            case ASTNode::Kind::SliceExpr:
                return getDerived().visitSliceExpr(static_cast<SliceExpr*>(expr));
            case ASTNode::Kind::BuiltinCallExpr:
                return getDerived().visitBuiltinCallExpr(static_cast<BuiltinCallExpr*>(expr));
            case ASTNode::Kind::IfExpr:
                return getDerived().visitIfExpr(static_cast<IfExpr*>(expr));
            case ASTNode::Kind::MatchExpr:
                return getDerived().visitMatchExpr(static_cast<MatchExpr*>(expr));
            case ASTNode::Kind::ClosureExpr:
                return getDerived().visitClosureExpr(static_cast<ClosureExpr*>(expr));
            case ASTNode::Kind::ArrayExpr:
                return getDerived().visitArrayExpr(static_cast<ArrayExpr*>(expr));
            case ASTNode::Kind::TupleExpr:
                return getDerived().visitTupleExpr(static_cast<TupleExpr*>(expr));
            case ASTNode::Kind::StructExpr:
                return getDerived().visitStructExpr(static_cast<StructExpr*>(expr));
            case ASTNode::Kind::RangeExpr:
                return getDerived().visitRangeExpr(static_cast<RangeExpr*>(expr));
            case ASTNode::Kind::AwaitExpr:
                return getDerived().visitAwaitExpr(static_cast<AwaitExpr*>(expr));
            case ASTNode::Kind::ErrorPropagateExpr:
                return getDerived().visitErrorPropagateExpr(static_cast<ErrorPropagateExpr*>(expr));
            case ASTNode::Kind::ErrorHandleExpr:
                return getDerived().visitErrorHandleExpr(static_cast<ErrorHandleExpr*>(expr));
            case ASTNode::Kind::CastExpr:
                return getDerived().visitCastExpr(static_cast<CastExpr*>(expr));
            default:
                return RetTy();
        }
    }
    
    /// \brief 访问整数字面量
    RetTy visitIntegerLiteralExpr(IntegerLiteralExpr* /*expr*/) {
        return RetTy();
    }
    
    /// \brief 访问浮点数字面量
    RetTy visitFloatLiteralExpr(FloatLiteralExpr* /*expr*/) {
        return RetTy();
    }
    
    /// \brief 访问布尔字面量
    RetTy visitBoolLiteralExpr(BoolLiteralExpr* /*expr*/) {
        return RetTy();
    }
    
    /// \brief 访问字符字面量
    RetTy visitCharLiteralExpr(CharLiteralExpr* /*expr*/) {
        return RetTy();
    }
    
    /// \brief 访问字符串字面量
    RetTy visitStringLiteralExpr(StringLiteralExpr* /*expr*/) {
        return RetTy();
    }
    
    /// \brief 访问 None 字面量
    RetTy visitNoneLiteralExpr(NoneLiteralExpr* /*expr*/) {
        return RetTy();
    }
    
    /// \brief 访问标识符表达式
    RetTy visitIdentifierExpr(IdentifierExpr* /*expr*/) {
        return RetTy();
    }
    
    /// \brief 访问成员访问表达式
    RetTy visitMemberExpr(MemberExpr* expr) {
        if (expr->getBase()) {
            getDerived().visitExpr(expr->getBase());
        }
        return RetTy();
    }
    
    /// \brief 访问二元表达式
    RetTy visitBinaryExpr(BinaryExpr* expr) {
        if (expr->getLHS()) {
            getDerived().visitExpr(expr->getLHS());
        }
        if (expr->getRHS()) {
            getDerived().visitExpr(expr->getRHS());
        }
        return RetTy();
    }
    
    /// \brief 访问一元表达式
    RetTy visitUnaryExpr(UnaryExpr* expr) {
        if (expr->getOperand()) {
            getDerived().visitExpr(expr->getOperand());
        }
        return RetTy();
    }
    
    /// \brief 访问赋值表达式
    RetTy visitAssignExpr(AssignExpr* expr) {
        if (expr->getTarget()) {
            getDerived().visitExpr(expr->getTarget());
        }
        if (expr->getValue()) {
            getDerived().visitExpr(expr->getValue());
        }
        return RetTy();
    }
    
    /// \brief 访问函数调用表达式
    RetTy visitCallExpr(CallExpr* expr) {
        if (expr->getCallee()) {
            getDerived().visitExpr(expr->getCallee());
        }
        for (auto* typeArg : expr->getTypeArgs()) {
            if (typeArg) {
                getDerived().visitTypeNode(typeArg);
            }
        }
        for (const auto& arg : expr->getArgs()) {
            if (arg.Value) {
                getDerived().visitExpr(arg.Value);
            }
        }
        return RetTy();
    }
    
    /// \brief 访问索引表达式
    RetTy visitIndexExpr(IndexExpr* expr) {
        if (expr->getBase()) {
            getDerived().visitExpr(expr->getBase());
        }
        if (expr->getIndex()) {
            getDerived().visitExpr(expr->getIndex());
        }
        return RetTy();
    }
    
    /// \brief 访问切片表达式
    RetTy visitSliceExpr(SliceExpr* expr) {
        if (expr->getBase()) {
            getDerived().visitExpr(expr->getBase());
        }
        if (expr->getStart()) {
            getDerived().visitExpr(expr->getStart());
        }
        if (expr->getEnd()) {
            getDerived().visitExpr(expr->getEnd());
        }
        return RetTy();
    }
    
    /// \brief 访问内置函数调用表达式
    RetTy visitBuiltinCallExpr(BuiltinCallExpr* expr) {
        for (const auto& arg : expr->getArgs()) {
            if (arg.isExpr()) {
                getDerived().visitExpr(arg.getExpr());
            } else if (arg.isType()) {
                getDerived().visitTypeNode(arg.getType());
            }
        }
        return RetTy();
    }
    
    /// \brief 访问 if 表达式
    RetTy visitIfExpr(IfExpr* expr) {
        for (const auto& branch : expr->getBranches()) {
            if (branch.Condition) {
                getDerived().visitExpr(branch.Condition);
            }
            if (branch.Body) {
                getDerived().visitExpr(branch.Body);
            }
        }
        return RetTy();
    }
    
    /// \brief 访问 match 表达式
    RetTy visitMatchExpr(MatchExpr* expr) {
        if (expr->getScrutinee()) {
            getDerived().visitExpr(expr->getScrutinee());
        }
        for (const auto& arm : expr->getArms()) {
            if (arm.Pat) {
                getDerived().visitPattern(arm.Pat);
            }
            if (arm.Guard) {
                getDerived().visitExpr(arm.Guard);
            }
            if (arm.Body) {
                getDerived().visitExpr(arm.Body);
            }
        }
        return RetTy();
    }
    
    /// \brief 访问闭包表达式
    RetTy visitClosureExpr(ClosureExpr* expr) {
        for (auto* param : expr->getParams()) {
            getDerived().visitParamDecl(param);
        }
        if (expr->getReturnType()) {
            getDerived().visitTypeNode(expr->getReturnType());
        }
        if (expr->getBody()) {
            getDerived().visitExpr(expr->getBody());
        }
        return RetTy();
    }
    
    /// \brief 访问数组表达式
    RetTy visitArrayExpr(ArrayExpr* expr) {
        for (auto* elem : expr->getElements()) {
            getDerived().visitExpr(elem);
        }
        if (expr->isRepeat() && expr->getRepeatCount()) {
            getDerived().visitExpr(expr->getRepeatCount());
        }
        return RetTy();
    }
    
    /// \brief 访问元组表达式
    RetTy visitTupleExpr(TupleExpr* expr) {
        for (auto* elem : expr->getElements()) {
            getDerived().visitExpr(elem);
        }
        return RetTy();
    }
    
    /// \brief 访问结构体表达式
    RetTy visitStructExpr(StructExpr* expr) {
        for (const auto& field : expr->getFields()) {
            if (field.Value) {
                getDerived().visitExpr(field.Value);
            }
        }
        if (expr->getBase()) {
            getDerived().visitExpr(expr->getBase());
        }
        return RetTy();
    }
    
    /// \brief 访问范围表达式
    RetTy visitRangeExpr(RangeExpr* expr) {
        if (expr->getStart()) {
            getDerived().visitExpr(expr->getStart());
        }
        if (expr->getEnd()) {
            getDerived().visitExpr(expr->getEnd());
        }
        return RetTy();
    }

    /// \brief 访问 await 表达式
    RetTy visitAwaitExpr(AwaitExpr* expr) {
        if (expr->getInner()) {
            getDerived().visitExpr(expr->getInner());
        }
        return RetTy();
    }
    
    /// \brief 访问错误传播表达式
    RetTy visitErrorPropagateExpr(ErrorPropagateExpr* expr) {
        if (expr->getInner()) {
            getDerived().visitExpr(expr->getInner());
        }
        return RetTy();
    }
    
    /// \brief 访问错误处理表达式
    RetTy visitErrorHandleExpr(ErrorHandleExpr* expr) {
        if (expr->getInner()) {
            getDerived().visitExpr(expr->getInner());
        }
        if (expr->getHandler()) {
            getDerived().visitBlockStmt(expr->getHandler());
        }
        return RetTy();
    }
    
    /// \brief 访问类型转换表达式
    RetTy visitCastExpr(CastExpr* expr) {
        if (expr->getExpr()) {
            getDerived().visitExpr(expr->getExpr());
        }
        if (expr->getTargetType()) {
            getDerived().visitTypeNode(expr->getTargetType());
        }
        return RetTy();
    }

    
    // =========================================================================
    // 类型访问
    // =========================================================================
    
    /// \brief 访问类型节点（分发到具体类型）
    RetTy visitTypeNode(TypeNode* type) {
        if (!type) return RetTy();
        
        switch (type->getKind()) {
            case ASTNode::Kind::BuiltinTypeNode:
                return getDerived().visitBuiltinTypeNode(static_cast<BuiltinTypeNode*>(type));
            case ASTNode::Kind::IdentifierTypeNode:
                return getDerived().visitIdentifierTypeNode(static_cast<IdentifierTypeNode*>(type));
            case ASTNode::Kind::ArrayTypeNode:
                return getDerived().visitArrayTypeNode(static_cast<ArrayTypeNode*>(type));
            case ASTNode::Kind::SliceTypeNode:
                return getDerived().visitSliceTypeNode(static_cast<SliceTypeNode*>(type));
            case ASTNode::Kind::TupleTypeNode:
                return getDerived().visitTupleTypeNode(static_cast<TupleTypeNode*>(type));
            case ASTNode::Kind::OptionalTypeNode:
                return getDerived().visitOptionalTypeNode(static_cast<OptionalTypeNode*>(type));
            case ASTNode::Kind::ReferenceTypeNode:
                return getDerived().visitReferenceTypeNode(static_cast<ReferenceTypeNode*>(type));
            case ASTNode::Kind::PointerTypeNode:
                return getDerived().visitPointerTypeNode(static_cast<PointerTypeNode*>(type));
            case ASTNode::Kind::FunctionTypeNode:
                return getDerived().visitFunctionTypeNode(static_cast<FunctionTypeNode*>(type));
            case ASTNode::Kind::ErrorTypeNode:
                return getDerived().visitErrorTypeNode(static_cast<ErrorTypeNode*>(type));
            case ASTNode::Kind::GenericTypeNode:
                return getDerived().visitGenericTypeNode(static_cast<GenericTypeNode*>(type));
            default:
                return RetTy();
        }
    }
    
    /// \brief 访问内置类型
    RetTy visitBuiltinTypeNode(BuiltinTypeNode* /*type*/) {
        return RetTy();
    }
    
    /// \brief 访问标识符类型
    RetTy visitIdentifierTypeNode(IdentifierTypeNode* /*type*/) {
        return RetTy();
    }
    
    /// \brief 访问数组类型
    RetTy visitArrayTypeNode(ArrayTypeNode* type) {
        if (type->getElementType()) {
            getDerived().visitTypeNode(type->getElementType());
        }
        if (type->getSize()) {
            getDerived().visitExpr(type->getSize());
        }
        return RetTy();
    }
    
    /// \brief 访问切片类型
    RetTy visitSliceTypeNode(SliceTypeNode* type) {
        if (type->getElementType()) {
            getDerived().visitTypeNode(type->getElementType());
        }
        return RetTy();
    }
    
    /// \brief 访问元组类型
    RetTy visitTupleTypeNode(TupleTypeNode* type) {
        for (auto* elem : type->getElements()) {
            getDerived().visitTypeNode(elem);
        }
        return RetTy();
    }
    
    /// \brief 访问 Optional 类型
    RetTy visitOptionalTypeNode(OptionalTypeNode* type) {
        if (type->getInnerType()) {
            getDerived().visitTypeNode(type->getInnerType());
        }
        return RetTy();
    }
    
    /// \brief 访问引用类型
    RetTy visitReferenceTypeNode(ReferenceTypeNode* type) {
        if (type->getPointeeType()) {
            getDerived().visitTypeNode(type->getPointeeType());
        }
        return RetTy();
    }
    
    /// \brief 访问指针类型
    RetTy visitPointerTypeNode(PointerTypeNode* type) {
        if (type->getPointeeType()) {
            getDerived().visitTypeNode(type->getPointeeType());
        }
        return RetTy();
    }
    
    /// \brief 访问函数类型
    RetTy visitFunctionTypeNode(FunctionTypeNode* type) {
        for (auto* param : type->getParamTypes()) {
            getDerived().visitTypeNode(param);
        }
        if (type->getReturnType()) {
            getDerived().visitTypeNode(type->getReturnType());
        }
        return RetTy();
    }
    
    /// \brief 访问错误类型
    RetTy visitErrorTypeNode(ErrorTypeNode* type) {
        if (type->getSuccessType()) {
            getDerived().visitTypeNode(type->getSuccessType());
        }
        return RetTy();
    }
    
    /// \brief 访问泛型类型
    RetTy visitGenericTypeNode(GenericTypeNode* type) {
        for (auto* arg : type->getTypeArgs()) {
            getDerived().visitTypeNode(arg);
        }
        return RetTy();
    }
    
    // =========================================================================
    // 模式访问
    // =========================================================================
    
    /// \brief 访问模式节点（分发到具体类型）
    RetTy visitPattern(Pattern* pattern) {
        if (!pattern) return RetTy();
        
        switch (pattern->getKind()) {
            case ASTNode::Kind::WildcardPattern:
                return getDerived().visitWildcardPattern(static_cast<WildcardPattern*>(pattern));
            case ASTNode::Kind::IdentifierPattern:
                return getDerived().visitIdentifierPattern(static_cast<IdentifierPattern*>(pattern));
            case ASTNode::Kind::LiteralPattern:
                return getDerived().visitLiteralPattern(static_cast<LiteralPattern*>(pattern));
            case ASTNode::Kind::TuplePattern:
                return getDerived().visitTuplePattern(static_cast<TuplePattern*>(pattern));
            case ASTNode::Kind::StructPattern:
                return getDerived().visitStructPattern(static_cast<StructPattern*>(pattern));
            case ASTNode::Kind::EnumPattern:
                return getDerived().visitEnumPattern(static_cast<EnumPattern*>(pattern));
            case ASTNode::Kind::RangePattern:
                return getDerived().visitRangePattern(static_cast<RangePattern*>(pattern));
            case ASTNode::Kind::OrPattern:
                return getDerived().visitOrPattern(static_cast<OrPattern*>(pattern));
            case ASTNode::Kind::BindPattern:
                return getDerived().visitBindPattern(static_cast<BindPattern*>(pattern));
            default:
                return RetTy();
        }
    }
    
    /// \brief 访问通配符模式
    RetTy visitWildcardPattern(WildcardPattern* /*pattern*/) {
        return RetTy();
    }
    
    /// \brief 访问标识符模式
    RetTy visitIdentifierPattern(IdentifierPattern* pattern) {
        if (pattern->getType()) {
            getDerived().visitTypeNode(pattern->getType());
        }
        return RetTy();
    }
    
    /// \brief 访问字面量模式
    RetTy visitLiteralPattern(LiteralPattern* pattern) {
        if (pattern->getLiteral()) {
            getDerived().visitExpr(pattern->getLiteral());
        }
        return RetTy();
    }
    
    /// \brief 访问元组模式
    RetTy visitTuplePattern(TuplePattern* pattern) {
        for (auto* elem : pattern->getElements()) {
            getDerived().visitPattern(elem);
        }
        return RetTy();
    }
    
    /// \brief 访问结构体模式
    RetTy visitStructPattern(StructPattern* pattern) {
        for (const auto& field : pattern->getFields()) {
            if (field.Pat) {
                getDerived().visitPattern(field.Pat);
            }
        }
        return RetTy();
    }
    
    /// \brief 访问枚举模式
    RetTy visitEnumPattern(EnumPattern* pattern) {
        for (auto* payload : pattern->getPayload()) {
            getDerived().visitPattern(payload);
        }
        return RetTy();
    }
    
    /// \brief 访问范围模式
    RetTy visitRangePattern(RangePattern* pattern) {
        if (pattern->getStart()) {
            getDerived().visitExpr(pattern->getStart());
        }
        if (pattern->getEnd()) {
            getDerived().visitExpr(pattern->getEnd());
        }
        return RetTy();
    }

    /// \brief 访问或模式
    RetTy visitOrPattern(OrPattern* pattern) {
        for (auto* alt : pattern->getPatterns()) {
            getDerived().visitPattern(alt);
        }
        return RetTy();
    }

    /// \brief 访问绑定模式
    RetTy visitBindPattern(BindPattern* pattern) {
        if (pattern->getType()) {
            getDerived().visitTypeNode(pattern->getType());
        }
        getDerived().visitPattern(pattern->getInner());
        return RetTy();
    }
};

/// \brief 常量 AST 访问者基类模板
///
/// 与 ASTVisitor 类似，但访问的是 const 节点。
///
/// \tparam Derived 派生类类型（CRTP）
/// \tparam RetTy 访问方法的返回类型，默认为 void
template<typename Derived, typename RetTy = void>
class ConstASTVisitor {
public:
    /// \brief 获取派生类引用
    Derived& getDerived() { return *static_cast<Derived*>(this); }
    
    /// \brief 访问任意 AST 节点
    RetTy visit(const ASTNode* node) {
        if (!node) return RetTy();
        
        if (node->isDecl()) {
            return visitDecl(static_cast<const Decl*>(node));
        } else if (node->isStmt()) {
            return visitStmt(static_cast<const Stmt*>(node));
        } else if (node->isExpr()) {
            return visitExpr(static_cast<const Expr*>(node));
        } else if (node->isTypeNode()) {
            return visitTypeNode(static_cast<const TypeNode*>(node));
        } else if (node->isPattern()) {
            return visitPattern(static_cast<const Pattern*>(node));
        }
        
        return RetTy();
    }
    
    // 声明访问
    RetTy visitDecl(const Decl* decl);
    RetTy visitVarDecl(const VarDecl* decl);
    RetTy visitConstDecl(const ConstDecl* decl);
    RetTy visitParamDecl(const ParamDecl* decl);
    RetTy visitFuncDecl(const FuncDecl* decl);
    RetTy visitFieldDecl(const FieldDecl* decl);
    RetTy visitStructDecl(const StructDecl* decl);
    RetTy visitEnumVariantDecl(const EnumVariantDecl* decl);
    RetTy visitEnumDecl(const EnumDecl* decl);
    RetTy visitTypeAliasDecl(const TypeAliasDecl* decl);
    RetTy visitTraitDecl(const TraitDecl* decl);
    RetTy visitImplDecl(const ImplDecl* decl);
    
    // 语句访问
    RetTy visitStmt(const Stmt* stmt);
    RetTy visitExprStmt(const ExprStmt* stmt);
    RetTy visitBlockStmt(const BlockStmt* stmt);
    RetTy visitReturnStmt(const ReturnStmt* stmt);
    RetTy visitIfStmt(const IfStmt* stmt);
    RetTy visitWhileStmt(const WhileStmt* stmt);
    RetTy visitLoopStmt(const LoopStmt* stmt);
    RetTy visitForStmt(const ForStmt* stmt);
    RetTy visitMatchStmt(const MatchStmt* stmt);
    RetTy visitBreakStmt(const BreakStmt* stmt);
    RetTy visitContinueStmt(const ContinueStmt* stmt);
    RetTy visitDeferStmt(const DeferStmt* stmt);
    
    // 表达式访问
    RetTy visitExpr(const Expr* expr);
    RetTy visitIntegerLiteralExpr(const IntegerLiteralExpr* expr);
    RetTy visitFloatLiteralExpr(const FloatLiteralExpr* expr);
    RetTy visitBoolLiteralExpr(const BoolLiteralExpr* expr);
    RetTy visitCharLiteralExpr(const CharLiteralExpr* expr);
    RetTy visitStringLiteralExpr(const StringLiteralExpr* expr);
    RetTy visitNoneLiteralExpr(const NoneLiteralExpr* expr);
    RetTy visitIdentifierExpr(const IdentifierExpr* expr);
    RetTy visitMemberExpr(const MemberExpr* expr);
    RetTy visitBinaryExpr(const BinaryExpr* expr);
    RetTy visitUnaryExpr(const UnaryExpr* expr);
    RetTy visitAssignExpr(const AssignExpr* expr);
    RetTy visitCallExpr(const CallExpr* expr);
    RetTy visitIndexExpr(const IndexExpr* expr);
    RetTy visitSliceExpr(const SliceExpr* expr);
    RetTy visitBuiltinCallExpr(const BuiltinCallExpr* expr);
    RetTy visitIfExpr(const IfExpr* expr);
    RetTy visitMatchExpr(const MatchExpr* expr);
    RetTy visitClosureExpr(const ClosureExpr* expr);
    RetTy visitArrayExpr(const ArrayExpr* expr);
    RetTy visitTupleExpr(const TupleExpr* expr);
    RetTy visitStructExpr(const StructExpr* expr);
    RetTy visitRangeExpr(const RangeExpr* expr);
    RetTy visitAwaitExpr(const AwaitExpr* expr);
    RetTy visitErrorPropagateExpr(const ErrorPropagateExpr* expr);
    RetTy visitErrorHandleExpr(const ErrorHandleExpr* expr);
    RetTy visitCastExpr(const CastExpr* expr);
    
    // 类型访问
    RetTy visitTypeNode(const TypeNode* type);
    RetTy visitBuiltinTypeNode(const BuiltinTypeNode* type);
    RetTy visitIdentifierTypeNode(const IdentifierTypeNode* type);
    RetTy visitArrayTypeNode(const ArrayTypeNode* type);
    RetTy visitSliceTypeNode(const SliceTypeNode* type);
    RetTy visitTupleTypeNode(const TupleTypeNode* type);
    RetTy visitOptionalTypeNode(const OptionalTypeNode* type);
    RetTy visitReferenceTypeNode(const ReferenceTypeNode* type);
    RetTy visitPointerTypeNode(const PointerTypeNode* type);
    RetTy visitFunctionTypeNode(const FunctionTypeNode* type);
    RetTy visitErrorTypeNode(const ErrorTypeNode* type);
    RetTy visitGenericTypeNode(const GenericTypeNode* type);
    
    // 模式访问
    RetTy visitPattern(const Pattern* pattern);
    RetTy visitWildcardPattern(const WildcardPattern* pattern);
    RetTy visitIdentifierPattern(const IdentifierPattern* pattern);
    RetTy visitLiteralPattern(const LiteralPattern* pattern);
    RetTy visitTuplePattern(const TuplePattern* pattern);
    RetTy visitStructPattern(const StructPattern* pattern);
    RetTy visitEnumPattern(const EnumPattern* pattern);
    RetTy visitRangePattern(const RangePattern* pattern);
    RetTy visitOrPattern(const OrPattern* pattern);
    RetTy visitBindPattern(const BindPattern* pattern);
};

} // namespace yuan

#endif // YUAN_AST_ASTVISITOR_H
