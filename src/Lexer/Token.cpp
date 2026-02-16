#include "yuan/Lexer/Token.h"
#include "yuan/Basic/TokenKinds.h"

namespace yuan {

Token::Token(TokenKind kind, SourceLocation loc, const std::string& text,
             const std::string& docComment)
    : Kind(kind), Loc(loc), Text(text), DocComment(docComment) {
}

SourceRange Token::getRange() const {
    // 计算 Token 的结束位置
    // 假设 SourceLocation 的偏移量是字节偏移
    uint32_t startOffset = Loc.getOffset();
    uint32_t endOffset = startOffset + static_cast<uint32_t>(Text.length());
    SourceLocation endLoc(endOffset);
    return SourceRange(Loc, endLoc);
}

bool Token::isKeyword() const {
    switch (Kind) {
        // 关键字
        case TokenKind::KW_var:
        case TokenKind::KW_const:
        case TokenKind::KW_func:
        case TokenKind::KW_return:
        case TokenKind::KW_struct:
        case TokenKind::KW_enum:
        case TokenKind::KW_trait:
        case TokenKind::KW_impl:
        case TokenKind::KW_pub:
        case TokenKind::KW_priv:
        case TokenKind::KW_internal:
        case TokenKind::KW_if:
        case TokenKind::KW_elif:
        case TokenKind::KW_else:
        case TokenKind::KW_match:
        case TokenKind::KW_while:
        case TokenKind::KW_loop:
        case TokenKind::KW_for:
        case TokenKind::KW_in:
        case TokenKind::KW_break:
        case TokenKind::KW_continue:
        case TokenKind::KW_true:
        case TokenKind::KW_false:
        case TokenKind::KW_async:
        case TokenKind::KW_await:
        case TokenKind::KW_as:
        case TokenKind::KW_self:
        case TokenKind::KW_Self:
        case TokenKind::KW_mut:
        case TokenKind::KW_ref:
        case TokenKind::KW_ptr:
        case TokenKind::KW_void:
        case TokenKind::KW_defer:
        case TokenKind::KW_type:
        case TokenKind::KW_where:
        case TokenKind::KW_None:
        case TokenKind::KW_orelse:
        // 类型关键字
        case TokenKind::KW_i8:
        case TokenKind::KW_i16:
        case TokenKind::KW_i32:
        case TokenKind::KW_i64:
        case TokenKind::KW_i128:
        case TokenKind::KW_isize:
        case TokenKind::KW_u8:
        case TokenKind::KW_u16:
        case TokenKind::KW_u32:
        case TokenKind::KW_u64:
        case TokenKind::KW_u128:
        case TokenKind::KW_usize:
        case TokenKind::KW_f32:
        case TokenKind::KW_f64:
        case TokenKind::KW_bool:
        case TokenKind::KW_char:
        case TokenKind::KW_str:
            return true;
        default:
            return false;
    }
}

bool Token::isLiteral() const {
    switch (Kind) {
        case TokenKind::IntegerLiteral:
        case TokenKind::FloatLiteral:
        case TokenKind::CharLiteral:
        case TokenKind::StringLiteral:
        case TokenKind::RawStringLiteral:
        case TokenKind::MultilineStringLiteral:
        case TokenKind::KW_true:
        case TokenKind::KW_false:
        case TokenKind::KW_None:
            return true;
        default:
            return false;
    }
}

bool Token::isOperator() const {
    switch (Kind) {
        // 算术运算符
        case TokenKind::Plus:
        case TokenKind::Minus:
        case TokenKind::Star:
        case TokenKind::Slash:
        case TokenKind::Percent:
        // 位运算符
        case TokenKind::Amp:
        case TokenKind::Pipe:
        case TokenKind::Caret:
        case TokenKind::Tilde:
        case TokenKind::LessLess:
        case TokenKind::GreaterGreater:
        // 逻辑运算符
        case TokenKind::AmpAmp:
        case TokenKind::PipePipe:
        case TokenKind::Exclaim:
        // 比较运算符
        case TokenKind::EqualEqual:
        case TokenKind::ExclaimEqual:
        case TokenKind::Less:
        case TokenKind::Greater:
        case TokenKind::LessEqual:
        case TokenKind::GreaterEqual:
        // 赋值运算符
        case TokenKind::Equal:
        case TokenKind::PlusEqual:
        case TokenKind::MinusEqual:
        case TokenKind::StarEqual:
        case TokenKind::SlashEqual:
        case TokenKind::PercentEqual:
        case TokenKind::AmpEqual:
        case TokenKind::PipeEqual:
        case TokenKind::CaretEqual:
        case TokenKind::LessLessEqual:
        case TokenKind::GreaterGreaterEqual:
        // 其他运算符
        case TokenKind::Arrow:
        case TokenKind::FatArrow:
        case TokenKind::DotDot:
        case TokenKind::DotDotEqual:
        case TokenKind::Question:
            return true;
        default:
            return false;
    }
}

} // namespace yuan
