#include "state_owned_reform_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::state_owned_detail {

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

std::optional<double> number_value(const Json& value, std::string_view key) {
    const auto raw = text_value(value, key);
    if (raw.empty() || raw == "-" || raw == "--") return std::nullopt;
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(raw, &used);
        if (used == raw.size() && std::isfinite(parsed)) return parsed;
    } catch (...) {
    }
    return std::nullopt;
}

std::optional<double> json_number(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found && found->is_number()
        ? std::optional<double>(found->as_number()) : std::nullopt;
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
    return value.substr(0, 4) + "-" + value.substr(4, 2) + "-" +
        value.substr(6, 2);
}

int market_id(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "0" || value == "sz") return 0;
    if (value == "1" || value == "sh") return 1;
    if (value == "2" || value == "44" || value == "bj") return 2;
    throw Error("market must be sz/sh/bj or 0/1/2");
}

int inferred_market(const std::string& raw, const std::string& code) {
    try {
        return market_id(raw);
    } catch (...) {
    }
    if (code.rfind("920", 0) == 0 || code[0] == '4' || code[0] == '8') return 2;
    return code[0] == '6' ? 1 : 0;
}

std::string market_name(int id) {
    return id == 0 ? "sz" : id == 1 ? "sh" : "bj";
}

std::string market_prefix(int id) {
    return id == 0 ? "SZ" : id == 1 ? "SH" : "BJ";
}

Json security_document(int id, const std::string& code,
                       const SecurityMap& securities) {
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

const Json* find_quote(const Json& quote_rows, int market,
                       const std::string& code) {
    if (!quote_rows.is_array()) return nullptr;
    for (const auto& quote : quote_rows.as_array()) {
        const auto value = json_number(quote, "market_id");
        if (value && static_cast<int>(*value) == market &&
            text_value(quote, "code") == code)
            return &quote;
    }
    return nullptr;
}

Json quote_metric(const Json* quote, std::string_view key) {
    if (!quote) return Json(nullptr);
    const auto* value = value_ptr(*quote, key);
    return value && value->is_number() ? *value : Json(nullptr);
}

Json return_metric(const Json* quote, const std::optional<double>& reference) {
    const auto last = quote ? json_number(*quote, "last_price") : std::nullopt;
    return last && reference && *reference > 0
        ? Json((*last / *reference - 1.0) * 100.0) : Json(nullptr);
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

const Json& document_for_resource(const Json& documents,
                                  const std::string& resource) {
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("missing state-owned reform resource: " + resource);
}

bool json_contains(const Json& value, const std::string& needle) {
    if (value.is_string())
        return lower_ascii(value.as_string()).find(needle) != std::string::npos;
    if (value.is_number() || value.is_bool())
        return lower_ascii(jsn_scalar_text(value)).find(needle) != std::string::npos;
    if (value.is_array()) {
        for (const auto& child : value.as_array())
            if (json_contains(child, needle)) return true;
    }
    if (value.is_object()) {
        for (const auto& [key, child] : value.as_object())
            if (lower_ascii(key).find(needle) != std::string::npos ||
                json_contains(child, needle)) return true;
    }
    return false;
}

Json group_reference(const Json& group) {
    Json result = Json::object();
    for (const auto* key : {"group_id", "dimension", "dimension_label", "unit_id",
                            "name", "declared_member_count", "member_count",
                            "detail_resource"})
        result[key] = group.at(key);
    return result;
}

Json quote_source_metadata(const Json& document, bool refreshed) {
    Json source = Json::object();
    for (const auto* key : {"command", "endpoint", "server_name", "generated_at",
                            "requested", "received"})
        source[key] = document.at(key);
    source["cache_refreshed"] = refreshed;
    return source;
}

void append_error(Json& errors, const std::string& resource,
                  const std::string& message) {
    Json item = Json::object();
    item["resource"] = resource;
    item["message"] = message;
    errors.push_back(std::move(item));
}

Json page_rows(const Json& rows, int offset, int limit) {
    Json result = Json::array();
    for (std::size_t i = static_cast<std::size_t>(offset);
         i < rows.size() && result.size() < static_cast<std::size_t>(limit); ++i)
        result.push_back(rows.as_array()[i]);
    return result;
}

}  // namespace tdx::state_owned_detail
