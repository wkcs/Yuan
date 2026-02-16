/// \file ImportBuiltin.cpp
/// \brief @import 内置函数实现。
///
/// @import 用于导入模块，例如：
/// - @import("std.io")
/// - @import("./local_module")

#include "yuan/Builtin/BuiltinHandler.h"
#include "yuan/Sema/Sema.h"
#include "yuan/Sema/Type.h"
#include "yuan/AST/Expr.h"
#include "yuan/Basic/Diagnostic.h"

namespace yuan {

/// \brief @import 内置函数处理器
///
/// 处理模块导入，支持：
/// - 标准库模块路径（如 "std.io"）
/// - 相对路径（如 "./local_module"）
class ImportBuiltin : public BuiltinHandler {
public:
    const char* getName() const override { return "import"; }
    
    BuiltinKind getKind() const override { return BuiltinKind::Import; }
    
    int getExpectedArgCount() const override { return 1; }
    
    std::string getArgDescription() const override {
        return "模块路径字符串";
    }
    
    Type* analyze(BuiltinCallExpr* expr, Sema& sema) override {
        // 检查参数数量
        if (expr->getArgCount() != 1) {
            sema.getDiagnostics()
                .report(DiagID::err_wrong_builtin_argument_count, expr->getBeginLoc(), expr->getRange())
                << 1u << static_cast<unsigned>(expr->getArgCount());
            return nullptr;
        }

        // 检查参数类型（必须是字符串字面量表达式）
        const auto& arg = expr->getArgs()[0];
        if (!arg.isExpr()) {
            sema.getDiagnostics()
                .report(DiagID::err_expected_expression, expr->getBeginLoc(), expr->getRange());
            return nullptr;
        }

        Expr* argExpr = arg.getExpr();
        if (!StringLiteralExpr::classof(argExpr)) {
            Type* argType = sema.analyzeExpr(argExpr);
            sema.getDiagnostics()
                .report(DiagID::err_type_mismatch, argExpr->getBeginLoc(), argExpr->getRange())
                << "string literal"
                << (argType ? argType->toString() : "unknown");
            return nullptr;
        }

        // 获取模块路径
        auto* strLit = static_cast<StringLiteralExpr*>(argExpr);
        const std::string& modulePath = strLit->getValue();

        // 调用 Sema 的模块解析方法
        return sema.resolveModuleType(modulePath, expr->getRange().getBegin());
    }
    
    llvm::Value* generate(BuiltinCallExpr* expr, CodeGen& codegen) override {
        // @import 在编译时处理，不生成运行时代码
        // 模块导入的符号已在语义分析阶段处理
        (void)expr;
        (void)codegen;
        return nullptr;
    }
};

/// \brief 创建 @import 内置函数处理器
std::unique_ptr<BuiltinHandler> createImportBuiltin() {
    return std::make_unique<ImportBuiltin>();
}

} // namespace yuan
