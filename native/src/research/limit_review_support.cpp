#include "limit_review_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::detail::limit_review {

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
        const auto parsed = std::stod(text, &used);
        if (used != text.size() || !std::isfinite(parsed)) return std::nullopt;
        return parsed;
    } catch (...) { return std::nullopt; }
}

Json number_or_null(const Json& value, std::string_view key, double scale) {
    const auto number = number_value(value, key);
    return number ? Json(*number * scale) : Json(nullptr);
}

bool digits(const std::string& value, std::size_t size) {
    return value.size() == size &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

int market_id(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized == "0" || normalized == "sz") return 0;
    if (normalized == "1" || normalized == "sh") return 1;
    if (normalized == "2" || normalized == "44" || normalized == "bj") return 2;
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

bool missing_resource_error(const std::string& message) {
    const auto folded = lower_ascii(message);
    return folded.find("zero-length") != std::string::npos ||
           folded.find("not found") != std::string::npos ||
           folded.find("does not exist") != std::string::npos ||
           folded.find("missing jsn") != std::string::npos;
}

Json missing_document(const std::string& resource, const std::string& message) {
    Json result = Json::object();
    result["resource"] = resource;
    result["rows"] = Json::array();
    result["missing"] = true;
    result["attempts"] = 1;
    result["stale"] = false;
    result["age_seconds"] = 0;
    result["size"] = 0;
    result["upstream_error"] = message;
    return result;
}

Json source_summary(const Json& document) {
    const auto* missing = value_ptr(document, "missing");
    if (!missing || !missing->is_bool() || !missing->as_bool())
        return jsn_source_metadata(document);
    Json result = Json::object();
    result["resource"] = text_value(document, "resource");
    result["endpoint"] = Json(nullptr);
    result["attempts"] = 1;
    result["stale"] = false;
    result["age_seconds"] = 0;
    result["row_count"] = 0;
    result["size"] = 0;
    result["missing"] = true;
    result["upstream_error"] = text_value(document, "upstream_error");
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

Json select_records(const Json& candidates, const QueryPlan& plan,
                    std::uint64_t& matched) {
    Json result = Json::array();
    const auto needle = lower_ascii(plan.options.query);
    matched = 0;
    for (const auto& row : candidates.as_array()) {
        const auto* security = value_ptr(row, "security");
        if (plan.selected_market >= 0) {
            if (!security || !security->is_object()) continue;
            const auto row_market = number_value(*security, "market_id");
            if (!row_market || static_cast<int>(*row_market) != plan.selected_market)
                continue;
        }
        if (!plan.options.code.empty() &&
            (!security || text_value(*security, "code") != plan.options.code))
            continue;
        if (!needle.empty() && !json_contains(row, needle)) continue;
        if (matched >= static_cast<std::uint64_t>(plan.options.offset) &&
            result.size() < static_cast<std::size_t>(plan.options.limit))
            result.push_back(row);
        ++matched;
    }
    return result;
}

void append_rows(Json& destination, const Json& source) {
    if (!source.is_array()) throw Error("limit-review normalized rows must be arrays");
    for (const auto& row : source.as_array()) destination.push_back(row);
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
        const int value = std::stoi(text, &used);
        if (used != text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(name + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

fs::path from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

}  // namespace tdx::detail::limit_review
