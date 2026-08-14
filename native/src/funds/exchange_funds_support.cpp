#include "exchange_funds_internal.hpp"
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
#include <stdexcept>

namespace fs = std::filesystem;

namespace tdx::exchange_fund_detail {

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
        return used == value.size() && std::isfinite(parsed)
            ? std::optional<double>(parsed) : std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

Json number(const Json& row, std::string_view name) {
    const auto value = number_value(row, name);
    return value ? Json(*value) : Json(nullptr);
}

Json change_pct(const std::optional<double>& current,
                const std::optional<double>& reference) {
    if (!current || !reference || std::abs(*reference) < 0.000001)
        return Json(nullptr);
    return Json((*current - *reference) * 100.0 / *reference);
}

bool digits(const std::string& value) {
    return value.size() == 6 &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

int integer_value(const Json& row, std::string_view name, int fallback) {
    const auto value = text_value(row, name);
    try {
        std::size_t used = 0;
        const int parsed = std::stoi(value, &used);
        return used == value.size() ? parsed : fallback;
    } catch (...) {
        return fallback;
    }
}

namespace {

int normalized_market(
    int source_market, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (source_market == 34) {
        const bool in_sz = securities.count({0, code}) != 0;
        const bool in_sh = securities.count({1, code}) != 0;
        if (in_sz != in_sh) return in_sz ? 0 : 1;
        return code.rfind("159", 0) == 0 ? 0 : 1;
    }
    if (source_market == 44) return 2;
    return source_market;
}

std::string market_name(int market) {
    return market == 0 ? "sz" : market == 1 ? "sh" : "bj";
}

std::string market_prefix(int market) {
    return market == 0 ? "SZ" : market == 1 ? "SH" : "BJ";
}

}  // namespace

int parsed_market(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized == "0" || normalized == "sz") return 0;
    if (normalized == "1" || normalized == "sh") return 1;
    if (normalized == "2" || normalized == "44" || normalized == "bj") return 2;
    return -1;
}

Json security_document(
    int source_market, const std::string& code, const std::string& source_name,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    const int market = normalized_market(source_market, code, securities);
    if (market < 0 || market > 2 || !digits(code)) return Json(nullptr);
    const auto found = securities.find({market, code});
    const auto name = !source_name.empty() ? source_name
        : found == securities.end() ? std::string{} : found->second.name;
    Json result = Json::object();
    result["market_id"] = market;
    result["market"] = market_name(market);
    result["code"] = code;
    result["security_id"] = market_prefix(market) + code;
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    return result;
}

Json reference_instrument(const Json& row) {
    const auto code = text_value(row, "$ZQDM1");
    const auto market = integer_value(row, "$SC1");
    if (code.empty() || market < 0) return Json(nullptr);
    Json result = Json::object();
    result["market_id"] = market;
    result["market"] = market == 0 ? "sz" : market == 1 ? "sh" :
        market == 2 || market == 44 ? "bj" : "m" + std::to_string(market);
    result["code"] = code;
    result["security_id"] = "M" + std::to_string(market) + code;
    result["name"] = text_value(row, "ZSJC1");
    result["name_resolved"] = !text_value(row, "ZSJC1").empty();
    return result;
}

std::pair<std::optional<double>, std::optional<double>> price_range(
    const std::string& value) {
    const auto separator = value.find('-');
    if (separator == std::string::npos) return {};
    try {
        std::size_t used_left = 0, used_right = 0;
        const auto left_text = trim(value.substr(0, separator));
        const auto right_text = trim(value.substr(separator + 1));
        const auto left = std::stod(left_text, &used_left);
        const auto right = std::stod(right_text, &used_right);
        if (used_left != left_text.size() || used_right != right_text.size() ||
            !std::isfinite(left) || !std::isfinite(right)) return {};
        return {left, right};
    } catch (...) {
        return {};
    }
}

std::string now_text() {
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

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

Json load_local_resource_rows(const fs::path& root,
                              const std::string& resource) {
    const auto path = root / native_path(resource);
    if (!fs::is_regular_file(path))
        throw Error("local exchange-fund resource is unavailable: " +
                    path_utf8(path));
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
    throw Error("missing exchange-fund resource: " + std::string(resource));
}

Json source_summary(const Json& document, std::size_t normalized_rows) {
    Json result = Json::object();
    for (const auto* name : {"resource", "size", "row_count", "endpoint"})
        result[name] = document.at(name);
    result["normalized_row_count"] =
        static_cast<std::uint64_t>(normalized_rows);
    result["blank_row_count"] = static_cast<std::uint64_t>(
        static_cast<std::size_t>(document.at("row_count").as_number()) -
        std::min(static_cast<std::size_t>(
                     document.at("row_count").as_number()),
                 normalized_rows));
    return result;
}

int bounded(const std::string& value, std::string_view name,
            int low, int high) {
    try {
        std::size_t used = 0;
        const auto parsed = std::stoi(value, &used);
        if (used != value.size() || parsed < low || parsed > high)
            throw std::invalid_argument("range");
        return parsed;
    } catch (...) {
        throw Error(std::string(name) + " must be in " +
                    std::to_string(low) + ".." + std::to_string(high));
    }
}

Json quote_for(const Json& security,
               const std::map<std::pair<int, std::string>, Json>& quotes) {
    if (security.is_null()) return Json(nullptr);
    const auto key = std::make_pair(
        static_cast<int>(security.at("market_id").as_number()),
        security.at("code").as_string());
    const auto found = quotes.find(key);
    return found == quotes.end() ? Json(nullptr) : found->second;
}

}  // namespace tdx::exchange_fund_detail
