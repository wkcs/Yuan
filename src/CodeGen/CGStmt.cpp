/// \file CGStmt.cpp
/// \brief Implementation of statement code generation.

#include "yuan/CodeGen/CodeGen.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/AST/Decl.h"
#include "yuan/AST/Stmt.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/Pattern.h"
#include "yuan/Sema/Type.h"
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>

namespace yuan {
namespace {

static Type* unwrapTypeAlias(Type* type) {
    Type* current = type;
    while (current && current->isTypeAlias()) {
        current = static_cast<TypeAlias*>(current)->getAliasedType();
    }
    return current;
}

} // namespace

// ============================================================================
// Statement code generation
// ============================================================================

bool CodeGen::generateStmt(Stmt* stmt) {
    if (!stmt) {
        return false;
    }

    switch (stmt->getKind()) {
        case ASTNode::Kind::DeclStmt:
            return generateDeclStmt(static_cast<DeclStmt*>(stmt));
        case ASTNode::Kind::ExprStmt:
            return generateExprStmt(static_cast<ExprStmt*>(stmt));
        case ASTNode::Kind::BlockStmt:
            return generateBlockStmt(static_cast<BlockStmt*>(stmt));
        case ASTNode::Kind::ReturnStmt:
            return generateReturnStmt(static_cast<ReturnStmt*>(stmt));
        case ASTNode::Kind::IfStmt:
            return generateIfStmt(static_cast<IfStmt*>(stmt));
        case ASTNode::Kind::WhileStmt:
            return generateWhileStmt(static_cast<WhileStmt*>(stmt));
        case ASTNode::Kind::LoopStmt:
            return generateLoopStmt(static_cast<LoopStmt*>(stmt));
        case ASTNode::Kind::ForStmt:
            return generateForStmt(static_cast<ForStmt*>(stmt));
        case ASTNode::Kind::MatchStmt:
            return generateMatchStmt(static_cast<MatchStmt*>(stmt));
        case ASTNode::Kind::BreakStmt:
            return generateBreakStmt(static_cast<BreakStmt*>(stmt));
        case ASTNode::Kind::ContinueStmt:
            return generateContinueStmt(static_cast<ContinueStmt*>(stmt));
        case ASTNode::Kind::DeferStmt:
            return generateDeferStmt(static_cast<DeferStmt*>(stmt));
        default:
            return false;
    }
}

// ============================================================================
// Basic statements
// ============================================================================

bool CodeGen::generateDeclStmt(DeclStmt* stmt) {
    if (!stmt) {
        return false;
    }
    return generateDecl(stmt->getDecl());
}

bool CodeGen::generateExprStmt(ExprStmt* stmt) {
    if (!stmt) {
        return false;
    }

    // Generate the expression (result is discarded).
    llvm::Value* value = generateExpr(stmt->getExpr());
    if (value) {
        return true;
    }

    // Some compile-time expressions (e.g. @import) intentionally have no
    // runtime value. Keep them valid as expression statements.
    if (Expr* expr = stmt->getExpr()) {
        Type* exprType = expr->getType();
        if (exprType && (exprType->isVoid() || exprType->isModule())) {
            return true;
        }
    }

    return false;
}

bool CodeGen::generateBlockStmt(BlockStmt* stmt) {
    if (!stmt) {
        return false;
    }

    size_t scopeDeferDepth = DeferStack.size();

    // Generate all statements in the block
    for (Stmt* s : stmt->getStatements()) {
        if (!generateStmt(s)) {
            DeferStack.resize(scopeDeferDepth);
            return false;
        }

        // Check if current block is terminated (by return, break, etc.)
        if (Builder->GetInsertBlock()->getTerminator()) {
            // Block is terminated, stop generating statements
            DeferStack.resize(scopeDeferDepth);
            return true;
        }
    }

    executeDeferredStatements(scopeDeferDepth);
    DeferStack.resize(scopeDeferDepth);
    return true;
}

llvm::Value* CodeGen::generateBlockStmtWithResult(BlockStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    size_t scopeDeferDepth = DeferStack.size();
    const auto& stmts = stmt->getStatements();
    llvm::Value* lastValue = nullptr;

    for (size_t i = 0; i < stmts.size(); ++i) {
        Stmt* s = stmts[i];
        bool isLast = (i + 1 == stmts.size());

        if (isLast) {
            if (auto* exprStmt = dynamic_cast<ExprStmt*>(s)) {
                lastValue = generateExpr(exprStmt->getExpr());
                if (lastValue && lastValue->getType()->isVoidTy()) {
                    lastValue = nullptr;
                }
            } else {
                if (!generateStmt(s)) {
                    DeferStack.resize(scopeDeferDepth);
                    return nullptr;
                }
            }
        } else {
            if (!generateStmt(s)) {
                DeferStack.resize(scopeDeferDepth);
                return nullptr;
            }
        }

        if (Builder->GetInsertBlock()->getTerminator()) {
            DeferStack.resize(scopeDeferDepth);
            return nullptr;
        }
    }

    executeDeferredStatements(scopeDeferDepth);
    DeferStack.resize(scopeDeferDepth);
    return lastValue;
}

bool CodeGen::generateReturnStmt(ReturnStmt* stmt) {
    if (!stmt) {
        return false;
    }

    // Execute all deferred statements in reverse order
    executeDeferredStatements(0);

    // Determine if current function can return errors
    FunctionType* funcType = nullptr;
    Type* successType = nullptr;
    if (CurrentFuncDecl && CurrentFuncDecl->getSemanticType() &&
        CurrentFuncDecl->getSemanticType()->isFunction()) {
        funcType = static_cast<FunctionType*>(CurrentFuncDecl->getSemanticType());
        successType = funcType->getReturnType();
    }

    bool canError = (funcType && funcType->canError());

    auto castValueIfNeeded = [&](llvm::Value* value, llvm::Type* targetType) -> llvm::Value* {
        if (!value || !targetType) {
            return nullptr;
        }
        if (value->getType() == targetType) {
            return value;
        }
        if (value->getType()->isIntegerTy() && targetType->isIntegerTy()) {
            return Builder->CreateSExtOrTrunc(value, targetType, "ret.cast");
        }
        if (value->getType()->isFloatingPointTy() && targetType->isFloatingPointTy()) {
            unsigned srcBits = value->getType()->getPrimitiveSizeInBits();
            unsigned dstBits = targetType->getPrimitiveSizeInBits();
            if (srcBits < dstBits) {
                return Builder->CreateFPExt(value, targetType, "ret.fp.ext");
            }
            return Builder->CreateFPTrunc(value, targetType, "ret.fp.trunc");
        }
        if (value->getType()->isPointerTy() && targetType->isPointerTy()) {
            return Builder->CreateBitCast(value, targetType, "ret.ptr.cast");
        }
        if (value->getType()->isPointerTy() && targetType->isIntegerTy()) {
            return Builder->CreatePtrToInt(value, targetType, "ret.ptrtoint");
        }
        if (value->getType()->isIntegerTy() && targetType->isPointerTy()) {
            return Builder->CreateIntToPtr(value, targetType, "ret.inttoptr");
        }
        return nullptr;
    };

    auto buildErrorResult = [&](llvm::Value* payload, bool isError) -> llvm::Value* {
        if (!successType) {
            return nullptr;
        }

        ErrorType* errorType = Ctx.getErrorType(successType);
        llvm::Type* llvmErrorTy = getLLVMType(errorType);
        if (!llvmErrorTy || !llvmErrorTy->isStructTy()) {
            return nullptr;
        }

        llvm::StructType* errStructTy = llvm::cast<llvm::StructType>(llvmErrorTy);
        if (errStructTy->getNumElements() < 3) {
            return nullptr;
        }
        llvm::Type* okTy = errStructTy->getElementType(1);
        llvm::Type* errPtrTy = errStructTy->getElementType(2);

        llvm::Value* result = llvm::UndefValue::get(errStructTy);
        llvm::Value* tag = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*Context), isError ? 1 : 0);
        result = Builder->CreateInsertValue(result, tag, 0, "err.tag");

        llvm::Value* okVal = llvm::Constant::getNullValue(okTy);
        llvm::Value* errVal = llvm::Constant::getNullValue(errPtrTy);
        if (payload) {
            if (isError) {
                if (payload->getType()->isPointerTy()) {
                    errVal = Builder->CreateBitCast(payload, errPtrTy, "err.ptr.cast");
                } else {
                    // Box non-pointer error payload onto heap and carry it as i8*.
                    llvm::Type* payloadTy = payload->getType();
                    uint64_t payloadSize = Module->getDataLayout().getTypeAllocSize(payloadTy);
                    llvm::Type* sizeTy = llvm::Type::getInt64Ty(*Context);
                    llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(*Context), 0);
                    llvm::FunctionCallee mallocFn = Module->getOrInsertFunction(
                        "malloc",
                        llvm::FunctionType::get(i8PtrTy, {sizeTy}, false)
                    );
                    llvm::Value* sizeVal = llvm::ConstantInt::get(sizeTy, payloadSize);
                    llvm::Value* rawPtr = Builder->CreateCall(mallocFn, {sizeVal}, "err.malloc");
                    llvm::Value* payloadPtr = Builder->CreateBitCast(
                        rawPtr,
                        llvm::PointerType::get(payloadTy, 0),
                        "err.payload.ptr"
                    );
                    Builder->CreateStore(payload, payloadPtr);
                    errVal = Builder->CreateBitCast(rawPtr, errPtrTy, "err.ptr");
                }
            } else {
                if (payload->getType() == okTy) {
                    okVal = payload;
                } else if (payload->getType()->isIntegerTy() && okTy->isIntegerTy()) {
                    okVal = Builder->CreateSExtOrTrunc(payload, okTy, "ok.val.int.cast");
                } else if (payload->getType()->isPointerTy() && okTy->isPointerTy()) {
                    okVal = Builder->CreateBitCast(payload, okTy, "ok.val.ptr.cast");
                } else if (payload->getType()->isPointerTy() && okTy->isIntegerTy()) {
                    okVal = Builder->CreatePtrToInt(payload, okTy, "ok.val.ptrtoint");
                } else if (payload->getType()->isIntegerTy() && okTy->isPointerTy()) {
                    okVal = Builder->CreateIntToPtr(payload, okTy, "ok.val.inttoptr");
                } else {
                    // Aggregate/mixed casts go through memory and require size match.
                    uint64_t srcSize = Module->getDataLayout().getTypeAllocSize(payload->getType());
                    uint64_t dstSize = Module->getDataLayout().getTypeAllocSize(okTy);
                    if (srcSize != dstSize) {
                        return nullptr;
                    }
                    llvm::AllocaInst* tmp = Builder->CreateAlloca(payload->getType(), nullptr, "ok.cast.tmp");
                    Builder->CreateStore(payload, tmp);
                    llvm::Value* castPtr = Builder->CreateBitCast(
                        tmp,
                        llvm::PointerType::get(okTy, 0),
                        "ok.cast.ptr"
                    );
                    okVal = Builder->CreateLoad(okTy, castPtr, "ok.cast.load");
                }
            }
        }

        result = Builder->CreateInsertValue(result, okVal, 1, "err.ok");
        result = Builder->CreateInsertValue(result, errVal, 2, "err.ptr");
        return result;
    };

    // Generate return instruction
    if (stmt->hasValue()) {
        // Return with value
        llvm::Value* retValue = generateExpr(stmt->getValue());
        if (!retValue) {
            return false;
        }
        Type* valueType = stmt->getValue()->getType();

        if (!canError) {
            if (successType && successType->isOptional()) {
                auto* expectedOptType = static_cast<OptionalType*>(successType);
                Type* expectedInnerType = expectedOptType->getInnerType();
                llvm::Type* llvmExpectedOptType = getLLVMType(successType);
                llvm::Type* llvmExpectedInnerType = getLLVMType(expectedInnerType);
                if (!llvmExpectedOptType || !llvmExpectedInnerType) {
                    return false;
                }

                // return ?U as ?T
                if (valueType && valueType->isOptional()) {
                    auto* actualOptType = static_cast<OptionalType*>(valueType);
                    Type* actualInnerType = actualOptType->getInnerType();
                    llvm::Value* hasValue = Builder->CreateExtractValue(retValue, 0, "ret.opt.has");

                    llvm::Value* innerValue = nullptr;
                    if (actualInnerType && actualInnerType->isVoid()) {
                        innerValue = llvm::Constant::getNullValue(llvmExpectedInnerType);
                    } else {
                        innerValue = Builder->CreateExtractValue(retValue, 1, "ret.opt.value");
                        innerValue = castValueIfNeeded(innerValue, llvmExpectedInnerType);
                    }
                    if (!innerValue) {
                        return false;
                    }

                    llvm::Value* normalized = llvm::UndefValue::get(llvmExpectedOptType);
                    normalized = Builder->CreateInsertValue(normalized, hasValue, 0, "ret.opt.has");
                    normalized = Builder->CreateInsertValue(normalized, innerValue, 1, "ret.opt.value");
                    Builder->CreateRet(normalized);
                    return true;
                }

                // return T as ?T (auto wrap Some)
                llvm::Value* innerValue = retValue;
                if (valueType && valueType->isReference() && innerValue->getType()->isPointerTy()) {
                    Type* pointeeType = static_cast<ReferenceType*>(valueType)->getPointeeType();
                    llvm::Type* llvmPointeeType = getLLVMType(pointeeType);
                    if (!llvmPointeeType) {
                        return false;
                    }
                    innerValue = Builder->CreateLoad(llvmPointeeType, innerValue, "ret.autoderef");
                }
                innerValue = castValueIfNeeded(innerValue, llvmExpectedInnerType);
                if (!innerValue) {
                    return false;
                }

                llvm::Value* wrapped = llvm::UndefValue::get(llvmExpectedOptType);
                wrapped = Builder->CreateInsertValue(
                    wrapped,
                    llvm::ConstantInt::get(llvm::Type::getInt1Ty(*Context), 1),
                    0,
                    "ret.opt.has"
                );
                wrapped = Builder->CreateInsertValue(wrapped, innerValue, 1, "ret.opt.value");
                Builder->CreateRet(wrapped);
                return true;
            }

            if (successType) {
                llvm::Type* llvmRetType = getLLVMType(successType);
                if (!llvmRetType) {
                    return false;
                }
                retValue = castValueIfNeeded(retValue, llvmRetType);
                if (!retValue) {
                    return false;
                }
            }
            Builder->CreateRet(retValue);
            return true;
        }

        if (valueType && valueType->isError()) {
            Builder->CreateRet(retValue);
            return true;
        }

        if (valueType && successType && valueType->isEqual(successType)) {
            llvm::Value* okResult = buildErrorResult(retValue, false);
            if (!okResult) {
                return false;
            }
            Builder->CreateRet(okResult);
            return true;
        }

        // Treat as error value
        llvm::Value* errResult = buildErrorResult(retValue, true);
        if (!errResult) {
            return false;
        }
        Builder->CreateRet(errResult);
    } else {
        if (!canError) {
            // Return void
            Builder->CreateRetVoid();
            return true;
        }

        // canError + void: return Ok(void)
        llvm::Value* okResult = buildErrorResult(nullptr, false);
        if (!okResult) {
            return false;
        }
        Builder->CreateRet(okResult);
    }

    return true;
}

// ============================================================================
// Control flow statements
// ============================================================================

bool CodeGen::generateIfStmt(IfStmt* stmt) {
    if (!stmt || stmt->getBranches().empty()) {
        return false;
    }

    llvm::Function* currentFunc = Builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*Context, "if.end", currentFunc);

    llvm::BasicBlock* currentBB = Builder->GetInsertBlock();

    // Generate branches
    for (size_t i = 0; i < stmt->getBranches().size(); ++i) {
        const IfStmt::Branch& branch = stmt->getBranches()[i];

        // Set insertion point to current block
        Builder->SetInsertPoint(currentBB);

        if (branch.Condition) {
            // If or elif branch - generate condition
            llvm::Value* cond = generateExpr(branch.Condition);
            if (!cond) {
                return false;
            }

            // Create blocks for then and next branch
            llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(*Context, "if.then", currentFunc);
            llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(*Context, "if.else", currentFunc);

            Builder->CreateCondBr(cond, thenBB, elseBB);

            // Generate then body
            Builder->SetInsertPoint(thenBB);
            if (!generateBlockStmt(branch.Body)) {
                return false;
            }
            if (!Builder->GetInsertBlock()->getTerminator()) {
                Builder->CreateBr(mergeBB);
            }

            // Continue with else block
            currentBB = elseBB;
        } else {
            // Else branch
            Builder->SetInsertPoint(currentBB);
            if (!generateBlockStmt(branch.Body)) {
                return false;
            }
            if (!Builder->GetInsertBlock()->getTerminator()) {
                Builder->CreateBr(mergeBB);
            }
            currentBB = nullptr; // No more branches
        }
    }

    // If no else branch, jump from last condition to merge
    if (currentBB) {
        Builder->SetInsertPoint(currentBB);
        Builder->CreateBr(mergeBB);
    }

    // Continue with merge block
    // Check if merge block has any predecessors
    if (mergeBB->hasNPredecessorsOrMore(1)) {
        // Merge block is reachable, use it
        Builder->SetInsertPoint(mergeBB);
    } else {
        // Merge block is unreachable (all branches terminated), remove it
        mergeBB->eraseFromParent();
        // Create a new unreachable block for further code generation
        llvm::BasicBlock* unreachableBB = llvm::BasicBlock::Create(*Context, "unreachable", currentFunc);
        Builder->SetInsertPoint(unreachableBB);
        Builder->CreateUnreachable();
    }

    return true;
}

bool CodeGen::generateWhileStmt(WhileStmt* stmt) {
    if (!stmt) {
        return false;
    }

    llvm::Function* currentFunc = Builder->GetInsertBlock()->getParent();

    // Create basic blocks
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*Context, "while.cond", currentFunc);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*Context, "while.body", currentFunc);
    llvm::BasicBlock* endBB = llvm::BasicBlock::Create(*Context, "while.end", currentFunc);

    // Jump to condition
    Builder->CreateBr(condBB);

    // Push loop context for break/continue
    LoopStack.push_back({condBB, endBB, stmt->getLabel(), DeferStack.size()});

    // Generate condition
    Builder->SetInsertPoint(condBB);
    llvm::Value* cond = generateExpr(stmt->getCondition());
    if (!cond) {
        LoopStack.pop_back();
        return false;
    }
    Builder->CreateCondBr(cond, bodyBB, endBB);

    // Generate body
    Builder->SetInsertPoint(bodyBB);
    if (!generateBlockStmt(stmt->getBody())) {
        LoopStack.pop_back();
        return false;
    }
    if (!Builder->GetInsertBlock()->getTerminator()) {
        Builder->CreateBr(condBB);
    }

    // Pop loop context
    LoopStack.pop_back();

    // Continue with end block
    // Check if end block has any predecessors
    if (endBB->hasNPredecessorsOrMore(1)) {
        Builder->SetInsertPoint(endBB);
    } else {
        // End block is unreachable, remove it
        endBB->eraseFromParent();
        // Create unreachable block for further code
        llvm::BasicBlock* unreachableBB = llvm::BasicBlock::Create(*Context, "unreachable", currentFunc);
        Builder->SetInsertPoint(unreachableBB);
        Builder->CreateUnreachable();
    }

    return true;
}

bool CodeGen::generateLoopStmt(LoopStmt* stmt) {
    if (!stmt) {
        return false;
    }

    llvm::Function* currentFunc = Builder->GetInsertBlock()->getParent();

    // Create basic blocks
    llvm::BasicBlock* loopBB = llvm::BasicBlock::Create(*Context, "loop.body", currentFunc);
    llvm::BasicBlock* endBB = llvm::BasicBlock::Create(*Context, "loop.end", currentFunc);

    // Jump to loop body
    Builder->CreateBr(loopBB);

    // Push loop context for break/continue
    LoopStack.push_back({loopBB, endBB, stmt->getLabel(), DeferStack.size()});

    // Generate body
    Builder->SetInsertPoint(loopBB);
    if (!generateBlockStmt(stmt->getBody())) {
        LoopStack.pop_back();
        return false;
    }
    if (!Builder->GetInsertBlock()->getTerminator()) {
        Builder->CreateBr(loopBB);
    }

    // Pop loop context
    LoopStack.pop_back();

    // Continue with end block
    // Check if end block has any predecessors
    if (endBB->hasNPredecessorsOrMore(1)) {
        Builder->SetInsertPoint(endBB);
    } else {
        // End block is unreachable, remove it
        endBB->eraseFromParent();
        // Create unreachable block for further code
        llvm::BasicBlock* unreachableBB = llvm::BasicBlock::Create(*Context, "unreachable", currentFunc);
        Builder->SetInsertPoint(unreachableBB);
        Builder->CreateUnreachable();
    }

    return true;
}

bool CodeGen::generateForStmt(ForStmt* stmt) {
    if (!stmt) {
        return false;
    }

    // Get the iterable expression
    Expr* iterable = stmt->getIterable();
    if (!iterable) {
        return false;
    }

    Type* iterableType = iterable->getType();
    if (!iterableType) {
        return false;
    }
    Type* originalIterableType = iterableType;
    while (iterableType->isReference()) {
        iterableType = static_cast<ReferenceType*>(iterableType)->getPointeeType();
    }

    llvm::Function* currentFunc = Builder->GetInsertBlock()->getParent();

    // Create basic blocks
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*Context, "for.cond", currentFunc);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*Context, "for.body", currentFunc);
    llvm::BasicBlock* incBB = llvm::BasicBlock::Create(*Context, "for.inc", currentFunc);
    llvm::BasicBlock* endBB = llvm::BasicBlock::Create(*Context, "for.end", currentFunc);

    auto materializeIterableValue = [&](Type* valueType) -> llvm::Value* {
        llvm::Value* value = generateExpr(iterable);
        if (!value) {
            return nullptr;
        }

        if (originalIterableType->isReference() && value->getType()->isPointerTy()) {
            llvm::Type* llvmValueType = getLLVMType(valueType);
            if (!llvmValueType) {
                return nullptr;
            }
            return Builder->CreateLoad(llvmValueType, value, "for.iter.autoderef");
        }

        return value;
    };

    // Handle different iterable types
    if (iterableType->isRange()) {
        // For Range: iterate over the range values
        // Range 迭代器实现：
        // 1. 提取 start, end, inclusive 字段
        // 2. 创建当前值变量 current = start
        // 3. 循环条件：current < end (或 current <= end 如果 inclusive)
        // 4. 循环体：绑定 current 到模式变量
        // 5. 递增：current++

        auto* rangeType = static_cast<RangeType*>(iterableType);
        Type* elementType = rangeType->getElementType();
        llvm::Type* llvmElementType = getLLVMType(elementType);
        if (!llvmElementType) {
            return false;
        }

        // Generate the range value
        llvm::Value* rangeValue = materializeIterableValue(iterableType);
        if (!rangeValue) {
            return false;
        }

        // Extract range fields
        llvm::Value* startValue = Builder->CreateExtractValue(rangeValue, 0, "range.start");
        llvm::Value* endValue = Builder->CreateExtractValue(rangeValue, 1, "range.end");
        llvm::Value* inclusiveValue = Builder->CreateExtractValue(rangeValue, 2, "range.inclusive");

        // Create current value variable
        llvm::AllocaInst* currentAlloca = Builder->CreateAlloca(llvmElementType, nullptr, "for.current");
        Builder->CreateStore(startValue, currentAlloca);

        // Jump to condition
        Builder->CreateBr(condBB);

        // Condition: current < end (or current <= end if inclusive)
        Builder->SetInsertPoint(condBB);
        llvm::Value* currentValue = Builder->CreateLoad(llvmElementType, currentAlloca, "current");

        // 根据 inclusive 标志选择比较操作
        // 使用 select 指令：inclusive ? (current <= end) : (current < end)
        llvm::Value* condLT;
        llvm::Value* condLE;

        if (elementType->isInteger()) {
            auto* intType = static_cast<IntegerType*>(elementType);
            if (intType->isSigned()) {
                condLT = Builder->CreateICmpSLT(currentValue, endValue, "cond.lt");
                condLE = Builder->CreateICmpSLE(currentValue, endValue, "cond.le");
            } else {
                condLT = Builder->CreateICmpULT(currentValue, endValue, "cond.lt");
                condLE = Builder->CreateICmpULE(currentValue, endValue, "cond.le");
            }
        } else {
            return false; // 不支持的元素类型
        }

        llvm::Value* cond = Builder->CreateSelect(inclusiveValue, condLE, condLT, "for.cond");
        Builder->CreateCondBr(cond, bodyBB, endBB);

        // Body: bind loop variable and execute body
        Builder->SetInsertPoint(bodyBB);

        // Bind the current value to the pattern variable
        Pattern* pattern = stmt->getPattern();
        if (!bindPattern(pattern, currentValue, elementType)) {
            return false;
        }

        // Push loop context
        LoopStack.push_back({incBB, endBB, stmt->getLabel(), DeferStack.size()});

        // Generate body
        if (!generateBlockStmt(stmt->getBody())) {
            LoopStack.pop_back();
            return false;
        }

        // Pop loop context
        LoopStack.pop_back();

        // If body doesn't have terminator, jump to increment
        if (!Builder->GetInsertBlock()->getTerminator()) {
            Builder->CreateBr(incBB);
        }

        // Increment: current++
        Builder->SetInsertPoint(incBB);
        llvm::Value* nextValue = Builder->CreateAdd(
            Builder->CreateLoad(llvmElementType, currentAlloca, "current"),
            llvm::ConstantInt::get(llvmElementType, 1),
            "next.value"
        );
        Builder->CreateStore(nextValue, currentAlloca);
        Builder->CreateBr(condBB);

    } else if (iterableType->isVarArgs()) {
        // For VarArgs: iterate over values
        auto* varArgsType = static_cast<VarArgsType*>(iterableType);
        Type* elementType = varArgsType->getElementType();
        llvm::Type* llvmElementType = getLLVMType(elementType);
        if (!llvmElementType) {
            return false;
        }

        llvm::Value* varArgsValue = materializeIterableValue(iterableType);
        if (!varArgsValue) {
            return false;
        }

        llvm::Value* lenValue = Builder->CreateExtractValue(varArgsValue, 0, "varargs.len");

        llvm::Type* i64Type = llvm::Type::getInt64Ty(*Context);
        llvm::AllocaInst* indexAlloca = Builder->CreateAlloca(i64Type, nullptr, "for.index");
        Builder->CreateStore(llvm::ConstantInt::get(i64Type, 0), indexAlloca);

        Builder->CreateBr(condBB);

        Builder->SetInsertPoint(condBB);
        llvm::Value* index = Builder->CreateLoad(i64Type, indexAlloca, "index");
        llvm::Value* cond = Builder->CreateICmpULT(index, lenValue, "for.cond");
        Builder->CreateCondBr(cond, bodyBB, endBB);

        Builder->SetInsertPoint(bodyBB);
        llvm::Value* valueObj = callVarArgsGet(varArgsValue, index);
        if (!valueObj) {
            return false;
        }
        llvm::Value* elementValue = convertValueToType(valueObj, elementType);
        if (!elementValue) {
            return false;
        }

        Pattern* pattern = stmt->getPattern();
        if (!bindPattern(pattern, elementValue, elementType)) {
            return false;
        }

        LoopStack.push_back({incBB, endBB, stmt->getLabel(), DeferStack.size()});

        if (!generateBlockStmt(stmt->getBody())) {
            LoopStack.pop_back();
            return false;
        }

        LoopStack.pop_back();

        if (!Builder->GetInsertBlock()->getTerminator()) {
            Builder->CreateBr(incBB);
        }

        Builder->SetInsertPoint(incBB);
        llvm::Value* nextIndex = Builder->CreateAdd(
            Builder->CreateLoad(i64Type, indexAlloca, "index"),
            llvm::ConstantInt::get(i64Type, 1),
            "next.index"
        );
        Builder->CreateStore(nextIndex, indexAlloca);
        Builder->CreateBr(condBB);

    } else if (iterableType->isArray()) {
        // For array: iterate over indices
        ArrayType* arrayType = static_cast<ArrayType*>(iterableType);
        uint64_t arraySize = arrayType->getArraySize();

        // Generate the array value
        llvm::Value* arrayValue = materializeIterableValue(iterableType);
        if (!arrayValue) {
            return false;
        }

        // Create index variable (i64)
        llvm::Type* i64Type = llvm::Type::getInt64Ty(*Context);
        llvm::AllocaInst* indexAlloca = Builder->CreateAlloca(i64Type, nullptr, "for.index");
        Builder->CreateStore(llvm::ConstantInt::get(i64Type, 0), indexAlloca);

        // Jump to condition
        Builder->CreateBr(condBB);

        // Condition: index < array.len
        Builder->SetInsertPoint(condBB);
        llvm::Value* index = Builder->CreateLoad(i64Type, indexAlloca, "index");
        llvm::Value* cond = Builder->CreateICmpULT(
            index,
            llvm::ConstantInt::get(i64Type, arraySize),
            "for.cond"
        );
        Builder->CreateCondBr(cond, bodyBB, endBB);

        // Body: bind loop variable and execute body
        Builder->SetInsertPoint(bodyBB);

        // Get element at current index
        Type* elementType = arrayType->getElementType();
        llvm::Type* llvmElementType = getLLVMType(elementType);
        if (!llvmElementType) {
            return false;
        }

        // Get pointer to array element
        llvm::Value* arrayPtr = arrayValue;
        if (!arrayValue->getType()->isPointerTy()) {
            llvm::Type* arrayLLVMType = getLLVMType(arrayType);
            llvm::Value* tempAlloca = Builder->CreateAlloca(arrayLLVMType, nullptr, "temp.array");
            Builder->CreateStore(arrayValue, tempAlloca);
            arrayPtr = tempAlloca;
        }

        llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Context), 0);
        llvm::Value* indices[] = {zero, index};
        llvm::Value* elementPtr = Builder->CreateGEP(
            getLLVMType(arrayType),
            arrayPtr,
            indices,
            "element.ptr"
        );
        llvm::Value* element = Builder->CreateLoad(llvmElementType, elementPtr, "element");

        // Bind the element to the pattern variable
        Pattern* pattern = stmt->getPattern();
        if (!bindPattern(pattern, element, elementType)) {
            return false;
        }

        // Push loop context
        LoopStack.push_back({incBB, endBB, stmt->getLabel(), DeferStack.size()});

        // Generate body
        if (!generateBlockStmt(stmt->getBody())) {
            LoopStack.pop_back();
            return false;
        }

        // Pop loop context
        LoopStack.pop_back();

        // If body doesn't have terminator, jump to increment
        if (!Builder->GetInsertBlock()->getTerminator()) {
            Builder->CreateBr(incBB);
        }

        // Increment: index++
        Builder->SetInsertPoint(incBB);
        llvm::Value* nextIndex = Builder->CreateAdd(
            Builder->CreateLoad(i64Type, indexAlloca, "index"),
            llvm::ConstantInt::get(i64Type, 1),
            "next.index"
        );
        Builder->CreateStore(nextIndex, indexAlloca);
        Builder->CreateBr(condBB);

    } else if (iterableType->isSlice()) {
        // For slice: iterate over elements
        SliceType* sliceType = static_cast<SliceType*>(iterableType);
        Type* elementType = sliceType->getElementType();

        llvm::Value* sliceValue = materializeIterableValue(iterableType);
        if (!sliceValue) {
            return false;
        }

        // Extract slice pointer and length
        llvm::Value* slicePtr = Builder->CreateExtractValue(sliceValue, 0, "slice.ptr");
        llvm::Value* sliceLen = Builder->CreateExtractValue(sliceValue, 1, "slice.len");

        // Create index variable
        llvm::Type* i64Type = llvm::Type::getInt64Ty(*Context);
        llvm::AllocaInst* indexAlloca = Builder->CreateAlloca(i64Type, nullptr, "for.index");
        Builder->CreateStore(llvm::ConstantInt::get(i64Type, 0), indexAlloca);

        // Jump to condition
        Builder->CreateBr(condBB);

        // Condition: index < slice.len
        Builder->SetInsertPoint(condBB);
        llvm::Value* index = Builder->CreateLoad(i64Type, indexAlloca, "index");
        llvm::Value* cond = Builder->CreateICmpULT(index, sliceLen, "for.cond");
        Builder->CreateCondBr(cond, bodyBB, endBB);

        // Body
        Builder->SetInsertPoint(bodyBB);

        // Get element at current index
        llvm::Type* llvmElementType = getLLVMType(elementType);
        if (!llvmElementType) {
            return false;
        }

        llvm::Value* elementPtr = Builder->CreateGEP(
            llvmElementType,
            slicePtr,
            index,
            "element.ptr"
        );
        llvm::Value* element = Builder->CreateLoad(llvmElementType, elementPtr, "element");

        // Bind the element to the pattern variable
        Pattern* pattern = stmt->getPattern();
        if (!bindPattern(pattern, element, elementType)) {
            return false;
        }

        // Push loop context
        LoopStack.push_back({incBB, endBB, stmt->getLabel(), DeferStack.size()});

        // Generate body
        if (!generateBlockStmt(stmt->getBody())) {
            LoopStack.pop_back();
            return false;
        }

        // Pop loop context
        LoopStack.pop_back();

        // If body doesn't have terminator, jump to increment
        if (!Builder->GetInsertBlock()->getTerminator()) {
            Builder->CreateBr(incBB);
        }

        // Increment
        Builder->SetInsertPoint(incBB);
        llvm::Value* nextIndex = Builder->CreateAdd(
            Builder->CreateLoad(i64Type, indexAlloca, "index"),
            llvm::ConstantInt::get(i64Type, 1),
            "next.index"
        );
        Builder->CreateStore(nextIndex, indexAlloca);
        Builder->CreateBr(condBB);

    } else if (iterableType->isString()) {
        Type* elementType = Ctx.getCharType();
        llvm::Type* llvmElementType = getLLVMType(elementType);
        if (!llvmElementType) {
            return false;
        }

        llvm::Value* strValue = materializeIterableValue(iterableType);
        if (!strValue) {
            return false;
        }

        llvm::Value* strPtr = Builder->CreateExtractValue(strValue, 0, "str.ptr");
        llvm::Value* strLen = Builder->CreateExtractValue(strValue, 1, "str.len");

        llvm::Type* i64Type = llvm::Type::getInt64Ty(*Context);
        llvm::AllocaInst* indexAlloca = Builder->CreateAlloca(i64Type, nullptr, "for.index");
        Builder->CreateStore(llvm::ConstantInt::get(i64Type, 0), indexAlloca);

        Builder->CreateBr(condBB);

        Builder->SetInsertPoint(condBB);
        llvm::Value* index = Builder->CreateLoad(i64Type, indexAlloca, "index");
        llvm::Value* cond = Builder->CreateICmpULT(index, strLen, "for.cond");
        Builder->CreateCondBr(cond, bodyBB, endBB);

        Builder->SetInsertPoint(bodyBB);

        llvm::Value* elementPtr = Builder->CreateGEP(
            llvm::Type::getInt8Ty(*Context),
            strPtr,
            index,
            "str.elem.ptr"
        );
        llvm::Value* element = Builder->CreateLoad(llvmElementType, elementPtr, "str.elem");

        if (!bindPattern(stmt->getPattern(), element, elementType)) {
            return false;
        }

        LoopStack.push_back({incBB, endBB, stmt->getLabel(), DeferStack.size()});
        if (!generateBlockStmt(stmt->getBody())) {
            LoopStack.pop_back();
            return false;
        }
        LoopStack.pop_back();

        if (!Builder->GetInsertBlock()->getTerminator()) {
            Builder->CreateBr(incBB);
        }

        Builder->SetInsertPoint(incBB);
        llvm::Value* nextIndex = Builder->CreateAdd(
            Builder->CreateLoad(i64Type, indexAlloca, "index"),
            llvm::ConstantInt::get(i64Type, 1),
            "next.index"
        );
        Builder->CreateStore(nextIndex, indexAlloca);
        Builder->CreateBr(condBB);

    } else if (iterableType->isTuple()) {
        // For tuple: iterate over elements (heterogeneous tuples use Value)
        auto* tupleType = static_cast<TupleType*>(iterableType);
        size_t elemCount = tupleType->getElementCount();
        if (elemCount == 0) {
            return true;
        }

        Type* firstType = tupleType->getElement(0);
        bool isUniformTuple = true;
        for (size_t i = 1; i < elemCount; ++i) {
            if (!tupleType->getElement(i)->isEqual(firstType)) {
                isUniformTuple = false;
                break;
            }
        }

        Type* elementType = isUniformTuple ? firstType : Ctx.getValueType();
        llvm::Type* llvmElementType = getLLVMType(elementType);
        if (!llvmElementType) {
            return false;
        }

        llvm::Value* tupleValue = materializeIterableValue(iterableType);
        if (!tupleValue) {
            return false;
        }

        llvm::Type* tupleLLVMType = getLLVMType(tupleType);
        if (!tupleLLVMType) {
            return false;
        }

        // Ensure tuple is in value form for extract
        llvm::Value* tupleVal = tupleValue;
        if (tupleValue->getType()->isPointerTy()) {
            tupleVal = Builder->CreateLoad(tupleLLVMType, tupleValue, "tuple.load");
        }

        llvm::Type* i64Type = llvm::Type::getInt64Ty(*Context);
        llvm::AllocaInst* indexAlloca = Builder->CreateAlloca(i64Type, nullptr, "for.index");
        Builder->CreateStore(llvm::ConstantInt::get(i64Type, 0), indexAlloca);

        llvm::AllocaInst* elementAlloca = Builder->CreateAlloca(llvmElementType, nullptr, "tuple.elem");

        Builder->CreateBr(condBB);

        Builder->SetInsertPoint(condBB);
        llvm::Value* index = Builder->CreateLoad(i64Type, indexAlloca, "index");
        llvm::Value* cond = Builder->CreateICmpULT(
            index,
            llvm::ConstantInt::get(i64Type, static_cast<uint64_t>(elemCount)),
            "for.cond"
        );
        Builder->CreateCondBr(cond, bodyBB, endBB);

        Builder->SetInsertPoint(bodyBB);

        llvm::BasicBlock* dispatchBB = llvm::BasicBlock::Create(*Context, "tuple.dispatch", currentFunc);
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*Context, "tuple.merge", currentFunc);
        Builder->CreateBr(dispatchBB);

        Builder->SetInsertPoint(dispatchBB);
        llvm::SwitchInst* switchInst = Builder->CreateSwitch(index, mergeBB, elemCount);
        auto* indexTy = llvm::cast<llvm::IntegerType>(i64Type);
        for (size_t i = 0; i < elemCount; ++i) {
            llvm::BasicBlock* caseBB = llvm::BasicBlock::Create(*Context, "tuple.case", currentFunc);
            switchInst->addCase(llvm::ConstantInt::get(indexTy, static_cast<uint64_t>(i)), caseBB);
            Builder->SetInsertPoint(caseBB);
            Type* tupleElementType = tupleType->getElement(i);
            llvm::Value* elemValue = Builder->CreateExtractValue(tupleVal, static_cast<unsigned>(i), "tuple.elem");
            if (!isUniformTuple) {
                elemValue = buildValueFrom(tupleElementType, elemValue, elementType);
                if (!elemValue) {
                    return false;
                }
            }
            Builder->CreateStore(elemValue, elementAlloca);
            Builder->CreateBr(mergeBB);
        }

        Builder->SetInsertPoint(mergeBB);
        llvm::Value* element = Builder->CreateLoad(llvmElementType, elementAlloca, "element");

        Pattern* pattern = stmt->getPattern();
        if (!bindPattern(pattern, element, elementType)) {
            return false;
        }

        LoopStack.push_back({incBB, endBB, stmt->getLabel(), DeferStack.size()});

        if (!generateBlockStmt(stmt->getBody())) {
            LoopStack.pop_back();
            return false;
        }

        LoopStack.pop_back();

        if (!Builder->GetInsertBlock()->getTerminator()) {
            Builder->CreateBr(incBB);
        }

        Builder->SetInsertPoint(incBB);
        llvm::Value* nextIndex = Builder->CreateAdd(
            Builder->CreateLoad(i64Type, indexAlloca, "index"),
            llvm::ConstantInt::get(i64Type, 1),
            "next.index"
        );
        Builder->CreateStore(nextIndex, indexAlloca);
        Builder->CreateBr(condBB);

    } else {
        // Iterator protocol fallback:
        // 1) iterable has next() -> ?Item
        // 2) iterable has iter(), and iter() result has next() -> ?Item
        auto unwrapRefs = [](Type* type) -> Type* {
            while (type && type->isReference()) {
                type = static_cast<ReferenceType*>(type)->getPointeeType();
            }
            return type;
        };

        FuncDecl* nextMethod = nullptr;
        Type* iteratorType = iterableType;
        llvm::Value* iteratorValue = nullptr;

        if (Type* iteratorBase = unwrapRefs(iteratorType)) {
            nextMethod = Ctx.getImplMethod(iteratorBase, "next");
        }

        if (!nextMethod) {
            FuncDecl* iterMethod = nullptr;
            if (Type* iterableBase = unwrapRefs(iterableType)) {
                iterMethod = Ctx.getImplMethod(iterableBase, "iter");
            }
            if (!iterMethod) {
                return false;
            }

            Type* iterType = iterMethod->getSemanticType();
            if (!iterType || !iterType->isFunction()) {
                return false;
            }
            auto* iterFuncType = static_cast<FunctionType*>(iterType);
            iteratorType = iterFuncType->getReturnType();
            if (!iteratorType) {
                return false;
            }

            auto* iterMember = Ctx.create<MemberExpr>(iterable->getRange(), iterable, "iter");
            iterMember->setResolvedDecl(iterMethod);
            iterMember->setType(iterType);
            auto* iterCall = Ctx.create<CallExpr>(
                iterable->getRange(),
                iterMember,
                std::vector<CallExpr::Arg>{},
                std::vector<TypeNode*>{}
            );
            iterCall->setType(iteratorType);
            iteratorValue = generateCallExpr(iterCall);
            if (!iteratorValue) {
                return false;
            }

            if (Type* iteratorBase = unwrapRefs(iteratorType)) {
                nextMethod = Ctx.getImplMethod(iteratorBase, "next");
            }
        } else {
            iteratorValue = generateExpr(iterable);
            if (!iteratorValue) {
                return false;
            }
        }

        if (!nextMethod) {
            return false;
        }

        Type* nextType = nextMethod->getSemanticType();
        if (!nextType || !nextType->isFunction()) {
            return false;
        }
        auto* nextFuncType = static_cast<FunctionType*>(nextType);
        Type* nextReturnType = nextFuncType->getReturnType();
        if (!nextReturnType || !nextReturnType->isOptional()) {
            return false;
        }
        Type* elementType = static_cast<OptionalType*>(nextReturnType)->getInnerType();

        llvm::Type* llvmElementType = getLLVMType(elementType);
        llvm::Type* llvmNextReturnType = getLLVMType(nextReturnType);
        if (!llvmElementType || !llvmNextReturnType) {
            return false;
        }

        llvm::Value* iteratorStorage = iteratorValue;
        if (!iteratorStorage->getType()->isPointerTy()) {
            Type* iteratorValueType = unwrapRefs(iteratorType);
            llvm::Type* llvmIteratorType = getLLVMType(iteratorValueType);
            if (!llvmIteratorType) {
                return false;
            }
            llvm::AllocaInst* iterAlloca = Builder->CreateAlloca(llvmIteratorType, nullptr, "iter.obj");
            Builder->CreateStore(iteratorStorage, iterAlloca);
            iteratorStorage = iterAlloca;
        }

        auto emitNextCall = [&]() -> llvm::Value* {
            std::string nextFuncName = getFunctionSymbolName(nextMethod);
            llvm::Function* nextFunc = Module->getFunction(nextFuncName);
            if (!nextFunc) {
                llvm::Type* llvmNextFuncType = getLLVMType(nextType);
                auto* fnType = llvm::dyn_cast<llvm::FunctionType>(llvmNextFuncType);
                if (!fnType) {
                    return nullptr;
                }
                nextFunc = llvm::Function::Create(
                    fnType,
                    llvm::Function::ExternalLinkage,
                    nextFuncName,
                    Module.get()
                );
            }

            std::vector<llvm::Value*> callArgs;
            if (nextFuncType->getParamCount() > 0) {
                Type* selfParamType = nextFuncType->getParam(0);
                llvm::Type* llvmSelfType = getLLVMType(selfParamType);
                if (!llvmSelfType) {
                    return nullptr;
                }

                llvm::Value* selfArg = iteratorStorage;
                if (selfParamType->isReference() || selfParamType->isPointer()) {
                    if (!selfArg->getType()->isPointerTy()) {
                        llvm::AllocaInst* selfAlloca = Builder->CreateAlloca(
                            selfArg->getType(), nullptr, "iter.self");
                        Builder->CreateStore(selfArg, selfAlloca);
                        selfArg = selfAlloca;
                    }
                    if (selfArg->getType() != llvmSelfType) {
                        selfArg = Builder->CreateBitCast(selfArg, llvmSelfType, "iter.self.cast");
                    }
                } else {
                    if (selfArg->getType()->isPointerTy()) {
                        selfArg = Builder->CreateLoad(llvmSelfType, selfArg, "iter.self.load");
                    } else if (selfArg->getType() != llvmSelfType) {
                        selfArg = Builder->CreateBitCast(selfArg, llvmSelfType, "iter.self.cast");
                    }
                }
                callArgs.push_back(selfArg);
            }

            llvm::Value* nextResult = Builder->CreateCall(nextFunc, callArgs, "iter.next");
            if (nextResult && nextResult->getType()->isPointerTy()) {
                nextResult = Builder->CreateLoad(llvmNextReturnType, nextResult, "iter.next.load");
            }
            return nextResult;
        };

        llvm::AllocaInst* elementAlloca = Builder->CreateAlloca(llvmElementType, nullptr, "iter.elem");

        Builder->CreateBr(condBB);

        Builder->SetInsertPoint(condBB);
        llvm::Value* nextValue = emitNextCall();
        if (!nextValue) {
            return false;
        }
        llvm::Value* hasValue = Builder->CreateExtractValue(nextValue, 0, "iter.has");
        llvm::Value* itemValue = Builder->CreateExtractValue(nextValue, 1, "iter.item");
        Builder->CreateStore(itemValue, elementAlloca);
        Builder->CreateCondBr(hasValue, bodyBB, endBB);

        Builder->SetInsertPoint(bodyBB);
        llvm::Value* element = Builder->CreateLoad(llvmElementType, elementAlloca, "iter.current");
        if (!bindPattern(stmt->getPattern(), element, elementType)) {
            return false;
        }

        LoopStack.push_back({incBB, endBB, stmt->getLabel(), DeferStack.size()});
        if (!generateBlockStmt(stmt->getBody())) {
            LoopStack.pop_back();
            return false;
        }
        LoopStack.pop_back();

        if (!Builder->GetInsertBlock()->getTerminator()) {
            Builder->CreateBr(incBB);
        }

        Builder->SetInsertPoint(incBB);
        Builder->CreateBr(condBB);
    }

    // Continue with end block
    if (endBB->hasNPredecessorsOrMore(1)) {
        Builder->SetInsertPoint(endBB);
    } else {
        // End block is unreachable, remove it
        endBB->eraseFromParent();
        llvm::BasicBlock* unreachableBB = llvm::BasicBlock::Create(*Context, "unreachable", currentFunc);
        Builder->SetInsertPoint(unreachableBB);
        Builder->CreateUnreachable();
    }

    return true;
}

bool CodeGen::generateMatchStmt(MatchStmt* stmt) {
    if (!stmt) {
        return false;
    }

    Expr* scrutinee = stmt->getScrutinee();
    if (!scrutinee) {
        return false;
    }

    Type* scrutineeType = scrutinee->getType();
    if (!scrutineeType) {
        return false;
    }

    llvm::Type* llvmScrutineeType = getLLVMType(scrutineeType);
    if (!llvmScrutineeType) {
        return false;
    }

    llvm::Value* scrutineeValue = generateExpr(scrutinee);
    if (!scrutineeValue) {
        return false;
    }

    llvm::Function* currentFunc = Builder->GetInsertBlock()->getParent();

    // Store scrutinee to avoid multiple evaluations
    llvm::AllocaInst* scrutineeAlloca = Builder->CreateAlloca(llvmScrutineeType, nullptr, "match.scrutinee");
    Builder->CreateStore(scrutineeValue, scrutineeAlloca);

    llvm::BasicBlock* endBB = llvm::BasicBlock::Create(*Context, "match.end", currentFunc);

    struct ArmInstance {
        Pattern* Pat;
        Expr* Guard;
        Stmt* Body;
    };

    std::vector<ArmInstance> instances;
    for (const auto& arm : stmt->getArms()) {
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
            return false;
        }

        if (instances[i].Guard) {
            llvm::BasicBlock* guardBB = llvm::BasicBlock::Create(*Context, "match.guard", currentFunc);
            Builder->CreateCondBr(cond, guardBB, fallthroughBB);

            Builder->SetInsertPoint(guardBB);
            llvm::Value* bindValue = Builder->CreateLoad(llvmScrutineeType, scrutineeAlloca, "match.val");
            if (!bindPattern(instances[i].Pat, bindValue, scrutineeType)) {
                return false;
            }
            llvm::Value* guardValue = generateExpr(instances[i].Guard);
            if (!guardValue) {
                return false;
            }
            Builder->CreateCondBr(guardValue, bodyBB, fallthroughBB);
        } else {
            Builder->CreateCondBr(cond, bodyBB, fallthroughBB);
        }

        // Body
        Builder->SetInsertPoint(bodyBB);
        if (!instances[i].Guard) {
            llvm::Value* bindValue = Builder->CreateLoad(llvmScrutineeType, scrutineeAlloca, "match.val");
            if (!bindPattern(instances[i].Pat, bindValue, scrutineeType)) {
                return false;
            }
        }

        if (!generateStmt(instances[i].Body)) {
            return false;
        }

        if (!Builder->GetInsertBlock()->getTerminator()) {
            Builder->CreateBr(endBB);
        }

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
    }

    return true;
}

// ============================================================================
// Jump statements
// ============================================================================

bool CodeGen::generateBreakStmt(BreakStmt* stmt) {
    if (!stmt) {
        return false;
    }

    // Find the target loop
    LoopContext* target = nullptr;
    if (stmt->hasLabel()) {
        // Find labeled loop
        for (auto it = LoopStack.rbegin(); it != LoopStack.rend(); ++it) {
            if (it->Label == stmt->getLabel()) {
                target = &(*it);
                break;
            }
        }
        if (!target) {
            // Error: label not found
            return false;
        }
    } else {
        // Break innermost loop
        if (LoopStack.empty()) {
            // Error: break outside loop
            return false;
        }
        target = &LoopStack.back();
    }

    // Execute deferred statements
    executeDeferredStatements(target->DeferDepth);

    // Jump to break block
    Builder->CreateBr(target->BreakBlock);
    return true;
}

bool CodeGen::generateContinueStmt(ContinueStmt* stmt) {
    if (!stmt) {
        return false;
    }

    // Find the target loop
    LoopContext* target = nullptr;
    if (stmt->hasLabel()) {
        // Find labeled loop
        for (auto it = LoopStack.rbegin(); it != LoopStack.rend(); ++it) {
            if (it->Label == stmt->getLabel()) {
                target = &(*it);
                break;
            }
        }
        if (!target) {
            // Error: label not found
            return false;
        }
    } else {
        // Continue innermost loop
        if (LoopStack.empty()) {
            // Error: continue outside loop
            return false;
        }
        target = &LoopStack.back();
    }

    // Execute deferred statements
    executeDeferredStatements(target->DeferDepth);

    // Jump to continue block
    Builder->CreateBr(target->ContinueBlock);
    return true;
}

// ============================================================================
// Defer statement
// ============================================================================

bool CodeGen::generateDeferStmt(DeferStmt* stmt) {
    if (!stmt) {
        return false;
    }

    // Add to defer stack (will be executed in reverse order)
    DeferStack.push_back(stmt->getBody());
    return true;
}

void CodeGen::executeDeferredStatements(size_t fromDepth) {
    if (fromDepth > DeferStack.size()) {
        fromDepth = 0;
    }
    // Execute deferred statements in reverse order (LIFO)
    for (size_t i = DeferStack.size(); i > fromDepth; --i) {
        generateStmt(DeferStack[i - 1]);
    }
}

// ============================================================================
// Pattern binding
// ============================================================================

bool CodeGen::bindPattern(Pattern* pattern, llvm::Value* value, Type* valueType) {
    if (!pattern || !value || !valueType) {
        return false;
    }

    // 目前只实现标识符模式的绑定
    // 未来可以扩展支持元组模式、结构体模式等

    switch (pattern->getKind()) {
        case ASTNode::Kind::IdentifierPattern: {
            // 标识符模式：创建变量并绑定值
            auto* idPattern = static_cast<IdentifierPattern*>(pattern);
            const std::string& varName = idPattern->getName();

            Type* baseType = valueType;
            llvm::Value* baseValue = value;
            if (baseType->isReference()) {
                baseType = static_cast<ReferenceType*>(baseType)->getPointeeType();
                llvm::Type* llvmBaseType = getLLVMType(baseType);
                if (llvmBaseType && baseValue->getType()->isPointerTy()) {
                    baseValue = Builder->CreateLoad(llvmBaseType, baseValue, "enum.load");
                }
            } else if (baseType->isPointer()) {
                baseType = static_cast<PointerType*>(baseType)->getPointeeType();
                llvm::Type* llvmBaseType = getLLVMType(baseType);
                if (llvmBaseType && baseValue->getType()->isPointerTy()) {
                    baseValue = Builder->CreateLoad(llvmBaseType, baseValue, "enum.load");
                }
            }
            if (baseType->isGenericInstance()) {
                baseType = static_cast<GenericInstanceType*>(baseType)->getBaseType();
            }
            if (baseType->isEnum()) {
                auto* enumType = static_cast<EnumType*>(baseType);
                const EnumType::Variant* variant = enumType->getVariant(varName);
                if (variant && variant->Data.empty()) {
                    // Treat as enum variant pattern, no binding.
                    return true;
                }
            }

            // 获取 LLVM 类型
            llvm::Type* llvmType = getLLVMType(valueType);
            if (!llvmType) {
                return false;
            }

            // 创建 alloca 用于存储循环变量
            // 注意：我们在当前函数的入口块创建 alloca，这样 LLVM 优化器可以更好地处理
            llvm::Function* currentFunc = Builder->GetInsertBlock()->getParent();
            llvm::IRBuilder<> allocaBuilder(&currentFunc->getEntryBlock(),
                                           currentFunc->getEntryBlock().begin());
            llvm::AllocaInst* varAlloca = allocaBuilder.CreateAlloca(llvmType, nullptr, varName);

            // 在当前位置存储值
            Builder->CreateStore(value, varAlloca);

            // 获取关联的 Decl 并注册到 ValueMap
            Decl* decl = idPattern->getDecl();
            if (decl) {
                ValueMap[decl] = varAlloca;
            }

            return true;
        }

        case ASTNode::Kind::BindPattern: {
            auto* bindPat = static_cast<BindPattern*>(pattern);

            // 先绑定内部模式
            if (!bindPattern(bindPat->getInner(), value, valueType)) {
                return false;
            }

            const std::string& varName = bindPat->getName();
            llvm::Type* llvmType = getLLVMType(valueType);
            if (!llvmType) {
                return false;
            }

            llvm::Function* currentFunc = Builder->GetInsertBlock()->getParent();
            llvm::IRBuilder<> allocaBuilder(&currentFunc->getEntryBlock(),
                                           currentFunc->getEntryBlock().begin());
            llvm::AllocaInst* varAlloca = allocaBuilder.CreateAlloca(llvmType, nullptr, varName);
            Builder->CreateStore(value, varAlloca);

            Decl* decl = bindPat->getDecl();
            if (decl) {
                ValueMap[decl] = varAlloca;
            }

            return true;
        }

        case ASTNode::Kind::OrPattern: {
            // 或模式：在绑定时默认使用第一个分支
            auto* orPat = static_cast<OrPattern*>(pattern);
            if (orPat->getPatterns().empty()) {
                return false;
            }
            return bindPattern(orPat->getPatterns().front(), value, valueType);
        }

        case ASTNode::Kind::WildcardPattern:
            // 通配符模式：不绑定任何变量
            return true;

        case ASTNode::Kind::LiteralPattern:
        case ASTNode::Kind::RangePattern:
            // 字面量/范围模式：不绑定任何变量
            return true;

        case ASTNode::Kind::TuplePattern: {
            // 元组模式：递归绑定每个元素
            Type* baseType = valueType;
            llvm::Value* baseValue = value;
            if (baseType->isReference()) {
                baseType = static_cast<ReferenceType*>(baseType)->getPointeeType();
                llvm::Type* llvmBaseType = getLLVMType(baseType);
                if (llvmBaseType && baseValue->getType()->isPointerTy()) {
                    baseValue = Builder->CreateLoad(llvmBaseType, baseValue, "tuple.load");
                }
            } else if (baseType->isPointer()) {
                baseType = static_cast<PointerType*>(baseType)->getPointeeType();
                llvm::Type* llvmBaseType = getLLVMType(baseType);
                if (llvmBaseType && baseValue->getType()->isPointerTy()) {
                    baseValue = Builder->CreateLoad(llvmBaseType, baseValue, "tuple.load");
                }
            }
            if (baseType->isGenericInstance()) {
                baseType = static_cast<GenericInstanceType*>(baseType)->getBaseType();
            }

            if (!baseType->isTuple()) {
                return false;
            }

            auto* tupleType = static_cast<TupleType*>(baseType);
            auto* tuplePat = static_cast<TuplePattern*>(pattern);
            if (tupleType->getElementCount() != tuplePat->getElementCount()) {
                return false;
            }

            for (size_t i = 0; i < tuplePat->getElementCount(); ++i) {
                Pattern* elemPat = tuplePat->getElements()[i];
                Type* elemType = tupleType->getElement(i);
                llvm::Value* elemValue = Builder->CreateExtractValue(baseValue, i, "tuple.elem");
                if (!bindPattern(elemPat, elemValue, elemType)) {
                    return false;
                }
            }

            return true;
        }

        case ASTNode::Kind::StructPattern: {
            // 结构体模式：绑定字段
            Type* baseType = valueType;
            llvm::Value* baseValue = value;
            if (baseType->isReference()) {
                baseType = static_cast<ReferenceType*>(baseType)->getPointeeType();
                llvm::Type* llvmBaseType = getLLVMType(baseType);
                if (llvmBaseType && baseValue->getType()->isPointerTy()) {
                    baseValue = Builder->CreateLoad(llvmBaseType, baseValue, "struct.load");
                }
            } else if (baseType->isPointer()) {
                baseType = static_cast<PointerType*>(baseType)->getPointeeType();
                llvm::Type* llvmBaseType = getLLVMType(baseType);
                if (llvmBaseType && baseValue->getType()->isPointerTy()) {
                    baseValue = Builder->CreateLoad(llvmBaseType, baseValue, "struct.load");
                }
            }
            if (baseType->isGenericInstance()) {
                baseType = static_cast<GenericInstanceType*>(baseType)->getBaseType();
            }

            if (!baseType->isStruct()) {
                return false;
            }

            auto* structType = static_cast<StructType*>(baseType);
            auto* structPat = static_cast<StructPattern*>(pattern);

            for (const auto& field : structPat->getFields()) {
                const StructType::Field* fieldInfo = structType->getField(field.Name);
                if (!fieldInfo) {
                    return false;
                }

                // Find field index
                size_t fieldIndex = 0;
                bool found = false;
                const auto& fields = structType->getFields();
                for (size_t i = 0; i < fields.size(); ++i) {
                    if (fields[i].Name == field.Name) {
                        fieldIndex = i;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return false;
                }

                llvm::Value* fieldValue = Builder->CreateExtractValue(baseValue, fieldIndex, "struct.field");
                if (!bindPattern(field.Pat, fieldValue, fieldInfo->FieldType)) {
                    return false;
                }
            }

            return true;
        }

        case ASTNode::Kind::EnumPattern: {
            // 枚举模式：绑定负载
            Type* baseType = valueType;
            llvm::Value* baseValue = value;
            if (baseType->isReference()) {
                baseType = static_cast<ReferenceType*>(baseType)->getPointeeType();
                llvm::Type* llvmBaseType = getLLVMType(baseType);
                if (llvmBaseType && baseValue->getType()->isPointerTy()) {
                    baseValue = Builder->CreateLoad(llvmBaseType, baseValue, "enum.load");
                }
            } else if (baseType->isPointer()) {
                baseType = static_cast<PointerType*>(baseType)->getPointeeType();
                llvm::Type* llvmBaseType = getLLVMType(baseType);
                if (llvmBaseType && baseValue->getType()->isPointerTy()) {
                    baseValue = Builder->CreateLoad(llvmBaseType, baseValue, "enum.load");
                }
            }
            if (baseType->isGenericInstance()) {
                baseType = static_cast<GenericInstanceType*>(baseType)->getBaseType();
            }

            if (!baseType->isEnum()) {
                return false;
            }

            auto* enumType = static_cast<EnumType*>(baseType);
            auto* enumPat = static_cast<EnumPattern*>(pattern);
            const EnumType::Variant* variant = enumType->getVariant(enumPat->getVariantName());
            if (!variant) {
                return false;
            }

            if (!enumPat->hasPayload()) {
                return true;
            }

            // Enum representation: { i32 tag, i8* data }
            llvm::Value* dataPtr = Builder->CreateExtractValue(baseValue, 1, "enum.data");

            std::vector<Type*> elementTypes;
            llvm::Type* payloadLLVMType = nullptr;

            if (variant->Data.size() == 1) {
                Type* payloadType = substituteType(variant->Data[0]);
                if (payloadType->isTuple()) {
                    auto* tupleType = static_cast<TupleType*>(payloadType);
                    for (size_t i = 0; i < tupleType->getElementCount(); ++i) {
                        elementTypes.push_back(tupleType->getElement(i));
                    }
                    payloadLLVMType = getLLVMType(payloadType);
                } else if (payloadType->isStruct()) {
                    auto* structType = static_cast<StructType*>(payloadType);
                    for (const auto& field : structType->getFields()) {
                        elementTypes.push_back(field.FieldType);
                    }
                    payloadLLVMType = getLLVMType(payloadType);
                } else {
                    elementTypes.push_back(payloadType);
                    payloadLLVMType = getLLVMType(payloadType);
                }
            } else {
                elementTypes.reserve(variant->Data.size());
                for (Type* fieldType : variant->Data) {
                    elementTypes.push_back(substituteType(fieldType));
                }
                std::vector<llvm::Type*> payloadFields;
                payloadFields.reserve(variant->Data.size());
                for (Type* fieldType : elementTypes) {
                    llvm::Type* llvmFieldType = getLLVMType(fieldType);
                    if (!llvmFieldType) {
                        return false;
                    }
                    payloadFields.push_back(llvmFieldType);
                }
                payloadLLVMType = llvm::StructType::get(*Context, payloadFields);
            }

            if (!payloadLLVMType || elementTypes.size() != enumPat->getPayloadCount()) {
                return false;
            }

            llvm::Value* payloadPtr = Builder->CreateBitCast(
                dataPtr,
                llvm::PointerType::get(payloadLLVMType, 0),
                "enum.payload.ptr"
            );
            llvm::Value* payloadValue = Builder->CreateLoad(payloadLLVMType, payloadPtr, "enum.payload");

            for (size_t i = 0; i < enumPat->getPayloadCount(); ++i) {
                llvm::Value* elemValue = payloadValue;
                if (payloadLLVMType->isStructTy()) {
                    elemValue = Builder->CreateExtractValue(payloadValue, i, "enum.payload.elem");
                }
                if (!bindPattern(enumPat->getPayload()[i], elemValue, elementTypes[i])) {
                    return false;
                }
            }

            return true;
        }

        default:
            // 不支持的模式类型
            return false;
    }
}

llvm::Value* CodeGen::generatePatternCondition(Pattern* pattern, llvm::Value* value, Type* valueType) {
    if (!pattern || !value || !valueType) {
        return nullptr;
    }

    llvm::Type* boolType = llvm::Type::getInt1Ty(*Context);
    llvm::Value* trueVal = llvm::ConstantInt::get(boolType, 1);
    llvm::Value* falseVal = llvm::ConstantInt::get(boolType, 0);

    auto loadIfPointer = [&](Type*& ty, llvm::Value* val, const char* name) -> llvm::Value* {
        ty = unwrapTypeAlias(ty);
        if (!ty) {
            return val;
        }
        if (ty->isReference()) {
            ty = static_cast<ReferenceType*>(ty)->getPointeeType();
            llvm::Type* llvmTy = getLLVMType(ty);
            if (llvmTy && val->getType()->isPointerTy()) {
                return Builder->CreateLoad(llvmTy, val, name);
            }
        } else if (ty->isPointer()) {
            ty = static_cast<PointerType*>(ty)->getPointeeType();
            llvm::Type* llvmTy = getLLVMType(ty);
            if (llvmTy && val->getType()->isPointerTy()) {
                return Builder->CreateLoad(llvmTy, val, name);
            }
        }
        ty = unwrapTypeAlias(ty);
        if (!ty) {
            return val;
        }
        if (ty->isGenericInstance()) {
            ty = static_cast<GenericInstanceType*>(ty)->getBaseType();
        }
        ty = unwrapTypeAlias(ty);
        return val;
    };

    switch (pattern->getKind()) {
        case ASTNode::Kind::WildcardPattern:
            return trueVal;

        case ASTNode::Kind::IdentifierPattern:
        {
            auto* identPat = static_cast<IdentifierPattern*>(pattern);
            Type* baseType = valueType;
            llvm::Value* baseValue = loadIfPointer(baseType, value, "enum.load");
            if (baseType->isGenericInstance()) {
                baseType = static_cast<GenericInstanceType*>(baseType)->getBaseType();
            }
            if (baseType->isEnum()) {
                auto* enumType = static_cast<EnumType*>(baseType);
                const EnumType::Variant* variant = enumType->getVariant(identPat->getName());
                if (variant && variant->Data.empty()) {
                    llvm::Value* tagValue = Builder->CreateExtractValue(baseValue, 0, "enum.tag");
                    return Builder->CreateICmpEQ(
                        tagValue,
                        llvm::ConstantInt::get(tagValue->getType(), variant->Tag),
                        "enum.tag.eq"
                    );
                }
            }
            return trueVal;
        }

        case ASTNode::Kind::BindPattern: {
            auto* bindPat = static_cast<BindPattern*>(pattern);
            return generatePatternCondition(bindPat->getInner(), value, valueType);
        }

        case ASTNode::Kind::OrPattern: {
            auto* orPat = static_cast<OrPattern*>(pattern);
            llvm::Value* cond = falseVal;
            for (auto* alt : orPat->getPatterns()) {
                llvm::Value* altCond = generatePatternCondition(alt, value, valueType);
                if (!altCond) {
                    return nullptr;
                }
                cond = Builder->CreateOr(cond, altCond, "or.cond");
            }
            return cond;
        }

        case ASTNode::Kind::LiteralPattern: {
            auto* litPat = static_cast<LiteralPattern*>(pattern);
            Expr* literal = litPat->getLiteral();
            if (!literal) {
                return falseVal;
            }

            Type* baseType = valueType;
            llvm::Value* baseValue = loadIfPointer(baseType, value, "lit.load");
            if (baseType->isGenericInstance()) {
                baseType = static_cast<GenericInstanceType*>(baseType)->getBaseType();
            }

            if (baseType->isOptional()) {
                llvm::Value* hasValue = Builder->CreateExtractValue(baseValue, 0, "opt.has");

                if (dynamic_cast<NoneLiteralExpr*>(literal)) {
                    return Builder->CreateNot(hasValue, "opt.none");
                }

                // Non-None literal: require hasValue and compare inner value
                llvm::Value* innerValue = Builder->CreateExtractValue(baseValue, 1, "opt.val");
                Type* innerType = static_cast<OptionalType*>(baseType)->getInnerType();
                llvm::Value* litValue = generateExpr(literal);
                if (!litValue) {
                    return nullptr;
                }

                llvm::Value* valueCond = nullptr;
                if (innerType->isString()) {
                    valueCond = emitStringEquality(innerValue, litValue);
                    if (!valueCond) {
                        return nullptr;
                    }
                } else {
                    valueCond = Builder->CreateICmpEQ(innerValue, litValue, "lit.eq");
                }

                llvm::Value* hasValBool = Builder->CreateICmpNE(
                    hasValue, llvm::ConstantInt::get(hasValue->getType(), 0), "has.value");
                return Builder->CreateAnd(hasValBool, valueCond, "opt.match");
            }

            // None literal matching enum variant (e.g., Option.None)
            if (dynamic_cast<NoneLiteralExpr*>(literal) && baseType->isEnum()) {
                auto* enumType = static_cast<EnumType*>(baseType);
                const EnumType::Variant* variant = enumType->getVariant("None");
                if (!variant) {
                    return nullptr;
                }
                llvm::Value* tagValue = Builder->CreateExtractValue(baseValue, 0, "enum.tag");
                return Builder->CreateICmpEQ(
                    tagValue,
                    llvm::ConstantInt::get(tagValue->getType(), variant->Tag),
                    "enum.none"
                );
            }

            llvm::Value* litValue = generateExpr(literal);
            if (!litValue) {
                return nullptr;
            }

            if (baseType->isString()) {
                return emitStringEquality(baseValue, litValue);
            }

            return Builder->CreateICmpEQ(baseValue, litValue, "lit.eq");
        }

        case ASTNode::Kind::RangePattern: {
            auto* rangePat = static_cast<RangePattern*>(pattern);

            Type* baseType = valueType;
            llvm::Value* baseValue = loadIfPointer(baseType, value, "range.load");
            if (!baseType->isInteger() && !baseType->isChar()) {
                return nullptr;
            }

            bool isSigned = false;
            if (baseType->isInteger()) {
                isSigned = static_cast<IntegerType*>(baseType)->isSigned();
            }

            llvm::Value* cond = trueVal;

            if (rangePat->getStart()) {
                llvm::Value* startValue = generateExpr(rangePat->getStart());
                if (!startValue) {
                    return nullptr;
                }
                llvm::Value* cmp = Builder->CreateICmp(
                    isSigned ? llvm::CmpInst::ICMP_SGE : llvm::CmpInst::ICMP_UGE,
                    baseValue,
                    startValue,
                    "range.start"
                );
                cond = Builder->CreateAnd(cond, cmp, "range.and");
            }

            if (rangePat->getEnd()) {
                llvm::Value* endValue = generateExpr(rangePat->getEnd());
                if (!endValue) {
                    return nullptr;
                }
                llvm::CmpInst::Predicate pred = rangePat->isInclusive()
                    ? (isSigned ? llvm::CmpInst::ICMP_SLE : llvm::CmpInst::ICMP_ULE)
                    : (isSigned ? llvm::CmpInst::ICMP_SLT : llvm::CmpInst::ICMP_ULT);
                llvm::Value* cmp = Builder->CreateICmp(pred, baseValue, endValue, "range.end");
                cond = Builder->CreateAnd(cond, cmp, "range.and");
            }

            return cond;
        }

        case ASTNode::Kind::TuplePattern: {
            Type* baseType = valueType;
            llvm::Value* baseValue = loadIfPointer(baseType, value, "tuple.load");
            if (!baseType->isTuple()) {
                return nullptr;
            }

            auto* tupleType = static_cast<TupleType*>(baseType);
            auto* tuplePat = static_cast<TuplePattern*>(pattern);
            if (tupleType->getElementCount() != tuplePat->getElementCount()) {
                return nullptr;
            }

            llvm::Value* cond = trueVal;
            for (size_t i = 0; i < tuplePat->getElementCount(); ++i) {
                llvm::Value* elemValue = Builder->CreateExtractValue(baseValue, i, "tuple.elem");
                llvm::Value* elemCond = generatePatternCondition(tuplePat->getElements()[i], elemValue, tupleType->getElement(i));
                if (!elemCond) {
                    return nullptr;
                }
                cond = Builder->CreateAnd(cond, elemCond, "tuple.and");
            }
            return cond;
        }

        case ASTNode::Kind::StructPattern: {
            Type* baseType = valueType;
            llvm::Value* baseValue = loadIfPointer(baseType, value, "struct.load");
            if (!baseType->isStruct()) {
                return nullptr;
            }

            auto* structType = static_cast<StructType*>(baseType);
            auto* structPat = static_cast<StructPattern*>(pattern);

            llvm::Value* cond = trueVal;
            for (const auto& field : structPat->getFields()) {
                if (!field.Pat) {
                    continue;
                }
                const StructType::Field* fieldInfo = structType->getField(field.Name);
                if (!fieldInfo) {
                    return nullptr;
                }

                size_t fieldIndex = 0;
                bool found = false;
                const auto& fields = structType->getFields();
                for (size_t i = 0; i < fields.size(); ++i) {
                    if (fields[i].Name == field.Name) {
                        fieldIndex = i;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return nullptr;
                }

                llvm::Value* fieldValue = Builder->CreateExtractValue(baseValue, fieldIndex, "struct.field");
                llvm::Value* fieldCond = generatePatternCondition(field.Pat, fieldValue, fieldInfo->FieldType);
                if (!fieldCond) {
                    return nullptr;
                }
                cond = Builder->CreateAnd(cond, fieldCond, "struct.and");
            }
            return cond;
        }

        case ASTNode::Kind::EnumPattern: {
            Type* baseType = valueType;
            llvm::Value* baseValue = loadIfPointer(baseType, value, "enum.load");
            if (baseType->isGenericInstance()) {
                baseType = static_cast<GenericInstanceType*>(baseType)->getBaseType();
            }
            if (!baseType->isEnum()) {
                return nullptr;
            }

            auto* enumType = static_cast<EnumType*>(baseType);
            auto* enumPat = static_cast<EnumPattern*>(pattern);
            const EnumType::Variant* variant = enumType->getVariant(enumPat->getVariantName());
            if (!variant) {
                return nullptr;
            }

            llvm::Value* tagValue = Builder->CreateExtractValue(baseValue, 0, "enum.tag");
            llvm::Value* tagCond = Builder->CreateICmpEQ(
                tagValue,
                llvm::ConstantInt::get(tagValue->getType(), variant->Tag),
                "enum.tag.eq"
            );

            if (!enumPat->hasPayload()) {
                return tagCond;
            }

            std::vector<Type*> elementTypes;
            llvm::Type* payloadLLVMType = nullptr;

            if (variant->Data.size() == 1) {
                Type* payloadType = substituteType(variant->Data[0]);
                if (payloadType->isTuple()) {
                    auto* tupleType = static_cast<TupleType*>(payloadType);
                    for (size_t i = 0; i < tupleType->getElementCount(); ++i) {
                        elementTypes.push_back(tupleType->getElement(i));
                    }
                    payloadLLVMType = getLLVMType(payloadType);
                } else if (payloadType->isStruct()) {
                    auto* structType = static_cast<StructType*>(payloadType);
                    for (const auto& field : structType->getFields()) {
                        elementTypes.push_back(field.FieldType);
                    }
                    payloadLLVMType = getLLVMType(payloadType);
                } else {
                    elementTypes.push_back(payloadType);
                    payloadLLVMType = getLLVMType(payloadType);
                }
            } else {
                elementTypes.reserve(variant->Data.size());
                for (Type* fieldType : variant->Data) {
                    elementTypes.push_back(substituteType(fieldType));
                }
                std::vector<llvm::Type*> payloadFields;
                payloadFields.reserve(variant->Data.size());
                for (Type* fieldType : elementTypes) {
                    llvm::Type* llvmFieldType = getLLVMType(fieldType);
                    if (!llvmFieldType) {
                        return nullptr;
                    }
                    payloadFields.push_back(llvmFieldType);
                }
                payloadLLVMType = llvm::StructType::get(*Context, payloadFields);
            }

            if (!payloadLLVMType || elementTypes.size() != enumPat->getPayloadCount()) {
                return nullptr;
            }

            llvm::Value* dataPtr = Builder->CreateExtractValue(baseValue, 1, "enum.data");
            llvm::Value* payloadPtr = Builder->CreateBitCast(
                dataPtr,
                llvm::PointerType::get(payloadLLVMType, 0),
                "enum.payload.ptr"
            );
            llvm::Value* payloadValue = Builder->CreateLoad(payloadLLVMType, payloadPtr, "enum.payload");

            llvm::Value* cond = tagCond;
            for (size_t i = 0; i < enumPat->getPayloadCount(); ++i) {
                llvm::Value* elemValue = payloadValue;
                if (payloadLLVMType->isStructTy()) {
                    elemValue = Builder->CreateExtractValue(payloadValue, i, "enum.payload.elem");
                }
                llvm::Value* elemCond = generatePatternCondition(enumPat->getPayload()[i], elemValue, elementTypes[i]);
                if (!elemCond) {
                    return nullptr;
                }
                cond = Builder->CreateAnd(cond, elemCond, "enum.and");
            }
            return cond;
        }

        default:
            return falseVal;
    }
}

} // namespace yuan
