#include "flow_followup_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <utility>

namespace fs = std::filesystem;

namespace tdx::flow_followup_detail {

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
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream output;
    output << local_timestamp_text(local);
    return output.str();
}

std::string today_text() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream output;
    output << std::put_time(&local, "%Y%m%d");
    return output.str();
}

bool digits(const std::string& value) {
    return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return ch >= '0' && ch <= '9';
    });
}

std::string compact_date(std::string value, std::string_view name) {
    value = trim(std::move(value));
    if (value.size() == 10 && value[4] == '-' && value[7] == '-')
        value = value.substr(0, 4) + value.substr(5, 2) + value.substr(8, 2);
    if (value.size() != 8 || !digits(value))
        throw Error(std::string(name) + " must be YYYYMMDD or YYYY-MM-DD");
    const int year = std::stoi(value.substr(0, 4));
    const int month = std::stoi(value.substr(4, 2));
    const int day = std::stoi(value.substr(6, 2));
    static const int month_days[]{0, 31, 28, 31, 30, 31, 30,
                                  31, 31, 30, 31, 30, 31};
    const bool leap = year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
    const int maximum = month == 2 ? 28 + (leap ? 1 : 0) :
                        (month >= 1 && month <= 12 ? month_days[month] : 0);
    if (year < 1990 || year > 2099 || month < 1 || month > 12 ||
        day < 1 || day > maximum)
        throw Error(std::string(name) + " is outside the supported calendar range");
    return value;
}

std::string display_date(const std::string& value) {
    if (value.size() == 8 && digits(value))
        return value.substr(0, 4) + "-" + value.substr(4, 2) + "-" + value.substr(6, 2);
    return value;
}

const Json* field(const Json& row, std::string_view key) {
    if (!row.is_object()) return nullptr;
    const auto exact = row.as_object().find(key);
    if (exact != row.as_object().end()) return &exact->second;
    const auto wanted = lower_ascii(std::string(key));
    for (const auto& [name, value] : row.as_object())
        if (lower_ascii(name) == wanted) return &value;
    return nullptr;
}

std::string text(const Json& row, std::string_view key) {
    const auto* value = field(row, key);
    if (!value || value->is_null()) return {};
    if (value->is_string()) return trim(value->as_string());
    if (value->is_number()) {
        std::ostringstream output;
        output << std::setprecision(15) << value->as_number();
        return output.str();
    }
    return {};
}

std::optional<double> number(const Json& row, std::string_view key) {
    const auto* value = field(row, key);
    if (!value || value->is_null()) return std::nullopt;
    if (value->is_number()) return value->as_number();
    if (!value->is_string()) return std::nullopt;
    auto raw = trim(value->as_string());
    raw.erase(std::remove(raw.begin(), raw.end(), ','), raw.end());
    if (!raw.empty() && raw.back() == '%') raw.pop_back();
    if (raw.empty() || raw == "-" || raw == "--") return std::nullopt;
    try {
        std::size_t used = 0;
        const double result = std::stod(raw, &used);
        if (used == raw.size() && std::isfinite(result)) return result;
    } catch (...) {}
    return std::nullopt;
}

Json number_json(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

bool bool_value(const Json& row, std::string_view key, bool fallback) {
    const auto* value = field(row, key);
    return value && value->is_bool() ? value->as_bool() : fallback;
}


std::string canonical_view(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    for (const auto& entry : view_aliases)
        if (entry.alias == value) return std::string(entry.canonical);
    throw Error(
        "view must be margin, northbound, financing-model, lending-model, "
        "northbound-inflow-model, or northbound-purchase-model");
}

bool history_view(const std::string& view) {
    return view == "margin" || view == "northbound";
}


const FlowModelDefinition& model_definition(const std::string& view) {
    for (const auto& definition : model_definitions)
        if (view == definition.view) return definition;
    throw Error("selected flow view is not a model view");
}

Json available_views() {
    Json result = Json::array();
    for (const auto name : available_view_names)
        result.push_back(std::string(name));
    return result;
}

int bounded(const std::string& raw, std::string_view name,
            int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(raw, &used);
        if (used != raw.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}


std::string cache_key(const FlowFollowupQuery& query) {
    std::ostringstream output;
    output << query.view << '|' << query.start_date << '|' << query.end_date << '|'
           << query.available_only << '|' << query.limit;
    return output.str();
}


}  // namespace tdx::flow_followup_detail
