#pragma once
// Tiny JSON reader (no external dependencies). Throws on malformed input.

#include <cctype>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace Json {

struct JValue;
using JObject = std::map<std::string, JValue>;
using JArray  = std::vector<JValue>;

struct JValue {
    std::variant<std::nullptr_t, bool, double, std::string,
                 std::shared_ptr<JArray>, std::shared_ptr<JObject>> v{ nullptr };

    const JObject* obj() const { auto p = std::get_if<std::shared_ptr<JObject>>(&v); return p ? p->get() : nullptr; }
    const JArray*  arr() const { auto p = std::get_if<std::shared_ptr<JArray>>(&v);  return p ? p->get() : nullptr; }
    std::string    str() const { auto p = std::get_if<std::string>(&v); return p ? *p : std::string(); }
    double         num() const { auto p = std::get_if<double>(&v); return p ? *p : 0.0; }
    bool           boolean() const { auto p = std::get_if<bool>(&v); return p && *p; }
    const JValue*  get(const char* key) const {
        const JObject* o = obj();
        if (!o) return nullptr;
        auto it = o->find(key);
        return it == o->end() ? nullptr : &it->second;
    }
};

class JParser {
    const std::string& s;
    size_t i = 0;
    int depth = 0;

    void ws() { while (i < s.size() && std::isspace((unsigned char)s[i])) ++i; }
    bool eat(char c) { ws(); if (i < s.size() && s[i] == c) { ++i; return true; } return false; }

    static void appendUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
        else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    }
    unsigned hex4() {
        if (i + 4 > s.size()) throw 0;
        unsigned v = std::stoul(s.substr(i, 4), nullptr, 16);
        i += 4;
        return v;
    }
    std::string str() {
        if (!eat('"')) throw 0;
        std::string out;
        while (i < s.size() && s[i] != '"') {
            char c = s[i++];
            if (c != '\\') { out += c; continue; }
            if (i >= s.size()) throw 0;
            char e = s[i++];
            switch (e) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'u': {
                unsigned cp = hex4();
                if (cp >= 0xD800 && cp < 0xDC00 && i + 6 <= s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                    i += 2;
                    unsigned lo = hex4();
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                }
                appendUtf8(out, cp);
                break;
            }
            default: out += e;
            }
        }
        if (i >= s.size()) throw 0;
        ++i;
        return out;
    }
public:
    explicit JParser(const std::string& src) : s(src) {}

    JValue value() {
        if (++depth > 64) throw 0;
        ws();
        if (i >= s.size()) throw 0;
        JValue r;
        char c = s[i];
        if (c == '{') {
            ++i;
            auto o = std::make_shared<JObject>();
            if (!eat('}')) {
                do {
                    ws();
                    std::string k = str();
                    if (!eat(':')) throw 0;
                    (*o)[k] = value();
                } while (eat(','));
                if (!eat('}')) throw 0;
            }
            r.v = o;
        } else if (c == '[') {
            ++i;
            auto a = std::make_shared<JArray>();
            if (!eat(']')) {
                do { a->push_back(value()); } while (eat(','));
                if (!eat(']')) throw 0;
            }
            r.v = a;
        } else if (c == '"') {
            r.v = str();
        } else if (s.compare(i, 4, "true") == 0)  { i += 4; r.v = true; }
        else if (s.compare(i, 5, "false") == 0)   { i += 5; r.v = false; }
        else if (s.compare(i, 4, "null") == 0)    { i += 4; r.v = nullptr; }
        else {
            size_t start = i;
            while (i < s.size() && (std::isdigit((unsigned char)s[i]) || strchr("+-.eE", s[i]))) ++i;
            if (start == i) throw 0;
            r.v = std::stod(s.substr(start, i - start));
        }
        --depth;
        return r;
    }
};

} // namespace Json
