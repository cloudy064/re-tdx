#include "convertible_bonds_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/market.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::convertible_bond_detail {

const Json* field(const Json& row, std::string_view name) {
    if (!row.is_object()) return nullptr;
    const auto found = row.as_object().find(name);
    return found == row.as_object().end() ? nullptr : &found->second;
}

std::string value_text(const Json& row, std::string_view name) {
    const auto* value = field(row, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

std::optional<double> value_number(const Json& row, std::string_view name) {
    const auto text = value_text(row, name);
    if (text.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto value = std::stod(text, &used);
        return used == text.size() && std::isfinite(value)
            ? std::optional<double>(value) : std::nullopt;
    } catch (...) { return std::nullopt; }
}

Json number(const Json& row, std::string_view name) {
    const auto value = value_number(row, name);
    return value ? Json(*value) : Json(nullptr);
}

Json optional_number(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

bool digits(const std::string& value, std::size_t length) {
    return value.size() == length &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

std::string compact_date_prefix(const std::string& source) {
    const auto value = trim(source);
    if (value.size() >= 10 && value[4] == '-' && value[7] == '-') {
        const auto candidate = value.substr(0, 4) + value.substr(5, 2) +
            value.substr(8, 2);
        if (digits(candidate, 8)) return candidate;
    }
    if (value.size() >= 8) {
        const auto candidate = value.substr(0, 8);
        if (digits(candidate, 8)) return candidate;
    }
    return {};
}

Json empty_rows_document() {
    Json document = Json::object();
    document["rows"] = Json::array();
    return document;
}

std::set<std::string> normalized_security_ids(const Json& rows,
                                              std::string_view member_name) {
    std::set<std::string> result;
    if (!rows.is_array()) return result;
    for (const auto& row : rows.as_array()) {
        if (!row.is_object()) continue;
        const auto member = row.as_object().find(member_name);
        if (member == row.as_object().end() || !member->second.is_object()) continue;
        const auto id = member->second.as_object().find("security_id");
        if (id != member->second.as_object().end() && id->second.is_string() &&
            !id->second.as_string().empty()) result.insert(id->second.as_string());
    }
    return result;
}

Json security_set_reconciliation(const Json& primary_rows,
                                 const Json& projection_rows,
                                 std::string_view member_name,
                                 const std::string& primary_resource,
                                 const std::string& projection_resource) {
    const auto primary = normalized_security_ids(primary_rows, member_name);
    const auto projection = normalized_security_ids(projection_rows, member_name);
    Json primary_only = Json::array();
    Json projection_only = Json::array();
    std::uint64_t common = 0;
    for (const auto& id : primary) {
        if (projection.count(id)) ++common;
        else primary_only.push_back(id);
    }
    for (const auto& id : projection)
        if (!primary.count(id)) projection_only.push_back(id);
    Json result = Json::object();
    result["primary_resource"] = primary_resource;
    result["projection_resource"] = projection_resource;
    result["primary_rows"] = static_cast<std::uint64_t>(
        primary_rows.is_array() ? primary_rows.size() : 0);
    result["projection_rows"] = static_cast<std::uint64_t>(
        projection_rows.is_array() ? projection_rows.size() : 0);
    result["primary_securities"] = static_cast<std::uint64_t>(primary.size());
    result["projection_securities"] = static_cast<std::uint64_t>(projection.size());
    result["common_securities"] = common;
    result["primary_only_security_ids"] = std::move(primary_only);
    result["projection_only_security_ids"] = std::move(projection_only);
    result["exact_security_set"] = primary == projection;
    return result;
}

int market_id(const std::string& value);
bool valid_code(const std::string& code);

std::set<std::string> raw_security_ids(const Json& document,
                                       std::string_view market_field,
                                       std::string_view code_field) {
    std::set<std::string> result;
    if (!document.is_object() || !document.as_object().count("rows") ||
        !document.at("rows").is_array()) return result;
    for (const auto& row : document.at("rows").as_array()) {
        const auto code = value_text(row, code_field);
        if (!valid_code(code)) continue;
        try {
            const auto market = market_id(value_text(row, market_field));
            result.insert((market == 0 ? "SZ" : market == 1 ? "SH" : "BJ") + code);
        } catch (...) {}
    }
    return result;
}

Json raw_document_reconciliation(const Json& primary_document,
                                 const Json& projection_document,
                                 const std::string& primary_resource,
                                 const std::string& projection_resource) {
    const auto primary = raw_security_ids(primary_document, "$SC", "$ZQDM");
    const auto projection = raw_security_ids(projection_document, "$SC", "$ZQDM");
    Json primary_only = Json::array(), projection_only = Json::array();
    std::uint64_t common = 0;
    for (const auto& id : primary) {
        if (projection.count(id)) ++common;
        else primary_only.push_back(id);
    }
    for (const auto& id : projection)
        if (!primary.count(id)) projection_only.push_back(id);
    Json result = Json::object();
    result["primary_resource"] = primary_resource;
    result["projection_resource"] = projection_resource;
    result["primary_securities"] = static_cast<std::uint64_t>(primary.size());
    result["projection_securities"] = static_cast<std::uint64_t>(projection.size());
    result["common_securities"] = common;
    result["primary_only_security_ids"] = std::move(primary_only);
    result["projection_only_security_ids"] = std::move(projection_only);
    result["exact_security_set"] = primary == projection;
    return result;
}

// Days since 1970-01-01. This civil-date conversion keeps the valuation path
// independent from platform-specific tm/DST behavior.
std::optional<long long> civil_days(const std::string& compact) {
    std::string value = trim(compact);
    value.erase(std::remove(value.begin(), value.end(), '-'), value.end());
    if (!digits(value, 8)) return std::nullopt;
    const int year = std::stoi(value.substr(0, 4));
    const unsigned month = static_cast<unsigned>(std::stoi(value.substr(4, 2)));
    const unsigned day = static_cast<unsigned>(std::stoi(value.substr(6, 2)));
    if (month < 1 || month > 12 || day < 1 || day > 31) return std::nullopt;
    int y = year - (month <= 2);
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned mp = month > 2 ? month - 3 : month + 9;
    const unsigned doy = (153 * mp + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<long long>(era) * 146097 + doe - 719468;
}

std::string today_compact() {
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

std::vector<std::string> csv_text(const std::string& source) {
    std::vector<std::string> result;
    for (const auto& part : split(source, ',')) {
        const auto value = trim(part);
        if (!value.empty()) result.push_back(value);
    }
    return result;
}

std::vector<double> csv_numbers(const std::string& source) {
    std::vector<double> result;
    for (const auto& part : split(source, ',')) {
        try {
            const auto value = std::stod(trim(part));
            if (std::isfinite(value)) result.push_back(value);
        } catch (...) {}
    }
    return result;
}

std::optional<double> quote_number(const Json& row, const char* name) {
    if (!row.is_object()) return std::nullopt;
    const auto found = row.as_object().find(name);
    return found != row.as_object().end() && found->second.is_number()
        ? std::optional<double>(found->second.as_number()) : std::nullopt;
}

std::optional<double> usable_quote_price(const Json* row) {
    if (!row) return std::nullopt;
    const auto last = quote_number(*row, "last_price");
    if (last && *last > 0) return last;
    const auto previous = quote_number(*row, "pre_close_price");
    return previous && *previous > 0 ? previous : std::nullopt;
}

std::string quote_price_source(const Json* row) {
    if (!row) return "unavailable";
    const auto last = quote_number(*row, "last_price");
    if (last && *last > 0) return "last-price";
    const auto previous = quote_number(*row, "pre_close_price");
    return previous && *previous > 0 ? "pre-close" : "unavailable";
}

std::optional<double> accrued_interest(const Json& row, const std::string& as_of) {
    const auto face = value_number(row, "MZ");
    const auto previous = civil_days(value_text(row, "SGFXRQ"));
    const auto next = civil_days(value_text(row, "XGFXRQ"));
    const auto current = civil_days(as_of);
    const auto remaining_rates = csv_numbers(value_text(row, "SYFXLLXL"));
    if (!face || !previous || !next || !current || remaining_rates.empty() ||
        *current < *previous || *current > *next) return std::nullopt;
    // The prospectus formula used by this table is IA = B * i * t / 365.
    return *face * remaining_rates.front() *
        static_cast<double>(*current - *previous) / 365.0;
}

std::vector<CashFlow> remaining_cash_flows(const Json& row,
                                           const std::string& as_of) {
    const auto current = civil_days(as_of);
    const auto face = value_number(row, "MZ");
    const auto dates = csv_text(value_text(row, "SYFXRQXL"));
    const auto rates = csv_numbers(value_text(row, "SYFXLLXL"));
    std::vector<CashFlow> flows;
    if (!current || !face) return flows;
    const auto count = std::min(dates.size(), rates.size());
    for (std::size_t i = 0; i < count; ++i) {
        const auto date = civil_days(dates[i]);
        if (!date || *date <= *current) continue;
        double amount = *face * rates[i];
        if (i + 1 == count) amount += *face;
        flows.push_back({static_cast<double>(*date - *current) / 365.0, amount});
    }
    return flows;
}

std::optional<double> discounted_value(const std::vector<CashFlow>& flows,
                                       double annual_rate) {
    if (flows.empty() || annual_rate <= -1.0) return std::nullopt;
    double value = 0.0;
    for (const auto& flow : flows) {
        const auto factor = std::pow(1.0 + annual_rate, flow.years);
        if (!std::isfinite(factor) || factor <= 0) return std::nullopt;
        value += flow.amount / factor;
    }
    return std::isfinite(value) ? std::optional<double>(value) : std::nullopt;
}

std::optional<double> solve_ytm(const std::vector<CashFlow>& flows,
                                double full_price) {
    if (flows.empty() || full_price <= 0) return std::nullopt;
    double low = -0.999999, high = 10.0;
    const auto low_value = discounted_value(flows, low);
    const auto high_value = discounted_value(flows, high);
    if (!low_value || !high_value || *low_value < full_price ||
        *high_value > full_price) return std::nullopt;
    for (int i = 0; i < 160; ++i) {
        const double middle = (low + high) / 2.0;
        const auto value = discounted_value(flows, middle);
        if (!value) return std::nullopt;
        if (*value > full_price) low = middle;
        else high = middle;
    }
    return (low + high) / 2.0;
}

int market_id(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized == "0" || normalized == "sz") return 0;
    if (normalized == "1" || normalized == "sh") return 1;
    if (normalized == "2" || normalized == "44" || normalized == "bj") return 2;
    throw Error("market must be sz, sh, bj, 0, 1, 2, or 44");
}

std::string market_name(int id) {
    return id == 0 ? "sz" : id == 1 ? "sh" : id == 2 ? "bj" : "";
}

std::string security_id(int id, const std::string& code) {
    auto prefix = market_name(id);
    std::transform(prefix.begin(), prefix.end(), prefix.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return prefix + code;
}

bool valid_code(const std::string& code) {
    return code.size() == 6 &&
        std::all_of(code.begin(), code.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

std::string key_for(const Json& row) {
    const auto code = value_text(row, "$ZQDM");
    if (!valid_code(code)) return {};
    try { return std::to_string(market_id(value_text(row, "$SC"))) + ":" + code; }
    catch (...) { return {}; }
}

Json security_document(int id, const std::string& code, const std::string& fallback,
                       const std::map<std::pair<int, std::string>, Security>& securities) {
    Json result = Json::object();
    result["market_id"] = id;
    result["market"] = market_name(id);
    result["code"] = code;
    result["security_id"] = security_id(id, code);
    const auto found = securities.find({id, code});
    result["name"] = found == securities.end() ? fallback : found->second.name;
    return result;
}

Json text_array(const std::string& source) {
    Json result = Json::array();
    for (const auto& part : split(source, ',')) if (!trim(part).empty()) result.push_back(trim(part));
    return result;
}

Json numeric_array(const std::string& source) {
    Json result = Json::array();
    for (const auto& part : split(source, ',')) {
        const auto text = trim(part);
        if (text.empty()) continue;
        try { result.push_back(std::stod(text)); }
        catch (...) { result.push_back(text); }
    }
    return result;
}

Json trigger_document(const Json& row, const char* start, const char* price,
                      const char* conversion, const char* current_days,
                      const char* status, const char* history_count,
                      const char* history_dates, const char* available_days) {
    Json result = Json::object();
    result["condition"] = value_text(row, "CFSJTJ");
    result["price_ratio_pct"] = number(row, "CFJGBL");
    result["start_date"] = value_text(row, start);
    result["trigger_price"] = number(row, price);
    result["conversion_price"] = number(row, conversion);
    result["current_days"] = value_text(row, current_days);
    result["current_ratio_pct"] = number(row, "CFJDBL");
    result["status"] = value_text(row, status);
    result["history_count"] = number(row, history_count);
    result["history_dates"] = text_array(value_text(row, history_dates));
    result["available_days"] = number(row, available_days);
    return result;
}

Json source_summary(const Json& document) {
    return jsn_source_metadata(document);
}

Json normalize_detail(const Json& source, const std::string& kind) {
    Json result = Json::array();
    for (const auto& row : source.at("rows").as_array()) {
        if (value_text(row, "$ZQDM").empty()) continue;
        Json item = Json::object();
        item["kind"] = kind;
        if (kind == "sellback") {
            item["start_date"] = value_text(row, "CFHSQSR");
            item["end_date"] = value_text(row, "CFHSZZR");
            item["payment_date"] = value_text(row, "CFFKRQ");
            item["price"] = number(row, "CFHSJG");
            item["quantity"] = number(row, "CFHSSL");
            item["amount"] = number(row, "CFHSJE");
            item["remaining_quantity"] = number(row, "WHSSL");
        } else if (kind == "redemption") {
            item["start_date"] = value_text(row, "CFSHQSR");
            item["payment_date"] = value_text(row, "CFFKRQ");
            item["price"] = number(row, "CFSHJG");
            item["ratio_pct"] = number(row, "CFSHBL");
            item["quantity"] = number(row, "CFSHSL");
            item["amount"] = number(row, "CFSHJE");
            item["remaining_quantity"] = number(row, "WSHSL");
        } else {
            item["date"] = value_text(row, "TZRQ");
            item["conversion_price"] = number(row, "TZZGJG");
            item["reason"] = value_text(row, "TZYY");
        }
        result.push_back(std::move(item));
    }
    return result;
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

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

int bounded(const std::string& text, std::string_view name, int low, int high) {
    try {
        std::size_t used = 0;
        const auto value = std::stoi(text, &used);
        if (used != text.size() || value < low || value > high)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(low) +
                    ".." + std::to_string(high));
    }
}

}  // namespace tdx::convertible_bond_detail
