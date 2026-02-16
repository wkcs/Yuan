/// \file AlignofBuiltin.cpp
/// \brief @alignof 内置函数实现。
///
/// @alignof 用于获取类型的对齐要求（字节数），例如：
/// - @alignof(i32)      // 返回 4
/// - @alignof(MyStruct) // 返回结构体对齐要求

#include "yuan/Builtin/BuiltinHandler.h"
#include "yuan/Builtin/BuiltinRegistry.h"
#include "yuan/AST/Expr.h"
#include "yuan/Sema/Sema.h"
#include "yuan/Sema/Type.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/Basic/Diagnostic.h"
#include "yuan/CodeGen/CodeGen.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>

namespace yuan {

/// \brief @alignof 内置函数处理器
///
/// 返回类型的对齐要求（以字节为单位）。
/// 参数应该是一个类型表达式。
class AlignofBuiltin : public BuiltinHandler {
public:
    const char* getName() const override { return "alignof"; }
    
    BuiltinKind getKind() const override { return BuiltinKind::Alignof; }
    
    int getExpectedArgCount() const override { return 1; }
    
    std::string getArgDescription() const override {
        return "类型或表达式";
    }
    
    Type* analyze(BuiltinCallExpr* expr, Sema& sema) override {
        // 检查参数数量
        if (expr->getArgCount() != 1) {
            sema.getDiagnostics()
                .report(DiagID::err_wrong_builtin_argument_count, expr->getBeginLoc(), expr->getRange())
                << 1u << static_cast<unsigned>(expr->getArgCount());
            return nullptr;
        }

        // 参数可以是类型或表达式
        auto& arg = expr->getArgsMutable()[0];
        if (arg.isType()) {
            Type* resolved = sema.resolveType(arg.getType());
            if (!resolved) {
                return nullptr;
            }
            arg.setResolvedType(resolved);
        } else if (arg.isExpr()) {
            Type* exprType = sema.analyzeExpr(arg.getExpr());
            if (!exprType) {
                return nullptr;
            }
        } else {
            return nullptr;
        }

        // 返回 usize 类型（无符号整数，大小与指针相同）
        return sema.getContext().getIntegerType(sema.getContext().getPointerBitWidth(), false);
    }
    
    llvm::Value* generate(BuiltinCallExpr* expr, CodeGen& codegen) override {
        // 获取参数类型的对齐要求
        if (expr->getArgCount() != 1) {
            return nullptr;
        }

        const auto& arg = expr->getArgs()[0];
        Type* type = nullptr;
        if (arg.isType()) {
            type = arg.getResolvedType();
        } else if (arg.isExpr()) {
            Expr* argExpr = arg.getExpr();
            type = argExpr ? argExpr->getType() : nullptr;
        }

        if (!type) {
            return nullptr;
        }

        size_t align = type->getAlignment();
        Type* resultType = expr->getType();
        llvm::Type* llvmResultTy = resultType ? codegen.getLLVMType(resultType) : nullptr;
        auto* intTy = llvm::dyn_cast_or_null<llvm::IntegerType>(llvmResultTy);
        if (!intTy) {
            intTy = llvm::Type::getInt64Ty(codegen.getContext());
        }
        return llvm::ConstantInt::get(intTy, align);
    }
};

/// \brief 创建 @alignof 内置函数处理器
std::unique_ptr<BuiltinHandler> createAlignofBuiltin() {
    return std::make_unique<AlignofBuiltin>();
}

} // namespace yuan
