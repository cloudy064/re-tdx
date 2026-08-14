#include "fund_analytics_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/cloud_resilience.hpp"
#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace fs = std::filesystem;

namespace tdx::fund_analytics_detail {

namespace {

bool leap_year(int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

int month_days(int year, int month) {
    static constexpr int lengths[]{31, 28, 31, 30, 31, 30,
                                   31, 31, 30, 31, 30, 31};
    return month == 2 && leap_year(year) ? 29 : lengths[month - 1];
}

}  // namespace

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

CivilDate today_local() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return {local.tm_year + 1900, local.tm_mon + 1, local.tm_mday};
}

std::string date_text(const CivilDate& value) {
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << value.year
           << std::setw(2) << value.month << std::setw(2) << value.day;
    return output.str();
}

CivilDate shift_months(CivilDate value, int months) {
    const int index = value.year * 12 + value.month - 1 + months;
    value.year = index / 12;
    value.month = index % 12 + 1;
    value.day = std::min(value.day, month_days(value.year, value.month));
    return value;
}

CivilDate shift_years(CivilDate value, int years) {
    value.year += years;
    value.day = std::min(value.day, month_days(value.year, value.month));
    return value;
}

CivilDate latest_full_fund_report(const CivilDate& value) {
    if (value.month > 8 || (value.month == 8 && value.day >= 31))
        return {value.year, 6, 30};
    return {value.year - 1, 12, 31};
}

std::string previous_full_fund_report(const std::string& value) {
    const int year = std::stoi(value.substr(0, 4));
    if (value.substr(4) == "1231") return std::to_string(year) + "0630";
    if (value.substr(4) == "0630") return std::to_string(year - 1) + "1231";
    throw Error("automatic report fallback requires a June or December report date");
}

std::string date_days_ago(int days) {
    const auto value = std::time(nullptr) -
        static_cast<std::time_t>(days) * 86400;
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &value);
#else
    localtime_r(&value, &local);
#endif
    return date_text({local.tm_year + 1900, local.tm_mon + 1, local.tm_mday});
}

bool digits(const std::string& value) {
    return !value.empty() &&
        std::all_of(value.begin(), value.end(), [](unsigned char ch) {
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
    if (year < 1900 || month < 1 || month > 12 || day < 1 ||
        day > month_days(year, month))
        throw Error(std::string(name) +
                    " is outside the supported calendar range");
    return value;
}

std::string display_date(const std::string& value) {
    return value.size() == 8
        ? value.substr(0, 4) + "-" + value.substr(4, 2) + "-" +
              value.substr(6, 2)
        : value;
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

const Json* field(const Json& row, std::string_view key) {
    if (!row.is_object()) return nullptr;
    const auto found = row.as_object().find(key);
    if (found != row.as_object().end()) return &found->second;
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

Json scaled_number_json(const Json& row, std::string_view key, double scale) {
    const auto value = number(row, key);
    return value ? Json(*value * scale) : Json(nullptr);
}

Json optional_date(const std::string& value) {
    return value.empty() || value == "0" || value == "-1"
        ? Json(nullptr) : Json(display_date(value));
}

int integer(const std::string& raw, std::string_view name,
            int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(raw, &used);
        if (used != raw.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " +
                    std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

Json managers(const std::string& raw) {
    Json result = Json::array();
    std::string item;
    auto flush = [&]() {
        item = trim(std::move(item));
        if (!item.empty()) result.push_back(item);
        item.clear();
    };
    for (std::size_t index = 0; index < raw.size();) {
        if (raw[index] == ',' ||
            (index + 2 < raw.size() &&
             static_cast<unsigned char>(raw[index]) == 0xEF &&
             static_cast<unsigned char>(raw[index + 1]) == 0xBC &&
             static_cast<unsigned char>(raw[index + 2]) == 0x8C)) {
            flush();
            index += raw[index] == ',' ? 1 : 3;
        } else {
            item.push_back(raw[index++]);
        }
    }
    flush();
    return result;
}

Json fund_document(const Json& row) {
    const auto code = text(row, "fund_code");
    Json result = Json::object();
    result["entity_type"] = "fund";
    result["fund_id"] = "FUND:" + code;
    result["code"] = code;
    result["name"] = text(row, "fund_name");
    const auto market = text(row, "fund_market").empty()
        ? text(row, "market_code") : text(row, "fund_market");
    result["tdx_market_id"] = market.empty() ? Json(nullptr) :
        number_json(number(
            row, text(row, "fund_market").empty()
                ? "market_code" : "fund_market"));
    result["market"] = market.empty()
        ? Json(nullptr) : Json("tdx-fund-" + market);
    return result;
}

Json common_fund_record(const Json& row) {
    Json result = Json::object();
    result["fund"] = fund_document(row);
    result["latest_nav"] = number_json(number(row, "netValue"));
    auto change = number(row, "changeRate");
    if (!change) change = number(row, "return_rate");
    result["latest_change_pct"] = number_json(change);
    const auto style = text(row, "style_details");
    Json style_value = Json::object();
    style_value["code"] = style;
    const auto* definition = style_definition(style);
    style_value["name"] = definition
        ? Json(std::string(definition->name)) : Json("unknown");
    result["style"] = std::move(style_value);
    result["company"] = text(row, "FundCopName");
    result["latest_share_count"] = number_json(number(row, "FundSize"));
    result["established_date"] = optional_date(text(row, "FundEstTime"));
    result["managers"] = managers(text(row, "ManagerName"));
    return result;
}

bool transient_error(const std::string& message) {
    return detail::is_transient_cloud_error(message);
}

std::string cache_key(const FundAnalyticsQuery& query) {
    std::ostringstream output;
    output << query.view << '|' << lower_ascii(query.query) << '|'
           << query.fund_code << '|' << query.style << '|' << query.fund_size
           << '|' << query.fund_age << '|' << query.benchmark << '|'
           << query.start_date << '|' << query.end_date << '|'
           << query.report_date << '|' << query.estimate_date << '|'
           << std::setprecision(10) << query.risk_free_rate << '|'
           << query.all_pages << '|' << query.limit;
    return output.str();
}

}  // namespace tdx::fund_analytics_detail
