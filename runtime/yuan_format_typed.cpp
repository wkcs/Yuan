/// \file yuan_format_typed.cpp
/// \brief Yuan 格式化运行时库实现（带类型标记）
///
/// 使用类型标记来处理不同类型的参数
/// 调用方式：yuan_format(format_str, arg_count, type1, value1, type2, value2, ...)

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
#include <cstdarg>

extern "C" {

/// \brief Yuan 字符串结构体
struct YuanString {
    const char* data;
    int64_t length;
};

/// \brief 参数类型枚举
enum YuanArgType : int32_t {
    TYPE_STRING = 0,
    TYPE_I32 = 1,
    TYPE_I64 = 2,
    TYPE_F32 = 3,
    TYPE_F64 = 4,
    TYPE_BOOL = 5,
    TYPE_CHAR = 6,
};

struct FormatSpec {
    int width = 0;
    int precision = -1;
    bool zeroPad = false;
    char type = 0;
};

struct ArgValue {
    YuanArgType type;
    int64_t i64 = 0;
    double f64 = 0.0;
    const char* str = nullptr;
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

static std::string formatArgValue(const ArgValue& arg, const FormatSpec& spec) {
    switch (arg.type) {
        case TYPE_STRING: {
            std::string out = arg.str ? arg.str : "(null)";
            if (spec.precision >= 0 && static_cast<int>(out.size()) > spec.precision) {
                out.resize(static_cast<size_t>(spec.precision));
            }
            return applyWidth(out, spec);
        }
        case TYPE_I32:
        case TYPE_I64:
            return formatIntValue(arg.i64, spec);
        case TYPE_F32:
        case TYPE_F64:
            return formatFloatValue(arg.f64, spec);
        case TYPE_BOOL: {
            std::string out = arg.i64 ? "true" : "false";
            return applyWidth(out, spec);
        }
        case TYPE_CHAR: {
            char buffer[2];
            buffer[0] = static_cast<char>(arg.i64 & 0xFF);
            buffer[1] = '\0';
            return applyWidth(std::string(buffer), spec);
        }
        default:
            return "(unknown)";
    }
}

/// \brief 格式化字符串函数（带类型标记）
/// \param format 格式化字符串
/// \param argc 参数数量
/// \param ... 类型和值对：type1, value1, type2, value2, ...
/// \return 格式化后的字符串
YuanString yuan_format(const char* format, int32_t argc, ...) {
    if (!format) {
        YuanString result;
        result.data = strdup("");
        result.length = 0;
        return result;
    }

    va_list args;
    va_start(args, argc);

    // 读取所有参数（类型和值）
    std::vector<ArgValue> argValues;
    argValues.reserve(static_cast<size_t>(argc));
    for (int32_t i = 0; i < argc; ++i) {
        YuanArgType type = static_cast<YuanArgType>(va_arg(args, int32_t));
        ArgValue arg;
        arg.type = type;
        switch (type) {
            case TYPE_STRING:
                arg.str = va_arg(args, const char*);
                break;
            case TYPE_I32:
                arg.i64 = static_cast<int64_t>(va_arg(args, int32_t));
                break;
            case TYPE_I64:
                arg.i64 = va_arg(args, int64_t);
                break;
            case TYPE_F32:
                arg.f64 = static_cast<double>(va_arg(args, double));
                break;
            case TYPE_F64:
                arg.f64 = va_arg(args, double);
                break;
            case TYPE_BOOL:
            case TYPE_CHAR:
                arg.i64 = static_cast<int64_t>(va_arg(args, int));
                break;
            default:
                break;
        }
        argValues.push_back(arg);
    }

    va_end(args);

    // 解析格式化字符串并替换占位符
    std::ostringstream result;
    size_t formatLen = strlen(format);
    size_t autoIndex = 0;

    for (size_t i = 0; i < formatLen; ++i) {
        if (format[i] == '{') {
            // 检查转义 {{
            if (i + 1 < formatLen && format[i + 1] == '{') {
                result << '{';
                ++i;
                continue;
            }

            // 查找匹配的 }
            size_t closePos = i + 1;
            while (closePos < formatLen && format[closePos] != '}') {
                ++closePos;
            }

            if (closePos >= formatLen) {
                result << '{';
                continue;
            }

            // 提取占位符内容
            std::string placeholder(format + i + 1, format + closePos);

            // 拆分 index 和 format spec
            std::string indexPart = placeholder;
            std::string specPart;
            auto colonPos = placeholder.find(':');
            if (colonPos != std::string::npos) {
                indexPart = placeholder.substr(0, colonPos);
                specPart = placeholder.substr(colonPos + 1);
            }

            // 确定参数索引
            size_t argIndex;
            if (indexPart.empty()) {
                argIndex = autoIndex++;
            } else {
                try {
                    argIndex = std::stoul(indexPart);
                } catch (...) {
                    result << '{' << placeholder << '}';
                    i = closePos;
                    continue;
                }
            }

            FormatSpec spec = parseFormatSpec(specPart);

            if (argIndex < argValues.size()) {
                result << formatArgValue(argValues[argIndex], spec);
            } else {
                result << "{out of range}";
            }

            i = closePos;
        } else if (format[i] == '}') {
            // 检查转义 }}
            if (i + 1 < formatLen && format[i + 1] == '}') {
                result << '}';
                ++i;
                continue;
            }
            result << '}';
        } else {
            result << format[i];
        }
    }

    // 分配结果字符串
    std::string resultStr = result.str();
    char* resultData = static_cast<char*>(malloc(resultStr.length() + 1));
    if (resultData) {
        memcpy(resultData, resultStr.c_str(), resultStr.length());
        resultData[resultStr.length()] = '\0';
    } else {
        resultData = strdup("");
    }

    YuanString yuanResult;
    yuanResult.data = resultData;
    yuanResult.length = static_cast<int64_t>(resultStr.length());
    return yuanResult;
}

} // extern "C"
