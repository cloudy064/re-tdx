#include "industry_profile_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::detail::industry_profile {

fs::path native_utf8_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::string current_time_text() {
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

const Json* json_field(const Json& object, std::string_view name) {
    if (!object.is_object()) throw Error("industry profile row must be an object");
    const auto found = object.as_object().find(name);
    return found == object.as_object().end() ? nullptr : &found->second;
}

Json json_copy(const Json& object, std::string_view name) {
    const auto* value = json_field(object, name);
    return value ? *value : Json(nullptr);
}

std::string json_text(const Json& object, std::string_view name) {
    const auto* value = json_field(object, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

std::optional<double> json_number_value(const Json& object, std::string_view name) {
    const auto* value = json_field(object, name);
    if (!value || value->is_null()) return std::nullopt;
    if (value->is_number()) return value->as_number();
    const auto text = trim(jsn_scalar_text(*value));
    if (text.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const double result = std::stod(text, &used);
        if (used != text.size() || !std::isfinite(result)) return std::nullopt;
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

Json numeric_difference(const std::optional<double>& current,
                const std::optional<double>& previous) {
    if (!current || !previous) return Json(nullptr);
    return *current - *previous;
}

Json numeric_percentage_change(const std::optional<double>& current,
                       const std::optional<double>& previous) {
    if (!current || !previous || *previous == 0.0) return Json(nullptr);
    return (*current / *previous - 1.0) * 100.0;
}

Json numeric_percentage_of(const std::optional<double>& numerator,
                   const std::optional<double>& denominator) {
    if (!numerator || !denominator || *denominator == 0.0) return Json(nullptr);
    return *numerator / *denominator * 100.0;
}

bool valid_research_industry_code(const std::string& value) {
    return value.size() == 6 && value.rfind("881", 0) == 0 &&
           std::all_of(value.begin(), value.end(), [](char ch) {
               return ch >= '0' && ch <= '9';
           });
}

bool valid_security_code(const std::string& value) {
    return value.size() == 6 &&
           std::all_of(value.begin(), value.end(), [](char ch) {
               return ch >= '0' && ch <= '9';
           });
}

int canonical_market_id(const std::string& value) {
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

Json make_security_document(
    int market_id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    const auto found = securities.find({market_id, code});
    Json result = Json::object();
    result["market"] = market_name(market_id);
    result["market_id"] = market_id;
    result["code"] = code;
    result["security_id"] = market_prefix(market_id) + code;
    result["name"] = found == securities.end() ? "" : found->second.name;
    result["name_resolved"] = found != securities.end();
    return result;
}

Json make_basic_industry(const Block& block, const std::string& parent_code) {
    Json result = Json::object();
    result["block_id"] = block.block_id;
    result["code"] = block.block_code;
    result["name"] = block.name;
    result["level"] = block.level;
    result["parent_code"] = parent_code;
    result["member_count"] = block.member_count;
    result["is_leaf"] = block.is_leaf;
    return result;
}

const Json& find_resource_document(const Json& documents, std::string_view resource) {
    if (!documents.is_array()) throw Error("industry profile documents must be an array");
    for (const auto& document : documents.as_array())
        if (json_text(document, "resource") == resource) return document;
    throw Error("missing industry profile resource: " + std::string(resource));
}

Json resource_source_summary(const Json& document) {
    return jsn_source_metadata(document);
}

const Json* find_industry_record(const Json& master, const std::string& code) {
    for (const auto& item : master.at("industries").as_array())
        if (json_text(item, "code") == code) return &item;
    return nullptr;
}

bool industry_has_data(const Json& industry, const char* name) {
    const auto* value = json_field(industry, name);
    return value && value->is_bool() && value->as_bool();
}

}  // namespace tdx::detail::industry_profile
