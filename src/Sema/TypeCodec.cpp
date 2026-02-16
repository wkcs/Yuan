/// \file TypeCodec.cpp
/// \brief Stable type encoding/decoding helpers for module interfaces.

#include "yuan/Sema/TypeCodec.h"
#include "yuan/AST/ASTContext.h"
#include "yuan/Sema/Type.h"
#include <cctype>
#include <cstdint>

namespace yuan {
namespace typecodec {
namespace {

constexpr char kHexDigits[] = "0123456789abcdef";

static void appendHexByte(std::string& out, unsigned char value) {
    out.push_back(kHexDigits[(value >> 4) & 0x0F]);
    out.push_back(kHexDigits[value & 0x0F]);
}

static std::string hexEncode(const std::string& text) {
    std::string out;
    out.reserve(text.size() * 2);
    for (unsigned char ch : text) {
        appendHexByte(out, ch);
    }
    return out;
}

static int fromHex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static std::string encodeIdentifier(const std::string& text) {
    return "I" + std::to_string(text.size()) + "_" + hexEncode(text);
}

static std::string encodeTypeImpl(Type* type) {
    if (!type) {
        return "Tn";
    }

    switch (type->getKind()) {
        case Type::Kind::Void:
            return "Tv";
        case Type::Kind::Bool:
            return "Tb";
        case Type::Kind::Char:
            return "Tc";
        case Type::Kind::String:
            return "Tstr";
        case Type::Kind::Value:
            return "Tval";
        case Type::Kind::Integer: {
            auto* intTy = static_cast<IntegerType*>(type);
            return (intTy->isSigned() ? "Ti" : "Tu") + std::to_string(intTy->getBitWidth());
        }
        case Type::Kind::Float: {
            auto* floatTy = static_cast<FloatType*>(type);
            return "Tf" + std::to_string(floatTy->getBitWidth());
        }
        case Type::Kind::Array: {
            auto* arrTy = static_cast<ArrayType*>(type);
            return "Ta" + std::to_string(arrTy->getArraySize()) + "_" +
                   encodeTypeImpl(arrTy->getElementType()) + "_E";
        }
        case Type::Kind::Slice: {
            auto* sliceTy = static_cast<SliceType*>(type);
            return "Ts" + std::string(sliceTy->isMutable() ? "m" : "i") + "_" +
                   encodeTypeImpl(sliceTy->getElementType()) + "_E";
        }
        case Type::Kind::Tuple: {
            auto* tupleTy = static_cast<TupleType*>(type);
            std::string out = "Tt" + std::to_string(tupleTy->getElementCount());
            for (size_t i = 0; i < tupleTy->getElementCount(); ++i) {
                out += "_";
                out += encodeTypeImpl(tupleTy->getElement(i));
            }
            out += "_E";
            return out;
        }
        case Type::Kind::VarArgs: {
            auto* varTy = static_cast<VarArgsType*>(type);
            return "Tvargs_" + encodeTypeImpl(varTy->getElementType()) + "_E";
        }
        case Type::Kind::Optional: {
            auto* optTy = static_cast<OptionalType*>(type);
            return "To_" + encodeTypeImpl(optTy->getInnerType()) + "_E";
        }
        case Type::Kind::Reference: {
            auto* refTy = static_cast<ReferenceType*>(type);
            return "Tr" + std::string(refTy->isMutable() ? "m" : "i") + "_" +
                   encodeTypeImpl(refTy->getPointeeType()) + "_E";
        }
        case Type::Kind::Pointer: {
            auto* ptrTy = static_cast<PointerType*>(type);
            return "Tp" + std::string(ptrTy->isMutable() ? "m" : "i") + "_" +
                   encodeTypeImpl(ptrTy->getPointeeType()) + "_E";
        }
        case Type::Kind::Function: {
            auto* fnTy = static_cast<FunctionType*>(type);
            std::string out = "Tfn" + std::to_string(fnTy->getParamCount());
            for (Type* paramTy : fnTy->getParamTypes()) {
                out += "_";
                out += encodeTypeImpl(paramTy);
            }
            out += "_R_";
            out += encodeTypeImpl(fnTy->getReturnType());
            out += "_Er";
            out += fnTy->canError() ? "1" : "0";
            out += "_Vr";
            out += fnTy->isVariadic() ? "1" : "0";
            out += "_E";
            return out;
        }
        case Type::Kind::Struct: {
            auto* structTy = static_cast<StructType*>(type);
            return "Tst_" + encodeIdentifier(structTy->getName());
        }
        case Type::Kind::Enum: {
            auto* enumTy = static_cast<EnumType*>(type);
            return "Ten_" + encodeIdentifier(enumTy->getName());
        }
        case Type::Kind::Trait: {
            auto* traitTy = static_cast<TraitType*>(type);
            return "Ttr_" + encodeIdentifier(traitTy->getName());
        }
        case Type::Kind::Generic: {
            auto* genericTy = static_cast<GenericType*>(type);
            return "Tg_" + encodeIdentifier(genericTy->getName());
        }
        case Type::Kind::GenericInstance: {
            auto* instTy = static_cast<GenericInstanceType*>(type);
            std::string out = "Tgi_";
            out += encodeTypeImpl(instTy->getBaseType());
            out += "_N";
            out += std::to_string(instTy->getTypeArgCount());
            for (Type* argTy : instTy->getTypeArgs()) {
                out += "_";
                out += encodeTypeImpl(argTy);
            }
            out += "_E";
            return out;
        }
        case Type::Kind::Error: {
            auto* errTy = static_cast<ErrorType*>(type);
            return "Terr_" + encodeTypeImpl(errTy->getSuccessType()) + "_E";
        }
        case Type::Kind::TypeVar: {
            auto* tvTy = static_cast<TypeVariable*>(type);
            if (tvTy->isResolved() && tvTy->getResolvedType()) {
                return "Ttv" + std::to_string(tvTy->getID()) + "_" +
                       encodeTypeImpl(tvTy->getResolvedType()) + "_E";
            }
            return "Ttv" + std::to_string(tvTy->getID());
        }
        case Type::Kind::TypeAlias: {
            auto* aliasTy = static_cast<TypeAlias*>(type);
            std::string out = "Tal_";
            out += encodeIdentifier(aliasTy->getName());
            out += "_";
            out += encodeTypeImpl(aliasTy->getAliasedType());
            out += "_E";
            return out;
        }
        case Type::Kind::Module: {
            auto* moduleTy = static_cast<ModuleType*>(type);
            return "Tmo_" + encodeIdentifier(moduleTy->getName());
        }
        case Type::Kind::Range: {
            auto* rangeTy = static_cast<RangeType*>(type);
            return "Tra" + std::string(rangeTy->isInclusive() ? "1" : "0") + "_" +
                   encodeTypeImpl(rangeTy->getElementType()) + "_E";
        }
    }
    return "Tn";
}

class Decoder {
public:
    Decoder(const std::string& text, ASTContext& ctx) : S(text), Ctx(ctx) {}

    Type* parseTop() {
        Type* out = parseType();
        if (!out || Pos != S.size()) {
            return nullptr;
        }
        return out;
    }

private:
    bool consume(const char* token) {
        size_t n = 0;
        while (token[n] != '\0') {
            ++n;
        }
        if (Pos + n > S.size()) {
            return false;
        }
        if (S.compare(Pos, n, token) != 0) {
            return false;
        }
        Pos += n;
        return true;
    }

    bool consumeChar(char c) {
        if (Pos >= S.size() || S[Pos] != c) {
            return false;
        }
        ++Pos;
        return true;
    }

    bool parseUInt(uint64_t& out) {
        if (Pos >= S.size() || !std::isdigit(static_cast<unsigned char>(S[Pos]))) {
            return false;
        }
        uint64_t value = 0;
        while (Pos < S.size() && std::isdigit(static_cast<unsigned char>(S[Pos]))) {
            value = value * 10 + static_cast<uint64_t>(S[Pos] - '0');
            ++Pos;
        }
        out = value;
        return true;
    }

    bool parseIdentifier(std::string& out) {
        if (!consume("I")) {
            return false;
        }

        uint64_t len = 0;
        if (!parseUInt(len) || !consumeChar('_')) {
            return false;
        }

        if (len > (S.size() - Pos) / 2) {
            return false;
        }

        out.clear();
        out.reserve(static_cast<size_t>(len));
        for (uint64_t i = 0; i < len; ++i) {
            if (Pos + 1 >= S.size()) {
                return false;
            }
            int hi = fromHex(S[Pos]);
            int lo = fromHex(S[Pos + 1]);
            if (hi < 0 || lo < 0) {
                return false;
            }
            out.push_back(static_cast<char>((hi << 4) | lo));
            Pos += 2;
        }
        return true;
    }

    bool parseBoolDigit(bool& out) {
        if (Pos >= S.size()) {
            return false;
        }
        if (S[Pos] == '0') {
            out = false;
            ++Pos;
            return true;
        }
        if (S[Pos] == '1') {
            out = true;
            ++Pos;
            return true;
        }
        return false;
    }

    Type* parseType() {
        if (!consume("T")) {
            return nullptr;
        }

        if (consume("n")) return nullptr;
        if (consume("vargs_")) {
            Type* elem = parseType();
            if (!elem || !consume("_E")) return nullptr;
            return Ctx.getVarArgsType(elem);
        }
        if (consume("val")) return Ctx.getValueType();
        if (consume("v")) return Ctx.getVoidType();
        if (consume("b")) return Ctx.getBoolType();
        if (consume("c")) return Ctx.getCharType();
        if (consume("str")) return Ctx.getStrType();

        if (consume("i")) {
            uint64_t bits = 0;
            if (!parseUInt(bits)) return nullptr;
            return Ctx.getIntegerType(static_cast<unsigned>(bits), true);
        }
        if (consume("u")) {
            uint64_t bits = 0;
            if (!parseUInt(bits)) return nullptr;
            return Ctx.getIntegerType(static_cast<unsigned>(bits), false);
        }
        if (consume("fn")) {
            uint64_t count = 0;
            if (!parseUInt(count)) return nullptr;
            std::vector<Type*> params;
            params.reserve(static_cast<size_t>(count));
            for (uint64_t i = 0; i < count; ++i) {
                if (!consumeChar('_')) return nullptr;
                Type* param = parseType();
                if (!param) return nullptr;
                params.push_back(param);
            }
            if (!consume("_R_")) return nullptr;
            Type* ret = parseType();
            bool canError = false;
            bool isVariadic = false;
            if (!ret || !consume("_Er") || !parseBoolDigit(canError) ||
                !consume("_Vr") || !parseBoolDigit(isVariadic) || !consume("_E")) {
                return nullptr;
            }
            return Ctx.getFunctionType(std::move(params), ret, canError, isVariadic);
        }
        if (consume("f")) {
            uint64_t bits = 0;
            if (!parseUInt(bits)) return nullptr;
            return Ctx.getFloatType(static_cast<unsigned>(bits));
        }
        if (consume("a")) {
            uint64_t size = 0;
            if (!parseUInt(size) || !consumeChar('_')) return nullptr;
            Type* elem = parseType();
            if (!elem || !consume("_E")) return nullptr;
            return Ctx.getArrayType(elem, size);
        }
        if (consume("st_")) {
            std::string name;
            if (!parseIdentifier(name)) return nullptr;
            return Ctx.getStructType(name, {}, {});
        }
        if (consume("en_")) {
            std::string name;
            if (!parseIdentifier(name)) return nullptr;
            return Ctx.getEnumType(name, {}, {});
        }
        if (consume("tr_")) {
            std::string name;
            if (!parseIdentifier(name)) return nullptr;
            return Ctx.getTraitType(name);
        }
        if (consume("tv")) {
            uint64_t id = 0;
            if (!parseUInt(id)) return nullptr;
            TypeVariable* typeVar = Ctx.getTypeVariable(static_cast<size_t>(id));
            if (consumeChar('_')) {
                Type* resolved = parseType();
                if (!resolved || !consume("_E")) return nullptr;
                typeVar->setResolvedType(resolved);
            }
            return typeVar;
        }
        if (consume("s")) {
            bool isMut = false;
            if (consume("m_")) {
                isMut = true;
            } else if (consume("i_")) {
                isMut = false;
            } else {
                return nullptr;
            }
            Type* elem = parseType();
            if (!elem || !consume("_E")) return nullptr;
            return Ctx.getSliceType(elem, isMut);
        }
        if (consume("t")) {
            uint64_t count = 0;
            if (!parseUInt(count)) return nullptr;
            std::vector<Type*> elems;
            elems.reserve(static_cast<size_t>(count));
            for (uint64_t i = 0; i < count; ++i) {
                if (!consumeChar('_')) return nullptr;
                Type* elem = parseType();
                if (!elem) return nullptr;
                elems.push_back(elem);
            }
            if (!consume("_E")) return nullptr;
            return Ctx.getTupleType(std::move(elems));
        }
        if (consume("o_")) {
            Type* inner = parseType();
            if (!inner || !consume("_E")) return nullptr;
            return Ctx.getOptionalType(inner);
        }
        if (consume("ra")) {
            bool inclusive = false;
            if (!parseBoolDigit(inclusive) || !consumeChar('_')) return nullptr;
            Type* elem = parseType();
            if (!elem || !consume("_E")) return nullptr;
            return Ctx.getRangeType(elem, inclusive);
        }
        if (consume("r")) {
            bool isMut = false;
            if (consume("m_")) {
                isMut = true;
            } else if (consume("i_")) {
                isMut = false;
            } else {
                return nullptr;
            }
            Type* pointee = parseType();
            if (!pointee || !consume("_E")) return nullptr;
            return Ctx.getReferenceType(pointee, isMut);
        }
        if (consume("p")) {
            bool isMut = false;
            if (consume("m_")) {
                isMut = true;
            } else if (consume("i_")) {
                isMut = false;
            } else {
                return nullptr;
            }
            Type* pointee = parseType();
            if (!pointee || !consume("_E")) return nullptr;
            return Ctx.getPointerType(pointee, isMut);
        }
        if (consume("g_")) {
            std::string name;
            if (!parseIdentifier(name)) return nullptr;
            return Ctx.getGenericType(name, {});
        }
        if (consume("gi_")) {
            Type* base = parseType();
            if (!base || !consume("_N")) return nullptr;
            uint64_t count = 0;
            if (!parseUInt(count)) return nullptr;
            std::vector<Type*> args;
            args.reserve(static_cast<size_t>(count));
            for (uint64_t i = 0; i < count; ++i) {
                if (!consumeChar('_')) return nullptr;
                Type* arg = parseType();
                if (!arg) return nullptr;
                args.push_back(arg);
            }
            if (!consume("_E")) return nullptr;
            return Ctx.getGenericInstanceType(base, std::move(args));
        }
        if (consume("err_")) {
            Type* success = parseType();
            if (!success || !consume("_E")) return nullptr;
            return Ctx.getErrorType(success);
        }
        if (consume("al_")) {
            std::string name;
            if (!parseIdentifier(name) || !consumeChar('_')) return nullptr;
            Type* aliased = parseType();
            if (!aliased || !consume("_E")) return nullptr;
            return Ctx.getTypeAlias(name, aliased);
        }
        if (consume("mo_")) {
            std::string name;
            if (!parseIdentifier(name)) return nullptr;
            return Ctx.getModuleType(name, {});
        }
        return nullptr;
    }

private:
    const std::string& S;
    ASTContext& Ctx;
    size_t Pos = 0;
};

} // namespace

std::string encode(Type* type) {
    return encodeTypeImpl(type);
}

Type* decode(const std::string& encoded, ASTContext& ctx) {
    Decoder decoder(encoded, ctx);
    return decoder.parseTop();
}

} // namespace typecodec
} // namespace yuan
