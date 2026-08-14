#include "options_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/minute.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <unordered_set>

namespace fs = std::filesystem;

namespace tdx::option_detail {

std::mutex option_catalog_mutex;
std::shared_ptr<Json> option_catalog_cache;
std::time_t option_catalog_cache_time{};

std::string upper_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

double parse_number(const std::string& text, std::string_view name,
                    double minimum, double maximum) {
    try {
        std::size_t used = 0;
        const double value = std::stod(trim(text), &used);
        if (used != trim(text).size() || !std::isfinite(value) ||
            value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be a number in the supported range");
    }
}

int parse_integer(const std::string& value, std::string_view name,
                  int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const auto parsed = std::stoll(value, &used);
        if (used != value.size() || parsed < minimum || parsed > maximum)
            throw std::invalid_argument("range");
        return static_cast<int>(parsed);
    } catch (...) {
        throw Error(std::string(name) + " is outside the supported range");
    }
}

int option_market_id(std::string value) {
    value = lower_ascii(trim(value));
    if (value.empty()) return -1;
    if (value == "czce" || value == "cz") return 4;
    if (value == "dce" || value == "dc") return 5;
    if (value == "shfe" || value == "sq") return 6;
    if (value == "cffex" || value == "cf") return 7;
    if (value == "gfex" || value == "gf") return 67;
    return parse_integer(value, "market", 3, 255);
}

std::string option_market_name(int market_id) {
    switch (market_id) {
        case 4: return "CZCE";
        case 5: return "DCE";
        case 6: return "SHFE";
        case 7: return "CFFEX";
        case 67: return "GFEX";
        default: return std::to_string(market_id);
    }
}

int underlying_market(int option_market) {
    switch (option_market) {
        case 4: return 28;
        case 5: return 29;
        case 6: return 30;
        case 67: return 66;
        default: return 47;
    }
}

std::string cffex_underlying(std::string_view contract) {
    if (contract.size() < 2) return {};
    const auto root = std::string(contract.substr(0, 2));
    if (root == "IO") return "IF300";
    if (root == "HO") return "IH50";
    if (root == "CO") return "IC500";
    if (root == "MO") return "IM1000";
    if (root == "ZO") return "IZ100";
    return {};
}

bool legacy_european_commodity(std::string_view contract) {
    std::size_t digits = 0;
    while (digits < contract.size() &&
           std::isalpha(static_cast<unsigned char>(contract[digits]))) ++digits;
    const auto root = std::string(contract.substr(0, digits));
    if (root != "CU" && root != "AU") return false;
    if (digits + 4 > contract.size()) return false;
    try {
        return std::stoi(std::string(contract.substr(digits, 4))) < 2211;
    } catch (...) {
        return false;
    }
}

bool contains_folded(const std::string& haystack, const std::string& needle) {
    return needle.empty() || lower_ascii(haystack).find(lower_ascii(needle)) != std::string::npos;
}

const Json* object_value(const Json& object, std::string_view key) {
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

Json cached_full_option_catalog(bool refresh, int cache_ttl_seconds, int timeout_ms) {
    if (cache_ttl_seconds < 0 || cache_ttl_seconds > 86400)
        throw Error("option catalog cache TTL is outside the supported range");
    std::lock_guard<std::mutex> lock(option_catalog_mutex);
    const auto now = std::time(nullptr);
    if (!refresh && option_catalog_cache &&
        now - option_catalog_cache_time <= cache_ttl_seconds)
        return *option_catalog_cache;
    const auto directory = fetch_expansion_instruments_document(
        0, 200000, -1, {}, timeout_ms);
    auto normalized = normalize_option_catalog_document(directory, -1, {}, {}, "all", {}, 200000);
    option_catalog_cache = std::make_shared<Json>(normalized);
    option_catalog_cache_time = now;
    return normalized;
}

const Json& select_bar(const Json& bars, const std::string& requested_date) {
    if (!bars.is_array() || bars.size() == 0) throw Error("K-line response contains no bars");
    const Json* selected = nullptr;
    const auto date = normalize_date(requested_date);
    for (const auto& bar : bars.as_array()) {
        const auto& bar_date = bar.at("date").as_string();
        if (date.empty() || bar_date <= date) selected = &bar;
    }
    if (!selected) throw Error("no K-line bar exists on or before the requested date");
    return *selected;
}

std::optional<OptionInstrument> find_option_in_catalog(
    const Json& catalog, int market_id, const std::string& wire_code) {
    for (const auto& row : catalog.at("options").as_array()) {
        if (static_cast<int>(row.at("market_id").as_number()) != market_id ||
            row.at("code").as_string() != wire_code) continue;
        return parse_option_instrument(market_id, wire_code, row.at("name").as_string());
    }
    return std::nullopt;
}

double json_number_or(const Json& object, std::string_view key, double fallback) {
    const auto* value = object_value(object, key);
    return value && value->is_number() && std::isfinite(value->as_number())
        ? value->as_number() : fallback;
}

const Json* find_expansion_quote(const Json& batch, int market_id,
                                 const std::string& code) {
    const auto* rows = object_value(batch, "quotes");
    if (!rows || !rows->is_array()) return nullptr;
    for (const auto& quote : rows->as_array())
        if (static_cast<int>(json_number_or(quote, "market_id", -1.0)) == market_id &&
            object_value(quote, "code") && quote.at("code").is_string() &&
            quote.at("code").as_string() == code)
            return &quote;
    return nullptr;
}

double top_of_book(const Json& quote, std::string_view side) {
    const auto* levels = object_value(quote, side);
    if (!levels || !levels->is_array() || levels->size() == 0) return 0.0;
    return json_number_or(levels->as_array().front(), "price");
}

std::string local_calendar_date() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream text;
    text << std::setfill('0') << std::setw(4) << local.tm_year + 1900 << '-'
         << std::setw(2) << local.tm_mon + 1 << '-' << std::setw(2) << local.tm_mday;
    return text.str();
}

}  // namespace tdx::option_detail

namespace tdx {

using namespace option_detail;

std::optional<OptionInstrument> parse_option_instrument(
    int market_id, std::string wire_code, std::string name) {
    static const std::set<int> supported_markets{4, 5, 6, 7, 67};
    if (supported_markets.find(market_id) == supported_markets.end()) return std::nullopt;
    wire_code = upper_ascii(std::move(wire_code));
    name = upper_ascii(trim(std::move(name)));
    auto marker = name.find("-C-");
    bool call = true;
    if (marker == std::string::npos) {
        marker = name.find("-P-");
        call = false;
    }
    if (marker == std::string::npos || marker == 0 || marker + 3 >= name.size())
        return std::nullopt;
    double strike = 0.0;
    try {
        std::size_t used = 0;
        strike = std::stod(name.substr(marker + 3), &used);
        if (used != name.size() - marker - 3 || !std::isfinite(strike) || strike <= 0.0)
            return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
    const auto contract = name.substr(0, marker);
    const bool cffex = market_id == 7;
    const auto underlying_code = cffex ? cffex_underlying(contract) : contract;
    if (underlying_code.empty()) return std::nullopt;
    OptionInstrument option;
    option.market_id = market_id;
    option.wire_code = std::move(wire_code);
    option.name = std::move(name);
    option.contract = contract;
    option.underlying_market_id = underlying_market(market_id);
    option.underlying_code = underlying_code;
    option.call = call;
    option.strike = strike;
    option.american = !cffex && !legacy_european_commodity(contract);
    option.futures_model = !cffex;
    return option;
}

Json option_instrument_document(const OptionInstrument& option) {
    Json row = Json::object();
    row["market_id"] = option.market_id;
    row["market"] = option_market_name(option.market_id);
    row["code"] = option.wire_code;
    row["security"] = std::to_string(option.market_id) + ":" + option.wire_code;
    row["name"] = option.name;
    row["contract"] = option.contract;
    row["type"] = option.call ? "call" : "put";
    row["strike"] = option.strike;
    row["underlying_market_id"] = option.underlying_market_id;
    row["underlying_code"] = option.underlying_code;
    row["underlying_security"] = std::to_string(option.underlying_market_id) + ":" +
                                 option.underlying_code;
    row["exercise_style"] = option.american ? "american" : "european";
    row["pricing_family"] = option.futures_model ? "black_76" : "black_scholes";
    return row;
}

Json normalize_option_catalog_document(
    const Json& instrument_directory, int market_filter,
    const std::string& underlying, const std::string& contract_filter,
    const std::string& type_option, const std::string& query, int limit) {
    if (limit < 1 || limit > 200000) throw Error("option catalog limit is outside the supported range");
    const auto type = lower_ascii(trim(type_option));
    if (type != "all" && type != "call" && type != "put")
        throw Error("option type must be all, call, or put");
    Json rows = Json::array();
    std::size_t matched = 0;
    const auto* source_rows = object_value(instrument_directory, "instruments");
    if (!source_rows) source_rows = object_value(instrument_directory, "options");
    if (!source_rows || !source_rows->is_array())
        throw Error("option catalog source contains neither instruments nor options");
    const auto& instruments = source_rows->as_array();
    for (const auto& instrument : instruments) {
        const int market_id = static_cast<int>(instrument.at("market_id").as_number());
        auto option = parse_option_instrument(market_id, instrument.at("code").as_string(),
                                              instrument.at("name").as_string());
        if (!option || (market_filter >= 0 && option->market_id != market_filter)) continue;
        if (!underlying.empty() && !contains_folded(
                std::to_string(option->underlying_market_id) + ":" + option->underlying_code,
                underlying)) continue;
        if (!contract_filter.empty() && !contains_folded(option->contract, contract_filter)) continue;
        if ((type == "call" && !option->call) || (type == "put" && option->call)) continue;
        if (!query.empty() && !contains_folded(
                option->wire_code + " " + option->name + " " + option->underlying_code,
                query)) continue;
        ++matched;
        if (static_cast<int>(rows.size()) < limit)
            rows.push_back(option_instrument_document(*option));
    }
    Json result = Json::object();
    result["schema"] = "tdx-option-catalog-v1";
    result["source_schema"] = instrument_directory.at("schema");
    if (const auto* total = object_value(instrument_directory, "total"))
        result["reported_directory_total"] = *total;
    else if (const auto* total = object_value(instrument_directory, "reported_directory_total"))
        result["reported_directory_total"] = *total;
    else
        result["reported_directory_total"] = static_cast<std::uint64_t>(instruments.size());
    result["scanned"] = static_cast<std::uint64_t>(instruments.size());
    result["matched"] = static_cast<std::uint64_t>(matched);
    result["returned"] = static_cast<std::uint64_t>(rows.size());
    result["truncated"] = matched > rows.size();
    result["market_filter"] = market_filter < 0 ? Json(nullptr) : Json(market_filter);
    result["underlying_filter"] = underlying;
    result["contract_filter"] = contract_filter;
    result["type_filter"] = type;
    result["query"] = query;
    if (const auto* endpoint = object_value(instrument_directory, "endpoint"))
        result["endpoint"] = *endpoint;
    result["transport"] = "tdx-7727-option-directory-normalized";
    result["options"] = std::move(rows);
    return result;
}

Json fetch_option_catalog_document(
    int market_filter, const std::string& underlying,
    const std::string& contract_filter, const std::string& type,
    const std::string& query, int limit, bool refresh,
    int cache_ttl_seconds, int timeout_ms) {
    auto full = cached_full_option_catalog(refresh, cache_ttl_seconds, timeout_ms);
    return normalize_option_catalog_document(full, market_filter, underlying,
                                             contract_filter, type, query, limit);
}

}  // namespace tdx
