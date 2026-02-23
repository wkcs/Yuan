/// \file TypeChecker.cpp
/// \brief 类型检查实现。

#include "yuan/Sema/TypeChecker.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/Expr.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/Sema/Scope.h"
#include "yuan/Sema/Type.h"

namespace yuan {

Type* TypeChecker::unwrapAliases(Type* type) {
    Type* current = type;
    while (current && current->isTypeAlias()) {
        current = static_cast<TypeAlias*>(current)->getAliasedType();
    }
    return current;
}

namespace {

struct VisitingGuard {
    std::unordered_set<const Type*>& Visiting;
    const Type* Key = nullptr;
    bool Active = false;

    VisitingGuard(std::unordered_set<const Type*>& visiting, const Type* key)
        : Visiting(visiting), Key(key) {
        Active = Visiting.insert(Key).second;
    }

    ~VisitingGuard() {
        if (Active) {
            Visiting.erase(Key);
        }
    }
};

} // namespace

bool TypeChecker::checkTypeCompatible(Type* expected, Type* actual, SourceLocation loc) {
    return checkTypeCompatible(expected, actual, SourceRange(loc));
}

bool TypeChecker::checkTypeCompatible(Type* expected, Type* actual, SourceRange range) {
    if (!expected || !actual) {
        return false;
    }

    expected = unwrapAliases(expected);
    actual = unwrapAliases(actual);
    if (!expected || !actual) {
        return false;
    }

    if (expected->isEqual(actual)) {
        return true;
    }

    // 整数隐式宽化：
    // - 仅允许相同符号性
    // - 仅允许从窄位宽到宽位宽
    if (expected->isInteger() && actual->isInteger()) {
        auto* expectedInt = static_cast<IntegerType*>(expected);
        auto* actualInt = static_cast<IntegerType*>(actual);
        if (expectedInt->isSigned() == actualInt->isSigned() &&
            expectedInt->getBitWidth() > actualInt->getBitWidth()) {
            return true;
        }
    }

    // 为类方法调用场景提供隐式借用。
    if (expected->isReference() && !actual->isReference()) {
        auto* expectedRef = static_cast<ReferenceType*>(expected);
        Type* pointee = unwrapAliases(expectedRef->getPointeeType());
        if (pointee && pointee->isEqual(actual)) {
            return true;
        }
    }

    // 泛型实例兼容：允许实际类型参数里存在未推导项。
    if (expected->isGenericInstance() && actual->isGenericInstance()) {
        auto* expectedInst = static_cast<GenericInstanceType*>(expected);
        auto* actualInst = static_cast<GenericInstanceType*>(actual);
        if (expectedInst->getBaseType()->isEqual(actualInst->getBaseType()) &&
            expectedInst->getTypeArgCount() == actualInst->getTypeArgCount()) {
            for (size_t i = 0; i < expectedInst->getTypeArgCount(); ++i) {
                Type* expectedArg = expectedInst->getTypeArg(i);
                Type* actualArg = actualInst->getTypeArg(i);
                if (actualArg && (actualArg->isGeneric() || actualArg->isTypeVar())) {
                    continue;
                }
                if (!checkTypeCompatible(expectedArg, actualArg, range)) {
                    return false;
                }
            }
            return true;
        }
    }

    // 值位置自动解引用。
    if (!expected->isReference() && actual->isReference()) {
        Type* pointee = static_cast<ReferenceType*>(actual)->getPointeeType();
        return checkTypeCompatible(expected, pointee, range);
    }

    // 可选类型兼容规则。
    if (expected->isOptional() && actual->isOptional()) {
        auto* expectedOpt = static_cast<OptionalType*>(expected);
        auto* actualOpt = static_cast<OptionalType*>(actual);
        if (actualOpt->getInnerType()->isVoid()) {
            return true;
        }
        return checkTypeCompatible(expectedOpt->getInnerType(), actualOpt->getInnerType(), range);
    }

    if (expected->isOptional()) {
        Type* innerType = static_cast<OptionalType*>(expected)->getInnerType();
        if (innerType->isEqual(actual)) {
            return true;
        }
    }

    // 期望不可变引用时，可接受可变引用。
    if (expected->isReference() && actual->isReference()) {
        auto* expectedRef = static_cast<ReferenceType*>(expected);
        auto* actualRef = static_cast<ReferenceType*>(actual);
        if (!expectedRef->isMutable() && actualRef->isMutable()) {
            return expectedRef->getPointeeType()->isEqual(actualRef->getPointeeType());
        }
    }

    std::string expectedText = expected->toString();
    std::string actualText = actual->toString();
    if (expected->isTuple() || actual->isTuple()) {
        expectedText = "tuple " + expectedText;
        actualText = "tuple " + actualText;
    }
    Diag.report(DiagID::err_type_mismatch, range.getBegin(), range)
        << expectedText
        << actualText;
    return false;
}

bool TypeChecker::checkAssignable(Expr* target, SourceLocation loc) {
    if (!target) {
        return false;
    }

    if (!target->isLValue()) {
        Diag.report(DiagID::err_type_mismatch, loc, target->getRange())
            << "assignment target"
            << "<expression>";
        return false;
    }

    if (auto* indexExpr = dynamic_cast<IndexExpr*>(target)) {
        Type* baseType = indexExpr->getBase()->getType();
        if (baseType && baseType->isVarArgs()) {
            Diag.report(DiagID::err_cannot_assign_to_immutable, loc, target->getRange())
                << "<varargs element>";
            return false;
        }
    }

    return true;
}

bool TypeChecker::checkMutable(Expr* target, SourceLocation loc) {
    if (!target) {
        return false;
    }

    if (auto* identExpr = dynamic_cast<IdentifierExpr*>(target)) {
        Symbol* symbol = Symbols.lookup(identExpr->getName());
        if (symbol && !symbol->isMutable()) {
            if (symbol->getKind() == SymbolKind::Constant) {
                Diag.report(DiagID::err_cannot_assign_to_const, loc, target->getRange())
                    << identExpr->getName();
            } else {
                Diag.report(DiagID::err_cannot_assign_to_immutable, loc, target->getRange())
                    << identExpr->getName();
            }
            return false;
        }
    }

    if (auto* memberExpr = dynamic_cast<MemberExpr*>(target)) {
        return checkMutable(memberExpr->getBase(), loc);
    }

    if (auto* indexExpr = dynamic_cast<IndexExpr*>(target)) {
        return checkMutable(indexExpr->getBase(), loc);
    }

    if (auto* unaryExpr = dynamic_cast<UnaryExpr*>(target)) {
        if (unaryExpr->getOp() == UnaryExpr::Op::Deref) {
            Type* operandType = unaryExpr->getOperand()->getType();
            if (operandType) {
                if (operandType->isReference()) {
                    if (!static_cast<ReferenceType*>(operandType)->isMutable()) {
                        Diag.report(DiagID::err_cannot_assign_to_immutable, loc, target->getRange())
                            << "<immutable reference>";
                        return false;
                    }
                } else if (operandType->isPointer()) {
                    if (!static_cast<PointerType*>(operandType)->isMutable()) {
                        Diag.report(DiagID::err_cannot_assign_to_immutable, loc, target->getRange())
                            << "<immutable pointer>";
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

bool TypeChecker::isCopyType(Type* type) {
    std::unordered_set<const Type*> visiting;
    return isCopyTypeImpl(type, visiting);
}

bool TypeChecker::needsDrop(Type* type) {
    std::unordered_set<const Type*> visiting;
    return needsDropImpl(type, visiting);
}

bool TypeChecker::isCopyTypeImpl(Type* type, std::unordered_set<const Type*>& visiting) {
    type = unwrapAliases(type);
    if (!type) {
        return false;
    }

    VisitingGuard guard(visiting, type);
    if (!guard.Active) {
        // Break cycles conservatively.
        return false;
    }

    if (type->isVoid() || type->isBool() || type->isChar() || type->isString() ||
        type->isInteger() || type->isFloat() || type->isPointer() ||
        type->isReference() || type->isFunction()) {
        return true;
    }

    if (type->isGeneric()) {
        auto* genericType = static_cast<GenericType*>(type);
        for (TraitType* constraint : genericType->getConstraints()) {
            if (constraint && constraint->getName() == "Copy") {
                return true;
            }
        }
        return false;
    }

    if (needsDrop(type)) {
        return false;
    }

    if (type->isArray()) {
        auto* arrType = static_cast<ArrayType*>(type);
        return isCopyTypeImpl(arrType->getElementType(), visiting);
    }
    if (type->isSlice()) {
        auto* sliceType = static_cast<SliceType*>(type);
        return isCopyTypeImpl(sliceType->getElementType(), visiting);
    }
    if (type->isOptional()) {
        auto* optType = static_cast<OptionalType*>(type);
        return isCopyTypeImpl(optType->getInnerType(), visiting);
    }
    if (type->isTuple()) {
        auto* tupleType = static_cast<TupleType*>(type);
        for (size_t i = 0; i < tupleType->getElementCount(); ++i) {
            if (!isCopyTypeImpl(tupleType->getElement(i), visiting)) {
                return false;
            }
        }
        return true;
    }
    if (type->isStruct()) {
        auto* structType = static_cast<StructType*>(type);
        for (const auto& field : structType->getFields()) {
            if (!isCopyTypeImpl(field.FieldType, visiting)) {
                return false;
            }
        }
        return true;
    }
    if (type->isEnum()) {
        auto* enumType = static_cast<EnumType*>(type);
        for (const auto& variant : enumType->getVariants()) {
            for (Type* payloadTy : variant.Data) {
                if (!isCopyTypeImpl(payloadTy, visiting)) {
                    return false;
                }
            }
        }
        return true;
    }
    if (type->isGenericInstance()) {
        auto* instType = static_cast<GenericInstanceType*>(type);
        if (!isCopyTypeImpl(instType->getBaseType(), visiting)) {
            return false;
        }
        for (Type* argTy : instType->getTypeArgs()) {
            if (!isCopyTypeImpl(argTy, visiting)) {
                return false;
            }
        }
        return true;
    }
    if (type->isError()) {
        auto* errType = static_cast<ErrorType*>(type);
        return isCopyTypeImpl(errType->getSuccessType(), visiting);
    }
    if (type->isRange()) {
        auto* rangeType = static_cast<RangeType*>(type);
        return isCopyTypeImpl(rangeType->getElementType(), visiting);
    }

    return false;
}

bool TypeChecker::needsDropImpl(Type* type, std::unordered_set<const Type*>& visiting) {
    type = unwrapAliases(type);
    if (!type) {
        return false;
    }

    VisitingGuard guard(visiting, type);
    if (!guard.Active) {
        return false;
    }

    if (type->isGeneric()) {
        // needsDrop 仅对“具有显式 Drop impl 的具体类型”返回 true。
        return false;
    }

    FuncDecl* dropMethod = Ctx.getImplMethod(type, "drop");
    if (!dropMethod) {
        return false;
    }
    if (dropMethod->getParams().empty()) {
        return false;
    }
    ParamDecl* selfParam = dropMethod->getParams()[0];
    if (!selfParam || !selfParam->isSelf() ||
        selfParam->getParamKind() != ParamDecl::ParamKind::MutRefSelf) {
        return false;
    }

    Type* retType = dropMethod->getSemanticType();
    if (!retType || !retType->isFunction()) {
        return false;
    }
    auto* fnType = static_cast<FunctionType*>(retType);
    return fnType->getReturnType() && fnType->getReturnType()->isVoid();
}

Type* TypeChecker::getCommonType(Type* t1, Type* t2) {
    if (!t1 || !t2) {
        return nullptr;
    }

    if (t1->isEqual(t2)) {
        return t1;
    }

    if (t1->isNumeric() && t2->isNumeric()) {
        if (t1->isFloat() && t2->isInteger()) {
            return t1;
        }
        if (t2->isFloat() && t1->isInteger()) {
            return t2;
        }

        if (t1->isFloat() && t2->isFloat()) {
            auto* f1 = static_cast<FloatType*>(t1);
            auto* f2 = static_cast<FloatType*>(t2);
            return f1->getBitWidth() >= f2->getBitWidth() ? t1 : t2;
        }

        if (t1->isInteger() && t2->isInteger()) {
            auto* i1 = static_cast<IntegerType*>(t1);
            auto* i2 = static_cast<IntegerType*>(t2);
            if (i1->isSigned() != i2->isSigned()) {
                return nullptr;
            }
            return i1->getBitWidth() >= i2->getBitWidth() ? t1 : t2;
        }
    }

    if (t1->isOptional() && !t2->isOptional()) {
        Type* inner = static_cast<OptionalType*>(t1)->getInnerType();
        if (inner->isEqual(t2)) {
            return t1;
        }
    }
    if (t2->isOptional() && !t1->isOptional()) {
        Type* inner = static_cast<OptionalType*>(t2)->getInnerType();
        if (inner->isEqual(t1)) {
            return t2;
        }
    }

    return nullptr;
}

bool TypeChecker::evaluateConstExpr(Expr* expr, int64_t& result) {
    if (!expr) {
        return false;
    }

    switch (expr->getKind()) {
        case ASTNode::Kind::IntegerLiteralExpr: {
            auto* intLit = static_cast<IntegerLiteralExpr*>(expr);
            result = static_cast<int64_t>(intLit->getValue());
            return true;
        }

        case ASTNode::Kind::BoolLiteralExpr: {
            auto* boolLit = static_cast<BoolLiteralExpr*>(expr);
            result = boolLit->getValue() ? 1 : 0;
            return true;
        }

        case ASTNode::Kind::UnaryExpr: {
            auto* unary = static_cast<UnaryExpr*>(expr);
            int64_t operandValue = 0;
            if (!evaluateConstExpr(unary->getOperand(), operandValue)) {
                return false;
            }

            switch (unary->getOp()) {
                case UnaryExpr::Op::Neg:
                    result = -operandValue;
                    return true;
                case UnaryExpr::Op::Not:
                    result = !operandValue;
                    return true;
                case UnaryExpr::Op::BitNot:
                    result = ~operandValue;
                    return true;
                default:
                    return false;
            }
        }

        case ASTNode::Kind::BinaryExpr: {
            auto* binary = static_cast<BinaryExpr*>(expr);
            int64_t leftValue = 0;
            int64_t rightValue = 0;

            if (!evaluateConstExpr(binary->getLHS(), leftValue)) {
                return false;
            }
            if (!evaluateConstExpr(binary->getRHS(), rightValue)) {
                return false;
            }

            switch (binary->getOp()) {
                case BinaryExpr::Op::Add:
                    result = leftValue + rightValue;
                    return true;
                case BinaryExpr::Op::Sub:
                    result = leftValue - rightValue;
                    return true;
                case BinaryExpr::Op::Mul:
                    result = leftValue * rightValue;
                    return true;
                case BinaryExpr::Op::Div:
                    if (rightValue == 0) {
                        Diag.report(DiagID::err_division_by_zero,
                                    binary->getRHS()->getBeginLoc(),
                                    binary->getRHS()->getRange());
                        return false;
                    }
                    result = leftValue / rightValue;
                    return true;
                case BinaryExpr::Op::Mod:
                    if (rightValue == 0) {
                        Diag.report(DiagID::err_division_by_zero,
                                    binary->getRHS()->getBeginLoc(),
                                    binary->getRHS()->getRange());
                        return false;
                    }
                    result = leftValue % rightValue;
                    return true;
                case BinaryExpr::Op::BitAnd:
                    result = leftValue & rightValue;
                    return true;
                case BinaryExpr::Op::BitOr:
                    result = leftValue | rightValue;
                    return true;
                case BinaryExpr::Op::BitXor:
                    result = leftValue ^ rightValue;
                    return true;
                case BinaryExpr::Op::Shl:
                    result = leftValue << rightValue;
                    return true;
                case BinaryExpr::Op::Shr:
                    result = leftValue >> rightValue;
                    return true;
                case BinaryExpr::Op::Eq:
                    result = leftValue == rightValue ? 1 : 0;
                    return true;
                case BinaryExpr::Op::Ne:
                    result = leftValue != rightValue ? 1 : 0;
                    return true;
                case BinaryExpr::Op::Lt:
                    result = leftValue < rightValue ? 1 : 0;
                    return true;
                case BinaryExpr::Op::Le:
                    result = leftValue <= rightValue ? 1 : 0;
                    return true;
                case BinaryExpr::Op::Gt:
                    result = leftValue > rightValue ? 1 : 0;
                    return true;
                case BinaryExpr::Op::Ge:
                    result = leftValue >= rightValue ? 1 : 0;
                    return true;
                case BinaryExpr::Op::And:
                    result = leftValue && rightValue ? 1 : 0;
                    return true;
                case BinaryExpr::Op::Or:
                    result = leftValue || rightValue ? 1 : 0;
                    return true;
                default:
                    return false;
            }
        }

        case ASTNode::Kind::IdentifierExpr: {
            auto* ident = static_cast<IdentifierExpr*>(expr);
            Symbol* sym = Symbols.lookup(ident->getName());
            if (!sym || sym->getKind() != SymbolKind::Constant) {
                return false;
            }

            Decl* decl = sym->getDecl();
            if (!decl || decl->getKind() != ASTNode::Kind::ConstDecl) {
                return false;
            }

            auto* constDecl = static_cast<ConstDecl*>(decl);
            return evaluateConstExpr(constDecl->getInit(), result);
        }

        default:
            return false;
    }
}

} // namespace yuan
