#include "yuan/Basic/TokenKinds.h"

namespace yuan {

const char* getTokenName(TokenKind kind) {
    switch (kind) {
        case TokenKind::EndOfFile: return "EndOfFile";
        case TokenKind::Invalid: return "Invalid";
        
        // 标识符和字面量
        case TokenKind::Identifier: return "Identifier";
        case TokenKind::BuiltinIdentifier: return "BuiltinIdentifier";
        case TokenKind::IntegerLiteral: return "IntegerLiteral";
        case TokenKind::FloatLiteral: return "FloatLiteral";
        case TokenKind::CharLiteral: return "CharLiteral";
        case TokenKind::StringLiteral: return "StringLiteral";
        case TokenKind::RawStringLiteral: return "RawStringLiteral";
        case TokenKind::MultilineStringLiteral: return "MultilineStringLiteral";
        
        // 关键字
        case TokenKind::KW_var: return "var";
        case TokenKind::KW_const: return "const";
        case TokenKind::KW_func: return "func";
        case TokenKind::KW_return: return "return";
        case TokenKind::KW_struct: return "struct";
        case TokenKind::KW_enum: return "enum";
        case TokenKind::KW_trait: return "trait";
        case TokenKind::KW_impl: return "impl";
        case TokenKind::KW_pub: return "pub";
        case TokenKind::KW_priv: return "priv";
        case TokenKind::KW_internal: return "internal";
        case TokenKind::KW_if: return "if";
        case TokenKind::KW_elif: return "elif";
        case TokenKind::KW_else: return "else";
        case TokenKind::KW_match: return "match";
        case TokenKind::KW_while: return "while";
        case TokenKind::KW_loop: return "loop";
        case TokenKind::KW_for: return "for";
        case TokenKind::KW_in: return "in";
        case TokenKind::KW_break: return "break";
        case TokenKind::KW_continue: return "continue";
        case TokenKind::KW_true: return "true";
        case TokenKind::KW_false: return "false";
        case TokenKind::KW_async: return "async";
        case TokenKind::KW_await: return "await";
        case TokenKind::KW_as: return "as";
        case TokenKind::KW_self: return "self";
        case TokenKind::KW_Self: return "Self";
        case TokenKind::KW_mut: return "mut";
        case TokenKind::KW_ref: return "ref";
        case TokenKind::KW_ptr: return "ptr";
        case TokenKind::KW_void: return "void";
        case TokenKind::KW_defer: return "defer";
        case TokenKind::KW_type: return "type";
        case TokenKind::KW_where: return "where";
        case TokenKind::KW_None: return "None";
        case TokenKind::KW_orelse: return "orelse";
        
        // 类型关键字
        case TokenKind::KW_i8: return "i8";
        case TokenKind::KW_i16: return "i16";
        case TokenKind::KW_i32: return "i32";
        case TokenKind::KW_i64: return "i64";
        case TokenKind::KW_i128: return "i128";
        case TokenKind::KW_isize: return "isize";
        case TokenKind::KW_u8: return "u8";
        case TokenKind::KW_u16: return "u16";
        case TokenKind::KW_u32: return "u32";
        case TokenKind::KW_u64: return "u64";
        case TokenKind::KW_u128: return "u128";
        case TokenKind::KW_usize: return "usize";
        case TokenKind::KW_f32: return "f32";
        case TokenKind::KW_f64: return "f64";
        case TokenKind::KW_bool: return "bool";
        case TokenKind::KW_char: return "char";
        case TokenKind::KW_str: return "str";
        
        // 运算符
        case TokenKind::Plus: return "+";
        case TokenKind::Minus: return "-";
        case TokenKind::Star: return "*";
        case TokenKind::Slash: return "/";
        case TokenKind::Percent: return "%";
        case TokenKind::Amp: return "&";
        case TokenKind::Pipe: return "|";
        case TokenKind::Caret: return "^";
        case TokenKind::Tilde: return "~";
        case TokenKind::LessLess: return "<<";
        case TokenKind::GreaterGreater: return ">>";
        case TokenKind::AmpAmp: return "&&";
        case TokenKind::PipePipe: return "||";
        case TokenKind::Exclaim: return "!";
        case TokenKind::Equal: return "=";
        case TokenKind::EqualEqual: return "==";
        case TokenKind::ExclaimEqual: return "!=";
        case TokenKind::Less: return "<";
        case TokenKind::Greater: return ">";
        case TokenKind::LessEqual: return "<=";
        case TokenKind::GreaterEqual: return ">=";
        case TokenKind::PlusEqual: return "+=";
        case TokenKind::MinusEqual: return "-=";
        case TokenKind::StarEqual: return "*=";
        case TokenKind::SlashEqual: return "/=";
        case TokenKind::PercentEqual: return "%=";
        case TokenKind::AmpEqual: return "&=";
        case TokenKind::PipeEqual: return "|=";
        case TokenKind::CaretEqual: return "^=";
        case TokenKind::LessLessEqual: return "<<=";
        case TokenKind::GreaterGreaterEqual: return ">>=";
        case TokenKind::Arrow: return "->";
        case TokenKind::FatArrow: return "=>";
        case TokenKind::DotDot: return "..";
        case TokenKind::DotDotEqual: return "..=";
        case TokenKind::Ellipsis: return "...";
        case TokenKind::Question: return "?";
        case TokenKind::QuestionDot: return "?.";
        
        // 标点符号
        case TokenKind::LParen: return "(";
        case TokenKind::RParen: return ")";
        case TokenKind::LBracket: return "[";
        case TokenKind::RBracket: return "]";
        case TokenKind::LBrace: return "{";
        case TokenKind::RBrace: return "}";
        case TokenKind::Comma: return ",";
        case TokenKind::Colon: return ":";
        case TokenKind::ColonColon: return "::";
        case TokenKind::Semicolon: return ";";
        case TokenKind::Dot: return ".";
        case TokenKind::At: return "@";
        case TokenKind::Underscore: return "_";
        
        case TokenKind::NUM_TOKENS: return "NUM_TOKENS";
    }
    return "Unknown";
}

const char* getSpelling(TokenKind kind) {
    switch (kind) {
        // 对于关键字和运算符，拼写就是它们的字符串表示
        case TokenKind::KW_var: return "var";
        case TokenKind::KW_const: return "const";
        case TokenKind::KW_func: return "func";
        case TokenKind::KW_return: return "return";
        case TokenKind::KW_struct: return "struct";
        case TokenKind::KW_enum: return "enum";
        case TokenKind::KW_trait: return "trait";
        case TokenKind::KW_impl: return "impl";
        case TokenKind::KW_pub: return "pub";
        case TokenKind::KW_priv: return "priv";
        case TokenKind::KW_internal: return "internal";
        case TokenKind::KW_if: return "if";
        case TokenKind::KW_elif: return "elif";
        case TokenKind::KW_else: return "else";
        case TokenKind::KW_match: return "match";
        case TokenKind::KW_while: return "while";
        case TokenKind::KW_loop: return "loop";
        case TokenKind::KW_for: return "for";
        case TokenKind::KW_in: return "in";
        case TokenKind::KW_break: return "break";
        case TokenKind::KW_continue: return "continue";
        case TokenKind::KW_true: return "true";
        case TokenKind::KW_false: return "false";
        case TokenKind::KW_async: return "async";
        case TokenKind::KW_await: return "await";
        case TokenKind::KW_as: return "as";
        case TokenKind::KW_self: return "self";
        case TokenKind::KW_Self: return "Self";
        case TokenKind::KW_mut: return "mut";
        case TokenKind::KW_ref: return "ref";
        case TokenKind::KW_ptr: return "ptr";
        case TokenKind::KW_void: return "void";
        case TokenKind::KW_defer: return "defer";
        case TokenKind::KW_type: return "type";
        case TokenKind::KW_where: return "where";
        case TokenKind::KW_None: return "None";
        case TokenKind::KW_orelse: return "orelse";
        
        // 类型关键字
        case TokenKind::KW_i8: return "i8";
        case TokenKind::KW_i16: return "i16";
        case TokenKind::KW_i32: return "i32";
        case TokenKind::KW_i64: return "i64";
        case TokenKind::KW_i128: return "i128";
        case TokenKind::KW_isize: return "isize";
        case TokenKind::KW_u8: return "u8";
        case TokenKind::KW_u16: return "u16";
        case TokenKind::KW_u32: return "u32";
        case TokenKind::KW_u64: return "u64";
        case TokenKind::KW_u128: return "u128";
        case TokenKind::KW_usize: return "usize";
        case TokenKind::KW_f32: return "f32";
        case TokenKind::KW_f64: return "f64";
        case TokenKind::KW_bool: return "bool";
        case TokenKind::KW_char: return "char";
        case TokenKind::KW_str: return "str";
        
        // 运算符
        case TokenKind::Plus: return "+";
        case TokenKind::Minus: return "-";
        case TokenKind::Star: return "*";
        case TokenKind::Slash: return "/";
        case TokenKind::Percent: return "%";
        case TokenKind::Amp: return "&";
        case TokenKind::Pipe: return "|";
        case TokenKind::Caret: return "^";
        case TokenKind::Tilde: return "~";
        case TokenKind::LessLess: return "<<";
        case TokenKind::GreaterGreater: return ">>";
        case TokenKind::AmpAmp: return "&&";
        case TokenKind::PipePipe: return "||";
        case TokenKind::Exclaim: return "!";
        case TokenKind::Equal: return "=";
        case TokenKind::EqualEqual: return "==";
        case TokenKind::ExclaimEqual: return "!=";
        case TokenKind::Less: return "<";
        case TokenKind::Greater: return ">";
        case TokenKind::LessEqual: return "<=";
        case TokenKind::GreaterEqual: return ">=";
        case TokenKind::PlusEqual: return "+=";
        case TokenKind::MinusEqual: return "-=";
        case TokenKind::StarEqual: return "*=";
        case TokenKind::SlashEqual: return "/=";
        case TokenKind::PercentEqual: return "%=";
        case TokenKind::AmpEqual: return "&=";
        case TokenKind::PipeEqual: return "|=";
        case TokenKind::CaretEqual: return "^=";
        case TokenKind::LessLessEqual: return "<<=";
        case TokenKind::GreaterGreaterEqual: return ">>=";
        case TokenKind::Arrow: return "->";
        case TokenKind::FatArrow: return "=>";
        case TokenKind::DotDot: return "..";
        case TokenKind::DotDotEqual: return "..=";
        case TokenKind::Ellipsis: return "...";
        case TokenKind::Question: return "?";
        case TokenKind::QuestionDot: return "?.";
        
        // 标点符号
        case TokenKind::LParen: return "(";
        case TokenKind::RParen: return ")";
        case TokenKind::LBracket: return "[";
        case TokenKind::RBracket: return "]";
        case TokenKind::LBrace: return "{";
        case TokenKind::RBrace: return "}";
        case TokenKind::Comma: return ",";
        case TokenKind::Colon: return ":";
        case TokenKind::ColonColon: return "::";
        case TokenKind::Semicolon: return ";";
        case TokenKind::Dot: return ".";
        case TokenKind::At: return "@";
        case TokenKind::Underscore: return "_";
        
        // 对于其他类型，返回空字符串或描述性文本
        default:
            return "";
    }
}

} // namespace yuan
