#include "thematic_opportunities_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <set>
#include <sstream>

namespace tdx::detail::thematic_opportunities {

const Json* value_ptr(const Json& value, std::string_view key) {
    if (!value.is_object()) return nullptr;
    const auto exact = value.as_object().find(key);
    if (exact != value.as_object().end()) return &exact->second;
    const auto wanted = lower_ascii(std::string(key));
    for (const auto& [name, child] : value.as_object())
        if (lower_ascii(name) == wanted) return &child;
    return nullptr;
}

std::string text_value(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found ? trim(jsn_scalar_text(*found)) : std::string{};
}

std::optional<double> number_value(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    if (!found || found->is_null()) return std::nullopt;
    if (found->is_number() && std::isfinite(found->as_number()))
        return found->as_number();
    const auto raw = trim(jsn_scalar_text(*found));
    if (raw.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const double parsed = std::stod(raw, &used);
        if (used == raw.size() && std::isfinite(parsed)) return parsed;
    } catch (...) {}
    return std::nullopt;
}

Json number_or_null(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

bool digits(const std::string& value, std::size_t count) {
    return value.size() == count &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

int market_id(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "0" || value == "sz") return 0;
    if (value == "1" || value == "sh") return 1;
    if (value == "2" || value == "44" || value == "bj") return 2;
    throw Error("market must be sz/sh/bj or 0/1/2");
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
    const std::map<std::pair<int, std::string>, Security>& securities,
    const std::string& source_name) {
    const auto found = securities.find({id, code});
    Json result = Json::object();
    result["market"] = market_name(id);
    result["market_id"] = id;
    result["code"] = code;
    result["security_id"] = market_prefix(id) + code;
    result["name"] = found == securities.end() ? source_name : found->second.name;
    result["name_resolved"] = found != securities.end() || !source_name.empty();
    result["name_source"] = found != securities.end() ? "security-directory" :
        !source_name.empty() ? "jsn" : "unresolved";
    return result;
}

std::vector<std::pair<int, std::string>> parse_members(
    const std::string& value, std::size_t* raw_count) {
    std::vector<std::pair<int, std::string>> result;
    std::set<std::pair<int, std::string>> seen;
    std::size_t count = 0, offset = 0;
    while (offset <= value.size()) {
        const auto end = value.find(',', offset);
        const auto token = trim(value.substr(offset,
            end == std::string::npos ? std::string::npos : end - offset));
        if (!token.empty()) {
            const auto separator = token.find('|');
            if (separator == std::string::npos)
                throw Error("opportunity-group member token is missing '|': " + token);
            const auto market = market_id(token.substr(0, separator));
            const auto code = trim(token.substr(separator + 1));
            if (!digits(code, 6))
                throw Error("opportunity-group member code is invalid: " + token);
            ++count;
            if (seen.insert({market, code}).second) result.push_back({market, code});
        }
        if (end == std::string::npos) break;
        offset = end + 1;
    }
    if (raw_count) *raw_count = count;
    return result;
}

const Json& document_for_resource(const Json& documents,
                                  std::string_view resource) {
    if (!documents.is_array()) throw Error("thematic opportunity sources must be an array");
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("thematic opportunity source is missing: " + std::string(resource));
}

Json group_summary(const Json& group) {
    Json result = group;
    result.as_object().erase("members");
    result.as_object().erase("raw");
    return result;
}

bool json_contains(const Json& value, const std::string& needle) {
    if (value.is_string())
        return lower_ascii(value.as_string()).find(needle) != std::string::npos;
    if (value.is_number() || value.is_bool())
        return lower_ascii(jsn_scalar_text(value)).find(needle) != std::string::npos;
    if (value.is_array())
        for (const auto& child : value.as_array())
            if (json_contains(child, needle)) return true;
    if (value.is_object())
        for (const auto& [key, child] : value.as_object())
            if (lower_ascii(key).find(needle) != std::string::npos ||
                json_contains(child, needle)) return true;
    return false;
}

void sort_groups(Json& groups, const std::string& sort_value,
                 const std::string& order_value) {
    const auto sort = lower_ascii(trim(sort_value));
    const auto order = lower_ascii(trim(order_value));
    if (!valid_group_sort(sort)) throw Error("group sort must be name, members, id, or type");
    if (!valid_order(order)) throw Error("order must be asc or desc");
    const bool desc = order == "desc";
    std::stable_sort(groups.as_array().begin(), groups.as_array().end(),
        [&](const Json& left, const Json& right) {
            if (sort == "members") {
                const auto a = left.at("member_count").as_number();
                const auto b = right.at("member_count").as_number();
                if (a != b) return desc ? a > b : a < b;
            } else {
                const auto key = sort == "name" ? "name" :
                    sort == "type" ? "type" : "group_id";
                const auto& a = left.at(key).as_string();
                const auto& b = right.at(key).as_string();
                if (a != b) return desc ? a > b : a < b;
            }
            return left.at("group_id").as_string() < right.at("group_id").as_string();
        });
}

void sort_hype(Json& rows, const std::string& order_value) {
    const auto order = lower_ascii(trim(order_value));
    if (!valid_order(order)) throw Error("order must be asc or desc");
    const bool desc = order == "desc";
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            const auto& a = left.at("start_date").as_string();
            const auto& b = right.at("start_date").as_string();
            if (a != b) return desc ? a > b : a < b;
            return text_value(left, "record_id") < text_value(right, "record_id");
        });
}

Json page(const Json& rows, int offset, int limit) {
    Json result = Json::array();
    for (std::size_t i = static_cast<std::size_t>(offset);
         i < rows.size() && result.size() < static_cast<std::size_t>(limit); ++i)
        result.push_back(rows.as_array()[i]);
    return result;
}

std::string current_time_text() {
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

std::filesystem::path native_utf8_path(const std::string& value) {
#ifdef _WIN32
    return std::filesystem::path(utf8_to_wide(value));
#else
    return std::filesystem::path(value);
#endif
}

}  // namespace tdx::detail::thematic_opportunities
