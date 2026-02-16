/// \file SizeofBuiltin.cpp
/// \brief @sizeof 内置函数实现。
///
/// @sizeof 用于获取类型的大小（字节数），例如：
/// - @sizeof(i32)      // 返回 4
/// - @sizeof(MyStruct) // 返回结构体大小

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

/// \brief @sizeof 内置函数处理器
///
/// 返回类型的大小（以字节为单位）。
/// 参数应该是一个类型表达式。
class SizeofBuiltin : public BuiltinHandler {
public:
    const char* getName() const override { return "sizeof"; }
    
    BuiltinKind getKind() const override { return BuiltinKind::Sizeof; }
    
    int getExpectedArgCount() const override { return 1; }
    
    std::string getArgDescription() const override {
        return "类型";
    }
    
    Type* analyze(BuiltinCallExpr* expr, Sema& sema) override {
        // 检查参数数量
        if (expr->getArgCount() != 1) {
            sema.getDiagnostics()
                .report(DiagID::err_wrong_builtin_argument_count, expr->getBeginLoc(), expr->getRange())
                << 1u << static_cast<unsigned>(expr->getArgCount());
            return nullptr;
        }

        // @sizeof 仅接受类型参数
        auto& arg = expr->getArgsMutable()[0];
        if (arg.isType()) {
            Type* resolved = sema.resolveType(arg.getType());
            if (!resolved) {
                return nullptr;
            }
            arg.setResolvedType(resolved);
        } else {
            sema.getDiagnostics()
                .report(DiagID::err_type_mismatch, expr->getBeginLoc(), expr->getRange())
                << "type"
                << "expression";
            return nullptr;
        }

        // 返回 usize 类型（无符号整数，大小与指针相同）
        return sema.getContext().getIntegerType(sema.getContext().getPointerBitWidth(), false);
    }
    
    llvm::Value* generate(BuiltinCallExpr* expr, CodeGen& codegen) override {
        // 获取参数类型的大小
        if (expr->getArgCount() != 1) {
            return nullptr;
        }

        const auto& arg = expr->getArgs()[0];
        Type* type = nullptr;
        if (arg.isType()) {
            type = arg.getResolvedType();
        }

        if (!type) {
            return nullptr;
        }

        // In generic specializations, @sizeof(T) must use the active
        // substitution mapping; otherwise GenericType::getSize() returns 0.
        type = codegen.substituteType(type);
        if (!type) {
            return nullptr;
        }

        size_t size = type->getSize();
        Type* resultType = expr->getType();
        llvm::Type* llvmResultTy = resultType ? codegen.getLLVMType(resultType) : nullptr;
        auto* intTy = llvm::dyn_cast_or_null<llvm::IntegerType>(llvmResultTy);
        if (!intTy) {
            intTy = llvm::Type::getInt64Ty(codegen.getContext());
        }
        return llvm::ConstantInt::get(intTy, size);
    }
};

/// \brief 创建 @sizeof 内置函数处理器
std::unique_ptr<BuiltinHandler> createSizeofBuiltin() {
    return std::make_unique<SizeofBuiltin>();
}

} // namespace yuan
