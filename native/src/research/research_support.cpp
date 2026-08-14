#include "research_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::detail::research {

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
    if (!object.is_object()) throw Error("research row must be an object");
    const auto found = object.as_object().find(name);
    return found == object.as_object().end() ? nullptr : &found->second;
}

Json copy_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    return value ? *value : Json(nullptr);
}

std::string text_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

Json first_value(const Json& object, std::initializer_list<const char*> names) {
    for (const auto* name : names) {
        const auto* value = value_ptr(object, name);
        if (value && !value->is_null() && !jsn_scalar_text(*value).empty()) return *value;
    }
    return Json(nullptr);
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

bool six_digits(const std::string& value) {
    return value.size() == 6 && std::all_of(value.begin(), value.end(), [](char ch) {
        return ch >= '0' && ch <= '9';
    });
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

Json security_document(
    int market_id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    const auto found = securities.find({market_id, code});
    Json result = Json::object();
    result["type"] = "security";
    result["market"] = market_name(market_id);
    result["market_id"] = market_id;
    result["code"] = code;
    result["id"] = market_prefix(market_id) + code;
    result["security_id"] = market_prefix(market_id) + code;
    result["name"] = found == securities.end() ? "" : found->second.name;
    result["name_resolved"] = found != securities.end();
    return result;
}

const Json& document_for_resource(const Json& documents, const std::string& resource) {
    if (!documents.is_array()) throw Error("research documents must be an array");
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("missing research resource: " + resource);
}

const Json& category_document(const Json& master, const std::string& id) {
    for (const auto& category : master.at("categories").as_array())
        if (category.at("id").as_string() == id) return category;
    throw Error("research category is missing from master cache: " + id);
}

Json source_summary(const Json& document) {
    return jsn_source_metadata(document);
}

void map_fields(Json& output, const Json& input,
                std::initializer_list<std::pair<const char*, const char*>> fields) {
    for (const auto& [target, source] : fields) {
        const auto* value = value_ptr(input, source);
        if (value) output[target] = *value;
    }
}

std::pair<std::string, std::string> split_embedded_text(const std::string& value) {
    const auto separator = value.find("TXT:");
    if (separator == std::string::npos) return {trim(value), {}};
    return {trim(value.substr(0, separator)), trim(value.substr(separator + 4))};
}

Json urls_json(const std::string& value) {
    std::vector<std::string> urls;
    std::size_t cursor = 0;
    while (cursor < value.size()) {
        const auto http = value.find("http", cursor);
        if (http == std::string::npos) break;
        std::size_t end = http;
        while (end < value.size()) {
            const char ch = value[end];
            if (ch == '\'' || ch == '\"' || ch == '<' || ch == '>' ||
                ch == ' ' || ch == '\r' || ch == '\n') break;
            ++end;
        }
        if (end > http) urls.push_back(value.substr(http, end - http));
        cursor = std::max(end, http + 4);
    }
    std::sort(urls.begin(), urls.end());
    urls.erase(std::unique(urls.begin(), urls.end()), urls.end());
    Json result = Json::array();
    for (const auto& url : urls) result.push_back(url);
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

Json limited_rows(const Json& rows, int limit) {
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        if (static_cast<int>(result.size()) >= limit) break;
        result.push_back(row);
    }
    return result;
}

bool same_security(const Json& record, int market_id, const std::string& code) {
    const auto& entity = record.at("entity");
    return text_value(entity, "type") == "security" &&
           static_cast<int>(entity.at("market_id").as_number()) == market_id &&
           text_value(entity, "code") == code;
}

std::map<std::string, std::string> block_names(const BlockData& data) {
    std::map<std::string, std::string> result;
    for (const auto& block : data.blocks)
        if (!block.block_code.empty()) result.emplace(block.block_code, block.name);
    return result;
}

}  // namespace tdx::detail::research
