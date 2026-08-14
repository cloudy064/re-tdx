#include "tdx/finance_events.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/stats.hpp"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string_view>

namespace tdx {
namespace {

struct EventSpec {
    int finance_id;
    const char* resource;
    const char* date_field;
};

constexpr EventSpec event_specs[] = {
    {90, "list/func_qxfa104_1.jsn", "ggrq"},
    {91, "list/func_qxfa401_1.jsn", "ggrq"},
};

const Json* optional(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string text_or_empty(const Json& object, std::string_view key) {
    const auto* value = optional(object, key);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

bool leap_year(int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

bool valid_date(const std::string& value) {
    if (value.size() != 8 || !std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        })) return false;
    const int year = std::stoi(value.substr(0, 4));
    const int month = std::stoi(value.substr(4, 2));
    const int day = std::stoi(value.substr(6, 2));
    if (year < 1900 || month < 1 || month > 12 || day < 1) return false;
    static constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const int maximum = days[month - 1] + (month == 2 && leap_year(year) ? 1 : 0);
    return day <= maximum;
}

long long civil_day_number(const std::string& value) {
    int year = std::stoi(value.substr(0, 4));
    const unsigned month = static_cast<unsigned>(std::stoi(value.substr(4, 2)));
    const unsigned day = static_cast<unsigned>(std::stoi(value.substr(6, 2)));
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned adjusted_month = month > 2 ? month - 3 : month + 9;
    const unsigned doy = (153 * adjusted_month + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<long long>(era) * 146097 + doe;
}

std::string normalized_market(const std::string& value) {
    const auto market = lower_ascii(trim(value));
    if (market == "0" || market == "sz") return "0";
    if (market == "1" || market == "sh") return "1";
    if (market == "2" || market == "44" || market == "bj") return "2";
    throw Error("finance event market must be sz/sh/bj or 0/1/2");
}

const Json& source_document(const Json& documents, std::string_view resource) {
    if (!documents.is_array()) throw Error("finance event sources must be an array");
    for (const auto& document : documents.as_array()) {
        if (text_or_empty(document, "resource") == resource) return document;
    }
    throw Error("missing finance event resource: " + std::string(resource));
}

Json fetch_documents(int timeout_ms) {
    return fetch_jsn_resources_rows({event_specs[0].resource, event_specs[1].resource},
                                    "bi", timeout_ms);
}

std::shared_ptr<const Json> cached_documents(int timeout_ms) {
    static std::mutex mutex;
    static std::shared_ptr<const Json> cache;
    static std::time_t fetched_at{};
    std::lock_guard<std::mutex> lock(mutex);
    const auto now = std::time(nullptr);
    if (!fetched_at || now - fetched_at >= 300) {
        cache = std::make_shared<const Json>(fetch_documents(timeout_ms));
        fetched_at = now;
    }
    return cache;
}

std::shared_ptr<const TdxStatsResource> cached_stats(int timeout_ms) {
    static std::mutex mutex;
    static std::shared_ptr<const TdxStatsResource> cache;
    static std::time_t fetched_at{};
    std::lock_guard<std::mutex> lock(mutex);
    const auto now = std::time(nullptr);
    if (!fetched_at || now - fetched_at >= 300) {
        cache = std::make_shared<const TdxStatsResource>(
            std::move(download_stats_resource({}, "zhb.zip", 30000, timeout_ms).resource));
        fetched_at = now;
    }
    return cache;
}

FinanceEventValue event_value(std::string date, std::string resource,
                              const std::string& today) {
    FinanceEventValue value;
    value.resource = std::move(resource);
    if (valid_date(date) && date <= today) {
        value.event_date = std::move(date);
        value.days = static_cast<int>(civil_day_number(today) -
                                      civil_day_number(value.event_date) + 1);
    }
    return value;
}

}  // namespace

std::string finance_event_today() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &now)) throw Error("cannot read local date");
#else
    if (!localtime_r(&now, &local)) throw Error("cannot read local date");
#endif
    std::ostringstream output;
    output << std::put_time(&local, "%Y%m%d");
    return output.str();
}

std::map<int, FinanceEventValue> finance_event_values_from_documents(
    const Json& documents,
    const std::string& market,
    const std::string& code,
    const std::set<int>& finance_ids,
    const std::string& today) {
    if (!valid_date(today)) throw Error("finance event today must be YYYYMMDD");
    if (code.size() != 6 || !std::all_of(code.begin(), code.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        })) throw Error("finance event code must contain six digits");
    const auto selected_market = normalized_market(market);
    std::map<int, FinanceEventValue> result;
    for (const auto& spec : event_specs) {
        if (!finance_ids.count(spec.finance_id)) continue;
        FinanceEventValue value;
        value.resource = spec.resource;
        const auto& document = source_document(documents, spec.resource);
        const auto* rows = optional(document, "rows");
        if (!rows || !rows->is_array())
            throw Error("finance event resource lacks rows: " + std::string(spec.resource));
        for (const auto& row : rows->as_array()) {
            if (text_or_empty(row, "$SC") != selected_market ||
                text_or_empty(row, "$ZQDM") != code) continue;
            const auto date = text_or_empty(row, spec.date_field);
            if (!valid_date(date) || date > today) continue;
            if (date > value.event_date) value.event_date = date;
        }
        if (!value.event_date.empty()) {
            value.days = static_cast<int>(civil_day_number(today) -
                                          civil_day_number(value.event_date) + 1);
        }
        result.emplace(spec.finance_id, std::move(value));
    }
    return result;
}

std::map<int, FinanceEventValue> finance_event_values_from_stats(
    const TdxStatsResource& resource,
    const std::string& market,
    const std::string& code,
    const std::set<int>& finance_ids,
    const std::string& today) {
    if (!valid_date(today)) throw Error("finance event today must be YYYYMMDD");
    if (code.size() != 6 || !std::all_of(code.begin(), code.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        })) throw Error("finance event code must contain six digits");
    const int market_id = std::stoi(normalized_market(market));
    std::map<int, FinanceEventValue> result;
    if (!finance_ids.count(88)) return result;
    FinanceEventValue value;
    value.resource = resource.source_path + "#tipinfo.dat";
    const auto found = resource.tip_info_events.find({market_id, code});
    if (found != resource.tip_info_events.end() && found->second.northbound_date &&
        found->second.northbound_direction &&
        *found->second.northbound_direction > 0.0001) {
        value = event_value(*found->second.northbound_date, value.resource, today);
    }
    result.emplace(88, std::move(value));
    return result;
}

std::map<int, FinanceEventValue> fetch_finance_event_values(
    const std::string& market,
    const std::string& code,
    const std::set<int>& finance_ids,
    int timeout_ms) {
    const auto today = finance_event_today();
    std::map<int, FinanceEventValue> result;
    if (finance_ids.count(88)) {
        auto values = finance_event_values_from_stats(*cached_stats(timeout_ms), market, code,
                                                      {88}, today);
        result.insert(values.begin(), values.end());
    }
    std::set<int> documents;
    for (const int id : finance_ids) if (id == 90 || id == 91) documents.insert(id);
    if (!documents.empty()) {
        auto values = finance_event_values_from_documents(*cached_documents(timeout_ms),
                                                          market, code, documents, today);
        result.insert(values.begin(), values.end());
    }
    return result;
}

}  // namespace tdx
