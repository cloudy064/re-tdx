#include "options_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/minute.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <unordered_set>

namespace fs = std::filesystem;

namespace tdx::option_detail {

std::string normalize_date(std::string value) {
    value = trim(value);
    if (value.empty()) return {};
    if (value.size() == 10 && value[4] == '-' && value[7] == '-') return value;
    if (value.size() == 8 && std::all_of(value.begin(), value.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
        }))
        return value.substr(0, 4) + "-" + value.substr(4, 2) + "-" + value.substr(6, 2);
    throw Error("date/expiry must use YYYYMMDD or YYYY-MM-DD");
}

// Days since 1970-01-01, using Howard Hinnant's civil-calendar transform.
long long civil_days(std::string value) {
    value = normalize_date(std::move(value));
    const int year = std::stoi(value.substr(0, 4));
    const unsigned month = static_cast<unsigned>(std::stoi(value.substr(5, 2)));
    const unsigned day = static_cast<unsigned>(std::stoi(value.substr(8, 2)));
    if (month < 1 || month > 12 || day < 1 || day > 31)
        throw Error("date is outside the supported calendar range");
    int y = year - (month <= 2);
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned mp = month > 2 ? month - 3 : month + 9;
    const unsigned doy = (153 * mp + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<long long>(era) * 146097 + static_cast<long long>(doe) - 719468;
}

std::string date_text(int value) {
    const auto digits = std::to_string(value);
    if (digits.size() != 8) return {};
    return digits.substr(0, 4) + "-" + digits.substr(4, 2) + "-" + digits.substr(6, 2);
}

int date_from_days(long long value) {
    value += 719468;
    const long long era = (value >= 0 ? value : value - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(value - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int year = static_cast<int>(yoe) + static_cast<int>(era * 400);
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned day = doy - (153 * mp + 2) / 5 + 1;
    const unsigned month = mp < 10 ? mp + 3 : mp - 9;
    year += month <= 2;
    return year * 10000 + static_cast<int>(month) * 100 + static_cast<int>(day);
}

long long date_days(int value) {
    const auto text = date_text(value);
    if (text.empty()) throw Error("invalid YYYYMMDD date in TDX option metadata");
    return civil_days(text);
}

int add_calendar_days(int value, int delta) {
    return date_from_days(date_days(value) + delta);
}

int weekday(int value) {
    auto result = static_cast<int>((date_days(value) + 4) % 7);
    if (result < 0) result += 7;
    return result;  // Sunday=0, Friday=5, Saturday=6, matching GetWeek.
}

bool weekday_session(int value) {
    const int day = weekday(value);
    return day != 0 && day != 6;
}

int last_day_of_month(int year, int month) {
    const int next_year = month == 12 ? year + 1 : year;
    const int next_month = month == 12 ? 1 : month + 1;
    return add_calendar_days(next_year * 10000 + next_month * 100 + 1, -1);
}

int shifted_contract_month(int contract_month, int delta) {
    int year = 2000 + contract_month / 100;
    int month = contract_month % 100;
    month += delta;
    while (month < 1) { month += 12; --year; }
    while (month > 12) { month -= 12; ++year; }
    return (year - 2000) * 100 + month;
}

int move_weekday(int value, int direction) {
    do value = add_calendar_days(value, direction); while (!weekday_session(value));
    return value;
}

int adjust_holiday_forward(int value, const std::set<int>& holidays) {
    while (holidays.count(value)) value = move_weekday(value, 1);
    return value;
}

std::vector<std::string> comma_fields(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t begin = 0;
    while (begin <= line.size()) {
        const auto end = line.find(',', begin);
        fields.push_back(line.substr(begin, end == std::string::npos ? std::string::npos : end - begin));
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return fields;
}

int metadata_integer(const std::vector<std::string>& fields, std::size_t index) {
    if (index >= fields.size() || trim(fields[index]).empty()) return 0;
    try { return std::stoi(trim(fields[index])); }
    catch (...) { return 0; }
}

struct OptionExpiryRule {
    int market_id{};
    std::string product;
    int cutoff_contract_month{};
    int cutoff_expiry{};
    int rule_code{};
    int rule_parameter{};
};

int rule_market(std::string value) {
    value = upper_ascii(trim(std::move(value)));
    if (value == "OJ") return 7;
    if (value == "OD") return 5;
    if (value == "OZ") return 4;
    if (value == "OS") return 6;
    if (value == "OG") return 67;
    return -1;
}

std::vector<OptionExpiryRule> read_option_expiry_rules(const fs::path& path) {
    const auto bytes = read_bytes(path);
    const std::string text(bytes.begin(), bytes.end());
    std::vector<OptionExpiryRule> rules;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto fields = comma_fields(line);
        if (fields.size() < 15) continue;
        OptionExpiryRule rule;
        rule.product = upper_ascii(trim(fields[0]));
        rule.market_id = rule_market(fields[2]);
        rule.cutoff_contract_month = metadata_integer(fields, 4);
        rule.cutoff_expiry = metadata_integer(fields, 5);
        rule.rule_code = metadata_integer(fields, 13);
        rule.rule_parameter = metadata_integer(fields, 14);
        if (rule.market_id >= 0 && !rule.product.empty() && rule.rule_code)
            rules.push_back(std::move(rule));
    }
    return rules;
}

std::set<int> read_holidays(const fs::path& path, std::string_view key) {
    const auto bytes = read_bytes(path);
    const std::string text(bytes.begin(), bytes.end());
    const std::string prefix = std::string(key) + "=";
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind(prefix, 0) != 0) continue;
        std::set<int> dates;
        for (const auto& field : comma_fields(line.substr(prefix.size()))) {
            const int value = metadata_integer({field}, 0);
            if (value >= 19000101 && value <= 21991231) dates.insert(value);
        }
        return dates;
    }
    return {};
}

std::pair<int, int> contract_year_month(int contract_month) {
    return {2000 + contract_month / 100, contract_month % 100};
}

int scan_trading_day(int start, int boundary, int direction, int ordinal,
                     const std::set<int>& holidays, bool final_step) {
    int value = start;
    int count = 0;
    while ((direction > 0 && value <= boundary) || (direction < 0 && value >= boundary)) {
        if (weekday_session(value) && !holidays.count(value)) {
            if (ordinal == 1) return value;
            ++count;
        }
        if (count == ordinal - 1) break;
        value = add_calendar_days(value, direction);
    }
    if (!final_step) return value;
    value = move_weekday(value, direction);
    return adjust_holiday_forward(value, holidays);
}

int calculate_option_expiry_rule(int contract_month, int rule_code, int parameter,
                                 const std::set<int>& holidays) {
    if (contract_month < 1 || parameter < 1) return 0;
    auto [year, month] = contract_year_month(contract_month);
    if (month < 1 || month > 12) return 0;
    const int first = year * 10000 + month * 100 + 1;
    const int last = last_day_of_month(year, month);
    switch (rule_code) {
        case 'd': {  // Nth Friday of the supplied month.
            int count = 0;
            for (int value = first; value <= last; value = add_calendar_days(value, 1))
                if (weekday(value) == 5 && ++count == parameter)
                    return adjust_holiday_forward(value, holidays);
            return 0;
        }
        case 'h':
            return scan_trading_day(first, last, 1, parameter, holidays, true);
        case 'i': {  // Nth-last session of the preceding month.
            const int previous = shifted_contract_month(contract_month, -1);
            const auto [py, pm] = contract_year_month(previous);
            return scan_trading_day(last_day_of_month(py, pm), py * 10000 + pm * 100 + 1,
                                    -1, parameter, holidays, true);
        }
        case 'j': {  // Nth-last session two months before delivery.
            const int previous = shifted_contract_month(contract_month, -2);
            const auto [py, pm] = contract_year_month(previous);
            return scan_trading_day(last_day_of_month(py, pm), py * 10000 + pm * 100 + 1,
                                    -1, parameter, holidays, true);
        }
        case 'r': {  // Nth session backwards from day 15.
            const int fifteenth = year * 10000 + month * 100 + 15;
            return scan_trading_day(fifteenth, first, -1, parameter, holidays, true);
        }
        case '}': {  // Same day-15 rule two months before delivery.
            const int previous = shifted_contract_month(contract_month, -2);
            const auto [py, pm] = contract_year_month(previous);
            return scan_trading_day(py * 10000 + pm * 100 + 15,
                                    py * 10000 + pm * 100 + 1,
                                    -1, parameter, holidays, true);
        }
        case '~': {  // Nth session from the start two months before delivery.
            const int previous = shifted_contract_month(contract_month, -2);
            const auto [py, pm] = contract_year_month(previous);
            return scan_trading_day(py * 10000 + pm * 100 + 1,
                                    last_day_of_month(py, pm), 1,
                                    parameter, holidays, true);
        }
        default:
            return 0;
    }
}

std::string option_product(const OptionInstrument& option) {
    std::size_t end = 0;
    while (end < option.contract.size() &&
           !std::isdigit(static_cast<unsigned char>(option.contract[end]))) ++end;
    return upper_ascii(option.contract.substr(0, end));
}

int option_contract_month(const OptionInstrument& option) {
    std::size_t begin = option.contract.size();
    while (begin > 0 && std::isdigit(static_cast<unsigned char>(option.contract[begin - 1]))) --begin;
    const auto digits = option.contract.substr(begin);
    if (digits.size() != 3 && digits.size() != 4) return 0;
    try {
        const int value = std::stoi(digits);
        return digits.size() == 3 ? value + 1000 : value;
    } catch (...) {
        return 0;
    }
}

}  // namespace tdx::option_detail

namespace tdx {

using namespace option_detail;

Json resolve_option_expiry_document(const fs::path& root_path,
                                    const OptionInstrument& option) {
    const auto root = find_tdx_root(root_path);
    const auto rules_path = root / "T0002" / "hq_cache" / "code2name_qq.ini";
    const auto holidays_path = root / "T0002" / "hq_cache" / "neednote.dat";
    if (!fs::is_regular_file(rules_path))
        throw Error("TDX option rule resource is missing: " + path_utf8(rules_path));
    const auto rules = read_option_expiry_rules(rules_path);
    const auto product = option_product(option);
    const int contract_month = option_contract_month(option);
    const OptionExpiryRule* selected = nullptr;
    for (const auto& rule : rules)
        if (rule.market_id == option.market_id && rule.product == product) {
            selected = &rule;
            break;
        }
    Json result = Json::object();
    result["schema"] = "tdx-option-expiry-v1";
    result["option"] = option_instrument_document(option);
    result["product"] = product;
    result["contract_month"] = contract_month;
    result["rules_source"] = path_utf8(rules_path);
    result["holiday_source"] = path_utf8(holidays_path);
    result["resource_mode"] = "TdxW-code2name_qq/neednote-exact";
    result["expiry"] = Json(nullptr);
    if (!selected || !contract_month) {
        result["status"] = selected ? "invalid_contract_month" : "rule_not_found";
        return result;
    }
    result["cutoff_contract_month"] = selected->cutoff_contract_month;
    result["cutoff_expiry_raw"] = selected->cutoff_expiry;
    result["rule_code"] = selected->rule_code;
    result["rule_character"] = std::string(1, static_cast<char>(selected->rule_code));
    result["rule_parameter"] = selected->rule_parameter;
    if (selected->cutoff_contract_month && contract_month < selected->cutoff_contract_month) {
        result["status"] = "contract_precedes_resource_cutoff";
        return result;
    }
    if (selected->cutoff_contract_month == contract_month && selected->cutoff_expiry) {
        result["expiry"] = date_text(selected->cutoff_expiry);
        result["expiry_raw"] = selected->cutoff_expiry;
        result["status"] = "exact_cutoff_override";
        result["rule_month"] = Json(nullptr);
        result["holiday_count"] = 0;
        return result;
    }
    std::set<int> holidays;
    if (fs::is_regular_file(holidays_path))
        holidays = read_holidays(holidays_path, "RecentHSHoliday");
    int rule_month = contract_month;
    // TdxW's option-side auxiliary code already carries the preceding month for
    // DCE/GFEX forward-count and CZCE day-15 rules.  Public catalog names carry
    // the delivery month, so reproduce that verified one-month conversion here.
    if (selected->rule_code == 'h' || selected->rule_code == 'r')
        rule_month = shifted_contract_month(contract_month, -1);
    const int expiry = calculate_option_expiry_rule(
        rule_month, selected->rule_code, selected->rule_parameter, holidays);
    result["rule_month"] = rule_month;
    result["holiday_count"] = static_cast<std::uint64_t>(holidays.size());
    if (!expiry) {
        result["status"] = "unsupported_rule";
        return result;
    }
    result["expiry"] = date_text(expiry);
    result["expiry_raw"] = expiry;
    result["status"] = "calculated_from_tdx_rule";
    return result;
}

}  // namespace tdx
