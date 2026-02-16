/// \file LocationBuiltin.cpp
/// \brief 位置信息内置函数实现。
///
/// 本文件实现以下内置函数：
/// - @file   - 返回当前文件名
/// - @line   - 返回当前行号
/// - @column - 返回当前列号
/// - @func   - 返回当前函数名

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

/// \brief 位置信息内置函数处理器基类
///
/// 所有位置信息内置函数共享相同的参数检查逻辑（无参数）。
class LocationBuiltinBase : public BuiltinHandler {
public:
    int getExpectedArgCount() const override { return 0; }
    
    std::string getArgDescription() const override {
        return "无参数";
    }
    
protected:
    /// \brief 检查参数数量
    bool checkArgs(BuiltinCallExpr* expr) const {
        return expr->getArgCount() == 0;
    }
};

/// \brief @file 内置函数处理器
///
/// 返回当前源文件的文件名。
class FileBuiltin : public LocationBuiltinBase {
public:
    const char* getName() const override { return "file"; }
    
    BuiltinKind getKind() const override { return BuiltinKind::File; }
    
    Type* analyze(BuiltinCallExpr* expr, Sema& sema) override {
        if (!checkArgs(expr)) {
            sema.getDiagnostics()
                .report(DiagID::err_wrong_builtin_argument_count, expr->getBeginLoc(), expr->getRange())
                << 0u << static_cast<unsigned>(expr->getArgCount());
            return nullptr;
        }
        
        // 返回 str 类型
        return sema.getContext().getStrType();
    }
    
    llvm::Value* generate(BuiltinCallExpr* expr, CodeGen& codegen) override {
        // 获取当前文件名并生成字符串常量
        SourceLocation loc = expr->getRange().getBegin();
        SourceManager& sm = codegen.getASTContext().getSourceManager();
        SourceManager::FileID fid = sm.getFileID(loc);
        const std::string& filename = sm.getFilename(fid);

        llvm::LLVMContext& context = codegen.getContext();
        llvm::Module* module = codegen.getModule();
        llvm::Constant* strConstant = llvm::ConstantDataArray::getString(
            context, filename, true
        );
        llvm::GlobalVariable* strGlobal = new llvm::GlobalVariable(
            *module,
            strConstant->getType(),
            true,
            llvm::GlobalValue::PrivateLinkage,
            strConstant,
            ".file.str"
        );

        llvm::Type* i8PtrType = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
        llvm::Type* i64Type = llvm::Type::getInt64Ty(context);
        llvm::StructType* stringType = llvm::StructType::get(context, {i8PtrType, i64Type});

        llvm::Constant* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
        llvm::Constant* indices[] = {zero, zero};
        llvm::Constant* strPtr = llvm::ConstantExpr::getGetElementPtr(
            strGlobal->getValueType(), strGlobal, indices
        );

        llvm::Constant* len = llvm::ConstantInt::get(i64Type, filename.size());
        return llvm::ConstantStruct::get(stringType, strPtr, len);
    }
};

/// \brief @line 内置函数处理器
///
/// 返回当前源代码的行号。
class LineBuiltin : public LocationBuiltinBase {
public:
    const char* getName() const override { return "line"; }
    
    BuiltinKind getKind() const override { return BuiltinKind::Line; }
    
    Type* analyze(BuiltinCallExpr* expr, Sema& sema) override {
        if (!checkArgs(expr)) {
            sema.getDiagnostics()
                .report(DiagID::err_wrong_builtin_argument_count, expr->getBeginLoc(), expr->getRange())
                << 0u << static_cast<unsigned>(expr->getArgCount());
            return nullptr;
        }
        
        // 返回 u32 类型
        return sema.getContext().getU32Type();
    }
    
    llvm::Value* generate(BuiltinCallExpr* expr, CodeGen& codegen) override {
        // 获取当前行号并生成整数常量
        SourceLocation loc = expr->getRange().getBegin();
        SourceManager& sm = codegen.getASTContext().getSourceManager();
        auto [line, col] = sm.getLineAndColumn(loc);
        (void)col;
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(codegen.getContext()), line);
    }
};

/// \brief @column 内置函数处理器
///
/// 返回当前源代码的列号。
class ColumnBuiltin : public LocationBuiltinBase {
public:
    const char* getName() const override { return "column"; }
    
    BuiltinKind getKind() const override { return BuiltinKind::Column; }
    
    Type* analyze(BuiltinCallExpr* expr, Sema& sema) override {
        if (!checkArgs(expr)) {
            sema.getDiagnostics()
                .report(DiagID::err_wrong_builtin_argument_count, expr->getBeginLoc(), expr->getRange())
                << 0u << static_cast<unsigned>(expr->getArgCount());
            return nullptr;
        }
        
        // 返回 u32 类型
        return sema.getContext().getU32Type();
    }
    
    llvm::Value* generate(BuiltinCallExpr* expr, CodeGen& codegen) override {
        // 获取当前列号并生成整数常量
        SourceLocation loc = expr->getRange().getBegin();
        SourceManager& sm = codegen.getASTContext().getSourceManager();
        auto [line, col] = sm.getLineAndColumn(loc);
        (void)line;
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(codegen.getContext()), col);
    }
};

/// \brief @func 内置函数处理器
///
/// 返回当前函数的名称。
class FuncBuiltin : public LocationBuiltinBase {
public:
    const char* getName() const override { return "func"; }
    
    BuiltinKind getKind() const override { return BuiltinKind::Func; }
    
    Type* analyze(BuiltinCallExpr* expr, Sema& sema) override {
        if (!checkArgs(expr)) {
            sema.getDiagnostics()
                .report(DiagID::err_wrong_builtin_argument_count, expr->getBeginLoc(), expr->getRange())
                << 0u << static_cast<unsigned>(expr->getArgCount());
            return nullptr;
        }
        
        // 返回 str 类型
        return sema.getContext().getStrType();
    }
    
    llvm::Value* generate(BuiltinCallExpr* expr, CodeGen& codegen) override {
        // 获取当前函数名并生成字符串常量
        std::string funcName = codegen.getCurrentFunctionName();

        llvm::LLVMContext& context = codegen.getContext();
        llvm::Module* module = codegen.getModule();
        llvm::Constant* strConstant = llvm::ConstantDataArray::getString(
            context, funcName, true
        );
        llvm::GlobalVariable* strGlobal = new llvm::GlobalVariable(
            *module,
            strConstant->getType(),
            true,
            llvm::GlobalValue::PrivateLinkage,
            strConstant,
            ".func.str"
        );

        llvm::Type* i8PtrType = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
        llvm::Type* i64Type = llvm::Type::getInt64Ty(context);
        llvm::StructType* stringType = llvm::StructType::get(context, {i8PtrType, i64Type});

        llvm::Constant* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
        llvm::Constant* indices[] = {zero, zero};
        llvm::Constant* strPtr = llvm::ConstantExpr::getGetElementPtr(
            strGlobal->getValueType(), strGlobal, indices
        );

        llvm::Constant* len = llvm::ConstantInt::get(i64Type, funcName.size());
        return llvm::ConstantStruct::get(stringType, strPtr, len);
    }
};

/// \brief 创建 @file 内置函数处理器
std::unique_ptr<BuiltinHandler> createFileBuiltin() {
    return std::make_unique<FileBuiltin>();
}

/// \brief 创建 @line 内置函数处理器
std::unique_ptr<BuiltinHandler> createLineBuiltin() {
    return std::make_unique<LineBuiltin>();
}

/// \brief 创建 @column 内置函数处理器
std::unique_ptr<BuiltinHandler> createColumnBuiltin() {
    return std::make_unique<ColumnBuiltin>();
}

/// \brief 创建 @func 内置函数处理器
std::unique_ptr<BuiltinHandler> createFuncBuiltin() {
    return std::make_unique<FuncBuiltin>();
}

} // namespace yuan
