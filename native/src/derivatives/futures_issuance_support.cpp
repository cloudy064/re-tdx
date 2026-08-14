#include "futures_issuance_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::futures_issuance_detail {

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

const Json* value_ptr(const Json& object, std::string_view name) {
    if (!object.is_object()) throw Error("futures/issuance row must be an object");
    const auto found = object.as_object().find(name);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string text_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

Json number_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    if (!value || value->is_null()) return nullptr;
    if (value->is_number()) return value->as_number();
    const auto text = trim(jsn_scalar_text(*value));
    if (text.empty() || text == "--") return nullptr;
    try {
        std::size_t used = 0;
        const double result = std::stod(text, &used);
        return used == text.size() && std::isfinite(result) ? Json(result) : Json(nullptr);
    } catch (...) { return nullptr; }
}

bool digits(const std::string& value, std::size_t count) {
    return value.size() == count && std::all_of(value.begin(), value.end(), [](char ch) {
        return ch >= '0' && ch <= '9';
    });
}

bool safe_key(const std::string& value, std::size_t minimum, std::size_t maximum) {
    return value.size() >= minimum && value.size() <= maximum &&
        std::all_of(value.begin(), value.end(), [](unsigned char ch) {
            return std::isalnum(ch) != 0 || ch == '-' || ch == '_';
        });
}

int market_id(const std::string& value) {
    if (value == "0") return 0;
    if (value == "1") return 1;
    if (value == "2" || value == "44") return 2;
    return -1;
}

std::string market_name(int value) {
    return value == 0 ? "sz" : value == 1 ? "sh" : value == 2 ? "bj" : "";
}

std::string market_prefix(int value) {
    return value == 0 ? "SZ" : value == 1 ? "SH" : value == 2 ? "BJ" : "";
}

std::string normalized_market_id(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized.empty()) return {};
    if (normalized == "sz" || normalized == "0") return "0";
    if (normalized == "sh" || normalized == "1") return "1";
    if (normalized == "bj" || normalized == "2" || normalized == "44") return "2";
    throw Error("market must be sz/sh/bj or 0/1/2");
}

bool contains_folded(const std::string& value, const std::string& needle) {
    return lower_ascii(value).find(lower_ascii(needle)) != std::string::npos;
}


Json security_document(const std::string& market, const std::string& code,
                       const std::map<std::pair<int, std::string>, Security>& securities) {
    const int id = market_id(market);
    Json result = Json::object();
    result["market"] = market_name(id);
    result["market_id"] = id;
    result["code"] = code;
    result["security_id"] = id < 0 ? code : market_prefix(id) + code;
    const auto found = securities.find({id, code});
    result["name"] = found == securities.end() ? "" : found->second.name;
    result["name_resolved"] = found != securities.end();
    return result;
}

const Json& document_for(const Json& documents, std::string_view resource) {
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("missing futures/issuance resource: " + std::string(resource));
}

Json source_summary(const Json& document) {
    Json result = Json::object();
    for (const auto* key : {"resource", "size", "row_count", "endpoint"})
        if (const auto* value = value_ptr(document, key)) result[key] = *value;
    return result;
}

Json limited(Json rows, int limit) {
    if (!rows.is_array()) throw Error("limited value must be an array");
    if (rows.size() <= static_cast<std::size_t>(limit)) return rows;
    Json result = Json::array();
    for (int index = 0; index < limit; ++index)
        result.push_back(rows.as_array()[static_cast<std::size_t>(index)]);
    return result;
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


}  // namespace tdx::futures_issuance_detail
