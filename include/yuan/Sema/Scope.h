//===--- Scope.h - 作用域定义 ------------------------------------------===//
//
// Yuan 编译器
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief 作用域定义，用于符号表的层次结构管理
///
//===----------------------------------------------------------------------===//

#ifndef YUAN_SEMA_SCOPE_H
#define YUAN_SEMA_SCOPE_H

#include "yuan/Sema/Symbol.h"
#include "llvm/ADT/StringRef.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>

namespace yuan {

class ASTContext;
class FuncDecl;

/// 作用域
/// 表示程序中的一个作用域，管理该作用域内的符号
class Scope {
public:
    /// 作用域类型
    enum class Kind {
        Global,     ///< 全局作用域
        Module,     ///< 模块作用域
        Function,   ///< 函数作用域
        Block,      ///< 块作用域
        Struct,     ///< 结构体作用域
        Enum,       ///< 枚举作用域
        Trait,      ///< Trait 作用域
        Impl,       ///< Impl 块作用域
        Loop,       ///< 循环作用域（用于 break/continue 检查）
    };

    /// 构造作用域
    /// \param kind 作用域类型
    /// \param parent 父作用域，nullptr 表示顶层作用域
    Scope(Kind kind, Scope* parent = nullptr);

    /// 获取作用域类型
    Kind getKind() const { return ScopeKind; }

    /// 获取父作用域
    Scope* getParent() const { return Parent; }

    /// 添加符号到当前作用域
    /// \param sym 要添加的符号
    /// \return 成功返回 true，如果符号已存在返回 false
    bool addSymbol(Symbol* sym);

    /// 在当前作用域中查找符号（不包括父作用域）
    /// \param name 符号名称
    /// \return 找到的符号，未找到返回 nullptr
    Symbol* lookupLocal(llvm::StringRef name) const;

    /// 查找符号（包括父作用域）
    /// \param name 符号名称
    /// \return 找到的符号，未找到返回 nullptr
    Symbol* lookup(llvm::StringRef name) const;

    /// 获取当前作用域的所有符号
    const std::unordered_map<std::string, Symbol*>& getSymbols() const {
        return Symbols;
    }

    /// 检查是否在循环内
    bool isInLoop() const;

    /// 检查是否在函数内
    bool isInFunction() const;

    /// 获取当前函数（如果在函数内）
    FuncDecl* getCurrentFunction() const;

    /// 设置当前函数
    void setCurrentFunction(FuncDecl* func) { CurrentFunc = func; }

    /// 设置循环标签（仅对循环作用域有效）
    void setLoopLabel(const std::string& label) { LoopLabel = label; }

    /// 获取循环标签
    const std::string& getLoopLabel() const { return LoopLabel; }

    /// 是否有循环标签
    bool hasLoopLabel() const { return !LoopLabel.empty(); }

    /// 获取作用域类型的字符串表示
    static const char* getKindName(Kind kind);

private:
    Kind ScopeKind;                                         ///< 作用域类型
    Scope* Parent;                                          ///< 父作用域
    std::unordered_map<std::string, Symbol*> Symbols;      ///< 符号映射
    FuncDecl* CurrentFunc = nullptr;                        ///< 当前函数（仅在函数作用域有效）
    std::string LoopLabel;                                  ///< 循环标签（仅在循环作用域有效）
};

/// 符号表
/// 管理作用域的层次结构和符号查找
class SymbolTable {
public:
    /// 构造符号表
    explicit SymbolTable(ASTContext& ctx);

    /// 析构符号表
    ~SymbolTable();

    /// 进入新作用域
    /// \param kind 作用域类型
    void enterScope(Scope::Kind kind, const std::string& label = "");

    /// 离开当前作用域
    void exitScope();

    /// 获取当前作用域
    Scope* getCurrentScope() const { return CurrentScope; }

    /// 获取全局作用域
    Scope* getGlobalScope() const { return GlobalScope; }

    /// 添加符号到当前作用域
    /// \param sym 要添加的符号
    /// \return 成功返回 true，如果符号已存在返回 false
    bool addSymbol(Symbol* sym);

    /// 查找符号（从当前作用域开始向上查找）
    /// \param name 符号名称
    /// \return 找到的符号，未找到返回 nullptr
    Symbol* lookup(llvm::StringRef name) const;

    /// 查找类型符号
    /// \param name 类型名称
    /// \return 找到的类型符号，未找到返回 nullptr
    Symbol* lookupType(llvm::StringRef name) const;

    /// 获取作用域深度
    size_t getScopeDepth() const;

private:
    Scope* CurrentScope;                                ///< 当前作用域
    Scope* GlobalScope;                                 ///< 全局作用域
    std::vector<std::unique_ptr<Scope>> AllScopes;     ///< 所有作用域的存储
    ASTContext& Ctx;                                    ///< AST 上下文

    /// 注册内置类型到全局作用域
    void registerBuiltinTypes();
};

} // namespace yuan

#endif // YUAN_SEMA_SCOPE_H
