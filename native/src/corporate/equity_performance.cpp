#include "tdx/equity_performance.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr const char* kResource = "list/func_aghq101.jsn";

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

int bounded(const std::string& value, std::string_view name, int low, int high) {
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

Json load_local_resource(const fs::path& root) {
    const auto path = root / native_path(kResource);
    if (!fs::is_regular_file(path))
        throw Error("local equity-performance resource is unavailable: " +
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
    result["resource"] = kResource;
    result["size"] = static_cast<std::uint64_t>(fs::file_size(path));
    result["row_count"] = static_cast<std::uint64_t>(rows.size());
    result["endpoint"] = "local-jsn:" + path_utf8(path);
    result["rows"] = std::move(rows);
    return result;
}

Json source_summary(const Json& document, std::size_t normalized_rows) {
    Json result = Json::object();
    for (const auto* name : {"resource", "size", "row_count", "endpoint"})
        result[name] = document.at(name);
    result["normalized_row_count"] = static_cast<std::uint64_t>(normalized_rows);
    return result;
}

std::string sort_field(const std::string& sort) {
    if (sort == "return-5d") return "return_5d_pct";
    if (sort == "return-20d") return "return_20d_pct";
    if (sort == "return-60d") return "return_60d_pct";
    if (sort == "month") return "month_to_date_pct";
    if (sort == "ytd") return "year_to_date_pct";
    if (sort == "turnover-day") return "daily_turnover_yuan";
    if (sort == "turnover-5d") return "five_day_turnover_yuan";
    if (sort == "pe") return "pe";
    if (sort == "close") return "close";
    if (sort == "code") return "";
    throw Error("unsupported equity-performance sort");
}

std::optional<double> normalized_number(const Json& row,
                                        const std::string& name) {
    const auto* value = field(row, name);
    if (value && value->is_number() && std::isfinite(value->as_number()))
        return value->as_number();
    return std::nullopt;
}

}  // namespace

Json normalize_equity_performance_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("equity-performance rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        auto security = security_document(integer_value(row, "$SC"),
                                          text_value(row, "$ZQDM"), securities);
        if (security.is_null()) continue;
        Json item = Json::object();
        item["security"] = std::move(security);
        item["quote_date"] = text_value(row, "hqrq");
        item["close"] = number(row, "price0");
        item["prior_month_close"] = number(row, "price1");
        item["daily_turnover_yuan"] = number(row, "cje");
        item["five_day_turnover_yuan"] = number(row, "5cje");
        item["return_5d_pct"] = number(row, "zdf_5d");
        item["return_20d_pct"] = number(row, "zdf_20d");
        item["return_60d_pct"] = number(row, "zdf_60d");
        item["year_to_date_pct"] = number(row, "zdf_ys");
        item["pe"] = number(row, "pe");
        const auto close = number_value(row, "price0");
        const auto prior = number_value(row, "price1");
        item["month_to_date_pct"] = close && prior && *prior != 0.0
            ? Json((*close - *prior) * 100.0 / *prior) : Json(nullptr);
        item["source_resource"] = kResource;
        item["raw"] = row;
        item["record_id"] = item.at("security").at("security_id").as_string() +
            ":" + item.at("quote_date").as_string();
        result.push_back(std::move(item));
    }
    return result;
}

EquityPerformanceService::EquityPerformanceService(
    fs::path root, std::map<std::pair<int, std::string>, Security> securities,
    fs::path jsn_root)
    : root_(std::move(root)), jsn_root_(std::move(jsn_root)),
      securities_(std::move(securities)) {}

Json EquityPerformanceService::fetch_master(
    const EquityPerformanceQuery& options, bool& refreshed, int& age_seconds) {
    const auto now = std::time(nullptr);
    age_seconds = cache_time_
        ? static_cast<int>(std::max<std::time_t>(0, now - cache_time_)) : 0;
    if (!options.refresh && cache_time_ && age_seconds < options.cache_ttl_seconds) {
        refreshed = false;
        return cache_;
    }
    Json document;
    if (!options.refresh && !jsn_root_.empty()) {
        try { document = load_local_resource(jsn_root_); } catch (...) {}
    }
    if (!document.is_object()) {
        const auto documents = fetch_jsn_resources_rows(
            {kResource}, "bi", options.timeout_ms);
        if (!documents.is_array() || documents.as_array().empty())
            throw Error("missing equity-performance resource");
        document = documents.as_array().front();
    }
    auto records = normalize_equity_performance_rows(
        document.at("rows"), securities_);
    Json result = Json::object();
    result["records"] = std::move(records);
    result["source"] = source_summary(document, result.at("records").size());
    cache_ = result;
    cache_time_ = std::time(nullptr);
    refreshed = true;
    age_seconds = 0;
    return result;
}

Json EquityPerformanceService::query(const EquityPerformanceQuery& options) {
    if (options.limit < 1 || options.limit > 20000)
        throw Error("limit must be in 1..20000");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    const auto selected_market = lower_ascii(trim(options.market));
    if (!selected_market.empty() && selected_market != "sz" &&
        selected_market != "sh" && selected_market != "bj")
        throw Error("market must be sz/sh/bj");
    if (!options.code.empty() && !digits(options.code))
        throw Error("code must contain six digits");
    if (options.order != "asc" && options.order != "desc")
        throw Error("order must be asc or desc");
    const auto selected_sort = sort_field(options.sort);

    bool refreshed = false;
    int age_seconds = 0;
    const auto master = fetch_master(options, refreshed, age_seconds);
    const auto needle = lower_ascii(trim(options.query));
    Json records = Json::array();
    for (const auto& row : master.at("records").as_array()) {
        const auto& security = row.at("security");
        if (!selected_market.empty() &&
            (security.at("market").as_string() != selected_market ||
             security.at("code").as_string() != options.code))
            continue;
        if (!needle.empty() &&
            lower_ascii(row.dump(-1)).find(needle) == std::string::npos)
            continue;
        records.push_back(row);
    }

    const bool ascending = options.order == "asc";
    std::sort(records.as_array().begin(), records.as_array().end(),
        [&](const Json& left, const Json& right) {
            const auto lid = left.at("record_id").as_string();
            const auto rid = right.at("record_id").as_string();
            if (selected_sort.empty()) return ascending ? lid < rid : lid > rid;
            const auto lv = normalized_number(left, selected_sort);
            const auto rv = normalized_number(right, selected_sort);
            if (lv && rv && *lv != *rv) return ascending ? *lv < *rv : *lv > *rv;
            if (lv.has_value() != rv.has_value()) return lv.has_value();
            return lid < rid;
        });

    std::map<std::string, std::uint64_t> market_counts;
    std::set<std::string> dates;
    for (const auto& row : records.as_array()) {
        ++market_counts[row.at("security").at("market").as_string()];
        const auto date = text_value(row, "quote_date");
        if (!date.empty()) dates.insert(date);
    }
    const auto matched = records.size();
    if (static_cast<int>(records.size()) > options.limit)
        records.as_array().resize(static_cast<std::size_t>(options.limit));
    if (!options.include_raw)
        for (auto& row : records.as_array()) row.as_object().erase("raw");

    Json summary = Json::object();
    summary["sh"] = market_counts["sh"];
    summary["sz"] = market_counts["sz"];
    summary["bj"] = market_counts["bj"];
    summary["quote_date_count"] = static_cast<std::uint64_t>(dates.size());
    summary["latest_quote_date"] = dates.empty() ? "" : *dates.rbegin();

    Json sources = Json::array();
    sources.push_back(master.at("source"));
    Json result = Json::object();
    result["schema"] = "tdx-market-equity-performance-native-v1";
    result["generated_at"] = now_text();
    result["sort"] = options.sort;
    result["order"] = options.order;
    result["mode"] = selected_market.empty() ? "catalog" : "security";
    result["availability"] = matched ? "live" : "empty";
    result["match_count"] = static_cast<std::uint64_t>(matched);
    result["returned"] = static_cast<std::uint64_t>(records.size());
    result["records"] = std::move(records);
    result["summary"] = std::move(summary);
    result["sources"] = std::move(sources);
    result["semantics"] =
        "AGHQ A-share cross-sectional performance snapshot. Turnover fields are yuan; "
        "5/20/60-day and year-to-date returns are CFG percentage points. Month-to-date "
        "is reproduced from (close-prior-month-close)/prior-month-close. Live price, "
        "daily return, market cap and industry are host runtime columns absent from JSN.";
    Json cache = Json::object();
    cache["refreshed"] = refreshed;
    cache["age_seconds"] = age_seconds;
    result["cache"] = std::move(cache);
    return result;
}

int command_market_equity_performance(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market equity-performance [options]\n\n"
            "Native A-share multi-period performance snapshot.\n\n"
            "  --sort return-5d|return-20d|return-60d|month|ytd|turnover-day|turnover-5d|pe|close|code\n"
            "  --order asc|desc --query TEXT --market sz|sh|bj --code CODE\n"
            "  --without-raw --refresh --root PATH --input-dir PATH --limit N\n"
            "  --output PATH --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    EquityPerformanceQuery query;
    query.sort = lower_ascii(trim(args.take_option("--sort", "return-5d")));
    query.order = lower_ascii(trim(args.take_option("--order", "desc")));
    query.query = trim(args.take_option("--query"));
    query.market = lower_ascii(trim(args.take_option("--market")));
    query.code = trim(args.take_option("--code"));
    query.include_raw = !args.take_flag("--without-raw");
    query.refresh = args.take_flag("--refresh");
    query.limit = bounded(args.take_option("--limit", "10000"),
                          "--limit", 1, 20000);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 60000);
    const auto input = native_path(args.take_option("--input-dir", "output/tdx-jsn"));
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-equity-performance-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    auto blocks = load_blocks(root, {});
    EquityPerformanceService service(root, std::move(blocks.securities), input);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number()
              << " equity-performance rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
