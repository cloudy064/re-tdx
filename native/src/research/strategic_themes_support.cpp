#include "strategic_themes_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::detail::strategic_themes {

const Json* value_ptr(const Json& value, std::string_view key) {
    if (!value.is_object()) return nullptr;
    const auto exact = value.as_object().find(key);
    if (exact != value.as_object().end()) return &exact->second;
    const auto wanted = lower_ascii(std::string(key));
    for (const auto& [name, child] : value.as_object())
        if (lower_ascii(name) == wanted) return &child;
    return nullptr;
}

std::string text_value(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found ? trim(jsn_scalar_text(*found)) : std::string{};
}

std::optional<std::uint64_t> unsigned_value(
    const Json& value, std::string_view key) {
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

std::vector<std::pair<int, std::string>> parse_members(
    const std::string& value, std::size_t* raw_count) {
    std::vector<std::pair<int, std::string>> result;
    std::set<std::pair<int, std::string>> seen;
    std::size_t count = 0;
    std::size_t offset = 0;
    while (offset <= value.size()) {
        const auto end = value.find(',', offset);
        const auto token = trim(value.substr(offset,
            end == std::string::npos ? std::string::npos : end - offset));
        if (!token.empty()) {
            const auto separator = token.find('|');
            if (separator == std::string::npos)
                throw Error("strategic theme member token is missing '|': " + token);
            const auto market = market_id(token.substr(0, separator));
            const auto code = trim(token.substr(separator + 1));
            if (!digits(code, 6))
                throw Error("strategic theme member code is invalid: " + token);
            ++count;
            if (seen.insert({market, code}).second) result.push_back({market, code});
        }
        if (end == std::string::npos) break;
        offset = end + 1;
    }
    if (raw_count) *raw_count = count;
    return result;
}

std::vector<std::string> member_ids(const Json& theme) {
    std::vector<std::string> result;
    for (const auto& member : theme.at("members").as_array())
        result.push_back(member.at("security_id").as_string());
    return result;
}

const Json& document_for_resource(
    const Json& documents, const std::string& resource) {
    if (!documents.is_array()) throw Error("strategic theme sources must be an array");
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("strategic theme source is missing: " + resource);
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

Json theme_summary(const Json& theme) {
    Json result = theme;
    result.as_object().erase("members");
    result.as_object().erase("raw");
    return result;
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

}  // namespace tdx::detail::strategic_themes
