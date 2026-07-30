// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — minimal, defensive JSON (dependency-free).
// Supports null/bool/number/string/array/object with bounded parse depth
// (security: malformed-profile recursion protection).
#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <cstdint>

namespace prismatic {

struct JsonValue;
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::map<std::string, JsonValue>;

enum class JsonType { Null, Bool, Number, String, Array, Object };

struct JsonValue {
    JsonType type = JsonType::Null;
    bool b = false;
    double num = 0.0;
    std::string str;
    std::shared_ptr<JsonArray> arr;
    std::shared_ptr<JsonObject> obj;

    JsonValue() = default;
    JsonValue(bool v) : type(JsonType::Bool), b(v) {}
    JsonValue(double v) : type(JsonType::Number), num(v) {}
    JsonValue(int v) : type(JsonType::Number), num((double)v) {}
    JsonValue(const char* v) : type(JsonType::String), str(v) {}
    JsonValue(const std::string& v) : type(JsonType::String), str(v) {}

    static JsonValue makeArray() { JsonValue v; v.type = JsonType::Array; v.arr = std::make_shared<JsonArray>(); return v; }
    static JsonValue makeObject() { JsonValue v; v.type = JsonType::Object; v.obj = std::make_shared<JsonObject>(); return v; }

    bool isObject() const { return type == JsonType::Object; }
    bool isArray() const { return type == JsonType::Array; }
    bool isNumber() const { return type == JsonType::Number; }
    bool isString() const { return type == JsonType::String; }
    bool isBool() const { return type == JsonType::Bool; }
    bool isNull() const { return type == JsonType::Null; }

    // Safe accessors with defaults.
    bool has(const std::string& k) const { return isObject() && obj->count(k) > 0; }
    const JsonValue& get(const std::string& k) const {
        static const JsonValue kNull;
        if (!isObject()) return kNull;
        auto it = obj->find(k);
        return it == obj->end() ? kNull : it->second;
    }
    double asNumber(double def = 0.0) const { return isNumber() ? num : def; }
    int asInt(int def = 0) const { return isNumber() ? (int)std::lround(num) : def; }
    bool asBool(bool def = false) const { return isBool() ? b : def; }
    std::string asString(const std::string& def = "") const { return isString() ? str : def; }

    void set(const std::string& k, JsonValue v) {
        if (!isObject()) { type = JsonType::Object; obj = std::make_shared<JsonObject>(); }
        (*obj)[k] = std::move(v);
    }
    void push(JsonValue v) {
        if (!isArray()) { type = JsonType::Array; arr = std::make_shared<JsonArray>(); }
        arr->push_back(std::move(v));
    }

    std::string dump(int indent = 2) const {
        std::ostringstream os;
        dumpTo(os, indent, 0);
        return os.str();
    }

private:
    static void escape(std::ostringstream& os, const std::string& s) {
        os << '"';
        for (char c : s) {
            switch (c) {
                case '"': os << "\\\""; break;
                case '\\': os << "\\\\"; break;
                case '\n': os << "\\n"; break;
                case '\r': os << "\\r"; break;
                case '\t': os << "\\t"; break;
                default:
                    if ((unsigned char)c < 0x20) { char buf[8]; std::snprintf(buf, sizeof(buf), "\\u%04x", c); os << buf; }
                    else os << c;
            }
        }
        os << '"';
    }
    void dumpTo(std::ostringstream& os, int indent, int depth) const {
        std::string pad(indent > 0 ? (size_t)(depth + 1) * indent : 0, ' ');
        std::string padEnd(indent > 0 ? (size_t)depth * indent : 0, ' ');
        const char* nl = indent > 0 ? "\n" : "";
        switch (type) {
            case JsonType::Null: os << "null"; break;
            case JsonType::Bool: os << (b ? "true" : "false"); break;
            case JsonType::Number: {
                if (std::floor(num) == num && std::abs(num) < 1e15) os << (long long)num;
                else { std::ostringstream t; t.precision(9); t << num; os << t.str(); }
                break;
            }
            case JsonType::String: escape(os, str); break;
            case JsonType::Array: {
                if (arr->empty()) { os << "[]"; break; }
                os << "[" << nl;
                for (size_t i = 0; i < arr->size(); ++i) {
                    os << pad; (*arr)[i].dumpTo(os, indent, depth + 1);
                    if (i + 1 < arr->size()) os << ",";
                    os << nl;
                }
                os << padEnd << "]";
                break;
            }
            case JsonType::Object: {
                if (obj->empty()) { os << "{}"; break; }
                os << "{" << nl;
                size_t i = 0;
                for (auto& kv : *obj) {
                    os << pad; escape(os, kv.first); os << (indent > 0 ? ": " : ":");
                    kv.second.dumpTo(os, indent, depth + 1);
                    if (++i < obj->size()) os << ",";
                    os << nl;
                }
                os << padEnd << "}";
                break;
            }
        }
    }
};

class JsonParseError : public std::runtime_error {
public:
    explicit JsonParseError(const std::string& m) : std::runtime_error(m) {}
};

class JsonParser {
public:
    static JsonValue parse(const std::string& text) {
        JsonParser p(text);
        p.skipWs();
        JsonValue v = p.parseValue(0);
        p.skipWs();
        if (p.pos_ != p.text_.size())
            throw JsonParseError("trailing characters after JSON value");
        return v;
    }

private:
    explicit JsonParser(const std::string& t) : text_(t) {}
    const std::string& text_;
    size_t pos_ = 0;
    static constexpr int kMaxDepth = 64;  // recursion bound

    char peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }
    char next() { return pos_ < text_.size() ? text_[pos_++] : '\0'; }
    void skipWs() { while (pos_ < text_.size()) { char c = text_[pos_]; if (c==' '||c=='\t'||c=='\n'||c=='\r') pos_++; else break; } }

    JsonValue parseValue(int depth) {
        if (depth > kMaxDepth) throw JsonParseError("max nesting depth exceeded");
        skipWs();
        char c = peek();
        switch (c) {
            case '{': return parseObject(depth);
            case '[': return parseArray(depth);
            case '"': { JsonValue v; v.type = JsonType::String; v.str = parseString(); return v; }
            case 't': case 'f': return parseBool();
            case 'n': expect("null"); return JsonValue();
            default:
                if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
                throw JsonParseError(std::string("unexpected character '") + c + "'");
        }
    }
    void expect(const char* lit) {
        for (const char* p = lit; *p; ++p)
            if (next() != *p) throw JsonParseError(std::string("expected literal ") + lit);
    }
    JsonValue parseBool() {
        if (peek() == 't') { expect("true"); return JsonValue(true); }
        expect("false"); return JsonValue(false);
    }
    JsonValue parseNumber() {
        size_t start = pos_;
        if (peek() == '-') next();
        while (isdigitc(peek())) next();
        if (peek() == '.') { next(); while (isdigitc(peek())) next(); }
        if (peek() == 'e' || peek() == 'E') { next(); if (peek()=='+'||peek()=='-') next(); while (isdigitc(peek())) next(); }
        JsonValue v; v.type = JsonType::Number;
        v.num = std::stod(text_.substr(start, pos_ - start));
        return v;
    }
    static bool isdigitc(char c) { return c >= '0' && c <= '9'; }
    std::string parseString() {
        if (next() != '"') throw JsonParseError("expected string");
        std::string out;
        while (true) {
            char c = next();
            if (c == '\0') throw JsonParseError("unterminated string");
            if (c == '"') break;
            if (c == '\\') {
                char e = next();
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'n': out.push_back('\n'); break;
                    case 't': out.push_back('\t'); break;
                    case 'r': out.push_back('\r'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'u': {
                        unsigned code = parseHex4();
                        // Basic BMP handling; encode as UTF-8.
                        if (code < 0x80) out.push_back((char)code);
                        else if (code < 0x800) { out.push_back((char)(0xC0 | (code >> 6))); out.push_back((char)(0x80 | (code & 0x3F))); }
                        else { out.push_back((char)(0xE0 | (code >> 12))); out.push_back((char)(0x80 | ((code >> 6) & 0x3F))); out.push_back((char)(0x80 | (code & 0x3F))); }
                        break;
                    }
                    default: throw JsonParseError("invalid escape");
                }
            } else {
                out.push_back(c);
            }
        }
        return out;
    }
    unsigned parseHex4() {
        unsigned v = 0;
        for (int i = 0; i < 4; ++i) {
            char c = next(); v <<= 4;
            if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
            else throw JsonParseError("invalid \\u escape");
        }
        return v;
    }
    JsonValue parseArray(int depth) {
        JsonValue v = JsonValue::makeArray();
        next();  // [
        skipWs();
        if (peek() == ']') { next(); return v; }
        while (true) {
            v.arr->push_back(parseValue(depth + 1));
            skipWs();
            char c = next();
            if (c == ']') break;
            if (c != ',') throw JsonParseError("expected ',' or ']' in array");
        }
        return v;
    }
    JsonValue parseObject(int depth) {
        JsonValue v = JsonValue::makeObject();
        next();  // {
        skipWs();
        if (peek() == '}') { next(); return v; }
        while (true) {
            skipWs();
            std::string key = parseString();
            skipWs();
            if (next() != ':') throw JsonParseError("expected ':' in object");
            (*v.obj)[key] = parseValue(depth + 1);
            skipWs();
            char c = next();
            if (c == '}') break;
            if (c != ',') throw JsonParseError("expected ',' or '}' in object");
        }
        return v;
    }
};

}  // namespace prismatic
