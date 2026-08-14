#include "tdx/json.hpp"
#include "tdx/common.hpp"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace tdx {
namespace {

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    Json parse() {
        skip_space();
        Json result = value();
        skip_space();
        if (position_ != text_.size()) fail("trailing JSON data");
        return result;
    }

private:
    [[noreturn]] void fail(const std::string& message) const {
        throw Error(message + " at JSON byte " + std::to_string(position_));
    }
    void skip_space() {
        while (position_ < text_.size() &&
               (text_[position_] == ' ' || text_[position_] == '\t' ||
                text_[position_] == '\r' || text_[position_] == '\n')) ++position_;
    }
    char take() {
        if (position_ >= text_.size()) fail("unexpected end");
        return text_[position_++];
    }
    bool consume(char expected) {
        if (position_ < text_.size() && text_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }
    Json value() {
        skip_space();
        if (position_ >= text_.size()) fail("expected value");
        switch (text_[position_]) {
        case 'n': literal("null"); return nullptr;
        case 't': literal("true"); return true;
        case 'f': literal("false"); return false;
        case '"': return string();
        case '[': return array();
        case '{': return object();
        default: return number();
        }
    }
    void literal(std::string_view expected) {
        if (text_.substr(position_, expected.size()) != expected) fail("invalid literal");
        position_ += expected.size();
    }
    static void append_utf8(std::string& result, unsigned codepoint) {
        if (codepoint <= 0x7f) result.push_back(static_cast<char>(codepoint));
        else if (codepoint <= 0x7ff) {
            result.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else if (codepoint <= 0xffff) {
            result.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else {
            result.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        }
    }
    unsigned hex4() {
        unsigned result = 0;
        for (int index = 0; index < 4; ++index) {
            const char ch = take();
            result <<= 4;
            if (ch >= '0' && ch <= '9') result |= static_cast<unsigned>(ch - '0');
            else if (ch >= 'a' && ch <= 'f') result |= static_cast<unsigned>(ch - 'a' + 10);
            else if (ch >= 'A' && ch <= 'F') result |= static_cast<unsigned>(ch - 'A' + 10);
            else fail("invalid Unicode escape");
        }
        return result;
    }
    Json string() {
        if (take() != '"') fail("expected string");
        std::string result;
        while (position_ < text_.size()) {
            const char ch = take();
            if (ch == '"') return result;
            if (static_cast<unsigned char>(ch) < 0x20) fail("control byte in string");
            if (ch != '\\') {
                result.push_back(ch);
                continue;
            }
            const char escaped = take();
            switch (escaped) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'u': {
                unsigned codepoint = hex4();
                if (codepoint >= 0xd800 && codepoint <= 0xdbff &&
                    text_.substr(position_, 2) == "\\u") {
                    position_ += 2;
                    const unsigned low = hex4();
                    if (low < 0xdc00 || low > 0xdfff) fail("invalid surrogate pair");
                    codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                }
                append_utf8(result, codepoint);
                break;
            }
            default: fail("invalid string escape");
            }
        }
        fail("unterminated string");
    }
    Json number() {
        const std::size_t begin = position_;
        if (consume('-')) {}
        if (consume('0')) {}
        else {
            if (position_ >= text_.size() || text_[position_] < '1' || text_[position_] > '9')
                fail("invalid number");
            while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9')
                ++position_;
        }
        if (consume('.')) {
            if (position_ >= text_.size() || text_[position_] < '0' || text_[position_] > '9')
                fail("invalid number fraction");
            while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9')
                ++position_;
        }
        if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
            ++position_;
            if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-'))
                ++position_;
            if (position_ >= text_.size() || text_[position_] < '0' || text_[position_] > '9')
                fail("invalid number exponent");
            while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9')
                ++position_;
        }
        const std::string token(text_.substr(begin, position_ - begin));
        char* end = nullptr;
        const double result = std::strtod(token.c_str(), &end);
        if (!end || *end) fail("invalid number");
        return result;
    }
    Json array() {
        take();
        Json result = Json::array();
        skip_space();
        if (consume(']')) return result;
        while (true) {
            result.push_back(value());
            skip_space();
            if (consume(']')) return result;
            if (!consume(',')) fail("expected ',' in array");
        }
    }
    Json object() {
        take();
        Json result = Json::object();
        skip_space();
        if (consume('}')) return result;
        while (true) {
            skip_space();
            if (position_ >= text_.size() || text_[position_] != '"') fail("expected object key");
            const std::string key = string().as_string();
            skip_space();
            if (!consume(':')) fail("expected ':'");
            result[key] = value();
            skip_space();
            if (consume('}')) return result;
            if (!consume(',')) fail("expected ',' in object");
        }
    }

    std::string_view text_;
    std::size_t position_{};
};

void escape_string(std::ostringstream& output, std::string_view value) {
    output << '"';
    for (unsigned char ch : value) {
        switch (ch) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (ch < 0x20) {
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<unsigned>(ch) << std::dec;
            } else output << static_cast<char>(ch);
        }
    }
    output << '"';
}

void dump_value(std::ostringstream& output, const Json& value, int indent, int depth) {
    const auto newline = [&](int extra = 0) {
        if (indent >= 0) output << '\n' << std::string((depth + extra) * indent, ' ');
    };
    if (value.is_null()) output << "null";
    else if (value.is_bool()) output << (value.as_bool() ? "true" : "false");
    else if (value.is_number()) {
        const double number = value.as_number();
        if (!std::isfinite(number)) throw Error("cannot encode non-finite JSON number");
        output << std::setprecision(17) << number;
    } else if (value.is_string()) escape_string(output, value.as_string());
    else if (value.is_array()) {
        output << '[';
        const auto& array = value.as_array();
        for (std::size_t index = 0; index < array.size(); ++index) {
            if (index) output << ',';
            newline(1);
            dump_value(output, array[index], indent, depth + 1);
        }
        if (!array.empty()) newline();
        output << ']';
    } else {
        output << '{';
        const auto& object = value.as_object();
        std::size_t index = 0;
        for (const auto& [key, item] : object) {
            if (index++) output << ',';
            newline(1);
            escape_string(output, key);
            output << (indent >= 0 ? ": " : ":");
            dump_value(output, item, indent, depth + 1);
        }
        if (!object.empty()) newline();
        output << '}';
    }
}

}  // namespace

Json::Json() noexcept : value_(nullptr) {}
Json::Json(std::nullptr_t) noexcept : value_(nullptr) {}
Json::Json(bool value) : value_(value) {}
Json::Json(int value) : value_(static_cast<double>(value)) {}
Json::Json(std::int64_t value) : value_(static_cast<double>(value)) {}
Json::Json(std::uint64_t value) : value_(static_cast<double>(value)) {}
Json::Json(double value) : value_(value) {}
Json::Json(const char* value) : value_(std::string(value ? value : "")) {}
Json::Json(std::string value) : value_(std::move(value)) {}
Json::Json(Array value) : value_(std::move(value)) {}
Json::Json(Object value) : value_(std::move(value)) {}
Json Json::array() { return Array{}; }
Json Json::object() { return Object{}; }
Json Json::parse(std::string_view text) { return Parser(text).parse(); }
bool Json::is_null() const noexcept { return std::holds_alternative<std::nullptr_t>(value_); }
bool Json::is_bool() const noexcept { return std::holds_alternative<bool>(value_); }
bool Json::is_number() const noexcept { return std::holds_alternative<double>(value_); }
bool Json::is_string() const noexcept { return std::holds_alternative<std::string>(value_); }
bool Json::is_array() const noexcept { return std::holds_alternative<Array>(value_); }
bool Json::is_object() const noexcept { return std::holds_alternative<Object>(value_); }
bool Json::as_bool() const { if (!is_bool()) throw Error("JSON value is not bool"); return std::get<bool>(value_); }
double Json::as_number() const { if (!is_number()) throw Error("JSON value is not number"); return std::get<double>(value_); }
const std::string& Json::as_string() const { if (!is_string()) throw Error("JSON value is not string"); return std::get<std::string>(value_); }
const Json::Array& Json::as_array() const { if (!is_array()) throw Error("JSON value is not array"); return std::get<Array>(value_); }
const Json::Object& Json::as_object() const { if (!is_object()) throw Error("JSON value is not object"); return std::get<Object>(value_); }
Json::Array& Json::as_array() { if (!is_array()) throw Error("JSON value is not array"); return std::get<Array>(value_); }
Json::Object& Json::as_object() { if (!is_object()) throw Error("JSON value is not object"); return std::get<Object>(value_); }
Json& Json::operator[](std::string key) { if (!is_object()) value_ = Object{}; return std::get<Object>(value_)[std::move(key)]; }
const Json& Json::at(std::string_view key) const { return as_object().at(std::string(key)); }
void Json::push_back(Json value) { as_array().push_back(std::move(value)); }
std::size_t Json::size() const noexcept { return is_array() ? std::get<Array>(value_).size() : is_object() ? std::get<Object>(value_).size() : 0; }
std::string Json::dump(int indent) const { std::ostringstream output; dump_value(output, *this, indent, 0); return output.str(); }

}  // namespace tdx
