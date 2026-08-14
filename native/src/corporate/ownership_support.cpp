#include "ownership_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::ownership_detail {
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
    if (!object.is_object()) throw Error("ownership row must be an object");
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
        const auto parsed = std::stod(text, &used);
        if (used != text.size() || !std::isfinite(parsed)) return std::nullopt;
        return parsed;
    } catch (...) {
        return std::nullopt;
    }
}

Json number_or_null(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

Json scaled_number(const Json& row, std::string_view name, double scale) {
    const auto value = number_value(row, name);
    // Every scaled ownership field is converted to its indivisible base unit:
    // shares for 万股 values and yuan for 亿元 values.  Rounding here also
    // prevents binary floating-point tails such as 0.14 * 1e8.
    return value ? Json(std::round(*value * scale)) : Json(nullptr);
}

std::string normalized_multiline(std::string value) {
    for (const auto& [from, to] : std::vector<std::pair<std::string, std::string>>{
             {"\\r\\n", "\n"}, {"\\n", "\n"}, {"\\r", "\n"}}) {
        std::size_t position = 0;
        while ((position = value.find(from, position)) != std::string::npos) {
            value.replace(position, from.size(), to);
            position += to.size();
        }
    }
    return trim(value);
}

std::string labeled_line(const std::string& value, const std::string& label) {
    std::istringstream input(value);
    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.rfind(label, 0) == 0) return trim(line.substr(label.size()));
    }
    return {};
}

bool digits(const std::string& value, std::size_t count) {
    return value.size() == count &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

bool safe_identifier(const std::string& value) {
    return !value.empty() && value.size() <= 64 &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return (ch >= '0' && ch <= '9') ||
                   (ch >= 'a' && ch <= 'z') ||
                   (ch >= 'A' && ch <= 'Z') || ch == '_' || ch == '-';
        });
}

int market_id(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized == "0" || normalized == "sz") return 0;
    if (normalized == "1" || normalized == "sh") return 1;
    if (normalized == "2" || normalized == "44" || normalized == "bj") return 2;
    throw Error("market must be sz, sh, bj, 0, 1, 2, or 44");
}

std::optional<int> row_market_id(const Json& row) {
    const auto value = text_value(row, "$SC");
    if (value.empty()) return std::nullopt;
    try {
        return market_id(value);
    } catch (...) {
        return std::nullopt;
    }
}

std::string market_name(int id) {
    return id == 0 ? "sz" : id == 1 ? "sh" : id == 2 ? "bj"
                                                        : "m" + std::to_string(id);
}

std::string market_prefix(int id) {
    return id == 0 ? "SZ" : id == 1 ? "SH" : id == 2 ? "BJ"
                                                        : "M" + std::to_string(id);
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

const Json& document_for_resource(const Json& documents,
                                  std::string_view resource) {
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("missing ownership resource: " + std::string(resource));
}

Json source_summary(const Json& document) {
    return jsn_source_metadata(document);
}

Json missing_source_summary(const std::string& resource) {
    Json source = Json::object();
    source["resource"] = resource;
    source["endpoint"] = Json(nullptr);
    source["attempts"] = 1;
    source["stale"] = false;
    source["age_seconds"] = 0;
    source["row_count"] = 0;
    source["size"] = 0;
    source["missing"] = true;
    source["upstream_error"] = "server reports a zero-length JSN resource";
    return source;
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

Json select_security(const Json& rows, bool security_mode, int selected_market,
                     const std::string& selected_code) {
    if (!security_mode) return rows;
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto& security = row.at("security");
        if (static_cast<int>(security.at("market_id").as_number()) == selected_market &&
            text_value(security, "code") == selected_code)
            result.push_back(row);
    }
    return result;
}

void append_rows(Json& destination, const Json& source) {
    for (const auto& row : source.as_array()) destination.push_back(row);
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

std::uint64_t unique_security_count(const std::vector<const Json*>& groups) {
    std::set<std::string> values;
    for (const auto* group : groups)
        for (const auto& row : group->as_array())
            values.insert(text_value(row.at("security"), "security_id"));
    return static_cast<std::uint64_t>(values.size());
}

}  // namespace tdx::ownership_detail