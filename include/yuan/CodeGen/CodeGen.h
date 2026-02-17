/// \file CodeGen.h
/// \brief LLVM IR code generation.

#ifndef YUAN_CODEGEN_CODEGEN_H
#define YUAN_CODEGEN_CODEGEN_H

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace yuan {

class ASTContext;
class Decl;
class Expr;
class FuncDecl;
class Stmt;
class Pattern;
class Type;
class StructType;
class GenericInstanceType;

/// \brief LLVM IR code generator.
///
/// The CodeGen class is responsible for generating LLVM IR from Yuan's AST.
/// It maintains the LLVM context, module, and IR builder, and provides
/// methods for translating AST nodes into LLVM IR instructions.
class CodeGen {
    // Allow builtin handlers to access private members for code generation
    friend class BuiltinHandler;

public:
    /// \brief Construct a code generator for a specific module.
    /// \param ctx AST context containing the AST to generate code for.
    /// \param moduleName Name of the LLVM module to create.
    CodeGen(ASTContext& ctx, const std::string& moduleName);

    /// \brief Destructor.
    ~CodeGen();

    /// \brief Generate LLVM IR for the entire module.
    /// \return True if code generation succeeded, false otherwise.
    bool generate();

    /// \brief Get the generated LLVM module.
    /// \return The LLVM module.
    llvm::Module* getModule() const { return Module.get(); }

    /// \brief Get the AST context.
    /// \return The AST context.
    ASTContext& getASTContext() { return Ctx; }

    /// \brief Get the LLVM context.
    /// \return The LLVM context.
    llvm::LLVMContext& getContext() { return *Context; }

    /// \brief Emit LLVM IR to a string.
    /// \return The LLVM IR as a string.
    std::string emitIR() const;

    /// \brief Emit LLVM IR to a file.
    /// \param filename Path to the output file.
    /// \return True if emission succeeded, false otherwise.
    bool emitIRToFile(const std::string& filename) const;

    /// \brief Emit object code to a file.
    /// \param filename Path to the output file.
    /// \param optimizationLevel Optimization level (0-3).
    /// \return True if emission succeeded, false otherwise.
    bool emitObjectFile(const std::string& filename, unsigned optimizationLevel = 0);

    /// \brief Link object file to create executable.
    /// \param objectFile Path to the object file.
    /// \param executableFile Path to the output executable.
    /// \return True if linking succeeded, false otherwise.
    bool linkExecutable(const std::string& objectFile, const std::string& executableFile);

    /// \brief Verify the generated LLVM IR module.
    /// \param errorMsg Optional output parameter for error messages.
    /// \return True if verification succeeded, false if errors were found.
    bool verifyModule(std::string* errorMsg = nullptr) const;

    /// \brief Convert a Yuan semantic type to an LLVM type.
    /// \param type Yuan semantic type.
    /// \return Corresponding LLVM type, or nullptr if conversion fails.
    llvm::Type* getLLVMType(const Type* type);

    /// \brief Generate code for a declaration.
    /// \param decl Declaration to generate code for.
    /// \return True if code generation succeeded, false otherwise.
    bool generateDecl(Decl* decl);

    /// \brief Get the IR builder (for use by builtin handlers).
    /// \return Reference to the IR builder.
    llvm::IRBuilder<>& getBuilder() { return *Builder; }

    /// \brief Get current source-level function name.
    /// \return Current function name, or empty if not in a function.
    const std::string& getCurrentFunctionName() const { return CurrentFunctionName; }

    /// \brief Get LLVM symbol name for a function declaration.
    /// \param decl Function declaration.
    /// \return Mangled symbol name for codegen.
    std::string getFunctionSymbolName(const FuncDecl* decl) const;

    /// \brief Get LLVM symbol name for a global variable/constant declaration.
    /// \param decl Declaration node.
    /// \param baseName Source-level identifier.
    /// \param kind Symbol kind tag ('V' for var, 'C' for const).
    /// \return Mangled symbol name for codegen.
    std::string getGlobalSymbolName(const Decl* decl,
                                    const std::string& baseName,
                                    char kind) const;

    /// \brief Emit a constant string value ({ i8*, i64 }).
    /// \param value String contents.
    /// \return LLVM value representing Yuan string.
    llvm::Value* emitStringLiteralValue(const std::string& value);

    using GenericSubst = std::unordered_map<std::string, Type*>;

    /// \brief Substitute generic types using the active specialization mapping.
    Type* substituteType(Type* type) const;

    /// \brief Coerce a generic-storage value to a concrete target type.
    llvm::Value* coerceGenericValue(llvm::Value* value, Type* targetType);

    /// \brief Build a generic mapping from expected/actual types.
    bool unifyGenericTypes(Type* expected, Type* actual, GenericSubst& mapping) const;

    /// \brief Build mapping for a struct generic instance (if possible).
    bool buildStructGenericMapping(const StructType* baseStruct,
                                   const GenericInstanceType* genInst,
                                   GenericSubst& mapping) const;

    /// \brief Get or create a specialized function for the given mapping.
    llvm::Function* getOrCreateSpecializedFunction(FuncDecl* decl, const GenericSubst& mapping);

    /// \brief Generate code for an expression (for use by builtin handlers).
    /// \param expr Expression to generate code for.
    /// \return Generated LLVM value, or nullptr if generation fails.
    llvm::Value* generateExprPublic(Expr* expr) { return generateExpr(expr); }

private:
    // AST context
    ASTContext& Ctx;

    // LLVM infrastructure
    std::unique_ptr<llvm::LLVMContext> Context;
    std::unique_ptr<llvm::Module> Module;
    std::unique_ptr<llvm::IRBuilder<>> Builder;

    // Type cache (Yuan Type -> LLVM Type)
    std::unordered_map<const Type*, llvm::Type*> TypeCache;

    // Symbol table (AST Decl -> LLVM Value)
    std::unordered_map<const Decl*, llvm::Value*> ValueMap;

    // Current function being generated
    llvm::Function* CurrentFunction = nullptr;
    std::string CurrentFunctionName;
    FuncDecl* CurrentFuncDecl = nullptr;

    // Generic specialization context
    std::vector<GenericSubst> GenericSubstStack;
    const FuncDecl* ActiveSpecializationDecl = nullptr;
    std::string ActiveSpecializationName;

    // Generic struct parameter names for specialization
    std::unordered_map<const StructType*, std::vector<std::string>> StructGenericParams;

    // Loop context for break/continue
    struct LoopContext {
        llvm::BasicBlock* ContinueBlock;  ///< Block to jump to for continue
        llvm::BasicBlock* BreakBlock;     ///< Block to jump to for break
        std::string Label;                ///< Loop label (if any)
        size_t DeferDepth = 0;            ///< Defer stack depth at loop entry
    };
    std::vector<LoopContext> LoopStack;

    // Defer stack (stores deferred statements)
    std::vector<Stmt*> DeferStack;

    // Helper methods
    std::string mangleIdentifier(const std::string& text) const;
    std::string mangleDeclModule(const Decl* decl) const;
    std::string mangleDeclDiscriminator(const Decl* decl) const;
    std::string mangleTypeForSymbol(Type* type) const;
    std::string buildFunctionSymbolBase(const FuncDecl* decl) const;
    std::string buildSpecializationSuffix(const FuncDecl* decl, const GenericSubst& mapping) const;

    llvm::Type* convertBuiltinType(const Type* type);
    llvm::Type* convertArrayType(const Type* type);
    llvm::Type* convertSliceType(const Type* type);
    llvm::Type* convertTupleType(const Type* type);
    llvm::Type* convertPointerType(const Type* type);
    llvm::Type* convertReferenceType(const Type* type);
    llvm::Type* convertFunctionType(const Type* type);
    llvm::Type* convertStructType(const Type* type);
    llvm::Type* convertEnumType(const Type* type);
    llvm::Type* convertErrorType(const Type* type);
    llvm::Type* convertRangeType(const Type* type);
    llvm::Type* convertOptionalType(const Type* type);
    llvm::Type* convertValueType(const Type* type);
    llvm::Type* convertVarArgsType(const Type* type);

    // Declaration generation (implemented in CGDecl.cpp)
    bool generateVarDecl(class VarDecl* decl);
    bool generateConstDecl(class ConstDecl* decl);
    bool generateFuncDecl(class FuncDecl* decl);
    bool generateStructDecl(class StructDecl* decl);
    bool generateEnumDecl(class EnumDecl* decl);
    bool generateTraitDecl(class TraitDecl* decl);
    bool generateImplDecl(class ImplDecl* decl);

    // Expression generation (implemented in CGExpr.cpp)
    llvm::Value* generateExpr(Expr* expr);
    llvm::Value* generateLiteralExpr(Expr* expr);
    llvm::Value* generateIntegerLiteral(class IntegerLiteralExpr* expr);
    llvm::Value* generateFloatLiteral(class FloatLiteralExpr* expr);
    llvm::Value* generateBoolLiteral(class BoolLiteralExpr* expr);
    llvm::Value* generateCharLiteral(class CharLiteralExpr* expr);
    llvm::Value* generateStringLiteral(class StringLiteralExpr* expr);
    llvm::Value* generateIdentifierExpr(class IdentifierExpr* expr);
    llvm::Value* generateMemberExpr(class MemberExpr* expr);
    llvm::Value* generateBinaryExpr(class BinaryExpr* expr);
    llvm::Value* generateLogicalBinaryExpr(class BinaryExpr* expr);
    llvm::Value* generateUnaryExpr(class UnaryExpr* expr);
    llvm::Value* generateCastExpr(class CastExpr* expr);
    llvm::Value* generateAssignExpr(class AssignExpr* expr);
    llvm::Value* generateCallExpr(class CallExpr* expr);
    llvm::Value* generateIndexExpr(class IndexExpr* expr);
    llvm::Value* generateSliceExpr(class SliceExpr* expr);
    llvm::Value* generateStructExpr(class StructExpr* expr);
    llvm::Value* generateArrayExpr(class ArrayExpr* expr);
    llvm::Value* generateTupleExpr(class TupleExpr* expr);
    llvm::Value* generateClosureExpr(class ClosureExpr* expr);
    llvm::Value* generateAwaitExpr(class AwaitExpr* expr);
    llvm::Value* generateIfExpr(class IfExpr* expr);
    llvm::Value* generateMatchExpr(class MatchExpr* expr);
    llvm::Value* generateBlockExpr(class BlockExpr* expr);
    llvm::Value* generateErrorPropagateExpr(class ErrorPropagateExpr* expr);
    llvm::Value* generateErrorHandleExpr(class ErrorHandleExpr* expr);
    llvm::Value* generateBuiltinCallExpr(class BuiltinCallExpr* expr);
    llvm::Value* generateRangeExpr(class RangeExpr* expr);

    // Helper methods for lvalue handling
    llvm::Value* generateLValueAddress(Expr* expr);

    // VarArgs/Value helpers
    llvm::Value* buildValueFrom(Type* type, llvm::Value* value, Type* expectedElementType = nullptr);
    llvm::Value* convertValueToType(llvm::Value* value, Type* targetType);
    llvm::Value* callVarArgsGet(llvm::Value* varArgsValue, llvm::Value* index);
    llvm::Value* emitStringEquality(llvm::Value* lhs, llvm::Value* rhs);

    // Pattern binding helper
    bool bindPattern(Pattern* pattern, llvm::Value* value, Type* valueType);
    llvm::Value* generatePatternCondition(Pattern* pattern, llvm::Value* value, Type* valueType);

    // Statement generation (implemented in CGStmt.cpp)
    bool generateStmt(Stmt* stmt);
    bool generateDeclStmt(class DeclStmt* stmt);
    bool generateExprStmt(class ExprStmt* stmt);
    bool generateBlockStmt(class BlockStmt* stmt);
    llvm::Value* generateBlockStmtWithResult(class BlockStmt* stmt);
    bool generateReturnStmt(class ReturnStmt* stmt);
    bool generateIfStmt(class IfStmt* stmt);
    bool generateWhileStmt(class WhileStmt* stmt);
    bool generateLoopStmt(class LoopStmt* stmt);
    bool generateForStmt(class ForStmt* stmt);
    bool generateMatchStmt(class MatchStmt* stmt);
    llvm::Value* generateMatchStmtWithResult(class MatchStmt* stmt);
    bool generateBreakStmt(class BreakStmt* stmt);
    bool generateContinueStmt(class ContinueStmt* stmt);
    bool generateDeferStmt(class DeferStmt* stmt);

    // Helper methods for defer
    void executeDeferredStatements(size_t fromDepth = 0);

    // Symbol mangling caches
    mutable std::unordered_map<const FuncDecl*, std::string> FunctionSymbolCache;
    mutable std::unordered_map<const Decl*, std::string> GlobalSymbolCache;
};

} // namespace yuan

#endif // YUAN_CODEGEN_CODEGEN_H
