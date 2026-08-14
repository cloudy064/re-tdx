#include "leverage_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <utility>

namespace fs = std::filesystem;

namespace tdx::leverage_detail {

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

const Json* ptr(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string value_text(const Json& object, std::string_view key) {
    const auto* value = ptr(object, key);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

std::optional<double> value_number(const Json& object, std::string_view key) {
    const auto* value = ptr(object, key);
    if (!value || value->is_null()) return std::nullopt;
    if (value->is_number()) return value->as_number();
    const auto text = trim(jsn_scalar_text(*value));
    if (text.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto result = std::stod(text, &used);
        return used == text.size() && std::isfinite(result)
            ? std::optional<double>(result) : std::nullopt;
    } catch (...) { return std::nullopt; }
}

Json number_json(const std::optional<double>& value, double multiplier) {
    return value ? Json(*value * multiplier) : Json(nullptr);
}

bool digits(const std::string& value, std::size_t size) {
    return value.size() == size && std::all_of(value.begin(), value.end(), [](char ch) {
        return ch >= '0' && ch <= '9';
    });
}

bool safe_group_id(const std::string& value) {
    return !value.empty() && value.size() <= 32 &&
           std::all_of(value.begin(), value.end(), [](unsigned char ch) {
               return std::isalnum(ch) || ch == '_' || ch == '-';
           });
}

int market_id(std::string value, bool mainland_only) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "sz") return 0;
    if (value == "sh") return 1;
    if (value == "bj") return 2;
    if (value == "hk") return 31;
    try {
        std::size_t used = 0;
        const int result = std::stoi(value, &used);
        if (used != value.size() || result < 0 || result > 255 ||
            (mainland_only && result != 0 && result != 1 && result != 2 && result != 44))
            throw std::invalid_argument("range");
        return result == 44 ? 2 : result;
    } catch (...) {
        throw Error(mainland_only ? "market must be sz, sh, bj, 0, 1, 2, or 44"
                                  : "stock-connect market is invalid");
    }
}

std::string market_name(int id) {
    if (id == 0) return "sz";
    if (id == 1) return "sh";
    if (id == 2 || id == 44) return "bj";
    if (id == 31 || id == 48) return "hk";
    return std::to_string(id);
}

std::string market_prefix(int id) {
    if (id == 0) return "SZ";
    if (id == 1) return "SH";
    if (id == 2 || id == 44) return "BJ";
    if (id == 31 || id == 48) return "HK";
    return "M" + std::to_string(id) + "-";
}

Json security_json(int id, const std::string& code,
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

Json source_summary(const Json& source) {
    return jsn_source_metadata(source);
}

bool zero_length_resource(const std::exception& error) {
    return std::string(error.what()).find("zero-length JSN resource") != std::string::npos;
}

Json missing_source(const std::string& resource, const std::string& message) {
    Json value = Json::object();
    value["resource"] = resource;
    value["size"] = 0;
    value["row_count"] = 0;
    value["endpoint"] = Json(nullptr);
    value["attempts"] = 1;
    value["stale"] = false;
    value["age_seconds"] = 0;
    value["upstream_error"] = message;
    value["missing"] = true;
    return value;
}

bool contains(const Json& value, const std::string& needle) {
    if (value.is_string()) return lower_ascii(value.as_string()).find(needle) != std::string::npos;
    if (value.is_number() || value.is_bool())
        return lower_ascii(jsn_scalar_text(value)).find(needle) != std::string::npos;
    if (value.is_array()) for (const auto& child : value.as_array())
        if (contains(child, needle)) return true;
    if (value.is_object()) for (const auto& [key, child] : value.as_object())
        if (lower_ascii(key).find(needle) != std::string::npos || contains(child, needle))
            return true;
    return false;
}

Json filter_rows(const Json& rows, const std::string& query, int limit) {
    Json result = Json::array();
    const auto needle = lower_ascii(trim(query));
    for (const auto& row : rows.as_array()) {
        if (!needle.empty() && !contains(row, needle)) continue;
        if (static_cast<int>(result.size()) >= limit) break;
        result.push_back(row);
    }
    return result;
}

int bounded(const std::string& text, std::string_view name, int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int result = std::stoi(text, &used);
        if (used != text.size() || result < minimum || result > maximum)
            throw std::invalid_argument("range");
        return result;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

void sort_date(Json& rows, bool descending) {
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [descending](const Json& left, const Json& right) {
            const auto l = value_text(left, "date"), r = value_text(right, "date");
            return descending ? l > r : l < r;
        });
}

int date_age_days(const std::string& compact) {
    if (!digits(compact, 8)) return -1;
    std::tm parsed{};
    parsed.tm_year = std::stoi(compact.substr(0, 4)) - 1900;
    parsed.tm_mon = std::stoi(compact.substr(4, 2)) - 1;
    parsed.tm_mday = std::stoi(compact.substr(6, 2));
    parsed.tm_hour = 12;
    const auto then = std::mktime(&parsed);
    if (then == static_cast<std::time_t>(-1)) return -1;
    const auto seconds = std::difftime(std::time(nullptr), then);
    return seconds < 0 ? 0 : static_cast<int>(seconds / 86400.0);
}

std::string snapshot_freshness(const std::string& date) {
    const int age = date_age_days(date);
    return age < 0 ? "unknown" : age <= 14 ? "current-window" : "historical-snapshot";
}

std::string active_channel_suffix(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "2" || value == "sh-northbound") return "2";
    if (value == "4" || value == "sz-northbound") return "4";
    throw Error("active-stock channel must be sh-northbound, sz-northbound, 2, or 4");
}

Json query_cache(bool refreshed, int age) {
    Json value = Json::object();
    value["refreshed"] = refreshed;
    value["age_seconds"] = age;
    return value;
}


} // namespace tdx::leverage_detail

namespace tdx {

LeverageService::LeverageService(BlockData data)
    : securities_(std::move(data.securities)) {}

LeverageService::FetchResult LeverageService::fetch_resource(
    const std::string& resource, bool refresh, int ttl_seconds, int timeout_ms) {
    const auto now = std::time(nullptr);
    for (auto item = resource_cache_.begin(); item != resource_cache_.end();) {
        if (now - item->second.fetched_at >= ttl_seconds) item = resource_cache_.erase(item);
        else ++item;
    }
    for (auto item = failure_cache_.begin(); item != failure_cache_.end();) {
        if (now - item->second.second >= ttl_seconds) item = failure_cache_.erase(item);
        else ++item;
    }
    if (refresh) { resource_cache_.erase(resource); failure_cache_.erase(resource); }
    if (const auto found = resource_cache_.find(resource); found != resource_cache_.end())
        return {found->second.document, false,
                static_cast<int>(std::max<std::time_t>(0, now - found->second.fetched_at))};
    if (const auto found = failure_cache_.find(resource); found != failure_cache_.end())
        throw Error(found->second.first);
    try {
        auto document = fetch_jsn_resource_rows(resource, "bi", timeout_ms);
        resource_cache_[resource] = {document, std::time(nullptr)};
        return {std::move(document), true, 0};
    } catch (const std::exception& error) {
        failure_cache_[resource] = {error.what(), std::time(nullptr)};
        throw;
    }
}


} // namespace tdx
