#include "tdx/announcement_signals_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace tdx {
namespace detail {
namespace announcement_signals {

const Json* value_ptr(const Json& value, std::string_view key) {
    if (!value.is_object()) return nullptr;
    const auto found = value.as_object().find(key);
    return found == value.as_object().end() ? nullptr : &found->second;
}

std::string text_value(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found ? trim(jsn_scalar_text(*found)) : std::string{};
}

std::string text_value_ci(const Json& value, std::string_view key) {
    if (!value.is_object()) return {};
    const auto expected = lower_ascii(std::string(key));
    for (const auto& [name, child] : value.as_object())
        if (lower_ascii(name) == expected) return trim(jsn_scalar_text(child));
    return {};
}

std::optional<double> number_value(const Json& value, std::string_view key) {
    const auto text = text_value_ci(value, key);
    if (text.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto number = std::stod(text, &used);
        return used == text.size() && std::isfinite(number)
            ? std::optional<double>(number) : std::nullopt;
    } catch (...) { return std::nullopt; }
}

Json number_json(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

bool digits(const std::string& value, std::size_t size) {
    return value.size() == size &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

std::string iso_date(const std::string& value) {
    if (!digits(value, 8)) return value;
    return value.substr(0, 4) + "-" + value.substr(4, 2) + "-" + value.substr(6, 2);
}

std::string compact_date(std::string value, const std::string& name) {
    value = trim(std::move(value));
    value.erase(std::remove(value.begin(), value.end(), '-'), value.end());
    if (!value.empty() && !digits(value, 8))
        throw Error(name + " must be YYYYMMDD or YYYY-MM-DD");
    return value;
}

int market_id(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "0" || value == "sz") return 0;
    if (value == "1" || value == "sh") return 1;
    if (value == "2" || value == "44" || value == "bj") return 2;
    throw Error("market must be sz/sh/bj or 0/1/2");
}

std::string market_name(int id) {
    return id == 0 ? "sz" : id == 1 ? "sh" : "bj";
}

std::string market_prefix(int id) {
    return id == 0 ? "SZ" : id == 1 ? "SH" : "BJ";
}

Json security_document(
    int id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities) {
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

std::pair<std::string, std::string> split_title_url(const std::string& value) {
    const auto marker = value.rfind("TXT:http");
    if (marker == std::string::npos) return {trim(value), {}};
    return {trim(value.substr(0, marker)), trim(value.substr(marker + 4))};
}

std::string normalized_direction(const std::string& raw) {
    if (raw == "利好") return "bullish";
    if (raw == "利空") return "bearish";
    return "unknown";
}

std::string history_resource(int id, const std::string& code) {
    return "ggjx/" + std::to_string(id) + code + ".jsn";
}

std::string json_text(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found && found->is_string() ? found->as_string() : std::string{};
}

std::optional<double> json_number(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found && found->is_number()
        ? std::optional<double>(found->as_number()) : std::nullopt;
}

bool json_contains(const Json& value, const std::string& needle) {
    if (value.is_string())
        return lower_ascii(value.as_string()).find(needle) != std::string::npos;
    if (value.is_number() || value.is_bool())
        return lower_ascii(jsn_scalar_text(value)).find(needle) != std::string::npos;
    if (value.is_array()) {
        for (const auto& child : value.as_array())
            if (json_contains(child, needle)) return true;
    } else if (value.is_object()) {
        for (const auto& [key, child] : value.as_object())
            if (lower_ascii(key).find(needle) != std::string::npos ||
                json_contains(child, needle)) return true;
    }
    return false;
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

int bounded(const std::string& text, const std::string& name,
            int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const auto value = std::stoi(text, &used);
        if (used != text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(name + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

}  // namespace announcement_signals
}  // namespace detail
}  // namespace tdx
