#include "institution_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <utility>

namespace fs = std::filesystem;

namespace tdx::institution_detail {

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::string now_text() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &now)) throw Error("cannot read local time");
#else
    if (!localtime_r(&now, &local)) throw Error("cannot read local time");
#endif
    std::ostringstream output;
    output << local_timestamp_text(local);
    return output.str();
}

const Json* value_ptr(const Json& object, std::string_view name) {
    if (!object.is_object()) throw Error("institution row must be an object");
    const auto found = object.as_object().find(name);
    return found == object.as_object().end() ? nullptr : &found->second;
}

Json copy_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    return value ? *value : Json(nullptr);
}

std::string text_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

std::string scalar_text(const Json& value) {
    if (value.is_null()) return {};
    if (value.is_string()) return trim(value.as_string());
    if (value.is_number()) {
        std::ostringstream output;
        output << std::setprecision(15) << value.as_number();
        return output.str();
    }
    if (value.is_bool()) return value.as_bool() ? "true" : "false";
    return value.dump(-1);
}

std::string percent_decode(std::string_view value) {
    auto digit = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    };
    std::string result;
    result.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '+') result.push_back(' ');
        else if (value[index] == '%' && index + 2 < value.size()) {
            const int high = digit(value[index + 1]);
            const int low = digit(value[index + 2]);
            if (high < 0 || low < 0) throw Error("invalid holder URL percent encoding");
            result.push_back(static_cast<char>((high << 4) | low));
            index += 2;
        } else result.push_back(value[index]);
    }
    return result;
}

int market_id(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized == "0" || normalized == "sz") return 0;
    if (normalized == "1" || normalized == "sh") return 1;
    if (normalized == "2" || normalized == "bj") return 2;
    throw Error("market must be sz/sh/bj or 0/1/2");
}

std::string market_name(int value) {
    return value == 0 ? "sz" : value == 1 ? "sh" : value == 2 ? "bj"
                                                               : "m" + std::to_string(value);
}

std::string market_prefix(int value) {
    return value == 0 ? "SZ" : value == 1 ? "SH" : value == 2 ? "BJ"
                                                               : "M" + std::to_string(value);
}

bool six_digits(const std::string& value) {
    return value.size() == 6 && std::all_of(value.begin(), value.end(), [](char ch) {
        return ch >= '0' && ch <= '9';
    });
}

void validate_token(const std::string& value, const std::string& name, bool optional) {
    if (value.empty()) {
        if (optional) return;
        throw Error(name + " is required");
    }
    if (value.size() > 64 || !std::all_of(value.begin(), value.end(), [](char ch) {
            const auto byte = static_cast<unsigned char>(ch);
            return std::isalnum(byte) || ch == '_' || ch == '-' || ch == '.' || ch == ':';
        }))
        throw Error(name + " contains unsupported characters");
}

int bounded_integer(const std::string& text, const std::string& name,
                    int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int result = std::stoi(text, &used);
        if (used != text.size() || result < minimum || result > maximum)
            throw std::invalid_argument("range");
        return result;
    } catch (...) {
        throw Error(name + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

Json security_document(const std::string& market, const std::string& code,
                       const std::map<std::pair<int, std::string>, Security>& securities) {
    const int id = market_id(market);
    const auto found = securities.find({id, code});
    Json result = Json::object();
    result["market"] = market_name(id);
    result["market_id"] = id;
    result["code"] = code;
    result["security_id"] = market_prefix(id) + code;
    result["name"] = found == securities.end() ? "" : found->second.name;
    result["name_resolved"] = found != securities.end();
    return result;
}

Json tqlex_request(const std::string& mode, const std::string& code,
                   const std::string& holder_id, const std::string& variant_id) {
    Json params = Json::array();
    params.push_back(mode);
    params.push_back(code);
    params.push_back(holder_id);
    params.push_back(variant_id);
    params.push_back("1");
    Json request = Json::object();
    request["Params"] = std::move(params);
    return request;
}

std::string change_label(const std::string& value) {
    if (value == "1") return "新进";
    if (value == "2") return "不变";
    if (value == "3") return "增持";
    if (value == "4") return "减持";
    return value;
}

}  // namespace tdx::institution_detail
