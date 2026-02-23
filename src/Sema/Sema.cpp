/// \file Sema.cpp
/// \brief 语义分析器实现。
///
/// 本文件实现了 Yuan 语言的语义分析器，负责类型检查、符号解析、
/// 语义验证等工作。

#include "yuan/Sema/Sema.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/Stmt.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Type.h"
#include "yuan/AST/Pattern.h"
#include "yuan/Sema/Type.h"
#include "yuan/Sema/TypeChecker.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Basic/SourceManager.h"
#include "yuan/Builtin/BuiltinRegistry.h"
#include <cstdint>
#include <functional>
#include <set>
#include <iostream>
#include <array>

namespace yuan {

namespace {

Type* unwrapAliases(Type* type) {
    Type* current = type;
    while (current && current->isTypeAlias()) {
        current = static_cast<TypeAlias*>(current)->getAliasedType();
    }
    return current;
}

Type* unwrapValueType(Type* type) {
    Type* current = unwrapAliases(type);
    while (current && current->isReference()) {
        current = unwrapAliases(static_cast<ReferenceType*>(current)->getPointeeType());
    }
    return current;
}

bool isOperatorTraitName(const std::string& traitName) {
    static const std::unordered_set<std::string> kOperatorTraits = {
        "Add", "Sub", "Mul", "Div", "Mod",
        "Eq", "Ne", "Lt", "Le", "Gt", "Ge",
        "Neg", "Not", "BitNot"
    };
    return kOperatorTraits.find(traitName) != kOperatorTraits.end();
}

bool isBuiltinOperatorForbiddenTarget(Type* type) {
    Type* base = unwrapAliases(type);
    return base && (base->isInteger() || base->isFloat() || base->isBool() ||
                    base->isChar() || base->isString());
}

bool isBuiltinArithmeticType(Type* type) {
    Type* base = unwrapValueType(type);
    return base && base->isNumeric();
}

bool isBuiltinComparisonType(Type* type) {
    Type* base = unwrapValueType(type);
    return base && (base->isInteger() || base->isFloat() || base->isBool() ||
                    base->isChar() || base->isString() || base->isPointer());
}

enum class OwnershipState {
    Live,
    Moved,
    MaybeMoved
};

bool isTrackedOwnershipDecl(const Decl* decl) {
    if (!decl) {
        return false;
    }
    return decl->getKind() == ASTNode::Kind::VarDecl ||
           decl->getKind() == ASTNode::Kind::ParamDecl;
}

std::string getOwnershipDeclName(const Decl* decl) {
    if (!decl) {
        return "<value>";
    }
    if (decl->getKind() == ASTNode::Kind::VarDecl) {
        return static_cast<const VarDecl*>(decl)->getName();
    }
    if (decl->getKind() == ASTNode::Kind::ParamDecl) {
        return static_cast<const ParamDecl*>(decl)->getName();
    }
    return "<value>";
}

OwnershipState joinOwnershipStates(const std::vector<OwnershipState>& states) {
    if (states.empty()) {
        return OwnershipState::Live;
    }
    bool allLive = true;
    bool allMoved = true;
    for (OwnershipState state : states) {
        if (state != OwnershipState::Live) {
            allLive = false;
        }
        if (state != OwnershipState::Moved) {
            allMoved = false;
        }
    }
    if (allLive) {
        return OwnershipState::Live;
    }
    if (allMoved) {
        return OwnershipState::Moved;
    }
    return OwnershipState::MaybeMoved;
}

class OwnershipAnalyzer {
public:
    OwnershipAnalyzer(Sema& sema, FuncDecl* func)
        : SemaRef(sema), Diag(sema.getDiagnostics()), Func(func) {}

    bool run() {
        if (!Func || !Func->getBody()) {
            return true;
        }

        enterScope();
        for (ParamDecl* param : Func->getParams()) {
            if (isTrackedOwnershipDecl(param)) {
                trackDecl(param, OwnershipState::Live);
            }
        }

        analyzeStmt(Func->getBody());
        exitScope();
        return Success;
    }

private:
    Sema& SemaRef;
    DiagnosticEngine& Diag;
    FuncDecl* Func = nullptr;
    std::unordered_map<const Decl*, OwnershipState> States;
    std::vector<std::vector<const Decl*>> ScopeDecls;
    bool Success = true;

    void enterScope() {
        ScopeDecls.emplace_back();
    }

    void exitScope() {
        if (ScopeDecls.empty()) {
            return;
        }
        for (const Decl* decl : ScopeDecls.back()) {
            States.erase(decl);
        }
        ScopeDecls.pop_back();
    }

    void trackDecl(const Decl* decl, OwnershipState state) {
        if (!decl || !isTrackedOwnershipDecl(decl)) {
            return;
        }
        if (ScopeDecls.empty()) {
            enterScope();
        }
        States[decl] = state;
        ScopeDecls.back().push_back(decl);
    }

    const Decl* getRootPlaceDecl(Expr* expr) const {
        if (!expr) {
            return nullptr;
        }
        if (auto* ident = dynamic_cast<IdentifierExpr*>(expr)) {
            Decl* decl = ident->getResolvedDecl();
            if (isTrackedOwnershipDecl(decl)) {
                return decl;
            }
            return nullptr;
        }
        if (auto* member = dynamic_cast<MemberExpr*>(expr)) {
            return getRootPlaceDecl(member->getBase());
        }
        if (auto* index = dynamic_cast<IndexExpr*>(expr)) {
            return getRootPlaceDecl(index->getBase());
        }
        return nullptr;
    }

    OwnershipState getStateOrLive(const Decl* decl) const {
        auto it = States.find(decl);
        if (it == States.end()) {
            return OwnershipState::Live;
        }
        return it->second;
    }

    void setStateIfTracked(const Decl* decl, OwnershipState state) {
        auto it = States.find(decl);
        if (it != States.end()) {
            it->second = state;
        }
    }

    void reportInvalidUse(const Decl* decl, Expr* atExpr) {
        if (!decl || !atExpr) {
            return;
        }
        OwnershipState state = getStateOrLive(decl);
        if (state == OwnershipState::Moved) {
            Diag.report(DiagID::err_use_after_move, atExpr->getBeginLoc(), atExpr->getRange())
                << getOwnershipDeclName(decl);
            Success = false;
        } else if (state == OwnershipState::MaybeMoved) {
            Diag.report(DiagID::err_use_of_maybe_moved, atExpr->getBeginLoc(), atExpr->getRange())
                << getOwnershipDeclName(decl);
            Success = false;
        }
    }

    void analyzePatternBindings(Pattern* pattern) {
        if (!pattern) {
            return;
        }
        switch (pattern->getKind()) {
            case ASTNode::Kind::IdentifierPattern: {
                auto* ident = static_cast<IdentifierPattern*>(pattern);
                if (Decl* decl = ident->getDecl()) {
                    trackDecl(decl, OwnershipState::Live);
                }
                return;
            }
            case ASTNode::Kind::BindPattern: {
                auto* bind = static_cast<BindPattern*>(pattern);
                if (Decl* decl = bind->getDecl()) {
                    trackDecl(decl, OwnershipState::Live);
                }
                analyzePatternBindings(bind->getInner());
                return;
            }
            case ASTNode::Kind::TuplePattern: {
                auto* tuple = static_cast<TuplePattern*>(pattern);
                for (Pattern* elem : tuple->getElements()) {
                    analyzePatternBindings(elem);
                }
                return;
            }
            case ASTNode::Kind::StructPattern: {
                auto* s = static_cast<StructPattern*>(pattern);
                for (const auto& field : s->getFields()) {
                    analyzePatternBindings(field.Pat);
                }
                return;
            }
            case ASTNode::Kind::EnumPattern: {
                auto* e = static_cast<EnumPattern*>(pattern);
                for (Pattern* payload : e->getPayload()) {
                    analyzePatternBindings(payload);
                }
                return;
            }
            case ASTNode::Kind::OrPattern: {
                auto* o = static_cast<OrPattern*>(pattern);
                for (Pattern* alt : o->getPatterns()) {
                    analyzePatternBindings(alt);
                }
                return;
            }
            default:
                return;
        }
    }

    void consumeExprValue(Expr* expr) {
        if (!expr) {
            return;
        }
        Type* exprType = expr->getType();
        if (!exprType || SemaRef.isCopyType(exprType)) {
            analyzeExprRead(expr);
            return;
        }

        if (auto* ident = dynamic_cast<IdentifierExpr*>(expr)) {
            Decl* decl = ident->getResolvedDecl();
            if (isTrackedOwnershipDecl(decl)) {
                reportInvalidUse(decl, expr);
                setStateIfTracked(decl, OwnershipState::Moved);
                ident->setMoveConsumed(true);
                return;
            }
        }

        if (dynamic_cast<MemberExpr*>(expr) || dynamic_cast<IndexExpr*>(expr)) {
            if (const Decl* root = getRootPlaceDecl(expr)) {
                if (States.find(root) != States.end()) {
                    Diag.report(DiagID::err_partial_move_not_supported, expr->getBeginLoc(),
                                expr->getRange())
                        << getOwnershipDeclName(root);
                    Success = false;
                }
            }
        }

        analyzeExprRead(expr);
    }

    void analyzeCallExpr(CallExpr* call) {
        if (!call) {
            return;
        }

        auto* memberCallee = dynamic_cast<MemberExpr*>(call->getCallee());
        auto* calleeType = dynamic_cast<FunctionType*>(call->getCallee() ? call->getCallee()->getType()
                                                                          : nullptr);
        FuncDecl* calleeDecl = nullptr;
        bool baseIsType = false;

        if (memberCallee) {
            if (Decl* resolved = memberCallee->getResolvedDecl()) {
                if (resolved->getKind() == ASTNode::Kind::FuncDecl) {
                    calleeDecl = static_cast<FuncDecl*>(resolved);
                }
            }
            if (auto* identBase = dynamic_cast<IdentifierExpr*>(memberCallee->getBase())) {
                if (Decl* baseDecl = identBase->getResolvedDecl()) {
                    if (baseDecl->getKind() == ASTNode::Kind::StructDecl ||
                        baseDecl->getKind() == ASTNode::Kind::EnumDecl ||
                        baseDecl->getKind() == ASTNode::Kind::TraitDecl ||
                        baseDecl->getKind() == ASTNode::Kind::TypeAliasDecl) {
                        baseIsType = true;
                    }
                }
            }
        } else if (auto* identCallee = dynamic_cast<IdentifierExpr*>(call->getCallee())) {
            if (Decl* resolved = identCallee->getResolvedDecl()) {
                if (resolved->getKind() == ASTNode::Kind::FuncDecl) {
                    calleeDecl = static_cast<FuncDecl*>(resolved);
                }
            }
        }

        bool injectSelf = false;
        if (memberCallee && calleeDecl && !calleeDecl->getParams().empty() &&
            calleeDecl->getParams()[0]->isSelf() && !baseIsType) {
            injectSelf = true;
        }

        if (memberCallee && injectSelf && memberCallee->getMember() == "drop" &&
            call->getArgCount() == 0) {
            const Decl* root = getRootPlaceDecl(memberCallee->getBase());
            std::string name = root ? getOwnershipDeclName(root) : memberCallee->getMember();
            Diag.report(DiagID::err_explicit_drop_call_forbidden, call->getBeginLoc(), call->getRange())
                << name;
            Success = false;
        }

        if (memberCallee) {
            if (injectSelf && calleeType && calleeType->getParamCount() > 0) {
                Type* selfParamType = calleeType->getParam(0);
                if (selfParamType &&
                    !selfParamType->isReference() &&
                    !selfParamType->isPointer()) {
                    consumeExprValue(memberCallee->getBase());
                } else {
                    analyzeExprRead(memberCallee->getBase());
                }
            } else {
                analyzeExprRead(memberCallee->getBase());
            }
        } else {
            analyzeExprRead(call->getCallee());
        }

        const auto& args = call->getArgs();
        size_t paramStart = injectSelf ? 1 : 0;
        for (size_t i = 0; i < args.size(); ++i) {
            Expr* argExpr = args[i].Value;
            if (!argExpr) {
                continue;
            }
            if (args[i].IsSpread || !calleeType) {
                analyzeExprRead(argExpr);
                continue;
            }

            Type* paramType = nullptr;
            size_t paramCount = calleeType->getParamCount();
            if (calleeType->isVariadic() && paramCount > 0 &&
                (i + paramStart) >= (paramCount - 1)) {
                paramType = calleeType->getParam(paramCount - 1);
                if (paramType && paramType->isVarArgs()) {
                    paramType = static_cast<VarArgsType*>(paramType)->getElementType();
                }
            } else if ((i + paramStart) < paramCount) {
                paramType = calleeType->getParam(i + paramStart);
            }

            if (paramType && (paramType->isReference() || paramType->isPointer())) {
                analyzeExprRead(argExpr);
            } else {
                consumeExprValue(argExpr);
            }
        }
    }

    void analyzeAssignExpr(AssignExpr* assign) {
        if (!assign) {
            return;
        }

        if (assign->isCompound()) {
            analyzeExprRead(assign->getTarget());
            analyzeExprRead(assign->getValue());
            return;
        }

        consumeExprValue(assign->getValue());
        if (auto* identTarget = dynamic_cast<IdentifierExpr*>(assign->getTarget())) {
            if (Decl* decl = identTarget->getResolvedDecl()) {
                setStateIfTracked(decl, OwnershipState::Live);
            }
        } else {
            analyzeExprRead(assign->getTarget());
        }
    }

    bool stmtTerminates(Stmt* stmt) const {
        if (!stmt) {
            return false;
        }
        switch (stmt->getKind()) {
            case ASTNode::Kind::ReturnStmt:
            case ASTNode::Kind::BreakStmt:
            case ASTNode::Kind::ContinueStmt:
                return true;
            case ASTNode::Kind::BlockStmt: {
                auto* block = static_cast<BlockStmt*>(stmt);
                for (Stmt* inner : block->getStatements()) {
                    if (stmtTerminates(inner)) {
                        return true;
                    }
                }
                return false;
            }
            case ASTNode::Kind::IfStmt: {
                auto* ifStmt = static_cast<IfStmt*>(stmt);
                if (!ifStmt->hasElse()) {
                    return false;
                }
                for (const auto& branch : ifStmt->getBranches()) {
                    if (!stmtTerminates(branch.Body)) {
                        return false;
                    }
                }
                return !ifStmt->getBranches().empty();
            }
            case ASTNode::Kind::MatchStmt: {
                auto* matchStmt = static_cast<MatchStmt*>(stmt);
                if (matchStmt->getArms().empty()) {
                    return false;
                }
                for (const auto& arm : matchStmt->getArms()) {
                    if (!stmtTerminates(arm.Body)) {
                        return false;
                    }
                }
                return true;
            }
            default:
                return false;
        }
    }

    void analyzeIfStmt(IfStmt* stmt) {
        if (!stmt) {
            return;
        }
        std::unordered_map<const Decl*, OwnershipState> entry = States;
        std::vector<std::unordered_map<const Decl*, OwnershipState>> branchStates;

        for (const auto& branch : stmt->getBranches()) {
            States = entry;
            if (branch.Condition) {
                analyzeExprRead(branch.Condition);
            }
            analyzeStmt(branch.Body);
            if (!stmtTerminates(branch.Body)) {
                branchStates.push_back(States);
            }
        }

        if (!stmt->hasElse()) {
            branchStates.push_back(entry);
        }
        if (branchStates.empty()) {
            States = entry;
            return;
        }

        States = entry;
        for (auto& item : States) {
            std::vector<OwnershipState> collected;
            collected.reserve(branchStates.size());
            for (const auto& branchMap : branchStates) {
                auto it = branchMap.find(item.first);
                collected.push_back(it == branchMap.end() ? item.second : it->second);
            }
            item.second = joinOwnershipStates(collected);
        }
    }

    void analyzeMatchStmt(MatchStmt* stmt) {
        if (!stmt) {
            return;
        }
        consumeExprValue(stmt->getScrutinee());
        std::unordered_map<const Decl*, OwnershipState> entry = States;
        std::vector<std::unordered_map<const Decl*, OwnershipState>> armStates;

        for (const auto& arm : stmt->getArms()) {
            States = entry;
            enterScope();
            analyzePatternBindings(arm.Pat);
            if (arm.Guard) {
                analyzeExprRead(arm.Guard);
            }
            analyzeStmt(arm.Body);
            exitScope();
            if (!stmtTerminates(arm.Body)) {
                armStates.push_back(States);
            }
        }

        if (armStates.empty()) {
            States = entry;
            return;
        }

        States = entry;
        for (auto& item : States) {
            std::vector<OwnershipState> collected;
            collected.reserve(armStates.size());
            for (const auto& armMap : armStates) {
                auto it = armMap.find(item.first);
                collected.push_back(it == armMap.end() ? item.second : it->second);
            }
            item.second = joinOwnershipStates(collected);
        }
    }

    void analyzeLoopBody(BlockStmt* body) {
        std::unordered_map<const Decl*, OwnershipState> entry = States;
        analyzeStmt(body);
        std::unordered_map<const Decl*, OwnershipState> bodyExit = States;
        States = entry;
        for (auto& item : States) {
            OwnershipState bodyState = item.second;
            auto it = bodyExit.find(item.first);
            if (it != bodyExit.end()) {
                bodyState = it->second;
            }
            if (item.second != bodyState) {
                item.second = OwnershipState::MaybeMoved;
            }
        }
    }

    void analyzeStmt(Stmt* stmt) {
        if (!stmt) {
            return;
        }
        switch (stmt->getKind()) {
            case ASTNode::Kind::DeclStmt: {
                auto* declStmt = static_cast<DeclStmt*>(stmt);
                Decl* decl = declStmt->getDecl();
                if (!decl) {
                    return;
                }
                if (decl->getKind() == ASTNode::Kind::VarDecl) {
                    auto* varDecl = static_cast<VarDecl*>(decl);
                    if (varDecl->getInit()) {
                        consumeExprValue(varDecl->getInit());
                    }
                    if (varDecl->getPattern()) {
                        analyzePatternBindings(varDecl->getPattern());
                    } else {
                        trackDecl(varDecl, OwnershipState::Live);
                    }
                } else if (decl->getKind() == ASTNode::Kind::ConstDecl) {
                    auto* constDecl = static_cast<ConstDecl*>(decl);
                    if (constDecl->getInit()) {
                        consumeExprValue(constDecl->getInit());
                    }
                }
                return;
            }
            case ASTNode::Kind::BlockStmt: {
                enterScope();
                auto* block = static_cast<BlockStmt*>(stmt);
                for (Stmt* inner : block->getStatements()) {
                    analyzeStmt(inner);
                }
                exitScope();
                return;
            }
            case ASTNode::Kind::ReturnStmt: {
                auto* ret = static_cast<ReturnStmt*>(stmt);
                if (ret->hasValue()) {
                    consumeExprValue(ret->getValue());
                }
                return;
            }
            case ASTNode::Kind::IfStmt:
                analyzeIfStmt(static_cast<IfStmt*>(stmt));
                return;
            case ASTNode::Kind::WhileStmt: {
                auto* whileStmt = static_cast<WhileStmt*>(stmt);
                analyzeExprRead(whileStmt->getCondition());
                analyzeLoopBody(whileStmt->getBody());
                return;
            }
            case ASTNode::Kind::LoopStmt: {
                auto* loopStmt = static_cast<LoopStmt*>(stmt);
                analyzeLoopBody(loopStmt->getBody());
                return;
            }
            case ASTNode::Kind::ForStmt: {
                auto* forStmt = static_cast<ForStmt*>(stmt);
                analyzeExprRead(forStmt->getIterable());
                std::unordered_map<const Decl*, OwnershipState> entry = States;
                enterScope();
                analyzePatternBindings(forStmt->getPattern());
                analyzeStmt(forStmt->getBody());
                exitScope();
                std::unordered_map<const Decl*, OwnershipState> bodyExit = States;
                States = entry;
                for (auto& item : States) {
                    OwnershipState bodyState = item.second;
                    auto it = bodyExit.find(item.first);
                    if (it != bodyExit.end()) {
                        bodyState = it->second;
                    }
                    if (item.second != bodyState) {
                        item.second = OwnershipState::MaybeMoved;
                    }
                }
                return;
            }
            case ASTNode::Kind::MatchStmt:
                analyzeMatchStmt(static_cast<MatchStmt*>(stmt));
                return;
            case ASTNode::Kind::DeferStmt: {
                auto* deferStmt = static_cast<DeferStmt*>(stmt);
                analyzeStmt(deferStmt->getBody());
                return;
            }
            case ASTNode::Kind::ExprStmt: {
                auto* exprStmt = static_cast<ExprStmt*>(stmt);
                analyzeExprRead(exprStmt->getExpr());
                return;
            }
            case ASTNode::Kind::BreakStmt:
            case ASTNode::Kind::ContinueStmt:
                return;
            default:
                return;
        }
    }

    void analyzeExprRead(Expr* expr) {
        if (!expr) {
            return;
        }
        switch (expr->getKind()) {
            case ASTNode::Kind::IdentifierExpr: {
                auto* ident = static_cast<IdentifierExpr*>(expr);
                if (Decl* decl = ident->getResolvedDecl()) {
                    if (isTrackedOwnershipDecl(decl)) {
                        reportInvalidUse(decl, expr);
                    }
                }
                return;
            }
            case ASTNode::Kind::MemberExpr:
                analyzeExprRead(static_cast<MemberExpr*>(expr)->getBase());
                return;
            case ASTNode::Kind::IndexExpr: {
                auto* index = static_cast<IndexExpr*>(expr);
                analyzeExprRead(index->getBase());
                analyzeExprRead(index->getIndex());
                return;
            }
            case ASTNode::Kind::SliceExpr: {
                auto* slice = static_cast<SliceExpr*>(expr);
                analyzeExprRead(slice->getBase());
                analyzeExprRead(slice->getStart());
                analyzeExprRead(slice->getEnd());
                return;
            }
            case ASTNode::Kind::UnaryExpr:
                analyzeExprRead(static_cast<UnaryExpr*>(expr)->getOperand());
                return;
            case ASTNode::Kind::BinaryExpr: {
                auto* binary = static_cast<BinaryExpr*>(expr);
                analyzeExprRead(binary->getLHS());
                analyzeExprRead(binary->getRHS());
                return;
            }
            case ASTNode::Kind::AssignExpr:
                analyzeAssignExpr(static_cast<AssignExpr*>(expr));
                return;
            case ASTNode::Kind::CallExpr:
                analyzeCallExpr(static_cast<CallExpr*>(expr));
                return;
            case ASTNode::Kind::CastExpr:
                analyzeExprRead(static_cast<CastExpr*>(expr)->getExpr());
                return;
            case ASTNode::Kind::IfExpr: {
                auto* ifExpr = static_cast<IfExpr*>(expr);
                std::unordered_map<const Decl*, OwnershipState> entry = States;
                std::vector<std::unordered_map<const Decl*, OwnershipState>> branchStates;

                for (const auto& branch : ifExpr->getBranches()) {
                    States = entry;
                    analyzeExprRead(branch.Condition);
                    analyzeExprRead(branch.Body);
                    branchStates.push_back(States);
                }

                if (!ifExpr->hasElse()) {
                    branchStates.push_back(entry);
                }

                States = entry;
                for (auto& item : States) {
                    std::vector<OwnershipState> collected;
                    collected.reserve(branchStates.size());
                    for (const auto& branchMap : branchStates) {
                        auto it = branchMap.find(item.first);
                        collected.push_back(it == branchMap.end() ? item.second : it->second);
                    }
                    item.second = joinOwnershipStates(collected);
                }
                return;
            }
            case ASTNode::Kind::MatchExpr: {
                auto* matchExpr = static_cast<MatchExpr*>(expr);
                consumeExprValue(matchExpr->getScrutinee());
                std::unordered_map<const Decl*, OwnershipState> entry = States;
                std::vector<std::unordered_map<const Decl*, OwnershipState>> armStates;
                for (const auto& arm : matchExpr->getArms()) {
                    States = entry;
                    enterScope();
                    analyzePatternBindings(arm.Pat);
                    analyzeExprRead(arm.Guard);
                    analyzeExprRead(arm.Body);
                    exitScope();
                    armStates.push_back(States);
                }
                States = entry;
                for (auto& item : States) {
                    std::vector<OwnershipState> collected;
                    collected.reserve(armStates.size());
                    for (const auto& armMap : armStates) {
                        auto it = armMap.find(item.first);
                        collected.push_back(it == armMap.end() ? item.second : it->second);
                    }
                    item.second = joinOwnershipStates(collected);
                }
                return;
            }
            case ASTNode::Kind::BlockExpr: {
                auto* blockExpr = static_cast<BlockExpr*>(expr);
                enterScope();
                for (Stmt* s : blockExpr->getStatements()) {
                    analyzeStmt(s);
                }
                if (blockExpr->hasResult()) {
                    analyzeExprRead(blockExpr->getResultExpr());
                }
                exitScope();
                return;
            }
            case ASTNode::Kind::ClosureExpr:
                return;
            case ASTNode::Kind::ArrayExpr: {
                auto* arrayExpr = static_cast<ArrayExpr*>(expr);
                for (Expr* element : arrayExpr->getElements()) {
                    consumeExprValue(element);
                }
                if (arrayExpr->isRepeat()) {
                    analyzeExprRead(arrayExpr->getRepeatCount());
                }
                return;
            }
            case ASTNode::Kind::TupleExpr: {
                auto* tupleExpr = static_cast<TupleExpr*>(expr);
                for (Expr* element : tupleExpr->getElements()) {
                    consumeExprValue(element);
                }
                return;
            }
            case ASTNode::Kind::StructExpr: {
                auto* structExpr = static_cast<StructExpr*>(expr);
                for (const auto& field : structExpr->getFields()) {
                    consumeExprValue(field.Value);
                }
                analyzeExprRead(structExpr->getBase());
                return;
            }
            case ASTNode::Kind::RangeExpr: {
                auto* rangeExpr = static_cast<RangeExpr*>(expr);
                analyzeExprRead(rangeExpr->getStart());
                analyzeExprRead(rangeExpr->getEnd());
                return;
            }
            case ASTNode::Kind::AwaitExpr:
                analyzeExprRead(static_cast<AwaitExpr*>(expr)->getInner());
                return;
            case ASTNode::Kind::ErrorPropagateExpr:
                analyzeExprRead(static_cast<ErrorPropagateExpr*>(expr)->getInner());
                return;
            case ASTNode::Kind::ErrorHandleExpr: {
                auto* errHandle = static_cast<ErrorHandleExpr*>(expr);
                analyzeExprRead(errHandle->getInner());
                enterScope();
                if (Decl* errorDecl = errHandle->getErrorVarDecl()) {
                    trackDecl(errorDecl, OwnershipState::Live);
                }
                analyzeStmt(errHandle->getHandler());
                exitScope();
                return;
            }
            case ASTNode::Kind::BuiltinCallExpr: {
                auto* builtin = static_cast<BuiltinCallExpr*>(expr);
                for (const auto& arg : builtin->getArgs()) {
                    if (!arg.isExpr()) {
                        continue;
                    }
                    consumeExprValue(arg.getExpr());
                }
                return;
            }
            default:
                return;
        }
    }
};

} // namespace

// ============================================================================
// 构造和析构
// ============================================================================

Sema::Sema(ASTContext& ctx, DiagnosticEngine& diag)
    : Ctx(ctx),
      Diag(diag),
      Symbols(ctx),
      TypeCheckerImpl(std::make_unique<TypeChecker>(Symbols, Diag, Ctx)) {
    // 符号表会自动注册内置类型

    // 初始化模块管理器
    // 通过 ASTContext 获取 SourceManager
    SourceManager& sourceMgr = ctx.getSourceManager();
    ModuleMgr = std::make_unique<ModuleManager>(sourceMgr, diag, ctx, *this);
}

Sema::~Sema() = default;

void Sema::registerBuiltinTraits(const CompilationUnit* unit) {
    auto* global = Symbols.getGlobalScope();
    std::unordered_set<std::string> userDeclaredTraits;
    if (unit) {
        for (Decl* decl : unit->getDecls()) {
            if (decl && decl->getKind() == ASTNode::Kind::TraitDecl) {
                userDeclaredTraits.insert(static_cast<TraitDecl*>(decl)->getName());
            }
        }
    }

    auto shouldSkipTrait = [&](const char* name) {
        return userDeclaredTraits.find(name) != userDeclaredTraits.end() ||
               global->lookupLocal(name) != nullptr;
    };

    enum class ReturnKind {
        Str,
        Bool,
        Self,
        Void
    };

    auto makeReturnType = [&](SourceRange range, ReturnKind kind) -> TypeNode* {
        switch (kind) {
            case ReturnKind::Str:
                return Ctx.create<BuiltinTypeNode>(range, BuiltinTypeNode::BuiltinKind::Str);
            case ReturnKind::Bool:
                return Ctx.create<BuiltinTypeNode>(range, BuiltinTypeNode::BuiltinKind::Bool);
            case ReturnKind::Self:
                return Ctx.create<IdentifierTypeNode>(range, "Self");
            case ReturnKind::Void:
                return Ctx.create<BuiltinTypeNode>(range, BuiltinTypeNode::BuiltinKind::Void);
        }
        return Ctx.create<BuiltinTypeNode>(range, BuiltinTypeNode::BuiltinKind::Void);
    };

    auto addTrait = [&](const char* name, const char* methodName,
                        ReturnKind returnKind, bool hasOtherParam) {
        if (shouldSkipTrait(name)) {
            return;
        }

        SourceRange range;
        auto* selfParam = ParamDecl::createSelf(range, ParamDecl::ParamKind::RefSelf);
        std::vector<ParamDecl*> params{selfParam};
        if (hasOtherParam) {
            auto* selfTypeNode = Ctx.create<IdentifierTypeNode>(range, "Self");
            auto* otherTypeNode = Ctx.create<ReferenceTypeNode>(range, selfTypeNode, false);
            auto* otherParam = Ctx.create<ParamDecl>(range, "other", otherTypeNode, false);
            params.push_back(otherParam);
        }

        TypeNode* retType = makeReturnType(range, returnKind);
        auto* method = Ctx.create<FuncDecl>(
            range,
            methodName,
            std::move(params),
            retType,
            nullptr,
            false,
            false,
            Visibility::Public
        );

        std::vector<FuncDecl*> methods;
        methods.push_back(method);
        std::vector<TypeAliasDecl*> assocTypes;
        auto* traitDecl = Ctx.create<TraitDecl>(
            range,
            name,
            std::move(methods),
            std::move(assocTypes),
            Visibility::Public
        );

        analyzeTraitDecl(traitDecl);
    };

    auto addMarkerTrait = [&](const char* name) {
        if (shouldSkipTrait(name)) {
            return;
        }
        SourceRange range;
        std::vector<FuncDecl*> methods;
        std::vector<TypeAliasDecl*> assocTypes;
        auto* traitDecl = Ctx.create<TraitDecl>(
            range,
            name,
            std::move(methods),
            std::move(assocTypes),
            Visibility::Public
        );
        analyzeTraitDecl(traitDecl);
    };

    auto addDropTrait = [&]() {
        if (shouldSkipTrait("Drop")) {
            return;
        }
        SourceRange range;
        auto* selfParam = ParamDecl::createSelf(range, ParamDecl::ParamKind::MutRefSelf);
        std::vector<ParamDecl*> params{selfParam};
        TypeNode* retType = makeReturnType(range, ReturnKind::Void);
        auto* method = Ctx.create<FuncDecl>(
            range,
            "drop",
            std::move(params),
            retType,
            nullptr,
            false,
            false,
            Visibility::Public
        );
        std::vector<FuncDecl*> methods;
        methods.push_back(method);
        std::vector<TypeAliasDecl*> assocTypes;
        auto* traitDecl = Ctx.create<TraitDecl>(
            range,
            "Drop",
            std::move(methods),
            std::move(assocTypes),
            Visibility::Public
        );
        analyzeTraitDecl(traitDecl);
    };

    addTrait("Display", "to_string", ReturnKind::Str, false);
    addTrait("Debug", "to_debug", ReturnKind::Str, false);
    addTrait("Error", "message", ReturnKind::Str, false);
    addTrait("Clone", "clone", ReturnKind::Self, false);
    addMarkerTrait("Copy");
    addDropTrait();

    addTrait("Add", "add", ReturnKind::Self, true);
    addTrait("Sub", "sub", ReturnKind::Self, true);
    addTrait("Mul", "mul", ReturnKind::Self, true);
    addTrait("Div", "div", ReturnKind::Self, true);
    addTrait("Mod", "mod", ReturnKind::Self, true);

    addTrait("Eq", "eq", ReturnKind::Bool, true);
    addTrait("Ne", "ne", ReturnKind::Bool, true);
    addTrait("Lt", "lt", ReturnKind::Bool, true);
    addTrait("Le", "le", ReturnKind::Bool, true);
    addTrait("Gt", "gt", ReturnKind::Bool, true);
    addTrait("Ge", "ge", ReturnKind::Bool, true);

    addTrait("Neg", "neg", ReturnKind::Self, false);
    addTrait("Not", "not", ReturnKind::Bool, false);
    addTrait("BitNot", "bit_not", ReturnKind::Self, false);

    if (Symbol* sysErrSym = Symbols.lookup("SysError")) {
        if (sysErrSym->getKind() == SymbolKind::Enum && sysErrSym->getType()) {
            ImplTraitMap[sysErrSym->getType()].insert("Error");
        }
    }
}

// ============================================================================
// 主要分析入口
// ============================================================================

bool Sema::analyze(CompilationUnit* unit) {
    if (!unit) {
        return false;
    }

    registerBuiltinTraits(unit);

    bool success = true;

    // 分析所有顶层声明
    for (Decl* decl : unit->getDecls()) {
        if (!analyzeDecl(decl)) {
            success = false;
        }
    }

    return success;
}

bool Sema::analyzeDecl(Decl* decl) {
    if (!decl) {
        return false;
    }

    switch (decl->getKind()) {
        case ASTNode::Kind::VarDecl:
            return analyzeVarDecl(static_cast<VarDecl*>(decl));
        case ASTNode::Kind::ConstDecl:
            return analyzeConstDecl(static_cast<ConstDecl*>(decl));
        case ASTNode::Kind::TypeAliasDecl:
            return analyzeTypeAliasDecl(static_cast<TypeAliasDecl*>(decl));
        case ASTNode::Kind::FuncDecl:
            return analyzeFuncDecl(static_cast<FuncDecl*>(decl));
        case ASTNode::Kind::StructDecl:
            return analyzeStructDecl(static_cast<StructDecl*>(decl));
        case ASTNode::Kind::EnumDecl:
            return analyzeEnumDecl(static_cast<EnumDecl*>(decl));
        case ASTNode::Kind::TraitDecl:
            return analyzeTraitDecl(static_cast<TraitDecl*>(decl));
        case ASTNode::Kind::ImplDecl:
            return analyzeImplDecl(static_cast<ImplDecl*>(decl));
        default:
            // 其他声明类型暂不处理
            return true;
    }
}

bool Sema::analyzeStmt(Stmt* stmt) {
    if (!stmt) {
        return false;
    }

    switch (stmt->getKind()) {
        case ASTNode::Kind::DeclStmt:
            return analyzeDecl(static_cast<DeclStmt*>(stmt)->getDecl());
        case ASTNode::Kind::BlockStmt:
            return analyzeBlockStmt(static_cast<BlockStmt*>(stmt));
        case ASTNode::Kind::ReturnStmt:
            return analyzeReturnStmt(static_cast<ReturnStmt*>(stmt));
        case ASTNode::Kind::IfStmt:
            return analyzeIfStmt(static_cast<IfStmt*>(stmt));
        case ASTNode::Kind::WhileStmt:
            return analyzeWhileStmt(static_cast<WhileStmt*>(stmt));
        case ASTNode::Kind::LoopStmt:
            return analyzeLoopStmt(static_cast<LoopStmt*>(stmt));
        case ASTNode::Kind::ForStmt:
            return analyzeForStmt(static_cast<ForStmt*>(stmt));
        case ASTNode::Kind::MatchStmt:
            return analyzeMatchStmt(static_cast<MatchStmt*>(stmt));
        case ASTNode::Kind::DeferStmt:
            return analyzeDeferStmt(static_cast<DeferStmt*>(stmt));
        case ASTNode::Kind::BreakStmt:
            return analyzeBreakStmt(static_cast<BreakStmt*>(stmt));
        case ASTNode::Kind::ContinueStmt:
            return analyzeContinueStmt(static_cast<ContinueStmt*>(stmt));
        case ASTNode::Kind::ExprStmt: {
            auto* exprStmt = static_cast<ExprStmt*>(stmt);
            Type* type = analyzeExpr(exprStmt->getExpr());
            return type != nullptr;
        }
        default:
            // 未知语句类型
            reportError(DiagID::err_unexpected_token, stmt->getBeginLoc());
            return false;
    }
}

Type* Sema::analyzeExpr(Expr* expr) {
    if (!expr) {
        return nullptr;
    }

    Type* type = nullptr;

    switch (expr->getKind()) {
        case ASTNode::Kind::IntegerLiteralExpr:
            type = analyzeIntegerLiteral(static_cast<IntegerLiteralExpr*>(expr));
            break;
        case ASTNode::Kind::FloatLiteralExpr:
            type = analyzeFloatLiteral(static_cast<FloatLiteralExpr*>(expr));
            break;
        case ASTNode::Kind::BoolLiteralExpr:
            type = analyzeBoolLiteral(static_cast<BoolLiteralExpr*>(expr));
            break;
        case ASTNode::Kind::CharLiteralExpr:
            type = analyzeCharLiteral(static_cast<CharLiteralExpr*>(expr));
            break;
        case ASTNode::Kind::StringLiteralExpr:
            type = analyzeStringLiteral(static_cast<StringLiteralExpr*>(expr));
            break;
        case ASTNode::Kind::NoneLiteralExpr:
            type = analyzeNoneLiteral(static_cast<NoneLiteralExpr*>(expr));
            break;
        case ASTNode::Kind::IdentifierExpr:
            type = analyzeIdentifier(static_cast<IdentifierExpr*>(expr));
            break;
        case ASTNode::Kind::BinaryExpr:
            type = analyzeBinaryExpr(static_cast<BinaryExpr*>(expr));
            break;
        case ASTNode::Kind::UnaryExpr:
            type = analyzeUnaryExpr(static_cast<UnaryExpr*>(expr));
            break;
        case ASTNode::Kind::AssignExpr:
            type = analyzeAssignExpr(static_cast<AssignExpr*>(expr));
            break;
        case ASTNode::Kind::CallExpr:
            type = analyzeCallExpr(static_cast<CallExpr*>(expr));
            break;
        case ASTNode::Kind::BuiltinCallExpr:
            type = analyzeBuiltinCallExpr(static_cast<BuiltinCallExpr*>(expr));
            break;
        case ASTNode::Kind::MemberExpr:
            type = analyzeMemberExpr(static_cast<MemberExpr*>(expr));
            break;
        case ASTNode::Kind::IndexExpr:
            type = analyzeIndexExpr(static_cast<IndexExpr*>(expr));
            break;
        case ASTNode::Kind::SliceExpr:
            type = analyzeSliceExpr(static_cast<SliceExpr*>(expr));
            break;
        case ASTNode::Kind::CastExpr:
            type = analyzeCastExpr(static_cast<CastExpr*>(expr));
            break;
        case ASTNode::Kind::IfExpr:
            type = analyzeIfExpr(static_cast<IfExpr*>(expr));
            break;
        case ASTNode::Kind::BlockExpr:
            type = analyzeBlockExpr(static_cast<BlockExpr*>(expr));
            break;
        case ASTNode::Kind::MatchExpr:
            type = analyzeMatchExpr(static_cast<MatchExpr*>(expr));
            break;
        case ASTNode::Kind::ClosureExpr:
            type = analyzeClosureExpr(static_cast<ClosureExpr*>(expr));
            break;
        case ASTNode::Kind::ArrayExpr:
            type = analyzeArrayExpr(static_cast<ArrayExpr*>(expr));
            break;
        case ASTNode::Kind::TupleExpr:
            type = analyzeTupleExpr(static_cast<TupleExpr*>(expr));
            break;
        case ASTNode::Kind::StructExpr:
            type = analyzeStructExpr(static_cast<StructExpr*>(expr));
            break;
        case ASTNode::Kind::RangeExpr:
            type = analyzeRangeExpr(static_cast<RangeExpr*>(expr));
            break;
        case ASTNode::Kind::AwaitExpr:
            type = analyzeAwaitExpr(static_cast<AwaitExpr*>(expr));
            break;
        case ASTNode::Kind::ErrorPropagateExpr:
            type = analyzeErrorPropagateExpr(static_cast<ErrorPropagateExpr*>(expr));
            break;
        case ASTNode::Kind::ErrorHandleExpr:
            type = analyzeErrorHandleExpr(static_cast<ErrorHandleExpr*>(expr));
            break;
        default:
            // 未知表达式类型
            reportError(DiagID::err_unexpected_token, expr->getBeginLoc());
            return nullptr;
    }

    // 设置表达式的类型
    if (type) {
        expr->setType(type);
    }

    return type;
}

Type* Sema::resolveType(TypeNode* node) {
    if (!node) {
        return nullptr;
    }

    switch (node->getKind()) {
        case ASTNode::Kind::BuiltinTypeNode:
            return resolveBuiltinType(static_cast<BuiltinTypeNode*>(node));
        case ASTNode::Kind::IdentifierTypeNode:
            return resolveIdentifierType(static_cast<IdentifierTypeNode*>(node));
        case ASTNode::Kind::ArrayTypeNode:
            return resolveArrayType(static_cast<ArrayTypeNode*>(node));
        case ASTNode::Kind::SliceTypeNode:
            return resolveSliceType(static_cast<SliceTypeNode*>(node));
        case ASTNode::Kind::TupleTypeNode:
            return resolveTupleType(static_cast<TupleTypeNode*>(node));
        case ASTNode::Kind::OptionalTypeNode:
            return resolveOptionalType(static_cast<OptionalTypeNode*>(node));
        case ASTNode::Kind::ReferenceTypeNode:
            return resolveReferenceType(static_cast<ReferenceTypeNode*>(node));
        case ASTNode::Kind::PointerTypeNode:
            return resolvePointerType(static_cast<PointerTypeNode*>(node));
        case ASTNode::Kind::FunctionTypeNode:
            return resolveFunctionType(static_cast<FunctionTypeNode*>(node));
        case ASTNode::Kind::ErrorTypeNode:
            return resolveErrorType(static_cast<ErrorTypeNode*>(node));
        case ASTNode::Kind::GenericTypeNode:
            return resolveGenericType(static_cast<GenericTypeNode*>(node));
        default:
            // 未知类型节点
            reportError(DiagID::err_unexpected_token, node->getBeginLoc());
            return nullptr;
    }
}

// ============================================================================
// 错误报告
// ============================================================================

void Sema::reportError(DiagID id, SourceLocation loc) {
    Diag.report(id, loc);
}

void Sema::reportNote(DiagID id, SourceLocation loc) {
    Diag.report(id, loc, DiagnosticLevel::Note);
}

void Sema::reportWarning(DiagID id, SourceLocation loc) {
    Diag.report(id, loc, DiagnosticLevel::Warning);
}

// ========================================================================
// 泛型参数作用域与类型替换
// ========================================================================

bool Sema::enterGenericParamScope(const std::vector<GenericParam>& params) {
    if (params.empty()) {
        return true;
    }

    Symbols.enterScope(Scope::Kind::Block);

    for (const auto& param : params) {
        std::vector<TraitType*> constraints;
        constraints.reserve(param.Bounds.size());

        for (const auto& bound : param.Bounds) {
            Symbol* boundSym = Symbols.lookup(bound);
            if (!boundSym || boundSym->getKind() != SymbolKind::Trait) {
                Diag.report(DiagID::err_expected_trait_bound, param.Loc);
                Symbols.exitScope();
                return false;
            }
            if (Type* traitType = boundSym->getType()) {
                constraints.push_back(static_cast<TraitType*>(traitType));
            }
        }

        Type* genericTy = Ctx.getGenericType(param.Name, constraints);
        auto* sym = new Symbol(SymbolKind::GenericParam, param.Name, genericTy,
                               param.Loc, Visibility::Private);
        if (!Symbols.addSymbol(sym)) {
            delete sym;
            Diag.report(DiagID::err_redefinition, param.Loc) << param.Name;
            Symbols.exitScope();
            return false;
        }
    }

    return true;
}

void Sema::exitGenericParamScope() {
    Symbols.exitScope();
}

bool Sema::buildGenericSubstitution(Type* baseType,
                                    const std::vector<Type*>& typeArgs,
                                    std::unordered_map<std::string, Type*>& mapping) {
    if (!baseType) {
        return false;
    }

    std::string baseName;
    if (baseType->isStruct()) {
        baseName = static_cast<StructType*>(baseType)->getName();
    } else if (baseType->isEnum()) {
        baseName = static_cast<EnumType*>(baseType)->getName();
    } else {
        return false;
    }

    Symbol* baseSymbol = Symbols.lookup(baseName);
    if (!baseSymbol) {
        return false;
    }

    Decl* baseDecl = baseSymbol->getDecl();
    if (!baseDecl) {
        return false;
    }

    const std::vector<GenericParam>* params = nullptr;
    switch (baseDecl->getKind()) {
        case ASTNode::Kind::StructDecl:
            params = &static_cast<StructDecl*>(baseDecl)->getGenericParams();
            break;
        case ASTNode::Kind::EnumDecl:
            params = &static_cast<EnumDecl*>(baseDecl)->getGenericParams();
            break;
        default:
            break;
    }

    if (!params) {
        return false;
    }

    if (params->size() != typeArgs.size()) {
        return false;
    }

    for (size_t i = 0; i < params->size(); ++i) {
        mapping[(*params)[i].Name] = typeArgs[i];
    }

    return true;
}

Type* Sema::substituteType(Type* type, const std::unordered_map<std::string, Type*>& mapping) {
    if (!type) {
        return nullptr;
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
            newArgs.push_back(substituteType(arg, mapping));
        }
        return Ctx.getGenericInstanceType(genInst->getBaseType(), std::move(newArgs));
    }

    if (type->isOptional()) {
        auto* opt = static_cast<OptionalType*>(type);
        return Ctx.getOptionalType(substituteType(opt->getInnerType(), mapping));
    }

    if (type->isArray()) {
        auto* arr = static_cast<ArrayType*>(type);
        return Ctx.getArrayType(substituteType(arr->getElementType(), mapping), arr->getSize());
    }

    if (type->isSlice()) {
        auto* slice = static_cast<SliceType*>(type);
        return Ctx.getSliceType(substituteType(slice->getElementType(), mapping), slice->isMutable());
    }

    if (type->isTuple()) {
        auto* tuple = static_cast<TupleType*>(type);
        std::vector<Type*> elems;
        elems.reserve(tuple->getElementCount());
        for (size_t i = 0; i < tuple->getElementCount(); ++i) {
            elems.push_back(substituteType(tuple->getElement(i), mapping));
        }
        return Ctx.getTupleType(std::move(elems));
    }

    if (type->isReference()) {
        auto* ref = static_cast<ReferenceType*>(type);
        return Ctx.getReferenceType(substituteType(ref->getPointeeType(), mapping), ref->isMutable());
    }

    if (type->isPointer()) {
        auto* ptr = static_cast<PointerType*>(type);
        return Ctx.getPointerType(substituteType(ptr->getPointeeType(), mapping), ptr->isMutable());
    }

    if (type->isVarArgs()) {
        auto* varargs = static_cast<VarArgsType*>(type);
        return Ctx.getVarArgsType(substituteType(varargs->getElementType(), mapping));
    }

    if (type->isFunction()) {
        auto* fn = static_cast<FunctionType*>(type);
        std::vector<Type*> params;
        params.reserve(fn->getParamCount());
        for (auto* p : fn->getParamTypes()) {
            params.push_back(substituteType(p, mapping));
        }
        Type* ret = substituteType(fn->getReturnType(), mapping);
        return Ctx.getFunctionType(std::move(params), ret, fn->canError(), fn->isVariadic());
    }

    if (type->isError()) {
        auto* err = static_cast<ErrorType*>(type);
        return Ctx.getErrorType(substituteType(err->getSuccessType(), mapping));
    }

    if (type->isRange()) {
        auto* range = static_cast<RangeType*>(type);
        return Ctx.getRangeType(substituteType(range->getElementType(), mapping), range->isInclusive());
    }

    return type;
}

bool Sema::unifyGenericTypes(Type* expected, Type* actual,
                             std::unordered_map<std::string, Type*>& mapping) {
    if (!expected || !actual) {
        return false;
    }

    // 泛型参数：建立或检查映射
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

    // 处理引用/指针/可选等包装类型
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

    // 泛型实例：递归统一类型参数
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

    // 其他类型要求完全一致
    return expected->isEqual(actual);
}

// ============================================================================
// 模块解析（供 ImportBuiltin 调用）
// ============================================================================

Type* Sema::resolveModuleType(const std::string& modulePath, SourceLocation loc) {
    // 获取当前文件路径（用于解析相对路径）
    std::string currentFilePath;
    SourceManager& sm = Ctx.getSourceManager();
    SourceManager::FileID fid = sm.getFileID(loc);
    if (fid != SourceManager::InvalidFileID) {
        currentFilePath = sm.getFilename(fid);
    }

    // 使用模块管理器加载模块
    ModuleInfo* moduleInfo = ModuleMgr->loadModule(modulePath, currentFilePath, ImportChain);

    if (!moduleInfo) {
        // 模块加载失败
        Diag.report(DiagID::err_module_not_found, loc)
            << modulePath;
        return nullptr;
    }

    // 检查循环导入
    if (ModuleMgr->isInImportChain(moduleInfo->Name, ImportChain)) {
        Diag.report(DiagID::err_circular_import, loc)
            << moduleInfo->Name;
        return nullptr;
    }

    // 构建模块类型，包含所有导出的成员
    std::vector<ModuleType::Member> members;

    auto appendDeclFallback = [&](Decl* decl) {
        if (!decl) return;

        std::string name;
        Type* type = nullptr;

        switch (decl->getKind()) {
            case ASTNode::Kind::VarDecl: {
                auto* varDecl = static_cast<VarDecl*>(decl);
                if (varDecl->getVisibility() != Visibility::Public) return;
                name = varDecl->getName();
                type = varDecl->getSemanticType();
                break;
            }
            case ASTNode::Kind::ConstDecl: {
                auto* constDecl = static_cast<ConstDecl*>(decl);
                if (constDecl->getVisibility() != Visibility::Public) return;
                name = constDecl->getName();
                type = constDecl->getSemanticType();
                break;
            }
            case ASTNode::Kind::FuncDecl: {
                auto* funcDecl = static_cast<FuncDecl*>(decl);
                if (funcDecl->getVisibility() != Visibility::Public) return;
                name = funcDecl->getName();
                type = funcDecl->getSemanticType();
                if (!type) {
                    std::vector<Type*> paramTypes;
                    bool isVariadic = false;
                    for (ParamDecl* param : funcDecl->getParams()) {
                        if (param->isVariadic()) {
                            isVariadic = true;
                            Type* elementType = param->getType() ? resolveType(param->getType())
                                                                 : Ctx.getValueType();
                            if (elementType) {
                                paramTypes.push_back(Ctx.getVarArgsType(elementType));
                            }
                            continue;
                        }
                        if (param->getType()) {
                            if (Type* p = resolveType(param->getType())) {
                                paramTypes.push_back(p);
                            }
                        }
                    }
                    Type* returnType = Ctx.getVoidType();
                    if (funcDecl->getReturnType()) {
                        if (Type* resolvedReturn = resolveType(funcDecl->getReturnType())) {
                            returnType = resolvedReturn;
                        }
                    }
                    type = Ctx.getFunctionType(paramTypes, returnType, false, isVariadic);
                }
                break;
            }
            case ASTNode::Kind::StructDecl: {
                auto* structDecl = static_cast<StructDecl*>(decl);
                if (structDecl->getVisibility() != Visibility::Public) return;
                name = structDecl->getName();
                type = Ctx.getStructType(name, {}, {});
                break;
            }
            case ASTNode::Kind::EnumDecl: {
                auto* enumDecl = static_cast<EnumDecl*>(decl);
                if (enumDecl->getVisibility() != Visibility::Public) return;
                name = enumDecl->getName();
                type = Ctx.getEnumType(name, {}, {});
                break;
            }
            case ASTNode::Kind::TraitDecl: {
                auto* traitDecl = static_cast<TraitDecl*>(decl);
                if (traitDecl->getVisibility() != Visibility::Public) return;
                name = traitDecl->getName();
                type = Ctx.getTraitType(name);
                break;
            }
            case ASTNode::Kind::TypeAliasDecl: {
                auto* typeAlias = static_cast<TypeAliasDecl*>(decl);
                if (typeAlias->getVisibility() != Visibility::Public) return;
                name = typeAlias->getName();
                if (TypeNode* aliased = typeAlias->getAliasedType()) {
                    type = resolveType(aliased);
                }
                break;
            }
            default:
                return;
        }

        if (!name.empty() && type) {
            members.push_back({name, type, decl, ""});
        }
    };

    if (!moduleInfo->Exports.empty()) {
        for (auto& exp : moduleInfo->Exports) {
            Type* type = exp.SemanticType;
            if (exp.ExportKind == ModuleExport::Kind::ModuleAlias && !type) {
                type = resolveModuleType(exp.ModulePath, loc);
                exp.SemanticType = type;
                if (exp.DeclNode) {
                    exp.DeclNode->setSemanticType(type);
                }
            }

            if (!exp.Name.empty() && type) {
                members.push_back({exp.Name, type, exp.DeclNode, exp.LinkName});
            }
        }
    } else {
        for (Decl* decl : moduleInfo->Declarations) {
            appendDeclFallback(decl);
        }
    }

    // 返回模块类型
    return Ctx.getModuleType(moduleInfo->Name, std::move(members));
}

EnumType* Sema::getExpectedEnumType(Type* type) {
    if (!type) {
        return nullptr;
    }

    if (type->isReference()) {
        type = static_cast<ReferenceType*>(type)->getPointeeType();
    } else if (type->isPointer()) {
        type = static_cast<PointerType*>(type)->getPointeeType();
    }

    if (type->isGenericInstance()) {
        type = static_cast<GenericInstanceType*>(type)->getBaseType();
    }

    if (type->isEnum()) {
        return static_cast<EnumType*>(type);
    }

    return nullptr;
}

Expr* Sema::applyEnumVariantSugar(Expr* expr, Type* expectedType) {
    if (!expr || !expectedType) {
        return expr;
    }

    EnumType* enumType = getExpectedEnumType(expectedType);
    if (!enumType) {
        return expr;
    }

    auto hasVariant = [&](const std::string& name) -> bool {
        return enumType->getVariant(name) != nullptr;
    };

    auto preferFunction = [&](const std::string& name, bool allowImplicitCall) -> Expr* {
        Symbol* sym = Symbols.lookup(name);
        if (!sym || sym->getKind() != SymbolKind::Function) {
            return nullptr;
        }

        Type* symType = sym->getType();
        if (!symType || !symType->isFunction()) {
            return nullptr;
        }

        auto* funcType = static_cast<FunctionType*>(symType);
        Type* returnType = funcType->getReturnType();
        if (!returnType || !returnType->isEqual(expectedType)) {
            return nullptr;
        }

        if (allowImplicitCall && funcType->getParamCount() != 0) {
            return nullptr;
        }

        Diag.report(DiagID::warn_enum_variant_function_preferred,
                    expr->getBeginLoc(),
                    expr->getRange())
            << name
            << enumType->getName();

        if (!allowImplicitCall) {
            return expr;
        }

        std::vector<CallExpr::Arg> args;
        SourceRange range(expr->getBeginLoc(), expr->getEndLoc());
        return Ctx.create<CallExpr>(range, expr, std::move(args));
    };

    if (auto* identExpr = dynamic_cast<IdentifierExpr*>(expr)) {
        const std::string& name = identExpr->getName();
        const EnumType::Variant* variant = enumType->getVariant(name);
        if (!variant) {
            return expr;
        }
        if (!variant->Data.empty()) {
            return expr;
        }

        if (Expr* funcExpr = preferFunction(name, true)) {
            return funcExpr;
        }

        SourceRange range(expr->getBeginLoc(), expr->getEndLoc());
        auto* enumIdent = Ctx.create<IdentifierExpr>(range, enumType->getName());
        return Ctx.create<MemberExpr>(range, enumIdent, name);
    }

    if (dynamic_cast<NoneLiteralExpr*>(expr)) {
        const EnumType::Variant* variant = enumType->getVariant("None");
        if (!variant || !variant->Data.empty()) {
            return expr;
        }
        SourceRange range(expr->getBeginLoc(), expr->getEndLoc());
        auto* enumIdent = Ctx.create<IdentifierExpr>(range, enumType->getName());
        return Ctx.create<MemberExpr>(range, enumIdent, "None");
    }

    if (auto* callExpr = dynamic_cast<CallExpr*>(expr)) {
        auto* calleeIdent = dynamic_cast<IdentifierExpr*>(callExpr->getCallee());
        if (!calleeIdent) {
            return expr;
        }

        const std::string& name = calleeIdent->getName();
        const EnumType::Variant* variant = enumType->getVariant(name);
        if (!variant) {
            return expr;
        }

        if (preferFunction(name, false)) {
            return expr;
        }

        SourceRange range(callExpr->getBeginLoc(), callExpr->getEndLoc());
        auto* enumIdent = Ctx.create<IdentifierExpr>(range, enumType->getName());
        auto* memberCallee = Ctx.create<MemberExpr>(range, enumIdent, name);
        std::vector<CallExpr::Arg> args = callExpr->getArgs();
        std::vector<TypeNode*> typeArgs = callExpr->getTypeArgs();
        return Ctx.create<CallExpr>(range, memberCallee, std::move(args), std::move(typeArgs));
    }

    return expr;
}

// ============================================================================
// 声明分析实现（占位符）
// ============================================================================

bool Sema::analyzeVarDecl(VarDecl* decl) {
    if (!decl) {
        return false;
    }

    Pattern* pattern = decl->getPattern();

    Type* varType = nullptr;

    // 如果有类型注解，解析类型
    if (decl->getType()) {
        varType = resolveType(decl->getType());
        if (!varType) {
            return false;
        }
    }

    // 如果有初始化表达式，分析表达式
    if (decl->getInit()) {
        if (varType) {
            Expr* coerced = applyEnumVariantSugar(decl->getInit(), varType);
            if (coerced != decl->getInit()) {
                decl->setInit(coerced);
            }
        }
        Type* initType = analyzeExpr(decl->getInit());
        if (!initType) {
            return false;
        }

        auto wrapInitWithPanicPropagate = [&]() -> bool {
            Expr* originalInit = decl->getInit();
            auto* wrapped = Ctx.create<ErrorPropagateExpr>(originalInit->getRange(), originalInit);
            decl->setInit(wrapped);
            Type* wrappedType = analyzeExpr(wrapped);
            if (!wrappedType) {
                return false;
            }
            initType = wrappedType;
            return true;
        };

        // 如果没有类型注解，使用初始化表达式的类型
        if (!varType) {
            if (initType->isError()) {
                if (!wrapInitWithPanicPropagate()) {
                    return false;
                }
            }
            varType = initType;
        } else {
            // 检查类型兼容性
            if (!checkTypeCompatible(varType, initType, decl->getInit()->getRange())) {
                if (initType->isError()) {
                    auto* errType = static_cast<ErrorType*>(initType);
                    if (checkTypeCompatible(varType, errType->getSuccessType(), decl->getInit()->getRange())) {
                        if (!wrapInitWithPanicPropagate()) {
                            return false;
                        }
                    } else {
                        return false;
                    }
                } else {
                    return false;
                }
            }
            // 显式类型注解优先：将初始化表达式类型收敛到目标类型，
            // 避免后续代码生成阶段继续看到未实化的泛型返回类型。
            decl->getInit()->setType(varType);
            initType = varType;
        }
    } else if (!varType) {
        // 既没有类型注解也没有初始化表达式
        reportError(DiagID::err_expected_type, decl->getBeginLoc());
        return false;
    }

    // 解构绑定：使用模式绑定变量
    if (pattern && pattern->getKind() != ASTNode::Kind::IdentifierPattern) {
        decl->setSemanticType(varType);
        if (!analyzePattern(pattern, varType)) {
            return false;
        }
        return true;
    }

    // 检查变量名是否已存在于当前作用域
    Symbol* existingSymbol = Symbols.getCurrentScope()->lookupLocal(decl->getName());
    if (existingSymbol) {
        Diag.report(DiagID::err_redefinition, decl->getBeginLoc(), decl->getRange())
            << decl->getName();
        Diag.report(DiagID::note_previous_definition, existingSymbol->getLocation(),
                    DiagnosticLevel::Note)
            << existingSymbol->getName().str();
        return false;
    }

    // 创建符号并添加到符号表
    auto* symbol = new Symbol(SymbolKind::Variable, decl->getName(), varType,
                              decl->getBeginLoc(), decl->getVisibility());
    symbol->setMutable(decl->isMutable());
    symbol->setDecl(decl);

    // 设置声明的语义类型
    decl->setSemanticType(varType);

    // 如果是标识符模式，关联到当前声明
    if (pattern && pattern->getKind() == ASTNode::Kind::IdentifierPattern) {
        auto* identPat = static_cast<IdentifierPattern*>(pattern);
        identPat->setDecl(decl);
    }

    if (!Symbols.addSymbol(symbol)) {
        delete symbol;
        return false;
    }

    return true;
}

bool Sema::analyzeConstDecl(ConstDecl* decl) {
    if (!decl) {
        return false;
    }

    // 检查常量名是否已存在于当前作用域
    Symbol* existingSymbol = Symbols.getCurrentScope()->lookupLocal(decl->getName());
    if (existingSymbol) {
        Diag.report(DiagID::err_redefinition, decl->getBeginLoc(), decl->getRange())
            << decl->getName();
        Diag.report(DiagID::note_previous_definition, existingSymbol->getLocation(),
                    DiagnosticLevel::Note)
            << existingSymbol->getName().str();
        return false;
    }

    // 常量必须有初始化表达式
    if (!decl->getInit()) {
        reportError(DiagID::err_expected_expression, decl->getBeginLoc());
        return false;
    }

    // 分析初始化表达式
    if (decl->getType()) {
        Type* annotated = resolveType(decl->getType());
        if (annotated) {
            Expr* coerced = applyEnumVariantSugar(decl->getInit(), annotated);
            if (coerced != decl->getInit()) {
                decl->setInit(coerced);
            }
        }
    }
    Type* initType = analyzeExpr(decl->getInit());
    if (!initType) {
        return false;
    }

    Type* constType = nullptr;

    // 如果有类型注解，解析类型并检查兼容性
    if (decl->getType()) {
        constType = resolveType(decl->getType());
        if (!constType) {
            return false;
        }

        // 检查类型兼容性
        if (!checkTypeCompatible(constType, initType, decl->getInit()->getRange())) {
            return false;
        }
    } else {
        // 使用初始化表达式的类型
        constType = initType;
    }

    // 创建符号并添加到符号表
    auto* symbol = new Symbol(SymbolKind::Constant, decl->getName(), constType,
                              decl->getBeginLoc(), decl->getVisibility());
    symbol->setMutable(false); // 常量不可变
    symbol->setDecl(decl);

    // 设置声明的语义类型
    decl->setSemanticType(constType);

    if (!Symbols.addSymbol(symbol)) {
        delete symbol;
        return false;
    }

    return true;
}

bool Sema::analyzeTypeAliasDecl(TypeAliasDecl* decl) {
    if (!decl) {
        return false;
    }

    Scope* outerScope = Symbols.getCurrentScope();

    // 检查别名名是否已存在于当前作用域
    Symbol* existingSymbol = outerScope->lookupLocal(decl->getName());
    if (existingSymbol) {
        Diag.report(DiagID::err_redefinition, decl->getBeginLoc(), decl->getRange())
            << decl->getName();
        Diag.report(DiagID::note_previous_definition, existingSymbol->getLocation(),
                    DiagnosticLevel::Note)
            << existingSymbol->getName().str();
        return false;
    }

    // 顶层 type 必须有具体别名目标（关联类型由 trait/impl 分析处理）
    if (decl->isAssociatedType() || !decl->getAliasedType()) {
        reportError(DiagID::err_expected_type, decl->getBeginLoc());
        return false;
    }

    bool hasGenericParams = decl->isGeneric();
    if (hasGenericParams && !enterGenericParamScope(decl->getGenericParams())) {
        return false;
    }

    Type* aliasedType = resolveType(decl->getAliasedType());
    if (!aliasedType) {
        if (hasGenericParams) {
            exitGenericParamScope();
        }
        return false;
    }

    Type* aliasType = Ctx.getTypeAlias(decl->getName(), aliasedType);
    decl->setSemanticType(aliasType);

    auto* symbol = new Symbol(SymbolKind::TypeAlias, decl->getName(), aliasType,
                              decl->getBeginLoc(), decl->getVisibility());
    symbol->setDecl(decl);

    if (!(hasGenericParams ? outerScope->addSymbol(symbol) : Symbols.addSymbol(symbol))) {
        delete symbol;
        if (hasGenericParams) {
            exitGenericParamScope();
        }
        return false;
    }

    if (hasGenericParams) {
        exitGenericParamScope();
    }

    return true;
}

bool Sema::analyzeFuncDecl(FuncDecl* decl) {
    if (!decl) {
        return false;
    }

    Scope* outerScope = Symbols.getCurrentScope();

    // 检查函数名是否已存在于当前作用域
    Symbol* existingSymbol = outerScope->lookupLocal(decl->getName());
    if (existingSymbol) {
        Diag.report(DiagID::err_redefinition, decl->getBeginLoc(), decl->getRange())
            << decl->getName();
        Diag.report(DiagID::note_previous_definition, existingSymbol->getLocation(),
                    DiagnosticLevel::Note)
            << existingSymbol->getName().str();
        return false;
    }

    bool hasGenericParams = decl->isGeneric();
    if (hasGenericParams) {
        if (!enterGenericParamScope(decl->getGenericParams())) {
            return false;
        }
    }

    // 解析参数类型
    std::vector<Type*> paramTypes;
    bool hasVariadicParam = false;
    bool seenDefaultParam = false;
    for (ParamDecl* param : decl->getParams()) {
        Type* paramType = nullptr;

        if (param->isSelf()) {
            // self 参数的类型需要根据上下文确定
            // 这里暂时跳过，在后续任务中实现
            paramType = Ctx.getVoidType(); // 占位符
        } else if (param->isVariadic()) {
            hasVariadicParam = true;
            // 可变参数：转换为 VarArgs<T>
            Type* elementType = nullptr;
            if (param->getType()) {
                elementType = resolveType(param->getType());
                if (!elementType) {
                    return false;
                }
            } else {
                elementType = Ctx.getValueType();
            }
            paramType = Ctx.getVarArgsType(elementType);
        } else {
            if (!param->getType()) {
                reportError(DiagID::err_expected_type, param->getBeginLoc());
                return false;
            }
            paramType = resolveType(param->getType());
            if (!paramType) {
                if (hasGenericParams) {
                    exitGenericParamScope();
                }
                return false;
            }
        }

        if (param->hasDefaultValue()) {
            seenDefaultParam = true;
            Type* defaultType = analyzeExpr(param->getDefaultValue());
            if (!defaultType) {
                return false;
            }
            if (!checkTypeCompatible(paramType, defaultType, param->getDefaultValue()->getRange())) {
                return false;
            }
        } else if (seenDefaultParam && !param->isVariadic()) {
            Diag.report(DiagID::err_unexpected_token, param->getBeginLoc(), param->getRange());
            return false;
        }

        paramTypes.push_back(paramType);
    }

    // 解析返回类型
    Type* returnType = Ctx.getVoidType(); // 默认为 void
    if (decl->getReturnType()) {
        returnType = resolveType(decl->getReturnType());
        if (!returnType) {
            if (hasGenericParams) {
                exitGenericParamScope();
            }
            return false;
        }
    }

    // 创建函数类型
    Type* funcType = Ctx.getFunctionType(std::move(paramTypes), returnType, decl->canError(), hasVariadicParam);

    // 设置函数声明的语义类型
    decl->setSemanticType(funcType);

    // 创建函数符号并添加到符号表
    auto* symbol = new Symbol(SymbolKind::Function, decl->getName(), funcType,
                              decl->getBeginLoc(), decl->getVisibility());
    symbol->setDecl(decl);

    if (!(hasGenericParams ? outerScope->addSymbol(symbol) : Symbols.addSymbol(symbol))) {
        delete symbol;
        if (hasGenericParams) {
            exitGenericParamScope();
        }
        return false;
    }

    // 如果有函数体，进入函数作用域分析函数体
    if (decl->hasBody()) {
        auto stmtAlwaysReturns = [&](auto&& self, Stmt* stmt) -> bool {
            if (!stmt) {
                return false;
            }
            switch (stmt->getKind()) {
                case ASTNode::Kind::ReturnStmt:
                    return true;
                case ASTNode::Kind::BlockStmt: {
                    auto* block = static_cast<BlockStmt*>(stmt);
                    for (Stmt* inner : block->getStatements()) {
                        if (self(self, inner)) {
                            return true;
                        }
                    }
                    return false;
                }
                case ASTNode::Kind::IfStmt: {
                    auto* ifStmt = static_cast<IfStmt*>(stmt);
                    bool hasElse = false;
                    for (const auto& branch : ifStmt->getBranches()) {
                        if (!branch.Condition) {
                            hasElse = true;
                        }
                        if (!self(self, branch.Body)) {
                            return false;
                        }
                    }
                    return hasElse;
                }
                case ASTNode::Kind::MatchStmt: {
                    auto* matchStmt = static_cast<MatchStmt*>(stmt);
                    if (!checkExhaustive(matchStmt)) {
                        return false;
                    }
                    for (const auto& arm : matchStmt->getArms()) {
                        if (!self(self, arm.Body)) {
                            return false;
                        }
                    }
                    return !matchStmt->getArms().empty();
                }
                case ASTNode::Kind::LoopStmt: {
                    auto* loopStmt = static_cast<LoopStmt*>(stmt);
                    return self(self, loopStmt->getBody());
                }
                default:
                    return false;
            }
        };

        auto hasImplicitTailReturn = [&](BlockStmt* body, Type* expectedReturnType) -> bool {
            if (!body || !expectedReturnType || expectedReturnType->isVoid()) {
                return false;
            }

            const auto& stmts = body->getStatements();
            if (stmts.empty()) {
                return false;
            }

            std::function<bool(Type*, Type*)> isCompatibleNoDiag;
            isCompatibleNoDiag = [&](Type* expected, Type* actual) -> bool {
                if (!expected || !actual) {
                    return false;
                }
                if (expected->isEqual(actual)) {
                    return true;
                }
                if (expected->isOptional() && actual->isOptional()) {
                    auto* expectedOpt = static_cast<OptionalType*>(expected);
                    auto* actualOpt = static_cast<OptionalType*>(actual);
                    if (actualOpt->getInnerType()->isVoid()) {
                        return true;
                    }
                    return isCompatibleNoDiag(expectedOpt->getInnerType(), actualOpt->getInnerType());
                }
                if (expected->isOptional()) {
                    Type* innerType = static_cast<OptionalType*>(expected)->getInnerType();
                    if (innerType->isEqual(actual)) {
                        return true;
                    }
                }
                if (expected->isReference() && actual->isReference()) {
                    auto* expectedRef = static_cast<ReferenceType*>(expected);
                    auto* actualRef = static_cast<ReferenceType*>(actual);
                    if (!expectedRef->isMutable() && actualRef->isMutable()) {
                        return expectedRef->getPointeeType()->isEqual(actualRef->getPointeeType());
                    }
                }
                return false;
            };

            auto isExprReturnCompatible = [&](Expr* expr) -> bool {
                if (!expr || !expr->getType()) {
                    return false;
                }
                return isCompatibleNoDiag(expectedReturnType, expr->getType());
            };

            Stmt* lastStmt = stmts.back();
            if (auto* exprStmt = dynamic_cast<ExprStmt*>(lastStmt)) {
                return isExprReturnCompatible(exprStmt->getExpr());
            }

            auto* matchStmt = dynamic_cast<MatchStmt*>(lastStmt);
            if (!matchStmt || matchStmt->getArms().empty()) {
                return false;
            }

            for (const auto& arm : matchStmt->getArms()) {
                auto* armExprStmt = dynamic_cast<ExprStmt*>(arm.Body);
                if (!armExprStmt) {
                    return false;
                }
                if (!isExprReturnCompatible(armExprStmt->getExpr())) {
                    return false;
                }
            }

            return true;
        };

        // 进入函数作用域
        Symbols.enterScope(Scope::Kind::Function);
        Symbols.getCurrentScope()->setCurrentFunction(decl);

        // 从函数类型获取参数类型（因为 paramTypes 已经被 move 了）
        FunctionType* funcTypePtr = static_cast<FunctionType*>(funcType);
        const std::vector<Type*>& paramTypesFromFunc = funcTypePtr->getParamTypes();

        // 将参数添加到函数作用域
        for (size_t i = 0; i < decl->getParams().size(); ++i) {
            ParamDecl* param = decl->getParams()[i];
            Type* paramType = paramTypesFromFunc[i];

            // 设置参数的语义类型
            param->setSemanticType(paramType);

            auto* paramSymbol = new Symbol(SymbolKind::Parameter, param->getName(), paramType,
                                           param->getBeginLoc(), Visibility::Private);
            paramSymbol->setMutable(param->isMutable());
            paramSymbol->setDecl(param);

            if (!Symbols.addSymbol(paramSymbol)) {
                delete paramSymbol;
                Symbols.exitScope();
                if (hasGenericParams) {
                    exitGenericParamScope();
                }
                return false;
            }
        }

        // 分析函数体
        bool bodySuccess = analyzeStmt(decl->getBody());

        if (bodySuccess && !returnType->isVoid()) {
            bool explicitReturnGuaranteed = stmtAlwaysReturns(stmtAlwaysReturns, decl->getBody());
            bool implicitTailReturn = hasImplicitTailReturn(decl->getBody(), returnType);
            if (!explicitReturnGuaranteed && !implicitTailReturn) {
                Diag.report(DiagID::err_missing_return, decl->getBeginLoc(), decl->getRange())
                    << returnType->toString();
                bodySuccess = false;
            }
        }

        // 退出函数作用域
        Symbols.exitScope();

        if (bodySuccess && !analyzeOwnership(decl)) {
            bodySuccess = false;
        }

        if (!bodySuccess) {
            if (hasGenericParams) {
                exitGenericParamScope();
            }
            return false;
        }
    }

    if (hasGenericParams) {
        exitGenericParamScope();
    }
    return true;
}

bool Sema::analyzeStructDecl(StructDecl* decl) {
    if (!decl) {
        return false;
    }

    Scope* outerScope = Symbols.getCurrentScope();

    // 检查结构体名是否已存在于当前作用域
    Symbol* existingSymbol = outerScope->lookupLocal(decl->getName());
    if (existingSymbol) {
        Diag.report(DiagID::err_redefinition, decl->getBeginLoc(), decl->getRange())
            << decl->getName();
        Diag.report(DiagID::note_previous_definition, existingSymbol->getLocation(),
                    DiagnosticLevel::Note)
            << existingSymbol->getName().str();
        return false;
    }

    bool hasGenericParams = decl->isGeneric();
    if (hasGenericParams) {
        if (!enterGenericParamScope(decl->getGenericParams())) {
            return false;
        }
    }

    // 解析字段类型
    std::vector<Type*> fieldTypes;
    std::vector<std::string> fieldNames;

    for (FieldDecl* field : decl->getFields()) {
        // 检查字段名是否重复
        for (const std::string& existingName : fieldNames) {
            if (field->getName() == existingName) {
                Diag.report(DiagID::err_redefinition, field->getBeginLoc(), field->getRange())
                    << field->getName();
                if (hasGenericParams) {
                    exitGenericParamScope();
                }
                return false;
            }
        }

        // 解析字段类型
        Type* fieldType = resolveType(field->getType());
        if (!fieldType) {
            if (hasGenericParams) {
                exitGenericParamScope();
            }
            return false;
        }

        // 检查默认值（如果有）
        if (field->hasDefaultValue()) {
            Type* defaultType = analyzeExpr(field->getDefaultValue());
            if (!defaultType) {
                if (hasGenericParams) {
                    exitGenericParamScope();
                }
                return false;
            }
            if (!checkTypeCompatible(fieldType, defaultType, field->getDefaultValue()->getRange())) {
                if (hasGenericParams) {
                    exitGenericParamScope();
                }
                return false;
            }
        }

        fieldTypes.push_back(fieldType);
        fieldNames.push_back(field->getName());
    }

    // 创建结构体类型
    Type* structType = Ctx.getStructType(decl->getName(), fieldTypes, fieldNames);
    decl->setSemanticType(structType);

    // 创建结构体符号并添加到符号表
    auto* symbol = new Symbol(SymbolKind::Struct, decl->getName(), structType,
                              decl->getBeginLoc(), decl->getVisibility());
    symbol->setDecl(decl);

    if (!(hasGenericParams ? outerScope->addSymbol(symbol) : Symbols.addSymbol(symbol))) {
        delete symbol;
        if (hasGenericParams) {
            exitGenericParamScope();
        }
        return false;
    }

    // 进入结构体作用域，添加字段符号
    Symbols.enterScope(Scope::Kind::Struct);

    for (size_t i = 0; i < decl->getFields().size(); ++i) {
        FieldDecl* field = decl->getFields()[i];
        Type* fieldType = fieldTypes[i];

        auto* fieldSymbol = new Symbol(SymbolKind::Field, field->getName(), fieldType,
                                       field->getBeginLoc(), field->getVisibility());
        fieldSymbol->setDecl(field);

        if (!Symbols.addSymbol(fieldSymbol)) {
            delete fieldSymbol;
            Symbols.exitScope();
            if (hasGenericParams) {
                exitGenericParamScope();
            }
            return false;
        }
    }

    // 退出结构体作用域
    Symbols.exitScope();

    if (hasGenericParams) {
        exitGenericParamScope();
    }
    return true;
}

bool Sema::analyzeEnumDecl(EnumDecl* decl) {
    if (!decl) {
        return false;
    }

    Scope* outerScope = Symbols.getCurrentScope();

    // 检查枚举名是否已存在于当前作用域
    Symbol* existingSymbol = outerScope->lookupLocal(decl->getName());
    if (existingSymbol) {
        Diag.report(DiagID::err_redefinition, decl->getBeginLoc(), decl->getRange())
            << decl->getName();
        Diag.report(DiagID::note_previous_definition, existingSymbol->getLocation(),
                    DiagnosticLevel::Note)
            << existingSymbol->getName().str();
        return false;
    }

    bool hasGenericParams = decl->isGeneric();
    if (hasGenericParams) {
        if (!enterGenericParamScope(decl->getGenericParams())) {
            return false;
        }
    }

    // 解析变体数据类型
    std::vector<Type*> variantDataTypes;
    std::vector<std::string> variantNames;

    for (EnumVariantDecl* variant : decl->getVariants()) {
        // 检查变体名是否重复
        for (const std::string& existingName : variantNames) {
            if (variant->getName() == existingName) {
                Diag.report(DiagID::err_redefinition, variant->getBeginLoc(), variant->getRange())
                    << variant->getName();
                if (hasGenericParams) {
                    exitGenericParamScope();
                }
                return false;
            }
        }

        Type* variantDataType = nullptr;

        if (variant->isUnit()) {
            // 单元变体没有数据
            variantDataType = nullptr;
        } else if (variant->isTuple()) {
            // 元组变体：解析元组类型
            std::vector<Type*> tupleTypes;
            for (TypeNode* typeNode : variant->getTupleTypes()) {
                Type* type = resolveType(typeNode);
                if (!type) {
                    if (hasGenericParams) {
                        exitGenericParamScope();
                    }
                    return false;
                }
                tupleTypes.push_back(type);
            }

            if (tupleTypes.size() == 1) {
                // 单个类型直接使用
                variantDataType = tupleTypes[0];
            } else {
                // 多个类型创建元组
                variantDataType = Ctx.getTupleType(std::move(tupleTypes));
            }
        } else if (variant->isStruct()) {
            // 结构体变体：解析字段类型
            std::vector<Type*> fieldTypes;
            std::vector<std::string> fieldNames;

            for (FieldDecl* field : variant->getFields()) {
                // 检查字段名是否重复
                for (const std::string& existingName : fieldNames) {
                    if (field->getName() == existingName) {
                        Diag.report(DiagID::err_redefinition, field->getBeginLoc(), field->getRange())
                            << field->getName();
                        return false;
                    }
                }

                Type* fieldType = resolveType(field->getType());
                if (!fieldType) {
                    if (hasGenericParams) {
                        exitGenericParamScope();
                    }
                    return false;
                }

                fieldTypes.push_back(fieldType);
                fieldNames.push_back(field->getName());
            }

            // 为变体创建匿名结构体类型
            std::string structName = decl->getName() + "::" + variant->getName();
            variantDataType = Ctx.getStructType(std::move(structName), std::move(fieldTypes), std::move(fieldNames));
        }

        variantDataTypes.push_back(variantDataType);
        variantNames.push_back(variant->getName());
    }

    // 创建枚举类型
    Type* enumType = Ctx.getEnumType(decl->getName(), std::move(variantDataTypes), std::move(variantNames));
    decl->setSemanticType(enumType);

    // 创建枚举符号并添加到符号表
    auto* symbol = new Symbol(SymbolKind::Enum, decl->getName(), enumType,
                              decl->getBeginLoc(), decl->getVisibility());
    symbol->setDecl(decl);

    if (!(hasGenericParams ? outerScope->addSymbol(symbol) : Symbols.addSymbol(symbol))) {
        delete symbol;
        if (hasGenericParams) {
            exitGenericParamScope();
        }
        return false;
    }

    // 进入枚举作用域，添加变体符号
    Symbols.enterScope(Scope::Kind::Enum);

    for (size_t i = 0; i < decl->getVariants().size(); ++i) {
        EnumVariantDecl* variant = decl->getVariants()[i];

        // 变体的类型是枚举类型本身
        auto* variantSymbol = new Symbol(SymbolKind::EnumVariant, variant->getName(), enumType,
                                         variant->getBeginLoc(), Visibility::Public);
        variantSymbol->setDecl(variant);

        if (!Symbols.addSymbol(variantSymbol)) {
            delete variantSymbol;
            Symbols.exitScope();
            if (hasGenericParams) {
                exitGenericParamScope();
            }
            return false;
        }

    }

    // 退出枚举作用域
    Symbols.exitScope();

    if (hasGenericParams) {
        exitGenericParamScope();
    }
    return true;
}

bool Sema::analyzeTraitDecl(TraitDecl* decl) {
    if (!decl) {
        return false;
    }

    // 检查 Trait 名是否已存在于当前作用域
    Symbol* existingSymbol = Symbols.getCurrentScope()->lookupLocal(decl->getName());
    if (existingSymbol) {
        Diag.report(DiagID::err_redefinition, decl->getBeginLoc(), decl->getRange())
            << decl->getName();
        Diag.report(DiagID::note_previous_definition, existingSymbol->getLocation(),
                    DiagnosticLevel::Note)
            << existingSymbol->getName().str();
        return false;
    }

    // 创建 Trait 类型
    Type* traitType = Ctx.getTraitType(decl->getName());

    // 创建 Trait 符号并添加到符号表
    auto* symbol = new Symbol(SymbolKind::Trait, decl->getName(), traitType,
                              decl->getBeginLoc(), decl->getVisibility());
    symbol->setDecl(decl);

    if (!Symbols.addSymbol(symbol)) {
        delete symbol;
        return false;
    }

    // 进入 Trait 作用域
    Symbols.enterScope(Scope::Kind::Trait);
    bool enteredTraitGenerics = false;
    if (decl->isGeneric()) {
        if (!enterGenericParamScope(decl->getGenericParams())) {
            Symbols.exitScope();
            return false;
        }
        enteredTraitGenerics = true;
    }

    bool success = true;

    // 在 trait 作用域中引入 Self 类型别名
    {
        auto* selfSymbol = new Symbol(SymbolKind::TypeAlias, "Self", traitType,
                                      decl->getBeginLoc(), Visibility::Private);
        if (!Symbols.addSymbol(selfSymbol)) {
            delete selfSymbol;
            success = false;
        }
    }

    // 分析关联类型
    for (TypeAliasDecl* assocType : decl->getAssociatedTypes()) {
        if (!assocType->isAssociatedType()) {
            reportError(DiagID::err_expected_type, assocType->getBeginLoc());
            success = false;
            continue;
        }

        // 关联类型在 Trait 中是声明，不需要具体类型
        auto* assocSymbol = new Symbol(SymbolKind::TypeAlias, assocType->getName(),
                                       nullptr, // 关联类型没有具体类型
                                       assocType->getBeginLoc(), assocType->getVisibility());
        assocSymbol->setDecl(assocType);

        if (!Symbols.addSymbol(assocSymbol)) {
            delete assocSymbol;
            success = false;
        }
    }

    // 分析方法声明
    for (FuncDecl* method : decl->getMethods()) {
        bool enteredMethodGenerics = false;
        if (method->isGeneric()) {
            if (!enterGenericParamScope(method->getGenericParams())) {
                success = false;
                continue;
            }
            enteredMethodGenerics = true;
        }

        // Trait 方法不能有函数体（除非是默认实现）
        if (method->hasBody()) {
            Diag.report(DiagID::err_default_trait_method_not_supported,
                        method->getBeginLoc(), method->getRange())
                << method->getName();
            success = false;
        }

        // 解析方法参数类型
        std::vector<Type*> paramTypes;
        bool methodTypeOK = true;
        for (ParamDecl* param : method->getParams()) {
            Type* paramType = nullptr;

            if (param->isSelf()) {
                // self 参数的类型是当前 Trait 类型
                switch (param->getParamKind()) {
                    case ParamDecl::ParamKind::Self:
                        paramType = traitType;
                        break;
                    case ParamDecl::ParamKind::RefSelf:
                        paramType = Ctx.getReferenceType(traitType, false);
                        break;
                    case ParamDecl::ParamKind::MutRefSelf:
                        paramType = Ctx.getReferenceType(traitType, true);
                        break;
                    default:
                        paramType = Ctx.getVoidType();
                        break;
                }
            } else {
                if (!param->getType()) {
                    reportError(DiagID::err_expected_type, param->getBeginLoc());
                    success = false;
                    methodTypeOK = false;
                    continue;
                }
                paramType = resolveType(param->getType());
                if (!paramType) {
                    success = false;
                    methodTypeOK = false;
                    continue;
                }
            }

            paramTypes.push_back(paramType);
        }

        // 解析返回类型
        Type* returnType = Ctx.getVoidType(); // 默认为 void
        if (method->getReturnType()) {
            returnType = resolveType(method->getReturnType());
            if (!returnType) {
                success = false;
                methodTypeOK = false;
            }
        }

        if (!methodTypeOK) {
            if (enteredMethodGenerics) {
                exitGenericParamScope();
            }
            continue;
        }

        // 创建方法类型
        Type* methodType = Ctx.getFunctionType(std::move(paramTypes), returnType, method->canError());

        method->setSemanticType(methodType);
        FunctionType* methodFuncType = static_cast<FunctionType*>(methodType);
        const std::vector<Type*>& paramTypesFromFunc = methodFuncType->getParamTypes();

        // 设置参数的语义类型
        for (size_t i = 0; i < method->getParams().size() && i < paramTypesFromFunc.size(); ++i) {
            method->getParams()[i]->setSemanticType(paramTypesFromFunc[i]);
        }
        // 创建方法符号并添加到 Trait 作用域
        auto* methodSymbol = new Symbol(SymbolKind::Method, method->getName(), methodType,
                                        method->getBeginLoc(), method->getVisibility());
        methodSymbol->setDecl(method);

        if (!Symbols.addSymbol(methodSymbol)) {
            delete methodSymbol;
            success = false;
        }

        if (enteredMethodGenerics) {
            exitGenericParamScope();
        }
    }

    if (enteredTraitGenerics) {
        exitGenericParamScope();
    }

    // 退出 Trait 作用域
    Symbols.exitScope();

    return success;
}

bool Sema::analyzeImplDecl(ImplDecl* decl) {
    if (!decl) {
        return false;
    }

    // 处理泛型参数（显式提供或从目标类型推导）
    std::vector<GenericParam> genericParams = decl->getGenericParams();
    if (genericParams.empty()) {
        std::set<std::string> seen;
        auto addParam = [&](const std::string& name, SourceLocation loc) {
            if (seen.insert(name).second) {
                genericParams.emplace_back(name, loc);
            }
        };

        std::function<void(TypeNode*)> collect = [&](TypeNode* node) {
            if (!node) {
                return;
            }

            switch (node->getKind()) {
                case ASTNode::Kind::IdentifierTypeNode: {
                    auto* ident = static_cast<IdentifierTypeNode*>(node);
                    const std::string& name = ident->getName();
                    Symbol* sym = Symbols.lookup(name);
                    if (!sym) {
                        addParam(name, ident->getBeginLoc());
                    }
                    break;
                }
                case ASTNode::Kind::GenericTypeNode: {
                    auto* gen = static_cast<GenericTypeNode*>(node);
                    for (TypeNode* arg : gen->getTypeArgs()) {
                        collect(arg);
                    }
                    break;
                }
                case ASTNode::Kind::ArrayTypeNode: {
                    auto* arr = static_cast<ArrayTypeNode*>(node);
                    collect(arr->getElementType());
                    break;
                }
                case ASTNode::Kind::SliceTypeNode: {
                    auto* slice = static_cast<SliceTypeNode*>(node);
                    collect(slice->getElementType());
                    break;
                }
                case ASTNode::Kind::TupleTypeNode: {
                    auto* tuple = static_cast<TupleTypeNode*>(node);
                    for (TypeNode* elem : tuple->getElements()) {
                        collect(elem);
                    }
                    break;
                }
                case ASTNode::Kind::OptionalTypeNode: {
                    auto* opt = static_cast<OptionalTypeNode*>(node);
                    collect(opt->getInnerType());
                    break;
                }
                case ASTNode::Kind::ReferenceTypeNode: {
                    auto* ref = static_cast<ReferenceTypeNode*>(node);
                    collect(ref->getPointeeType());
                    break;
                }
                case ASTNode::Kind::PointerTypeNode: {
                    auto* ptr = static_cast<PointerTypeNode*>(node);
                    collect(ptr->getPointeeType());
                    break;
                }
                case ASTNode::Kind::FunctionTypeNode: {
                    auto* fn = static_cast<FunctionTypeNode*>(node);
                    for (TypeNode* param : fn->getParamTypes()) {
                        collect(param);
                    }
                    collect(fn->getReturnType());
                    break;
                }
                case ASTNode::Kind::ErrorTypeNode: {
                    auto* err = static_cast<ErrorTypeNode*>(node);
                    collect(err->getSuccessType());
                    break;
                }
                default:
                    break;
            }
        };

        collect(decl->getTargetType());
        if (decl->isTraitImpl() && decl->getTraitRefType()) {
            collect(decl->getTraitRefType());
        }
        if (!genericParams.empty()) {
            decl->setGenericParams(genericParams);
        }
    }

    bool enteredGeneric = false;
    if (!genericParams.empty()) {
        if (!enterGenericParamScope(genericParams)) {
            return false;
        }
        enteredGeneric = true;
    }

    // 解析目标类型
    Type* targetType = resolveType(decl->getTargetType());
    if (!targetType) {
        if (enteredGeneric) {
            exitGenericParamScope();
        }
        return false;
    }
    decl->setSemanticTargetType(targetType);

    TraitDecl* traitDecl = nullptr;
    Type* traitType = nullptr;
    Type* traitPattern = nullptr;

    // 如果是 Trait 实现，查找 Trait 声明
    if (decl->isTraitImpl()) {
        Symbol* traitSymbol = Symbols.lookup(decl->getTraitName());
        if (!traitSymbol) {
            Diag.report(DiagID::err_undeclared_identifier, decl->getBeginLoc(), decl->getRange())
                << decl->getTraitName();
            if (enteredGeneric) {
                exitGenericParamScope();
            }
            return false;
        }

        if (traitSymbol->getKind() != SymbolKind::Trait) {
            reportError(DiagID::err_expected_type, decl->getBeginLoc());
            if (enteredGeneric) {
                exitGenericParamScope();
            }
            return false;
        }

        traitDecl = static_cast<TraitDecl*>(traitSymbol->getDecl());
        traitType = traitSymbol->getType();
        traitPattern = traitType;

        if (traitDecl && !traitDecl->isGeneric() && decl->hasTraitTypeArgs()) {
            Diag.report(DiagID::err_generic_param_count_mismatch, decl->getBeginLoc(), decl->getRange())
                << 0u
                << static_cast<unsigned>(decl->getTraitTypeArgs().size());
            if (enteredGeneric) {
                exitGenericParamScope();
            }
            return false;
        }

        if (traitDecl && traitDecl->isGeneric()) {
            const auto& traitParams = traitDecl->getGenericParams();
            std::vector<Type*> traitArgs;
            traitArgs.reserve(traitParams.size());

            const auto& explicitTraitArgs = decl->getTraitTypeArgs();
            if (!explicitTraitArgs.empty()) {
                if (explicitTraitArgs.size() != traitParams.size()) {
                    Diag.report(DiagID::err_generic_param_count_mismatch, decl->getBeginLoc(), decl->getRange())
                        << static_cast<unsigned>(traitParams.size())
                        << static_cast<unsigned>(explicitTraitArgs.size());
                    if (enteredGeneric) {
                        exitGenericParamScope();
                    }
                    return false;
                }
                for (size_t i = 0; i < explicitTraitArgs.size(); ++i) {
                    Type* argType = resolveType(explicitTraitArgs[i]);
                    if (!argType) {
                        if (enteredGeneric) {
                            exitGenericParamScope();
                        }
                        return false;
                    }
                    traitArgs.push_back(argType);
                }
            } else {
                Diag.report(DiagID::err_generic_param_count_mismatch, decl->getBeginLoc(), decl->getRange())
                    << static_cast<unsigned>(traitParams.size())
                    << 0u;
                if (enteredGeneric) {
                    exitGenericParamScope();
                }
                return false;
            }

            traitPattern = Ctx.getGenericInstanceType(traitType, std::move(traitArgs));
        }

        if (traitDecl && isOperatorTraitName(traitDecl->getName()) &&
            isBuiltinOperatorForbiddenTarget(targetType)) {
            Diag.report(DiagID::err_builtin_operator_overload_forbidden,
                        decl->getBeginLoc(), decl->getRange())
                << targetType->toString()
                << traitDecl->getName();
            if (enteredGeneric) {
                exitGenericParamScope();
            }
            return false;
        }

        // 记录 impl 映射，供 trait 约束检查使用
        ImplTraitMap[targetType].insert(traitDecl->getName());
        if (targetType->isGenericInstance()) {
            const Type* baseKey = static_cast<GenericInstanceType*>(targetType)->getBaseType();
            ImplTraitMap[baseKey].insert(traitDecl->getName());
        }
    }

    // 进入 Impl 作用域
    Symbols.enterScope(Scope::Kind::Impl);

    bool success = true;

    // 在 impl 作用域中引入 Self 类型别名
    {
        auto* selfSymbol = new Symbol(SymbolKind::TypeAlias, "Self", targetType,
                                      decl->getBeginLoc(), Visibility::Private);
        if (!Symbols.addSymbol(selfSymbol)) {
            delete selfSymbol;
            success = false;
        }
    }

    // 注册 impl 候选（供 trait bound 与方法选择统一使用）
    size_t implCandidateIndex = ImplCandidates.size();
    ImplCandidates.push_back({decl, traitDecl, targetType, traitPattern, genericParams});

    // 分析关联类型实现（如果是 Trait 实现）
    if (traitDecl) {
        for (TypeAliasDecl* assocTypeImpl : decl->getAssociatedTypes()) {
            // 检查关联类型是否在 Trait 中声明
            TypeAliasDecl* traitAssocType = traitDecl->findAssociatedType(assocTypeImpl->getName());
            if (!traitAssocType) {
                Diag.report(DiagID::err_undeclared_identifier, assocTypeImpl->getBeginLoc(),
                            assocTypeImpl->getRange())
                    << assocTypeImpl->getName();
                success = false;
                continue;
            }

            // 解析关联类型的具体实现
            Type* implType = resolveType(assocTypeImpl->getAliasedType());
            if (!implType) {
                success = false;
                continue;
            }

            // 创建关联类型实现符号
            auto* assocSymbol = new Symbol(SymbolKind::TypeAlias, assocTypeImpl->getName(),
                                           implType, assocTypeImpl->getBeginLoc(),
                                           assocTypeImpl->getVisibility());
            assocSymbol->setDecl(assocTypeImpl);

            if (!Symbols.addSymbol(assocSymbol)) {
                delete assocSymbol;
                success = false;
            }
        }
    }

    // 分析方法实现
    for (FuncDecl* method : decl->getMethods()) {
        bool enteredMethodGenerics = false;
        if (method->isGeneric()) {
            if (!enterGenericParamScope(method->getGenericParams())) {
                success = false;
                continue;
            }
            enteredMethodGenerics = true;
        }

        // 如果是 Trait 实现，检查方法是否在 Trait 中声明
        if (traitDecl) {
            FuncDecl* traitMethod = traitDecl->findMethod(method->getName());
            if (!traitMethod) {
                Diag.report(DiagID::err_function_not_found, method->getBeginLoc(), method->getRange())
                    << method->getName();
                success = false;
                if (enteredMethodGenerics) {
                    exitGenericParamScope();
                }
                continue;
            }
        }

        // 解析方法参数类型
        std::vector<Type*> paramTypes;
        bool methodTypeOK = true;
        for (ParamDecl* param : method->getParams()) {
            Type* paramType = nullptr;

            if (param->isSelf()) {
                // self 参数的类型是目标类型
                switch (param->getParamKind()) {
                    case ParamDecl::ParamKind::Self:
                        paramType = targetType;
                        break;
                    case ParamDecl::ParamKind::RefSelf:
                        paramType = Ctx.getReferenceType(targetType, false);
                        break;
                    case ParamDecl::ParamKind::MutRefSelf:
                        paramType = Ctx.getReferenceType(targetType, true);
                        break;
                    default:
                        paramType = Ctx.getVoidType();
                        break;
                }
            } else {
                if (!param->getType()) {
                    reportError(DiagID::err_expected_type, param->getBeginLoc());
                    success = false;
                    methodTypeOK = false;
                    continue;
                }
                paramType = resolveType(param->getType());
                if (!paramType) {
                    success = false;
                    methodTypeOK = false;
                    continue;
                }
            }

            paramTypes.push_back(paramType);
        }

        // 解析返回类型
        Type* returnType = Ctx.getVoidType(); // 默认为 void
        if (method->getReturnType()) {
            returnType = resolveType(method->getReturnType());
            if (!returnType) {
                success = false;
                methodTypeOK = false;
            }
        }

        if (!methodTypeOK) {
            if (enteredMethodGenerics) {
                exitGenericParamScope();
            }
            continue;
        }

        // 创建方法类型
        Type* methodType = Ctx.getFunctionType(std::move(paramTypes), returnType, method->canError());
        method->setSemanticType(methodType);
        FunctionType* methodFuncType = static_cast<FunctionType*>(methodType);
        const std::vector<Type*>& paramTypesFromFunc = methodFuncType->getParamTypes();

        // 设置参数的语义类型
        for (size_t i = 0; i < method->getParams().size() && i < paramTypesFromFunc.size(); ++i) {
            method->getParams()[i]->setSemanticType(paramTypesFromFunc[i]);
        }

        // 创建方法符号并添加到 Impl 作用域
        auto* methodSymbol = new Symbol(SymbolKind::Method, method->getName(), methodType,
                                        method->getBeginLoc(), method->getVisibility());
        methodSymbol->setDecl(method);

        if (!Symbols.addSymbol(methodSymbol)) {
            delete methodSymbol;
            success = false;
            if (enteredMethodGenerics) {
                exitGenericParamScope();
            }
            continue;
        }

        // 注册 impl 方法，供成员访问与调用解析
        Ctx.registerImplMethod(targetType, method);

        // 如果方法有函数体，分析函数体
        if (method->hasBody()) {
            // 进入方法作用域
            Symbols.enterScope(Scope::Kind::Function);
            Symbols.getCurrentScope()->setCurrentFunction(method);

            // 将参数添加到方法作用域
            for (size_t i = 0; i < method->getParams().size(); ++i) {
                ParamDecl* param = method->getParams()[i];
                Type* paramType = paramTypesFromFunc[i];

                auto* paramSymbol = new Symbol(SymbolKind::Parameter, param->getName(), paramType,
                                               param->getBeginLoc(), Visibility::Private);
                paramSymbol->setMutable(param->isMutable());
                paramSymbol->setDecl(param);

                if (!Symbols.addSymbol(paramSymbol)) {
                    delete paramSymbol;
                    success = false;
                }
            }

            // 分析方法体
            bool methodBodyOk = analyzeStmt(method->getBody());
            if (!methodBodyOk) {
                success = false;
            }

            // 退出方法作用域
            Symbols.exitScope();

            if (methodBodyOk && !analyzeOwnership(method)) {
                success = false;
            }
        }

        if (enteredMethodGenerics) {
            exitGenericParamScope();
        }
    }

    // 如果是 Trait 实现，检查实现的完整性
    if (traitDecl && success) {
        if (!checkTraitImpl(decl)) {
            success = false;
        }
    }

    // 记录 Display/Debug 的实现方法，供 @format 使用
    if (decl->isTraitImpl()) {
        const std::string& traitName = decl->getTraitName();
        if (traitName == "Display") {
            if (FuncDecl* method = decl->findMethod("to_string")) {
                Ctx.registerDisplayImpl(targetType, method);
            }
        } else if (traitName == "Debug") {
            if (FuncDecl* method = decl->findMethod("to_debug")) {
                Ctx.registerDebugImpl(targetType, method);
            }
        }
    }

    // 退出 Impl 作用域
    Symbols.exitScope();
    if (enteredGeneric) {
        exitGenericParamScope();
    }

    if (!success &&
        implCandidateIndex < ImplCandidates.size() &&
        ImplCandidates[implCandidateIndex].Decl == decl) {
        ImplCandidates.erase(ImplCandidates.begin() + implCandidateIndex);
    }

    return success;
}

// ============================================================================
// 语句分析实现（占位符）
// ============================================================================

bool Sema::analyzeBlockStmt(BlockStmt* stmt) {
    if (!stmt) {
        return false;
    }

    // 进入块作用域
    Symbols.enterScope(Scope::Kind::Block);

    bool success = true;

    // 分析块中的每个语句
    for (Stmt* s : stmt->getStatements()) {
        if (!analyzeStmt(s)) {
            success = false;
            // 继续分析后续语句以发现更多错误
        }
    }

    // 退出块作用域
    Symbols.exitScope();

    return success;
}

bool Sema::analyzeReturnStmt(ReturnStmt* stmt) {
    if (!stmt) {
        return false;
    }

    // 查找当前所在的函数
    Scope* funcScope = Symbols.getCurrentScope();
    while (funcScope && funcScope->getKind() != Scope::Kind::Function) {
        funcScope = funcScope->getParent();
    }

    if (!funcScope) {
        // return 语句不在函数内
        reportError(DiagID::err_return_outside_function, stmt->getBeginLoc());
        return false;
    }

    FuncDecl* currentFunc = funcScope->getCurrentFunction();
    if (!currentFunc) {
        reportError(DiagID::err_return_outside_function, stmt->getBeginLoc());
        return false;
    }

    // 获取函数的返回类型
    Type* expectedReturnType = Ctx.getVoidType();
    if (currentFunc->getReturnType()) {
        expectedReturnType = resolveType(currentFunc->getReturnType());
        if (!expectedReturnType) {
            return false;
        }
    }

    std::function<bool(Type*, Type*)> isCompatibleNoDiag;
    isCompatibleNoDiag = [&](Type* expected, Type* actual) -> bool {
        if (!expected || !actual) {
            return false;
        }
        if (expected->isEqual(actual)) {
            return true;
        }
        if (expected->isOptional() && actual->isOptional()) {
            auto* expectedOpt = static_cast<OptionalType*>(expected);
            auto* actualOpt = static_cast<OptionalType*>(actual);
            if (actualOpt->getInnerType()->isVoid()) {
                return true;
            }
            return isCompatibleNoDiag(expectedOpt->getInnerType(), actualOpt->getInnerType());
        }
        if (expected->isOptional()) {
            Type* innerType = static_cast<OptionalType*>(expected)->getInnerType();
            if (innerType->isEqual(actual)) {
                return true;
            }
        }
        if (expected->isReference() && actual->isReference()) {
            auto* expectedRef = static_cast<ReferenceType*>(expected);
            auto* actualRef = static_cast<ReferenceType*>(actual);
            if (!expectedRef->isMutable() && actualRef->isMutable()) {
                return expectedRef->getPointeeType()->isEqual(actualRef->getPointeeType());
            }
        }
        return false;
    };

    // 分析返回值（如果有）
    if (stmt->hasValue()) {
        Expr* coerced = applyEnumVariantSugar(stmt->getValue(), expectedReturnType);
        if (coerced != stmt->getValue()) {
            stmt->setValue(coerced);
        }
        Type* returnValueType = analyzeExpr(stmt->getValue());
        if (!returnValueType) {
            return false;
        }

        if (currentFunc->canError()) {
            // 允许直接返回 ErrorType（Result<T, E>）
            if (returnValueType->isError()) {
                auto* errType = static_cast<ErrorType*>(returnValueType);
                if (!errType->getSuccessType()->isEqual(expectedReturnType)) {
                    Diag.report(DiagID::err_return_type_mismatch, stmt->getBeginLoc(), stmt->getRange())
                        << expectedReturnType->toString()
                        << returnValueType->toString();
                    return false;
                }
                return true;
            }

            // 成功返回值类型兼容
            if (isCompatibleNoDiag(expectedReturnType, returnValueType)) {
                return true;
            }

            // 对于具名聚合类型（struct/enum），允许作为错误类型候选并检查 Error trait。
            // 其他类型（例如 str）按普通返回值不匹配处理。
            Type* checkType = returnValueType;
            if (checkType->isReference()) {
                checkType = static_cast<ReferenceType*>(checkType)->getPointeeType();
            } else if (checkType->isPointer()) {
                checkType = static_cast<PointerType*>(checkType)->getPointeeType();
            }
            if (checkType->isGenericInstance()) {
                checkType = static_cast<GenericInstanceType*>(checkType)->getBaseType();
            }

            if (checkType->isEnum() || checkType->isStruct()) {
                Symbol* errorTraitSym = Symbols.lookup("Error");
                TraitDecl* errorTrait = nullptr;
                if (errorTraitSym && errorTraitSym->getKind() == SymbolKind::Trait) {
                    errorTrait = static_cast<TraitDecl*>(errorTraitSym->getDecl());
                }
                if (errorTrait && checkTraitBound(checkType, errorTrait)) {
                    return true;
                }

                Diag.report(DiagID::err_error_type_not_implemented,
                            stmt->getBeginLoc(),
                            stmt->getRange())
                    << returnValueType->toString();
                return false;
            }

            Diag.report(DiagID::err_type_mismatch, stmt->getBeginLoc(), stmt->getRange())
                << expectedReturnType->toString()
                << returnValueType->toString();
            return false;
        } else {
            // 检查返回值类型与函数返回类型是否匹配
            if (!checkTypeCompatible(expectedReturnType, returnValueType, stmt->getValue()->getRange())) {
                return false;
            }
        }
    } else {
        // 没有返回值，检查函数是否需要返回值
        if (!expectedReturnType->isVoid()) {
            Diag.report(DiagID::err_return_type_mismatch, stmt->getBeginLoc(), stmt->getRange())
                << expectedReturnType->toString()
                << "void";
            return false;
        }
    }

    return true;
}

bool Sema::analyzeIfStmt(IfStmt* stmt) {
    if (!stmt) {
        return false;
    }

    bool success = true;

    // 分析每个分支
    for (const auto& branch : stmt->getBranches()) {
        // 分析条件表达式（else 分支没有条件）
        if (branch.Condition) {
            Type* condType = analyzeExpr(branch.Condition);
            if (!condType) {
                success = false;
                continue;
            }
            if (!condType->isBool()) {
                Diag.report(DiagID::err_type_mismatch, branch.Condition->getBeginLoc(),
                            branch.Condition->getRange())
                    << "bool"
                    << condType->toString();
                success = false;
            }
        }

        // 分析分支体
        if (!analyzeStmt(branch.Body)) {
            success = false;
        }
    }

    return success;
}

bool Sema::analyzeWhileStmt(WhileStmt* stmt) {
    if (!stmt) {
        return false;
    }

    bool success = true;

    // 分析条件表达式
    Type* condType = analyzeExpr(stmt->getCondition());
    if (!condType) {
        success = false;
    } else if (!condType->isBool()) {
        Diag.report(DiagID::err_type_mismatch, stmt->getCondition()->getBeginLoc(),
                    stmt->getCondition()->getRange())
            << "bool"
            << condType->toString();
        success = false;
    }

    // 进入循环作用域
    Symbols.enterScope(Scope::Kind::Loop, stmt->getLabel());

    // 分析循环体
    if (!analyzeStmt(stmt->getBody())) {
        success = false;
    }

    // 退出循环作用域
    Symbols.exitScope();

    return success;
}

bool Sema::analyzeLoopStmt(LoopStmt* stmt) {
    if (!stmt) {
        return false;
    }

    // 进入循环作用域
    Symbols.enterScope(Scope::Kind::Loop, stmt->getLabel());

    // 分析循环体
    bool success = analyzeStmt(stmt->getBody());

    // 退出循环作用域
    Symbols.exitScope();

    return success;
}

bool Sema::analyzeForStmt(ForStmt* stmt) {
    if (!stmt) {
        return false;
    }

    bool success = true;

    // 分析可迭代表达式
    Type* iterableType = analyzeExpr(stmt->getIterable());
    if (!iterableType) {
        return false;
    }

    auto unwrapRefs = [](Type* type) -> Type* {
        while (type && type->isReference()) {
            type = static_cast<ReferenceType*>(type)->getPointeeType();
        }
        return type;
    };

    TraitDecl* iteratorTraitDecl = nullptr;
    if (Symbol* iteratorTraitSym = Symbols.lookup("Iterator")) {
        if (iteratorTraitSym->getKind() == SymbolKind::Trait) {
            iteratorTraitDecl = static_cast<TraitDecl*>(iteratorTraitSym->getDecl());
        }
    }

    auto getBuiltinElementType = [&](Type* type) -> Type* {
        Type* base = unwrapRefs(type);
        if (!base) {
            return nullptr;
        }

        if (base->isRange()) {
            return static_cast<RangeType*>(base)->getElementType();
        }
        if (base->isVarArgs()) {
            return static_cast<VarArgsType*>(base)->getElementType();
        }
        if (base->isArray()) {
            return static_cast<ArrayType*>(base)->getElementType();
        }
        if (base->isSlice()) {
            return static_cast<SliceType*>(base)->getElementType();
        }
        if (base->isString()) {
            return Ctx.getCharType();
        }
        if (base->isTuple()) {
            auto* tupleType = static_cast<TupleType*>(base);
            if (tupleType->getElementCount() == 0) {
                return Ctx.getValueType();
            }
            Type* firstType = tupleType->getElement(0);
            for (size_t i = 1; i < tupleType->getElementCount(); ++i) {
                if (!tupleType->getElement(i)->isEqual(firstType)) {
                    return Ctx.getValueType();
                }
            }
            return firstType;
        }

        return nullptr;
    };

    Type* iteratorTraitMissingType = nullptr;

    auto getIteratorItemTypeFromNext = [&](Type* iteratorType) -> Type* {
        Type* iteratorBaseType = unwrapRefs(iteratorType);
        if (!iteratorBaseType) {
            return nullptr;
        }

        // 协议约束：迭代器类型必须实现 Iterator trait
        if (!iteratorTraitDecl || !checkTraitBound(iteratorBaseType, iteratorTraitDecl)) {
            iteratorTraitMissingType = iteratorBaseType;
            return nullptr;
        }

        std::unordered_map<std::string, Type*> nextMapping;
        FuncDecl* nextMethod = resolveImplMethod(iteratorBaseType, "next",
                                                 &nextMapping, nullptr, true);
        if (!nextMethod) {
            return nullptr;
        }

        Type* nextType = nextMethod->getSemanticType();
        if (nextType && !nextMapping.empty()) {
            nextType = substituteType(nextType, nextMapping);
        }
        if (!nextType || !nextType->isFunction()) {
            return nullptr;
        }

        auto* nextFuncType = static_cast<FunctionType*>(nextType);
        Type* nextReturnType = nextFuncType->getReturnType();
        if (!nextReturnType || !nextReturnType->isOptional()) {
            return nullptr;
        }

        return static_cast<OptionalType*>(nextReturnType)->getInnerType();
    };

    auto getProtocolElementType = [&](Type* type) -> Type* {
        if (Type* itemType = getIteratorItemTypeFromNext(type)) {
            return itemType;
        }

        Type* iterableBaseType = unwrapRefs(type);
        if (!iterableBaseType) {
            return nullptr;
        }

        std::unordered_map<std::string, Type*> iterMapping;
        FuncDecl* iterMethod = resolveImplMethod(iterableBaseType, "iter",
                                                 &iterMapping, nullptr, true);
        if (!iterMethod) {
            return nullptr;
        }

        Type* iterType = iterMethod->getSemanticType();
        if (iterType && !iterMapping.empty()) {
            iterType = substituteType(iterType, iterMapping);
        }
        if (!iterType || !iterType->isFunction()) {
            return nullptr;
        }

        auto* iterFuncType = static_cast<FunctionType*>(iterType);
        return getIteratorItemTypeFromNext(iterFuncType->getReturnType());
    };

    // 确定迭代元素类型：先内置容器，再迭代器协议（iter()/next()）
    Type* elementType = getBuiltinElementType(iterableType);
    if (!elementType) {
        elementType = getProtocolElementType(iterableType);
    }

    if (!elementType) {
        Diag.report(DiagID::err_type_mismatch, stmt->getIterable()->getBeginLoc(),
                    stmt->getIterable()->getRange())
            << "iterable"
            << iterableType->toString();
        return false;
    }

    // 进入循环作用域
    Symbols.enterScope(Scope::Kind::Loop, stmt->getLabel());

    // 分析模式并绑定变量
    if (!analyzePattern(stmt->getPattern(), elementType)) {
        success = false;
    }

    // 分析循环体
    if (!analyzeStmt(stmt->getBody())) {
        success = false;
    }

    // 退出循环作用域
    Symbols.exitScope();

    return success;
}

bool Sema::analyzeMatchStmt(MatchStmt* stmt) {
    if (!stmt) {
        return false;
    }

    // 分析被匹配的表达式
    Type* scrutineeType = analyzeExpr(stmt->getScrutinee());
    if (!scrutineeType) {
        return false;
    }

    const auto& arms = stmt->getArms();
    if (arms.empty()) {
        reportError(DiagID::err_unexpected_token, stmt->getBeginLoc());
        return false;
    }

    bool success = true;

    for (const auto& arm : arms) {
        // 进入分支作用域（用于模式绑定）
        Symbols.enterScope(Scope::Kind::Block);

        // 分析模式（绑定变量到当前作用域）
        if (!analyzePattern(arm.Pat, scrutineeType)) {
            success = false;
        }

        // 分析守卫条件（如果有）
        if (arm.Guard) {
            Type* guardType = analyzeExpr(arm.Guard);
            if (!guardType) {
                success = false;
            } else if (!guardType->isBool()) {
                Diag.report(DiagID::err_type_mismatch, arm.Guard->getBeginLoc(),
                            arm.Guard->getRange())
                    << "bool"
                    << guardType->toString();
                success = false;
            }
        }

        // 分析分支体
        if (!analyzeStmt(arm.Body)) {
            success = false;
        }

        // 退出分支作用域
        Symbols.exitScope();
    }

    // 检查 match 穷尽性
    if (success && !checkExhaustive(stmt)) {
        success = false;
    }

    return success;
}

bool Sema::analyzeDeferStmt(DeferStmt* stmt) {
    if (!stmt) {
        return false;
    }

    // 检查是否在函数内
    Scope* funcScope = Symbols.getCurrentScope();
    while (funcScope && funcScope->getKind() != Scope::Kind::Function) {
        funcScope = funcScope->getParent();
    }

    if (!funcScope) {
        // defer 语句不在函数内
        reportError(DiagID::err_unexpected_token, stmt->getBeginLoc());
        return false;
    }

    // 分析延迟执行的语句
    return analyzeStmt(stmt->getBody());
}

bool Sema::analyzeBreakStmt(BreakStmt* stmt) {
    if (!stmt) {
        return false;
    }

    // 检查是否在循环内
    Scope* loopScope = Symbols.getCurrentScope();
    while (loopScope && loopScope->getKind() != Scope::Kind::Loop) {
        // 如果到达函数作用域边界，说明不在循环内
        if (loopScope->getKind() == Scope::Kind::Function) {
            reportError(DiagID::err_break_outside_loop, stmt->getBeginLoc());
            return false;
        }
        loopScope = loopScope->getParent();
    }

    if (!loopScope) {
        // break 语句不在循环内
        reportError(DiagID::err_break_outside_loop, stmt->getBeginLoc());
        return false;
    }

    if (stmt->hasLabel()) {
        Scope* scope = Symbols.getCurrentScope();
        while (scope) {
            if (scope->getKind() == Scope::Kind::Function) {
                break;
            }
            if (scope->getKind() == Scope::Kind::Loop &&
                scope->hasLoopLabel() &&
                scope->getLoopLabel() == stmt->getLabel()) {
                return true;
            }
            scope = scope->getParent();
        }

        Diag.report(DiagID::err_undeclared_identifier, stmt->getBeginLoc(), stmt->getRange())
            << stmt->getLabel();
        return false;
    }

    return true;
}

bool Sema::analyzeContinueStmt(ContinueStmt* stmt) {
    if (!stmt) {
        return false;
    }

    // 检查是否在循环内
    Scope* loopScope = Symbols.getCurrentScope();
    while (loopScope && loopScope->getKind() != Scope::Kind::Loop) {
        // 如果到达函数作用域边界，说明不在循环内
        if (loopScope->getKind() == Scope::Kind::Function) {
            reportError(DiagID::err_continue_outside_loop, stmt->getBeginLoc());
            return false;
        }
        loopScope = loopScope->getParent();
    }

    if (!loopScope) {
        // continue 语句不在循环内
        reportError(DiagID::err_continue_outside_loop, stmt->getBeginLoc());
        return false;
    }

    if (stmt->hasLabel()) {
        Scope* scope = Symbols.getCurrentScope();
        while (scope) {
            if (scope->getKind() == Scope::Kind::Function) {
                break;
            }
            if (scope->getKind() == Scope::Kind::Loop &&
                scope->hasLoopLabel() &&
                scope->getLoopLabel() == stmt->getLabel()) {
                return true;
            }
            scope = scope->getParent();
        }

        Diag.report(DiagID::err_undeclared_identifier, stmt->getBeginLoc(), stmt->getRange())
            << stmt->getLabel();
        return false;
    }

    return true;
}

// ============================================================================
// 表达式分析实现（占位符）
// ============================================================================

Type* Sema::analyzeIntegerLiteral(IntegerLiteralExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // 如果指定了类型后缀，使用指定的类型
    if (expr->hasTypeSuffix()) {
        if (expr->isPointerSizedSuffix()) {
            return Ctx.getIntegerType(Ctx.getPointerBitWidth(), expr->isSigned());
        }
        return Ctx.getIntegerType(expr->getBitWidth(), expr->isSigned());
    }

    // 默认整数类型为 i32
    return Ctx.getI32Type();
}

Type* Sema::analyzeFloatLiteral(FloatLiteralExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // 如果指定了类型后缀，使用指定的类型
    if (expr->hasTypeSuffix()) {
        return Ctx.getFloatType(expr->getBitWidth());
    }

    // 默认浮点类型为 f64
    return Ctx.getF64Type();
}

Type* Sema::analyzeBoolLiteral(BoolLiteralExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    return Ctx.getBoolType();
}

Type* Sema::analyzeCharLiteral(CharLiteralExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    return Ctx.getCharType();
}

Type* Sema::analyzeStringLiteral(StringLiteralExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    return Ctx.getStrType();
}

Type* Sema::analyzeNoneLiteral(NoneLiteralExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // None 字面量的类型是 ?void，表示一个空的可选类型
    // 实际的类型会在类型推断过程中根据上下文确定
    return Ctx.getOptionalType(Ctx.getVoidType());
}

Type* Sema::analyzeIdentifier(IdentifierExpr* expr) {
    if (!expr) {
        return nullptr;
    }


    // 在符号表中查找标识符
    Symbol* symbol = Symbols.lookup(expr->getName());
    if (!symbol) {
        Diag.report(DiagID::err_undeclared_identifier, expr->getBeginLoc(), expr->getRange())
            << expr->getName();
        return nullptr;
    }


    // 设置解析后的声明
    expr->setResolvedDecl(symbol->getDecl());

    auto instantiateVisibleGenerics = [&](Type* baseType, Decl* decl) -> Type* {
        if (!baseType || !decl) {
            return baseType;
        }

        const std::vector<GenericParam>* params = nullptr;
        switch (decl->getKind()) {
            case ASTNode::Kind::StructDecl:
                params = &static_cast<StructDecl*>(decl)->getGenericParams();
                break;
            case ASTNode::Kind::EnumDecl:
                params = &static_cast<EnumDecl*>(decl)->getGenericParams();
                break;
            case ASTNode::Kind::TraitDecl:
                params = &static_cast<TraitDecl*>(decl)->getGenericParams();
                break;
            default:
                return baseType;
        }

        if (!params || params->empty()) {
            return baseType;
        }

        std::vector<Type*> typeArgs;
        typeArgs.reserve(params->size());
        for (const auto& param : *params) {
            Symbol* argSym = Symbols.lookup(param.Name);
            if (!argSym || argSym->getKind() != SymbolKind::GenericParam || !argSym->getType()) {
                return baseType;
            }
            typeArgs.push_back(argSym->getType());
        }

        return Ctx.getGenericInstanceType(baseType, std::move(typeArgs));
    };

    // 返回符号的类型
    return instantiateVisibleGenerics(symbol->getType(), symbol->getDecl());
}

Type* Sema::analyzeBinaryExpr(BinaryExpr* expr) {
    if (!expr) {
        return nullptr;
    }
    expr->clearResolvedOpMethod();

    // 分析左右操作数
    Type* lhsType = analyzeExpr(expr->getLHS());
    Type* rhsType = analyzeExpr(expr->getRHS());

    if (!lhsType || !rhsType) {
        return nullptr;
    }

    Type* lhsValueType = unwrapValueType(lhsType);
    Type* rhsValueType = unwrapValueType(rhsType);

    auto adaptUnsuffixedIntLiteral = [](Expr* operandExpr,
                                        Type*& operandType,
                                        Type* expectedType) {
        if (!operandExpr || !operandType || !expectedType) {
            return;
        }
        auto* lit = dynamic_cast<IntegerLiteralExpr*>(operandExpr);
        if (!lit || lit->hasTypeSuffix()) {
            return;
        }
        if (!operandType->isInteger() || !expectedType->isInteger()) {
            return;
        }
        operandExpr->setType(expectedType);
        operandType = expectedType;
    };
    adaptUnsuffixedIntLiteral(expr->getLHS(), lhsValueType, rhsValueType);
    adaptUnsuffixedIntLiteral(expr->getRHS(), rhsValueType, lhsValueType);
    lhsType = expr->getLHS()->getType();
    rhsType = expr->getRHS()->getType();
    lhsValueType = unwrapValueType(lhsType);
    rhsValueType = unwrapValueType(rhsType);

    BinaryExpr::Op op = expr->getOp();
    auto reportInvalidOperands = [&](BinaryExpr::Op opKind) {
        Diag.report(DiagID::err_invalid_operand_types, expr->getBeginLoc(), expr->getRange())
            << BinaryExpr::getOpSpelling(opKind)
            << lhsType->toString()
            << rhsType->toString();
    };

    auto resolveBinaryOverload = [&](const char* traitName,
                                     const char* methodName,
                                     bool expectBoolResult) -> Type* {
        if (!lhsValueType || !rhsValueType || !lhsValueType->isEqual(rhsValueType)) {
            reportInvalidOperands(op);
            return nullptr;
        }

        Symbol* traitSymbol = Symbols.lookup(traitName);
        if (!traitSymbol || traitSymbol->getKind() != SymbolKind::Trait) {
            reportError(DiagID::err_expected_trait_bound, expr->getBeginLoc());
            return nullptr;
        }

        auto* traitDecl = dynamic_cast<TraitDecl*>(traitSymbol->getDecl());
        if (!traitDecl) {
            reportError(DiagID::err_expected_trait_bound, expr->getBeginLoc());
            return nullptr;
        }

        if (!checkTraitBound(lhsValueType, traitDecl)) {
            Diag.report(DiagID::err_trait_not_implemented, expr->getBeginLoc(), expr->getRange())
                << traitName
                << lhsValueType->toString();
            return nullptr;
        }

        std::unordered_map<std::string, Type*> mapping;
        ImplDecl* matchedImpl = nullptr;
        if (!resolveImplCandidate(lhsValueType, traitDecl, mapping, &matchedImpl)) {
            matchedImpl = nullptr;
        }

        FuncDecl* methodDecl = nullptr;
        if (matchedImpl) {
            methodDecl = matchedImpl->findMethod(methodName);
        }
        if (!methodDecl) {
            methodDecl = traitDecl->findMethod(methodName);
        }
        if (!methodDecl) {
            Diag.report(DiagID::err_missing_trait_method, expr->getBeginLoc(), expr->getRange())
                << methodName;
            return nullptr;
        }

        Type* methodType = methodDecl->getSemanticType();
        if (!methodType || !methodType->isFunction()) {
            Diag.report(DiagID::err_trait_method_signature_mismatch, expr->getBeginLoc(), expr->getRange())
                << methodName;
            return nullptr;
        }

        if (!mapping.empty()) {
            methodType = substituteType(methodType, mapping);
        }

        if (!matchedImpl) {
            auto replaceTraitSelf = [&](auto&& self, Type* ty) -> Type* {
                if (!ty) {
                    return nullptr;
                }
                ty = unwrapAliases(ty);
                if (!ty) {
                    return nullptr;
                }
                if (ty->isTrait()) {
                    auto* traitTy = static_cast<TraitType*>(ty);
                    if (traitTy->getName() == traitDecl->getName()) {
                        return lhsValueType;
                    }
                    return ty;
                }
                if (ty->isReference()) {
                    auto* refTy = static_cast<ReferenceType*>(ty);
                    Type* replaced = self(self, refTy->getPointeeType());
                    return replaced ? Ctx.getReferenceType(replaced, refTy->isMutable()) : nullptr;
                }
                if (ty->isPointer()) {
                    auto* ptrTy = static_cast<PointerType*>(ty);
                    Type* replaced = self(self, ptrTy->getPointeeType());
                    return replaced ? Ctx.getPointerType(replaced, ptrTy->isMutable()) : nullptr;
                }
                if (ty->isOptional()) {
                    auto* optTy = static_cast<OptionalType*>(ty);
                    Type* replaced = self(self, optTy->getInnerType());
                    return replaced ? Ctx.getOptionalType(replaced) : nullptr;
                }
                if (ty->isArray()) {
                    auto* arrTy = static_cast<ArrayType*>(ty);
                    Type* replaced = self(self, arrTy->getElementType());
                    return replaced ? Ctx.getArrayType(replaced, arrTy->getArraySize()) : nullptr;
                }
                if (ty->isSlice()) {
                    auto* sliceTy = static_cast<SliceType*>(ty);
                    Type* replaced = self(self, sliceTy->getElementType());
                    return replaced ? Ctx.getSliceType(replaced, sliceTy->isMutable()) : nullptr;
                }
                if (ty->isTuple()) {
                    auto* tupleTy = static_cast<TupleType*>(ty);
                    std::vector<Type*> elems;
                    elems.reserve(tupleTy->getElementCount());
                    for (size_t i = 0; i < tupleTy->getElementCount(); ++i) {
                        Type* replaced = self(self, tupleTy->getElement(i));
                        if (!replaced) {
                            return nullptr;
                        }
                        elems.push_back(replaced);
                    }
                    return Ctx.getTupleType(std::move(elems));
                }
                if (ty->isFunction()) {
                    auto* fnTy = static_cast<FunctionType*>(ty);
                    std::vector<Type*> params;
                    params.reserve(fnTy->getParamCount());
                    for (Type* paramTy : fnTy->getParamTypes()) {
                        Type* replaced = self(self, paramTy);
                        if (!replaced) {
                            return nullptr;
                        }
                        params.push_back(replaced);
                    }
                    Type* retTy = self(self, fnTy->getReturnType());
                    return retTy ? Ctx.getFunctionType(std::move(params), retTy, fnTy->canError(),
                                                       fnTy->isVariadic())
                                 : nullptr;
                }
                if (ty->isError()) {
                    auto* errTy = static_cast<ErrorType*>(ty);
                    Type* replaced = self(self, errTy->getSuccessType());
                    return replaced ? Ctx.getErrorType(replaced) : nullptr;
                }
                if (ty->isRange()) {
                    auto* rangeTy = static_cast<RangeType*>(ty);
                    Type* replaced = self(self, rangeTy->getElementType());
                    return replaced ? Ctx.getRangeType(replaced, rangeTy->isInclusive()) : nullptr;
                }
                return ty;
            };
            methodType = replaceTraitSelf(replaceTraitSelf, methodType);
            if (!methodType || !methodType->isFunction()) {
                Diag.report(DiagID::err_trait_method_signature_mismatch, expr->getBeginLoc(), expr->getRange())
                    << methodName;
                return nullptr;
            }
        }

        auto* fnType = static_cast<FunctionType*>(methodType);
        if (fnType->getParamCount() != 2) {
            Diag.report(DiagID::err_trait_method_signature_mismatch, expr->getBeginLoc(), expr->getRange())
                << methodName;
            return nullptr;
        }
        if (!checkTypeCompatible(fnType->getParam(0), lhsType, expr->getLHS()->getRange())) {
            return nullptr;
        }
        if (!checkTypeCompatible(fnType->getParam(1), rhsType, expr->getRHS()->getRange())) {
            return nullptr;
        }

        Type* returnType = unwrapAliases(fnType->getReturnType());
        if (!returnType) {
            Diag.report(DiagID::err_trait_method_signature_mismatch, expr->getBeginLoc(), expr->getRange())
                << methodName;
            return nullptr;
        }
        if (expectBoolResult) {
            if (!returnType->isBool()) {
                Diag.report(DiagID::err_type_mismatch, expr->getBeginLoc(), expr->getRange())
                    << "bool"
                    << returnType->toString();
                return nullptr;
            }
            returnType = Ctx.getBoolType();
        } else if (!returnType->isEqual(lhsValueType)) {
            Diag.report(DiagID::err_type_mismatch, expr->getBeginLoc(), expr->getRange())
                << lhsValueType->toString()
                << returnType->toString();
            return nullptr;
        }

        expr->setResolvedOpMethod(methodDecl);
        return returnType;
    };

    switch (op) {
        // 算术运算符：要求两个操作数为相同的数值类型
        case BinaryExpr::Op::Add:
        case BinaryExpr::Op::Sub:
        case BinaryExpr::Op::Mul:
        case BinaryExpr::Op::Div:
        case BinaryExpr::Op::Mod: {
            if (isBuiltinArithmeticType(lhsValueType) || isBuiltinArithmeticType(rhsValueType)) {
                if (!lhsValueType->isNumeric() || !rhsValueType->isNumeric()) {
                    reportInvalidOperands(op);
                    return nullptr;
                }
                // 检查类型是否相同
                if (!lhsValueType->isEqual(rhsValueType)) {
                    reportInvalidOperands(op);
                    return nullptr;
                }
                return lhsValueType;
            }

            if (isBuiltinOperatorForbiddenTarget(lhsValueType) ||
                isBuiltinOperatorForbiddenTarget(rhsValueType) ||
                (lhsValueType && lhsValueType->isPointer()) ||
                (rhsValueType && rhsValueType->isPointer())) {
                reportInvalidOperands(op);
                return nullptr;
            }
            switch (op) {
                case BinaryExpr::Op::Add: return resolveBinaryOverload("Add", "add", false);
                case BinaryExpr::Op::Sub: return resolveBinaryOverload("Sub", "sub", false);
                case BinaryExpr::Op::Mul: return resolveBinaryOverload("Mul", "mul", false);
                case BinaryExpr::Op::Div: return resolveBinaryOverload("Div", "div", false);
                case BinaryExpr::Op::Mod: return resolveBinaryOverload("Mod", "mod", false);
                default: break;
            }
            reportInvalidOperands(op);
            return nullptr;
        }

        // 位运算符：要求两个操作数为相同的整数类型
        case BinaryExpr::Op::BitAnd:
        case BinaryExpr::Op::BitOr:
        case BinaryExpr::Op::BitXor:
        case BinaryExpr::Op::Shl:
        case BinaryExpr::Op::Shr: {
            if (!lhsValueType->isInteger() || !rhsValueType->isInteger()) {
                reportInvalidOperands(op);
                return nullptr;
            }
            // 移位操作符允许不同整数类型
            if (op == BinaryExpr::Op::Shl || op == BinaryExpr::Op::Shr) {
                return lhsValueType;
            }
            // 其他位运算要求类型相同
            if (!lhsValueType->isEqual(rhsValueType)) {
                reportInvalidOperands(op);
                return nullptr;
            }
            return lhsValueType;
        }

        // 逻辑运算符：要求两个操作数为布尔类型
        case BinaryExpr::Op::And:
        case BinaryExpr::Op::Or: {
            if (!lhsValueType->isBool() || !rhsValueType->isBool()) {
                reportInvalidOperands(op);
                return nullptr;
            }
            return Ctx.getBoolType();
        }

        // 比较运算符：要求两个操作数类型相同，返回布尔类型
        case BinaryExpr::Op::Eq:
        case BinaryExpr::Op::Ne:
        case BinaryExpr::Op::Lt:
        case BinaryExpr::Op::Le:
        case BinaryExpr::Op::Gt:
        case BinaryExpr::Op::Ge: {
            if (isBuiltinComparisonType(lhsValueType) || isBuiltinComparisonType(rhsValueType)) {
                if (!lhsValueType->isEqual(rhsValueType)) {
                    reportInvalidOperands(op);
                    return nullptr;
                }
                return Ctx.getBoolType();
            }

            if (isBuiltinOperatorForbiddenTarget(lhsValueType) ||
                isBuiltinOperatorForbiddenTarget(rhsValueType)) {
                reportInvalidOperands(op);
                return nullptr;
            }
            switch (op) {
                case BinaryExpr::Op::Eq: return resolveBinaryOverload("Eq", "eq", true);
                case BinaryExpr::Op::Ne: return resolveBinaryOverload("Ne", "ne", true);
                case BinaryExpr::Op::Lt: return resolveBinaryOverload("Lt", "lt", true);
                case BinaryExpr::Op::Le: return resolveBinaryOverload("Le", "le", true);
                case BinaryExpr::Op::Gt: return resolveBinaryOverload("Gt", "gt", true);
                case BinaryExpr::Op::Ge: return resolveBinaryOverload("Ge", "ge", true);
                default: break;
            }
            reportInvalidOperands(op);
            return nullptr;
        }

        // 范围运算符：返回 Range 类型
        case BinaryExpr::Op::Range:
        case BinaryExpr::Op::RangeInclusive: {
            if (!lhsValueType->isInteger() || !rhsValueType->isInteger()) {
                reportInvalidOperands(op);
                return nullptr;
            }
            if (!lhsValueType->isEqual(rhsValueType)) {
                reportInvalidOperands(op);
                return nullptr;
            }
            // 范围类型暂时用元组表示 (start, end)
            std::vector<Type*> rangeElements = {lhsValueType, rhsValueType};
            return Ctx.getTupleType(std::move(rangeElements));
        }

        // orelse 运算符：用于可选类型的默认值
        // 支持链式调用：a orelse b orelse c orelse 0
        // 其中 a, b, c 都是 ?i32，0 是 i32
        case BinaryExpr::Op::OrElse: {
            if (!lhsValueType->isOptional()) {
                reportInvalidOperands(op);
                return nullptr;
            }
            Type* innerType = static_cast<OptionalType*>(lhsValueType)->getInnerType();
            
            // RHS 可以是内部类型（用于链的末尾）或者另一个相同内部类型的 Optional（用于链式调用）
            bool rhsIsInnerType = innerType->isEqual(rhsValueType);
            bool rhsIsSameOptional = rhsValueType->isOptional() && 
                                     static_cast<OptionalType*>(rhsValueType)->getInnerType()->isEqual(innerType);
            
            if (!rhsIsInnerType && !rhsIsSameOptional) {
                reportInvalidOperands(op);
                return nullptr;
            }
            
            return innerType;
        }

        default:
            reportError(DiagID::err_unexpected_token, expr->getBeginLoc());
            return nullptr;
    }
}

Type* Sema::analyzeUnaryExpr(UnaryExpr* expr) {
    if (!expr) {
        return nullptr;
    }
    expr->clearResolvedOpMethod();

    // 分析操作数
    Type* operandType = analyzeExpr(expr->getOperand());
    if (!operandType) {
        return nullptr;
    }
    Type* operandValueType = unwrapValueType(operandType);

    UnaryExpr::Op op = expr->getOp();
    auto reportUnaryMismatch = [&](const char* expected) {
        Diag.report(DiagID::err_type_mismatch, expr->getBeginLoc(), expr->getRange())
            << expected
            << operandType->toString();
    };

    auto resolveUnaryOverload = [&](const char* traitName,
                                    const char* methodName,
                                    bool expectBoolResult) -> Type* {
        if (!operandValueType) {
            reportUnaryMismatch("operator operand");
            return nullptr;
        }

        Symbol* traitSymbol = Symbols.lookup(traitName);
        if (!traitSymbol || traitSymbol->getKind() != SymbolKind::Trait) {
            reportError(DiagID::err_expected_trait_bound, expr->getBeginLoc());
            return nullptr;
        }

        auto* traitDecl = dynamic_cast<TraitDecl*>(traitSymbol->getDecl());
        if (!traitDecl) {
            reportError(DiagID::err_expected_trait_bound, expr->getBeginLoc());
            return nullptr;
        }

        if (!checkTraitBound(operandValueType, traitDecl)) {
            Diag.report(DiagID::err_trait_not_implemented, expr->getBeginLoc(), expr->getRange())
                << traitName
                << operandValueType->toString();
            return nullptr;
        }

        std::unordered_map<std::string, Type*> mapping;
        ImplDecl* matchedImpl = nullptr;
        if (!resolveImplCandidate(operandValueType, traitDecl, mapping, &matchedImpl)) {
            matchedImpl = nullptr;
        }

        FuncDecl* methodDecl = nullptr;
        if (matchedImpl) {
            methodDecl = matchedImpl->findMethod(methodName);
        }
        if (!methodDecl) {
            methodDecl = traitDecl->findMethod(methodName);
        }
        if (!methodDecl) {
            Diag.report(DiagID::err_missing_trait_method, expr->getBeginLoc(), expr->getRange())
                << methodName;
            return nullptr;
        }

        Type* methodType = methodDecl->getSemanticType();
        if (!methodType || !methodType->isFunction()) {
            Diag.report(DiagID::err_trait_method_signature_mismatch, expr->getBeginLoc(), expr->getRange())
                << methodName;
            return nullptr;
        }

        if (!mapping.empty()) {
            methodType = substituteType(methodType, mapping);
        }

        if (!matchedImpl) {
            auto replaceTraitSelf = [&](auto&& self, Type* ty) -> Type* {
                if (!ty) {
                    return nullptr;
                }
                ty = unwrapAliases(ty);
                if (!ty) {
                    return nullptr;
                }
                if (ty->isTrait()) {
                    auto* traitTy = static_cast<TraitType*>(ty);
                    if (traitTy->getName() == traitDecl->getName()) {
                        return operandValueType;
                    }
                    return ty;
                }
                if (ty->isReference()) {
                    auto* refTy = static_cast<ReferenceType*>(ty);
                    Type* replaced = self(self, refTy->getPointeeType());
                    return replaced ? Ctx.getReferenceType(replaced, refTy->isMutable()) : nullptr;
                }
                if (ty->isPointer()) {
                    auto* ptrTy = static_cast<PointerType*>(ty);
                    Type* replaced = self(self, ptrTy->getPointeeType());
                    return replaced ? Ctx.getPointerType(replaced, ptrTy->isMutable()) : nullptr;
                }
                if (ty->isOptional()) {
                    auto* optTy = static_cast<OptionalType*>(ty);
                    Type* replaced = self(self, optTy->getInnerType());
                    return replaced ? Ctx.getOptionalType(replaced) : nullptr;
                }
                if (ty->isArray()) {
                    auto* arrTy = static_cast<ArrayType*>(ty);
                    Type* replaced = self(self, arrTy->getElementType());
                    return replaced ? Ctx.getArrayType(replaced, arrTy->getArraySize()) : nullptr;
                }
                if (ty->isSlice()) {
                    auto* sliceTy = static_cast<SliceType*>(ty);
                    Type* replaced = self(self, sliceTy->getElementType());
                    return replaced ? Ctx.getSliceType(replaced, sliceTy->isMutable()) : nullptr;
                }
                if (ty->isTuple()) {
                    auto* tupleTy = static_cast<TupleType*>(ty);
                    std::vector<Type*> elems;
                    elems.reserve(tupleTy->getElementCount());
                    for (size_t i = 0; i < tupleTy->getElementCount(); ++i) {
                        Type* replaced = self(self, tupleTy->getElement(i));
                        if (!replaced) {
                            return nullptr;
                        }
                        elems.push_back(replaced);
                    }
                    return Ctx.getTupleType(std::move(elems));
                }
                if (ty->isFunction()) {
                    auto* fnTy = static_cast<FunctionType*>(ty);
                    std::vector<Type*> params;
                    params.reserve(fnTy->getParamCount());
                    for (Type* paramTy : fnTy->getParamTypes()) {
                        Type* replaced = self(self, paramTy);
                        if (!replaced) {
                            return nullptr;
                        }
                        params.push_back(replaced);
                    }
                    Type* retTy = self(self, fnTy->getReturnType());
                    return retTy ? Ctx.getFunctionType(std::move(params), retTy, fnTy->canError(),
                                                       fnTy->isVariadic())
                                 : nullptr;
                }
                if (ty->isError()) {
                    auto* errTy = static_cast<ErrorType*>(ty);
                    Type* replaced = self(self, errTy->getSuccessType());
                    return replaced ? Ctx.getErrorType(replaced) : nullptr;
                }
                if (ty->isRange()) {
                    auto* rangeTy = static_cast<RangeType*>(ty);
                    Type* replaced = self(self, rangeTy->getElementType());
                    return replaced ? Ctx.getRangeType(replaced, rangeTy->isInclusive()) : nullptr;
                }
                return ty;
            };
            methodType = replaceTraitSelf(replaceTraitSelf, methodType);
            if (!methodType || !methodType->isFunction()) {
                Diag.report(DiagID::err_trait_method_signature_mismatch, expr->getBeginLoc(), expr->getRange())
                    << methodName;
                return nullptr;
            }
        }

        auto* fnType = static_cast<FunctionType*>(methodType);
        if (fnType->getParamCount() != 1) {
            Diag.report(DiagID::err_trait_method_signature_mismatch, expr->getBeginLoc(), expr->getRange())
                << methodName;
            return nullptr;
        }
        if (!checkTypeCompatible(fnType->getParam(0), operandType, expr->getOperand()->getRange())) {
            return nullptr;
        }

        Type* returnType = unwrapAliases(fnType->getReturnType());
        if (!returnType) {
            Diag.report(DiagID::err_trait_method_signature_mismatch, expr->getBeginLoc(), expr->getRange())
                << methodName;
            return nullptr;
        }
        if (expectBoolResult) {
            if (!returnType->isBool()) {
                Diag.report(DiagID::err_type_mismatch, expr->getBeginLoc(), expr->getRange())
                    << "bool"
                    << returnType->toString();
                return nullptr;
            }
            returnType = Ctx.getBoolType();
        } else if (!returnType->isEqual(operandValueType)) {
            Diag.report(DiagID::err_type_mismatch, expr->getBeginLoc(), expr->getRange())
                << operandValueType->toString()
                << returnType->toString();
            return nullptr;
        }

        expr->setResolvedOpMethod(methodDecl);
        return returnType;
    };

    switch (op) {
        // 取负运算符：要求操作数为数值类型
        case UnaryExpr::Op::Neg: {
            if (isBuiltinArithmeticType(operandValueType)) {
                if (!operandValueType->isNumeric()) {
                    reportUnaryMismatch("numeric");
                    return nullptr;
                }
                // 对于整数类型，检查是否为有符号类型
                if (operandValueType->isInteger()) {
                    auto* intType = static_cast<IntegerType*>(operandValueType);
                    if (!intType->isSigned()) {
                        reportUnaryMismatch("signed integer");
                        return nullptr;
                    }
                }
                return operandValueType;
            }
            if (isBuiltinOperatorForbiddenTarget(operandValueType)) {
                reportUnaryMismatch("numeric");
                return nullptr;
            }
            if (operandValueType && operandValueType->isPointer()) {
                reportUnaryMismatch("numeric");
                return nullptr;
            }
            return resolveUnaryOverload("Neg", "neg", false);
        }

        // 逻辑非运算符：要求操作数为布尔类型
        case UnaryExpr::Op::Not: {
            if (operandValueType->isBool()) {
                return Ctx.getBoolType();
            }
            if (isBuiltinOperatorForbiddenTarget(operandValueType)) {
                reportUnaryMismatch("bool");
                return nullptr;
            }
            if (operandValueType && operandValueType->isPointer()) {
                reportUnaryMismatch("bool");
                return nullptr;
            }
            return resolveUnaryOverload("Not", "not", true);
        }

        // 位取反运算符：要求操作数为整数类型
        case UnaryExpr::Op::BitNot: {
            if (operandValueType->isInteger()) {
                return operandValueType;
            }
            if (isBuiltinOperatorForbiddenTarget(operandValueType)) {
                reportUnaryMismatch("integer");
                return nullptr;
            }
            if (operandValueType && operandValueType->isPointer()) {
                reportUnaryMismatch("integer");
                return nullptr;
            }
            return resolveUnaryOverload("BitNot", "bit_not", false);
        }

        // 取引用运算符：返回引用类型
        case UnaryExpr::Op::Ref: {
            bool borrowable = expr->getOperand()->isLValue();
            if (!borrowable &&
                expr->getOperand()->getKind() == ASTNode::Kind::SliceExpr) {
                borrowable = true;
            }
            if (!borrowable) {
                Diag.report(DiagID::err_invalid_borrow, expr->getBeginLoc(), expr->getRange())
                    << operandType->toString();
                return nullptr;
            }
            // `&arr[1..3]` 应得到切片类型，避免形成二次引用。
            if (expr->getOperand()->getKind() == ASTNode::Kind::SliceExpr &&
                operandType->isSlice()) {
                return operandType;
            }
            return Ctx.getReferenceType(operandType, false);
        }

        // 取可变引用运算符：返回可变引用类型
        case UnaryExpr::Op::RefMut: {
            if (!expr->getOperand()->isLValue()) {
                Diag.report(DiagID::err_invalid_borrow, expr->getBeginLoc(), expr->getRange())
                    << operandType->toString();
                return nullptr;
            }
            // 检查操作数是否可变
            if (!checkMutable(expr->getOperand(), expr->getBeginLoc())) {
                return nullptr;
            }
            return Ctx.getReferenceType(operandType, true);
        }

        // 解引用运算符：要求操作数为引用或指针类型
        case UnaryExpr::Op::Deref: {
            if (operandType->isReference()) {
                return static_cast<ReferenceType*>(operandType)->getPointeeType();
            }
            if (operandType->isPointer()) {
                return static_cast<PointerType*>(operandType)->getPointeeType();
            }
            Diag.report(DiagID::err_cannot_deref_non_pointer, expr->getBeginLoc(), expr->getRange())
                << operandType->toString();
            return nullptr;
        }

        default:
            reportError(DiagID::err_unexpected_token, expr->getBeginLoc());
            return nullptr;
    }
}

Type* Sema::analyzeAssignExpr(AssignExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    if (auto* identTarget = dynamic_cast<IdentifierExpr*>(expr->getTarget())) {
        if (identTarget->getName() == "_") {
            if (expr->getOp() != AssignExpr::Op::Assign) {
                Diag.report(DiagID::err_invalid_operand_types, expr->getBeginLoc(), expr->getRange())
                    << AssignExpr::getOpSpelling(expr->getOp())
                    << "_"
                    << "discard";
                return nullptr;
            }
            Type* valueType = analyzeExpr(expr->getValue());
            return valueType;
        }
    }

    // 分析赋值目标
    Type* targetType = analyzeExpr(expr->getTarget());
    if (!targetType) {
        return nullptr;
    }

    // 检查目标是否为左值
    if (!checkAssignable(expr->getTarget(), expr->getTarget()->getBeginLoc())) {
        return nullptr;
    }

    // 检查目标是否可变
    if (!checkMutable(expr->getTarget(), expr->getTarget()->getBeginLoc())) {
        return nullptr;
    }

    // 分析赋值值
    Expr* coerced = applyEnumVariantSugar(expr->getValue(), targetType);
    if (coerced != expr->getValue()) {
        expr->setValue(coerced);
    }
    Type* valueType = analyzeExpr(expr->getValue());
    if (!valueType) {
        return nullptr;
    }

    AssignExpr::Op op = expr->getOp();
    auto reportInvalidAssignOperands = [&](AssignExpr::Op opKind) {
        Diag.report(DiagID::err_invalid_operand_types, expr->getBeginLoc(), expr->getRange())
            << AssignExpr::getOpSpelling(opKind)
            << targetType->toString()
            << valueType->toString();
    };

    // 对于复合赋值运算符，检查运算符的类型约束
    if (expr->isCompound()) {
        switch (op) {
            case AssignExpr::Op::AddAssign:
            case AssignExpr::Op::SubAssign:
            case AssignExpr::Op::MulAssign:
            case AssignExpr::Op::DivAssign:
            case AssignExpr::Op::ModAssign: {
                // 要求操作数为数值类型
                if (!targetType->isNumeric() || !valueType->isNumeric()) {
                    reportInvalidAssignOperands(op);
                    return nullptr;
                }
                if (!targetType->isEqual(valueType)) {
                    reportInvalidAssignOperands(op);
                    return nullptr;
                }
                break;
            }
            case AssignExpr::Op::BitAndAssign:
            case AssignExpr::Op::BitOrAssign:
            case AssignExpr::Op::BitXorAssign:
            case AssignExpr::Op::ShlAssign:
            case AssignExpr::Op::ShrAssign: {
                // 要求操作数为整数类型
                if (!targetType->isInteger() || !valueType->isInteger()) {
                    reportInvalidAssignOperands(op);
                    return nullptr;
                }
                // 移位运算允许不同整数类型
                if (op != AssignExpr::Op::ShlAssign && op != AssignExpr::Op::ShrAssign) {
                    if (!targetType->isEqual(valueType)) {
                        reportInvalidAssignOperands(op);
                        return nullptr;
                    }
                }
                break;
            }
            default:
                break;
        }
    } else {
        // 简单赋值：检查类型兼容性
        if (!checkTypeCompatible(targetType, valueType, expr->getValue()->getRange())) {
            return nullptr;
        }
    }

    // 赋值表达式的类型是目标类型
    return targetType;
}

Type* Sema::analyzeCallExpr(CallExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    auto& callArgs = expr->getArgsMutable();
    std::vector<Expr*> plainArgs;
    plainArgs.reserve(callArgs.size());
    bool hasSpreadArg = false;
    size_t spreadCount = 0;
    for (const auto& arg : callArgs) {
        if (arg.IsSpread) {
            hasSpreadArg = true;
            ++spreadCount;
        }
        plainArgs.push_back(arg.Value);
    }
    if (spreadCount > 1) {
        Diag.report(DiagID::err_unexpected_token, expr->getBeginLoc(), expr->getRange());
        return nullptr;
    }

    MemberExpr* memberCallee = dynamic_cast<MemberExpr*>(expr->getCallee());
    bool baseIsType = false;
    FuncDecl* methodDecl = nullptr;
    FuncDecl* calleeDecl = nullptr;
    if (memberCallee) {
        if (Decl* resolved = memberCallee->getResolvedDecl()) {
            if (resolved->getKind() == ASTNode::Kind::FuncDecl) {
                methodDecl = static_cast<FuncDecl*>(resolved);
                calleeDecl = methodDecl;
            }
        }
        if (auto* identBase = dynamic_cast<IdentifierExpr*>(memberCallee->getBase())) {
            if (Decl* baseDecl = identBase->getResolvedDecl()) {
                if (baseDecl->getKind() == ASTNode::Kind::StructDecl ||
                    baseDecl->getKind() == ASTNode::Kind::EnumDecl ||
                    baseDecl->getKind() == ASTNode::Kind::TraitDecl ||
                    baseDecl->getKind() == ASTNode::Kind::TypeAliasDecl) {
                    baseIsType = true;
                }
            } else {
                if (Symbol* baseSym = Symbols.lookup(identBase->getName())) {
                    if (baseSym->getKind() == SymbolKind::Struct ||
                        baseSym->getKind() == SymbolKind::Enum ||
                        baseSym->getKind() == SymbolKind::Trait ||
                        baseSym->getKind() == SymbolKind::TypeAlias) {
                        baseIsType = true;
                        identBase->setResolvedDecl(baseSym->getDecl());
                    }
                }
            }
        }
    } else if (auto* identCallee = dynamic_cast<IdentifierExpr*>(expr->getCallee())) {
        if (Decl* resolved = identCallee->getResolvedDecl()) {
            if (resolved->getKind() == ASTNode::Kind::FuncDecl) {
                calleeDecl = static_cast<FuncDecl*>(resolved);
            }
        }
    }

    // 分析被调用的表达式
    Type* calleeType = analyzeExpr(expr->getCallee());
    if (!calleeType) {
        return nullptr;
    }

    // 重新获取解析后的声明（analyzeExpr 可能刚解析出符号）
    if (!calleeDecl) {
        if (auto* identCallee = dynamic_cast<IdentifierExpr*>(expr->getCallee())) {
            if (Decl* resolved = identCallee->getResolvedDecl()) {
                if (resolved->getKind() == ASTNode::Kind::FuncDecl) {
                    calleeDecl = static_cast<FuncDecl*>(resolved);
                }
            }
        } else if (memberCallee) {
            if (Decl* resolved = memberCallee->getResolvedDecl()) {
                if (resolved->getKind() == ASTNode::Kind::FuncDecl) {
                    methodDecl = static_cast<FuncDecl*>(resolved);
                    calleeDecl = methodDecl;
                }
            }
        }
    }

    auto analyzeEnumVariantCall = [&](EnumType* enumType, const std::string& variantName) -> Type* {
        if (!enumType) {
            return nullptr;
        }

        const EnumType::Variant* variant = enumType->getVariant(variantName);
        if (!variant) {
            Diag.report(DiagID::err_undeclared_identifier, expr->getBeginLoc(), expr->getRange())
                << variantName;
            return nullptr;
        }

        // 查找枚举声明用于泛型推导
        EnumDecl* enumDecl = nullptr;
        if (Symbol* enumSym = Symbols.lookup(enumType->getName())) {
            if (Decl* decl = enumSym->getDecl()) {
                if (decl->getKind() == ASTNode::Kind::EnumDecl) {
                    enumDecl = static_cast<EnumDecl*>(decl);
                }
            }
        }

        std::unordered_map<std::string, Type*> mapping;

        auto matchType = [&](Type* expectedType, Type* actualType, SourceRange range) -> bool {
            if (!unifyGenericTypes(expectedType, actualType, mapping)) {
                return checkTypeCompatible(expectedType, actualType, range);
            }
            return true;
        };

        if (hasSpreadArg) {
            Diag.report(DiagID::err_unexpected_token, expr->getBeginLoc(), expr->getRange());
            return nullptr;
        }
        size_t argCount = plainArgs.size();
        if (variant->Data.empty()) {
            if (argCount != 0) {
                Diag.report(DiagID::err_wrong_argument_count, expr->getBeginLoc(), expr->getRange())
                    << 0u
                    << static_cast<unsigned>(argCount);
                return nullptr;
            }
        } else if (variant->Data.size() == 1) {
            Type* payloadType = variant->Data[0];
            if (payloadType->isTuple()) {
                auto* tupleType = static_cast<TupleType*>(payloadType);
                if (argCount != tupleType->getElementCount() && argCount != 1) {
                    Diag.report(DiagID::err_wrong_argument_count, expr->getBeginLoc(), expr->getRange())
                        << static_cast<unsigned>(tupleType->getElementCount())
                        << static_cast<unsigned>(argCount);
                    return nullptr;
                }
                if (argCount == 1) {
                    Type* argType = analyzeExpr(plainArgs[0]);
                    if (!argType || !matchType(payloadType, argType, plainArgs[0]->getRange())) {
                        return nullptr;
                    }
                } else {
                    for (size_t i = 0; i < argCount; ++i) {
                        Type* argType = analyzeExpr(plainArgs[i]);
                        if (!argType ||
                            !matchType(tupleType->getElement(i), argType, plainArgs[i]->getRange())) {
                            return nullptr;
                        }
                    }
                }
            } else {
                if (argCount != 1) {
                    Diag.report(DiagID::err_wrong_argument_count, expr->getBeginLoc(), expr->getRange())
                        << 1u
                        << static_cast<unsigned>(argCount);
                    return nullptr;
                }
                Type* argType = analyzeExpr(plainArgs[0]);
                if (!argType || !matchType(payloadType, argType, plainArgs[0]->getRange())) {
                    return nullptr;
                }
            }
        } else {
            if (argCount != variant->Data.size()) {
                Diag.report(DiagID::err_wrong_argument_count, expr->getBeginLoc(), expr->getRange())
                    << static_cast<unsigned>(variant->Data.size())
                    << static_cast<unsigned>(argCount);
                return nullptr;
            }
            for (size_t i = 0; i < argCount; ++i) {
                Type* argType = analyzeExpr(plainArgs[i]);
                if (!argType || !matchType(variant->Data[i], argType, plainArgs[i]->getRange())) {
                    return nullptr;
                }
            }
        }

        // 如果是泛型枚举，构造实例类型
        if (enumDecl && enumDecl->isGeneric()) {
            const auto& params = enumDecl->getGenericParams();
            std::vector<Type*> typeArgs;
            typeArgs.reserve(params.size());
            for (const auto& param : params) {
                auto it = mapping.find(param.Name);
                if (it == mapping.end()) {
                    Diag.report(DiagID::err_expected_type, expr->getBeginLoc());
                    return nullptr;
                }
                typeArgs.push_back(it->second);
            }
            return Ctx.getGenericInstanceType(enumType, std::move(typeArgs));
        }

        return enumType;
    };

    // 在调用语境下，若成员解析到了同名字段但存在同名方法，优先方法。
    if (memberCallee && !calleeType->isFunction()) {
        Type* callBaseType = analyzeExpr(memberCallee->getBase());
        if (callBaseType) {
            auto unwrapAliases = [](Type* type) -> Type* {
                Type* cur = type;
                while (cur && cur->isTypeAlias()) {
                    cur = static_cast<TypeAlias*>(cur)->getAliasedType();
                }
                return cur;
            };

            callBaseType = unwrapAliases(callBaseType);
            if (callBaseType && callBaseType->isReference()) {
                callBaseType = unwrapAliases(static_cast<ReferenceType*>(callBaseType)->getPointeeType());
            }
            if (callBaseType && callBaseType->isPointer()) {
                callBaseType = unwrapAliases(static_cast<PointerType*>(callBaseType)->getPointeeType());
            }

            if (callBaseType) {
                std::unordered_map<std::string, Type*> methodMapping;
                if (FuncDecl* forcedMethod = resolveImplMethod(callBaseType, memberCallee->getMember(),
                                                               &methodMapping, nullptr, true)) {
                    memberCallee->setResolvedDecl(forcedMethod);
                    methodDecl = forcedMethod;
                    calleeDecl = forcedMethod;
                    Type* forcedType = forcedMethod->getSemanticType();
                    if (forcedType && !methodMapping.empty()) {
                        forcedType = substituteType(forcedType, methodMapping);
                    }
                    calleeType = forcedType;
                }
            }
        }
    }

    // 枚举变体构造调用：Enum.Variant(...)
    if (!calleeType->isFunction()) {
        if (memberCallee && baseIsType && calleeType->isEnum()) {
            return analyzeEnumVariantCall(static_cast<EnumType*>(calleeType),
                                          memberCallee->getMember());
        }

        if (auto* identCallee = dynamic_cast<IdentifierExpr*>(expr->getCallee())) {
            if (Decl* resolved = identCallee->getResolvedDecl()) {
                if (resolved->getKind() == ASTNode::Kind::EnumVariantDecl && calleeType->isEnum()) {
                    return analyzeEnumVariantCall(static_cast<EnumType*>(calleeType),
                                                  identCallee->getName());
                }
            }
        }

        Diag.report(DiagID::err_type_mismatch, expr->getBeginLoc(), expr->getRange())
            << "function"
            << calleeType->toString();
        return nullptr;
    }

    auto* funcType = static_cast<FunctionType*>(calleeType);

    if (hasSpreadArg && !funcType->isVariadic()) {
        Diag.report(DiagID::err_unexpected_token, expr->getBeginLoc(), expr->getRange());
        return nullptr;
    }

    auto containsGenericParam = [&](auto&& self, Type* type) -> bool {
        if (!type) {
            return false;
        }
        if (type->isGeneric() || type->isTypeVar()) {
            return true;
        }
        if (type->isGenericInstance()) {
            auto* inst = static_cast<GenericInstanceType*>(type);
            for (Type* arg : inst->getTypeArgs()) {
                if (self(self, arg)) {
                    return true;
                }
            }
            return false;
        }
        if (type->isReference()) {
            return self(self, static_cast<ReferenceType*>(type)->getPointeeType());
        }
        if (type->isPointer()) {
            return self(self, static_cast<PointerType*>(type)->getPointeeType());
        }
        if (type->isOptional()) {
            return self(self, static_cast<OptionalType*>(type)->getInnerType());
        }
        if (type->isArray()) {
            return self(self, static_cast<ArrayType*>(type)->getElementType());
        }
        if (type->isSlice()) {
            return self(self, static_cast<SliceType*>(type)->getElementType());
        }
        if (type->isVarArgs()) {
            return self(self, static_cast<VarArgsType*>(type)->getElementType());
        }
        if (type->isTuple()) {
            auto* tuple = static_cast<TupleType*>(type);
            for (size_t i = 0; i < tuple->getElementCount(); ++i) {
                if (self(self, tuple->getElement(i))) {
                    return true;
                }
            }
            return false;
        }
        if (type->isFunction()) {
            auto* fn = static_cast<FunctionType*>(type);
            for (size_t i = 0; i < fn->getParamCount(); ++i) {
                if (self(self, fn->getParam(i))) {
                    return true;
                }
            }
            return self(self, fn->getReturnType());
        }
        if (type->isError()) {
            return self(self, static_cast<ErrorType*>(type)->getSuccessType());
        }
        if (type->isRange()) {
            return self(self, static_cast<RangeType*>(type)->getElementType());
        }
        return false;
    };

    auto checkGenericBounds = [&](const std::vector<GenericParam>& params,
                                  const std::unordered_map<std::string, Type*>& mapping) -> bool {
        for (const auto& param : params) {
            if (param.Bounds.empty()) {
                continue;
            }

            auto it = mapping.find(param.Name);
            if (it == mapping.end() || !it->second) {
                continue;
            }

            Type* actualType = it->second;
            Type* ownershipCheckType = actualType;
            while (actualType && actualType->isReference()) {
                actualType = static_cast<ReferenceType*>(actualType)->getPointeeType();
            }
            while (actualType && actualType->isTypeAlias()) {
                actualType = static_cast<TypeAlias*>(actualType)->getAliasedType();
            }

            for (const std::string& bound : param.Bounds) {
                if (bound == "Copy") {
                    if (!ownershipCheckType || !isCopyType(ownershipCheckType)) {
                        Diag.report(DiagID::err_type_not_copyable, expr->getBeginLoc(), expr->getRange())
                            << (ownershipCheckType ? ownershipCheckType->toString() : "<?>");
                        return false;
                    }
                    continue;
                }
                if (bound == "Drop") {
                    if (!ownershipCheckType || !needsDrop(ownershipCheckType)) {
                        Diag.report(DiagID::err_type_requires_drop_impl, expr->getBeginLoc(),
                                    expr->getRange())
                            << (ownershipCheckType ? ownershipCheckType->toString() : "<?>");
                        return false;
                    }
                    continue;
                }

                Symbol* traitSymbol = Symbols.lookup(bound);
                if (!traitSymbol || traitSymbol->getKind() != SymbolKind::Trait) {
                    Diag.report(DiagID::err_expected_trait_bound, expr->getBeginLoc(), expr->getRange());
                    return false;
                }

                auto* traitDecl = dynamic_cast<TraitDecl*>(traitSymbol->getDecl());
                if (!traitDecl || !checkTraitBound(actualType, traitDecl)) {
                    Diag.report(DiagID::err_missing_trait_method, expr->getBeginLoc(), expr->getRange())
                        << ("trait bound " + bound);
                    return false;
                }
            }
        }

        return true;
    };

    auto ensureGenericInferenceComplete =
        [&](Type* type, const std::unordered_map<std::string, Type*>& mapping) -> bool {
            std::unordered_set<std::string> required;
            auto collectRequired = [&](auto&& self, Type* current) -> void {
                if (!current) {
                    return;
                }
                while (current->isTypeAlias()) {
                    current = static_cast<TypeAlias*>(current)->getAliasedType();
                    if (!current) {
                        return;
                    }
                }

                if (current->isGeneric()) {
                    required.insert(static_cast<GenericType*>(current)->getName());
                    return;
                }
                if (current->isTypeVar()) {
                    auto* tv = static_cast<TypeVariable*>(current);
                    if (tv->isResolved() && tv->getResolvedType()) {
                        self(self, tv->getResolvedType());
                    }
                    return;
                }
                if (current->isGenericInstance()) {
                    auto* inst = static_cast<GenericInstanceType*>(current);
                    self(self, inst->getBaseType());
                    for (Type* arg : inst->getTypeArgs()) {
                        self(self, arg);
                    }
                    return;
                }
                if (current->isReference()) {
                    self(self, static_cast<ReferenceType*>(current)->getPointeeType());
                    return;
                }
                if (current->isPointer()) {
                    self(self, static_cast<PointerType*>(current)->getPointeeType());
                    return;
                }
                if (current->isOptional()) {
                    self(self, static_cast<OptionalType*>(current)->getInnerType());
                    return;
                }
                if (current->isArray()) {
                    self(self, static_cast<ArrayType*>(current)->getElementType());
                    return;
                }
                if (current->isSlice()) {
                    self(self, static_cast<SliceType*>(current)->getElementType());
                    return;
                }
                if (current->isVarArgs()) {
                    self(self, static_cast<VarArgsType*>(current)->getElementType());
                    return;
                }
                if (current->isTuple()) {
                    auto* tuple = static_cast<TupleType*>(current);
                    for (size_t ti = 0; ti < tuple->getElementCount(); ++ti) {
                        self(self, tuple->getElement(ti));
                    }
                    return;
                }
                if (current->isFunction()) {
                    auto* fn = static_cast<FunctionType*>(current);
                    for (size_t pi = 0; pi < fn->getParamCount(); ++pi) {
                        self(self, fn->getParam(pi));
                    }
                    self(self, fn->getReturnType());
                    return;
                }
                if (current->isError()) {
                    self(self, static_cast<ErrorType*>(current)->getSuccessType());
                    return;
                }
                if (current->isRange()) {
                    self(self, static_cast<RangeType*>(current)->getElementType());
                    return;
                }
            };

            collectRequired(collectRequired, type);
            for (const std::string& name : required) {
                auto it = mapping.find(name);
                if (it == mapping.end() || !it->second) {
                    Diag.report(DiagID::err_expected_type, expr->getBeginLoc(), expr->getRange());
                    return false;
                }
            }
            return true;
        };

    auto sameValueTypeIgnoringAliases = [&](Type* lhs, Type* rhs) -> bool {
        auto unwrapAliases = [](Type* type) -> Type* {
            Type* current = type;
            while (current && current->isTypeAlias()) {
                current = static_cast<TypeAlias*>(current)->getAliasedType();
            }
            return current;
        };

        Type* left = unwrapAliases(lhs);
        Type* right = unwrapAliases(rhs);
        if (!left || !right) {
            return false;
        }

        if (left->isReference()) {
            left = unwrapAliases(static_cast<ReferenceType*>(left)->getPointeeType());
        }
        if (right->isReference()) {
            right = unwrapAliases(static_cast<ReferenceType*>(right)->getPointeeType());
        }

        return left && right && left->isEqual(right);
    };

    // 泛型实参替换
    if (expr->hasTypeArgs()) {
        if (!calleeDecl) {
            Diag.report(DiagID::err_expected_declaration, expr->getBeginLoc(), expr->getRange());
            return nullptr;
        }
        size_t expected = calleeDecl->getGenericParams().size();
        size_t actual = expr->getTypeArgCount();
        if (expected != actual) {
            Diag.report(DiagID::err_generic_param_count_mismatch, expr->getBeginLoc(), expr->getRange())
                << static_cast<unsigned>(expected)
                << static_cast<unsigned>(actual);
            return nullptr;
        }

        std::unordered_map<std::string, Type*> mapping;
        for (size_t i = 0; i < actual; ++i) {
            Type* argType = resolveType(expr->getTypeArgs()[i]);
            if (!argType) {
                return nullptr;
            }
            mapping[calleeDecl->getGenericParams()[i].Name] = argType;
        }

        if (!checkGenericBounds(calleeDecl->getGenericParams(), mapping)) {
            return nullptr;
        }

        Type* substituted = substituteType(funcType, mapping);
        if (!substituted || !substituted->isFunction()) {
            return nullptr;
        }
        funcType = static_cast<FunctionType*>(substituted);
    } else if ((calleeDecl && calleeDecl->isGeneric()) ||
               containsGenericParam(containsGenericParam, funcType)) {
        // 基于实参推导泛型参数。除了显式泛型函数外，也覆盖来自泛型 impl
        // 的方法签名（例如 Vec<T>::from_slice）。
        std::unordered_map<std::string, Type*> mapping;

        bool injectSelf = false;
        if (methodDecl && !baseIsType && !methodDecl->getParams().empty() &&
            methodDecl->getParams()[0]->isSelf()) {
            injectSelf = true;
        }

        if (injectSelf && memberCallee && funcType->getParamCount() > 0) {
            Type* selfActualType = analyzeExpr(memberCallee->getBase());
            if (!selfActualType) {
                return nullptr;
            }
            Type* selfParamType = funcType->getParam(0);
            if (!unifyGenericTypes(selfParamType, selfActualType, mapping)) {
                bool unified = false;
                if (selfParamType->isReference()) {
                    auto* refType = static_cast<ReferenceType*>(selfParamType);
                    unified = unifyGenericTypes(refType->getPointeeType(), selfActualType, mapping);
                } else if (selfActualType->isReference()) {
                    auto* refType = static_cast<ReferenceType*>(selfActualType);
                    unified = unifyGenericTypes(selfParamType, refType->getPointeeType(), mapping);
                }

                if (!unified &&
                    !sameValueTypeIgnoringAliases(selfParamType, selfActualType) &&
                    !checkTypeCompatible(selfParamType, selfActualType, memberCallee->getBase()->getRange())) {
                    return nullptr;
                }
            }
        }

        size_t paramStartIndex = injectSelf ? 1 : 0;
        size_t paramCount = funcType->getParamCount();
        size_t argCount = plainArgs.size();

        size_t fixedCount = paramCount;
        if (funcType->isVariadic() && fixedCount > 0) {
            fixedCount -= 1;
        }
        size_t expectedArgs = fixedCount >= paramStartIndex ? fixedCount - paramStartIndex : 0;
        size_t inferCount = std::min(expectedArgs, argCount);

        for (size_t i = 0; i < inferCount; ++i) {
            if (callArgs[i].IsSpread) {
                Diag.report(DiagID::err_unexpected_token,
                            callArgs[i].Value->getBeginLoc(),
                            callArgs[i].Value->getRange());
                return nullptr;
            }
            Type* argType = analyzeExpr(plainArgs[i]);
            if (!argType) {
                return nullptr;
            }
            Type* paramType = funcType->getParam(i + paramStartIndex);
            if (auto* intLit = dynamic_cast<IntegerLiteralExpr*>(plainArgs[i])) {
                if (!intLit->hasTypeSuffix() && argType->isInteger() && paramType && paramType->isInteger()) {
                    plainArgs[i]->setType(paramType);
                    argType = paramType;
                }
            }
            if (!unifyGenericTypes(paramType, argType, mapping)) {
                if (!checkTypeCompatible(paramType, argType, plainArgs[i]->getRange())) {
                    return nullptr;
                }
            }
        }

        if (calleeDecl && calleeDecl->isGeneric()) {
            const auto& params = calleeDecl->getGenericParams();
            for (const auto& param : params) {
                if (mapping.find(param.Name) == mapping.end()) {
                    Diag.report(DiagID::err_expected_type, expr->getBeginLoc());
                    return nullptr;
                }
            }

            if (!checkGenericBounds(params, mapping)) {
                return nullptr;
            }
        }

        bool requireCompleteInference = false;
        if (calleeDecl && calleeDecl->isGeneric()) {
            requireCompleteInference = true;
        }
        if (methodDecl && methodDecl->isGeneric()) {
            requireCompleteInference = true;
        }
        if (requireCompleteInference && !ensureGenericInferenceComplete(funcType, mapping)) {
            return nullptr;
        }

        Type* substituted = substituteType(funcType, mapping);
        if (!substituted || !substituted->isFunction()) {
            return nullptr;
        }
        funcType = static_cast<FunctionType*>(substituted);
    }

    bool injectSelf = false;
    if (methodDecl && !baseIsType && !methodDecl->getParams().empty() &&
        methodDecl->getParams()[0]->isSelf()) {
        injectSelf = true;
    }

    // 检查参数数量（非可变参数函数支持尾部默认参数）
    size_t expectedParamCount = funcType->getParamCount();
    size_t actualArgCount = plainArgs.size();
    size_t implicitSelfCount = injectSelf ? 1 : 0;

    auto rebuildPlainArgs = [&]() {
        plainArgs.clear();
        plainArgs.reserve(callArgs.size());
        for (const auto& arg : callArgs) {
            plainArgs.push_back(arg.Value);
        }
    };

    if (funcType->isVariadic()) {
        // 可变参数函数：实际参数数量必须 >= 固定参数数量
        size_t fixedParamCount = expectedParamCount - 1;
        size_t expectedFixedArgs = fixedParamCount >= implicitSelfCount ? fixedParamCount - implicitSelfCount : 0;
        if (actualArgCount < expectedFixedArgs) {
            Diag.report(DiagID::err_wrong_argument_count, expr->getBeginLoc(), expr->getRange())
                << static_cast<unsigned>(expectedFixedArgs)
                << static_cast<unsigned>(actualArgCount);
            return nullptr;
        }
        for (size_t i = 0; i < expectedFixedArgs; ++i) {
            if (callArgs[i].IsSpread) {
                Diag.report(DiagID::err_unexpected_token,
                            callArgs[i].Value->getBeginLoc(),
                            callArgs[i].Value->getRange());
                return nullptr;
            }
        }
    } else {
        // 普通函数：参数数量必须完全匹配
        size_t expectedArgs = expectedParamCount >= implicitSelfCount ? expectedParamCount - implicitSelfCount : 0;
        if (actualArgCount < expectedArgs && calleeDecl && !hasSpreadArg) {
            const auto& declParams = calleeDecl->getParams();
            size_t originalArgs = actualArgCount;
            bool canFill = true;
            for (size_t i = actualArgCount; i < expectedArgs; ++i) {
                size_t paramIndex = i + implicitSelfCount;
                if (paramIndex >= declParams.size() || !declParams[paramIndex]->hasDefaultValue()) {
                    canFill = false;
                    break;
                }
            }
            if (canFill) {
                for (size_t i = actualArgCount; i < expectedArgs; ++i) {
                    size_t paramIndex = i + implicitSelfCount;
                    callArgs.emplace_back(declParams[paramIndex]->getDefaultValue(), false);
                }
                rebuildPlainArgs();
                actualArgCount = plainArgs.size();
            } else {
                Diag.report(DiagID::err_wrong_argument_count, expr->getBeginLoc(), expr->getRange())
                    << static_cast<unsigned>(expectedArgs)
                    << static_cast<unsigned>(originalArgs);
                return nullptr;
            }
        }
        if (actualArgCount != expectedArgs) {
            Diag.report(DiagID::err_wrong_argument_count, expr->getBeginLoc(), expr->getRange())
                << static_cast<unsigned>(expectedArgs)
                << static_cast<unsigned>(actualArgCount);
            return nullptr;
        }
    }

    // 处理隐式 self 参数
    size_t paramStartIndex = 0;
    if (injectSelf && memberCallee) {
        Type* baseType = analyzeExpr(memberCallee->getBase());
        if (!baseType) {
            return nullptr;
        }
        Type* expectedSelfType = funcType->getParam(0);
        bool ok = false;
        if (expectedSelfType->isReference() && !baseType->isReference()) {
            auto* refType = static_cast<ReferenceType*>(expectedSelfType);
            if (sameValueTypeIgnoringAliases(refType->getPointeeType(), baseType)) {
                ok = true;
            } else {
                ok = checkTypeCompatible(expectedSelfType, baseType, memberCallee->getBase()->getRange());
            }
        } else {
            if (!expectedSelfType->isReference() && baseType->isReference()) {
                baseType = static_cast<ReferenceType*>(baseType)->getPointeeType();
            }
            ok = checkTypeCompatible(expectedSelfType, baseType, memberCallee->getBase()->getRange());
        }

        if (!ok) {
            return nullptr;
        }
        paramStartIndex = 1;
    }

    // 检查每个参数的类型
    auto adaptUnsuffixedIntArg = [](Expr* argExpr, Type*& argType, Type* paramType) {
        if (!argExpr || !argType || !paramType) {
            return;
        }
        auto* intLit = dynamic_cast<IntegerLiteralExpr*>(argExpr);
        if (!intLit || intLit->hasTypeSuffix()) {
            return;
        }
        if (!argType->isInteger() || !paramType->isInteger()) {
            return;
        }
        argExpr->setType(paramType);
        argType = paramType;
    };

    for (size_t i = 0; i < plainArgs.size(); ++i) {
        Expr* argExpr = plainArgs[i];
        Type* argType = analyzeExpr(argExpr);
        if (!argType) {
            return nullptr;
        }

        // 对于可变参数，最后一个参数是 VarArgs 类型，需要特殊处理
        if (funcType->isVariadic() && (i + paramStartIndex) >= expectedParamCount - 1) {
            Type* varParamType = funcType->getParam(expectedParamCount - 1);
            if (varParamType && varParamType->isVarArgs()) {
                auto* varArgsType = static_cast<VarArgsType*>(varParamType);
                Type* elemType = varArgsType->getElementType();
                adaptUnsuffixedIntArg(argExpr, argType, elemType);
                if (callArgs[i].IsSpread) {
                    if (i + 1 != callArgs.size()) {
                        Diag.report(DiagID::err_unexpected_token, argExpr->getBeginLoc(), argExpr->getRange());
                        return nullptr;
                    }
                    if (!argType->isVarArgs()) {
                        Diag.report(DiagID::err_type_mismatch, argExpr->getBeginLoc(), argExpr->getRange())
                            << "VarArgs"
                            << argType->toString();
                        return nullptr;
                    }
                    Type* spreadElemType = static_cast<VarArgsType*>(argType)->getElementType();
                    if (!elemType->isValue() &&
                        !checkTypeCompatible(elemType, spreadElemType, argExpr->getRange())) {
                        return nullptr;
                    }
                    continue;
                }
                // VarArgs<Value> 接受任意类型
                if (!elemType->isValue()) {
                    if (!checkTypeCompatible(elemType, argType, argExpr->getRange())) {
                        return nullptr;
                    }
                } else {
                    auto unwrapBase = [](Type* type) -> Type* {
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
                            break;
                        }
                        return base;
                    };
                    auto hasGenericParam = [&](auto&& self, Type* type) -> bool {
                        if (!type) {
                            return false;
                        }
                        if (type->isGeneric() || type->isTypeVar()) {
                            return true;
                        }
                        if (type->isGenericInstance()) {
                            auto* inst = static_cast<GenericInstanceType*>(type);
                            for (Type* arg : inst->getTypeArgs()) {
                                if (self(self, arg)) {
                                    return true;
                                }
                            }
                            return false;
                        }
                        if (type->isReference()) {
                            return self(self, static_cast<ReferenceType*>(type)->getPointeeType());
                        }
                        if (type->isPointer()) {
                            return self(self, static_cast<PointerType*>(type)->getPointeeType());
                        }
                        if (type->isOptional()) {
                            return self(self, static_cast<OptionalType*>(type)->getInnerType());
                        }
                        if (type->isArray()) {
                            return self(self, static_cast<ArrayType*>(type)->getElementType());
                        }
                        if (type->isSlice()) {
                            return self(self, static_cast<SliceType*>(type)->getElementType());
                        }
                        if (type->isTuple()) {
                            auto* tuple = static_cast<TupleType*>(type);
                            for (size_t ti = 0; ti < tuple->getElementCount(); ++ti) {
                                if (self(self, tuple->getElement(ti))) {
                                    return true;
                                }
                            }
                            return false;
                        }
                        if (type->isError()) {
                            return self(self, static_cast<ErrorType*>(type)->getSuccessType());
                        }
                        if (type->isRange()) {
                            return self(self, static_cast<RangeType*>(type)->getElementType());
                        }
                        if (type->isVarArgs()) {
                            return self(self, static_cast<VarArgsType*>(type)->getElementType());
                        }
                        return false;
                    };

                    Type* baseType = unwrapBase(argType);
                    if (hasGenericParam(hasGenericParam, argType)) {
                        continue;
                    }
                    Type* traitCheckType = baseType;
                    Type* aggregateBase = traitCheckType;
                    if (aggregateBase && aggregateBase->isGenericInstance()) {
                        aggregateBase = static_cast<GenericInstanceType*>(aggregateBase)->getBaseType();
                    }

                    if (aggregateBase && (aggregateBase->isStruct() || aggregateBase->isEnum())) {
                        TraitDecl* displayTraitDecl = nullptr;
                        TraitDecl* debugTraitDecl = nullptr;
                        if (Symbol* displaySym = Symbols.lookup("Display")) {
                            displayTraitDecl = dynamic_cast<TraitDecl*>(displaySym->getDecl());
                        }
                        if (Symbol* debugSym = Symbols.lookup("Debug")) {
                            debugTraitDecl = dynamic_cast<TraitDecl*>(debugSym->getDecl());
                        }

                        bool hasDisplay = displayTraitDecl && checkTraitBound(traitCheckType, displayTraitDecl);
                        bool hasDebug = debugTraitDecl && checkTraitBound(traitCheckType, debugTraitDecl);
                        if (!hasDisplay && !hasDebug) {
                            Diag.report(DiagID::err_trait_not_implemented,
                                        argExpr->getBeginLoc(),
                                        argExpr->getRange())
                                << "Display"
                                << argType->toString();
                            return nullptr;
                        }
                    } else if (baseType && (baseType->isInteger() || baseType->isFloat() ||
                                            baseType->isString() || baseType->isBool() ||
                                            baseType->isChar() || baseType->isValue())) {
                        // ok
                    } else {
                        Diag.report(DiagID::err_trait_not_implemented,
                                    argExpr->getBeginLoc(),
                                    argExpr->getRange())
                            << "Display"
                            << argType->toString();
                        return nullptr;
                    }
                }
            }
            continue;
        }

        if (callArgs[i].IsSpread) {
            Diag.report(DiagID::err_unexpected_token, argExpr->getBeginLoc(), argExpr->getRange());
            return nullptr;
        }
        Type* paramType = funcType->getParam(i + paramStartIndex);
        adaptUnsuffixedIntArg(argExpr, argType, paramType);
        if (!checkTypeCompatible(paramType, argType, argExpr->getRange())) {
            return nullptr;
        }
    }

    // 返回函数的返回类型
    Type* returnType = funcType->getReturnType();

    // 如果函数可能返回错误，返回 Error 类型
    if (funcType->canError()) {
        return Ctx.getErrorType(returnType);
    }

    return returnType;
}

Type* Sema::analyzeBuiltinCallExpr(BuiltinCallExpr* expr) {
    // 委托给 BuiltinRegistry 处理
    auto& registry = BuiltinRegistry::instance();
    auto* handler = registry.getHandler(expr->getBuiltinKind());

    if (!handler) {
        Diag.report(DiagID::err_function_not_found, expr->getBeginLoc(), expr->getRange())
            << BuiltinCallExpr::getBuiltinName(expr->getBuiltinKind());
        return nullptr;
    }

    return handler->analyze(expr, *this);
}

Type* Sema::analyzeMemberExpr(MemberExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // 分析基础表达式
    Type* baseType = analyzeExpr(expr->getBase());
    if (!baseType) {
        return nullptr;
    }

    auto unwrapAliases = [](Type* type) -> Type* {
        Type* current = type;
        while (current && current->isTypeAlias()) {
            current = static_cast<TypeAlias*>(current)->getAliasedType();
        }
        return current;
    };

    baseType = unwrapAliases(baseType);
    if (!baseType) {
        reportError(DiagID::err_expected_type, expr->getBeginLoc());
        return nullptr;
    }

    // 如果是引用类型，获取被引用的类型
    if (baseType->isReference()) {
        baseType = static_cast<ReferenceType*>(baseType)->getPointeeType();
        baseType = unwrapAliases(baseType);
    }

    // 如果是指针类型，获取被指向的类型
    if (baseType->isPointer()) {
        baseType = static_cast<PointerType*>(baseType)->getPointeeType();
        baseType = unwrapAliases(baseType);
    }

    // 泛型实例类型：记录基础类型与实参
    GenericInstanceType* genericInst = nullptr;
    if (baseType->isGenericInstance()) {
        genericInst = static_cast<GenericInstanceType*>(baseType);
        baseType = genericInst->getBaseType();
        baseType = unwrapAliases(baseType);
    }

    // 处理字符串类型的成员访问
    if (baseType->isString()) {
        const std::string& member = expr->getMember();
        if (member == "len") {
            // 返回零参数函数类型，避免调用时参数检查失败
            return Ctx.getFunctionType({}, Ctx.getI32Type(), false);
        }
        if (member == "iter") {
            return Ctx.getFunctionType({}, Ctx.getStrType(), false);
        }
        if (member == "ptr") {
            return Ctx.getPointerType(Ctx.getU8Type(), false);
        }
    }

    // 处理数组类型的成员访问
    if (baseType->isArray()) {
        const std::string& member = expr->getMember();
        if (member == "len") {
            return Ctx.getFunctionType({}, Ctx.getI64Type(), false);
        }
        if (member == "iter") {
            return Ctx.getFunctionType({}, baseType, false);
        }
    }

    // 处理切片类型的成员访问
    if (baseType->isSlice()) {
        auto* sliceType = static_cast<SliceType*>(baseType);
        const std::string& member = expr->getMember();
        if (member == "len") {
            return Ctx.getFunctionType({}, Ctx.getI64Type(), false);
        }
        if (member == "iter") {
            return Ctx.getFunctionType({}, baseType, false);
        }
        if (member == "ptr") {
            return Ctx.getPointerType(sliceType->getElementType(), sliceType->isMutable());
        }
    }

    // 处理 Range 类型成员访问
    if (baseType->isRange()) {
        if (expr->getMember() == "iter") {
            return Ctx.getFunctionType({}, baseType, false);
        }
    }

    // 处理受 Trait 约束的泛型参数成员访问
    if (baseType->isGeneric()) {
        auto* genericType = static_cast<GenericType*>(baseType);
        const auto& constraints = genericType->getConstraints();

        for (TraitType* constraint : constraints) {
            if (!constraint) {
                continue;
            }

            Symbol* traitSymbol = Symbols.lookup(constraint->getName());
            if (!traitSymbol || traitSymbol->getKind() != SymbolKind::Trait) {
                continue;
            }

            auto* traitDecl = dynamic_cast<TraitDecl*>(traitSymbol->getDecl());
            if (!traitDecl) {
                continue;
            }

            FuncDecl* method = traitDecl->findMethod(expr->getMember());
            if (!method) {
                continue;
            }

            expr->setResolvedDecl(method);

            Type* methodType = method->getSemanticType();
            if (!methodType || !methodType->isFunction()) {
                return methodType;
            }

            auto* methodFunc = static_cast<FunctionType*>(methodType);
            auto replaceTraitSelf = [&](auto&& self, Type* ty) -> Type* {
                if (!ty) {
                    return nullptr;
                }

                if (ty->isTrait()) {
                    auto* traitTy = static_cast<TraitType*>(ty);
                    if (traitTy->getName() == constraint->getName()) {
                        return baseType;
                    }
                    return ty;
                }
                if (ty->isReference()) {
                    auto* refTy = static_cast<ReferenceType*>(ty);
                    Type* replaced = self(self, refTy->getPointeeType());
                    if (!replaced) {
                        return nullptr;
                    }
                    if (replaced == refTy->getPointeeType()) {
                        return ty;
                    }
                    return Ctx.getReferenceType(replaced, refTy->isMutable());
                }
                if (ty->isPointer()) {
                    auto* ptrTy = static_cast<PointerType*>(ty);
                    Type* replaced = self(self, ptrTy->getPointeeType());
                    if (!replaced) {
                        return nullptr;
                    }
                    if (replaced == ptrTy->getPointeeType()) {
                        return ty;
                    }
                    return Ctx.getPointerType(replaced, ptrTy->isMutable());
                }
                if (ty->isOptional()) {
                    auto* optTy = static_cast<OptionalType*>(ty);
                    Type* replaced = self(self, optTy->getInnerType());
                    if (!replaced) {
                        return nullptr;
                    }
                    if (replaced == optTy->getInnerType()) {
                        return ty;
                    }
                    return Ctx.getOptionalType(replaced);
                }
                if (ty->isArray()) {
                    auto* arrTy = static_cast<ArrayType*>(ty);
                    Type* replaced = self(self, arrTy->getElementType());
                    if (!replaced) {
                        return nullptr;
                    }
                    if (replaced == arrTy->getElementType()) {
                        return ty;
                    }
                    return Ctx.getArrayType(replaced, arrTy->getArraySize());
                }
                if (ty->isSlice()) {
                    auto* sliceTy = static_cast<SliceType*>(ty);
                    Type* replaced = self(self, sliceTy->getElementType());
                    if (!replaced) {
                        return nullptr;
                    }
                    if (replaced == sliceTy->getElementType()) {
                        return ty;
                    }
                    return Ctx.getSliceType(replaced, sliceTy->isMutable());
                }
                if (ty->isTuple()) {
                    auto* tupleTy = static_cast<TupleType*>(ty);
                    std::vector<Type*> elems;
                    elems.reserve(tupleTy->getElementCount());
                    bool changed = false;
                    for (size_t i = 0; i < tupleTy->getElementCount(); ++i) {
                        Type* orig = tupleTy->getElement(i);
                        Type* replaced = self(self, orig);
                        if (!replaced) {
                            return nullptr;
                        }
                        changed = changed || (replaced != orig);
                        elems.push_back(replaced);
                    }
                    if (!changed) {
                        return ty;
                    }
                    return Ctx.getTupleType(std::move(elems));
                }
                if (ty->isFunction()) {
                    auto* fnTy = static_cast<FunctionType*>(ty);
                    std::vector<Type*> params;
                    params.reserve(fnTy->getParamCount());
                    bool changed = false;
                    for (Type* paramTy : fnTy->getParamTypes()) {
                        Type* replaced = self(self, paramTy);
                        if (!replaced) {
                            return nullptr;
                        }
                        changed = changed || (replaced != paramTy);
                        params.push_back(replaced);
                    }
                    Type* returnTy = self(self, fnTy->getReturnType());
                    if (!returnTy) {
                        return nullptr;
                    }
                    changed = changed || (returnTy != fnTy->getReturnType());
                    if (!changed) {
                        return ty;
                    }
                    return Ctx.getFunctionType(std::move(params), returnTy, fnTy->canError(),
                                               fnTy->isVariadic());
                }
                if (ty->isError()) {
                    auto* errTy = static_cast<ErrorType*>(ty);
                    Type* replaced = self(self, errTy->getSuccessType());
                    if (!replaced) {
                        return nullptr;
                    }
                    if (replaced == errTy->getSuccessType()) {
                        return ty;
                    }
                    return Ctx.getErrorType(replaced);
                }
                if (ty->isRange()) {
                    auto* rangeTy = static_cast<RangeType*>(ty);
                    Type* replaced = self(self, rangeTy->getElementType());
                    if (!replaced) {
                        return nullptr;
                    }
                    if (replaced == rangeTy->getElementType()) {
                        return ty;
                    }
                    return Ctx.getRangeType(replaced, rangeTy->isInclusive());
                }
                return ty;
            };

            Type* specialized = replaceTraitSelf(replaceTraitSelf, methodType);
            if (!specialized) {
                return nullptr;
            }
            return specialized;
        }

        Diag.report(DiagID::err_field_not_found, expr->getBeginLoc(), expr->getRange())
            << expr->getMember()
            << baseType->toString();
        return nullptr;
    }

    // 处理结构体类型的成员访问
    if (baseType->isStruct()) {
        auto* structType = static_cast<StructType*>(baseType);
        const StructType::Field* field = structType->getField(expr->getMember());
        if (!field) {
            Type* receiverType = genericInst ? static_cast<Type*>(genericInst) : baseType;
            std::unordered_map<std::string, Type*> methodMapping;
            FuncDecl* method = resolveImplMethod(receiverType, expr->getMember(),
                                                 &methodMapping, nullptr, true);
            if (!method && receiverType && !receiverType->isGenericInstance() &&
                !receiverType->isGeneric()) {
                method = Ctx.getImplMethod(receiverType, expr->getMember());
            }
            if (method) {
                expr->setResolvedDecl(method);
                Type* methodType = method->getSemanticType();
                if (!methodType) {
                    return nullptr;
                }
                if (!methodMapping.empty()) {
                    methodType = substituteType(methodType, methodMapping);
                }
                return methodType;
            }

            Diag.report(DiagID::err_field_not_found, expr->getBeginLoc(), expr->getRange())
                << expr->getMember()
                << baseType->toString();
            return nullptr;
        }
        Type* fieldType = field->FieldType;
        if (genericInst) {
            std::unordered_map<std::string, Type*> mapping;
            if (buildGenericSubstitution(baseType, genericInst->getTypeArgs(), mapping)) {
                fieldType = substituteType(fieldType, mapping);
            }
        }
        return fieldType;
    }

    // 处理 VarArgs 类型的成员访问
    if (baseType->isVarArgs()) {
        const std::string& member = expr->getMember();
        if (member == "len") {
            return Ctx.getI64Type();
        }
        if (member == "iter") {
            return Ctx.getFunctionType({}, baseType, false);
        }
        Diag.report(DiagID::err_field_not_found, expr->getBeginLoc(), expr->getRange())
            << expr->getMember()
            << baseType->toString();
        return nullptr;
    }

    // 处理元组类型的成员访问（使用数字索引）
    if (baseType->isTuple()) {
        auto* tupleType = static_cast<TupleType*>(baseType);
        const std::string& member = expr->getMember();

        if (member == "iter") {
            return Ctx.getFunctionType({}, baseType, false);
        }

        // 尝试将成员名解析为数字索引
        char* end;
        unsigned long index = strtoul(member.c_str(), &end, 10);
        if (*end != '\0' || member.empty()) {
            Diag.report(DiagID::err_field_not_found, expr->getBeginLoc(), expr->getRange())
                << expr->getMember()
                << baseType->toString();
            return nullptr;
        }

        if (index >= tupleType->getElementCount()) {
            Diag.report(DiagID::err_index_out_of_bounds, expr->getBeginLoc(), expr->getRange())
                << static_cast<unsigned>(index)
                << static_cast<unsigned>(tupleType->getElementCount());
            return nullptr;
        }

        return tupleType->getElement(index);
    }

    // 处理枚举类型的变体访问
    if (baseType->isEnum()) {
        auto* enumType = static_cast<EnumType*>(baseType);
        const EnumType::Variant* variant = enumType->getVariant(expr->getMember());
        if (!variant) {
            if (enumType->getName() == "SysError") {
                if (expr->getMember() == "message" || expr->getMember() == "full_trace") {
                    return Ctx.getFunctionType({}, Ctx.getStrType(), false);
                }
                if (expr->getMember() == "func_name" || expr->getMember() == "file") {
                    return Ctx.getStrType();
                }
                if (expr->getMember() == "line") {
                    return Ctx.getU32Type();
                }
            }
            Type* receiverType = genericInst ? static_cast<Type*>(genericInst) : baseType;
            std::unordered_map<std::string, Type*> methodMapping;
            FuncDecl* method = resolveImplMethod(receiverType, expr->getMember(),
                                                 &methodMapping, nullptr, true);
            if (!method && receiverType && !receiverType->isGenericInstance() &&
                !receiverType->isGeneric()) {
                method = Ctx.getImplMethod(receiverType, expr->getMember());
            }
            if (method) {
                expr->setResolvedDecl(method);
                Type* methodType = method->getSemanticType();
                if (!methodType) {
                    return nullptr;
                }
                if (!methodMapping.empty()) {
                    methodType = substituteType(methodType, methodMapping);
                }
                return methodType;
            }

            Diag.report(DiagID::err_field_not_found, expr->getBeginLoc(), expr->getRange())
                << expr->getMember()
                << baseType->toString();
            return nullptr;
        }
        // 枚举变体的类型是枚举类型本身
        return enumType;
    }

    // 处理模块类型的成员访问
    if (baseType->isModule()) {
        auto* moduleType = static_cast<ModuleType*>(baseType);
        const ModuleType::Member* member = moduleType->getMember(expr->getMember());
        if (!member) {
            Diag.report(DiagID::err_field_not_found, expr->getBeginLoc(), expr->getRange())
                << expr->getMember()
                << baseType->toString();
            return nullptr;
        }

        // 设置 ResolvedDecl
        expr->setResolvedDecl(static_cast<Decl*>(member->Decl));

        return member->MemberType;
    }

    // 其他类型不支持成员访问
    Diag.report(DiagID::err_field_not_found, expr->getBeginLoc(), expr->getRange())
        << expr->getMember()
        << baseType->toString();
    return nullptr;
}

Type* Sema::analyzeIndexExpr(IndexExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // 分析基础表达式
    Type* baseType = analyzeExpr(expr->getBase());
    if (!baseType) {
        return nullptr;
    }

    // 分析索引表达式
    Type* indexType = analyzeExpr(expr->getIndex());
    if (!indexType) {
        return nullptr;
    }

    // 索引必须是整数类型
    if (!indexType->isInteger()) {
        Diag.report(DiagID::err_type_mismatch, expr->getIndex()->getBeginLoc(), expr->getIndex()->getRange())
            << "integer"
            << indexType->toString();
        return nullptr;
    }

    // 如果是引用类型，获取被引用的类型
    if (baseType->isReference()) {
        baseType = static_cast<ReferenceType*>(baseType)->getPointeeType();
    }

    // 处理数组类型的索引
    if (baseType->isArray()) {
        return static_cast<ArrayType*>(baseType)->getElementType();
    }

    // 处理切片类型的索引
    if (baseType->isSlice()) {
        return static_cast<SliceType*>(baseType)->getElementType();
    }

    // 处理字符串类型的索引
    if (baseType->isString()) {
        return Ctx.getCharType();
    }

    // 处理 VarArgs 类型的索引
    if (baseType->isVarArgs()) {
        return static_cast<VarArgsType*>(baseType)->getElementType();
    }

    // 处理元组类型的索引（仅支持常量索引）
    if (baseType->isTuple()) {
        // 元组索引需要在编译时确定，这里暂时不支持
        Diag.report(DiagID::err_cannot_index_non_array, expr->getBeginLoc(), expr->getRange())
            << baseType->toString();
        return nullptr;
    }

    Diag.report(DiagID::err_cannot_index_non_array, expr->getBeginLoc(), expr->getRange())
        << baseType->toString();
    return nullptr;
}

Type* Sema::analyzeSliceExpr(SliceExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // 分析基础表达式
    Type* baseType = analyzeExpr(expr->getBase());
    if (!baseType) {
        return nullptr;
    }

    // 分析起始索引（如果有）
    if (expr->hasStart()) {
        Type* startType = analyzeExpr(expr->getStart());
        if (!startType) {
            return nullptr;
        }
        if (!startType->isInteger()) {
            Diag.report(DiagID::err_type_mismatch, expr->getStart()->getBeginLoc(),
                        expr->getStart()->getRange())
                << "integer"
                << startType->toString();
            return nullptr;
        }
    }

    // 分析结束索引（如果有）
    if (expr->hasEnd()) {
        Type* endType = analyzeExpr(expr->getEnd());
        if (!endType) {
            return nullptr;
        }
        if (!endType->isInteger()) {
            Diag.report(DiagID::err_type_mismatch, expr->getEnd()->getBeginLoc(),
                        expr->getEnd()->getRange())
                << "integer"
                << endType->toString();
            return nullptr;
        }
    }

    // 如果是引用类型，获取被引用的类型
    bool isMutable = false;
    if (baseType->isReference()) {
        auto* refType = static_cast<ReferenceType*>(baseType);
        isMutable = refType->isMutable();
        baseType = refType->getPointeeType();
    }

    // 处理数组类型的切片
    if (baseType->isArray()) {
        Type* elementType = static_cast<ArrayType*>(baseType)->getElementType();
        return Ctx.getSliceType(elementType, isMutable);
    }

    // 处理切片类型的切片（返回相同类型的切片）
    if (baseType->isSlice()) {
        return baseType;
    }

    // 处理字符串类型的切片
    if (baseType->isString()) {
        return Ctx.getStrType();
    }

    Diag.report(DiagID::err_type_mismatch, expr->getBeginLoc(), expr->getRange())
        << "slice/array/str"
        << baseType->toString();
    return nullptr;
}

Type* Sema::analyzeCastExpr(CastExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // 分析被转换的表达式
    Type* exprType = analyzeExpr(expr->getExpr());
    if (!exprType) {
        return nullptr;
    }

    // 解析目标类型
    Type* targetType = resolveType(expr->getTargetType());
    if (!targetType) {
        return nullptr;
    }

    // 检查类型转换的有效性
    // 允许的转换：
    // - 数值类型之间的转换
    // - 指针类型之间的转换
    // - 引用类型之间的转换

    bool validCast = false;

    // 数值类型之间的转换
    if (exprType->isNumeric() && targetType->isNumeric()) {
        validCast = true;
    }

    // 整数和字符之间的转换
    if ((exprType->isInteger() && targetType->isChar()) ||
        (exprType->isChar() && targetType->isInteger())) {
        validCast = true;
    }

    // 指针类型之间的转换
    if (exprType->isPointer() && targetType->isPointer()) {
        validCast = true;
    }

    // 引用与指针之间的转换
    if (exprType->isReference() && targetType->isPointer()) {
        Type* srcPointee = static_cast<ReferenceType*>(exprType)->getPointeeType();
        Type* dstPointee = static_cast<PointerType*>(targetType)->getPointeeType();
        if (srcPointee->isEqual(dstPointee)) {
            validCast = true;
        }
    }
    if (exprType->isPointer() && targetType->isReference()) {
        Type* srcPointee = static_cast<PointerType*>(exprType)->getPointeeType();
        Type* dstPointee = static_cast<ReferenceType*>(targetType)->getPointeeType();
        if (srcPointee->isEqual(dstPointee)) {
            validCast = true;
        }
    }

    // 指针与指针宽度整数（usize/isize）之间的转换
    if (exprType->isPointer() && targetType->isInteger()) {
        auto* intTy = static_cast<IntegerType*>(targetType);
        if (intTy->getBitWidth() == Ctx.getPointerBitWidth()) {
            validCast = true;
        }
    }
    if (exprType->isInteger() && targetType->isPointer()) {
        auto* intTy = static_cast<IntegerType*>(exprType);
        if (intTy->getBitWidth() == Ctx.getPointerBitWidth()) {
            validCast = true;
        }
    }

    // 引用类型之间的转换（如果目标不是更宽松的可变性）
    if (exprType->isReference() && targetType->isReference()) {
        auto* srcRef = static_cast<ReferenceType*>(exprType);
        auto* dstRef = static_cast<ReferenceType*>(targetType);
        // 只允许从可变引用转换为不可变引用，或者类型相同
        if (srcRef->getPointeeType()->isEqual(dstRef->getPointeeType())) {
            if (!dstRef->isMutable() || srcRef->isMutable()) {
                validCast = true;
            }
        }
    }

    if (!validCast) {
        // 非指针宽度整数到指针的转换按规范归类为类型不匹配
        if (exprType->isInteger() && targetType->isPointer()) {
            Diag.report(DiagID::err_type_mismatch, expr->getBeginLoc(), expr->getRange())
                << targetType->toString()
                << exprType->toString();
            return nullptr;
        }
        Diag.report(DiagID::err_invalid_cast, expr->getBeginLoc(), expr->getRange())
            << exprType->toString()
            << targetType->toString();
        return nullptr;
    }

    return targetType;
}

Type* Sema::analyzeIfExpr(IfExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    const auto& branches = expr->getBranches();
    if (branches.empty()) {
        return nullptr;
    }

    Type* resultType = nullptr;

    for (const auto& branch : branches) {
        // 分析条件表达式（else 分支没有条件）
        if (branch.Condition) {
            Type* condType = analyzeExpr(branch.Condition);
            if (!condType) {
                return nullptr;
            }
            if (!condType->isBool()) {
                Diag.report(DiagID::err_type_mismatch, branch.Condition->getBeginLoc(),
                            branch.Condition->getRange())
                    << "bool"
                    << condType->toString();
                return nullptr;
            }
        }

        // 分析分支体
        Type* bodyType = analyzeExpr(branch.Body);
        if (!bodyType) {
            return nullptr;
        }

        // 确定结果类型
        if (!resultType) {
            resultType = bodyType;
        } else if (!resultType->isEqual(bodyType)) {
            // 所有分支必须返回相同类型
            Diag.report(DiagID::err_type_mismatch, branch.Body->getBeginLoc(),
                        branch.Body->getRange())
                << resultType->toString()
                << bodyType->toString();
            return nullptr;
        }
    }

    // 如果没有 else 分支，返回 Optional 类型
    if (!expr->hasElse()) {
        return Ctx.getOptionalType(resultType);
    }

    return resultType;
}

Type* Sema::analyzeBlockExpr(BlockExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    Symbols.enterScope(Scope::Kind::Block);

    for (Stmt* stmt : expr->getStatements()) {
        if (!analyzeStmt(stmt)) {
            Symbols.exitScope();
            return nullptr;
        }
    }

    Type* resultType = Ctx.getVoidType();
    if (expr->hasResult()) {
        resultType = analyzeExpr(expr->getResultExpr());
        if (!resultType) {
            Symbols.exitScope();
            return nullptr;
        }
    }

    Symbols.exitScope();
    return resultType;
}

Type* Sema::analyzeMatchExpr(MatchExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // 分析被匹配的表达式
    Type* scrutineeType = analyzeExpr(expr->getScrutinee());
    if (!scrutineeType) {
        return nullptr;
    }

    const auto& arms = expr->getArms();
    if (arms.empty()) {
        reportError(DiagID::err_unexpected_token, expr->getBeginLoc());
        return nullptr;
    }

    Type* resultType = nullptr;
    bool success = true;

    for (const auto& arm : arms) {
        Symbols.enterScope(Scope::Kind::Block);

        // 分析模式（绑定变量到当前作用域）
        if (!analyzePattern(arm.Pat, scrutineeType)) {
            success = false;
        }

        // 分析守卫条件（如果有）
        if (arm.Guard) {
            Type* guardType = analyzeExpr(arm.Guard);
            if (!guardType) {
                success = false;
            } else if (!guardType->isBool()) {
                Diag.report(DiagID::err_type_mismatch, arm.Guard->getBeginLoc(),
                            arm.Guard->getRange())
                    << "bool"
                    << guardType->toString();
                success = false;
            }
        }

        // 分析分支体
        Type* bodyType = analyzeExpr(arm.Body);
        if (!bodyType) {
            success = false;
        } else if (!resultType) {
            resultType = bodyType;
        } else if (!resultType->isEqual(bodyType)) {
            Diag.report(DiagID::err_type_mismatch, arm.Body->getBeginLoc(),
                        arm.Body->getRange())
                << resultType->toString()
                << bodyType->toString();
            success = false;
        }

        Symbols.exitScope();
    }

    if (!success || !resultType) {
        return nullptr;
    }

    Type* scrutineeBase = scrutineeType;
    if (scrutineeBase->isReference()) {
        scrutineeBase = static_cast<ReferenceType*>(scrutineeBase)->getPointeeType();
    } else if (scrutineeBase->isPointer()) {
        scrutineeBase = static_cast<PointerType*>(scrutineeBase)->getPointeeType();
    }
    if (scrutineeBase->isGenericInstance()) {
        scrutineeBase = static_cast<GenericInstanceType*>(scrutineeBase)->getBaseType();
    }

    auto hasCatchAllPattern = [&]() -> bool {
        std::function<bool(Pattern*)> isAlwaysMatch = [&](Pattern* pat) -> bool {
            if (!pat) return false;
            switch (pat->getKind()) {
                case ASTNode::Kind::WildcardPattern:
                case ASTNode::Kind::IdentifierPattern:
                    return true;
                case ASTNode::Kind::BindPattern:
                    return isAlwaysMatch(static_cast<BindPattern*>(pat)->getInner());
                case ASTNode::Kind::OrPattern: {
                    auto* orPat = static_cast<OrPattern*>(pat);
                    for (auto* alt : orPat->getPatterns()) {
                        if (isAlwaysMatch(alt)) return true;
                    }
                    return false;
                }
                default:
                    return false;
            }
        };
        for (const auto& arm : arms) {
            if (!arm.Pat || arm.Guard) continue;
            if (isAlwaysMatch(arm.Pat)) {
                return true;
            }
        }
        return false;
    };

    if (!hasCatchAllPattern()) {
        auto reportMissing = [&](const std::string& missing) {
            Diag.report(DiagID::err_non_exhaustive_match, expr->getBeginLoc(), expr->getRange())
                << missing;
        };

        if (scrutineeBase->isBool()) {
            bool hasTrue = false;
            bool hasFalse = false;
            std::function<void(Pattern*)> collectBool = [&](Pattern* pat) {
                if (!pat) return;
                if (auto* litPat = dynamic_cast<LiteralPattern*>(pat)) {
                    if (auto* boolLit = dynamic_cast<BoolLiteralExpr*>(litPat->getLiteral())) {
                        if (boolLit->getValue()) hasTrue = true;
                        else hasFalse = true;
                    }
                    return;
                }
                if (auto* orPat = dynamic_cast<OrPattern*>(pat)) {
                    for (auto* alt : orPat->getPatterns()) collectBool(alt);
                    return;
                }
                if (auto* bindPat = dynamic_cast<BindPattern*>(pat)) {
                    collectBool(bindPat->getInner());
                }
            };
            for (const auto& arm : arms) {
                if (!arm.Pat || arm.Guard) continue;
                collectBool(arm.Pat);
            }
            if (!(hasTrue && hasFalse)) {
                std::string missing;
                if (!hasTrue) missing += "true";
                if (!hasFalse) missing += (missing.empty() ? "" : ", ") + std::string("false");
                reportMissing(missing);
                return nullptr;
            }
        } else if (scrutineeBase->isEnum()) {
            auto* enumType = static_cast<EnumType*>(scrutineeBase);
            std::set<std::string> covered;
            std::function<void(Pattern*)> collect = [&](Pattern* pat) {
                if (!pat) return;
                if (auto* enumPat = dynamic_cast<EnumPattern*>(pat)) {
                    covered.insert(enumPat->getVariantName());
                    return;
                }
                if (auto* identPat = dynamic_cast<IdentifierPattern*>(pat)) {
                    if (const EnumType::Variant* variant = enumType->getVariant(identPat->getName())) {
                        if (variant->Data.empty()) covered.insert(identPat->getName());
                    }
                    return;
                }
                if (auto* litPat = dynamic_cast<LiteralPattern*>(pat)) {
                    if (dynamic_cast<NoneLiteralExpr*>(litPat->getLiteral()) &&
                        enumType->getVariant("None")) {
                        covered.insert("None");
                    }
                    return;
                }
                if (auto* orPat = dynamic_cast<OrPattern*>(pat)) {
                    for (auto* alt : orPat->getPatterns()) collect(alt);
                    return;
                }
                if (auto* bindPat = dynamic_cast<BindPattern*>(pat)) {
                    collect(bindPat->getInner());
                }
            };
            for (const auto& arm : arms) {
                if (!arm.Pat || arm.Guard) continue;
                collect(arm.Pat);
            }
            std::vector<std::string> missing;
            for (const auto& variant : enumType->getVariants()) {
                if (covered.find(variant.Name) == covered.end()) {
                    missing.push_back(variant.Name);
                }
            }
            if (!missing.empty()) {
                std::string names;
                for (size_t i = 0; i < missing.size(); ++i) {
                    if (i) names += ", ";
                    names += missing[i];
                }
                reportMissing(names);
                return nullptr;
            }
        } else if (scrutineeBase->isOptional()) {
            bool hasSome = false;
            bool hasNone = false;
            std::function<void(Pattern*)> collect = [&](Pattern* pat) {
                if (!pat) return;
                if (auto* enumPat = dynamic_cast<EnumPattern*>(pat)) {
                    if (enumPat->getVariantName() == "Some") hasSome = true;
                    if (enumPat->getVariantName() == "None") hasNone = true;
                    return;
                }
                if (auto* litPat = dynamic_cast<LiteralPattern*>(pat)) {
                    if (dynamic_cast<NoneLiteralExpr*>(litPat->getLiteral())) hasNone = true;
                    return;
                }
                if (auto* orPat = dynamic_cast<OrPattern*>(pat)) {
                    for (auto* alt : orPat->getPatterns()) collect(alt);
                    return;
                }
                if (auto* bindPat = dynamic_cast<BindPattern*>(pat)) {
                    collect(bindPat->getInner());
                }
            };
            for (const auto& arm : arms) {
                if (!arm.Pat || arm.Guard) continue;
                collect(arm.Pat);
            }
            if (!(hasSome && hasNone)) {
                std::string missing;
                if (!hasSome) missing += "Some";
                if (!hasNone) missing += (missing.empty() ? "" : ", ") + std::string("None");
                reportMissing(missing);
                return nullptr;
            }
        } else {
            reportMissing("wildcard or identifier pattern");
            return nullptr;
        }
    }

    return resultType;
}

Type* Sema::analyzeClosureExpr(ClosureExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    bool enteredClosureGenerics = false;
    if (expr->isGeneric()) {
        if (!enterGenericParamScope(expr->getGenericParams())) {
            return nullptr;
        }
        enteredClosureGenerics = true;
    }

    auto cleanupAndFail = [&]() -> Type* {
        if (enteredClosureGenerics) {
            exitGenericParamScope();
        }
        return nullptr;
    };

    // 进入闭包作用域
    Symbols.enterScope(Scope::Kind::Function);
    // 闭包体中的 return 不应引用外层函数上下文。
    Symbols.getCurrentScope()->setCurrentFunction(nullptr);

    std::vector<Type*> paramTypes;

    // 分析参数
    for (ParamDecl* param : expr->getParams()) {
        Type* paramType = nullptr;

        // 如果有类型注解，解析类型
        if (param->getType()) {
            paramType = resolveType(param->getType());
            if (!paramType) {
                Symbols.exitScope();
                return cleanupAndFail();
            }
        } else {
            // 没有类型注解，需要从上下文推断
            // 暂时创建一个类型变量
            paramType = Ctx.getVoidType(); // 占位符，后续实现类型推断
        }

        paramTypes.push_back(paramType);

        // 将参数添加到闭包作用域
        auto* paramSymbol = new Symbol(SymbolKind::Parameter, param->getName(), paramType,
                                       param->getBeginLoc(), Visibility::Private);
        paramSymbol->setMutable(param->isMutable());
        paramSymbol->setDecl(param);

        if (!Symbols.addSymbol(paramSymbol)) {
            delete paramSymbol;
            Symbols.exitScope();
            return cleanupAndFail();
        }
    }

    Type* bodyType = nullptr;
    if (auto* blockBody = dynamic_cast<BlockExpr*>(expr->getBody())) {
        Symbols.enterScope(Scope::Kind::Block);

        bool bodyOk = true;
        bool sawYieldReturn = false;
        bodyType = Ctx.getVoidType();

        for (Stmt* stmt : blockBody->getStatements()) {
            if (auto* ret = dynamic_cast<ReturnStmt*>(stmt)) {
                sawYieldReturn = true;
                if (ret->hasValue()) {
                    bodyType = analyzeExpr(ret->getValue());
                    if (!bodyType) {
                        bodyOk = false;
                    }
                } else {
                    bodyType = Ctx.getVoidType();
                }
                break;
            }

            if (!analyzeStmt(stmt)) {
                bodyOk = false;
                break;
            }
        }

        if (bodyOk && !sawYieldReturn) {
            if (blockBody->hasResult()) {
                bodyType = analyzeExpr(blockBody->getResultExpr());
                if (!bodyType) {
                    bodyOk = false;
                }
            } else {
                bodyType = Ctx.getVoidType();
            }
        }

        Symbols.exitScope();

        if (!bodyOk) {
            Symbols.exitScope();
            return cleanupAndFail();
        }
    } else {
        bodyType = analyzeExpr(expr->getBody());
        if (!bodyType) {
            Symbols.exitScope();
            return cleanupAndFail();
        }
    }

    // 确定返回类型
    Type* returnType = nullptr;
    if (expr->getReturnType()) {
        returnType = resolveType(expr->getReturnType());
        if (!returnType) {
            Symbols.exitScope();
            return cleanupAndFail();
        }
        // 检查闭包体类型与返回类型是否兼容
        if (!checkTypeCompatible(returnType, bodyType, expr->getBody()->getRange())) {
            Symbols.exitScope();
            return cleanupAndFail();
        }
    } else {
        // 从闭包体推断返回类型
        returnType = bodyType;
    }

    // 退出闭包作用域
    Symbols.exitScope();

    if (enteredClosureGenerics) {
        exitGenericParamScope();
    }

    // 创建函数类型
    return Ctx.getFunctionType(std::move(paramTypes), returnType, false);
}

Type* Sema::analyzeArrayExpr(ArrayExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    const auto& elements = expr->getElements();

    if (expr->isRepeat()) {
        // 重复初始化形式：[element; count]
        if (elements.empty()) {
            return nullptr;
        }

        Type* elementType = analyzeExpr(elements[0]);
        if (!elementType) {
            return nullptr;
        }

        Type* countType = analyzeExpr(expr->getRepeatCount());
        if (!countType) {
            return nullptr;
        }

        if (!countType->isInteger()) {
            Diag.report(DiagID::err_type_mismatch, expr->getRepeatCount()->getBeginLoc(),
                        expr->getRepeatCount()->getRange())
                << "integer"
                << countType->toString();
            return nullptr;
        }

        int64_t arraySizeValue = 0;
        if (!evaluateConstExpr(expr->getRepeatCount(), arraySizeValue)) {
            Diag.report(DiagID::err_invalid_array_size, expr->getRepeatCount()->getBeginLoc(),
                        expr->getRepeatCount()->getRange());
            return nullptr;
        }

        if (arraySizeValue <= 0) {
            Diag.report(DiagID::err_invalid_array_size, expr->getRepeatCount()->getBeginLoc(),
                        expr->getRepeatCount()->getRange());
            return nullptr;
        }

        uint64_t arraySize = static_cast<uint64_t>(arraySizeValue);
        return Ctx.getArrayType(elementType, arraySize);
    } else {
        // 元素列表形式：[e1, e2, ...]
        if (elements.empty()) {
            // 空数组，需要从上下文推断类型
            return Ctx.getArrayType(Ctx.getVoidType(), 0);
        }

        // 分析第一个元素确定类型
        Type* elementType = analyzeExpr(elements[0]);
        if (!elementType) {
            return nullptr;
        }

        // 检查所有元素类型是否相同
        for (size_t i = 1; i < elements.size(); ++i) {
            Type* elemType = analyzeExpr(elements[i]);
            if (!elemType) {
                return nullptr;
            }
            if (!elementType->isEqual(elemType)) {
                Diag.report(DiagID::err_type_mismatch, elements[i]->getBeginLoc(),
                            elements[i]->getRange())
                    << elementType->toString()
                    << elemType->toString();
                return nullptr;
            }
        }

        return Ctx.getArrayType(elementType, elements.size());
    }
}

Type* Sema::analyzeTupleExpr(TupleExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    const auto& elements = expr->getElements();

    // 空元组
    if (elements.empty()) {
        return Ctx.getTupleType({});
    }

    std::vector<Type*> elementTypes;
    elementTypes.reserve(elements.size());

    for (Expr* elem : elements) {
        Type* elemType = analyzeExpr(elem);
        if (!elemType) {
            return nullptr;
        }
        elementTypes.push_back(elemType);
    }

    return Ctx.getTupleType(std::move(elementTypes));
}

Type* Sema::analyzeStructExpr(StructExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // 查找结构体类型
    Symbol* structSymbol = Symbols.lookup(expr->getTypeName());
    if (!structSymbol) {
        // 尝试解析为枚举变体结构体形式：Enum.Variant { ... }
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
        if (splitEnumVariant(expr->getTypeName(), enumName, variantName)) {
            Symbol* enumSymbol = Symbols.lookup(enumName);
            if (enumSymbol && enumSymbol->getKind() == SymbolKind::Enum) {
                Type* enumType = enumSymbol->getType();
                if (enumType && enumType->isEnum()) {
                    auto* enumTy = static_cast<EnumType*>(enumType);
                    const EnumType::Variant* variant = enumTy->getVariant(variantName);
                    if (!variant) {
                        Diag.report(DiagID::err_undeclared_identifier, expr->getBeginLoc(), expr->getRange())
                            << variantName;
                        return nullptr;
                    }
                    if (variant->Data.size() != 1 || !variant->Data[0]->isStruct()) {
                        Diag.report(DiagID::err_type_mismatch, expr->getBeginLoc(), expr->getRange())
                            << "struct payload"
                            << ("payload(" + std::to_string(variant->Data.size()) + ")");
                        return nullptr;
                    }

                    auto* sType = static_cast<StructType*>(variant->Data[0]);

                    // 检查每个字段初始化
                    for (const auto& fieldInit : expr->getFields()) {
                        const StructType::Field* field = sType->getField(fieldInit.Name);
                        if (!field) {
                            Diag.report(DiagID::err_field_not_found, fieldInit.Loc)
                                << fieldInit.Name
                                << sType->toString();
                            return nullptr;
                        }

                        Type* valueType = analyzeExpr(fieldInit.Value);
                        if (!valueType) {
                            return nullptr;
                        }

                        if (!checkTypeCompatible(field->FieldType, valueType, fieldInit.Value->getRange())) {
                            return nullptr;
                        }
                    }

                    if (expr->hasBase()) {
                        Type* baseType = analyzeExpr(expr->getBase());
                        if (!baseType) {
                            return nullptr;
                        }
                        if (!baseType->isEqual(sType)) {
                            Diag.report(DiagID::err_type_mismatch, expr->getBase()->getBeginLoc(),
                                        expr->getBase()->getRange())
                                << sType->toString()
                                << baseType->toString();
                            return nullptr;
                        }
                    }

                    return enumType;
                }
            }
        }

        Diag.report(DiagID::err_undeclared_identifier, expr->getBeginLoc(), expr->getRange())
            << expr->getTypeName();
        return nullptr;
    }

    if (structSymbol->getKind() != SymbolKind::Struct) {
        reportError(DiagID::err_expected_type, expr->getBeginLoc());
        return nullptr;
    }

    Type* structType = structSymbol->getType();
    if (!structType || !structType->isStruct()) {
        return nullptr;
    }

    auto* sType = static_cast<StructType*>(structType);
    auto* structDecl = dynamic_cast<StructDecl*>(structSymbol->getDecl());

    // 处理泛型实例化（优先使用字面量显式类型实参）
    Type* instanceType = structType;
    std::unordered_map<std::string, Type*> mapping;
    if (structDecl && structDecl->isGeneric()) {
        const auto& params = structDecl->getGenericParams();

        if (expr->hasTypeArgs()) {
            if (expr->getTypeArgs().size() != params.size()) {
                Diag.report(DiagID::err_wrong_argument_count, expr->getBeginLoc(), expr->getRange())
                    << static_cast<unsigned>(params.size())
                    << static_cast<unsigned>(expr->getTypeArgs().size());
                return nullptr;
            }

            std::vector<Type*> explicitArgs;
            explicitArgs.reserve(expr->getTypeArgs().size());
            for (TypeNode* argNode : expr->getTypeArgs()) {
                Type* argType = resolveType(argNode);
                if (!argType) {
                    return nullptr;
                }
                explicitArgs.push_back(argType);
            }

            instanceType = Ctx.getGenericInstanceType(structType, explicitArgs);
            buildGenericSubstitution(structType, explicitArgs, mapping);
        } else {
            std::vector<Type*> inferredArgs;
            inferredArgs.reserve(params.size());
            bool canInstantiate = true;
            for (const auto& param : params) {
                Symbol* paramSym = Symbols.lookup(param.Name);
                if (!paramSym || paramSym->getKind() != SymbolKind::GenericParam) {
                    canInstantiate = false;
                    break;
                }
                inferredArgs.push_back(paramSym->getType());
            }
            if (canInstantiate) {
                instanceType = Ctx.getGenericInstanceType(structType, inferredArgs);
                buildGenericSubstitution(structType, inferredArgs, mapping);
            }
        }
    }

    std::set<std::string> initializedFields;

    // 检查每个字段初始化
    for (const auto& fieldInit : expr->getFields()) {
        // 查找字段
        const StructType::Field* field = sType->getField(fieldInit.Name);
        if (!field) {
            Diag.report(DiagID::err_field_not_found, fieldInit.Loc)
                << fieldInit.Name
                << structType->toString();
            return nullptr;
        }

        if (initializedFields.find(fieldInit.Name) != initializedFields.end()) {
            Diag.report(DiagID::err_redefinition, fieldInit.Loc)
                << fieldInit.Name;
            return nullptr;
        }
        initializedFields.insert(fieldInit.Name);

        // 分析字段值
        Type* valueType = analyzeExpr(fieldInit.Value);
        if (!valueType) {
            return nullptr;
        }

        Type* expectedFieldType = field->FieldType;
        if (!mapping.empty()) {
            expectedFieldType = substituteType(expectedFieldType, mapping);
        }

        // 检查类型兼容性
        if (!checkTypeCompatible(expectedFieldType, valueType, fieldInit.Value->getRange())) {
            return nullptr;
        }
    }

    // 检查缺失字段（带 base 更新语法时允许缺失；带默认值字段允许缺失）
    if (!expr->hasBase()) {
        if (structDecl) {
            for (FieldDecl* declField : structDecl->getFields()) {
                if (initializedFields.find(declField->getName()) != initializedFields.end()) {
                    continue;
                }
                if (declField->hasDefaultValue()) {
                    continue;
                }
                Diag.report(DiagID::err_field_not_found, expr->getBeginLoc(), expr->getRange())
                    << declField->getName()
                    << structType->toString();
                return nullptr;
            }
        } else {
            for (const auto& field : sType->getFields()) {
                if (initializedFields.find(field.Name) == initializedFields.end()) {
                    Diag.report(DiagID::err_field_not_found, expr->getBeginLoc(), expr->getRange())
                        << field.Name
                        << structType->toString();
                    return nullptr;
                }
            }
        }
    }

    // 如果有基础表达式，分析它
    if (expr->hasBase()) {
        Type* baseType = analyzeExpr(expr->getBase());
        if (!baseType) {
            return nullptr;
        }
        if (!baseType->isEqual(instanceType)) {
            Diag.report(DiagID::err_type_mismatch, expr->getBase()->getBeginLoc(),
                        expr->getBase()->getRange())
                << instanceType->toString()
                << baseType->toString();
            return nullptr;
        }
    }

    return instanceType;
}

Type* Sema::analyzeRangeExpr(RangeExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    Type* startType = nullptr;
    Type* endType = nullptr;

    // 分析起始值（如果有）
    if (expr->hasStart()) {
        startType = analyzeExpr(expr->getStart());
        if (!startType) {
            return nullptr;
        }
        if (!startType->isInteger()) {
            Diag.report(DiagID::err_type_mismatch, expr->getStart()->getBeginLoc(),
                        expr->getStart()->getRange())
                << "integer"
                << startType->toString();
            return nullptr;
        }
    }

    // 分析结束值（如果有）
    if (expr->hasEnd()) {
        endType = analyzeExpr(expr->getEnd());
        if (!endType) {
            return nullptr;
        }
        if (!endType->isInteger()) {
            Diag.report(DiagID::err_type_mismatch, expr->getEnd()->getBeginLoc(),
                        expr->getEnd()->getRange())
                << "integer"
                << endType->toString();
            return nullptr;
        }
    }

    // 如果两者都有，检查类型是否相同
    if (startType && endType && !startType->isEqual(endType)) {
        Diag.report(DiagID::err_type_mismatch, expr->getBeginLoc(), expr->getRange())
            << startType->toString()
            << endType->toString();
        return nullptr;
    }

    // 确定范围的元素类型
    Type* elementType = startType ? startType : endType;
    if (!elementType) {
        elementType = Ctx.getI32Type(); // 默认类型
    }

    // 创建 Range 类型
    return Ctx.getRangeType(elementType, expr->isInclusive());
}

Type* Sema::analyzeAwaitExpr(AwaitExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    Scope* currentScope = Symbols.getCurrentScope();
    FuncDecl* currentFunc = currentScope ? currentScope->getCurrentFunction() : nullptr;
    if (!currentFunc || !currentFunc->isAsync()) {
        reportError(DiagID::err_await_outside_async, expr->getBeginLoc());
        return nullptr;
    }

    Type* innerType = analyzeExpr(expr->getInner());
    if (!innerType) {
        return nullptr;
    }

    // await 保持被等待表达式的类型；若是 `!T`，可由上层 `!`/`-> err` 继续处理。
    return innerType;
}

Type* Sema::analyzeErrorPropagateExpr(ErrorPropagateExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // 分析内部表达式
    Type* innerType = analyzeExpr(expr->getInner());
    if (!innerType) {
        return nullptr;
    }

    // 内部表达式必须是 Error 类型
    if (!innerType->isError()) {
        reportError(DiagID::err_error_propagation_invalid, expr->getBeginLoc());
        return nullptr;
    }

    // `expr!` 要求当前函数可返回错误（`-> !T`）。
    Scope* funcScope = Symbols.getCurrentScope();
    while (funcScope && funcScope->getKind() != Scope::Kind::Function) {
        funcScope = funcScope->getParent();
    }
    if (funcScope) {
        FuncDecl* currentFunc = funcScope->getCurrentFunction();
        if (currentFunc && !currentFunc->canError()) {
            std::string funcName = currentFunc->getName();
            if (funcName.empty()) {
                funcName = "<closure>";
            }
            Diag.report(DiagID::err_unhandled_error_propagation, expr->getBeginLoc(), expr->getRange())
                << funcName;
            return nullptr;
        }
    }

    // 返回成功时的类型
    return static_cast<ErrorType*>(innerType)->getSuccessType();
}

Type* Sema::analyzeErrorHandleExpr(ErrorHandleExpr* expr) {
    if (!expr) {
        return nullptr;
    }

    // 分析内部表达式（允许 expr! -> err {} 在非错误函数内）
    Expr* innerExpr = expr->getInner();
    if (auto* propagate = dynamic_cast<ErrorPropagateExpr*>(innerExpr)) {
        innerExpr = propagate->getInner();
    }

    Type* innerType = analyzeExpr(innerExpr);
    if (!innerType) {
        return nullptr;
    }

    // 内部表达式必须是 Error 类型
    if (!innerType->isError()) {
        Diag.report(DiagID::err_type_mismatch, expr->getBeginLoc(), expr->getRange())
            << "error"
            << innerType->toString();
        return nullptr;
    }

    Type* successType = static_cast<ErrorType*>(innerType)->getSuccessType();

    // 进入错误处理作用域
    Symbols.enterScope(Scope::Kind::Block);

    // 将错误变量添加到作用域
    // 优先使用内置 SysError 类型，否则退回到 str
    Type* errorVarType = Ctx.getStrType();
    if (Symbol* sysErrSym = Symbols.lookup("SysError")) {
        if (sysErrSym->getKind() == SymbolKind::Enum && sysErrSym->getType()) {
            errorVarType = sysErrSym->getType();
        }
    }

    auto* errorSymbol = new Symbol(SymbolKind::Variable, expr->getErrorVar(),
                                   errorVarType, expr->getBeginLoc(), Visibility::Private);
    errorSymbol->setMutable(false);

    // 创建一个隐式的错误变量声明用于后续代码生成
    auto* errorDecl = Ctx.create<VarDecl>(
        SourceRange(expr->getBeginLoc(), expr->getBeginLoc()),
        expr->getErrorVar(),
        nullptr,
        nullptr,
        false,
        Visibility::Private
    );
    errorDecl->setSemanticType(errorVarType);
    errorSymbol->setDecl(errorDecl);
    expr->setErrorVarDecl(errorDecl);

    if (!Symbols.addSymbol(errorSymbol)) {
        delete errorSymbol;
        Symbols.exitScope();
        return nullptr;
    }

    // 分析错误处理块
    if (!analyzeStmt(expr->getHandler())) {
        Symbols.exitScope();
        return nullptr;
    }

    // 退出错误处理作用域
    Symbols.exitScope();

    // 错误处理表达式的类型是成功时的类型
    return successType;
}

// ============================================================================
// 类型检查辅助函数（占位符）
// ============================================================================

bool Sema::checkTypeCompatible(Type* expected, Type* actual, SourceLocation loc) {
    return TypeCheckerImpl && TypeCheckerImpl->checkTypeCompatible(expected, actual, loc);
}

bool Sema::checkTypeCompatible(Type* expected, Type* actual, SourceRange range) {
    return TypeCheckerImpl && TypeCheckerImpl->checkTypeCompatible(expected, actual, range);
}

bool Sema::checkAssignable(Expr* target, SourceLocation loc) {
    return TypeCheckerImpl && TypeCheckerImpl->checkAssignable(target, loc);
}

bool Sema::checkMutable(Expr* target, SourceLocation loc) {
    return TypeCheckerImpl && TypeCheckerImpl->checkMutable(target, loc);
}

Type* Sema::getCommonType(Type* t1, Type* t2) {
    return TypeCheckerImpl ? TypeCheckerImpl->getCommonType(t1, t2) : nullptr;
}

bool Sema::isCopyType(Type* type) {
    return TypeCheckerImpl && TypeCheckerImpl->isCopyType(type);
}

bool Sema::needsDrop(Type* type) {
    return TypeCheckerImpl && TypeCheckerImpl->needsDrop(type);
}

bool Sema::analyzeOwnership(FuncDecl* decl) {
    OwnershipAnalyzer analyzer(*this, decl);
    return analyzer.run();
}

// ============================================================================
// 模式分析（占位符）
// ============================================================================

bool Sema::analyzePattern(Pattern* pattern, Type* expectedType) {
    if (!pattern) {
        return false;
    }

    // 模式匹配基于值类型，而不是引用/指针外层包装类型。
    Type* matchType = expectedType;
    if (matchType) {
        if (matchType->isReference()) {
            matchType = static_cast<ReferenceType*>(matchType)->getPointeeType();
        } else if (matchType->isPointer()) {
            matchType = static_cast<PointerType*>(matchType)->getPointeeType();
        }
    }

    Type* expectedBase = matchType;
    std::unordered_map<std::string, Type*> mapping;
    if (matchType && matchType->isGenericInstance()) {
        auto* genInst = static_cast<GenericInstanceType*>(matchType);
        expectedBase = genInst->getBaseType();
        buildGenericSubstitution(expectedBase, genInst->getTypeArgs(), mapping);
    }

    switch (pattern->getKind()) {
        case ASTNode::Kind::WildcardPattern:
            // 通配符模式匹配任何类型，不绑定变量
            return true;

        case ASTNode::Kind::IdentifierPattern: {
            auto* identPat = static_cast<IdentifierPattern*>(pattern);
            if (expectedBase && expectedBase->isEnum()) {
                auto* enumType = static_cast<EnumType*>(expectedBase);
                const EnumType::Variant* variant = enumType->getVariant(identPat->getName());
                if (variant && variant->Data.empty()) {
                    // 在枚举类型上下文中按枚举变体模式处理。
                    return true;
                }
            }
            // 创建变量符号
            auto* symbol = new Symbol(SymbolKind::Variable, identPat->getName(), matchType, identPat->getBeginLoc(), Visibility::Private);
            symbol->setMutable(identPat->isMutable());
            
            // 创建变量声明并关联到符号和模式
            // 注意：我们创建一个变量声明，但不将其添加到 ASTContext 的顶层声明中
            // 它是属于 ForStmt 或 MatchStmt 的局部声明
            auto* varDecl = new VarDecl(
                identPat->getRange(),
                identPat->getName(),
                identPat->getType(),
                nullptr, // 无初始化表达式（由模式绑定提供）
                identPat->isMutable(),
                Visibility::Private,
                nullptr
            );

            varDecl->setSemanticType(matchType);
            
            symbol->setDecl(varDecl);
            identPat->setDecl(varDecl);

            if (!Symbols.addSymbol(symbol)) {
                delete symbol;
                // 注意：该变量声明由 symbol 管理（如果 symbol 被删除），或者如果 symbol 不管理 decl，
                // 则需要在这里手动删除该变量声明。
                // 在当前实现中，Symbol 并不拥有 Decl 的所有权（Decl 通常由 ASTContext 拥有），
                // 但这里 new 出来的变量声明如果不被 ASTContext 跟踪，可能会泄露。
                // 理想情况下应通过 ASTContext 的工厂方法创建，但目前没有对应接口。
                // 暂时假设 Decl 的生命周期由 ASTContext 管理（如果注册的话）或者在这里泄露（如果在此处失败）。
                // 为了简化处理，这里不手动 delete 该变量声明，因为它可能已被部分引用。
                // 实际项目中应有统一的 AST 节点内存管理策略。
                
                Diag.report(DiagID::err_redefinition, identPat->getBeginLoc(), identPat->getRange())
                    << identPat->getName();
                return false;
            }

            // 如果有类型注解，检查是否匹配
            if (identPat->hasType()) {
                Type* annotatedType = resolveType(identPat->getType());
                if (annotatedType && !checkTypeCompatible(annotatedType, matchType, identPat->getRange())) {
                    return false;
                }
            }
            return true;
        }

        case ASTNode::Kind::BindPattern: {
            auto* bindPat = static_cast<BindPattern*>(pattern);

            // 先分析内部模式（它可能会绑定其他变量）
            if (!analyzePattern(bindPat->getInner(), matchType)) {
                return false;
            }

            // 创建绑定变量符号
            auto* symbol = new Symbol(SymbolKind::Variable, bindPat->getName(), matchType, bindPat->getBeginLoc(), Visibility::Private);
            symbol->setMutable(bindPat->isMutable());

            auto* varDecl = new VarDecl(
                bindPat->getRange(),
                bindPat->getName(),
                bindPat->getType(),
                nullptr,
                bindPat->isMutable(),
                Visibility::Private,
                nullptr
            );

            varDecl->setSemanticType(matchType);

            symbol->setDecl(varDecl);
            bindPat->setDecl(varDecl);

            if (!Symbols.addSymbol(symbol)) {
                delete symbol;
                Diag.report(DiagID::err_redefinition, bindPat->getBeginLoc(), bindPat->getRange())
                    << bindPat->getName();
                return false;
            }

            // 如果有类型注解，检查是否匹配
            if (bindPat->hasType()) {
                Type* annotatedType = resolveType(bindPat->getType());
                if (annotatedType && !checkTypeCompatible(annotatedType, matchType, bindPat->getRange())) {
                    return false;
                }
            }

            return true;
        }

        case ASTNode::Kind::OrPattern: {
            auto* orPat = static_cast<OrPattern*>(pattern);
            const auto& patterns = orPat->getPatterns();
            if (patterns.empty()) {
                return false;
            }

            // 第一个分支在当前作用域中绑定变量
            if (!analyzePattern(patterns[0], matchType)) {
                return false;
            }

            // 其他分支在临时作用域中进行类型检查，避免重复定义
            for (size_t i = 1; i < patterns.size(); ++i) {
                Symbols.enterScope(Scope::Kind::Block);
                bool ok = analyzePattern(patterns[i], matchType);
                Symbols.exitScope();
                if (!ok) {
                    return false;
                }
            }

            return true;
        }

        case ASTNode::Kind::LiteralPattern: {
            auto* litPat = static_cast<LiteralPattern*>(pattern);
            if (auto* noneLit = dynamic_cast<NoneLiteralExpr*>(litPat->getLiteral())) {
                (void)noneLit;
                if (expectedBase && expectedBase->isEnum()) {
                    auto* enumType = static_cast<EnumType*>(expectedBase);
                    if (enumType->getVariant("None")) {
                        return true;
                    }
                }
            }
            // 分析字面量表达式并检查类型匹配
            Type* litType = analyzeExpr(litPat->getLiteral());
            if (!litType) {
                return false;
            }
            if (!checkTypeCompatible(matchType, litType, litPat->getRange())) {
                return false;
            }
            return true;
        }

        case ASTNode::Kind::TuplePattern: {
            auto* tuplePat = static_cast<TuplePattern*>(pattern);
            // 检查期望类型是否为元组
            if (!expectedBase->isTuple()) {
                Diag.report(DiagID::err_type_mismatch, tuplePat->getBeginLoc(), tuplePat->getRange())
                    << "tuple"
                    << expectedBase->toString();
                return false;
            }
            auto* tupleType = static_cast<TupleType*>(expectedBase);
            if (tupleType->getElementCount() != tuplePat->getElementCount()) {
                Diag.report(DiagID::err_type_mismatch, tuplePat->getBeginLoc(), tuplePat->getRange())
                    << ("tuple(" + std::to_string(tupleType->getElementCount()) + " elements)")
                    << ("tuple(" + std::to_string(tuplePat->getElementCount()) + " elements)");
                return false;
            }
            // 递归分析每个元素模式
            for (size_t i = 0; i < tuplePat->getElementCount(); ++i) {
                if (!analyzePattern(tuplePat->getElements()[i], tupleType->getElement(i))) {
                    return false;
                }
            }
            return true;
        }

        case ASTNode::Kind::StructPattern: {
            auto* structPat = static_cast<StructPattern*>(pattern);
            // 检查期望类型是否为结构体
            if (!expectedBase->isStruct()) {
                Diag.report(DiagID::err_type_mismatch, structPat->getBeginLoc(), structPat->getRange())
                    << "struct"
                    << expectedBase->toString();
                return false;
            }
            auto* structType = static_cast<StructType*>(expectedBase);
            // 检查字段
            for (const auto& field : structPat->getFields()) {
                const StructType::Field* structField = structType->getField(field.Name);
                if (!structField) {
                    Diag.report(DiagID::err_field_not_found, field.Loc)
                        << field.Name
                        << structType->toString();
                    return false;
                }
                if (field.Pat) {
                    Type* fieldType = structField->FieldType;
                    if (!mapping.empty()) {
                        fieldType = substituteType(fieldType, mapping);
                    }
                    if (!analyzePattern(field.Pat, fieldType)) {
                        return false;
                    }
                } else {
                    // 简写形式：创建同名变量
                    auto* symbol = new Symbol(
                        SymbolKind::Variable,
                        field.Name,
                        !mapping.empty() ? substituteType(structField->FieldType, mapping)
                                         : structField->FieldType,
                        field.Loc,
                        Visibility::Private
                    );
                    symbol->setMutable(false);
                    if (!Symbols.addSymbol(symbol)) {
                        delete symbol;
                        Diag.report(DiagID::err_redefinition, field.Loc)
                            << field.Name;
                        return false;
                    }
                }
            }
            return true;
        }

        case ASTNode::Kind::EnumPattern: {
            auto* enumPat = static_cast<EnumPattern*>(pattern);
            
            // Optional 类型作为枚举处理（Some/None 变体）
            if (expectedBase->isOptional()) {
                auto* optType = static_cast<OptionalType*>(expectedBase);
                Type* innerType = optType->getInnerType();
                const std::string& variantName = enumPat->getVariantName();
                
                if (variantName == "None") {
                    // None 没有负载
                    if (enumPat->hasPayload()) {
                        Diag.report(DiagID::err_type_mismatch, enumPat->getBeginLoc(), enumPat->getRange())
                            << "payload(0)"
                            << ("payload(" + std::to_string(enumPat->getPayloadCount()) + ")");
                        return false;
                    }
                    return true;
                } else if (variantName == "Some") {
                    // Some 有一个负载，类型为 Optional 的内部类型
                    if (!enumPat->hasPayload()) {
                        Diag.report(DiagID::err_type_mismatch, enumPat->getBeginLoc(), enumPat->getRange())
                            << "payload(1)"
                            << "payload(0)";
                        return false;
                    }
                    if (enumPat->getPayloadCount() != 1) {
                        Diag.report(DiagID::err_type_mismatch, enumPat->getBeginLoc(), enumPat->getRange())
                            << "payload(1)"
                            << ("payload(" + std::to_string(enumPat->getPayloadCount()) + ")");
                        return false;
                    }
                    return analyzePattern(enumPat->getPayload()[0], innerType);
                } else {
                    Diag.report(DiagID::err_undeclared_identifier, enumPat->getBeginLoc(), enumPat->getRange())
                        << variantName;
                    return false;
                }
            }
            
            // 检查期望类型是否为枚举
            if (!expectedBase->isEnum()) {
                Diag.report(DiagID::err_type_mismatch, enumPat->getBeginLoc(), enumPat->getRange())
                    << "enum"
                    << expectedBase->toString();
                return false;
            }
            auto* enumType = static_cast<EnumType*>(expectedBase);
            // 检查变体是否存在
            const EnumType::Variant* variant = enumType->getVariant(enumPat->getVariantName());
            if (!variant) {
                Diag.report(DiagID::err_undeclared_identifier, enumPat->getBeginLoc(), enumPat->getRange())
                    << enumPat->getVariantName();
                return false;
            }
            // 检查负载模式
            if (enumPat->hasPayload()) {
                if (variant->Data.empty()) {
                    Diag.report(DiagID::err_type_mismatch, enumPat->getBeginLoc(), enumPat->getRange())
                        << "payload(0)"
                        << ("payload(" + std::to_string(enumPat->getPayloadCount()) + ")");
                    return false;
                }

                if (variant->Data.size() == 1) {
                    Type* payloadType = variant->Data[0];
                    if (!mapping.empty()) {
                        payloadType = substituteType(payloadType, mapping);
                    }
                    if (payloadType->isTuple()) {
                        auto* tupleType = static_cast<TupleType*>(payloadType);
                        if (enumPat->getPayloadCount() != tupleType->getElementCount()) {
                            Diag.report(DiagID::err_type_mismatch, enumPat->getBeginLoc(), enumPat->getRange())
                                << ("payload(" + std::to_string(tupleType->getElementCount()) + ")")
                                << ("payload(" + std::to_string(enumPat->getPayloadCount()) + ")");
                            return false;
                        }
                        for (size_t i = 0; i < enumPat->getPayloadCount(); ++i) {
                            if (!analyzePattern(enumPat->getPayload()[i], tupleType->getElement(i))) {
                                return false;
                            }
                        }
                    } else if (payloadType->isStruct()) {
                        auto* structType = static_cast<StructType*>(payloadType);
                        const auto& fields = structType->getFields();
                        if (enumPat->getPayloadCount() != fields.size()) {
                            Diag.report(DiagID::err_type_mismatch, enumPat->getBeginLoc(), enumPat->getRange())
                                << ("payload(" + std::to_string(fields.size()) + ")")
                                << ("payload(" + std::to_string(enumPat->getPayloadCount()) + ")");
                            return false;
                        }
                        for (size_t i = 0; i < enumPat->getPayloadCount(); ++i) {
                            Type* fieldType = fields[i].FieldType;
                            if (!mapping.empty()) {
                                fieldType = substituteType(fieldType, mapping);
                            }
                            if (!analyzePattern(enumPat->getPayload()[i], fieldType)) {
                                return false;
                            }
                        }
                    } else {
                        if (enumPat->getPayloadCount() != 1) {
                            Diag.report(DiagID::err_type_mismatch, enumPat->getBeginLoc(), enumPat->getRange())
                                << "payload(1)"
                                << ("payload(" + std::to_string(enumPat->getPayloadCount()) + ")");
                            return false;
                        }
                        if (!analyzePattern(enumPat->getPayload()[0], payloadType)) {
                            return false;
                        }
                    }
                } else {
                    if (variant->Data.size() != enumPat->getPayloadCount()) {
                        Diag.report(DiagID::err_type_mismatch, enumPat->getBeginLoc(), enumPat->getRange())
                            << ("payload(" + std::to_string(variant->Data.size()) + ")")
                            << ("payload(" + std::to_string(enumPat->getPayloadCount()) + ")");
                        return false;
                    }
                    for (size_t i = 0; i < enumPat->getPayloadCount(); ++i) {
                        Type* payloadType = variant->Data[i];
                        if (!mapping.empty()) {
                            payloadType = substituteType(payloadType, mapping);
                        }
                        if (!analyzePattern(enumPat->getPayload()[i], payloadType)) {
                            return false;
                        }
                    }
                }
            }
            return true;
        }

        case ASTNode::Kind::RangePattern: {
            auto* rangePat = static_cast<RangePattern*>(pattern);
            // 分析范围端点
            Type* startType = nullptr;
            Type* endType = nullptr;
            if (rangePat->getStart()) {
                startType = analyzeExpr(rangePat->getStart());
                if (!startType) {
                    return false;
                }
                if (!checkTypeCompatible(matchType, startType, rangePat->getStart()->getBeginLoc())) {
                    return false;
                }
            }
            if (rangePat->getEnd()) {
                endType = analyzeExpr(rangePat->getEnd());
                if (!endType) {
                    return false;
                }
                if (!checkTypeCompatible(matchType, endType, rangePat->getEnd()->getBeginLoc())) {
                    return false;
                }
            }
            return true;
        }

        default:
            // 未知模式类型
            return false;
    }
}

bool Sema::checkExhaustive(MatchStmt* match) {
    if (!match) {
        return false;
    }

    const auto& arms = match->getArms();
    if (arms.empty()) {
        reportError(DiagID::err_unexpected_token, match->getBeginLoc());
        return false;
    }

    Type* scrutineeType = match->getScrutinee()->getType();
    if (!scrutineeType) {
        return true; // 无法检查
    }

    Type* scrutineeBase = scrutineeType;
    if (scrutineeBase->isReference()) {
        scrutineeBase = static_cast<ReferenceType*>(scrutineeBase)->getPointeeType();
    } else if (scrutineeBase->isPointer()) {
        scrutineeBase = static_cast<PointerType*>(scrutineeBase)->getPointeeType();
    }
    if (scrutineeBase->isGenericInstance()) {
        scrutineeBase = static_cast<GenericInstanceType*>(scrutineeBase)->getBaseType();
    }

    // 检查是否有通配符或标识符模式（这些总是穷尽的）
    for (const auto& arm : arms) {
        if (!arm.Pat) continue;

        if (arm.Guard) {
            continue;
        }

        std::function<bool(Pattern*)> isAlwaysMatch = [&](Pattern* pat) -> bool {
            if (!pat) return false;
            switch (pat->getKind()) {
                case ASTNode::Kind::WildcardPattern:
                    return true;
                case ASTNode::Kind::IdentifierPattern:
                    return true;
                case ASTNode::Kind::BindPattern: {
                    auto* bindPat = static_cast<BindPattern*>(pat);
                    return isAlwaysMatch(bindPat->getInner());
                }
                case ASTNode::Kind::OrPattern: {
                    auto* orPat = static_cast<OrPattern*>(pat);
                    for (auto* alt : orPat->getPatterns()) {
                        if (isAlwaysMatch(alt)) {
                            return true;
                        }
                    }
                    return false;
                }
                default:
                    return false;
            }
        };

        if (isAlwaysMatch(arm.Pat)) {
            return true;
        }
    }

    auto reportMissing = [&](const std::string& missing) {
        Diag.report(DiagID::err_non_exhaustive_match, match->getBeginLoc(), match->getRange())
            << missing;
    };

    auto joinNames = [](const std::vector<std::string>& names) {
        std::string result;
        for (size_t i = 0; i < names.size(); ++i) {
            if (i > 0) {
                result += ", ";
            }
            result += names[i];
        }
        return result;
    };

    // 对于布尔类型,检查是否覆盖了 true 和 false
    if (scrutineeBase->isBool()) {
        bool hasTrue = false;
        bool hasFalse = false;
        std::function<void(Pattern*)> collectBool = [&](Pattern* pat) {
            if (!pat) return;
            if (auto* litPat = dynamic_cast<LiteralPattern*>(pat)) {
                if (auto* boolLit = dynamic_cast<BoolLiteralExpr*>(litPat->getLiteral())) {
                    if (boolLit->getValue()) {
                        hasTrue = true;
                    } else {
                        hasFalse = true;
                    }
                }
                return;
            }
            if (auto* orPat = dynamic_cast<OrPattern*>(pat)) {
                for (auto* alt : orPat->getPatterns()) {
                    collectBool(alt);
                }
                return;
            }
            if (auto* bindPat = dynamic_cast<BindPattern*>(pat)) {
                collectBool(bindPat->getInner());
                return;
            }
        };
        for (const auto& arm : arms) {
            if (!arm.Pat || arm.Guard) continue;
            collectBool(arm.Pat);
        }
        if (hasTrue && hasFalse) {
            return true;
        }
        // 不完整的布尔匹配 - 报告错误
        std::vector<std::string> missing;
        if (!hasTrue) missing.push_back("true");
        if (!hasFalse) missing.push_back("false");
        reportMissing(joinNames(missing));
        return false;
    }

    // 对于枚举类型,检查是否覆盖了所有变体
    if (scrutineeBase->isEnum()) {
        auto* enumType = static_cast<EnumType*>(scrutineeBase);
        std::set<std::string> coveredVariants;
        std::function<void(Pattern*)> collectVariants = [&](Pattern* pat) {
            if (!pat) return;
            if (auto* enumPat = dynamic_cast<EnumPattern*>(pat)) {
                coveredVariants.insert(enumPat->getVariantName());
                return;
            }
            if (auto* identPat = dynamic_cast<IdentifierPattern*>(pat)) {
                const EnumType::Variant* variant = enumType->getVariant(identPat->getName());
                if (variant && variant->Data.empty()) {
                    coveredVariants.insert(identPat->getName());
                }
                return;
            }
            if (auto* litPat = dynamic_cast<LiteralPattern*>(pat)) {
                if (dynamic_cast<NoneLiteralExpr*>(litPat->getLiteral())) {
                    if (enumType->getVariant("None")) {
                        coveredVariants.insert("None");
                    }
                }
                return;
            }
            if (auto* orPat = dynamic_cast<OrPattern*>(pat)) {
                for (auto* alt : orPat->getPatterns()) {
                    collectVariants(alt);
                }
                return;
            }
            if (auto* bindPat = dynamic_cast<BindPattern*>(pat)) {
                collectVariants(bindPat->getInner());
                return;
            }
        };

        for (const auto& arm : arms) {
            if (!arm.Pat || arm.Guard) continue;
            collectVariants(arm.Pat);
        }

        // 检查是否所有变体都被覆盖
        bool allCovered = true;
        std::vector<std::string> uncoveredVariants;

        for (const auto& variant : enumType->getVariants()) {
            if (coveredVariants.find(variant.Name) == coveredVariants.end()) {
                allCovered = false;
                uncoveredVariants.push_back(variant.Name);
            }
        }

        if (allCovered) {
            return true;
        }

        // 不完整的枚举匹配 - 报告错误
        reportMissing(joinNames(uncoveredVariants));
        return false;
    }

    // 对于 Option 类型,检查是否覆盖了 Some 和 None
    if (scrutineeBase->isOptional()) {
        bool hasSome = false;
        bool hasNone = false;
        std::function<void(Pattern*)> collectOptional = [&](Pattern* pat) {
            if (!pat) return;
            if (auto* enumPat = dynamic_cast<EnumPattern*>(pat)) {
                const std::string& variantName = enumPat->getVariantName();
                if (variantName == "Some") {
                    hasSome = true;
                } else if (variantName == "None") {
                    hasNone = true;
                }
                return;
            }
            if (auto* litPat = dynamic_cast<LiteralPattern*>(pat)) {
                if (dynamic_cast<NoneLiteralExpr*>(litPat->getLiteral())) {
                    hasNone = true;
                }
                return;
            }
            if (auto* orPat = dynamic_cast<OrPattern*>(pat)) {
                for (auto* alt : orPat->getPatterns()) {
                    collectOptional(alt);
                }
                return;
            }
            if (auto* bindPat = dynamic_cast<BindPattern*>(pat)) {
                collectOptional(bindPat->getInner());
                return;
            }
        };

        for (const auto& arm : arms) {
            if (!arm.Pat || arm.Guard) continue;
            collectOptional(arm.Pat);
        }

        if (hasSome && hasNone) {
            return true;
        }

        // 不完整的 Option 匹配 - 报告错误
        std::vector<std::string> missing;
        if (!hasSome) missing.push_back("Some");
        if (!hasNone) missing.push_back("None");
        reportMissing(joinNames(missing));
        return false;
    }

    // 对于整数类型,原则上无法穷尽(除非有范围约束)
    // 因此必须有通配符或标识符模式
    if (scrutineeBase->isInteger()) {
        // 已经检查过通配符和标识符,如果没有则不穷尽
        reportMissing("wildcard or identifier pattern");
        return false;
    }

    // 对于其他类型,如果没有通配符/标识符模式,要求有显式的默认分支
    // 这是保守的做法,避免运行时错误
    reportMissing("wildcard or identifier pattern");
    return false;
}

// ============================================================================
// Trait 检查（占位符）
// ============================================================================

bool Sema::checkTraitImpl(ImplDecl* impl) {
    if (!impl || !impl->isTraitImpl()) {
        return true; // 固有实现不需要检查
    }

    Symbol* traitSymbol = Symbols.lookup(impl->getTraitName());
    if (!traitSymbol || traitSymbol->getKind() != SymbolKind::Trait) {
        return false;
    }

    auto* traitDecl = static_cast<TraitDecl*>(traitSymbol->getDecl());
    bool success = true;

    std::unordered_map<std::string, Type*> traitSubst;
    if (traitDecl && traitDecl->isGeneric()) {
        const auto& params = traitDecl->getGenericParams();
        const auto& args = impl->getTraitTypeArgs();
        if (params.size() != args.size()) {
            Diag.report(DiagID::err_generic_param_count_mismatch, impl->getBeginLoc(), impl->getRange())
                << static_cast<unsigned>(params.size())
                << static_cast<unsigned>(args.size());
            success = false;
        } else {
            for (size_t i = 0; i < params.size(); ++i) {
                Type* argType = resolveType(args[i]);
                if (!argType) {
                    success = false;
                    continue;
                }
                traitSubst[params[i].Name] = argType;
            }
        }
    }

    for (FuncDecl* traitMethod : traitDecl->getMethods()) {
        FuncDecl* implMethod = impl->findMethod(traitMethod->getName());
        if (!implMethod) {
            if (!traitMethod->hasBody()) {
                Diag.report(DiagID::err_missing_trait_method, impl->getBeginLoc(), impl->getRange())
                    << traitMethod->getName();
                Diag.report(DiagID::note_declared_here, traitMethod->getBeginLoc(),
                            DiagnosticLevel::Note)
                    << traitMethod->getName();
                success = false;
            }
            continue;
        }

        if (!checkMethodSignatureMatch(
                traitMethod, implMethod, impl,
                traitSubst.empty() ? nullptr : &traitSubst)) {
            success = false;
        }
    }

    for (TypeAliasDecl* traitAssocType : traitDecl->getAssociatedTypes()) {
        bool found = false;
        for (TypeAliasDecl* implAssocType : impl->getAssociatedTypes()) {
            if (implAssocType->getName() == traitAssocType->getName()) {
                found = true;
                break;
            }
        }

        if (!found) {
            reportError(DiagID::err_expected_type, impl->getBeginLoc());
            Diag.report(DiagID::note_declared_here, traitAssocType->getBeginLoc(),
                        DiagnosticLevel::Note)
                << traitAssocType->getName();
            success = false;
        }
    }

    for (FuncDecl* implMethod : impl->getMethods()) {
        FuncDecl* traitMethod = traitDecl->findMethod(implMethod->getName());
        if (!traitMethod) {
            Diag.report(DiagID::err_function_not_found, implMethod->getBeginLoc(), implMethod->getRange())
                << implMethod->getName();
            success = false;
        }
    }

    return success;
}

bool Sema::checkMethodSignatureMatch(FuncDecl* traitMethod, FuncDecl* implMethod,
                                     ImplDecl* impl,
                                     const std::unordered_map<std::string, Type*>* traitSubst) {
    if (!traitMethod || !implMethod || !impl) {
        return false;
    }

    Type* targetType = impl->getSemanticTargetType();
    if (!targetType && impl->getTargetType()) {
        targetType = resolveType(impl->getTargetType());
    }
    if (!targetType) {
        return false;
    }

    auto* traitFn = traitMethod->getSemanticType() && traitMethod->getSemanticType()->isFunction()
                        ? static_cast<FunctionType*>(traitMethod->getSemanticType())
                        : nullptr;
    auto* implFn = implMethod->getSemanticType() && implMethod->getSemanticType()->isFunction()
                       ? static_cast<FunctionType*>(implMethod->getSemanticType())
                       : nullptr;
    if (!traitFn || !implFn) {
        return false;
    }

    bool success = true;
    auto paramKindName = [](ParamDecl::ParamKind kind) -> const char* {
        switch (kind) {
            case ParamDecl::ParamKind::Normal:
                return "param";
            case ParamDecl::ParamKind::Self:
                return "self";
            case ParamDecl::ParamKind::RefSelf:
                return "&self";
            case ParamDecl::ParamKind::MutRefSelf:
                return "&mut self";
            case ParamDecl::ParamKind::Variadic:
                return "variadic";
        }
        return "param";
    };

    auto replaceTraitSelf = [&](auto&& self, Type* ty) -> Type* {
        if (!ty) {
            return nullptr;
        }
        if (ty->isTrait()) {
            auto* traitTy = static_cast<TraitType*>(ty);
            if (traitTy->getName() == impl->getTraitName()) {
                return targetType;
            }
            return ty;
        }
        if (ty->isReference()) {
            auto* refTy = static_cast<ReferenceType*>(ty);
            Type* replaced = self(self, refTy->getPointeeType());
            return replaced ? Ctx.getReferenceType(replaced, refTy->isMutable()) : nullptr;
        }
        if (ty->isPointer()) {
            auto* ptrTy = static_cast<PointerType*>(ty);
            Type* replaced = self(self, ptrTy->getPointeeType());
            return replaced ? Ctx.getPointerType(replaced, ptrTy->isMutable()) : nullptr;
        }
        if (ty->isOptional()) {
            auto* optTy = static_cast<OptionalType*>(ty);
            Type* replaced = self(self, optTy->getInnerType());
            return replaced ? Ctx.getOptionalType(replaced) : nullptr;
        }
        if (ty->isArray()) {
            auto* arrTy = static_cast<ArrayType*>(ty);
            Type* replaced = self(self, arrTy->getElementType());
            return replaced ? Ctx.getArrayType(replaced, arrTy->getSize()) : nullptr;
        }
        if (ty->isSlice()) {
            auto* sliceTy = static_cast<SliceType*>(ty);
            Type* replaced = self(self, sliceTy->getElementType());
            return replaced ? Ctx.getSliceType(replaced, sliceTy->isMutable()) : nullptr;
        }
        if (ty->isTuple()) {
            auto* tupleTy = static_cast<TupleType*>(ty);
            std::vector<Type*> elems;
            elems.reserve(tupleTy->getElementCount());
            for (size_t i = 0; i < tupleTy->getElementCount(); ++i) {
                Type* replaced = self(self, tupleTy->getElement(i));
                if (!replaced) {
                    return nullptr;
                }
                elems.push_back(replaced);
            }
            return Ctx.getTupleType(std::move(elems));
        }
        if (ty->isFunction()) {
            auto* fnTy = static_cast<FunctionType*>(ty);
            std::vector<Type*> params;
            params.reserve(fnTy->getParamCount());
            for (Type* paramTy : fnTy->getParamTypes()) {
                Type* replaced = self(self, paramTy);
                if (!replaced) {
                    return nullptr;
                }
                params.push_back(replaced);
            }
            Type* retTy = self(self, fnTy->getReturnType());
            if (!retTy) {
                return nullptr;
            }
            return Ctx.getFunctionType(std::move(params), retTy, fnTy->canError(), fnTy->isVariadic());
        }
        if (ty->isError()) {
            auto* errTy = static_cast<ErrorType*>(ty);
            Type* replaced = self(self, errTy->getSuccessType());
            return replaced ? Ctx.getErrorType(replaced) : nullptr;
        }
        if (ty->isRange()) {
            auto* rangeTy = static_cast<RangeType*>(ty);
            Type* replaced = self(self, rangeTy->getElementType());
            return replaced ? Ctx.getRangeType(replaced, rangeTy->isInclusive()) : nullptr;
        }
        if (ty->isGenericInstance()) {
            auto* inst = static_cast<GenericInstanceType*>(ty);
            std::vector<Type*> args;
            args.reserve(inst->getTypeArgCount());
            for (Type* argTy : inst->getTypeArgs()) {
                Type* replaced = self(self, argTy);
                if (!replaced) {
                    return nullptr;
                }
                args.push_back(replaced);
            }
            return Ctx.getGenericInstanceType(inst->getBaseType(), std::move(args));
        }
        return ty;
    };

    auto prepareExpected = [&](Type* traitTy) -> Type* {
        Type* expected = traitTy;
        if (traitSubst && !traitSubst->empty()) {
            expected = substituteType(expected, *traitSubst);
        }
        return replaceTraitSelf(replaceTraitSelf, expected);
    };

    if (traitMethod->getParams().size() != implMethod->getParams().size()) {
        Diag.report(DiagID::err_wrong_argument_count, implMethod->getBeginLoc(), implMethod->getRange())
            << static_cast<unsigned>(traitMethod->getParams().size())
            << static_cast<unsigned>(implMethod->getParams().size());
        Diag.report(DiagID::note_declared_here, traitMethod->getBeginLoc(),
                    DiagnosticLevel::Note)
            << traitMethod->getName();
        return false;
    }

    for (size_t i = 0; i < traitMethod->getParams().size(); ++i) {
        ParamDecl* traitParam = traitMethod->getParams()[i];
        ParamDecl* implParam = implMethod->getParams()[i];

        if (traitParam->isSelf() && implParam->isSelf()) {
            if (traitParam->getParamKind() != implParam->getParamKind()) {
                Diag.report(DiagID::err_type_mismatch, implParam->getBeginLoc(), implParam->getRange())
                    << paramKindName(traitParam->getParamKind())
                    << paramKindName(implParam->getParamKind());
                Diag.report(DiagID::note_declared_here, traitParam->getBeginLoc(),
                            DiagnosticLevel::Note)
                    << traitMethod->getName();
                success = false;
            }
            continue;
        } else if (traitParam->isSelf() || implParam->isSelf()) {
            Diag.report(DiagID::err_type_mismatch, implParam->getBeginLoc(), implParam->getRange())
                << paramKindName(traitParam->getParamKind())
                << paramKindName(implParam->getParamKind());
            Diag.report(DiagID::note_declared_here, traitParam->getBeginLoc(),
                        DiagnosticLevel::Note)
                << traitMethod->getName();
            success = false;
            continue;
        }

        Type* expectedType = i < traitFn->getParamCount() ? prepareExpected(traitFn->getParam(i)) : nullptr;
        Type* actualType = i < implFn->getParamCount() ? implFn->getParam(i) : nullptr;
        if (!expectedType || !actualType || !expectedType->isEqual(actualType)) {
            Diag.report(DiagID::err_type_mismatch, implParam->getBeginLoc(), implParam->getRange())
                << (expectedType ? expectedType->toString() : "<?>")
                << (actualType ? actualType->toString() : "<?>");
            Diag.report(DiagID::note_declared_here, traitParam->getBeginLoc(),
                        DiagnosticLevel::Note)
                << traitMethod->getName();
            success = false;
        }
    }

    Type* expectedReturnType = prepareExpected(traitFn->getReturnType());
    Type* actualReturnType = implFn->getReturnType();
    if (!expectedReturnType || !actualReturnType || !expectedReturnType->isEqual(actualReturnType)) {
        Diag.report(DiagID::err_return_type_mismatch, implMethod->getBeginLoc(), implMethod->getRange())
            << (expectedReturnType ? expectedReturnType->toString() : "<?>")
            << (actualReturnType ? actualReturnType->toString() : "<?>");
        Diag.report(DiagID::note_declared_here, traitMethod->getBeginLoc(),
                    DiagnosticLevel::Note)
            << traitMethod->getName();
        success = false;
    }

    if (traitMethod->canError() != implMethod->canError()) {
        Diag.report(DiagID::err_return_type_mismatch, implMethod->getBeginLoc(), implMethod->getRange())
            << (traitMethod->canError() ? "error" : "non-error")
            << (implMethod->canError() ? "error" : "non-error");
        Diag.report(DiagID::note_declared_here, traitMethod->getBeginLoc(),
                    DiagnosticLevel::Note)
            << traitMethod->getName();
        success = false;
    }

    return success;
}

bool Sema::checkGenericBoundsSatisfied(
    const std::vector<GenericParam>& params,
    const std::unordered_map<std::string, Type*>& mapping) const {
    for (const auto& param : params) {
        if (param.Bounds.empty()) {
            continue;
        }

        auto it = mapping.find(param.Name);
        if (it == mapping.end() || !it->second) {
            return false;
        }

        Type* actualType = it->second;
        Type* normalizedType = actualType;
        while (normalizedType && normalizedType->isReference()) {
            normalizedType = static_cast<ReferenceType*>(normalizedType)->getPointeeType();
        }
        while (normalizedType && normalizedType->isPointer()) {
            normalizedType = static_cast<PointerType*>(normalizedType)->getPointeeType();
        }
        while (normalizedType && normalizedType->isTypeAlias()) {
            normalizedType = static_cast<TypeAlias*>(normalizedType)->getAliasedType();
        }

        for (const std::string& bound : param.Bounds) {
            if (bound == "Copy") {
                if (!const_cast<Sema*>(this)->isCopyType(actualType)) {
                    return false;
                }
                continue;
            }
            if (bound == "Drop") {
                if (!const_cast<Sema*>(this)->needsDrop(actualType)) {
                    return false;
                }
                continue;
            }

            Symbol* traitSymbol = Symbols.lookup(bound);
            if (!traitSymbol || traitSymbol->getKind() != SymbolKind::Trait) {
                return false;
            }

            auto* traitDecl = dynamic_cast<TraitDecl*>(traitSymbol->getDecl());
            if (!traitDecl || !const_cast<Sema*>(this)->checkTraitBound(normalizedType, traitDecl)) {
                return false;
            }
        }
    }
    return true;
}

bool Sema::resolveImplCandidate(Type* actualType,
                                TraitDecl* trait,
                                std::unordered_map<std::string, Type*>& mapping,
                                ImplDecl** matchedImpl) {
    if (!actualType) {
        return false;
    }

    auto normalize = [](Type* type) -> Type* {
        Type* current = type;
        while (current && current->isTypeAlias()) {
            current = static_cast<TypeAlias*>(current)->getAliasedType();
        }
        while (current && current->isReference()) {
            current = static_cast<ReferenceType*>(current)->getPointeeType();
        }
        while (current && current->isPointer()) {
            current = static_cast<PointerType*>(current)->getPointeeType();
        }
        return current;
    };

    Type* normalizedActual = normalize(actualType);
    if (!normalizedActual) {
        return false;
    }

    for (auto it = ImplCandidates.rbegin(); it != ImplCandidates.rend(); ++it) {
        const ImplCandidate& candidate = *it;
        if (!candidate.Decl || !candidate.TargetPattern) {
            continue;
        }

        if (trait) {
            if (!candidate.Trait) {
                continue;
            }
            if (candidate.Trait != trait &&
                candidate.Trait->getName() != trait->getName()) {
                continue;
            }
        } else if (candidate.Trait) {
            continue;
        }

        std::unordered_map<std::string, Type*> localMapping;
        if (!unifyGenericTypes(candidate.TargetPattern, normalizedActual, localMapping)) {
            continue;
        }
        if (!checkGenericBoundsSatisfied(candidate.GenericParams, localMapping)) {
            continue;
        }

        mapping = std::move(localMapping);
        if (matchedImpl) {
            *matchedImpl = candidate.Decl;
        }
        return true;
    }

    return false;
}

FuncDecl* Sema::resolveImplMethod(Type* actualType,
                                  const std::string& methodName,
                                  std::unordered_map<std::string, Type*>* mapping,
                                  ImplDecl** matchedImpl,
                                  bool includeTraitImpl) {
    if (!actualType) {
        return nullptr;
    }

    auto normalize = [](Type* ty) -> Type* {
        Type* current = ty;
        while (current && current->isTypeAlias()) {
            current = static_cast<TypeAlias*>(current)->getAliasedType();
        }
        while (current && current->isReference()) {
            current = static_cast<ReferenceType*>(current)->getPointeeType();
        }
        while (current && current->isPointer()) {
            current = static_cast<PointerType*>(current)->getPointeeType();
        }
        return current;
    };
    Type* normalizedActual = normalize(actualType);
    if (!normalizedActual) {
        return nullptr;
    }

    auto collectPatternGenerics = [&](auto&& self, Type* pattern,
                                      std::unordered_map<std::string, Type*>& out) -> void {
        if (!pattern) {
            return;
        }
        while (pattern->isTypeAlias()) {
            pattern = static_cast<TypeAlias*>(pattern)->getAliasedType();
            if (!pattern) {
                return;
            }
        }

        if (pattern->isGeneric()) {
            auto* genericTy = static_cast<GenericType*>(pattern);
            out.emplace(genericTy->getName(), pattern);
            return;
        }
        if (pattern->isGenericInstance()) {
            auto* inst = static_cast<GenericInstanceType*>(pattern);
            self(self, inst->getBaseType(), out);
            for (Type* arg : inst->getTypeArgs()) {
                self(self, arg, out);
            }
            return;
        }
        if (pattern->isReference()) {
            self(self, static_cast<ReferenceType*>(pattern)->getPointeeType(), out);
            return;
        }
        if (pattern->isPointer()) {
            self(self, static_cast<PointerType*>(pattern)->getPointeeType(), out);
            return;
        }
        if (pattern->isOptional()) {
            self(self, static_cast<OptionalType*>(pattern)->getInnerType(), out);
            return;
        }
        if (pattern->isArray()) {
            self(self, static_cast<ArrayType*>(pattern)->getElementType(), out);
            return;
        }
        if (pattern->isSlice()) {
            self(self, static_cast<SliceType*>(pattern)->getElementType(), out);
            return;
        }
        if (pattern->isTuple()) {
            auto* tupleTy = static_cast<TupleType*>(pattern);
            for (size_t i = 0; i < tupleTy->getElementCount(); ++i) {
                self(self, tupleTy->getElement(i), out);
            }
            return;
        }
        if (pattern->isFunction()) {
            auto* fnTy = static_cast<FunctionType*>(pattern);
            for (size_t i = 0; i < fnTy->getParamCount(); ++i) {
                self(self, fnTy->getParam(i), out);
            }
            self(self, fnTy->getReturnType(), out);
            return;
        }
        if (pattern->isError()) {
            self(self, static_cast<ErrorType*>(pattern)->getSuccessType(), out);
            return;
        }
        if (pattern->isRange()) {
            self(self, static_cast<RangeType*>(pattern)->getElementType(), out);
            return;
        }
    };

    auto tryResolve = [&](bool traitImpl) -> FuncDecl* {
        for (auto it = ImplCandidates.rbegin(); it != ImplCandidates.rend(); ++it) {
            const ImplCandidate& candidate = *it;
            if (!candidate.Decl || !candidate.TargetPattern) {
                continue;
            }
            if ((candidate.Trait != nullptr) != traitImpl) {
                continue;
            }

            std::unordered_map<std::string, Type*> localMapping;
            if (!unifyGenericTypes(candidate.TargetPattern, normalizedActual, localMapping)) {
                Type* candidateTarget = candidate.TargetPattern;
                while (candidateTarget && candidateTarget->isTypeAlias()) {
                    candidateTarget = static_cast<TypeAlias*>(candidateTarget)->getAliasedType();
                }

                bool relaxedMatch = false;
                if (candidateTarget && candidateTarget->isGenericInstance() &&
                    !normalizedActual->isGenericInstance()) {
                    auto* patInst = static_cast<GenericInstanceType*>(candidateTarget);
                    Type* patBase = patInst->getBaseType();
                    while (patBase && patBase->isTypeAlias()) {
                        patBase = static_cast<TypeAlias*>(patBase)->getAliasedType();
                    }
                    if (patBase && patBase->isEqual(normalizedActual)) {
                        collectPatternGenerics(collectPatternGenerics, candidate.TargetPattern, localMapping);
                        relaxedMatch = true;
                    }
                }
                if (!relaxedMatch) {
                    continue;
                }
            }
            if (!checkGenericBoundsSatisfied(candidate.GenericParams, localMapping)) {
                continue;
            }

            FuncDecl* method = candidate.Decl->findMethod(methodName);
            if (!method) {
                continue;
            }

            if (mapping) {
                *mapping = std::move(localMapping);
            }
            if (matchedImpl) {
                *matchedImpl = candidate.Decl;
            }
            return method;
        }
        return nullptr;
    };

    if (FuncDecl* method = tryResolve(false)) {
        return method;
    }
    if (includeTraitImpl) {
        return tryResolve(true);
    }
    return nullptr;
}

bool Sema::checkTraitBound(Type* type, TraitDecl* trait) {
    if (!type || !trait) {
        return false;
    }

    const std::string& traitName = trait->getName();

    auto normalize = [](Type* ty) -> Type* {
        Type* current = ty;
        while (current && current->isTypeAlias()) {
            current = static_cast<TypeAlias*>(current)->getAliasedType();
        }
        while (current && current->isReference()) {
            current = static_cast<ReferenceType*>(current)->getPointeeType();
        }
        while (current && current->isPointer()) {
            current = static_cast<PointerType*>(current)->getPointeeType();
        }
        return current;
    };

    Type* normalized = normalize(type);
    if (!normalized) {
        return false;
    }

    if (traitName == "Copy") {
        return isCopyType(normalized);
    }
    if (traitName == "Drop") {
        if (normalized->isGeneric()) {
            auto* genericType = static_cast<GenericType*>(normalized);
            for (TraitType* constraint : genericType->getConstraints()) {
                if (constraint && constraint->getName() == "Drop") {
                    return true;
                }
            }
        }
        return needsDrop(normalized);
    }
    if ((traitName == "Eq" || traitName == "Ne") && isBuiltinComparisonType(normalized)) {
        return true;
    }

    struct RecursionGuard {
        std::unordered_set<uintptr_t>& Set;
        uintptr_t Key;
        bool Active = false;
        RecursionGuard(std::unordered_set<uintptr_t>& set, uintptr_t key)
            : Set(set), Key(key) {
            auto [it, inserted] = Set.insert(Key);
            Active = inserted;
            (void)it;
        }
        ~RecursionGuard() {
            if (Active) {
                Set.erase(Key);
            }
        }
    };
    static thread_local std::unordered_set<uintptr_t> InProgress;
    uintptr_t recursionKey = reinterpret_cast<uintptr_t>(normalized) ^
                             (reinterpret_cast<uintptr_t>(trait) << 1);
    RecursionGuard guard(InProgress, recursionKey);
    if (!guard.Active) {
        return false;
    }

    if (normalized->isGeneric()) {
        auto* genericType = static_cast<GenericType*>(normalized);
        for (TraitType* constraint : genericType->getConstraints()) {
            if (constraint && constraint->getName() == traitName) {
                return true;
            }
        }
    }

    std::unordered_map<std::string, Type*> mapping;
    if (resolveImplCandidate(normalized, trait, mapping, nullptr)) {
        return true;
    }

    auto it = ImplTraitMap.find(normalized);
    if (it != ImplTraitMap.end() && it->second.find(traitName) != it->second.end()) {
        return true;
    }
    if (normalized->isGenericInstance()) {
        auto* genInst = static_cast<GenericInstanceType*>(normalized);
        const Type* baseType = genInst->getBaseType();
        auto bit = ImplTraitMap.find(baseType);
        if (bit != ImplTraitMap.end() && bit->second.find(traitName) != bit->second.end()) {
            return true;
        }
    }

    return false;
}

// ============================================================================
// 错误处理检查（占位符）
// ============================================================================

bool Sema::checkErrorHandling(FuncDecl* func) {
    if (!func) {
        return true;
    }

    // 检查函数是否可能返回错误
    bool canError = func->canError();

    // 如果函数不能返回错误，检查函数体中是否有未处理的错误传播
    if (!canError) {
        // 遍历函数体检查是否有 ! 操作符
        // 如果有，需要确保错误被捕获（-> err {}）
        // 这个检查在 analyzeErrorPropagateExpr 中已经实现
    }

    // 如果函数可以返回错误，检查是否所有路径都正确处理了错误
    // 这个检查比较复杂，需要进行控制流分析
    // 目前只做基本检查

    return true;
}

bool Sema::checkUnusedResult(Expr* expr, SourceLocation loc) {
    if (!expr) {
        return true;
    }

    // 获取表达式的类型
    Type* exprType = expr->getType();
    if (!exprType) {
        return true; // 如果没有类型信息，不进行检查
    }

    // void 类型的返回值不需要使用
    if (exprType->isVoid()) {
        return true;
    }

    // 检查是否是函数调用表达式
    if (auto* callExpr = dynamic_cast<CallExpr*>(expr)) {
        // 如果调用的是有返回值的函数，发出警告
        Type* calleeType = callExpr->getCallee()->getType();
        if (calleeType && calleeType->isFunction()) {
            auto* funcType = static_cast<FunctionType*>(calleeType);
            Type* returnType = funcType->getReturnType();
            if (returnType && !returnType->isVoid()) {
                // 警告：函数返回值未使用
                reportWarning(DiagID::warn_unused_result, loc);
                return false;
            }
        }
    }

    // 检查内置函数调用
    if (auto* builtinExpr = dynamic_cast<BuiltinCallExpr*>(expr)) {
        // 某些内置函数的返回值应该被使用
        BuiltinKind kind = builtinExpr->getBuiltinKind();
        if (kind == BuiltinKind::Sizeof ||
            kind == BuiltinKind::Alignof ||
            kind == BuiltinKind::Typeof) {
            reportWarning(DiagID::warn_unused_result, loc);
            return false;
        }
    }

    if (auto* awaitExpr = dynamic_cast<AwaitExpr*>(expr)) {
        return checkUnusedResult(awaitExpr->getInner(), loc);
    }

    // 赋值表达式的结果通常不需要使用
    if (dynamic_cast<AssignExpr*>(expr)) {
        return true;
    }

    return true;
}

// ============================================================================
// 类型解析辅助方法实现
// ============================================================================

Type* Sema::resolveBuiltinType(BuiltinTypeNode* node) {
    if (!node) {
        return nullptr;
    }

    switch (node->getBuiltinKind()) {
        case BuiltinTypeNode::BuiltinKind::Void:
            return Ctx.getVoidType();
        case BuiltinTypeNode::BuiltinKind::Bool:
            return Ctx.getBoolType();
        case BuiltinTypeNode::BuiltinKind::Char:
            return Ctx.getCharType();
        case BuiltinTypeNode::BuiltinKind::Str:
            return Ctx.getStrType();
        case BuiltinTypeNode::BuiltinKind::I8:
            return Ctx.getI8Type();
        case BuiltinTypeNode::BuiltinKind::I16:
            return Ctx.getI16Type();
        case BuiltinTypeNode::BuiltinKind::I32:
            return Ctx.getI32Type();
        case BuiltinTypeNode::BuiltinKind::I64:
            return Ctx.getI64Type();
        case BuiltinTypeNode::BuiltinKind::I128:
            return Ctx.getIntegerType(128, true);
        case BuiltinTypeNode::BuiltinKind::ISize:
            return Ctx.getIntegerType(Ctx.getPointerBitWidth(), true);
        case BuiltinTypeNode::BuiltinKind::U8:
            return Ctx.getU8Type();
        case BuiltinTypeNode::BuiltinKind::U16:
            return Ctx.getU16Type();
        case BuiltinTypeNode::BuiltinKind::U32:
            return Ctx.getU32Type();
        case BuiltinTypeNode::BuiltinKind::U64:
            return Ctx.getU64Type();
        case BuiltinTypeNode::BuiltinKind::U128:
            return Ctx.getIntegerType(128, false);
        case BuiltinTypeNode::BuiltinKind::USize:
            return Ctx.getIntegerType(Ctx.getPointerBitWidth(), false);
        case BuiltinTypeNode::BuiltinKind::F32:
            return Ctx.getF32Type();
        case BuiltinTypeNode::BuiltinKind::F64:
            return Ctx.getF64Type();
        default:
            reportError(DiagID::err_expected_type, node->getBeginLoc());
            return nullptr;
    }
}

Type* Sema::resolveIdentifierType(IdentifierTypeNode* node) {
    if (!node) {
        return nullptr;
    }

    auto isTypeLike = [](Type* type) -> bool {
        if (!type) {
            return false;
        }
        if (type->isGenericInstance()) {
            type = static_cast<GenericInstanceType*>(type)->getBaseType();
        }
        return type->isStruct() || type->isEnum() || type->isTrait();
    };

    auto isTypeDeclKind = [](ASTNode::Kind kind) -> bool {
        return kind == ASTNode::Kind::StructDecl ||
               kind == ASTNode::Kind::EnumDecl ||
               kind == ASTNode::Kind::TraitDecl ||
               kind == ASTNode::Kind::TypeAliasDecl;
    };

    auto resolveTypeDeclFromConstInit = [&](Symbol* sym) -> Decl* {
        if (!sym || sym->getKind() != SymbolKind::Constant) {
            return nullptr;
        }
        auto* constDecl = dynamic_cast<ConstDecl*>(sym->getDecl());
        if (!constDecl || !constDecl->getInit()) {
            return nullptr;
        }

        Decl* targetDecl = nullptr;
        if (auto* ident = dynamic_cast<IdentifierExpr*>(constDecl->getInit())) {
            targetDecl = ident->getResolvedDecl();
        } else if (auto* member = dynamic_cast<MemberExpr*>(constDecl->getInit())) {
            targetDecl = member->getResolvedDecl();
        }

        if (targetDecl && isTypeDeclKind(targetDecl->getKind())) {
            return targetDecl;
        }
        return nullptr;
    };

    auto resolveQualifiedType = [&](const std::string& qualifiedName,
                                    Type*& outType,
                                    Decl*& outDecl) -> bool {
        outType = nullptr;
        outDecl = nullptr;

        size_t dotPos = qualifiedName.find('.');
        if (dotPos == std::string::npos) {
            return false;
        }

        std::string rootName = qualifiedName.substr(0, dotPos);
        Symbol* rootSymbol = Symbols.lookup(rootName);
        if (!rootSymbol) {
            return false;
        }

        Type* currentType = rootSymbol->getType();
        Decl* currentDecl = rootSymbol->getDecl();
        size_t cursor = dotPos + 1;

        while (true) {
            size_t nextDot = qualifiedName.find('.', cursor);
            std::string part = qualifiedName.substr(
                cursor,
                nextDot == std::string::npos ? std::string::npos : (nextDot - cursor));
            if (part.empty() || !currentType || !currentType->isModule()) {
                return false;
            }

            auto* moduleType = static_cast<ModuleType*>(currentType);
            const ModuleType::Member* member = moduleType->getMember(part);
            if (!member) {
                return false;
            }

            currentType = member->MemberType;
            currentDecl = static_cast<Decl*>(member->Decl);

            if (nextDot == std::string::npos) {
                break;
            }
            cursor = nextDot + 1;
        }

        outType = currentType;
        outDecl = currentDecl;
        return true;
    };

    const std::string& typeName = node->getName();
    Symbol* symbol = Symbols.lookup(typeName);
    Type* resolvedType = nullptr;
    Decl* resolvedDecl = nullptr;

    if (symbol) {
        resolvedType = symbol->getType();
        resolvedDecl = symbol->getDecl();
    } else if (!resolveQualifiedType(typeName, resolvedType, resolvedDecl)) {
        Diag.report(DiagID::err_undeclared_identifier, node->getBeginLoc(), node->getRange())
            << typeName;
        return nullptr;
    }

    if (symbol && symbol->getKind() == SymbolKind::GenericParam) {
        return resolvedType;
    }

    if (symbol) {
        if (symbol->getKind() == SymbolKind::Struct ||
            symbol->getKind() == SymbolKind::Enum ||
            symbol->getKind() == SymbolKind::Trait ||
            symbol->getKind() == SymbolKind::TypeAlias) {
            return resolvedType;
        }
        if (symbol->getKind() == SymbolKind::Constant) {
            if (resolveTypeDeclFromConstInit(symbol) || isTypeLike(resolvedType) ||
                (resolvedType && resolvedType->isTypeAlias())) {
                return resolvedType;
            }
        }
    } else {
        if ((resolvedDecl && isTypeDeclKind(resolvedDecl->getKind())) || isTypeLike(resolvedType)) {
            return resolvedType;
        }
    }

    reportError(DiagID::err_expected_type, node->getBeginLoc());
    return nullptr;
}

Type* Sema::resolveArrayType(ArrayTypeNode* node) {
    if (!node) {
        return nullptr;
    }

    // 解析元素类型
    Type* elementType = resolveType(node->getElementType());
    if (!elementType) {
        return nullptr;
    }

    // 分析大小表达式
    Type* sizeType = analyzeExpr(node->getSize());
    if (!sizeType) {
        return nullptr;
    }

    // 检查大小表达式是否为整数类型
    if (!sizeType->isInteger()) {
        Diag.report(DiagID::err_type_mismatch, node->getSize()->getBeginLoc(),
                    node->getSize()->getRange())
            << "integer"
            << sizeType->toString();
        return nullptr;
    }

    // 计算数组大小的常量值
    int64_t arraySizeValue;
    if (!evaluateConstExpr(node->getSize(), arraySizeValue)) {
        // 无法求值为常量
        Diag.report(DiagID::err_invalid_array_size, node->getSize()->getBeginLoc(),
                    node->getSize()->getRange());
        return nullptr;
    }

    // 检查数组大小是否为正数
    if (arraySizeValue <= 0) {
        Diag.report(DiagID::err_invalid_array_size, node->getSize()->getBeginLoc(),
                    node->getSize()->getRange());
        return nullptr;
    }

    uint64_t arraySize = static_cast<uint64_t>(arraySizeValue);

    return Ctx.getArrayType(elementType, arraySize);
}

Type* Sema::resolveSliceType(SliceTypeNode* node) {
    if (!node) {
        return nullptr;
    }

    // 解析元素类型
    Type* elementType = resolveType(node->getElementType());
    if (!elementType) {
        return nullptr;
    }

    return Ctx.getSliceType(elementType, node->isMutable());
}

Type* Sema::resolveTupleType(TupleTypeNode* node) {
    if (!node) {
        return nullptr;
    }

    std::vector<Type*> elementTypes;
    elementTypes.reserve(node->getElementCount());

    // 解析所有元素类型
    for (TypeNode* elementNode : node->getElements()) {
        Type* elementType = resolveType(elementNode);
        if (!elementType) {
            return nullptr;
        }
        elementTypes.push_back(elementType);
    }

    return Ctx.getTupleType(std::move(elementTypes));
}

Type* Sema::resolveOptionalType(OptionalTypeNode* node) {
    if (!node) {
        return nullptr;
    }

    // 解析内部类型
    Type* innerType = resolveType(node->getInnerType());
    if (!innerType) {
        return nullptr;
    }

    return Ctx.getOptionalType(innerType);
}

Type* Sema::resolveReferenceType(ReferenceTypeNode* node) {
    if (!node) {
        return nullptr;
    }

    // 解析被引用的类型
    Type* pointeeType = resolveType(node->getPointeeType());
    if (!pointeeType) {
        return nullptr;
    }

    return Ctx.getReferenceType(pointeeType, node->isMutable());
}

Type* Sema::resolvePointerType(PointerTypeNode* node) {
    if (!node) {
        return nullptr;
    }

    // 解析被指向的类型
    Type* pointeeType = resolveType(node->getPointeeType());
    if (!pointeeType) {
        return nullptr;
    }

    return Ctx.getPointerType(pointeeType, node->isMutable());
}

Type* Sema::resolveFunctionType(FunctionTypeNode* node) {
    if (!node) {
        return nullptr;
    }

    std::vector<Type*> paramTypes;
    paramTypes.reserve(node->getParamCount());

    // 解析所有参数类型
    for (TypeNode* paramNode : node->getParamTypes()) {
        Type* paramType = resolveType(paramNode);
        if (!paramType) {
            return nullptr;
        }
        paramTypes.push_back(paramType);
    }

    // 解析返回类型
    Type* returnType = resolveType(node->getReturnType());
    if (!returnType) {
        return nullptr;
    }

    return Ctx.getFunctionType(std::move(paramTypes), returnType, node->canError());
}

Type* Sema::resolveErrorType(ErrorTypeNode* node) {
    if (!node) {
        return nullptr;
    }

    // 解析成功时的类型
    Type* successType = resolveType(node->getSuccessType());
    if (!successType) {
        return nullptr;
    }

    return Ctx.getErrorType(successType);
}

Type* Sema::resolveGenericType(GenericTypeNode* node) {
    if (!node) {
        return nullptr;
    }

    // 1. 解析基础类型名称
    const std::string& baseName = node->getBaseName();

    auto isTypeLike = [](Type* type) -> bool {
        if (!type) {
            return false;
        }
        if (type->isGenericInstance()) {
            type = static_cast<GenericInstanceType*>(type)->getBaseType();
        }
        return type->isStruct() || type->isEnum() || type->isTrait();
    };

    auto resolveQualifiedType = [&](const std::string& qualifiedName,
                                    Type*& outType,
                                    Decl*& outDecl) -> bool {
        outType = nullptr;
        outDecl = nullptr;

        size_t dotPos = qualifiedName.find('.');
        if (dotPos == std::string::npos) {
            return false;
        }

        std::string rootName = qualifiedName.substr(0, dotPos);
        Symbol* rootSymbol = Symbols.lookup(rootName);
        if (!rootSymbol) {
            return false;
        }

        Type* currentType = rootSymbol->getType();
        Decl* currentDecl = rootSymbol->getDecl();
        size_t cursor = dotPos + 1;

        while (true) {
            size_t nextDot = qualifiedName.find('.', cursor);
            std::string part = qualifiedName.substr(
                cursor,
                nextDot == std::string::npos ? std::string::npos : (nextDot - cursor));
            if (part.empty() || !currentType || !currentType->isModule()) {
                return false;
            }

            auto* moduleType = static_cast<ModuleType*>(currentType);
            const ModuleType::Member* member = moduleType->getMember(part);
            if (!member) {
                return false;
            }

            currentType = member->MemberType;
            currentDecl = static_cast<Decl*>(member->Decl);

            if (nextDot == std::string::npos) {
                break;
            }
            cursor = nextDot + 1;
        }

        outType = currentType;
        outDecl = currentDecl;
        return true;
    };

    auto isTypeDeclKind = [](ASTNode::Kind kind) -> bool {
        return kind == ASTNode::Kind::StructDecl ||
               kind == ASTNode::Kind::EnumDecl ||
               kind == ASTNode::Kind::TypeAliasDecl;
    };

    auto resolveTypeDeclFromConstInit = [&](Symbol* sym) -> Decl* {
        if (!sym || sym->getKind() != SymbolKind::Constant) {
            return nullptr;
        }
        auto* constDecl = dynamic_cast<ConstDecl*>(sym->getDecl());
        if (!constDecl || !constDecl->getInit()) {
            return nullptr;
        }

        Decl* targetDecl = nullptr;
        if (auto* ident = dynamic_cast<IdentifierExpr*>(constDecl->getInit())) {
            targetDecl = ident->getResolvedDecl();
        } else if (auto* member = dynamic_cast<MemberExpr*>(constDecl->getInit())) {
            targetDecl = member->getResolvedDecl();
        }

        if (targetDecl && isTypeDeclKind(targetDecl->getKind())) {
            return targetDecl;
        }
        return nullptr;
    };

    // 2. 在符号表中查找基础类型（支持模块限定路径）
    Symbol* baseSymbol = Symbols.lookup(baseName);
    Type* baseType = nullptr;
    Decl* baseDecl = nullptr;
    if (baseSymbol) {
        baseType = baseSymbol->getType();
        baseDecl = baseSymbol->getDecl();
        if (baseSymbol->getKind() == SymbolKind::Constant) {
            if (Decl* typeDecl = resolveTypeDeclFromConstInit(baseSymbol)) {
                baseDecl = typeDecl;
            }
        }
    } else if (!resolveQualifiedType(baseName, baseType, baseDecl)) {
        Diag.report(DiagID::err_undeclared_identifier, node->getBeginLoc(), node->getRange())
            << baseName;
        return nullptr;
    }

    // 3. 验证是否为类型符号
    if (baseSymbol) {
        SymbolKind kind = baseSymbol->getKind();
        if (kind != SymbolKind::Struct &&
            kind != SymbolKind::Enum &&
            kind != SymbolKind::TypeAlias) {
            if (!(kind == SymbolKind::Constant &&
                  (isTypeLike(baseType) || (baseDecl && isTypeDeclKind(baseDecl->getKind()))))) {
                reportError(DiagID::err_expected_type, node->getBeginLoc());
                return nullptr;
            }
        }
    } else if (!isTypeLike(baseType) &&
               !(baseDecl && isTypeDeclKind(baseDecl->getKind()))) {
        reportError(DiagID::err_expected_type, node->getBeginLoc());
        return nullptr;
    }

    if (!baseType) {
        reportError(DiagID::err_expected_type, node->getBeginLoc());
        return nullptr;
    }

    // 4. 解析所有类型参数
    std::vector<Type*> typeArgs;
    typeArgs.reserve(node->getTypeArgCount());

    for (size_t i = 0; i < node->getTypeArgCount(); ++i) {
        Type* argType = resolveType(node->getTypeArgs()[i]);
        if (!argType) {
            // 类型参数解析失败，继续解析其他参数以报告所有错误
            typeArgs.push_back(nullptr);
        } else {
            typeArgs.push_back(argType);
        }
    }

    // 5. 检查是否所有类型参数都解析成功
    for (Type* arg : typeArgs) {
        if (!arg) {
            return nullptr; // 有参数解析失败
        }
    }

    // 6. 验证类型参数数量
    size_t expectedTypeArgs = 0;
    if (baseDecl) {
        switch (baseDecl->getKind()) {
            case ASTNode::Kind::StructDecl:
                expectedTypeArgs = static_cast<StructDecl*>(baseDecl)->getGenericParams().size();
                break;
            case ASTNode::Kind::EnumDecl:
                expectedTypeArgs = static_cast<EnumDecl*>(baseDecl)->getGenericParams().size();
                break;
            case ASTNode::Kind::TypeAliasDecl:
                expectedTypeArgs = static_cast<TypeAliasDecl*>(baseDecl)->getGenericParams().size();
                break;
            default:
                expectedTypeArgs = 0;
                break;
        }
    } else if (baseType->isGenericInstance()) {
        expectedTypeArgs = static_cast<GenericInstanceType*>(baseType)->getTypeArgCount();
    }

    size_t actualTypeArgs = typeArgs.size();
    if (expectedTypeArgs != actualTypeArgs) {
        Diag.report(DiagID::err_wrong_argument_count, node->getBeginLoc(), node->getRange())
            << static_cast<unsigned>(expectedTypeArgs)
            << static_cast<unsigned>(actualTypeArgs);
        return nullptr;
    }

    // 7. 检查泛型约束
    const std::vector<GenericParam>* genericParams = nullptr;
    if (baseDecl) {
        switch (baseDecl->getKind()) {
            case ASTNode::Kind::StructDecl:
                genericParams = &static_cast<StructDecl*>(baseDecl)->getGenericParams();
                break;
            case ASTNode::Kind::EnumDecl:
                genericParams = &static_cast<EnumDecl*>(baseDecl)->getGenericParams();
                break;
            case ASTNode::Kind::TypeAliasDecl:
                genericParams = &static_cast<TypeAliasDecl*>(baseDecl)->getGenericParams();
                break;
            default:
                break;
        }
    }

    if (genericParams && genericParams->size() == typeArgs.size()) {
        for (size_t i = 0; i < genericParams->size(); ++i) {
            const GenericParam& param = (*genericParams)[i];
            Type* argType = typeArgs[i];
            if (!argType || param.Bounds.empty()) {
                continue;
            }

            Type* normalizedArg = argType;
            while (normalizedArg && normalizedArg->isReference()) {
                normalizedArg = static_cast<ReferenceType*>(normalizedArg)->getPointeeType();
            }
            while (normalizedArg && normalizedArg->isPointer()) {
                normalizedArg = static_cast<PointerType*>(normalizedArg)->getPointeeType();
            }
            while (normalizedArg && normalizedArg->isTypeAlias()) {
                normalizedArg = static_cast<TypeAlias*>(normalizedArg)->getAliasedType();
            }

            for (const std::string& bound : param.Bounds) {
                if (bound == "Copy") {
                    if (!isCopyType(argType)) {
                        Diag.report(DiagID::err_type_not_copyable, node->getBeginLoc(), node->getRange())
                            << argType->toString();
                        return nullptr;
                    }
                    continue;
                }
                if (bound == "Drop") {
                    if (!needsDrop(argType)) {
                        Diag.report(DiagID::err_type_requires_drop_impl,
                                    node->getBeginLoc(), node->getRange())
                            << argType->toString();
                        return nullptr;
                    }
                    continue;
                }

                Symbol* traitSymbol = Symbols.lookup(bound);
                if (!traitSymbol || traitSymbol->getKind() != SymbolKind::Trait) {
                    Diag.report(DiagID::err_expected_trait_bound, node->getBeginLoc(), node->getRange());
                    return nullptr;
                }
                auto* traitDecl = dynamic_cast<TraitDecl*>(traitSymbol->getDecl());
                if (!traitDecl || !checkTraitBound(normalizedArg, traitDecl)) {
                    Diag.report(DiagID::err_missing_trait_method, node->getBeginLoc(), node->getRange())
                        << ("trait bound " + bound);
                    return nullptr;
                }
            }
        }
    }

    // 8. 创建泛型实例类型
    return Ctx.getGenericInstanceType(baseType, std::move(typeArgs));
}

// ============================================================================
// 常量求值
// ============================================================================

bool Sema::evaluateConstExpr(Expr* expr, int64_t& result) {
    return TypeCheckerImpl && TypeCheckerImpl->evaluateConstExpr(expr, result);
}

} // namespace yuan
