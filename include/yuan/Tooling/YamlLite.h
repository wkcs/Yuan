#ifndef YUAN_TOOLING_YAMLLITE_H
#define YUAN_TOOLING_YAMLLITE_H

#include <cctype>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace yuan::yaml_lite {

struct Value {
    enum class Kind { Null, Scalar, Map, Seq };
    Kind kind = Kind::Null;
    std::string scalar;
    std::map<std::string, Value> map;
    std::vector<Value> seq;

    static Value makeNull() { return Value{}; }
    static Value makeScalar(std::string text) {
        Value v;
        v.kind = Kind::Scalar;
        v.scalar = std::move(text);
        return v;
    }
    static Value makeMap() {
        Value v;
        v.kind = Kind::Map;
        return v;
    }
    static Value makeSeq() {
        Value v;
        v.kind = Kind::Seq;
        return v;
    }

    bool isNull() const { return kind == Kind::Null; }
    bool isScalar() const { return kind == Kind::Scalar; }
    bool isMap() const { return kind == Kind::Map; }
    bool isSeq() const { return kind == Kind::Seq; }
};

struct ParseError {
    size_t line = 0;
    std::string message;
};

namespace detail {

inline std::string ltrim(std::string s) {
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    s.erase(0, i);
    return s;
}

inline std::string rtrim(std::string s) {
    size_t i = s.size();
    while (i > 0 && std::isspace(static_cast<unsigned char>(s[i - 1]))) {
        --i;
    }
    s.erase(i);
    return s;
}

inline std::string trim(std::string s) {
    return rtrim(ltrim(std::move(s)));
}

inline std::string stripComments(const std::string& line) {
    bool inSingle = false;
    bool inDouble = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"' && !inSingle) {
            if (i == 0 || line[i - 1] != '\\') {
                inDouble = !inDouble;
            }
        } else if (c == '\'' && !inDouble) {
            inSingle = !inSingle;
        } else if (c == '#' && !inSingle && !inDouble) {
            return line.substr(0, i);
        }
    }
    return line;
}

inline std::string unquote(const std::string& text) {
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        std::string out;
        out.reserve(text.size());
        for (size_t i = 1; i + 1 < text.size(); ++i) {
            char c = text[i];
            if (c == '\\' && i + 1 < text.size() - 1) {
                char n = text[i + 1];
                switch (n) {
                    case 'n': out.push_back('\n'); break;
                    case 't': out.push_back('\t'); break;
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    default: out.push_back(n); break;
                }
                ++i;
            } else {
                out.push_back(c);
            }
        }
        return out;
    }
    if (text.size() >= 2 && text.front() == '\'' && text.back() == '\'') {
        std::string out;
        out.reserve(text.size());
        for (size_t i = 1; i + 1 < text.size(); ++i) {
            char c = text[i];
            if (c == '\'' && i + 1 < text.size() - 1 && text[i + 1] == '\'') {
                out.push_back('\'');
                ++i;
            } else {
                out.push_back(c);
            }
        }
        return out;
    }
    return text;
}

inline size_t findUnquotedColon(const std::string& text) {
    bool inSingle = false;
    bool inDouble = false;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '"' && !inSingle) {
            if (i == 0 || text[i - 1] != '\\') {
                inDouble = !inDouble;
            }
        } else if (c == '\'' && !inDouble) {
            inSingle = !inSingle;
        } else if (c == ':' && !inSingle && !inDouble) {
            return i;
        }
    }
    return std::string::npos;
}

struct Line {
    size_t lineNo = 0;
    int indent = 0;
    bool isSeq = false;
    std::string key;
    std::string value;
};

inline bool isBlankOrComment(const std::string& line) {
    for (char c : line) {
        if (c == '#') {
            return true;
        }
        if (!std::isspace(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

inline bool parseLines(const std::string& text,
                       std::vector<Line>& lines,
                       ParseError& error) {
    std::istringstream iss(text);
    std::string raw;
    size_t lineNo = 0;
    while (std::getline(iss, raw)) {
        ++lineNo;
        if (!raw.empty() && raw.back() == '\r') {
            raw.pop_back();
        }
        if (isBlankOrComment(raw)) {
            continue;
        }
        std::string noComment = stripComments(raw);
        noComment = rtrim(noComment);
        if (noComment.empty()) {
            continue;
        }
        int indent = 0;
        for (char c : noComment) {
            if (c == ' ') {
                ++indent;
                continue;
            }
            if (c == '\t') {
                error = {lineNo, "不支持 tab 缩进"};
                return false;
            }
            break;
        }
        std::string content = noComment.substr(indent);
        content = rtrim(content);
        if (content.empty()) {
            continue;
        }
        Line line;
        line.lineNo = lineNo;
        line.indent = indent;
        if (content[0] == '-') {
            line.isSeq = true;
            std::string rest = content.substr(1);
            if (!rest.empty() && rest[0] == ' ') {
                rest.erase(rest.begin());
            }
            line.value = trim(rest);
            lines.push_back(std::move(line));
            continue;
        }

        size_t colon = findUnquotedColon(content);
        if (colon == std::string::npos) {
            error = {lineNo, "缺少 ':' 分隔符"};
            return false;
        }
        line.isSeq = false;
        line.key = trim(content.substr(0, colon));
        if (line.key.empty()) {
            error = {lineNo, "空键名"};
            return false;
        }
        std::string value = content.substr(colon + 1);
        line.value = trim(value);
        lines.push_back(std::move(line));
    }
    return true;
}

inline Value parseInlineValue(const std::string& text) {
    if (text == "{}") {
        return Value::makeMap();
    }
    if (text == "[]") {
        return Value::makeSeq();
    }
    return Value::makeScalar(unquote(text));
}

inline bool parseBlock(const std::vector<Line>& lines,
                       size_t& index,
                       int indent,
                       Value& out,
                       ParseError& error);

inline bool parseSeq(const std::vector<Line>& lines,
                     size_t& index,
                     int indent,
                     Value& out,
                     ParseError& error) {
    Value seq = Value::makeSeq();
    while (index < lines.size() && lines[index].indent == indent && lines[index].isSeq) {
        const Line& ln = lines[index];
        ++index;
        if (ln.value.empty()) {
            if (index < lines.size() && lines[index].indent > indent) {
                Value child;
                if (!parseBlock(lines, index, lines[index].indent, child, error)) {
                    return false;
                }
                seq.seq.push_back(std::move(child));
            } else {
                seq.seq.push_back(Value::makeScalar(""));
            }
        } else {
            seq.seq.push_back(parseInlineValue(ln.value));
        }
    }
    if (index < lines.size() && lines[index].indent == indent && !lines[index].isSeq) {
        error = {lines[index].lineNo, "同一层级混合序列与映射"};
        return false;
    }
    out = std::move(seq);
    return true;
}

inline bool parseMap(const std::vector<Line>& lines,
                     size_t& index,
                     int indent,
                     Value& out,
                     ParseError& error) {
    Value map = Value::makeMap();
    while (index < lines.size() && lines[index].indent == indent && !lines[index].isSeq) {
        const Line& ln = lines[index];
        ++index;
        std::string key = unquote(ln.key);
        if (ln.value.empty()) {
            if (index < lines.size() && lines[index].indent > indent) {
                Value child;
                if (!parseBlock(lines, index, lines[index].indent, child, error)) {
                    return false;
                }
                map.map[key] = std::move(child);
            } else {
                map.map[key] = Value::makeScalar("");
            }
        } else {
            map.map[key] = parseInlineValue(ln.value);
        }
    }
    if (index < lines.size() && lines[index].indent == indent && lines[index].isSeq) {
        error = {lines[index].lineNo, "同一层级混合序列与映射"};
        return false;
    }
    out = std::move(map);
    return true;
}

inline bool parseBlock(const std::vector<Line>& lines,
                       size_t& index,
                       int indent,
                       Value& out,
                       ParseError& error) {
    if (index >= lines.size()) {
        out = Value::makeNull();
        return true;
    }
    if (lines[index].indent != indent) {
        error = {lines[index].lineNo, "缩进不一致"};
        return false;
    }
    if (lines[index].isSeq) {
        return parseSeq(lines, index, indent, out, error);
    }
    return parseMap(lines, index, indent, out, error);
}

} // namespace detail

inline bool parse(const std::string& text, Value& out, ParseError& error) {
    std::vector<detail::Line> lines;
    if (!detail::parseLines(text, lines, error)) {
        return false;
    }
    if (lines.empty()) {
        out = Value::makeMap();
        return true;
    }
    size_t index = 0;
    return detail::parseBlock(lines, index, lines[0].indent, out, error);
}

inline const Value* lookup(const Value& map, const std::string& key) {
    if (!map.isMap()) {
        return nullptr;
    }
    auto it = map.map.find(key);
    if (it == map.map.end()) {
        return nullptr;
    }
    return &it->second;
}

} // namespace yuan::yaml_lite

#endif // YUAN_TOOLING_YAMLLITE_H
