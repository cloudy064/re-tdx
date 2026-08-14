#include "tdx/fund_statistics.hpp"
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
#include <map>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr const char* kNewFunds = "list/func_jjtj101_1.jsn";
constexpr const char* kDividends = "list/func_jjtj102_1.jsn";
constexpr const char* kPerformance = "list/func_jjtj103_1.jsn";
constexpr const char* kFundMarketSize = "list/func_jjtj104_1.jsn";
constexpr const char* kFundMarketSizeChart = "list/func_jjtj104_2.jsn";
constexpr const char* kEtfMarketSize = "list/func_jjtj105_1.jsn";
constexpr const char* kEtfSubscriptionChart = "list/func_jjtj105_2.jsn";
constexpr const char* kEtfWeekly = "list/func_jjtj108_1.jsn";
constexpr const char* kListedFunds = "list/func_jjtj109_1.jsn";

const std::vector<std::string> kResources{
    kNewFunds, kDividends, kPerformance, kFundMarketSize,
    kFundMarketSizeChart, kEtfMarketSize, kEtfSubscriptionChart,
    kEtfWeekly, kListedFunds};

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

Json scaled(const Json& row, std::string_view name, double scale) {
    const auto value = number_value(row, name);
    return value ? Json(*value * scale) : Json(nullptr);
}

bool digits(const std::string& value, std::size_t count) {
    return value.size() == count &&
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
    } catch (...) {
        return -1;
    }
}

int parsed_market(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "0" || value == "sz") return 0;
    if (value == "1" || value == "sh") return 1;
    if (value == "33" || value == "fund") return 33;
    return -1;
}

std::string market_name(int market) {
    if (market == 0) return "sz";
    if (market == 1) return "sh";
    if (market == 33) return "fund";
    return "m" + std::to_string(market);
}

std::string market_prefix(int market) {
    if (market == 0) return "SZ";
    if (market == 1) return "SH";
    if (market == 33) return "FUND";
    return "M" + std::to_string(market);
}

Json security_document(const Json& row, const std::string& name_field) {
    const auto market = integer_value(row, "$SC");
    const auto code = text_value(row, "$ZQDM");
    if (market < 0 || !digits(code, 6)) return Json(nullptr);
    Json result = Json::object();
    result["market_id"] = market;
    result["market"] = market_name(market);
    result["code"] = code;
    result["security_id"] = market_prefix(market) + code;
    result["name"] = text_value(row, name_field);
    return result;
}

std::string kind_for(const std::string& resource) {
    if (resource == kNewFunds) return "new-funds";
    if (resource == kDividends) return "fund-dividends";
    if (resource == kPerformance) return "equity-fund-performance";
    if (resource == kFundMarketSize) return "fund-market-size";
    if (resource == kFundMarketSizeChart) return "fund-market-size-chart";
    if (resource == kEtfMarketSize) return "etf-market-size";
    if (resource == kEtfSubscriptionChart) return "etf-subscription-chart";
    if (resource == kEtfWeekly) return "etf-weekly";
    if (resource == kListedFunds) return "listed-funds";
    throw Error("unknown fund-statistics resource: " + resource);
}

std::string label_for(const std::string& kind) {
    if (kind == "new-funds") return "新发基金";
    if (kind == "fund-dividends") return "基金分红";
    if (kind == "equity-fund-performance") return "股票型基金收益";
    if (kind == "fund-market-size") return "基金市场规模";
    if (kind == "fund-market-size-chart") return "基金规模走势";
    if (kind == "etf-market-size") return "ETF市场规模";
    if (kind == "etf-subscription-chart") return "ETF申购净量走势";
    if (kind == "etf-weekly") return "A股ETF周度统计";
    return "新上市基金";
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

Json load_local_resource_rows(const fs::path& root,
                              const std::string& resource) {
    const auto path = root / native_path(resource);
    if (!fs::is_regular_file(path))
        throw Error("local fund-statistics resource is unavailable: " +
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
    throw Error("missing fund-statistics resource: " + std::string(resource));
}

Json source_summary(const Json& document, std::size_t normalized_rows) {
    Json result = Json::object();
    for (const auto* name : {"resource", "size", "row_count", "endpoint"})
        result[name] = document.at(name);
    result["normalized_row_count"] =
        static_cast<std::uint64_t>(normalized_rows);
    return result;
}

}  // namespace

Json normalize_fund_statistics_rows(const std::string& resource,
                                    const Json& rows) {
    if (!rows.is_array()) throw Error("fund-statistics rows must be an array");
    const auto kind = kind_for(resource);
    Json result = Json::array();
    std::size_t index = 0;
    for (const auto& row : rows.as_array()) {
        Json item = Json::object();
        item["kind"] = kind;
        item["kind_label"] = label_for(kind);
        item["source_resource"] = resource;
        item["raw"] = row;
        std::string date;

        if (kind == "new-funds") {
            item["security"] = security_document(row, "jjjc");
            date = text_value(row, "fxksr");
            item["offering_start"] = date;
            item["offering_end"] = text_value(row, "fxjzr");
            item["subscription_fee_pct"] = number(row, "rgfl");
            item["minimum_subscription_amount"] = number(row, "rgje");
            item["purchase_fee_pct"] = number(row, "sgfl");
            item["minimum_purchase_units"] = number(row, "sgje");
            item["redemption_fee_pct"] = number(row, "shfl");
            item["minimum_redemption_units"] = number(row, "shje");
            item["fund_class"] = text_value(row, "jjlx");
            item["product_type"] = text_value(row, "tzlx");
            item["tracking_index_code"] = text_value(row, "gzzsdm");
            item["tracking_index_name"] = text_value(row, "gzzsjc");
            item["manager"] = text_value(row, "glr");
            item["fund_managers"] = text_value(row, "jjjl");
        } else if (kind == "fund-dividends") {
            item["security"] = security_document(row, "ZQJC");
            date = text_value(row, "GGRQ");
            item["announcement_date"] = date;
            item["record_date"] = text_value(row, "QYDJR");
            item["ex_dividend_date"] = text_value(row, "CXR");
            item["payment_date"] = text_value(row, "HLPFR");
            item["unit_nav"] = number(row, "DWJZ");
            item["cumulative_nav"] = number(row, "LJJZ");
            item["distribution_ratio_raw"] = number(row, "PXBL");
            item["distribution_description"] = text_value(row, "FHSM");
            item["purchase_fee_pct"] = number(row, "sgfl");
            item["redemption_fee_pct"] = number(row, "shfl");
            item["product_type"] = text_value(row, "tzlx");
            item["manager"] = text_value(row, "glr");
            item["fund_managers"] = text_value(row, "jjjl");
        } else if (kind == "equity-fund-performance") {
            item["security"] = security_document(row, "ZQJC");
            date = text_value(row, "ZXRQ");
            item["snapshot_date"] = date;
            item["unit_nav"] = number(row, "DWJZ");
            item["cumulative_nav"] = number(row, "LJJZ");
            item["return_1m_pct"] = number(row, "JYY");
            item["return_3m_pct"] = number(row, "JSY");
            item["return_6m_pct"] = number(row, "JLY");
            item["return_1y_pct"] = number(row, "JYN");
            item["return_ytd_pct"] = number(row, "JNYL");
            item["purchase_fee_pct"] = number(row, "sgfl");
            item["redemption_fee_pct"] = number(row, "shfl");
            item["product_type"] = text_value(row, "tzlx");
            item["manager"] = text_value(row, "glr");
            item["fund_managers"] = text_value(row, "jjjl");
        } else if (kind == "fund-market-size") {
            item["security"] = Json(nullptr);
            date = text_value(row, "jzrq");
            item["snapshot_date"] = date;
            item["all_fund_count"] = number(row, "qbzs");
            item["fund_company_count"] = number(row, "qbjs");
            item["all_fund_units_100m"] = number(row, "qbfe");
            item["all_fund_units"] = scaled(row, "qbfe", 1e8);
            item["all_fund_nav_100m_yuan"] = number(row, "qbzc");
            item["all_fund_nav_yuan"] = scaled(row, "qbzc", 1e8);
            item["equity_holdings_100m_yuan"] = number(row, "cgsz");
            item["equity_holdings_yuan"] = scaled(row, "cgsz", 1e8);
            item["open_fund_count"] = number(row, "kfzs");
            item["open_fund_units_100m"] = number(row, "kffe");
            item["open_fund_units"] = scaled(row, "kffe", 1e8);
            item["open_fund_nav_100m_yuan"] = number(row, "kfzc");
            item["open_fund_nav_yuan"] = scaled(row, "kfzc", 1e8);
            item["closed_fund_count"] = number(row, "fbzs");
            item["closed_fund_units_100m"] = number(row, "fbfe");
            item["closed_fund_units"] = scaled(row, "fbfe", 1e8);
            item["closed_fund_nav_100m_yuan"] = number(row, "fbzc");
            item["closed_fund_nav_yuan"] = scaled(row, "fbzc", 1e8);
        } else if (kind == "fund-market-size-chart") {
            item["security"] = Json(nullptr);
            date = text_value(row, "rq");
            item["snapshot_date"] = date;
            item["all_fund_units_100m"] = number(row, "data");
            item["all_fund_units"] = scaled(row, "data", 1e8);
        } else if (kind == "etf-market-size") {
            item["security"] = Json(nullptr);
            date = text_value(row, "rq");
            item["snapshot_date"] = date;
            item["sh_market_size_100m_yuan"] = number(row, "shgm");
            item["sh_market_size_yuan"] = scaled(row, "shgm", 1e8);
            item["sz_market_size_100m_yuan"] = number(row, "szgm");
            item["sz_market_size_yuan"] = scaled(row, "szgm", 1e8);
            item["total_market_size_100m_yuan"] = number(row, "hjgm");
            item["total_market_size_yuan"] = scaled(row, "hjgm", 1e8);
            item["sh_net_subscription_100m_units"] = number(row, "shss");
            item["sh_net_subscription_units"] = scaled(row, "shss", 1e8);
            item["sz_net_subscription_100m_units"] = number(row, "szss");
            item["sz_net_subscription_units"] = scaled(row, "szss", 1e8);
            item["total_net_subscription_100m_units"] = number(row, "hjss");
            item["total_net_subscription_units"] = scaled(row, "hjss", 1e8);
            item["sh_composite_close"] = number(row, "szzs");
            item["sh_composite_change_pct"] = number(row, "zdf");
        } else if (kind == "etf-subscription-chart") {
            item["security"] = Json(nullptr);
            date = text_value(row, "rq");
            item["snapshot_date"] = date;
            item["total_net_subscription_100m_units"] = number(row, "data");
            item["total_net_subscription_units"] = scaled(row, "data", 1e8);
        } else if (kind == "etf-weekly") {
            item["security"] = Json(nullptr);
            date = text_value(row, "BZJZRQ");
            item["week_end_date"] = date;
            item["turnover_100m_yuan"] = number(row, "BZCJE");
            item["turnover_yuan"] = scaled(row, "BZCJE", 1e8);
            item["turnover_change_100m_yuan"] = number(row, "CJEBD");
            item["turnover_change_yuan"] = scaled(row, "CJEBD", 1e8);
            item["total_units_100m"] = number(row, "BZZFE");
            item["total_units"] = scaled(row, "BZZFE", 1e8);
            item["total_units_change_100m"] = number(row, "ZFEBD");
            item["total_units_change"] = scaled(row, "ZFEBD", 1e8);
            item["financing_balance_100m_yuan"] = number(row, "ZRZYE");
            item["financing_balance_yuan"] = scaled(row, "ZRZYE", 1e8);
            item["financing_balance_change_100m_yuan"] = number(row, "RZYEBD");
            item["financing_balance_change_yuan"] = scaled(row, "RZYEBD", 1e8);
            item["securities_lending_100m_units"] = number(row, "ZRQYL");
            item["securities_lending_units"] = scaled(row, "ZRQYL", 1e8);
            item["securities_lending_change_100m_units"] = number(row, "RQYLBD");
            item["securities_lending_change_units"] = scaled(row, "RQYLBD", 1e8);
            item["sh_composite_close"] = number(row, "SPJ");
            item["sh_composite_weekly_change_pct"] = number(row, "ZZF");
        } else {
            item["security"] = security_document(row, "jjjc");
            date = text_value(row, "ssrq");
            item["listing_date"] = date;
            item["listing_announcement_date"] = text_value(row, "ssggr");
            item["raised_units_100m"] = number(row, "mjfe");
            item["raised_units"] = scaled(row, "mjfe", 1e8);
            item["listed_tradable_units_100m"] = number(row, "ssjyfe");
            item["listed_tradable_units"] = scaled(row, "ssjyfe", 1e8);
            item["listing_day_nav"] = number(row, "ssdrjz");
            item["holder_households"] = number(row, "cyrhs");
            item["fund_managers"] = text_value(row, "jjjl");
            item["investment_style"] = text_value(row, "tzfg");
            item["manager"] = text_value(row, "glr");
        }

        item["date"] = date;
        const auto* security = field(item, "security");
        const auto security_id = security && !security->is_null()
            ? security->at("security_id").as_string() : std::string("MARKET");
        item["event_id"] = kind + ":" + security_id + ":" + date + ":" +
            std::to_string(index++);
        result.push_back(std::move(item));
    }
    return result;
}

FundStatisticsService::FundStatisticsService(fs::path jsn_root)
    : jsn_root_(std::move(jsn_root)) {}

Json FundStatisticsService::fetch_master(
    const FundStatisticsQuery& options, bool& refreshed, int& age_seconds) {
    const auto now = std::time(nullptr);
    age_seconds = cache_time_
        ? static_cast<int>(std::max<std::time_t>(0, now - cache_time_)) : 0;
    if (!options.refresh && cache_time_ && age_seconds < options.cache_ttl_seconds) {
        refreshed = false;
        return cache_;
    }
    Json documents = Json::array();
    if (!options.refresh && !jsn_root_.empty()) {
        try {
            for (const auto& resource : kResources)
                documents.push_back(load_local_resource_rows(jsn_root_, resource));
        } catch (...) {
            documents = Json::array();
        }
    }
    if (documents.as_array().empty())
        documents = fetch_jsn_resources_rows(kResources, "bi", options.timeout_ms);

    Json records = Json::array();
    Json sources = Json::array();
    for (const auto& resource : kResources) {
        const auto& document = document_for(documents, resource);
        auto normalized = normalize_fund_statistics_rows(
            resource, document.at("rows"));
        sources.push_back(source_summary(document, normalized.size()));
        for (auto& row : normalized.as_array()) records.push_back(std::move(row));
    }
    Json result = Json::object();
    result["records"] = std::move(records);
    result["sources"] = std::move(sources);
    cache_ = result;
    cache_time_ = std::time(nullptr);
    refreshed = true;
    age_seconds = 0;
    return result;
}

Json FundStatisticsService::query(const FundStatisticsQuery& options) {
    const std::set<std::string> views{
        "all", "new-funds", "fund-dividends", "equity-fund-performance",
        "fund-market-size", "fund-market-size-chart", "etf-market-size",
        "etf-subscription-chart", "etf-weekly", "listed-funds"};
    if (!views.count(options.view)) throw Error("unsupported fund-statistics view");
    if (options.limit < 1 || options.limit > 10000)
        throw Error("limit must be in 1..10000");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    int selected_market = -1;
    if (!options.market.empty()) {
        selected_market = parsed_market(options.market);
        if (selected_market < 0)
            throw Error("market must be sz/sh/fund or 0/1/33");
        if (!digits(options.code, 6))
            throw Error("code must contain six digits");
    }

    bool refreshed = false;
    int age_seconds = 0;
    const auto master = fetch_master(options, refreshed, age_seconds);
    const auto needle = lower_ascii(trim(options.query));
    Json records = Json::array();
    for (const auto& row : master.at("records").as_array()) {
        if (options.view != "all" && text_value(row, "kind") != options.view)
            continue;
        if (selected_market >= 0) {
            const auto* security = field(row, "security");
            if (!security || security->is_null()) continue;
            if (static_cast<int>(security->at("market_id").as_number()) !=
                    selected_market ||
                security->at("code").as_string() != options.code) continue;
        }
        if (!needle.empty() &&
            lower_ascii(row.dump(-1)).find(needle) == std::string::npos) continue;
        records.push_back(row);
    }

    std::map<std::string, std::uint64_t> counts;
    std::set<std::string> funds;
    std::string earliest, latest;
    for (const auto& row : records.as_array()) {
        ++counts[text_value(row, "kind")];
        const auto* security = field(row, "security");
        if (security && !security->is_null())
            funds.insert(security->at("security_id").as_string());
        const auto date = text_value(row, "date");
        if (!date.empty()) {
            if (earliest.empty() || date < earliest) earliest = date;
            if (date > latest) latest = date;
        }
    }
    const auto matched = records.size();
    if (static_cast<int>(records.size()) > options.limit)
        records.as_array().resize(static_cast<std::size_t>(options.limit));

    Json summary = Json::object();
    summary["new_funds"] = counts["new-funds"];
    summary["fund_dividends"] = counts["fund-dividends"];
    summary["equity_fund_performance"] = counts["equity-fund-performance"];
    summary["fund_market_size"] = counts["fund-market-size"];
    summary["fund_market_size_chart"] = counts["fund-market-size-chart"];
    summary["etf_market_size"] = counts["etf-market-size"];
    summary["etf_subscription_chart"] = counts["etf-subscription-chart"];
    summary["etf_weekly"] = counts["etf-weekly"];
    summary["listed_funds"] = counts["listed-funds"];
    summary["unique_funds"] = static_cast<std::uint64_t>(funds.size());
    summary["earliest_date"] = earliest;
    summary["latest_date"] = latest;

    Json result = Json::object();
    result["schema"] = "tdx-market-fund-statistics-native-v1";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["mode"] = selected_market < 0 ? "catalog" : "security";
    result["availability"] = matched ? "live" : "empty";
    result["match_count"] = static_cast<std::uint64_t>(matched);
    result["returned"] = static_cast<std::uint64_t>(records.size());
    result["records"] = std::move(records);
    result["summary"] = std::move(summary);
    result["sources"] = master.at("sources");
    result["semantics"] =
        "Nine JJTJ client tables are normalized as fund issuance, dividend, "
        "performance and aggregate market snapshots. Values labelled 亿 are "
        "also exposed in base units; raw rows remain available. OTC market 33 "
        "is retained as fund rather than misclassified as a stock exchange.";
    Json cache = Json::object();
    cache["refreshed"] = refreshed;
    cache["age_seconds"] = age_seconds;
    result["cache"] = std::move(cache);
    return result;
}

int command_market_fund_statistics(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market fund-statistics [options]\n\n"
            "Native JJTJ issuance, dividend, performance and market statistics.\n\n"
            "  --view all|new-funds|fund-dividends|equity-fund-performance\n"
            "         |fund-market-size|fund-market-size-chart|etf-market-size\n"
            "         |etf-subscription-chart|etf-weekly|listed-funds\n"
            "  --query TEXT --market sz|sh|fund --code CODE\n"
            "  --refresh --input-dir PATH --limit N --output PATH --compact\n";
        return 0;
    }
    FundStatisticsQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "all")));
    query.query = trim(args.take_option("--query"));
    query.market = lower_ascii(trim(args.take_option("--market")));
    query.code = trim(args.take_option("--code"));
    query.refresh = args.take_flag("--refresh");
    query.limit = bounded(args.take_option("--limit", "5000"),
                          "--limit", 1, 10000);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 60000);
    const auto input = native_path(args.take_option(
        "--input-dir", "output/tdx-jsn"));
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-fund-statistics-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    FundStatisticsService service(input);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number()
              << " fund-statistics rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
