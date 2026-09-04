module;
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

export module Json;

// Minimal, exception'suz, bağımlılıksız DOM JSON parser (SADECE OKUMA).
// Legacy parrot json.cpp portu — serileştirme/raw/dirty makinesi çıkarıldı.

export namespace json {

enum class Type { Null, Bool, Number, String, Array, Object };

struct Value {
    Type type = Type::Null;
    bool b = false;
    std::string num;   // ham sayı metni (Type::Number)
    std::string str;   // çözülmüş string (Type::String)
    std::vector<Value> arr;                        // Type::Array
    std::vector<std::pair<std::string, Value>> obj; // Type::Object

    const Value* Find(const char* key) const {
        for (const auto& kv : obj)
            if (kv.first == key)
                return &kv.second;
        return nullptr;
    }
};

bool Parse(const char* data, std::size_t len, Value& out);
const Value* Find(const Value& v, const char* key);
long GetLong(const Value& v, const char* key, long dflt);
double GetDouble(const Value& v, const char* key, double dflt);
bool GetBool(const Value& v, const char* key, bool dflt);

}  // namespace json

namespace {

struct Cursor {
    const char* p;
    const char* end;
};

void SkipWs(Cursor& c) {
    for (;;) {
        while (c.p < c.end && (*c.p == ' ' || *c.p == '\t' || *c.p == '\n' || *c.p == '\r'))
            ++c.p;
        if (c.p + 1 < c.end && c.p[0] == '/' && c.p[1] == '/') {
            c.p += 2;
            while (c.p < c.end && *c.p != '\n')
                ++c.p;
            continue;
        }
        if (c.p + 1 < c.end && c.p[0] == '/' && c.p[1] == '*') {
            c.p += 2;
            while (c.p + 1 < c.end && !(c.p[0] == '*' && c.p[1] == '/'))
                ++c.p;
            if (c.p + 1 < c.end)
                c.p += 2;
            continue;
        }
        break;
    }
}

bool Consume(Cursor& c, char ch) {
    SkipWs(c);
    if (c.p < c.end && *c.p == ch) {
        ++c.p;
        return true;
    }
    return false;
}

bool ParseString(Cursor& c, std::string& out) {
    SkipWs(c);
    if (c.p >= c.end || *c.p != '"')
        return false;
    ++c.p;
    out.clear();
    while (c.p < c.end) {
        const char ch = *c.p;
        if (ch == '"') {
            ++c.p;
            return true;
        }
        if (ch == '\\') {
            if (++c.p >= c.end)
                return false;
            const char esc = *c.p++;
            switch (esc) {
                case '"':  out += '"'; break;
                case '\\': out += '\\'; break;
                case '/':  out += '/'; break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'u': {
                    // Basit \uXXXX (surrogate çifti desteği olmadan) -> UTF-8.
                    if (c.end - c.p < 4) return false;
                    int v = 0;
                    for (int i = 0; i < 4; ++i) {
                        char h = *c.p++;
                        int d = 0;
                        if (h >= '0' && h <= '9') d = h - '0';
                        else if (h >= 'a' && h <= 'f') d = h - 'a' + 10;
                        else if (h >= 'A' && h <= 'F') d = h - 'A' + 10;
                        else return false;
                        v = (v << 4) | d;
                    }
                    if (v < 0x80) out += static_cast<char>(v);
                    else if (v < 0x800) {
                        out += static_cast<char>(0xC0 | (v >> 6));
                        out += static_cast<char>(0x80 | (v & 0x3F));
                    } else {
                        out += static_cast<char>(0xE0 | (v >> 12));
                        out += static_cast<char>(0x80 | ((v >> 6) & 0x3F));
                        out += static_cast<char>(0x80 | (v & 0x3F));
                    }
                    break;
                }
                default: return false;
            }
        } else {
            out += ch;
            ++c.p;
        }
    }
    return false;
}

bool ParseNumberToken(Cursor& c, std::string& out) {
    SkipWs(c);
    const char* const start = c.p;
    if (c.p < c.end && *c.p == '-')
        ++c.p;
    while (c.p < c.end && *c.p >= '0' && *c.p <= '9')
        ++c.p;
    if (c.p < c.end && *c.p == '.') {
        ++c.p;
        while (c.p < c.end && *c.p >= '0' && *c.p <= '9')
            ++c.p;
    }
    if (c.p < c.end && (*c.p == 'e' || *c.p == 'E')) {
        ++c.p;
        if (c.p < c.end && (*c.p == '-' || *c.p == '+'))
            ++c.p;
        while (c.p < c.end && *c.p >= '0' && *c.p <= '9')
            ++c.p;
    }
    if (c.p == start)
        return false;
    out.assign(start, static_cast<std::size_t>(c.p - start));
    return true;
}

bool ParseLiteral(Cursor& c, json::Value& out) {
    SkipWs(c);
    for (const char* lit : {"true", "false", "null"}) {
        const std::size_t n = std::strlen(lit);
        if (static_cast<std::size_t>(c.end - c.p) >= n && std::strncmp(c.p, lit, n) == 0) {
            c.p += n;
            if (lit[0] == 't') { out.type = json::Type::Bool; out.b = true; }
            else if (lit[0] == 'f') { out.type = json::Type::Bool; out.b = false; }
            else { out.type = json::Type::Null; }
            return true;
        }
    }
    return false;
}

bool ParseValue(Cursor& c, json::Value& out) {
    SkipWs(c);
    if (c.p >= c.end)
        return false;
    switch (*c.p) {
        case '{': {
            out.type = json::Type::Object;
            ++c.p;
            if (Consume(c, '}')) return true;
            for (;;) {
                std::string key;
                if (!ParseString(c, key) || !Consume(c, ':')) return false;
                json::Value child;
                if (!ParseValue(c, child)) return false;
                out.obj.emplace_back(std::move(key), std::move(child));
                if (Consume(c, '}')) return true;
                if (!Consume(c, ',')) return false;
            }
        }
        case '[': {
            out.type = json::Type::Array;
            ++c.p;
            if (Consume(c, ']')) return true;
            for (;;) {
                json::Value child;
                if (!ParseValue(c, child)) return false;
                out.arr.push_back(std::move(child));
                if (Consume(c, ']')) return true;
                if (!Consume(c, ',')) return false;
            }
        }
        case '"': {
            out.type = json::Type::String;
            return ParseString(c, out.str);
        }
        case 't':
        case 'f':
        case 'n':
            return ParseLiteral(c, out);
        default: {
            std::string tok;
            if (!ParseNumberToken(c, tok)) return false;
            out.type = json::Type::Number;
            out.num = std::move(tok);
            return true;
        }
    }
}

}  // namespace

export namespace json {

bool Parse(const char* data, std::size_t len, Value& out) {
    Cursor c{data, data + len};
    Value root;
    if (!ParseValue(c, root))
        return false;
    SkipWs(c);
    if (c.p != c.end)
        return false;
    out = std::move(root);
    return true;
}

const Value* Find(const Value& v, const char* key) {
    return v.Find(key);
}

long GetLong(const Value& v, const char* key, long dflt) {
    const Value* child = v.Find(key);
    if (!child || child->type != Type::Number)
        return dflt;
    return std::strtol(child->num.c_str(), nullptr, 10);
}

double GetDouble(const Value& v, const char* key, double dflt) {
    const Value* child = v.Find(key);
    if (!child || child->type != Type::Number)
        return dflt;
    return std::strtod(child->num.c_str(), nullptr);
}

bool GetBool(const Value& v, const char* key, bool dflt) {
    const Value* child = v.Find(key);
    if (!child || child->type != Type::Bool)
        return dflt;
    return child->b;
}

}  // namespace json
