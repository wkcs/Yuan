/// \file Expr.h
/// \brief 表达式 AST 节点定义。
///
/// 本文件定义了所有表达式相关的 AST 节点，包括字面量、标识符、
/// 运算符表达式、函数调用、控制流表达式等。

#ifndef YUAN_AST_EXPR_H
#define YUAN_AST_EXPR_H

#include "yuan/AST/AST.h"
#include <string>
#include <vector>
#include <optional>

namespace yuan {

// 前向声明
class TypeNode;
class Type;
class Stmt;
class BlockStmt;
class ParamDecl;
class Pattern;
class FieldDecl;

/// \brief 表达式节点基类
///
/// 所有表达式节点都继承自此类。表达式节点表示程序中的各种表达式，
/// 如字面量、标识符、运算符表达式、函数调用等。
class Expr : public ASTNode {
public:
    /// \brief 构造表达式节点
    /// \param kind 节点类型
    /// \param range 源码范围
    Expr(Kind kind, SourceRange range)
        : ASTNode(kind, range), ExprType(nullptr) {}
    
    /// \brief 获取表达式类型（语义分析后设置）
    Type* getType() const { return ExprType; }
    
    /// \brief 设置表达式类型
    void setType(Type* type) { ExprType = type; }
    
    /// \brief 是否为左值
    virtual bool isLValue() const { return false; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->isExpr();
    }
    
protected:
    Type* ExprType;  ///< 表达式类型（语义分析后设置）
};


// ============================================================================
// 字面量表达式
// ============================================================================

/// \brief 整数字面量表达式
///
/// 表示整数字面量，例如：
/// - 42
/// - 0xFF
/// - 100_000i64
class IntegerLiteralExpr : public Expr {
public:
    /// \brief 构造整数字面量
    /// \param range 源码范围
    /// \param value 整数值
    /// \param isSigned 是否有符号
    /// \param bitWidth 位宽（0 表示未指定）
    /// \param hasTypeSuffix 是否显式包含类型后缀
    /// \param isPointerSizedSuffix 是否为 isize/usize 后缀
    IntegerLiteralExpr(SourceRange range, uint64_t value,
                       bool isSigned, unsigned bitWidth,
                       bool hasTypeSuffix = false,
                       bool isPointerSizedSuffix = false);
    
    /// \brief 获取整数值
    uint64_t getValue() const { return Value; }
    
    /// \brief 是否有符号
    bool isSigned() const { return IsSigned; }
    
    /// \brief 获取位宽
    unsigned getBitWidth() const { return BitWidth; }
    
    /// \brief 是否指定了类型后缀
    bool hasTypeSuffix() const { return HasTypeSuffix; }

    /// \brief 是否为指针宽度后缀（isize/usize）
    bool isPointerSizedSuffix() const { return IsPointerSizedSuffix; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::IntegerLiteralExpr;
    }
    
private:
    uint64_t Value;     ///< 整数值
    bool IsSigned;      ///< 是否有符号
    unsigned BitWidth;  ///< 位宽（0 表示未指定）
    bool HasTypeSuffix; ///< 是否显式包含类型后缀
    bool IsPointerSizedSuffix; ///< 是否使用 isize/usize 后缀
};

/// \brief 浮点数字面量表达式
///
/// 表示浮点数字面量，例如：
/// - 3.14
/// - 1.0e-10
/// - 2.5f32
class FloatLiteralExpr : public Expr {
public:
    /// \brief 构造浮点数字面量
    /// \param range 源码范围
    /// \param value 浮点数值
    /// \param bitWidth 位宽（32 或 64，0 表示未指定）
    FloatLiteralExpr(SourceRange range, double value, unsigned bitWidth);
    
    /// \brief 获取浮点数值
    double getValue() const { return Value; }
    
    /// \brief 获取位宽
    unsigned getBitWidth() const { return BitWidth; }
    
    /// \brief 是否指定了类型后缀
    bool hasTypeSuffix() const { return BitWidth != 0; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::FloatLiteralExpr;
    }
    
private:
    double Value;       ///< 浮点数值
    unsigned BitWidth;  ///< 位宽（0 表示未指定）
};


/// \brief 布尔字面量表达式
///
/// 表示布尔字面量：true 或 false
class BoolLiteralExpr : public Expr {
public:
    /// \brief 构造布尔字面量
    /// \param range 源码范围
    /// \param value 布尔值
    BoolLiteralExpr(SourceRange range, bool value);
    
    /// \brief 获取布尔值
    bool getValue() const { return Value; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::BoolLiteralExpr;
    }
    
private:
    bool Value;  ///< 布尔值
};

/// \brief 字符字面量表达式
///
/// 表示字符字面量，例如：
/// - 'a'
/// - '\n'
/// - '\u{1F600}'
class CharLiteralExpr : public Expr {
public:
    /// \brief 构造字符字面量
    /// \param range 源码范围
    /// \param codepoint Unicode 码点
    CharLiteralExpr(SourceRange range, uint32_t codepoint);
    
    /// \brief 获取 Unicode 码点
    uint32_t getCodepoint() const { return Codepoint; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::CharLiteralExpr;
    }
    
private:
    uint32_t Codepoint;  ///< Unicode 码点
};

/// \brief 字符串字面量表达式
///
/// 表示字符串字面量，例如：
/// - "hello"
/// - r"raw string"
/// - """multiline"""
class StringLiteralExpr : public Expr {
public:
    /// \brief 字符串类型
    enum class StringKind {
        Normal,     ///< 普通字符串 "..."
        Raw,        ///< 原始字符串 r"..."
        Multiline   ///< 多行字符串 """..."""
    };
    
    /// \brief 构造字符串字面量
    /// \param range 源码范围
    /// \param value 字符串值（已处理转义）
    /// \param kind 字符串类型
    StringLiteralExpr(SourceRange range, const std::string& value,
                      StringKind kind = StringKind::Normal);
    
    /// \brief 获取字符串值
    const std::string& getValue() const { return Value; }
    
    /// \brief 获取字符串类型
    StringKind getStringKind() const { return Kind_; }
    
    /// \brief 是否为原始字符串
    bool isRaw() const { return Kind_ == StringKind::Raw; }
    
    /// \brief 是否为多行字符串
    bool isMultiline() const { return Kind_ == StringKind::Multiline; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::StringLiteralExpr;
    }
    
private:
    std::string Value;  ///< 字符串值
    StringKind Kind_;   ///< 字符串类型
};


/// \brief None 字面量表达式
///
/// 表示 None 字面量，用于 Optional 类型
class NoneLiteralExpr : public Expr {
public:
    /// \brief 构造 None 字面量
    /// \param range 源码范围
    explicit NoneLiteralExpr(SourceRange range);
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::NoneLiteralExpr;
    }
};

// ============================================================================
// 标识符和成员访问表达式
// ============================================================================

/// \brief 标识符表达式
///
/// 表示标识符引用，例如：
/// - x
/// - foo
/// - MyStruct
class IdentifierExpr : public Expr {
public:
    /// \brief 构造标识符表达式
    /// \param range 源码范围
    /// \param name 标识符名称
    IdentifierExpr(SourceRange range, const std::string& name);
    
    /// \brief 获取标识符名称
    const std::string& getName() const { return Name; }

    /// \brief 设置解析后的声明（由 Sema 调用）
    void setResolvedDecl(class Decl* decl) { ResolvedDecl = decl; }

    /// \brief 获取解析后的声明
    class Decl* getResolvedDecl() const { return ResolvedDecl; }

    /// \brief 是否为左值
    bool isLValue() const override { return true; }

    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::IdentifierExpr;
    }

private:
    std::string Name;  ///< 标识符名称
    class Decl* ResolvedDecl = nullptr;  ///< 解析后的声明（由 Sema 设置）
};

/// \brief 成员访问表达式
///
/// 表示成员访问，例如：
/// - point.x
/// - obj.method
class MemberExpr : public Expr {
public:
    /// \brief 构造成员访问表达式
    /// \param range 源码范围
    /// \param base 基础表达式
    /// \param member 成员名称
    MemberExpr(SourceRange range, Expr* base, const std::string& member);

    /// \brief 获取基础表达式
    Expr* getBase() const { return Base; }

    /// \brief 获取成员名称
    const std::string& getMember() const { return Member; }

    /// \brief 设置解析后的声明（由 Sema 调用）
    void setResolvedDecl(class Decl* decl) { ResolvedDecl = decl; }

    /// \brief 获取解析后的声明
    class Decl* getResolvedDecl() const { return ResolvedDecl; }

    /// \brief 是否为左值
    bool isLValue() const override { return true; }

    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::MemberExpr;
    }

private:
    Expr* Base;         ///< 基础表达式
    std::string Member; ///< 成员名称
    class Decl* ResolvedDecl = nullptr;  ///< 解析后的声明（由 Sema 设置）
};

/// \brief 可选链表达式
///
/// 表示可选链操作，例如：
/// - obj?.method()
/// - value?.field
class OptionalChainingExpr : public Expr {
public:
    /// \brief 构造可选链表达式
    /// \param range 源码范围
    /// \param base 基础表达式
    /// \param member 成员名称
    OptionalChainingExpr(SourceRange range, Expr* base, const std::string& member)
        : Expr(Kind::OptionalChainingExpr, range), Base(base), Member(member) {}
    
    /// \brief 获取基础表达式
    Expr* getBase() const { return Base; }
    
    /// \brief 获取成员名称
    const std::string& getMember() const { return Member; }
    
    /// \brief 是否为左值
    bool isLValue() const override { return false; } // 可选链结果不是左值
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::OptionalChainingExpr;
    }
    
private:
    Expr* Base;         ///< 基础表达式
    std::string Member; ///< 成员名称
};


// ============================================================================
// 运算符表达式
// ============================================================================

/// \brief 二元表达式
///
/// 表示二元运算，例如：
/// - a + b
/// - x == y
/// - left && right
class BinaryExpr : public Expr {
public:
    /// \brief 二元运算符类型
    enum class Op {
        // 算术运算符
        Add,        ///< +
        Sub,        ///< -
        Mul,        ///< *
        Div,        ///< /
        Mod,        ///< %
        
        // 位运算符
        BitAnd,     ///< &
        BitOr,      ///< |
        BitXor,     ///< ^
        Shl,        ///< <<
        Shr,        ///< >>
        
        // 逻辑运算符
        And,        ///< &&
        Or,         ///< ||
        
        // 比较运算符
        Eq,         ///< ==
        Ne,         ///< !=
        Lt,         ///< <
        Le,         ///< <=
        Gt,         ///< >
        Ge,         ///< >=
        
        // 范围运算符
        Range,          ///< ..
        RangeInclusive, ///< ..=
        
        // 其他
        OrElse,     ///< orelse
    };
    
    /// \brief 构造二元表达式
    /// \param range 源码范围
    /// \param op 运算符
    /// \param lhs 左操作数
    /// \param rhs 右操作数
    BinaryExpr(SourceRange range, Op op, Expr* lhs, Expr* rhs);
    
    /// \brief 获取运算符
    Op getOp() const { return Operator; }
    
    /// \brief 获取左操作数
    Expr* getLHS() const { return LHS; }
    
    /// \brief 获取右操作数
    Expr* getRHS() const { return RHS; }
    
    /// \brief 获取运算符的字符串表示
    static const char* getOpSpelling(Op op);
    
    /// \brief 获取运算符名称
    static const char* getOpName(Op op);
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::BinaryExpr;
    }
    
private:
    Op Operator;  ///< 运算符
    Expr* LHS;    ///< 左操作数
    Expr* RHS;    ///< 右操作数
};


/// \brief 一元表达式
///
/// 表示一元运算，例如：
/// - -x
/// - !flag
/// - &value
/// - *ptr
class UnaryExpr : public Expr {
public:
    /// \brief 一元运算符类型
    enum class Op {
        Neg,        ///< - (取负)
        Not,        ///< ! (逻辑非)
        BitNot,     ///< ~ (位取反)
        Ref,        ///< & (取引用)
        RefMut,     ///< &mut (取可变引用)
        Deref,      ///< * (解引用)
    };
    
    /// \brief 构造一元表达式
    /// \param range 源码范围
    /// \param op 运算符
    /// \param operand 操作数
    UnaryExpr(SourceRange range, Op op, Expr* operand);
    
    /// \brief 获取运算符
    Op getOp() const { return Operator; }
    
    /// \brief 获取操作数
    Expr* getOperand() const { return Operand; }
    
    /// \brief 是否为左值
    bool isLValue() const override {
        // 解引用表达式是左值
        return Operator == Op::Deref;
    }
    
    /// \brief 获取运算符的字符串表示
    static const char* getOpSpelling(Op op);
    
    /// \brief 获取运算符名称
    static const char* getOpName(Op op);
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::UnaryExpr;
    }
    
private:
    Op Operator;    ///< 运算符
    Expr* Operand;  ///< 操作数
};

/// \brief 赋值表达式
///
/// 表示赋值运算，例如：
/// - x = 10
/// - arr[0] = value
/// - x += 1
class AssignExpr : public Expr {
public:
    /// \brief 赋值运算符类型
    enum class Op {
        Assign,         ///< =
        AddAssign,      ///< +=
        SubAssign,      ///< -=
        MulAssign,      ///< *=
        DivAssign,      ///< /=
        ModAssign,      ///< %=
        BitAndAssign,   ///< &=
        BitOrAssign,    ///< |=
        BitXorAssign,   ///< ^=
        ShlAssign,      ///< <<=
        ShrAssign,      ///< >>=
    };
    
    /// \brief 构造赋值表达式
    /// \param range 源码范围
    /// \param op 赋值运算符
    /// \param target 赋值目标（左值）
    /// \param value 赋值值
    AssignExpr(SourceRange range, Op op, Expr* target, Expr* value);
    
    /// \brief 获取赋值运算符
    Op getOp() const { return Operator; }
    
    /// \brief 获取赋值目标
    Expr* getTarget() const { return Target; }
    
    /// \brief 获取赋值值
    Expr* getValue() const { return Value; }

    /// \brief 设置赋值值
    void setValue(Expr* value) { Value = value; }
    
    /// \brief 是否为复合赋值
    bool isCompound() const { return Operator != Op::Assign; }
    
    /// \brief 获取运算符的字符串表示
    static const char* getOpSpelling(Op op);
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::AssignExpr;
    }
    
private:
    Op Operator;    ///< 赋值运算符
    Expr* Target;   ///< 赋值目标
    Expr* Value;    ///< 赋值值
};


// ============================================================================
// 调用和索引表达式
// ============================================================================

/// \brief 函数调用表达式
///
/// 表示函数调用，例如：
/// - foo()
/// - add(1, 2)
/// - obj.method(arg)
class CallExpr : public Expr {
public:
    /// \brief 调用参数项（支持普通参数与 spread 参数）
    struct Arg {
        Expr* Value;
        bool IsSpread;

        explicit Arg(Expr* value, bool isSpread = false)
            : Value(value), IsSpread(isSpread) {}
    };

    /// \brief 构造函数调用表达式
    /// \param range 源码范围
    /// \param callee 被调用的表达式
    /// \param args 参数列表
    /// \param typeArgs 泛型类型实参（可为空）
    CallExpr(SourceRange range, Expr* callee, std::vector<Arg> args,
             std::vector<TypeNode*> typeArgs = {});

    /// \brief 向后兼容：从普通参数列表构造（默认均为非 spread）
    CallExpr(SourceRange range, Expr* callee, std::vector<Expr*> args,
             std::vector<TypeNode*> typeArgs = {});
    
    /// \brief 获取被调用的表达式
    Expr* getCallee() const { return Callee; }
    
    /// \brief 获取参数列表
    const std::vector<Arg>& getArgs() const { return Args; }

    /// \brief 获取参数列表（可变）
    std::vector<Arg>& getArgsMutable() { return Args; }
    
    /// \brief 获取参数数量
    size_t getArgCount() const { return Args.size(); }

    /// \brief 获取泛型类型实参
    const std::vector<TypeNode*>& getTypeArgs() const { return TypeArgs; }

    /// \brief 泛型实参数量
    size_t getTypeArgCount() const { return TypeArgs.size(); }

    /// \brief 是否有泛型实参
    bool hasTypeArgs() const { return !TypeArgs.empty(); }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::CallExpr;
    }
    
private:
    static std::vector<Arg> toArgVector(std::vector<Expr*> args);

    Expr* Callee;              ///< 被调用的表达式
    std::vector<Arg> Args;     ///< 参数列表
    std::vector<TypeNode*> TypeArgs; ///< 泛型类型实参
};

/// \brief 索引表达式
///
/// 表示数组/切片索引，例如：
/// - arr[0]
/// - matrix[i][j]
class IndexExpr : public Expr {
public:
    /// \brief 构造索引表达式
    /// \param range 源码范围
    /// \param base 基础表达式
    /// \param index 索引表达式
    IndexExpr(SourceRange range, Expr* base, Expr* index);
    
    /// \brief 获取基础表达式
    Expr* getBase() const { return Base; }
    
    /// \brief 获取索引表达式
    Expr* getIndex() const { return Index; }
    
    /// \brief 是否为左值
    bool isLValue() const override { return true; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::IndexExpr;
    }
    
private:
    Expr* Base;   ///< 基础表达式
    Expr* Index;  ///< 索引表达式
};

/// \brief 切片表达式
///
/// 表示切片操作，例如：
/// - arr[1..5]
/// - arr[..n]
/// - arr[start..]
class SliceExpr : public Expr {
public:
    /// \brief 构造切片表达式
    /// \param range 源码范围
    /// \param base 基础表达式
    /// \param start 起始索引（可为 nullptr）
    /// \param end 结束索引（可为 nullptr）
    /// \param isInclusive 是否包含结束索引
    SliceExpr(SourceRange range, Expr* base, Expr* start, Expr* end,
              bool isInclusive = false);
    
    /// \brief 获取基础表达式
    Expr* getBase() const { return Base; }
    
    /// \brief 获取起始索引
    Expr* getStart() const { return Start; }
    
    /// \brief 获取结束索引
    Expr* getEnd() const { return End; }
    
    /// \brief 是否包含结束索引
    bool isInclusive() const { return IsInclusive; }
    
    /// \brief 是否有起始索引
    bool hasStart() const { return Start != nullptr; }
    
    /// \brief 是否有结束索引
    bool hasEnd() const { return End != nullptr; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::SliceExpr;
    }
    
private:
    Expr* Base;         ///< 基础表达式
    Expr* Start;        ///< 起始索引
    Expr* End;          ///< 结束索引
    bool IsInclusive;   ///< 是否包含结束索引
};


// ============================================================================
// 内置函数调用表达式
// ============================================================================

/// \brief 内置函数类型
enum class BuiltinKind {
    Import,     ///< @import - 导入模块
    Sizeof,     ///< @sizeof - 获取类型大小
    Typeof,     ///< @typeof - 获取类型信息
    PlatformOs, ///< @platform_os - 目标操作系统
    PlatformArch, ///< @platform_arch - 目标架构
    PlatformPointerBits, ///< @platform_pointer_bits - 指针位宽
    Panic,      ///< @panic - 触发 panic
    Assert,     ///< @assert - 断言
    Alignof,    ///< @alignof - 获取类型对齐
    File,       ///< @file - 当前文件名
    Line,       ///< @line - 当前行号
    Column,     ///< @column - 当前列号
    Func,       ///< @func - 当前函数名
    Print,      ///< @print - 打印到标准输出
    Format,     ///< @format - 格式化字符串（类似 C++ std::format）
    Alloc,      ///< @alloc - 申请内存
    Realloc,    ///< @realloc - 重新分配内存
    Free,       ///< @free - 释放内存
    Memcpy,     ///< @memcpy - 内存拷贝
    Memmove,    ///< @memmove - 内存移动
    Memset,     ///< @memset - 内存填充
    StrFromParts, ///< @str_from_parts - 从指针+长度构造 str
    Slice,      ///< @slice - 从指针+长度构造切片
    AsyncSchedulerCreate,       ///< @async_scheduler_create - 创建调度器
    AsyncSchedulerDestroy,      ///< @async_scheduler_destroy - 销毁调度器
    AsyncSchedulerSetCurrent,   ///< @async_scheduler_set_current - 设置当前调度器
    AsyncSchedulerCurrent,      ///< @async_scheduler_current - 获取当前调度器
    AsyncSchedulerRunOne,       ///< @async_scheduler_run_one - 执行一个任务
    AsyncSchedulerRunUntilIdle, ///< @async_scheduler_run_until_idle - 执行直到空闲
    AsyncPromiseCreate,         ///< @async_promise_create - 创建 Promise
    AsyncPromiseRetain,         ///< @async_promise_retain - Promise 引用计数 +1
    AsyncPromiseRelease,        ///< @async_promise_release - Promise 引用计数 -1
    AsyncPromiseStatus,         ///< @async_promise_status - 查询 Promise 状态
    AsyncPromiseValue,          ///< @async_promise_value - 读取成功值
    AsyncPromiseError,          ///< @async_promise_error - 读取错误值
    AsyncPromiseResolve,        ///< @async_promise_resolve - 完成 Promise
    AsyncPromiseReject,         ///< @async_promise_reject - 失败 Promise
    AsyncPromiseAwait,          ///< @async_promise_await - 等待 Promise
    AsyncStep,                  ///< @async_step - 单步推进异步执行
    AsyncStepCount,             ///< @async_step_count - 读取异步步数
    OsTimeUnixNanos,            ///< @os_time_unix_nanos - 当前 unix 时间（纳秒）
    OsSleepNanos,               ///< @os_sleep_nanos - 休眠纳秒
    OsYield,                    ///< @os_yield - 让出线程
    OsThreadSpawn,              ///< @os_thread_spawn - 启动后台线程
    OsThreadIsFinished,         ///< @os_thread_is_finished - 查询线程是否结束
    OsThreadJoin,               ///< @os_thread_join - 等待线程结束并释放句柄
    OsReadFile,                 ///< @os_read_file - 读取文件内容
    OsWriteFile,                ///< @os_write_file - 写入文件内容
    OsExists,                   ///< @os_exists - 路径是否存在
    OsIsFile,                   ///< @os_is_file - 是否普通文件
    OsIsDir,                    ///< @os_is_dir - 是否目录
    OsCreateDir,                ///< @os_create_dir - 创建目录
    OsCreateDirAll,             ///< @os_create_dir_all - 递归创建目录
    OsRemoveDir,                ///< @os_remove_dir - 删除目录
    OsRemoveFile,               ///< @os_remove_file - 删除文件
    OsReadDirOpen,              ///< @os_read_dir_open - 打开目录迭代器
    OsReadDirNext,              ///< @os_read_dir_next - 读取下一个目录项
    OsReadDirEntryPath,         ///< @os_read_dir_entry_path - 当前项完整路径
    OsReadDirEntryName,         ///< @os_read_dir_entry_name - 当前项文件名
    OsReadDirEntryIsFile,       ///< @os_read_dir_entry_is_file - 当前项是否文件
    OsReadDirEntryIsDir,        ///< @os_read_dir_entry_is_dir - 当前项是否目录
    OsReadDirClose,             ///< @os_read_dir_close - 关闭目录迭代器
    OsStdinReadLine,            ///< @os_stdin_read_line - 从 stdin 读一行
    OsHttpGetStatus,            ///< @os_http_get_status - HTTP GET 状态码
    OsHttpGetBody,              ///< @os_http_get_body - HTTP GET 响应体
    OsHttpPostStatus,           ///< @os_http_post_status - HTTP POST 状态码
    OsHttpPostBody,             ///< @os_http_post_body - HTTP POST 响应体
    FfiOpen,                    ///< @ffi_open - 打开动态库
    FfiOpenSelf,                ///< @ffi_open_self - 打开当前进程符号表
    FfiSym,                     ///< @ffi_sym - 查找符号地址
    FfiClose,                   ///< @ffi_close - 关闭动态库句柄
    FfiLastError,               ///< @ffi_last_error - 获取最近 FFI 错误
    FfiCStrLen,                 ///< @ffi_cstr_len - C 字符串长度（strlen）
    FfiCall0,                   ///< @ffi_call0 - 调用无参 C 函数
    FfiCall1,                   ///< @ffi_call1 - 调用 1 参数 C 函数
    FfiCall2,                   ///< @ffi_call2 - 调用 2 参数 C 函数
    FfiCall3,                   ///< @ffi_call3 - 调用 3 参数 C 函数
    FfiCall4,                   ///< @ffi_call4 - 调用 4 参数 C 函数
    FfiCall5,                   ///< @ffi_call5 - 调用 5 参数 C 函数
    FfiCall6,                   ///< @ffi_call6 - 调用 6 参数 C 函数
};

/// \brief 内置函数调用表达式
///
/// 处理 @import, @sizeof, @typeof, @panic, @assert 等内置函数
class BuiltinCallExpr : public Expr {
public:
    /// \brief 内置函数参数类型
    /// 
    /// 内置函数的参数可以是表达式或类型，使用此联合体表示
    struct Argument {
        enum class Kind {
            Expression,  ///< 表达式参数
            Type        ///< 类型参数
        };
        
        Kind kind;
        union {
            Expr* expr;      ///< 表达式参数
            TypeNode* type;  ///< 类型参数
        };
        Type* ResolvedType = nullptr;  ///< 语义分析后解析出的类型（仅类型参数）
        
        /// \brief 构造表达式参数
        explicit Argument(Expr* e) : kind(Kind::Expression), expr(e), ResolvedType(nullptr) {}
        
        /// \brief 构造类型参数
        explicit Argument(TypeNode* t) : kind(Kind::Type), type(t), ResolvedType(nullptr) {}
        
        /// \brief 是否为表达式参数
        bool isExpr() const { return kind == Kind::Expression; }
        
        /// \brief 是否为类型参数
        bool isType() const { return kind == Kind::Type; }
        
        /// \brief 获取表达式参数（仅当 isExpr() 为 true 时有效）
        Expr* getExpr() const { return expr; }
        
        /// \brief 获取类型参数（仅当 isType() 为 true 时有效）
        TypeNode* getType() const { return type; }

        /// \brief 设置解析后的类型（仅当 isType() 为 true 时有效）
        void setResolvedType(Type* ty) { ResolvedType = ty; }

        /// \brief 获取解析后的类型（仅当 isType() 为 true 时有效）
        Type* getResolvedType() const { return ResolvedType; }
    };
    
    /// \brief 构造内置函数调用表达式
    /// \param range 源码范围
    /// \param kind 内置函数类型
    /// \param args 参数列表
    BuiltinCallExpr(SourceRange range, BuiltinKind kind,
                    std::vector<Argument> args);
    
    /// \brief 获取内置函数类型
    BuiltinKind getBuiltinKind() const { return Kind_; }
    
    /// \brief 获取参数列表
    const std::vector<Argument>& getArgs() const { return Args; }

    /// \brief 获取参数列表（可变）
    std::vector<Argument>& getArgsMutable() { return Args; }
    
    /// \brief 获取参数数量
    size_t getArgCount() const { return Args.size(); }
    
    /// \brief 获取内置函数名称
    static const char* getBuiltinName(BuiltinKind kind);
    
    /// \brief 从标识符解析内置函数类型
    /// \param name 内置函数名称（不含 @ 前缀）
    /// \return 内置函数类型，如果无效返回 std::nullopt
    static std::optional<BuiltinKind> getBuiltinKind(const std::string& name);
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::BuiltinCallExpr;
    }
    
private:
    BuiltinKind Kind_;              ///< 内置函数类型
    std::vector<Argument> Args;     ///< 参数列表
};


// ============================================================================
// 控制流表达式
// ============================================================================

/// \brief If 表达式
///
/// 表示 if 表达式（作为表达式使用时），例如：
/// - if x > 0 { 1 } else { -1 }
class IfExpr : public Expr {
public:
    /// \brief 分支结构
    struct Branch {
        Expr* Condition;    ///< 条件（else 分支为 nullptr）
        Expr* Body;         ///< 分支体
    };
    
    /// \brief 构造 If 表达式
    /// \param range 源码范围
    /// \param branches 分支列表
    IfExpr(SourceRange range, std::vector<Branch> branches);
    
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
    Expr* getThenExpr() const {
        return Branches.empty() ? nullptr : Branches[0].Body;
    }
    
    /// \brief 获取 else 分支体
    Expr* getElseExpr() const {
        if (hasElse()) {
            return Branches.back().Body;
        }
        return nullptr;
    }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::IfExpr;
    }
    
private:
    std::vector<Branch> Branches;  ///< 分支列表
};

/// \brief Match 表达式
///
/// 表示 match 表达式，例如：
/// - match x { 0 => "zero", _ => "other" }
class MatchExpr : public Expr {
public:
    /// \brief 匹配分支
    struct Arm {
        Pattern* Pat;       ///< 模式
        Expr* Guard;        ///< 守卫条件（可为 nullptr）
        Expr* Body;         ///< 分支体
    };
    
    /// \brief 构造 Match 表达式
    /// \param range 源码范围
    /// \param scrutinee 被匹配的表达式
    /// \param arms 匹配分支列表
    MatchExpr(SourceRange range, Expr* scrutinee, std::vector<Arm> arms);
    
    /// \brief 获取被匹配的表达式
    Expr* getScrutinee() const { return Scrutinee; }
    
    /// \brief 获取匹配分支列表
    const std::vector<Arm>& getArms() const { return Arms; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::MatchExpr;
    }
    
private:
    Expr* Scrutinee;        ///< 被匹配的表达式
    std::vector<Arm> Arms;  ///< 匹配分支列表
};


// ============================================================================
// 闭包和复合表达式
// ============================================================================

/// \brief 闭包表达式
///
/// 表示闭包/lambda，例如：
/// - |x| x + 1
/// - |x: i32, y: i32| -> i32 { x + y }
class ClosureExpr : public Expr {
public:
    /// \brief 构造闭包表达式
    /// \param range 源码范围
    /// \param params 参数列表
    /// \param returnType 返回类型（可为 nullptr）
    /// \param body 闭包体（表达式或块）
    ClosureExpr(SourceRange range,
                std::vector<ParamDecl*> params,
                TypeNode* returnType,
                Expr* body,
                std::vector<GenericParam> genericParams = {});
    
    /// \brief 获取参数列表
    const std::vector<ParamDecl*>& getParams() const { return Params; }
    
    /// \brief 获取返回类型
    TypeNode* getReturnType() const { return ReturnType; }
    
    /// \brief 获取闭包体
    Expr* getBody() const { return Body; }

    /// \brief 设置泛型参数
    void setGenericParams(std::vector<GenericParam> params) {
        GenericParams = std::move(params);
    }

    /// \brief 获取泛型参数
    const std::vector<GenericParam>& getGenericParams() const {
        return GenericParams;
    }

    /// \brief 是否为泛型闭包
    bool isGeneric() const { return !GenericParams.empty(); }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::ClosureExpr;
    }
    
private:
    std::vector<ParamDecl*> Params;  ///< 参数列表
    TypeNode* ReturnType;            ///< 返回类型
    Expr* Body;                      ///< 闭包体
    std::vector<GenericParam> GenericParams; ///< 泛型参数
};

/// \brief 数组表达式
///
/// 表示数组字面量，例如：
/// - [1, 2, 3]
/// - [0; 10]  // 重复初始化
class ArrayExpr : public Expr {
public:
    /// \brief 构造数组表达式（元素列表形式）
    /// \param range 源码范围
    /// \param elements 元素列表
    ArrayExpr(SourceRange range, std::vector<Expr*> elements);
    
    /// \brief 构造数组表达式（重复初始化形式）
    /// \param range 源码范围
    /// \param element 重复的元素
    /// \param count 重复次数
    static ArrayExpr* createRepeat(SourceRange range, Expr* element, Expr* count);
    
    /// \brief 获取元素列表
    const std::vector<Expr*>& getElements() const { return Elements; }
    
    /// \brief 是否为重复初始化形式
    bool isRepeat() const { return IsRepeat; }
    
    /// \brief 获取重复次数（仅重复初始化形式有效）
    Expr* getRepeatCount() const { return RepeatCount; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::ArrayExpr;
    }
    
private:
    /// \brief 私有构造函数（重复初始化形式）
    ArrayExpr(SourceRange range, Expr* element, Expr* count, bool isRepeat);
    
    std::vector<Expr*> Elements;  ///< 元素列表
    Expr* RepeatCount;            ///< 重复次数
    bool IsRepeat;                ///< 是否为重复初始化
};


/// \brief 元组表达式
///
/// 表示元组字面量，例如：
/// - (1, "hello", true)
/// - ()  // 空元组
class TupleExpr : public Expr {
public:
    /// \brief 构造元组表达式
    /// \param range 源码范围
    /// \param elements 元素列表
    TupleExpr(SourceRange range, std::vector<Expr*> elements);
    
    /// \brief 获取元素列表
    const std::vector<Expr*>& getElements() const { return Elements; }
    
    /// \brief 是否为空元组
    bool isEmpty() const { return Elements.empty(); }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::TupleExpr;
    }
    
private:
    std::vector<Expr*> Elements;  ///< 元素列表
};

/// \brief 结构体表达式
///
/// 表示结构体字面量，例如：
/// - Point { x: 1.0, y: 2.0 }
/// - Person { name: "Alice", ..default }
class StructExpr : public Expr {
public:
    /// \brief 字段初始化
    struct FieldInit {
        std::string Name;   ///< 字段名
        Expr* Value;        ///< 字段值
        SourceLocation Loc; ///< 字段位置
    };
    
    /// \brief 构造结构体表达式
    /// \param range 源码范围
    /// \param typeName 结构体类型名
    /// \param fields 字段初始化列表
    /// \param base 基础表达式（用于 ..base 语法，可为 nullptr）
    StructExpr(SourceRange range,
               const std::string& typeName,
               std::vector<FieldInit> fields,
               std::vector<TypeNode*> typeArgs = {},
               Expr* base = nullptr);
    
    /// \brief 获取结构体类型名
    const std::string& getTypeName() const { return TypeName; }
    
    /// \brief 获取字段初始化列表
    const std::vector<FieldInit>& getFields() const { return Fields; }

    /// \brief 获取类型实参列表
    const std::vector<TypeNode*>& getTypeArgs() const { return TypeArgs; }

    /// \brief 是否有类型实参
    bool hasTypeArgs() const { return !TypeArgs.empty(); }
    
    /// \brief 获取基础表达式
    Expr* getBase() const { return Base; }
    
    /// \brief 是否有基础表达式
    bool hasBase() const { return Base != nullptr; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::StructExpr;
    }
    
private:
    std::string TypeName;           ///< 结构体类型名
    std::vector<FieldInit> Fields;  ///< 字段初始化列表
    std::vector<TypeNode*> TypeArgs;///< 泛型类型实参
    Expr* Base;                     ///< 基础表达式
};

/// \brief 范围表达式
///
/// 表示范围，例如：
/// - 1..10
/// - 0..=n
/// - ..end
/// - start..
class RangeExpr : public Expr {
public:
    /// \brief 构造范围表达式
    /// \param range 源码范围
    /// \param start 起始值（可为 nullptr）
    /// \param end 结束值（可为 nullptr）
    /// \param isInclusive 是否包含结束值
    RangeExpr(SourceRange range, Expr* start, Expr* end,
              bool isInclusive = false);
    
    /// \brief 获取起始值
    Expr* getStart() const { return Start; }
    
    /// \brief 获取结束值
    Expr* getEnd() const { return End; }
    
    /// \brief 是否包含结束值
    bool isInclusive() const { return IsInclusive; }
    
    /// \brief 是否有起始值
    bool hasStart() const { return Start != nullptr; }
    
    /// \brief 是否有结束值
    bool hasEnd() const { return End != nullptr; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::RangeExpr;
    }
    
private:
    Expr* Start;        ///< 起始值
    Expr* End;          ///< 结束值
    bool IsInclusive;   ///< 是否包含结束值
};


/// \brief await 表达式
///
/// 表示 await 操作，例如：
/// - await fetch_data()
class AwaitExpr : public Expr {
public:
    /// \brief 构造 await 表达式
    /// \param range 源码范围
    /// \param inner 被等待的表达式
    AwaitExpr(SourceRange range, Expr* inner);

    /// \brief 获取被等待的表达式
    Expr* getInner() const { return Inner; }

    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::AwaitExpr;
    }

private:
    Expr* Inner; ///< 被等待的表达式
};


// ============================================================================
// 错误处理表达式
// ============================================================================

/// \brief 错误传播表达式
///
/// 表示 ! 操作符，用于错误传播，例如：
/// - file.read()!
/// - parse(input)!
class ErrorPropagateExpr : public Expr {
public:
    /// \brief 构造错误传播表达式
    /// \param range 源码范围
    /// \param inner 内部表达式
    ErrorPropagateExpr(SourceRange range, Expr* inner);
    
    /// \brief 获取内部表达式
    Expr* getInner() const { return Inner; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::ErrorPropagateExpr;
    }
    
private:
    Expr* Inner;  ///< 内部表达式
};

/// \brief 错误处理表达式
///
/// 表示 -> err {} 语法，用于错误处理，例如：
/// - file.read()! -> err { return default_value }
class ErrorHandleExpr : public Expr {
public:
    /// \brief 构造错误处理表达式
    /// \param range 源码范围
    /// \param inner 内部表达式
    /// \param errorVar 错误变量名
    /// \param handler 错误处理块
    ErrorHandleExpr(SourceRange range,
                    Expr* inner,
                    const std::string& errorVar,
                    BlockStmt* handler);
    
    /// \brief 获取内部表达式
    Expr* getInner() const { return Inner; }
    
    /// \brief 获取错误变量名
    const std::string& getErrorVar() const { return ErrorVar; }
    
    /// \brief 获取错误处理块
    BlockStmt* getHandler() const { return Handler; }

    /// \brief 设置错误变量声明
    void setErrorVarDecl(class VarDecl* decl) { ErrorVarDecl = decl; }

    /// \brief 获取错误变量声明
    class VarDecl* getErrorVarDecl() const { return ErrorVarDecl; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::ErrorHandleExpr;
    }
    
private:
    Expr* Inner;            ///< 内部表达式
    std::string ErrorVar;   ///< 错误变量名
    BlockStmt* Handler;     ///< 错误处理块
    class VarDecl* ErrorVarDecl = nullptr; ///< 错误变量声明
};

/// \brief 类型转换表达式
///
/// 表示 as 类型转换，例如：
/// - x as i64
/// - ptr as *u8
class CastExpr : public Expr {
public:
    /// \brief 构造类型转换表达式
    /// \param range 源码范围
    /// \param expr 被转换的表达式
    /// \param targetType 目标类型
    CastExpr(SourceRange range, Expr* expr, TypeNode* targetType);
    
    /// \brief 获取被转换的表达式
    Expr* getExpr() const { return Expression; }
    
    /// \brief 获取目标类型
    TypeNode* getTargetType() const { return TargetType; }
    
    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::CastExpr;
    }
    
private:
    Expr* Expression;       ///< 被转换的表达式
    TypeNode* TargetType;   ///< 目标类型
};

/// \brief Loop 表达式
///
/// 表示无限循环表达式，可以通过 break 语句返回值。
/// 例如：
/// - var result = loop { if cond { break value } }
/// - loop { process(); if done { break } }
class LoopExpr : public Expr {
public:
    /// \brief 构造 Loop 表达式
    /// \param range 源码范围
    /// \param body 循环体（块表达式）
    LoopExpr(SourceRange range, Expr* body);

    /// \brief 获取循环体
    Expr* getBody() const { return Body; }

    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::LoopExpr;
    }

private:
    Expr* Body;   ///< 循环体
};

/// \brief 块表达式
///
/// 表示一个代码块作为表达式使用，块的最后一个表达式的值作为整个块的值。
/// 例如：
/// - return { var x = 10; x * 2 }
/// - var result = { compute(); value }
class BlockExpr : public Expr {
public:
    /// \brief 构造块表达式
    /// \param range 源码范围
    /// \param stmts 语句列表
    /// \param resultExpr 结果表达式（可选，块的最后一个表达式）
    BlockExpr(SourceRange range, std::vector<Stmt*> stmts, Expr* resultExpr = nullptr);

    /// \brief 获取语句列表
    const std::vector<Stmt*>& getStatements() const { return Stmts; }

    /// \brief 获取结果表达式
    Expr* getResultExpr() const { return ResultExpr; }

    /// \brief 是否有结果表达式
    bool hasResult() const { return ResultExpr != nullptr; }

    /// \brief RTTI 支持
    static bool classof(const ASTNode* node) {
        return node->getKind() == Kind::BlockExpr;
    }

private:
    std::vector<Stmt*> Stmts;   ///< 语句列表
    Expr* ResultExpr;            ///< 结果表达式
};

} // namespace yuan

#endif // YUAN_AST_EXPR_H
