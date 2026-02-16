#ifndef YUAN_BASIC_TOKENKINDS_H
#define YUAN_BASIC_TOKENKINDS_H

#include <cstdint>

namespace yuan {

/// Token 类型枚举
enum class TokenKind : uint16_t {
    // 特殊 Token
    EndOfFile,
    Invalid,
    
    // 标识符和字面量
    Identifier,         // 普通标识符
    BuiltinIdentifier,  // @开头的内置函数
    IntegerLiteral,
    FloatLiteral,
    CharLiteral,
    StringLiteral,
    RawStringLiteral,
    MultilineStringLiteral,
    
    // 关键字
    KW_var,
    KW_const,
    KW_func,
    KW_return,
    KW_struct,
    KW_enum,
    KW_trait,
    KW_impl,
    KW_pub,
    KW_priv,
    KW_internal,
    KW_if,
    KW_elif,
    KW_else,
    KW_match,
    KW_while,
    KW_loop,
    KW_for,
    KW_in,
    KW_break,
    KW_continue,
    KW_true,
    KW_false,
    KW_async,
    KW_await,
    KW_as,
    KW_self,
    KW_Self,
    KW_mut,
    KW_ref,
    KW_ptr,
    KW_void,
    KW_defer,
    KW_type,
    KW_where,
    KW_None,
    KW_orelse,
    
    // 类型关键字
    KW_i8, KW_i16, KW_i32, KW_i64, KW_i128, KW_isize,
    KW_u8, KW_u16, KW_u32, KW_u64, KW_u128, KW_usize,
    KW_f32, KW_f64,
    KW_bool, KW_char, KW_str,
    
    // 运算符
    Plus,           // +
    Minus,          // -
    Star,           // *
    Slash,          // /
    Percent,        // %
    Amp,            // &
    Pipe,           // |
    Caret,          // ^
    Tilde,          // ~
    LessLess,       // <<
    GreaterGreater, // >>
    AmpAmp,         // &&
    PipePipe,       // ||
    Exclaim,        // !
    Equal,          // =
    EqualEqual,     // ==
    ExclaimEqual,   // !=
    Less,           // <
    Greater,        // >
    LessEqual,      // <=
    GreaterEqual,   // >=
    PlusEqual,      // +=
    MinusEqual,     // -=
    StarEqual,      // *=
    SlashEqual,     // /=
    PercentEqual,   // %=
    AmpEqual,       // &=
    PipeEqual,      // |=
    CaretEqual,     // ^=
    LessLessEqual,  // <<=
    GreaterGreaterEqual, // >>=
    Arrow,          // ->
    FatArrow,       // =>
    DotDot,         // ..
    DotDotEqual,    // ..=
    Ellipsis,       // ...
    Question,       // ?
    QuestionDot,    // ?.
    
    // 标点符号
    LParen,         // (
    RParen,         // )
    LBracket,       // [
    RBracket,       // ]
    LBrace,         // {
    RBrace,         // }
    Comma,          // ,
    Colon,          // :
    ColonColon,     // ::
    Semicolon,      // ;
    Dot,            // .
    At,             // @
    Underscore,     // _
    
    NUM_TOKENS
};

/// 获取 Token 类型的名称（用于调试）
const char* getTokenName(TokenKind kind);

/// 获取 Token 类型的拼写（源码中的字符串表示）
const char* getSpelling(TokenKind kind);

} // namespace yuan

#endif // YUAN_BASIC_TOKENKINDS_H
