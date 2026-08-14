#include "forecasts_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace tdx::forecast_detail {

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
    if (!object.is_object()) throw Error("forecast row must be an object");
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

Json scaled_number(const Json& row, std::string_view name, double scale,
                   bool integral) {
    const auto value = number_value(row, name);
    if (!value) return Json(nullptr);
    const auto result = *value * scale;
    return Json(integral ? std::round(result) : result);
}

bool digits(const std::string& value, std::size_t size) {
    return value.size() == size &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

bool industry_code(const std::string& value) {
    return digits(value, 6) && value.rfind("881", 0) == 0;
}

bool forecast_group_code(const std::string& value) {
    return industry_code(value) || value == "880001";
}

bool safe_identifier(const std::string& value) {
    return !value.empty() && value.size() <= 32 &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
        });
}

int mainland_market_id(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized == "0" || normalized == "sz") return 0;
    if (normalized == "1" || normalized == "sh") return 1;
    if (normalized == "2" || normalized == "44" || normalized == "bj") return 2;
    throw Error("market must be sz, sh, bj, 0, 1, 2, or 44");
}

std::optional<int> row_mainland_market(const Json& row) {
    try {
        return mainland_market_id(text_value(row, "$SC"));
    } catch (...) {
        return std::nullopt;
    }
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

Json security_document(int id, const std::string& code,
                       const SecurityMap& securities) {
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

std::string normalized_multiline(std::string value) {
    for (const auto& [from, to] : std::array<std::pair<std::string_view,
                                                       std::string_view>, 3>{
             std::pair{"\\r\\n", "\n"}, {"\\n", "\n"}, {"\\r", "\n"}}) {
        std::size_t position = 0;
        while ((position = value.find(from, position)) != std::string::npos) {
            value.replace(position, from.size(), to);
            position += to.size();
        }
    }
    return trim(value);
}

std::string sentiment(const std::string& type) {
    static constexpr std::array<std::string_view, 5> positive{
        "上升", "预增", "预盈", "扭亏", "减亏"};
    static constexpr std::array<std::string_view, 3> negative{
        "下降", "预降", "预亏"};
    if (std::any_of(positive.begin(), positive.end(), [&](std::string_view word) {
            return type.find(word) != std::string::npos;
        })) return "positive";
    if (std::any_of(negative.begin(), negative.end(), [&](std::string_view word) {
            return type.find(word) != std::string::npos;
        })) return "negative";
    return "uncertain";
}

std::pair<std::string, std::string> split_hong_kong_text(std::string value) {
    value = normalized_multiline(std::move(value));
    constexpr std::string_view content_label = "预告内容：";
    constexpr std::array<std::string_view, 2> reason_labels{
        "变换原因：", "变动原因："};
    std::size_t reason_position = std::string::npos;
    std::string_view reason_label;
    for (const auto label : reason_labels) {
        const auto found = value.find(label);
        if (found != std::string::npos &&
            (reason_position == std::string::npos || found < reason_position)) {
            reason_position = found;
            reason_label = label;
        }
    }
    const bool has_content_label = value.rfind(content_label, 0) == 0;
    auto content = has_content_label ? value.substr(content_label.size()) : value;
    std::string reason;
    if (reason_position != std::string::npos) {
        const auto offset = has_content_label ? content_label.size() : 0;
        content = content.substr(0, reason_position >= offset
            ? reason_position - offset : 0);
        reason = value.substr(reason_position + reason_label.size());
    }
    return {trim(content), trim(reason)};
}

const Json& document_for_resource(const Json& documents,
                                  std::string_view resource) {
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("missing forecast resource: " + std::string(resource));
}

Json source_summary(const Json& document) {
    Json result = Json::object();
    for (const auto* key : {"resource", "size", "row_count", "endpoint"})
        result[key] = document.at(key);
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

Json limited_filtered(const Json& rows, const std::string& query, int limit,
                      const std::string& category) {
    Json result = Json::array();
    const auto needle = lower_ascii(trim(query));
    for (const auto& row : rows.as_array()) {
        if (category != "all" && text_value(row, "sentiment") != category)
            continue;
        if (!needle.empty() && !json_contains(row, needle)) continue;
        if (static_cast<int>(result.size()) >= limit) break;
        result.push_back(row);
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

}  // namespace tdx::forecast_detail
