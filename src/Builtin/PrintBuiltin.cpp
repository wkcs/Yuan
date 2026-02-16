/// \file PrintBuiltin.cpp
/// \brief @print 内置函数实现。
///
/// @print 用于打印文本到标准输出，例如：
/// - @print("Hello, World!")
/// - @print(message)

#include "yuan/Builtin/BuiltinHandler.h"
#include "yuan/Sema/Sema.h"
#include "yuan/Sema/Type.h"
#include "yuan/AST/Expr.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/CodeGen/CodeGen.h"
#include "yuan/Basic/Diagnostic.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>

namespace yuan {

/// \brief @print 内置函数处理器
///
/// 打印文本到标准输出（不换行）。
/// 参数是要打印的字符串。
class PrintBuiltin : public BuiltinHandler {
public:
    const char* getName() const override { return "print"; }

    BuiltinKind getKind() const override { return BuiltinKind::Print; }

    int getExpectedArgCount() const override { return 1; }

    std::string getArgDescription() const override {
        return "要打印的字符串";
    }

    Type* analyze(BuiltinCallExpr* expr, Sema& sema) override {
        // 检查参数数量
        if (expr->getArgCount() != 1) {
            sema.getDiagnostics()
                .report(DiagID::err_wrong_builtin_argument_count, expr->getBeginLoc(), expr->getRange())
                << 1u << static_cast<unsigned>(expr->getArgCount());
            return nullptr;
        }

        // 检查参数类型（必须是字符串或 Value）
        const auto& arg = expr->getArgs()[0];
        if (!arg.isExpr()) {
            return nullptr;
        }

        // 分析参数表达式并检查类型
        Expr* argExpr = arg.getExpr();
        Type* argType = sema.analyzeExpr(argExpr);
        if (!argType) {
            return nullptr;
        }

        // 检查类型是否为字符串或 Value
        if (!argType->isString() && !argType->isValue()) {
            sema.getDiagnostics()
                .report(DiagID::err_type_mismatch, argExpr->getBeginLoc(), argExpr->getRange())
                << "str or Value"
                << argType->toString();
            return nullptr;
        }

        // @print 返回 void 类型
        return sema.getContext().getVoidType();
    }

    llvm::Value* generate(BuiltinCallExpr* expr, CodeGen& codegen) override {
        // 获取参数
        const auto& arg = expr->getArgs()[0];
        if (!arg.isExpr()) {
            return nullptr;
        }

        Expr* argExpr = arg.getExpr();

        // 生成参数的 LLVM IR
        llvm::Value* argValue = codegen.generateExprPublic(argExpr);
        if (!argValue) {
            return nullptr;
        }

        // 字符串字面量在 Yuan 中表示为 { i8*, i64 } 结构体
        // 我们需要提取指针和长度，优先使用 fwrite 避免 printf 格式解析
        llvm::Value* strPtr = nullptr;
        llvm::Value* strLen = nullptr;
#if defined(_WIN32)
        bool useWindowsSRet = true;
#else
        bool useWindowsSRet = false;
#endif

        if (argExpr->getType() && argExpr->getType()->isValue()) {
            // Value 转换为字符串
            llvm::LLVMContext& context = codegen.getContext();
            llvm::Module* module = codegen.getModule();
            llvm::Function* valueToStringFunc = module->getFunction("yuan_value_to_string");
            llvm::Value* strValue = nullptr;
            if (useWindowsSRet) {
                if (!valueToStringFunc) {
                    llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
                    llvm::Type* i64Ty = llvm::Type::getInt64Ty(context);
                    llvm::Type* i32Ty = llvm::Type::getInt32Ty(context);
                    llvm::StructType* stringStructTy = llvm::StructType::get(context, {i8PtrTy, i64Ty});
                    llvm::StructType* valueStructTy = llvm::StructType::getTypeByName(context, "YuanValue");
                    if (!valueStructTy) {
                        valueStructTy = llvm::StructType::create(context, "YuanValue");
                    }
                    if (valueStructTy->isOpaque()) {
                        valueStructTy->setBody({i32Ty, i32Ty, i64Ty, i64Ty});
                    }

                    llvm::Type* stringStructPtrTy = llvm::PointerType::get(stringStructTy, 0);
                    llvm::FunctionType* valueToStringType = llvm::FunctionType::get(
                        llvm::Type::getVoidTy(context),
                        {stringStructPtrTy, valueStructTy},
                        false
                    );
                    valueToStringFunc = llvm::Function::Create(
                        valueToStringType,
                        llvm::Function::ExternalLinkage,
                        "yuan_value_to_string",
                        module
                    );
                    valueToStringFunc->addParamAttr(0, llvm::Attribute::StructRet);
                }

                llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
                llvm::Type* i64Ty = llvm::Type::getInt64Ty(context);
                llvm::StructType* stringStructTy = llvm::StructType::get(context, {i8PtrTy, i64Ty});
                llvm::AllocaInst* outAlloca = codegen.getBuilder().CreateAlloca(stringStructTy, nullptr, "value.str.out");
                codegen.getBuilder().CreateCall(valueToStringFunc, {outAlloca, argValue});
                strValue = codegen.getBuilder().CreateLoad(stringStructTy, outAlloca, "value.str.ret");
            } else {
                if (!valueToStringFunc) {
                    llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
                    llvm::Type* i64Ty = llvm::Type::getInt64Ty(context);
                    llvm::Type* i32Ty = llvm::Type::getInt32Ty(context);
                    llvm::StructType* stringStructTy = llvm::StructType::get(context, {i8PtrTy, i64Ty});
                    llvm::StructType* valueStructTy = llvm::StructType::getTypeByName(context, "YuanValue");
                    if (!valueStructTy) {
                        valueStructTy = llvm::StructType::create(context, "YuanValue");
                    }
                    if (valueStructTy->isOpaque()) {
                        valueStructTy->setBody({i32Ty, i32Ty, i64Ty, i64Ty});
                    }
                    llvm::FunctionType* valueToStringType = llvm::FunctionType::get(
                        stringStructTy,
                        {valueStructTy},
                        false
                    );
                    valueToStringFunc = llvm::Function::Create(
                        valueToStringType,
                        llvm::Function::ExternalLinkage,
                        "yuan_value_to_string",
                        module
                    );
                }
                strValue = codegen.getBuilder().CreateCall(valueToStringFunc, {argValue});
            }
            strPtr = codegen.getBuilder().CreateExtractValue(strValue, 0, "value.str.ptr");
            strLen = codegen.getBuilder().CreateExtractValue(strValue, 1, "value.str.len");
        } else if (argValue->getType()->isStructTy()) {
            // 提取结构体的第一个字段（字符串指针）
            strPtr = codegen.getBuilder().CreateExtractValue(argValue, 0, "str.ptr");
            strLen = codegen.getBuilder().CreateExtractValue(argValue, 1, "str.len");
        } else if (argValue->getType()->isPointerTy()) {
            // 如果语义类型是 str 且当前值是指针，先按字符串结构体解引用
            if (argExpr->getType() && argExpr->getType()->isString()) {
                llvm::Type* strTy = codegen.getLLVMType(argExpr->getType());
                if (strTy && strTy->isStructTy()) {
                    llvm::Value* loaded = codegen.getBuilder().CreateLoad(strTy, argValue, "str.load");
                    strPtr = codegen.getBuilder().CreateExtractValue(loaded, 0, "str.ptr");
                    strLen = codegen.getBuilder().CreateExtractValue(loaded, 1, "str.len");
                }
            }
            if (!strPtr) {
                // 如果已经是指针，直接使用
                strPtr = argValue;
            }
        } else {
            // 不支持的类型
            return nullptr;
        }

        llvm::LLVMContext& context = codegen.getContext();
        llvm::Module* module = codegen.getModule();
        llvm::Type* i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
        if (strPtr && strLen) {
            // 使用 printf("%.*s", len, ptr)，避免依赖平台相关 stdout 符号。
            llvm::Function* printfFunc = module->getFunction("printf");
            if (!printfFunc) {
                llvm::FunctionType* printfType = llvm::FunctionType::get(
                    llvm::Type::getInt32Ty(context),
                    i8PtrTy,
                    true
                );
                printfFunc = llvm::Function::Create(
                    printfType,
                    llvm::Function::ExternalLinkage,
                    "printf",
                    module
                );
            }

            llvm::Value* fmt = codegen.getBuilder().CreateGlobalStringPtr("%.*s", "print.fmt");
            llvm::Value* lenVal = strLen;
            if (!lenVal->getType()->isIntegerTy(32)) {
                lenVal = codegen.getBuilder().CreateSExtOrTrunc(
                    lenVal,
                    llvm::Type::getInt32Ty(context),
                    "str.len32"
                );
            }
            codegen.getBuilder().CreateCall(printfFunc, {fmt, lenVal, strPtr});
            return nullptr;
        }

        // 获取或声明 printf 函数
        llvm::Function* printfFunc = module->getFunction("printf");

        if (!printfFunc) {
            // 声明 printf: i32 (i8*, ...)
            llvm::FunctionType* printfType = llvm::FunctionType::get(
                llvm::Type::getInt32Ty(context),
                i8PtrTy,
                true  // 可变参数
            );
            printfFunc = llvm::Function::Create(
                printfType,
                llvm::Function::ExternalLinkage,
                "printf",
                module
            );
        }

        // 调用 printf
        codegen.getBuilder().CreateCall(printfFunc, {strPtr});

        return nullptr;
    }
};

/// \brief 创建 @print 内置函数处理器
std::unique_ptr<BuiltinHandler> createPrintBuiltin() {
    return std::make_unique<PrintBuiltin>();
}

} // namespace yuan
