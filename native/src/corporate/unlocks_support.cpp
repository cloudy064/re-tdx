#include "unlocks_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <tuple>
#include <utility>

namespace fs = std::filesystem;

namespace tdx::unlocks_detail {

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

std::string today_text() {
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

const Json* value_ptr(const Json& object, std::string_view name) {
    if (!object.is_object()) throw Error("unlock row must be an object");
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

void append_unique_text(Json& values, const std::string& value) {
    if (value.empty()) return;
    for (const auto& item : values.as_array())
        if (item.is_string() && item.as_string() == value) return;
    values.push_back(value);
}

bool event_label_matches(const Json& event, std::string_view scalar_name,
                         std::string_view array_name, const std::string& expected) {
    if (expected.empty()) return true;
    if (text_value(event, scalar_name) == expected) return true;
    const auto* values = value_ptr(event, array_name);
    if (!values || !values->is_array()) return false;
    return std::any_of(values->as_array().begin(), values->as_array().end(),
        [&](const Json& value) {
            return value.is_string() && value.as_string() == expected;
        });
}

bool six_digits(const std::string& value) {
    return value.size() == 6 &&
           std::all_of(value.begin(), value.end(), [](char ch) {
               return ch >= '0' && ch <= '9';
           });
}

bool eight_digits(const std::string& value) {
    return value.size() == 8 &&
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

Json source_summary(const Json& document) {
    return jsn_source_metadata(document);
}

Json load_local_resource(const fs::path& root, const std::string& resource) {
    const auto path = root / native_path(resource);
    if (!fs::is_regular_file(path))
        throw Error("local unlock resource is unavailable: " + path_utf8(path));
    const auto tables = load_jsn_tables(path);
    Json rows = Json::array();
    for (std::size_t group = 0; group < tables.size(); ++group) {
        for (const auto& cells : tables[group].rows) {
            Json row = Json::object();
            for (std::size_t column = 0; column < tables[group].headers.size(); ++column)
                row[tables[group].headers[column]] = cells[column];
            row["_group"] = static_cast<std::uint64_t>(group);
            rows.push_back(std::move(row));
        }
    }
    Json result = Json::object();
    result["resource"] = resource;
    result["size"] = static_cast<std::uint64_t>(fs::file_size(path));
    result["row_count"] = static_cast<std::uint64_t>(rows.size());
    result["endpoint"] = "local-jsn:" + path_utf8(path);
    result["rows"] = std::move(rows);
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

Json summarize_events(const Json& events, std::uint64_t raw_rows) {
    Json result = Json::object();
    std::set<std::string> securities;
    std::map<std::string, std::pair<std::uint64_t, double>> reasons;
    std::map<std::string, std::pair<std::uint64_t, double>> dates;
    std::uint64_t implemented = 0, pending = 0, mixed_progress = 0;
    double total_shares = 0.0, total_value = 0.0;
    bool has_value = false;
    std::string first_date, last_date;
    for (const auto& event : events.as_array()) {
        const auto date = text_value(event, "date");
        const auto reason = text_value(event, "reason");
        const auto progress = text_value(event, "progress");
        const double shares = number_value(event, "unlock_shares").value_or(0.0);
        securities.insert(text_value(event.at("security"), "security_id"));
        if (const auto* mixed = value_ptr(event, "mixed_progress");
            mixed && mixed->is_bool() && mixed->as_bool()) ++mixed_progress;
        else if (progress == "实施" || progress == "已解禁") ++implemented;
        else ++pending;
        total_shares += shares;
        auto value = number_value(event, "unlock_market_value");
        if (value) { total_value += *value; has_value = true; }
        ++reasons[reason].first;
        reasons[reason].second += shares;
        ++dates[date].first;
        dates[date].second += shares;
        if (first_date.empty() || date < first_date) first_date = date;
        if (last_date.empty() || date > last_date) last_date = date;
    }
    Json reason_counts = Json::array();
    for (const auto& [label, values] : reasons) {
        Json item = Json::object();
        item["label"] = label;
        item["count"] = values.first;
        item["unlock_shares"] = values.second;
        reason_counts.push_back(std::move(item));
    }
    std::stable_sort(reason_counts.as_array().begin(), reason_counts.as_array().end(),
        [](const Json& left, const Json& right) {
            return left.at("count").as_number() > right.at("count").as_number();
        });
    Json date_counts = Json::array();
    for (const auto& [date, values] : dates) {
        Json item = Json::object();
        item["date"] = date;
        item["count"] = values.first;
        item["unlock_shares"] = values.second;
        date_counts.push_back(std::move(item));
    }
    result["events"] = static_cast<std::uint64_t>(events.size());
    result["raw_rows"] = raw_rows;
    result["unique_securities"] = static_cast<std::uint64_t>(securities.size());
    result["implemented_events"] = implemented;
    result["pending_events"] = pending;
    result["mixed_progress_events"] = mixed_progress;
    result["total_unlock_shares"] = total_shares;
    result["total_unlock_market_value"] = has_value ? Json(total_value) : Json(nullptr);
    result["first_date"] = first_date;
    result["last_date"] = last_date;
    result["reason_counts"] = std::move(reason_counts);
    result["date_counts"] = std::move(date_counts);
    return result;
}

}  // namespace tdx::unlocks_detail
