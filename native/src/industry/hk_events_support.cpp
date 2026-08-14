#include "hk_events_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::detail {

const Json* json_field(const Json& row, std::string_view name) {
    if (!row.is_object()) return nullptr;
    const auto found = row.as_object().find(name);
    return found == row.as_object().end() ? nullptr : &found->second;
}

std::string json_text(const Json& row, std::string_view name) {
    const auto* value = json_field(row, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

std::optional<double> json_number_value(const Json& row, std::string_view name) {
    const auto value = json_text(row, name);
    if (value.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(value, &used);
        return used == value.size() && std::isfinite(parsed)
            ? std::optional<double>(parsed) : std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

Json json_number(const Json& row, std::string_view name) {
    const auto value = json_number_value(row, name);
    return value ? Json(*value) : Json(nullptr);
}

Json json_scaled_number(const Json& row, std::string_view name, double scale) {
    const auto value = json_number_value(row, name);
    return value ? Json(*value * scale) : Json(nullptr);
}

int hk_market_id(const Json& row) {
    const auto value = json_text(row, "$SC");
    try {
        std::size_t used = 0;
        const int id = std::stoi(value, &used);
        return used == value.size() && (id == 31 || id == 48 || id == 49)
            ? id : -1;
    } catch (...) {
        return -1;
    }
}

bool valid_hk_code(const std::string& code) {
    return code.size() == 5 &&
        std::all_of(code.begin(), code.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

Json hk_security_document(const Json& row) {
    const auto id = hk_market_id(row);
    const auto code = json_text(row, "$ZQDM");
    if (id < 0 || !valid_hk_code(code)) return Json(nullptr);
    Json security = Json::object();
    security["market_id"] = id;
    security["market"] = "hk";
    security["code"] = code;
    security["security_id"] = "HK" + code;
    const auto name = json_text(row, "ZQJC");
    security["name"] = name;
    security["name_resolved"] = !name.empty();
    return security;
}

std::string hk_date_key(std::string value) {
    std::string result;
    for (const auto ch : value)
        if (ch >= '0' && ch <= '9') result.push_back(ch);
    return result.size() >= 8 ? result.substr(0, 8) : result;
}

std::string current_time_text() {
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

fs::path native_utf8_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

Json load_local_resource_rows(const fs::path& root,
                              const std::string& resource) {
    const auto path = root / native_utf8_path(resource);
    if (!fs::is_regular_file(path))
        throw Error("local HK event resource is unavailable: " + path_utf8(path));
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
    result["rows"] = std::move(rows);
    return result;
}

const Json& hk_document_for(const Json& documents, std::string_view resource) {
    for (const auto& document : documents.as_array())
        if (json_text(document, "resource") == resource) return document;
    throw Error("missing HK event resource: " + std::string(resource));
}

Json hk_source_summary(const Json& document) {
    Json result = Json::object();
    for (const auto* name : {"resource", "size", "row_count", "endpoint"})
        result[name] = document.at(name);
    return result;
}

}  // namespace tdx::detail
