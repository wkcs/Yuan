/// \file Expr.cpp
/// \brief 表达式 AST 节点实现。

#include "yuan/AST/Expr.h"
#include <cassert>
#include <unordered_map>

namespace yuan {

// ============================================================================
// 字面量表达式实现
// ============================================================================

IntegerLiteralExpr::IntegerLiteralExpr(SourceRange range, uint64_t value,
                                       bool isSigned, unsigned bitWidth,
                                       bool hasTypeSuffix,
                                       bool isPointerSizedSuffix)
    : Expr(Kind::IntegerLiteralExpr, range),
      Value(value),
      IsSigned(isSigned),
      BitWidth(bitWidth),
      HasTypeSuffix(hasTypeSuffix || bitWidth != 0),
      IsPointerSizedSuffix(isPointerSizedSuffix) {}

FloatLiteralExpr::FloatLiteralExpr(SourceRange range, double value,
                                   unsigned bitWidth)
    : Expr(Kind::FloatLiteralExpr, range),
      Value(value), BitWidth(bitWidth) {}

BoolLiteralExpr::BoolLiteralExpr(SourceRange range, bool value)
    : Expr(Kind::BoolLiteralExpr, range), Value(value) {}

CharLiteralExpr::CharLiteralExpr(SourceRange range, uint32_t codepoint)
    : Expr(Kind::CharLiteralExpr, range), Codepoint(codepoint) {}

StringLiteralExpr::StringLiteralExpr(SourceRange range, const std::string& value,
                                     StringKind kind)
    : Expr(Kind::StringLiteralExpr, range), Value(value), Kind_(kind) {}

NoneLiteralExpr::NoneLiteralExpr(SourceRange range)
    : Expr(Kind::NoneLiteralExpr, range) {}

// ============================================================================
// 标识符和成员访问表达式实现
// ============================================================================

IdentifierExpr::IdentifierExpr(SourceRange range, const std::string& name)
    : Expr(Kind::IdentifierExpr, range), Name(name) {}

MemberExpr::MemberExpr(SourceRange range, Expr* base, const std::string& member)
    : Expr(Kind::MemberExpr, range), Base(base), Member(member) {
    assert(base && "MemberExpr base cannot be null");
}


// ============================================================================
// 运算符表达式实现
// ============================================================================

BinaryExpr::BinaryExpr(SourceRange range, Op op, Expr* lhs, Expr* rhs)
    : Expr(Kind::BinaryExpr, range), Operator(op), LHS(lhs), RHS(rhs) {
    assert(lhs && "BinaryExpr LHS cannot be null");
    assert(rhs && "BinaryExpr RHS cannot be null");
}

const char* BinaryExpr::getOpSpelling(Op op) {
    switch (op) {
        case Op::Add:            return "+";
        case Op::Sub:            return "-";
        case Op::Mul:            return "*";
        case Op::Div:            return "/";
        case Op::Mod:            return "%";
        case Op::BitAnd:         return "&";
        case Op::BitOr:          return "|";
        case Op::BitXor:         return "^";
        case Op::Shl:            return "<<";
        case Op::Shr:            return ">>";
        case Op::And:            return "&&";
        case Op::Or:             return "||";
        case Op::Eq:             return "==";
        case Op::Ne:             return "!=";
        case Op::Lt:             return "<";
        case Op::Le:             return "<=";
        case Op::Gt:             return ">";
        case Op::Ge:             return ">=";
        case Op::Range:          return "..";
        case Op::RangeInclusive: return "..=";
        case Op::OrElse:         return "orelse";
    }
    return "?";
}

const char* BinaryExpr::getOpName(Op op) {
    switch (op) {
        case Op::Add:            return "Add";
        case Op::Sub:            return "Sub";
        case Op::Mul:            return "Mul";
        case Op::Div:            return "Div";
        case Op::Mod:            return "Mod";
        case Op::BitAnd:         return "BitAnd";
        case Op::BitOr:          return "BitOr";
        case Op::BitXor:         return "BitXor";
        case Op::Shl:            return "Shl";
        case Op::Shr:            return "Shr";
        case Op::And:            return "And";
        case Op::Or:             return "Or";
        case Op::Eq:             return "Eq";
        case Op::Ne:             return "Ne";
        case Op::Lt:             return "Lt";
        case Op::Le:             return "Le";
        case Op::Gt:             return "Gt";
        case Op::Ge:             return "Ge";
        case Op::Range:          return "Range";
        case Op::RangeInclusive: return "RangeInclusive";
        case Op::OrElse:         return "OrElse";
    }
    return "Unknown";
}

UnaryExpr::UnaryExpr(SourceRange range, Op op, Expr* operand)
    : Expr(Kind::UnaryExpr, range), Operator(op), Operand(operand) {
    assert(operand && "UnaryExpr operand cannot be null");
}

const char* UnaryExpr::getOpSpelling(Op op) {
    switch (op) {
        case Op::Neg:     return "-";
        case Op::Not:     return "!";
        case Op::BitNot:  return "~";
        case Op::Ref:     return "&";
        case Op::RefMut:  return "&mut";
        case Op::Deref:   return "*";
    }
    return "?";
}

const char* UnaryExpr::getOpName(Op op) {
    switch (op) {
        case Op::Neg:     return "Neg";
        case Op::Not:     return "Not";
        case Op::BitNot:  return "BitNot";
        case Op::Ref:     return "Ref";
        case Op::RefMut:  return "RefMut";
        case Op::Deref:   return "Deref";
    }
    return "Unknown";
}


AssignExpr::AssignExpr(SourceRange range, Op op, Expr* target, Expr* value)
    : Expr(Kind::AssignExpr, range), Operator(op), Target(target), Value(value) {
    assert(target && "AssignExpr target cannot be null");
    assert(value && "AssignExpr value cannot be null");
}

const char* AssignExpr::getOpSpelling(Op op) {
    switch (op) {
        case Op::Assign:       return "=";
        case Op::AddAssign:    return "+=";
        case Op::SubAssign:    return "-=";
        case Op::MulAssign:    return "*=";
        case Op::DivAssign:    return "/=";
        case Op::ModAssign:    return "%=";
        case Op::BitAndAssign: return "&=";
        case Op::BitOrAssign:  return "|=";
        case Op::BitXorAssign: return "^=";
        case Op::ShlAssign:    return "<<=";
        case Op::ShrAssign:    return ">>=";
    }
    return "?";
}

// ============================================================================
// 调用和索引表达式实现
// ============================================================================

CallExpr::CallExpr(SourceRange range, Expr* callee, std::vector<CallExpr::Arg> args,
                   std::vector<TypeNode*> typeArgs)
    : Expr(Kind::CallExpr, range),
      Callee(callee),
      Args(std::move(args)),
      TypeArgs(std::move(typeArgs)) {
    assert(callee && "CallExpr callee cannot be null");
}

CallExpr::CallExpr(SourceRange range, Expr* callee, std::vector<Expr*> args,
                   std::vector<TypeNode*> typeArgs)
    : CallExpr(range, callee, toArgVector(std::move(args)), std::move(typeArgs)) {}

std::vector<CallExpr::Arg> CallExpr::toArgVector(std::vector<Expr*> args) {
    std::vector<Arg> result;
    result.reserve(args.size());
    for (Expr* expr : args) {
        result.emplace_back(expr, false);
    }
    return result;
}

IndexExpr::IndexExpr(SourceRange range, Expr* base, Expr* index)
    : Expr(Kind::IndexExpr, range), Base(base), Index(index) {
    assert(base && "IndexExpr base cannot be null");
    assert(index && "IndexExpr index cannot be null");
}

SliceExpr::SliceExpr(SourceRange range, Expr* base, Expr* start, Expr* end,
                     bool isInclusive)
    : Expr(Kind::SliceExpr, range), Base(base), Start(start), End(end),
      IsInclusive(isInclusive) {
    assert(base && "SliceExpr base cannot be null");
}

// ============================================================================
// 内置函数调用表达式实现
// ============================================================================

BuiltinCallExpr::BuiltinCallExpr(SourceRange range, BuiltinKind kind,
                                 std::vector<Argument> args)
    : Expr(Kind::BuiltinCallExpr, range), Kind_(kind), Args(std::move(args)) {}

const char* BuiltinCallExpr::getBuiltinName(BuiltinKind kind) {
    switch (kind) {
        case BuiltinKind::Import:  return "import";
        case BuiltinKind::Sizeof:  return "sizeof";
        case BuiltinKind::Typeof:  return "typeof";
        case BuiltinKind::PlatformOs: return "platform_os";
        case BuiltinKind::PlatformArch: return "platform_arch";
        case BuiltinKind::PlatformPointerBits: return "platform_pointer_bits";
        case BuiltinKind::Panic:   return "panic";
        case BuiltinKind::Assert:  return "assert";
        case BuiltinKind::Alignof: return "alignof";
        case BuiltinKind::File:    return "file";
        case BuiltinKind::Line:    return "line";
        case BuiltinKind::Column:  return "column";
        case BuiltinKind::Func:    return "func";
        case BuiltinKind::Print:   return "print";
        case BuiltinKind::Format:  return "format";
        case BuiltinKind::Alloc:   return "alloc";
        case BuiltinKind::Realloc: return "realloc";
        case BuiltinKind::Free:    return "free";
        case BuiltinKind::Memcpy:  return "memcpy";
        case BuiltinKind::Memmove: return "memmove";
        case BuiltinKind::Memset:  return "memset";
        case BuiltinKind::StrFromParts: return "str_from_parts";
        case BuiltinKind::Slice:   return "slice";
        case BuiltinKind::AsyncSchedulerCreate: return "async_scheduler_create";
        case BuiltinKind::AsyncSchedulerDestroy: return "async_scheduler_destroy";
        case BuiltinKind::AsyncSchedulerSetCurrent: return "async_scheduler_set_current";
        case BuiltinKind::AsyncSchedulerCurrent: return "async_scheduler_current";
        case BuiltinKind::AsyncSchedulerRunOne: return "async_scheduler_run_one";
        case BuiltinKind::AsyncSchedulerRunUntilIdle: return "async_scheduler_run_until_idle";
        case BuiltinKind::AsyncPromiseCreate: return "async_promise_create";
        case BuiltinKind::AsyncPromiseRetain: return "async_promise_retain";
        case BuiltinKind::AsyncPromiseRelease: return "async_promise_release";
        case BuiltinKind::AsyncPromiseStatus: return "async_promise_status";
        case BuiltinKind::AsyncPromiseValue: return "async_promise_value";
        case BuiltinKind::AsyncPromiseError: return "async_promise_error";
        case BuiltinKind::AsyncPromiseResolve: return "async_promise_resolve";
        case BuiltinKind::AsyncPromiseReject: return "async_promise_reject";
        case BuiltinKind::AsyncPromiseAwait: return "async_promise_await";
        case BuiltinKind::AsyncStep: return "async_step";
        case BuiltinKind::AsyncStepCount: return "async_step_count";
        case BuiltinKind::OsTimeUnixNanos: return "os_time_unix_nanos";
        case BuiltinKind::OsSleepNanos: return "os_sleep_nanos";
        case BuiltinKind::OsYield: return "os_yield";
        case BuiltinKind::OsThreadSpawn: return "os_thread_spawn";
        case BuiltinKind::OsThreadIsFinished: return "os_thread_is_finished";
        case BuiltinKind::OsThreadJoin: return "os_thread_join";
        case BuiltinKind::OsReadFile: return "os_read_file";
        case BuiltinKind::OsWriteFile: return "os_write_file";
        case BuiltinKind::OsExists: return "os_exists";
        case BuiltinKind::OsIsFile: return "os_is_file";
        case BuiltinKind::OsIsDir: return "os_is_dir";
        case BuiltinKind::OsCreateDir: return "os_create_dir";
        case BuiltinKind::OsCreateDirAll: return "os_create_dir_all";
        case BuiltinKind::OsRemoveDir: return "os_remove_dir";
        case BuiltinKind::OsRemoveFile: return "os_remove_file";
        case BuiltinKind::OsReadDirOpen: return "os_read_dir_open";
        case BuiltinKind::OsReadDirNext: return "os_read_dir_next";
        case BuiltinKind::OsReadDirEntryPath: return "os_read_dir_entry_path";
        case BuiltinKind::OsReadDirEntryName: return "os_read_dir_entry_name";
        case BuiltinKind::OsReadDirEntryIsFile: return "os_read_dir_entry_is_file";
        case BuiltinKind::OsReadDirEntryIsDir: return "os_read_dir_entry_is_dir";
        case BuiltinKind::OsReadDirClose: return "os_read_dir_close";
        case BuiltinKind::OsStdinReadLine: return "os_stdin_read_line";
        case BuiltinKind::OsHttpGetStatus: return "os_http_get_status";
        case BuiltinKind::OsHttpGetBody: return "os_http_get_body";
        case BuiltinKind::OsHttpPostStatus: return "os_http_post_status";
        case BuiltinKind::OsHttpPostBody: return "os_http_post_body";
        case BuiltinKind::FfiOpen: return "ffi_open";
        case BuiltinKind::FfiOpenSelf: return "ffi_open_self";
        case BuiltinKind::FfiSym: return "ffi_sym";
        case BuiltinKind::FfiClose: return "ffi_close";
        case BuiltinKind::FfiLastError: return "ffi_last_error";
        case BuiltinKind::FfiCStrLen: return "ffi_cstr_len";
        case BuiltinKind::FfiCall0: return "ffi_call0";
        case BuiltinKind::FfiCall1: return "ffi_call1";
        case BuiltinKind::FfiCall2: return "ffi_call2";
        case BuiltinKind::FfiCall3: return "ffi_call3";
        case BuiltinKind::FfiCall4: return "ffi_call4";
        case BuiltinKind::FfiCall5: return "ffi_call5";
        case BuiltinKind::FfiCall6: return "ffi_call6";
    }
    return "unknown";
}

std::optional<BuiltinKind> BuiltinCallExpr::getBuiltinKind(const std::string& name) {
    static const std::unordered_map<std::string, BuiltinKind> builtinMap = {
        {"import",  BuiltinKind::Import},
        {"sizeof",  BuiltinKind::Sizeof},
        {"typeof",  BuiltinKind::Typeof},
        {"platform_os", BuiltinKind::PlatformOs},
        {"platform_arch", BuiltinKind::PlatformArch},
        {"platform_pointer_bits", BuiltinKind::PlatformPointerBits},
        {"panic",   BuiltinKind::Panic},
        {"assert",  BuiltinKind::Assert},
        {"alignof", BuiltinKind::Alignof},
        {"file",    BuiltinKind::File},
        {"line",    BuiltinKind::Line},
        {"column",  BuiltinKind::Column},
        {"func",    BuiltinKind::Func},
        {"print",   BuiltinKind::Print},
        {"format",  BuiltinKind::Format},
        {"alloc",   BuiltinKind::Alloc},
        {"realloc", BuiltinKind::Realloc},
        {"free",    BuiltinKind::Free},
        {"memcpy",  BuiltinKind::Memcpy},
        {"memmove", BuiltinKind::Memmove},
        {"memset",  BuiltinKind::Memset},
        {"str_from_parts", BuiltinKind::StrFromParts},
        {"slice",   BuiltinKind::Slice},
        {"async_scheduler_create", BuiltinKind::AsyncSchedulerCreate},
        {"async_scheduler_destroy", BuiltinKind::AsyncSchedulerDestroy},
        {"async_scheduler_set_current", BuiltinKind::AsyncSchedulerSetCurrent},
        {"async_scheduler_current", BuiltinKind::AsyncSchedulerCurrent},
        {"async_scheduler_run_one", BuiltinKind::AsyncSchedulerRunOne},
        {"async_scheduler_run_until_idle", BuiltinKind::AsyncSchedulerRunUntilIdle},
        {"async_promise_create", BuiltinKind::AsyncPromiseCreate},
        {"async_promise_retain", BuiltinKind::AsyncPromiseRetain},
        {"async_promise_release", BuiltinKind::AsyncPromiseRelease},
        {"async_promise_status", BuiltinKind::AsyncPromiseStatus},
        {"async_promise_value", BuiltinKind::AsyncPromiseValue},
        {"async_promise_error", BuiltinKind::AsyncPromiseError},
        {"async_promise_resolve", BuiltinKind::AsyncPromiseResolve},
        {"async_promise_reject", BuiltinKind::AsyncPromiseReject},
        {"async_promise_await", BuiltinKind::AsyncPromiseAwait},
        {"async_step", BuiltinKind::AsyncStep},
        {"async_step_count", BuiltinKind::AsyncStepCount},
        {"os_time_unix_nanos", BuiltinKind::OsTimeUnixNanos},
        {"os_sleep_nanos", BuiltinKind::OsSleepNanos},
        {"os_yield", BuiltinKind::OsYield},
        {"os_thread_spawn", BuiltinKind::OsThreadSpawn},
        {"os_thread_is_finished", BuiltinKind::OsThreadIsFinished},
        {"os_thread_join", BuiltinKind::OsThreadJoin},
        {"os_read_file", BuiltinKind::OsReadFile},
        {"os_write_file", BuiltinKind::OsWriteFile},
        {"os_exists", BuiltinKind::OsExists},
        {"os_is_file", BuiltinKind::OsIsFile},
        {"os_is_dir", BuiltinKind::OsIsDir},
        {"os_create_dir", BuiltinKind::OsCreateDir},
        {"os_create_dir_all", BuiltinKind::OsCreateDirAll},
        {"os_remove_dir", BuiltinKind::OsRemoveDir},
        {"os_remove_file", BuiltinKind::OsRemoveFile},
        {"os_read_dir_open", BuiltinKind::OsReadDirOpen},
        {"os_read_dir_next", BuiltinKind::OsReadDirNext},
        {"os_read_dir_entry_path", BuiltinKind::OsReadDirEntryPath},
        {"os_read_dir_entry_name", BuiltinKind::OsReadDirEntryName},
        {"os_read_dir_entry_is_file", BuiltinKind::OsReadDirEntryIsFile},
        {"os_read_dir_entry_is_dir", BuiltinKind::OsReadDirEntryIsDir},
        {"os_read_dir_close", BuiltinKind::OsReadDirClose},
        {"os_stdin_read_line", BuiltinKind::OsStdinReadLine},
        {"os_http_get_status", BuiltinKind::OsHttpGetStatus},
        {"os_http_get_body", BuiltinKind::OsHttpGetBody},
        {"os_http_post_status", BuiltinKind::OsHttpPostStatus},
        {"os_http_post_body", BuiltinKind::OsHttpPostBody},
        {"ffi_open", BuiltinKind::FfiOpen},
        {"ffi_open_self", BuiltinKind::FfiOpenSelf},
        {"ffi_sym", BuiltinKind::FfiSym},
        {"ffi_close", BuiltinKind::FfiClose},
        {"ffi_last_error", BuiltinKind::FfiLastError},
        {"ffi_cstr_len", BuiltinKind::FfiCStrLen},
        {"ffi_call0", BuiltinKind::FfiCall0},
        {"ffi_call1", BuiltinKind::FfiCall1},
        {"ffi_call2", BuiltinKind::FfiCall2},
        {"ffi_call3", BuiltinKind::FfiCall3},
        {"ffi_call4", BuiltinKind::FfiCall4},
        {"ffi_call5", BuiltinKind::FfiCall5},
        {"ffi_call6", BuiltinKind::FfiCall6},
    };

    auto it = builtinMap.find(name);
    if (it != builtinMap.end()) {
        return it->second;
    }
    return std::nullopt;
}


// ============================================================================
// 控制流表达式实现
// ============================================================================

IfExpr::IfExpr(SourceRange range, std::vector<Branch> branches)
    : Expr(Kind::IfExpr, range), Branches(std::move(branches)) {}

MatchExpr::MatchExpr(SourceRange range, Expr* scrutinee, std::vector<Arm> arms)
    : Expr(Kind::MatchExpr, range), Scrutinee(scrutinee), Arms(std::move(arms)) {
    assert(scrutinee && "MatchExpr scrutinee cannot be null");
}

// ============================================================================
// 闭包和复合表达式实现
// ============================================================================

ClosureExpr::ClosureExpr(SourceRange range,
                         std::vector<ParamDecl*> params,
                         TypeNode* returnType,
                         Expr* body,
                         std::vector<GenericParam> genericParams)
    : Expr(Kind::ClosureExpr, range),
      Params(std::move(params)), ReturnType(returnType), Body(body),
      GenericParams(std::move(genericParams)) {
    assert(body && "ClosureExpr body cannot be null");
}

ArrayExpr::ArrayExpr(SourceRange range, std::vector<Expr*> elements)
    : Expr(Kind::ArrayExpr, range),
      Elements(std::move(elements)), RepeatCount(nullptr), IsRepeat(false) {}

ArrayExpr::ArrayExpr(SourceRange range, Expr* element, Expr* count, bool isRepeat)
    : Expr(Kind::ArrayExpr, range),
      Elements({element}), RepeatCount(count), IsRepeat(isRepeat) {
    assert(element && "ArrayExpr repeat element cannot be null");
    assert(count && "ArrayExpr repeat count cannot be null");
}

ArrayExpr* ArrayExpr::createRepeat(SourceRange range, Expr* element, Expr* count) {
    return new ArrayExpr(range, element, count, true);
}

TupleExpr::TupleExpr(SourceRange range, std::vector<Expr*> elements)
    : Expr(Kind::TupleExpr, range), Elements(std::move(elements)) {}

StructExpr::StructExpr(SourceRange range,
                       const std::string& typeName,
                       std::vector<FieldInit> fields,
                       std::vector<TypeNode*> typeArgs,
                       Expr* base)
    : Expr(Kind::StructExpr, range),
      TypeName(typeName), Fields(std::move(fields)),
      TypeArgs(std::move(typeArgs)), Base(base) {}

RangeExpr::RangeExpr(SourceRange range, Expr* start, Expr* end, bool isInclusive)
    : Expr(Kind::RangeExpr, range),
      Start(start), End(end), IsInclusive(isInclusive) {}

AwaitExpr::AwaitExpr(SourceRange range, Expr* inner)
    : Expr(Kind::AwaitExpr, range), Inner(inner) {
    assert(inner && "AwaitExpr inner cannot be null");
}

// ============================================================================
// 错误处理表达式实现
// ============================================================================

ErrorPropagateExpr::ErrorPropagateExpr(SourceRange range, Expr* inner)
    : Expr(Kind::ErrorPropagateExpr, range), Inner(inner) {
    assert(inner && "ErrorPropagateExpr inner cannot be null");
}

ErrorHandleExpr::ErrorHandleExpr(SourceRange range,
                                 Expr* inner,
                                 const std::string& errorVar,
                                 BlockStmt* handler)
    : Expr(Kind::ErrorHandleExpr, range),
      Inner(inner), ErrorVar(errorVar), Handler(handler) {
    assert(inner && "ErrorHandleExpr inner cannot be null");
    assert(handler && "ErrorHandleExpr handler cannot be null");
}

CastExpr::CastExpr(SourceRange range, Expr* expr, TypeNode* targetType)
    : Expr(Kind::CastExpr, range), Expression(expr), TargetType(targetType) {
    assert(expr && "CastExpr expression cannot be null");
    assert(targetType && "CastExpr target type cannot be null");
}

LoopExpr::LoopExpr(SourceRange range, Expr* body)
    : Expr(Kind::LoopExpr, range), Body(body) {
    assert(body && "LoopExpr body cannot be null");
}

BlockExpr::BlockExpr(SourceRange range, std::vector<Stmt*> stmts, Expr* resultExpr)
    : Expr(Kind::BlockExpr, range), Stmts(std::move(stmts)), ResultExpr(resultExpr) {
}

} // namespace yuan
