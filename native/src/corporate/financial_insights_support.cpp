#include "financial_insights_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::detail::financial_insights {

const Json* field(const Json& row, std::string_view name) {
    if (!row.is_object()) return nullptr;
    const auto found = row.as_object().find(name);
    return found == row.as_object().end() ? nullptr : &found->second;
}
std::string text_value(const Json& row, std::string_view name) {
    const auto* value = field(row, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}
std::optional<double> number_value(const Json& row, std::string_view name) {
    const auto value = text_value(row, name);
    if (value.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(value, &used);
        if (used == value.size() && std::isfinite(parsed)) return parsed;
    } catch (...) {}
    return std::nullopt;
}
Json number(const Json& row, std::string_view name) {
    const auto value = number_value(row, name);
    return value ? Json(*value) : Json(nullptr);
}
Json scaled(const Json& row, std::string_view name, double scale) {
    const auto value = number_value(row, name);
    return value ? Json(*value * scale) : Json(nullptr);
}
Json difference(const Json& row, std::string_view left, std::string_view right) {
    const auto l = number_value(row, left), r = number_value(row, right);
    return l && r ? Json(*l - *r) : Json(nullptr);
}
Json change_pct(const Json& row, std::string_view current,
                std::string_view prior) {
    const auto c = number_value(row, current), p = number_value(row, prior);
    return c && p && *p != 0.0 ? Json((*c - *p) * 100.0 / *p) : Json(nullptr);
}
Json ratio_pct(const Json& row, std::string_view numerator,
               std::string_view denominator) {
    const auto n = number_value(row, numerator), d = number_value(row, denominator);
    return n && d && *d != 0.0 ? Json(*n * 100.0 / *d) : Json(nullptr);
}
Json midpoint(const Json& row, std::string_view lower, std::string_view upper) {
    const auto l = number_value(row, lower), u = number_value(row, upper);
    return l && u ? Json((*l + *u) / 2.0) : Json(nullptr);
}
Json change_pct_absolute_base(const Json& row, std::string_view current,
                              std::string_view prior) {
    const auto c = number_value(row, current), p = number_value(row, prior);
    return c && p && *p != 0.0
        ? Json((*c - *p) * 100.0 / std::abs(*p)) : Json(nullptr);
}
bool digits(const std::string& value) {
    return value.size() == 6 &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}
int integer_value(const Json& row, std::string_view name) {
    const auto value = text_value(row, name);
    try {
        std::size_t used = 0;
        const auto parsed = std::stoi(value, &used);
        return used == value.size() ? parsed : -1;
    } catch (...) { return -1; }
}
std::string market_name(int market) {
    if (market == 0) return "sz";
    if (market == 1) return "sh";
    if (market == 2 || market == 44) return "bj";
    return "m" + std::to_string(market);
}
std::string market_prefix(int market) {
    if (market == 0) return "SZ";
    if (market == 1) return "SH";
    if (market == 2 || market == 44) return "BJ";
    return "M" + std::to_string(market);
}
Json security_document(
    int market, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (market == 44) market = 2;
    if (market < 0 || !digits(code)) return Json(nullptr);
    std::string name;
    const auto found = securities.find({market, code});
    if (found != securities.end()) name = found->second.name;
    Json result = Json::object();
    result["market_id"] = market;
    result["market"] = market_name(market);
    result["code"] = code;
    result["security_id"] = market_prefix(market) + code;
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    return result;
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
Json load_local_resource_rows(const fs::path& root, const std::string& resource) {
    const auto path = root / native_utf8_path(resource);
    if (!fs::is_regular_file(path))
        throw Error("local financial-insights resource is unavailable: " + path_utf8(path));
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
const Json& document_for(const Json& documents, std::string_view resource) {
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("missing financial-insights resource: " + std::string(resource));
}
Json source_summary(const Json& document, std::size_t normalized_rows) {
    Json result = Json::object();
    for (const auto* name : {"resource", "size", "row_count", "endpoint"})
        result[name] = document.at(name);
    result["normalized_row_count"] = static_cast<std::uint64_t>(normalized_rows);
    return result;
}
void add_number(Json& item, const Json& row, const char* output, const char* input) {
    item[output] = number(row, input);
}
std::optional<double> normalized_number(const Json& row,
                                        const std::string& name) {
    const auto* value = field(row, name);
    if (value && value->is_number() && std::isfinite(value->as_number()))
        return value->as_number();
    return std::nullopt;
}
void set_signal(Json& item, const Json& signal, const Json& amount, const Json& ratio) {
    item["signal_value"] = signal;
    item["amount_value_yuan"] = amount;
    item["ratio_value_pct"] = ratio;
}

}  // namespace tdx::detail::financial_insights
