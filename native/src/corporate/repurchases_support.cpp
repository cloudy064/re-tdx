#include "repurchases_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::detail::repurchases {

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
    if (!object.is_object()) throw Error("repurchase row must be an object");
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

bool digits(const std::string& value, std::size_t length) {
    return value.size() == length &&
           std::all_of(value.begin(), value.end(), [](char ch) {
               return ch >= '0' && ch <= '9';
           });
}

bool valid_year(const std::string& value) { return digits(value, 4); }

int market_id(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized == "0" || normalized == "sz") return 0;
    if (normalized == "1" || normalized == "sh") return 1;
    if (normalized == "2" || normalized == "44" || normalized == "bj") return 2;
    if (normalized == "31" || normalized == "hk") return 31;
    if (digits(normalized, 2)) return std::stoi(normalized);
    throw Error("market must be sz/sh/bj/hk or a two-digit overseas market");
}

std::string market_name(int id) {
    if (id == 0) return "sz";
    if (id == 1) return "sh";
    if (id == 2) return "bj";
    if (id == 31) return "hk";
    return std::to_string(id);
}

std::string market_prefix(int id) {
    if (id == 0) return "SZ";
    if (id == 1) return "SH";
    if (id == 2) return "BJ";
    if (id == 31) return "HK";
    return "M" + std::to_string(id);
}

Json security_document(
    int id, const std::string& code,
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

const Json& document_for_resource(const Json& documents, std::string_view resource) {
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("missing repurchase resource: " + std::string(resource));
}

Json source_summary(const Json& document) {
    return jsn_source_metadata(document);
}

Json load_local_resource_rows(const fs::path& root, const std::string& resource) {
    const auto path = root / native_path(resource);
    if (!fs::is_regular_file(path))
        throw Error("local repurchase resource is unavailable: " + path_utf8(path));
    const auto tables = load_jsn_tables(path);
    Json rows = Json::array();
    for (std::size_t group = 0; group < tables.size(); ++group) {
        const auto& table = tables[group];
        for (const auto& cells : table.rows) {
            Json row = Json::object();
            for (std::size_t column = 0; column < table.headers.size(); ++column)
                row[table.headers[column]] = cells[column];
            row["_group"] = static_cast<std::uint64_t>(group);
            rows.push_back(std::move(row));
        }
    }
    Json result = Json::object();
    result["resource"] = resource;
    result["size"] = static_cast<std::uint64_t>(fs::file_size(path));
    result["row_count"] = static_cast<std::uint64_t>(rows.size());
    result["endpoint"] = "local-jsn:" + path_utf8(path);
    result["attempts"] = 0;
    result["stale"] = false;
    result["age_seconds"] = 0;
    result["upstream_error"] = Json(nullptr);
    result["rows"] = std::move(rows);
    return result;
}

namespace {

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

}  // namespace

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

Json summarize_plans(const Json& plans) {
    std::set<std::string> securities;
    std::uint64_t complete = 0;
    double planned_amount = 0.0, actual_amount = 0.0, actual_shares = 0.0;
    for (const auto& row : plans.as_array()) {
        securities.insert(text_value(row.at("security"), "security_id"));
        if (row.at("completed").as_bool()) ++complete;
        planned_amount += number_value(row, "planned_amount_upper_yuan").value_or(0.0);
        actual_amount += number_value(row, "actual_amount_yuan").value_or(0.0);
        actual_shares += number_value(row, "actual_shares").value_or(0.0);
    }
    Json result = Json::object();
    result["plans"] = static_cast<std::uint64_t>(plans.size());
    result["unique_securities"] = static_cast<std::uint64_t>(securities.size());
    result["completed_plans"] = complete;
    result["active_plans"] = static_cast<std::uint64_t>(plans.size()) - complete;
    result["planned_amount_upper_yuan"] = planned_amount;
    result["actual_amount_yuan"] = actual_amount;
    result["actual_shares"] = actual_shares;
    result["amount_completion_pct"] = planned_amount > 0.0
        ? Json(actual_amount / planned_amount * 100.0) : Json(nullptr);
    return result;
}

}  // namespace tdx::detail::repurchases
