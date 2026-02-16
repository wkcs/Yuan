/// \file yuan_varargs.cpp
/// \brief Yuan 可变参数与 Value 运行时支持

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <iomanip>

extern "C" {

/// \brief Yuan 字符串结构体
struct YuanString {
    const char* data;
    int64_t length;
};

/// \brief Value 类型标记
enum YuanValueTag : int32_t {
    TYPE_STRING = 0,
    TYPE_I32 = 1,
    TYPE_I64 = 2,
    TYPE_F32 = 3,
    TYPE_F64 = 4,
    TYPE_BOOL = 5,
    TYPE_CHAR = 6,
};

/// \brief Yuan 动态值
struct YuanValue {
    int32_t tag;
    int32_t padding;
    int64_t data0;
    int64_t data1;
};

static void debug_log(const char* label, int64_t a = 0, int64_t b = 0) {
    return;
    std::fprintf(stderr, "[yuan_varargs] %s a=%lld b=%lld\n",
                 label,
                 static_cast<long long>(a),
                 static_cast<long long>(b));
}

static void debug_log_ptr(const char* label, const void* ptr) {
    return;
    std::fprintf(stderr, "[yuan_varargs] %s ptr=%p\n", label, ptr);
}

static void debug_log_value(const char* label, const YuanValue& value) {
    return;
    std::fprintf(stderr,
                 "[yuan_varargs] %s tag=%d data0=%lld data1=%lld\n",
                 label,
                 static_cast<int>(value.tag),
                 static_cast<long long>(value.data0),
                 static_cast<long long>(value.data1));
}

/// \brief Yuan 可变参数结构
struct YuanVarArgs {
    int64_t len;
    YuanValue* values;
};

static YuanString make_string(const std::string& s) {
    debug_log("make_string.len", static_cast<int64_t>(s.size()), 0);
    YuanString result;
    char* data = static_cast<char*>(malloc(s.size() + 1));
    if (!data) {
        result.data = "";
        result.length = 0;
        return result;
    }
    memcpy(data, s.data(), s.size());
    data[s.size()] = '\0';
    result.data = data;
    result.length = static_cast<int64_t>(s.size());
    debug_log("make_string.ptr", reinterpret_cast<int64_t>(data), result.length);
    return result;
}

static double double_from_bits(int64_t bits) {
    double value = 0.0;
    memcpy(&value, &bits, sizeof(double));
    return value;
}

struct FormatSpec {
    int width = 0;
    int precision = -1;
    bool zeroPad = false;
    char type = 0;
};

static FormatSpec parseFormatSpec(const std::string& spec) {
    FormatSpec fs;
    if (spec.empty()) {
        return fs;
    }

    std::string work = spec;
    if (!work.empty() && std::isalpha(static_cast<unsigned char>(work.back()))) {
        fs.type = work.back();
        work.pop_back();
    }

    std::string widthPart = work;
    std::string precPart;
    auto dotPos = work.find('.');
    if (dotPos != std::string::npos) {
        widthPart = work.substr(0, dotPos);
        precPart = work.substr(dotPos + 1);
        if (!precPart.empty()) {
            fs.precision = std::atoi(precPart.c_str());
        } else {
            fs.precision = 0;
        }
    }

    if (!widthPart.empty() && widthPart[0] == '0') {
        fs.zeroPad = true;
        widthPart.erase(widthPart.begin());
    }

    if (!widthPart.empty()) {
        fs.width = std::atoi(widthPart.c_str());
    }

    return fs;
}

static std::string applyWidth(std::string value, const FormatSpec& spec) {
    if (spec.width <= 0) {
        return value;
    }
    if (static_cast<int>(value.size()) >= spec.width) {
        return value;
    }
    size_t padCount = static_cast<size_t>(spec.width - static_cast<int>(value.size()));
    char padChar = spec.zeroPad ? '0' : ' ';

    if (padChar == '0' && !value.empty() && value[0] == '-') {
        std::string result;
        result.reserve(static_cast<size_t>(spec.width));
        result.push_back('-');
        result.append(padCount, '0');
        result.append(value.substr(1));
        return result;
    }

    return std::string(padCount, padChar) + value;
}

static std::string formatIntValue(int64_t value, const FormatSpec& spec) {
    std::ostringstream oss;
    char type = spec.type;

    if (type == 'x' || type == 'X') {
        oss << std::hex << static_cast<uint64_t>(value);
    } else if (type == 'o') {
        oss << std::oct << static_cast<uint64_t>(value);
    } else if (type == 'b') {
        uint64_t v = static_cast<uint64_t>(value);
        if (v == 0) {
            oss << '0';
        } else {
            std::string bits;
            while (v > 0) {
                bits.push_back((v & 1) ? '1' : '0');
                v >>= 1;
            }
            std::reverse(bits.begin(), bits.end());
            oss << bits;
        }
    } else {
        oss << value;
    }

    std::string out = oss.str();
    if (type == 'X') {
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
    }

    return applyWidth(out, spec);
}

static std::string formatFloatValue(double value, const FormatSpec& spec) {
    std::ostringstream oss;
    if (spec.precision >= 0) {
        oss << std::fixed << std::setprecision(spec.precision);
    }

    char type = spec.type;
    if (type == 'e' || type == 'E') {
        oss << std::scientific;
    }

    oss << value;
    std::string out = oss.str();

    if (type == 'E') {
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
    }

    return applyWidth(out, spec);
}

static std::string valueToString(const YuanValue& value) {
    debug_log("valueToString.tag", value.tag, value.data1);
    switch (static_cast<YuanValueTag>(value.tag)) {
        case TYPE_STRING: {
            const char* ptr = reinterpret_cast<const char*>(static_cast<intptr_t>(value.data0));
            int64_t len = value.data1;
            debug_log("valueToString.string", reinterpret_cast<intptr_t>(ptr), len);
            if (!ptr) {
                return "";
            }
            if (len < 0) {
                return std::string(ptr);
            }
            return std::string(ptr, static_cast<size_t>(len));
        }
        case TYPE_I32:
            return formatIntValue(static_cast<int64_t>(static_cast<int32_t>(value.data0)), FormatSpec{});
        case TYPE_I64:
            return formatIntValue(static_cast<int64_t>(value.data0), FormatSpec{});
        case TYPE_F32: {
            double d = double_from_bits(value.data0);
            return formatFloatValue(static_cast<float>(d), FormatSpec{});
        }
        case TYPE_F64: {
            double d = double_from_bits(value.data0);
            return formatFloatValue(d, FormatSpec{});
        }
        case TYPE_BOOL:
            return value.data0 ? "true" : "false";
        case TYPE_CHAR: {
            char buffer[2];
            buffer[0] = static_cast<char>(value.data0 & 0xFF);
            buffer[1] = '\0';
            return std::string(buffer);
        }
        default:
            return "<unknown>";
    }
}

static std::string formatValue(const YuanValue& value, const FormatSpec& spec) {
    debug_log_value("formatValue", value);
    switch (static_cast<YuanValueTag>(value.tag)) {
        case TYPE_STRING: {
            const char* ptr = reinterpret_cast<const char*>(static_cast<intptr_t>(value.data0));
            int64_t len = value.data1;
            std::string out;
            debug_log_ptr("formatValue.str.ptr", ptr);
            debug_log("formatValue.str.len", len, 0);
            if (ptr) {
                if (len < 0) {
                    debug_log("formatValue.str.copy", -1, 0);
                    out = std::string(ptr);
                } else {
                    debug_log("formatValue.str.copy", len, 0);
                    out = std::string(ptr, static_cast<size_t>(len));
                }
                debug_log("formatValue.str.built", static_cast<int64_t>(out.size()), 0);
            }
            if (spec.precision >= 0 && static_cast<int>(out.size()) > spec.precision) {
                out.resize(static_cast<size_t>(spec.precision));
            }
            return applyWidth(out, spec);
        }
        case TYPE_I32:
            return formatIntValue(static_cast<int64_t>(static_cast<int32_t>(value.data0)), spec);
        case TYPE_I64:
            return formatIntValue(static_cast<int64_t>(value.data0), spec);
        case TYPE_F32: {
            double d = double_from_bits(value.data0);
            return formatFloatValue(static_cast<float>(d), spec);
        }
        case TYPE_F64: {
            double d = double_from_bits(value.data0);
            return formatFloatValue(d, spec);
        }
        case TYPE_BOOL: {
            std::string out = value.data0 ? "true" : "false";
            return applyWidth(out, spec);
        }
        case TYPE_CHAR: {
            char buffer[2];
            buffer[0] = static_cast<char>(value.data0 & 0xFF);
            buffer[1] = '\0';
            return applyWidth(std::string(buffer), spec);
        }
        default:
            return "<unknown>";
    }
}

static void formatWithValues(const std::string& format, const YuanValue* values, int64_t count, std::string& out) {
    size_t formatLen = format.size();
    size_t autoIndex = 0;
    debug_log("formatWithValues.count", count, static_cast<int64_t>(formatLen));
    debug_log_ptr("formatWithValues.values", values);

    out.clear();
    out.reserve(formatLen + 16);

    for (size_t i = 0; i < formatLen; ++i) {
        char ch = format[i];
        if (ch == '{') {
            if (i + 1 < formatLen && format[i + 1] == '{') {
                out.push_back('{');
                ++i;
                continue;
            }

            size_t closePos = i + 1;
            while (closePos < formatLen && format[closePos] != '}') {
                ++closePos;
            }

            if (closePos >= formatLen) {
                out.push_back('{');
                continue;
            }

            std::string placeholder(format.data() + i + 1, format.data() + closePos);
            std::string indexPart = placeholder;
            std::string specPart;
            auto colonPos = placeholder.find(':');
            if (colonPos != std::string::npos) {
                indexPart = placeholder.substr(0, colonPos);
                specPart = placeholder.substr(colonPos + 1);
            }

            size_t argIndex;
            if (indexPart.empty()) {
                argIndex = autoIndex++;
            } else {
                try {
                    argIndex = std::stoul(indexPart);
                } catch (...) {
                    out.push_back('{');
                    out.append(placeholder);
                    out.push_back('}');
                    i = closePos;
                    continue;
                }
            }

            FormatSpec spec = parseFormatSpec(specPart);
            if (argIndex < static_cast<size_t>(count)) {
                debug_log("formatWithValues.argIndex", static_cast<int64_t>(argIndex), static_cast<int64_t>(count));
                debug_log_value("formatWithValues.value", values[argIndex]);
                std::string formatted = formatValue(values[argIndex], spec);
                out.append(formatted);
            } else {
                out.append("{out of range}");
            }

            i = closePos;
        } else if (ch == '}') {
            if (i + 1 < formatLen && format[i + 1] == '}') {
                out.push_back('}');
                ++i;
                continue;
            }
            out.push_back('}');
        } else {
            out.push_back(ch);
        }
    }

    debug_log("formatWithValues.done", static_cast<int64_t>(out.size()), 0);
}

/// \brief 获取 VarArgs 中指定索引的 Value（带边界检查）
YuanValue yuan_varargs_get(int64_t len, YuanValue* values, int64_t idx) {
    if (idx < 0 || idx >= len || values == nullptr) {
        std::fprintf(stderr, "Yuan runtime error: varargs index out of range\n");
        std::abort();
    }
    return values[idx];
}

/// \brief 将 Value 转换为字符串
YuanString yuan_value_to_string(YuanValue value) {
    switch (static_cast<YuanValueTag>(value.tag)) {
        case TYPE_STRING: {
            YuanString result;
            result.data = reinterpret_cast<const char*>(static_cast<intptr_t>(value.data0));
            result.length = value.data1;
            return result;
        }
        case TYPE_I32: {
            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int32_t>(value.data0));
            return make_string(buffer);
        }
        case TYPE_I64: {
            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value.data0));
            return make_string(buffer);
        }
        case TYPE_F32: {
            char buffer[64];
            double d = double_from_bits(value.data0);
            std::snprintf(buffer, sizeof(buffer), "%g", static_cast<float>(d));
            return make_string(buffer);
        }
        case TYPE_F64: {
            char buffer[64];
            double d = double_from_bits(value.data0);
            std::snprintf(buffer, sizeof(buffer), "%g", d);
            return make_string(buffer);
        }
        case TYPE_BOOL: {
            return make_string(value.data0 ? "true" : "false");
        }
        case TYPE_CHAR: {
            char buffer[2];
            buffer[0] = static_cast<char>(value.data0 & 0xFF);
            buffer[1] = '\0';
            return make_string(buffer);
        }
        default:
            return make_string("<unknown>");
    }
}

/// \brief 使用 Value 参数格式化字符串
YuanString yuan_format_values(const char* format, int64_t len, YuanValue* values) {
    debug_log("format_values.len", len, 0);
    if (!format) {
        return make_string("");
    }

    std::string fmt(format);
    if (!values || len <= 0) {
        debug_log("format_values.empty", len, 0);
        return make_string(fmt);
    }

    std::string formatted;
    formatWithValues(fmt, values, len, formatted);
    return make_string(formatted);
}

/// \brief 使用 VarArgs 格式化字符串（第一个参数为格式字符串）
YuanString yuan_format_all(int64_t len, YuanValue* values) {
    debug_log("format_all.len", len, 0);
    if (!values || len <= 0) {
        debug_log("format_all.empty", len, 0);
        return make_string("");
    }

    YuanValue first = values[0];
    debug_log("format_all.first.tag", first.tag, first.data1);
    if (static_cast<YuanValueTag>(first.tag) != TYPE_STRING) {
        if (len == 1) {
            return make_string(valueToString(first));
        }
        std::fprintf(stderr, "Yuan runtime error: format string must be str\n");
        return make_string("");
    }

    const char* fmtPtr = reinterpret_cast<const char*>(static_cast<intptr_t>(first.data0));
    int64_t fmtLen = first.data1;
    debug_log("format_all.fmt", reinterpret_cast<intptr_t>(fmtPtr), fmtLen);
    if (!fmtPtr) {
        return make_string("");
    }
    std::string fmt;
    if (fmtLen < 0) {
        fmt = std::string(fmtPtr);
    } else {
        fmt = std::string(fmtPtr, static_cast<size_t>(fmtLen));
    }

    if (len <= 1) {
        return make_string(fmt);
    }

    std::string formatted;
    formatWithValues(fmt, values + 1, len - 1, formatted);
    debug_log("format_all.formatted", static_cast<int64_t>(formatted.size()), 0);
    debug_log_ptr("format_all.formatted.ptr", formatted.data());
    return make_string(formatted);
}

} // extern "C"
