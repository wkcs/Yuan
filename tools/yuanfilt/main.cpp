/// \file
/// \brief yuanfilt - Yuan symbol demangler (Rust-style readable output).

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool isHexDigit(char c) {
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool isTokenChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

std::string join(const std::vector<std::string>& parts, const char* sep) {
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) out += sep;
        out += parts[i];
    }
    return out;
}

std::string toYuanPath(std::string text) {
    for (char& c : text) {
        if (c == '/' || c == '\\' || c == ':') {
            c = '.';
        }
    }
    return text;
}

class Parser {
public:
    explicit Parser(const std::string& text) : Input(text) {}

    bool parse(std::string& out) {
        if (!consume("_Y1")) {
            return false;
        }
        if (eof()) {
            return false;
        }

        char kind = Input[Pos++];
        switch (kind) {
            case 'F':
            case 'M':
                if (!parseFunction(kind, out)) {
                    return false;
                }
                break;
            case 'V':
            case 'C':
                if (!parseGlobal(kind, out)) {
                    return false;
                }
                break;
            default:
                return false;
        }

        return eof();
    }

private:
    const std::string& Input;
    size_t Pos = 0;

    bool eof() const { return Pos == Input.size(); }

    bool startsWith(const std::string& prefix) const {
        return Input.compare(Pos, prefix.size(), prefix) == 0;
    }

    bool consume(const std::string& prefix) {
        if (!startsWith(prefix)) {
            return false;
        }
        Pos += prefix.size();
        return true;
    }

    bool consumeChar(char c) {
        if (eof() || Input[Pos] != c) {
            return false;
        }
        ++Pos;
        return true;
    }

    bool parseNumber(uint64_t& value) {
        if (eof() || !std::isdigit(static_cast<unsigned char>(Input[Pos]))) {
            return false;
        }
        uint64_t result = 0;
        while (!eof() && std::isdigit(static_cast<unsigned char>(Input[Pos]))) {
            result = result * 10 + static_cast<uint64_t>(Input[Pos] - '0');
            ++Pos;
        }
        value = result;
        return true;
    }

    bool parseBit(bool& bit) {
        if (eof()) {
            return false;
        }
        if (Input[Pos] == '0') {
            bit = false;
            ++Pos;
            return true;
        }
        if (Input[Pos] == '1') {
            bit = true;
            ++Pos;
            return true;
        }
        return false;
    }

    bool parseIdentifier(std::string& out) {
        if (!consume("I")) {
            return false;
        }

        uint64_t byteLen = 0;
        if (!parseNumber(byteLen)) {
            return false;
        }
        if (!consumeChar('_')) {
            return false;
        }

        const uint64_t hexLen = byteLen * 2;
        if (Input.size() - Pos < hexLen) {
            return false;
        }

        std::string decoded;
        decoded.reserve(static_cast<size_t>(byteLen));
        for (uint64_t i = 0; i < byteLen; ++i) {
            char hi = Input[Pos + static_cast<size_t>(i * 2)];
            char lo = Input[Pos + static_cast<size_t>(i * 2 + 1)];
            if (!isHexDigit(hi) || !isHexDigit(lo)) {
                return false;
            }
            int hv = hexValue(hi);
            int lv = hexValue(lo);
            if (hv < 0 || lv < 0) {
                return false;
            }
            decoded.push_back(static_cast<char>((hv << 4) | lv));
        }

        Pos += static_cast<size_t>(hexLen);
        out = decoded;
        return true;
    }

    bool parseType(std::string& out) {
        if (consume("Tvargs_")) {
            std::string elem;
            if (!parseType(elem) || !consume("_E")) return false;
            out = "VarArgs<" + elem + ">";
            return true;
        }
        if (consume("Tstr")) {
            out = "str";
            return true;
        }
        if (consume("Tval")) {
            out = "Value";
            return true;
        }
        if (consume("Tv")) {
            out = "void";
            return true;
        }
        if (consume("Tb")) {
            out = "bool";
            return true;
        }
        if (consume("Tc")) {
            out = "char";
            return true;
        }

        if (consume("Ti")) {
            uint64_t bits = 0;
            if (!parseNumber(bits)) return false;
            out = "i" + std::to_string(bits);
            return true;
        }
        if (consume("Tu")) {
            uint64_t bits = 0;
            if (!parseNumber(bits)) return false;
            out = "u" + std::to_string(bits);
            return true;
        }
        if (consume("Tfn")) {
            uint64_t n = 0;
            if (!parseNumber(n)) return false;
            std::vector<std::string> params;
            params.reserve(static_cast<size_t>(n));
            for (uint64_t i = 0; i < n; ++i) {
                if (!consumeChar('_')) return false;
                std::string p;
                if (!parseType(p)) return false;
                params.push_back(std::move(p));
            }
            if (!consume("_R_")) return false;
            std::string ret;
            if (!parseType(ret)) return false;
            if (!consume("_Er")) return false;
            bool canError = false;
            if (!parseBit(canError)) return false;
            if (!consume("_Vr")) return false;
            bool isVariadic = false;
            if (!parseBit(isVariadic)) return false;
            if (!consume("_E")) return false;

            std::string paramText = join(params, ", ");
            if (isVariadic) {
                if (paramText.empty()) {
                    paramText = "...";
                } else {
                    paramText += ", ...";
                }
            }
            out = "func(" + paramText + ") -> " + (canError ? "!" : "") + ret;
            return true;
        }
        if (consume("Tf")) {
            uint64_t bits = 0;
            if (!parseNumber(bits)) return false;
            out = "f" + std::to_string(bits);
            return true;
        }

        if (consume("Tst_") || consume("Ten_") || consume("Ttr_") || consume("Tg_")) {
            std::string name;
            if (!parseIdentifier(name)) return false;
            out = name;
            return true;
        }
        if (consume("Tgi_")) {
            std::string base;
            if (!parseType(base) || !consume("_N")) return false;
            uint64_t n = 0;
            if (!parseNumber(n)) return false;
            std::vector<std::string> args;
            args.reserve(static_cast<size_t>(n));
            for (uint64_t i = 0; i < n; ++i) {
                if (!consumeChar('_')) return false;
                std::string a;
                if (!parseType(a)) return false;
                args.push_back(std::move(a));
            }
            if (!consume("_E")) return false;
            out = base + "<" + join(args, ", ") + ">";
            return true;
        }
        if (consume("Ttv")) {
            uint64_t id = 0;
            if (!parseNumber(id)) return false;

            size_t saved = Pos;
            if (consumeChar('_')) {
                std::string resolved;
                if (parseType(resolved) && consume("_E")) {
                    out = "?" + std::to_string(id) + "=" + resolved;
                    return true;
                }
                Pos = saved;
            }

            out = "?" + std::to_string(id);
            return true;
        }
        if (consume("Terr_")) {
            std::string succ;
            if (!parseType(succ) || !consume("_E")) return false;
            out = "!" + succ;
            return true;
        }
        if (consume("Tmo_")) {
            std::string name;
            if (!parseIdentifier(name)) return false;
            out = "module " + name;
            return true;
        }
        if (consume("Tal_")) {
            std::string alias;
            if (!parseIdentifier(alias) || !consumeChar('_')) return false;
            std::string aliased;
            if (!parseType(aliased) || !consume("_E")) return false;
            out = alias + "(alias " + aliased + ")";
            return true;
        }
        if (consume("Tra")) {
            bool inclusive = false;
            if (!parseBit(inclusive) || !consumeChar('_')) return false;
            std::string elem;
            if (!parseType(elem) || !consume("_E")) return false;
            out = "Range<" + elem + (inclusive ? ", inclusive>" : ", exclusive>");
            return true;
        }

        if (consume("Ta")) {
            uint64_t n = 0;
            if (!parseNumber(n) || !consumeChar('_')) return false;
            std::string elem;
            if (!parseType(elem) || !consume("_E")) return false;
            out = "[" + elem + "; " + std::to_string(n) + "]";
            return true;
        }
        if (consume("Ts")) {
            if (eof()) return false;
            char mk = Input[Pos++];
            if ((mk != 'm' && mk != 'i') || !consumeChar('_')) return false;
            std::string elem;
            if (!parseType(elem) || !consume("_E")) return false;
            out = (mk == 'm') ? "&mut [" + elem + "]" : "&[" + elem + "]";
            return true;
        }
        if (consume("Tt")) {
            uint64_t n = 0;
            if (!parseNumber(n)) return false;
            std::vector<std::string> elems;
            elems.reserve(static_cast<size_t>(n));
            for (uint64_t i = 0; i < n; ++i) {
                if (!consumeChar('_')) return false;
                std::string e;
                if (!parseType(e)) return false;
                elems.push_back(std::move(e));
            }
            if (!consume("_E")) return false;
            out = "(" + join(elems, ", ") + ")";
            return true;
        }
        if (consume("To_")) {
            std::string inner;
            if (!parseType(inner) || !consume("_E")) return false;
            out = "?" + inner;
            return true;
        }
        if (consume("Tr")) {
            if (eof()) return false;
            char mk = Input[Pos++];
            if ((mk != 'm' && mk != 'i') || !consumeChar('_')) return false;
            std::string pointee;
            if (!parseType(pointee) || !consume("_E")) return false;
            out = (mk == 'm') ? "&mut " + pointee : "&" + pointee;
            return true;
        }
        if (consume("Tp")) {
            if (eof()) return false;
            char mk = Input[Pos++];
            if ((mk != 'm' && mk != 'i') || !consumeChar('_')) return false;
            std::string pointee;
            if (!parseType(pointee) || !consume("_E")) return false;
            out = (mk == 'm') ? "*mut " + pointee : "*" + pointee;
            return true;
        }

        return false;
    }

    bool parseDiscriminator(std::string& out) {
        if (consume("Dnone")) {
            out = "none";
            return true;
        }
        if (consume("DL")) {
            uint64_t line = 0;
            uint64_t column = 0;
            if (!parseNumber(line) || !consumeChar('_') || !parseNumber(column)) {
                return false;
            }
            out = "line=" + std::to_string(line) + ",col=" + std::to_string(column);
            return true;
        }
        if (consume("DP")) {
            size_t start = Pos;
            while (!eof() && isHexDigit(Input[Pos])) {
                ++Pos;
            }
            if (Pos == start) {
                return false;
            }
            out = "ptr=0x" + Input.substr(start, Pos - start);
            return true;
        }
        return false;
    }

    bool parseSpecialization(std::string& out) {
        if (!consume("_S")) {
            return false;
        }
        uint64_t n = 0;
        if (!parseNumber(n)) {
            return false;
        }
        std::vector<std::string> entries;
        entries.reserve(static_cast<size_t>(n));
        for (uint64_t i = 0; i < n; ++i) {
            if (!consumeChar('_')) return false;
            std::string paramName;
            if (!parseIdentifier(paramName) || !consumeChar('_')) return false;
            std::string concrete;
            if (!parseType(concrete)) return false;
            entries.push_back(paramName + "=" + concrete);
        }
        if (!consume("_E")) {
            return false;
        }
        out = join(entries, ", ");
        return true;
    }

    bool parseFunction(char kind, std::string& out) {
        if (!consume("M")) {
            return false;
        }
        std::string moduleName;
        if (!parseIdentifier(moduleName)) {
            return false;
        }
        if (!consume("N")) {
            return false;
        }
        std::string funcName;
        if (!parseIdentifier(funcName)) {
            return false;
        }

        if (!consume("P")) return false;
        uint64_t paramCount = 0;
        if (!parseNumber(paramCount)) return false;
        std::vector<std::string> params;
        params.reserve(static_cast<size_t>(paramCount));
        for (uint64_t i = 0; i < paramCount; ++i) {
            if (!consumeChar('_')) return false;
            std::string p;
            if (!parseType(p)) return false;
            params.push_back(std::move(p));
        }
        if (!consume("_E")) return false;

        if (!consume("R_")) return false;
        std::string retType;
        if (!parseType(retType)) return false;

        if (!consume("_Er")) return false;
        bool canError = false;
        if (!parseBit(canError)) return false;
        if (!consume("_Vr")) return false;
        bool isVariadic = false;
        if (!parseBit(isVariadic)) return false;
        if (!consume("_Ar")) return false;
        bool isAsync = false;
        if (!parseBit(isAsync)) return false;

        if (!consume("G")) return false;
        uint64_t genericCount = 0;
        if (!parseNumber(genericCount)) return false;
        std::vector<std::string> genericParams;
        genericParams.reserve(static_cast<size_t>(genericCount));
        for (uint64_t i = 0; i < genericCount; ++i) {
            if (!consumeChar('_')) return false;
            std::string g;
            if (!parseIdentifier(g)) return false;
            genericParams.push_back(std::move(g));
        }
        if (!consume("_E")) return false;

        if (!consumeChar('_')) return false;
        std::string discriminator;
        if (!parseDiscriminator(discriminator)) return false;

        std::string specialization;
        if (startsWith("_S")) {
            if (!parseSpecialization(specialization)) return false;
        }

        std::ostringstream oss;
        oss << "func ";
        oss << toYuanPath(moduleName) << "." << funcName;
        if (!genericParams.empty()) {
            oss << "<" << join(genericParams, ", ") << ">";
        }
        oss << "(" << join(params, ", ") << ")";
        oss << " -> " << (canError ? "!" : "") << retType;
        if (kind == 'M') {
            oss << " [kind: method]";
        }
        oss << " [flags: error=" << (canError ? 1 : 0)
            << ", variadic=" << (isVariadic ? 1 : 0)
            << ", async=" << (isAsync ? 1 : 0) << "]";
        if (!specialization.empty()) {
            oss << " [specialization: " << specialization << "]";
        }
        oss << " [discriminator: " << discriminator << "]";
        out = oss.str();
        return true;
    }

    bool parseGlobal(char kind, std::string& out) {
        if (!consume("M")) return false;
        std::string moduleName;
        if (!parseIdentifier(moduleName)) return false;
        if (!consume("N")) return false;
        std::string name;
        if (!parseIdentifier(name)) return false;
        if (!consume("T_")) return false;
        std::string typeText;
        if (!parseType(typeText)) return false;
        if (!consumeChar('_')) return false;
        std::string discriminator;
        if (!parseDiscriminator(discriminator)) return false;

        std::ostringstream oss;
        oss << (kind == 'V' ? "global var " : "global const ");
        oss << toYuanPath(moduleName) << "." << name << ": " << typeText;
        oss << " [discriminator: " << discriminator << "]";
        out = oss.str();
        return true;
    }
};

bool demangleSymbol(const std::string& symbol, std::string& out) {
    if (symbol == "yuan_main") {
        out = "func <entry>.main() -> i32 [yuan runtime entry]";
        return true;
    }
    if (symbol == "main") {
        out = "extern C main";
        return true;
    }
    if (symbol.rfind("_Y1", 0) != 0) {
        return false;
    }

    Parser parser(symbol);
    return parser.parse(out);
}

std::string demangleToken(const std::string& token) {
    std::string core = token;

    // Accept LLVM-style symbol decoration, e.g. @_Y1..., %_Y1..., @"_Y1..."
    if (!core.empty() && (core.front() == '@' || core.front() == '%')) {
        core.erase(core.begin());
    }
    if (core.size() >= 2 && core.front() == '"' && core.back() == '"') {
        core = core.substr(1, core.size() - 2);
    }

    std::string demangled;
    if (demangleSymbol(core, demangled)) {
        return demangled;
    }
    return token;
}

std::string demangleLine(const std::string& line) {
    std::string out;
    out.reserve(line.size());

    size_t i = 0;
    while (i < line.size()) {
        if (isTokenChar(line[i])) {
            size_t j = i + 1;
            while (j < line.size() && isTokenChar(line[j])) {
                ++j;
            }
            std::string token = line.substr(i, j - i);
            out += demangleToken(token);
            i = j;
            continue;
        }
        out.push_back(line[i]);
        ++i;
    }

    return out;
}

void printHelp(const char* argv0) {
    std::cout << "Usage: " << argv0 << " [symbol ...]\n";
    std::cout << "If no symbols are provided, reads stdin and demangles token by token.\n";
    std::cout << "Example:\n";
    std::cout << "  " << argv0 << " _Y1F...\n";
    std::cout << "  nm a.out | " << argv0 << "\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc > 1) {
        if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
            printHelp(argv[0]);
            return 0;
        }
        for (int i = 1; i < argc; ++i) {
            std::cout << demangleToken(argv[i]) << '\n';
        }
        return 0;
    }

    std::string line;
    while (std::getline(std::cin, line)) {
        std::cout << demangleLine(line) << '\n';
    }
    return 0;
}
