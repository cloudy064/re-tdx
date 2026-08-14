#include "tdx/theme_library_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::theme_library_detail {

const Json* value_ptr(const Json& value, std::string_view key) {
    if (!value.is_object()) return nullptr;
    auto found = value.as_object().find(key);
    if (found != value.as_object().end()) return &found->second;
    const auto wanted = lower_ascii(std::string(key));
    for (const auto& [name, child] : value.as_object())
        if (lower_ascii(name) == wanted) return &child;
    return nullptr;
}

std::string text_value(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found ? trim(jsn_scalar_text(*found)) : std::string{};
}

std::optional<double> number_value(const Json& value, std::string_view key) {
    const auto raw = text_value(value, key);
    if (raw.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(raw, &used);
        if (used == raw.size() && std::isfinite(parsed)) return parsed;
    } catch (...) {}
    return std::nullopt;
}

std::optional<std::uint64_t> unsigned_value(const Json& value,
                                            std::string_view key) {
    const auto raw = text_value(value, key);
    if (raw.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto parsed = std::stoull(raw, &used);
        if (used == raw.size()) return parsed;
    } catch (...) {}
    return std::nullopt;
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

std::string market_name(int id) { return id == 0 ? "sz" : id == 1 ? "sh" : "bj"; }
std::string market_prefix(int id) { return id == 0 ? "SZ" : id == 1 ? "SH" : "BJ"; }

Json security_document(
    int market, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    const auto found = securities.find({market, code});
    Json result = Json::object();
    result["market"] = market_name(market);
    result["market_id"] = market;
    result["code"] = code;
    result["security_id"] = market_prefix(market) + code;
    result["name"] = found == securities.end() ? "" : found->second.name;
    result["name_resolved"] = found != securities.end();
    return result;
}

std::vector<std::pair<int, std::string>> parse_members(const std::string& value,
                                                       std::size_t* raw_count) {
    std::vector<std::pair<int, std::string>> result;
    std::set<std::pair<int, std::string>> seen;
    std::size_t count = 0;
    for (auto token : split(value, ',')) {
        token = trim(std::move(token));
        if (token.empty()) continue;
        const auto separator = token.find('|');
        if (separator == std::string::npos)
            throw Error("theme-library member token is missing '|': " + token);
        const auto market = market_id(token.substr(0, separator));
        const auto code = trim(token.substr(separator + 1));
        if (!digits(code, 6)) throw Error("theme-library member code is invalid: " + code);
        ++count;
        if (seen.insert({market, code}).second) result.push_back({market, code});
    }
    if (raw_count) *raw_count = count;
    return result;
}

const Json& document_for_resource(const Json& documents,
                                  const std::string& resource) {
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("theme-library source is missing: " + resource);
}

bool json_contains(const Json& value, const std::string& needle) {
    if (value.is_string())
        return lower_ascii(value.as_string()).find(needle) != std::string::npos;
    if (value.is_number() || value.is_bool())
        return lower_ascii(jsn_scalar_text(value)).find(needle) != std::string::npos;
    if (value.is_array())
        for (const auto& child : value.as_array())
            if (json_contains(child, needle)) return true;
    if (value.is_object())
        for (const auto& [key, child] : value.as_object())
            if (lower_ascii(key).find(needle) != std::string::npos ||
                json_contains(child, needle)) return true;
    return false;
}

Json theme_summary(Json theme) {
    theme.as_object().erase("members");
    theme.as_object().erase("raw");
    return theme;
}

void sort_themes(Json& rows, const std::string& sort_value,
                 const std::string& order_value) {
    const auto sort = lower_ascii(trim(sort_value));
    const auto order = lower_ascii(trim(order_value));
    if (!std::set<std::string>{"name", "members", "id", "created"}.count(sort))
        throw Error("theme-library sort must be name, members, id, or created");
    if (order != "asc" && order != "desc") throw Error("order must be asc or desc");
    const bool desc = order == "desc";
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            if (sort == "members") {
                const auto a = left.at("member_count").as_number();
                const auto b = right.at("member_count").as_number();
                if (a != b) return desc ? a > b : a < b;
            } else {
                const char* key = sort == "name" ? "name" :
                                  sort == "created" ? "created_date" : "theme_id";
                const auto& a = left.at(key).as_string();
                const auto& b = right.at(key).as_string();
                if (a != b) return desc ? a > b : a < b;
            }
            return left.at("record_id").as_string() < right.at("record_id").as_string();
        });
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

int bounded(const std::string& text, const std::string& name, int low, int high) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used != text.size() || value < low || value > high)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(name + " must be in " + std::to_string(low) + ".." +
                    std::to_string(high));
    }
}

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

}  // namespace tdx::theme_library_detail
