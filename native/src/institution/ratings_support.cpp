#include "ratings_internal.hpp"
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
#include <optional>
#include <sstream>
#include <utility>

namespace fs = std::filesystem;

namespace tdx::ratings_detail {

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
    if (!object.is_object()) throw Error("rating row must be an object");
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
        const auto result = std::stod(text, &used);
        if (used != text.size() || !std::isfinite(result)) return std::nullopt;
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

Json number_or_null(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

bool digits(const std::string& value, std::size_t size) {
    return value.size() == size &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

std::string upper_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char ch) {
            return ch < 0x80 ? static_cast<char>(std::toupper(ch))
                             : static_cast<char>(ch);
        });
    return value;
}

bool united_states_symbol(const std::string& value) {
    if (value.empty() || value.size() > 24) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isalnum(ch) || ch == '.' || ch == '-' || ch == '^' || ch == '_';
    });
}

int mainland_market_id(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized == "0" || normalized == "sz") return 0;
    if (normalized == "1" || normalized == "sh") return 1;
    if (normalized == "2" || normalized == "44" || normalized == "bj") return 2;
    throw Error("market must be sz, sh, bj, 0, 1, 2, or 44");
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
    return "M" + std::to_string(id) + "-";
}

Json mainland_security_document(
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

Json hong_kong_security_document(
    const std::string& code,
    const std::map<std::string, std::string>& names) {
    const auto found = names.find(code);
    Json result = Json::object();
    result["market"] = "hk";
    result["market_id"] = 31;
    result["code"] = code;
    result["security_id"] = "HK" + code;
    result["name"] = found == names.end() ? "" : found->second;
    result["name_resolved"] = found != names.end();
    return result;
}

Json united_states_security_document(const std::string& code,
                                     const std::string& name) {
    Json result = Json::object();
    result["market"] = "us";
    result["market_id"] = 74;
    result["code"] = code;
    result["security_id"] = "US" + code;
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    return result;
}

Json industry_document(const std::string& code,
                       const std::map<std::string, std::string>& names) {
    const auto found = names.find(code);
    Json result = Json::object();
    result["code"] = code;
    result["industry_id"] = "TDX-RESEARCH-" + code;
    result["name"] = found == names.end() ? "" : found->second;
    result["name_resolved"] = found != names.end();
    return result;
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

Json filtered_rows(const Json& rows, const std::string& query,
                   const std::string& stance, int limit) {
    Json result = Json::array();
    const auto needle = lower_ascii(trim(query));
    for (const auto& row : rows.as_array()) {
        if (stance != "all" && text_value(row, "stance") != stance) continue;
        if (!needle.empty() && !json_contains(row, needle)) continue;
        if (static_cast<int>(result.size()) >= limit) break;
        result.push_back(row);
    }
    return result;
}

const Json& document_for_resource(const Json& documents,
                                  std::string_view resource) {
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("missing rating resource: " + std::string(resource));
}

Json source_summary(const Json& document) {
    return jsn_source_metadata(document);
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

}  // namespace tdx::ratings_detail
