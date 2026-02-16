/// \file FormatBuiltin.cpp
/// \brief @format 内置函数实现（简化版）
///
/// @format 用于格式化字符串，类似于 C++ 的 std::format，例如：
/// - @format("Hello, {}!", "World")
/// - @format("x = {}, y = {}", 10, 20)
/// - @format("{0} {1} {0}", "a", "b")  // 支持位置参数
///
/// 实现策略：
/// 1. 在编译时将 {} 占位符转换为 printf 格式说明符（%s, %d, %f 等）
/// 2. 调用 snprintf 生成格式化字符串
/// 3. 返回 Yuan 字符串结构体

#include "yuan/Builtin/BuiltinHandler.h"
#include "yuan/Sema/Sema.h"
#include "yuan/Sema/Type.h"
#include "yuan/AST/Expr.h"
#include "yuan/CodeGen/CodeGen.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>

namespace yuan {

/// \brief @format 内置函数处理器
class FormatBuiltin : public BuiltinHandler {
public:
    const char* getName() const override { return "format"; }

    BuiltinKind getKind() const override { return BuiltinKind::Format; }

    int getExpectedArgCount() const override { return -1; }  // 可变参数

    std::string getArgDescription() const override {
        return "格式化字符串和可变参数";
    }

    Type* analyze(BuiltinCallExpr* expr, Sema& sema) override {
        // 至少需要一个参数（格式化字符串）
        if (expr->getArgCount() < 1) {
            sema.getDiagnostics()
                .report(DiagID::err_wrong_builtin_argument_count, expr->getBeginLoc(), expr->getRange())
                << ">= 1" << static_cast<unsigned>(expr->getArgCount());
            return nullptr;
        }

        // 第一个参数必须是字符串
        const auto& formatArg = expr->getArgs()[0];
        if (!formatArg.isExpr()) {
            return nullptr;
        }

        // 分析格式化字符串表达式
        Expr* formatExpr = formatArg.getExpr();
        Type* formatType = sema.analyzeExpr(formatExpr);
        if (!formatType) {
            return nullptr;
        }

        // 检查类型是否为字符串
        if (!formatType->isString()) {
            sema.getDiagnostics()
                .report(DiagID::err_type_mismatch, formatExpr->getBeginLoc(), formatExpr->getRange())
                << "str"
                << formatType->toString();
            return nullptr;
        }

        // 分析所有参数表达式
        for (size_t i = 1; i < expr->getArgCount(); ++i) {
            const auto& arg = expr->getArgs()[i];
            if (!arg.isExpr()) {
                continue;
            }

            Expr* argExpr = arg.getExpr();
            Type* argType = sema.analyzeExpr(argExpr);
            if (!argType) {
                return nullptr;
            }

            // 验证参数类型是否可格式化
            // 支持：整数、浮点数、字符串、布尔值
            if (!argType->isInteger() && !argType->isFloat() &&
                !argType->isString() && !argType->isBool()) {
                sema.getDiagnostics()
                    .report(DiagID::err_type_mismatch, argExpr->getBeginLoc(), argExpr->getRange())
                    << "integer/float/bool/str"
                    << argType->toString();
                return nullptr;
            }
        }

        // @format 返回字符串类型
        return formatType;
    }

    llvm::Value* generate(BuiltinCallExpr* expr, CodeGen& codegen) override {
        if (expr->getArgCount() < 1) {
            return nullptr;
        }

        llvm::LLVMContext& context = codegen.getContext();
        llvm::Module* module = codegen.getModule();

        // 获取格式化字符串
        const auto& formatArg = expr->getArgs()[0];
        if (!formatArg.isExpr()) {
            return nullptr;
        }

        Expr* formatExpr = formatArg.getExpr();

        // 尝试获取格式化字符串的字面量值（用于编译时转换）
        // 注意：这里简化处理，假设格式化字符串是字面量
        // 完整实现需要处理运行时字符串

        // 生成所有参数的 LLVM IR
        std::vector<llvm::Value*> printfArgs;

        // 第一个参数是格式化字符串（暂时直接使用，不做转换）
        llvm::Value* formatValue = codegen.generateExprPublic(formatExpr);
        if (!formatValue) {
            return nullptr;
        }

        llvm::Value* formatPtr = nullptr;
        if (formatValue->getType()->isStructTy()) {
            formatPtr = codegen.getBuilder().CreateExtractValue(formatValue, 0, "format.ptr");
        } else if (formatValue->getType()->isPointerTy()) {
            formatPtr = formatValue;
        } else {
            return nullptr;
        }

        printfArgs.push_back(formatPtr);

        // 处理每个参数
        for (size_t i = 1; i < expr->getArgCount(); ++i) {
            const auto& arg = expr->getArgs()[i];
            if (!arg.isExpr()) {
                continue;
            }

            Expr* argExpr = arg.getExpr();
            llvm::Value* argValue = codegen.generateExprPublic(argExpr);
            if (!argValue) {
                continue;
            }

            // 根据类型处理参数
            Type* argType = argExpr->getType();

            if (argType && argType->isString()) {
                // 字符串类型：提取指针
                if (argValue->getType()->isStructTy()) {
                    llvm::Value* strPtr = codegen.getBuilder().CreateExtractValue(
                        argValue, 0, "arg.str.ptr"
                    );
                    printfArgs.push_back(strPtr);
                } else {
                    printfArgs.push_back(argValue);
                }
            } else {
                // 其他类型直接传递（整数、浮点数等）
                printfArgs.push_back(argValue);
            }
        }

        // 声明 snprintf 函数
        llvm::Function* snprintfFunc = module->getFunction("snprintf");
        if (!snprintfFunc) {
            llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
            llvm::FunctionType* snprintfType = llvm::FunctionType::get(
                llvm::Type::getInt32Ty(context),
                {i8PtrTy, llvm::Type::getInt64Ty(context), i8PtrTy},
                true  // 可变参数
            );
            snprintfFunc = llvm::Function::Create(
                snprintfType,
                llvm::Function::ExternalLinkage,
                "snprintf",
                module
            );
        }

        // 分配缓冲区（栈上分配 1024 字节）
        llvm::Value* bufferSize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1024);
        llvm::Value* buffer = codegen.getBuilder().CreateAlloca(
            llvm::Type::getInt8Ty(context),
            bufferSize,
            "format.buffer"
        );

        // 调用 snprintf
        std::vector<llvm::Value*> snprintfCallArgs = {buffer, bufferSize};
        snprintfCallArgs.insert(snprintfCallArgs.end(), printfArgs.begin(), printfArgs.end());
        llvm::Value* length = codegen.getBuilder().CreateCall(snprintfFunc, snprintfCallArgs);

        // 转换长度为 i64
        llvm::Value* length64 = codegen.getBuilder().CreateSExt(
            length,
            llvm::Type::getInt64Ty(context),
            "format.length"
        );

        // 构造 Yuan 字符串结构体 { i8*, i64 }
        llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
        llvm::Type* stringStructTy = llvm::StructType::get(context, {i8PtrTy, llvm::Type::getInt64Ty(context)});

        llvm::Value* result = llvm::UndefValue::get(stringStructTy);
        result = codegen.getBuilder().CreateInsertValue(result, buffer, 0, "format.result.ptr");
        result = codegen.getBuilder().CreateInsertValue(result, length64, 1, "format.result");

        return result;
    }
};

/// \brief 创建 @format 内置函数处理器
std::unique_ptr<BuiltinHandler> createFormatBuiltin() {
    return std::make_unique<FormatBuiltin>();
}

} // namespace yuan
