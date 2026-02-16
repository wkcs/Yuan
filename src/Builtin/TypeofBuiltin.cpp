/// \file TypeofBuiltin.cpp
/// \brief @typeof 内置函数实现。
///
/// @typeof 用于获取表达式的类型信息，例如：
/// - @typeof(x)        // 返回变量 x 的类型
/// - @typeof(1 + 2)    // 返回表达式的类型

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
#include <llvm/IR/GlobalVariable.h>

namespace yuan {

/// \brief @typeof 内置函数处理器
///
/// 返回表达式的类型信息。
/// 这是一个编译时操作，返回类型的字符串表示。
class TypeofBuiltin : public BuiltinHandler {
public:
    const char* getName() const override { return "typeof"; }
    
    BuiltinKind getKind() const override { return BuiltinKind::Typeof; }
    
    int getExpectedArgCount() const override { return 1; }
    
    std::string getArgDescription() const override {
        return "表达式";
    }
    
    Type* analyze(BuiltinCallExpr* expr, Sema& sema) override {
        // 检查参数数量
        if (expr->getArgCount() != 1) {
            sema.getDiagnostics()
                .report(DiagID::err_wrong_builtin_argument_count, expr->getBeginLoc(), expr->getRange())
                << 1u << static_cast<unsigned>(expr->getArgCount());
            return nullptr;
        }

        const auto& arg = expr->getArgs()[0];
        if (!arg.isExpr()) {
            sema.getDiagnostics()
                .report(DiagID::err_expected_expression, expr->getBeginLoc(), expr->getRange());
            return nullptr;
        }

        // 分析参数表达式以获取其类型
        if (!sema.analyzeExpr(arg.getExpr())) {
            return nullptr;
        }

        // 返回 str 类型（类型名称的字符串表示）
        return sema.getContext().getStrType();
    }
    
    llvm::Value* generate(BuiltinCallExpr* expr, CodeGen& codegen) override {
        if (expr->getArgCount() != 1) {
            return nullptr;
        }

        const auto& arg = expr->getArgs()[0];
        if (!arg.isExpr()) {
            return nullptr;
        }

        Expr* argExpr = arg.getExpr();
        Type* type = argExpr ? argExpr->getType() : nullptr;
        if (!type) {
            return nullptr;
        }

        std::string typeName = type->toString();
        if (auto* intLit = dynamic_cast<IntegerLiteralExpr*>(argExpr)) {
            if (intLit->hasTypeSuffix() && intLit->isPointerSizedSuffix()) {
                typeName = intLit->isSigned() ? "isize" : "usize";
            }
        }

        // 生成类型名称的字符串常量（{ i8*, i64 }）
        llvm::LLVMContext& context = codegen.getContext();
        llvm::Module* module = codegen.getModule();
        llvm::Constant* strConstant = llvm::ConstantDataArray::getString(
            context, typeName, true
        );

        llvm::GlobalVariable* strGlobal = new llvm::GlobalVariable(
            *module,
            strConstant->getType(),
            true,
            llvm::GlobalValue::PrivateLinkage,
            strConstant,
            ".str"
        );

        llvm::Type* i8PtrType = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
        llvm::Type* i64Type = llvm::Type::getInt64Ty(context);
        llvm::StructType* stringType = llvm::StructType::get(context, {i8PtrType, i64Type});

        llvm::Constant* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
        llvm::Constant* indices[] = {zero, zero};
        llvm::Constant* strPtr = llvm::ConstantExpr::getGetElementPtr(
            strGlobal->getValueType(), strGlobal, indices
        );

        llvm::Constant* len = llvm::ConstantInt::get(i64Type, typeName.size());
        return llvm::ConstantStruct::get(stringType, strPtr, len);
    }
};

/// \brief 创建 @typeof 内置函数处理器
std::unique_ptr<BuiltinHandler> createTypeofBuiltin() {
    return std::make_unique<TypeofBuiltin>();
}

} // namespace yuan
