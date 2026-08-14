#include "capital_strength_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::detail::capital_strength {

const Json* value_ptr(const Json& value, std::string_view key) {
    if (!value.is_object()) return nullptr;
    const auto found = value.as_object().find(key);
    return found == value.as_object().end() ? nullptr : &found->second;
}

std::string text_value(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found ? trim(jsn_scalar_text(*found)) : std::string{};
}

std::optional<double> number_value(const Json& value, std::string_view key) {
    const auto text = text_value(value, key);
    if (text.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto result = std::stod(text, &used);
        return used == text.size() && std::isfinite(result)
            ? std::optional<double>(result) : std::nullopt;
    } catch (...) { return std::nullopt; }
}

Json number_json(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

std::optional<double> json_number(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found && found->is_number()
        ? std::optional<double>(found->as_number()) : std::nullopt;
}

std::string json_text(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found && found->is_string() ? found->as_string() : std::string{};
}

bool digits(const std::string& value, std::size_t size) {
    return value.size() == size &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
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

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

}  // namespace tdx::detail::capital_strength
