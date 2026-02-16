/// \file TypeCodec.h
/// \brief Stable type encoding/decoding helpers for module interfaces.

#ifndef YUAN_SEMA_TYPE_CODEC_H
#define YUAN_SEMA_TYPE_CODEC_H

#include <string>

namespace yuan {

class ASTContext;
class Type;

namespace typecodec {

/// Encode semantic type into a compact stable string.
std::string encode(Type* type);

/// Decode semantic type from a previously encoded string.
/// Returns nullptr on parse error.
Type* decode(const std::string& encoded, ASTContext& ctx);

} // namespace typecodec

} // namespace yuan

#endif // YUAN_SEMA_TYPE_CODEC_H
