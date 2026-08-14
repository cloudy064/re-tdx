#include "block_trades_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace tdx::block_trade_detail {

const CoreResourceSpec& core_resource(CoreDataset dataset) {
    const auto found = std::find_if(
        core_resources.begin(), core_resources.end(),
        [dataset](const CoreResourceSpec& item) {
            return item.dataset == dataset;
        });
    if (found == core_resources.end())
        throw Error("unknown block-trade core dataset");
    return *found;
}

const BrokerPeriodSpec& broker_period(std::string_view id) {
    const auto found = std::find_if(
        broker_periods.begin(), broker_periods.end(),
        [id](const BrokerPeriodSpec& item) { return item.id == id; });
    if (found == broker_periods.end())
        throw Error("period must be 1m, 3m, 6m, or 1y");
    return *found;
}

std::vector<std::string> core_resource_names() {
    std::vector<std::string> result;
    result.reserve(core_resources.size());
    for (const auto& item : core_resources)
        result.emplace_back(item.resource);
    return result;
}

bool valid_view(std::string_view view) {
    return std::find(supported_views.begin(), supported_views.end(), view) !=
           supported_views.end();
}

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
    if (!object.is_object()) throw Error("block-trade row must be an object");
    const auto found = object.as_object().find(name);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string text_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

std::optional<double> number_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
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

Json number_or_null(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

Json scaled_number(const Json& row, std::string_view name, double scale) {
    const auto value = number_value(row, name);
    return value ? Json(*value * scale) : Json(nullptr);
}

bool six_digits(const std::string& value) {
    return value.size() == 6 &&
           std::all_of(value.begin(), value.end(), [](char ch) {
               return ch >= '0' && ch <= '9';
           });
}

bool valid_month(const std::string& value) {
    return value.size() == 7 && value[4] == '-' &&
           std::all_of(value.begin(), value.begin() + 4, [](char ch) {
               return ch >= '0' && ch <= '9';
           }) &&
           std::all_of(value.begin() + 5, value.end(), [](char ch) {
               return ch >= '0' && ch <= '9';
           }) &&
           value.substr(5) >= "01" && value.substr(5) <= "12";
}

int canonical_market_id(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized == "0" || normalized == "sz") return 0;
    if (normalized == "1" || normalized == "sh") return 1;
    if (normalized == "2" || normalized == "44" || normalized == "bj") return 2;
    throw Error("market must be sz/sh/bj or 0/1/2");
}

namespace {

std::string market_name(int value) {
    return value == 0 ? "sz" : value == 1 ? "sh" : value == 2 ? "bj"
                                                               : "m" + std::to_string(value);
}

std::string market_prefix(int value) {
    return value == 0 ? "SZ" : value == 1 ? "SH" : value == 2 ? "BJ"
                                                               : "M" + std::to_string(value);
}

}  // namespace

Json security_document(
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

const Json& document_for_resource(const Json& documents,
                                  std::string_view resource) {
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("missing block-trade resource: " + std::string(resource));
}

Json source_summary(const Json& document) {
    return jsn_source_metadata(document);
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

Json limited_filtered(const Json& values, const std::string& query, int limit) {
    Json result = Json::array();
    const auto needle = lower_ascii(trim(query));
    for (const auto& value : values.as_array()) {
        if (!needle.empty() && !json_contains(value, needle)) continue;
        if (static_cast<int>(result.size()) >= limit) break;
        result.push_back(value);
    }
    return result;
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

Json performance_point(int days, const Json& row,
                       const char* success, const char* average) {
    Json result = Json::object();
    result["days"] = days;
    result["success_pct"] = number_or_null(number_value(row, success));
    result["average_return_pct"] = number_or_null(number_value(row, average));
    return result;
}

}  // namespace tdx::block_trade_detail
