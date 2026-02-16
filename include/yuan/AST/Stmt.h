/// \file Stmt.h
/// \brief 语句 AST 节点定义。
///
/// 本文件定义了所有语句相关的 AST 节点，包括表达式语句、块语句、
/// 控制流语句、跳转语句和延迟语句等。

#ifndef YUAN_AST_STMT_H
#define YUAN_AST_STMT_H

#include "yuan/AST/AST.h"
#include <string>
#include <vector>

namespace yuan {

// 前向声明
class Expr;
class Pattern;
class BlockStmt;

/// \brief 语句节点基类
///
/// 所有语句节点都继承自此类。语句节点表示程序中的各种语句，
/// 如表达式语句、控制流语句、跳转语句等。
class Stmt : public ASTNode {
public:
    /// \brief 构造语句节点
    /// \param kind 节点类型
    /// \param range 源码范围
    Stmt(Kind kind, SourceRange range)
        : ASTNode(kind, range) {}
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->isStmt();
    }
};


// ============================================================================
// 基本语句
// ============================================================================

/// \brief 声明语句
///
/// 将声明作为语句使用，例如函数体内的变量声明。
class DeclStmt : public Stmt {
public:
    /// \brief 构造声明语句
    /// \param range 源码范围
    /// \param decl 关联的声明
    DeclStmt(SourceRange range, class Decl* decl)
        : Stmt(Kind::DeclStmt, range), DeclNode(decl) {}

    /// \brief 获取关联的声明
    class Decl* getDecl() const { return DeclNode; }

    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::DeclStmt;
    }

private:
    class Decl* DeclNode;  ///< 关联的声明
};

/// \brief 表达式语句
///
/// 将表达式作为语句使用，例如：
/// - foo();
/// - x + y;
class ExprStmt : public Stmt {
public:
    /// \brief 构造表达式语句
    /// \param range 源码范围
    /// \param expr 表达式
    ExprStmt(SourceRange range, Expr* expr);
    
    /// \brief 获取表达式
    Expr* getExpr() const { return Expression; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::ExprStmt;
    }
    
private:
    Expr* Expression;  ///< 表达式
};

/// \brief 块语句
///
/// 表示由花括号包围的语句序列，例如：
/// - { stmt1; stmt2; }
class BlockStmt : public Stmt {
public:
    /// \brief 构造块语句
    /// \param range 源码范围
    /// \param stmts 语句列表
    BlockStmt(SourceRange range, std::vector<Stmt*> stmts);
    
    /// \brief 获取语句列表
    const std::vector<Stmt*>& getStatements() const { return Stmts; }
    
    /// \brief 获取语句数量
    size_t getStatementCount() const { return Stmts.size(); }
    
    /// \brief 是否为空块
    bool isEmpty() const { return Stmts.empty(); }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::BlockStmt;
    }
    
private:
    std::vector<Stmt*> Stmts;  ///< 语句列表
};

/// \brief Return 语句
///
/// 表示函数返回语句，例如：
/// - return;
/// - return value;
class ReturnStmt : public Stmt {
public:
    /// \brief 构造 Return 语句
    /// \param range 源码范围
    /// \param value 返回值（可为 nullptr）
    ReturnStmt(SourceRange range, Expr* value = nullptr);
    
    /// \brief 获取返回值
    Expr* getValue() const { return Value; }

    /// \brief 设置返回值
    void setValue(Expr* value) { Value = value; }
    
    /// \brief 是否有返回值
    bool hasValue() const { return Value != nullptr; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::ReturnStmt;
    }
    
private:
    Expr* Value;  ///< 返回值
};


// ============================================================================
// 控制流语句
// ============================================================================

/// \brief If 语句
///
/// 表示条件分支语句，例如：
/// - if condition { ... }
/// - if cond1 { ... } elif cond2 { ... } else { ... }
class IfStmt : public Stmt {
public:
    /// \brief 分支结构
    struct Branch {
        Expr* Condition;    ///< 条件（else 分支为 nullptr）
        BlockStmt* Body;    ///< 分支体
    };
    
    /// \brief 构造 If 语句
    /// \param range 源码范围
    /// \param branches 分支列表
    IfStmt(SourceRange range, std::vector<Branch> branches);
    
    /// \brief 获取分支列表
    const std::vector<Branch>& getBranches() const { return Branches; }
    
    /// \brief 是否有 else 分支
    bool hasElse() const {
        return !Branches.empty() && Branches.back().Condition == nullptr;
    }
    
    /// \brief 获取条件表达式（第一个分支）
    Expr* getCondition() const {
        return Branches.empty() ? nullptr : Branches[0].Condition;
    }
    
    /// \brief 获取 then 分支体
    BlockStmt* getThenBody() const {
        return Branches.empty() ? nullptr : Branches[0].Body;
    }
    
    /// \brief 获取 else 分支体
    BlockStmt* getElseBody() const {
        if (hasElse()) {
            return Branches.back().Body;
        }
        return nullptr;
    }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::IfStmt;
    }
    
private:
    std::vector<Branch> Branches;  ///< 分支列表
};

/// \brief While 语句
///
/// 表示 while 循环，例如：
/// - while condition { ... }
class WhileStmt : public Stmt {
public:
    /// \brief 构造 While 语句
    /// \param range 源码范围
    /// \param condition 循环条件
    /// \param body 循环体
    /// \param label 可选循环标签
    WhileStmt(SourceRange range, Expr* condition, BlockStmt* body,
              const std::string& label = "");
    
    /// \brief 获取循环条件
    Expr* getCondition() const { return Condition; }
    
    /// \brief 获取循环体
    BlockStmt* getBody() const { return Body; }

    /// \brief 获取循环标签
    const std::string& getLabel() const { return Label; }

    /// \brief 是否有标签
    bool hasLabel() const { return !Label.empty(); }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::WhileStmt;
    }
    
private:
    Expr* Condition;    ///< 循环条件
    BlockStmt* Body;    ///< 循环体
    std::string Label;  ///< 循环标签
};

/// \brief Loop 语句
///
/// 表示无限循环，例如：
/// - loop { ... }
class LoopStmt : public Stmt {
public:
    /// \brief 构造 Loop 语句
    /// \param range 源码范围
    /// \param body 循环体
    /// \param label 可选循环标签
    LoopStmt(SourceRange range, BlockStmt* body, const std::string& label = "");
    
    /// \brief 获取循环体
    BlockStmt* getBody() const { return Body; }

    /// \brief 获取循环标签
    const std::string& getLabel() const { return Label; }

    /// \brief 是否有标签
    bool hasLabel() const { return !Label.empty(); }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::LoopStmt;
    }
    
private:
    BlockStmt* Body;  ///< 循环体
    std::string Label;  ///< 循环标签
};

/// \brief For 语句
///
/// 表示 for-in 循环，例如：
/// - for i in 0..10 { ... }
/// - for (x, y) in points { ... }
class ForStmt : public Stmt {
public:
    /// \brief 构造 For 语句
    /// \param range 源码范围
    /// \param pattern 循环变量模式
    /// \param iterable 可迭代表达式
    /// \param body 循环体
    /// \param label 可选循环标签
    ForStmt(SourceRange range, Pattern* pattern, Expr* iterable, BlockStmt* body,
            const std::string& label = "");
    
    /// \brief 获取循环变量模式
    Pattern* getPattern() const { return Pat; }
    
    /// \brief 获取可迭代表达式
    Expr* getIterable() const { return Iterable; }
    
    /// \brief 获取循环体
    BlockStmt* getBody() const { return Body; }

    /// \brief 获取循环标签
    const std::string& getLabel() const { return Label; }

    /// \brief 是否有标签
    bool hasLabel() const { return !Label.empty(); }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::ForStmt;
    }
    
private:
    Pattern* Pat;       ///< 循环变量模式
    Expr* Iterable;     ///< 可迭代表达式
    BlockStmt* Body;    ///< 循环体
    std::string Label;  ///< 循环标签
};

/// \brief Match 语句
///
/// 表示模式匹配语句，例如：
/// - match value { pattern1 => { ... }, pattern2 => { ... } }
class MatchStmt : public Stmt {
public:
    /// \brief 匹配分支
    struct Arm {
        Pattern* Pat;       ///< 模式
        Expr* Guard;        ///< 守卫条件（可为 nullptr）
        Stmt* Body;         ///< 分支体（BlockStmt 或 ExprStmt）
    };
    
    /// \brief 构造 Match 语句
    /// \param range 源码范围
    /// \param scrutinee 被匹配的表达式
    /// \param arms 匹配分支列表
    MatchStmt(SourceRange range, Expr* scrutinee, std::vector<Arm> arms);
    
    /// \brief 获取被匹配的表达式
    Expr* getScrutinee() const { return Scrutinee; }
    
    /// \brief 获取匹配分支列表
    const std::vector<Arm>& getArms() const { return Arms; }
    
    /// \brief 获取分支数量
    size_t getArmCount() const { return Arms.size(); }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::MatchStmt;
    }
    
private:
    Expr* Scrutinee;        ///< 被匹配的表达式
    std::vector<Arm> Arms;  ///< 匹配分支列表
};


// ============================================================================
// 跳转和延迟语句
// ============================================================================

/// \brief Break 语句
///
/// 表示跳出循环，例如：
/// - break;
/// - break 'label;
class BreakStmt : public Stmt {
public:
    /// \brief 构造 Break 语句
    /// \param range 源码范围
    /// \param label 标签（可为空）
    BreakStmt(SourceRange range, const std::string& label = "");
    
    /// \brief 获取标签
    const std::string& getLabel() const { return Label; }
    
    /// \brief 是否有标签
    bool hasLabel() const { return !Label.empty(); }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::BreakStmt;
    }
    
private:
    std::string Label;  ///< 标签
};

/// \brief Continue 语句
///
/// 表示继续下一次循环，例如：
/// - continue;
/// - continue 'label;
class ContinueStmt : public Stmt {
public:
    /// \brief 构造 Continue 语句
    /// \param range 源码范围
    /// \param label 标签（可为空）
    ContinueStmt(SourceRange range, const std::string& label = "");
    
    /// \brief 获取标签
    const std::string& getLabel() const { return Label; }
    
    /// \brief 是否有标签
    bool hasLabel() const { return !Label.empty(); }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::ContinueStmt;
    }
    
private:
    std::string Label;  ///< 标签
};

/// \brief Defer 语句
///
/// 表示延迟执行的语句，在作用域退出时执行，例如：
/// - defer { cleanup(); }
/// - defer file.close();
class DeferStmt : public Stmt {
public:
    /// \brief 构造 Defer 语句
    /// \param range 源码范围
    /// \param body 延迟执行的语句
    DeferStmt(SourceRange range, Stmt* body);
    
    /// \brief 获取延迟执行的语句
    Stmt* getBody() const { return Body; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::DeferStmt;
    }
    
private:
    Stmt* Body;  ///< 延迟执行的语句
};

} // namespace yuan

#endif // YUAN_AST_STMT_H
