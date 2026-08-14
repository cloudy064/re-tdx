#pragma once

#include "tdx/json.hpp"

#include <cmath>
#include <cstddef>
#include <ctime>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::recon_contract_detail {

struct ApiContractSpec {
    const char* id;
    const char* title;
    const char* path;
    bool full_only;
};

struct ApiContractPostRequest {
    std::string action;
    std::string body;
};

const std::vector<ApiContractSpec>& api_contract_specs();
const ApiContractPostRequest* api_contract_post_request(std::string_view id);

inline std::string current_local_date() {
    const auto stamp = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &stamp);
#else
    localtime_r(&stamp, &local);
#endif
    char buffer[16]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &local);
    return buffer;
}

inline const Json* member(const Json& value, std::string_view key) {
    if (!value.is_object()) return nullptr;
    const auto found = value.as_object().find(key);
    return found == value.as_object().end() ? nullptr : &found->second;
}

inline const Json* member_path(
    const Json& value, std::initializer_list<std::string_view> path) {
    const Json* current = &value;
    for (const auto key : path) {
        current = member(*current, key);
        if (!current) return nullptr;
    }
    return current;
}

inline Json value_or_null(const Json* value) {
    return value ? *value : Json(nullptr);
}

inline void add_assertion(Json& result, const std::string& name, bool passed,
                          Json expected, Json actual) {
    Json assertion = Json::object();
    assertion["name"] = name;
    assertion["passed"] = passed;
    assertion["expected"] = std::move(expected);
    assertion["actual"] = std::move(actual);
    result["assertions"].push_back(std::move(assertion));
    if (!passed) result["passed"] = false;
}

inline bool string_is(const Json* value, std::string_view expected) {
    return value && value->is_string() && value->as_string() == expected;
}

inline bool nonempty_string(const Json* value) {
    return value && value->is_string() && !value->as_string().empty();
}

inline bool bool_is(const Json* value, bool expected) {
    return value && value->is_bool() && value->as_bool() == expected;
}

inline bool number_is(const Json* value, double expected) {
    return value && value->is_number() && value->as_number() == expected;
}

inline bool array_empty(const Json* value) {
    return value && value->is_array() && value->size() == 0;
}

inline bool object_has(const Json* value, std::string_view key) {
    return value && value->is_object() && value->as_object().count(key) != 0;
}

inline bool feature_exists(const Json& document, std::string_view command) {
    const auto* features = member(document, "features");
    if (!features || !features->is_array()) return false;
    for (const auto& feature : features->as_array())
        if (string_is(member(feature, "command"), command)) return true;
    return false;
}

inline bool feature_api_is(const Json& document, std::string_view command,
                           std::string_view endpoint) {
    const auto* features = member(document, "features");
    if (!features || !features->is_array()) return false;
    for (const auto& feature : features->as_array())
        if (string_is(member(feature, "command"), command))
            return string_is(member(feature, "api_endpoint"), endpoint);
    return false;
}

inline bool source_exists(const Json& document, std::string_view resource) {
    const auto* sources = member(document, "sources");
    if (!sources || !sources->is_array()) return false;
    for (const auto& source : sources->as_array())
        if (string_is(member(source, "resource"), resource)) return true;
    return false;
}

inline bool source_prefix_exists(const Json& document, std::string_view prefix) {
    const auto* sources = member(document, "sources");
    if (!sources || !sources->is_array()) return false;
    for (const auto& source : sources->as_array()) {
        const auto* resource = member(source, "resource");
        if (resource && resource->is_string() &&
            resource->as_string().rfind(prefix, 0) == 0)
            return true;
    }
    return false;
}

inline std::optional<double> numeric_value(const Json* value) {
    if (!value) return std::nullopt;
    if (value->is_number()) return value->as_number();
    if (!value->is_string()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(value->as_string(), &used);
        return used == value->as_string().size() ? std::optional<double>(parsed)
                                                 : std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

inline bool formula_adjustment_cache_is(const Json& document,
                                        std::size_t expected_inputs) {
    const auto* summary = member(document, "adjustment_summary");
    const auto* counts = summary ? member(*summary, "input_cache_counts") : nullptr;
    const auto* shared = summary ? member(*summary, "shared_cache") : nullptr;
    if (!counts || !counts->is_object() || !shared || !shared->is_object() ||
        !string_is(member(*shared, "schema"),
                   "tdx-kline-adjustment-input-cache-v1") ||
        !number_is(member(*shared, "maximum_entries"), 512.0))
        return false;
    double total = 0.0;
    for (const auto& [name, value] : counts->as_object()) {
        (void)name;
        if (!value.is_number() || value.as_number() < 0.0) return false;
        total += value.as_number();
    }
    const auto entries = numeric_value(member(*shared, "entry_count"));
    return total == static_cast<double>(expected_inputs) && entries &&
           *entries >= 0.0 && *entries <= 512.0;
}

bool validate_formula_contract(const std::string& contract_id,
                               const Json& document, Json& result);
bool validate_market_contract(const std::string& contract_id,
                              const Json& document, const Json& context,
                              Json& result);
bool validate_market_data_contract(const std::string& contract_id,
                                   const Json& document, const Json& context,
                                   Json& result);
bool validate_market_research_contract(const std::string& contract_id,
                                       const Json& document, const Json& context,
                                       Json& result);
bool validate_market_corporate_contract(const std::string& contract_id,
                                        const Json& document, const Json& context,
                                        Json& result);

}  // namespace tdx::recon_contract_detail
