#include "tdx/financial_screen.hpp"
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

constexpr const char* kShanghaiMain = "list/func_cwzb101_1.jsn";
constexpr const char* kShenzhenMain = "list/func_cwzb102_1.jsn";
constexpr const char* kChinext = "list/func_cwzb104_1.jsn";
constexpr const char* kStar = "list/func_cwzb105_1.jsn";
constexpr const char* kBeijing = "list/func_cwzb107_1.jsn";
constexpr const char* kSmallCapMain = "list/func_xpcz101_1.jsn";
constexpr const char* kSmallCapChinext = "list/func_xpcz103_1.jsn";
constexpr const char* kSmallCapStar = "list/func_xpcz104_1.jsn";
const std::vector<std::string> kResources{
    kShanghaiMain, kShenzhenMain, kChinext, kStar, kBeijing};
const std::vector<std::string> kSmallCapGrowthResources{
    kSmallCapMain, kSmallCapChinext, kSmallCapStar};

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
    } catch (...) { return -1; }
}
std::string board_for(const std::string& resource) {
    if (resource == kShanghaiMain) return "sh-main";
    if (resource == kShenzhenMain) return "sz-main";
    if (resource == kChinext) return "chinext";
    if (resource == kStar) return "star";
    if (resource == kBeijing) return "beijing";
    throw Error("unknown financial-screen resource: " + resource);
}
std::string small_cap_board_for(int market, const std::string& code) {
    if (code.rfind("300", 0) == 0 || code.rfind("301", 0) == 0)
        return "chinext";
    if (code.rfind("688", 0) == 0 || code.rfind("689", 0) == 0)
        return "star";
    return market == 1 ? "sh-main" : "sz-main";
}
std::string board_label(const std::string& board) {
    if (board == "sh-main") return "沪市主板";
    if (board == "sz-main") return "深市主板";
    if (board == "chinext") return "创业板";
    if (board == "star") return "科创板";
    return "北证A股";
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
    int market, const std::string& code, const std::string& source_name,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (market == 44) market = 2;
    if (market < 0 || !digits(code, 6)) return Json(nullptr);
    std::string name = source_name;
    const auto found = securities.find({market, code});
    if (found != securities.end() && !found->second.name.empty())
        name = found->second.name;
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
Json load_local_resource_rows(const fs::path& root, const std::string& resource) {
    const auto path = root / native_path(resource);
    if (!fs::is_regular_file(path))
        throw Error("local financial-screen resource is unavailable: " + path_utf8(path));
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
    throw Error("missing financial-screen resource: " + std::string(resource));
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
std::string sort_field(const std::string& sort, const std::string& dataset) {
    if (dataset == "small-cap-growth") {
        if (sort == "profit-cagr") return "adjusted_net_profit_cagr_3y_pct";
        if (sort == "revenue-cagr") return "revenue_cagr_3y_pct";
        if (sort == "profit-growth") return "adjusted_net_profit_yoy_pct";
        if (sort == "revenue-growth") return "revenue_yoy_pct";
        if (sort == "code") return "";
        throw Error("unsupported small-cap-growth sort");
    }
    if (sort == "market-cap") return "market_cap_yuan";
    if (sort == "pe") return "pe_ttm";
    if (sort == "pb") return "pb_mrq";
    if (sort == "roe") return "roe_pct";
    if (sort == "revenue-growth") return "revenue_yoy_pct";
    if (sort == "profit-growth") return "net_profit_yoy_pct";
    if (sort == "dividend-yield") return "dividend_yield_pct";
    if (sort == "code") return "";
    throw Error("unsupported financial-screen sort");
}
}  // namespace

Json normalize_financial_screen_rows(
    const std::string& resource, const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("financial-screen rows must be an array");
    const auto board = board_for(resource);
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto market = integer_value(row, "$SC");
        const auto code = text_value(row, "$ZQDM");
        auto security = security_document(
            market, code, text_value(row, "ZQJC"), securities);
        if (security.is_null()) continue;
        Json item = Json::object();
        item["dataset"] = "snapshot";
        item["board"] = board;
        item["board_label"] = board_label(board);
        item["security"] = std::move(security);
        item["report_period"] = text_value(row, "BGQ");
        item["dividend_year"] = text_value(row, "FHND");
        const auto cap = number_value(row, "SZ");
        item["market_cap_source_10m_yuan"] = cap ? Json(*cap) : Json(nullptr);
        item["market_cap_yuan"] = cap ? Json(*cap * 10000000.0) : Json(nullptr);
        add_number(item, row, "pe_ttm", "PE");
        add_number(item, row, "pb_mrq", "PB");
        add_number(item, row, "ps_ttm", "PS");
        add_number(item, row, "peg", "PEG");
        add_number(item, row, "debt_ratio_pct", "ZCFZ");
        add_number(item, row, "net_profit_yoy_pct", "TB1");
        add_number(item, row, "adjusted_net_profit_yoy_pct", "TB3");
        add_number(item, row, "revenue_yoy_pct", "TB2");
        add_number(item, row, "contract_liability_yuan", "htfzbq");
        add_number(item, row, "prior_contract_liability_yuan", "htfzsq");
        const auto current = number_value(row, "htfzbq");
        const auto prior = number_value(row, "htfzsq");
        item["contract_liability_yoy_pct"] = current && prior && *prior != 0.0
            ? Json((*current - *prior) * 100.0 / *prior) : Json(nullptr);
        add_number(item, row, "roe_pct", "ROE");
        add_number(item, row, "gross_margin_pct", "XSM");
        add_number(item, row, "rd_to_revenue_pct", "fyzb1");
        add_number(item, row, "selling_to_revenue_pct", "fyzb2");
        add_number(item, row, "admin_to_revenue_pct", "fyzb3");
        add_number(item, row, "finance_to_revenue_pct", "fyzb4");
        add_number(item, row, "inventory_turnover_source", "ZZL1");
        add_number(item, row, "current_asset_turnover_source", "ZZL2");
        add_number(item, row, "institution_float_holding_pct", "JGCC");
        add_number(item, row, "dividend_yield_pct", "GXL");
        item["source_resource"] = resource;
        item["raw"] = row;
        item["record_id"] = board + ":" +
            item.at("security").at("security_id").as_string() + ":" +
            item.at("report_period").as_string();
        result.push_back(std::move(item));
    }
    return result;
}

Json normalize_small_cap_growth_rows(
    const std::string& resource, const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("small-cap-growth rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto market = integer_value(row, "$SC");
        const auto code = text_value(row, "$ZQDM");
        auto security = security_document(
            market, code, text_value(row, "ZQJC"), securities);
        if (security.is_null()) continue;
        const auto board = small_cap_board_for(market, code);
        Json item = Json::object();
        item["dataset"] = "small-cap-growth";
        item["board"] = board;
        item["board_label"] = board_label(board);
        item["security"] = std::move(security);
        item["report_period"] = text_value(row, "bgq");
        item["adjusted_net_profit_yoy_pct"] = number(row, "jlzs1");
        item["adjusted_net_profit_yoy_t_minus_1_pct"] = number(row, "jlzs2");
        item["adjusted_net_profit_yoy_t_minus_2_pct"] = number(row, "jlzs3");
        item["adjusted_net_profit_yoy_t_minus_3_pct"] = number(row, "jlzs4");
        item["adjusted_net_profit_cagr_3y_pct"] = number(row, "jlfh");
        item["revenue_yoy_pct"] = number(row, "yszs1");
        item["revenue_yoy_t_minus_1_pct"] = number(row, "yszs2");
        item["revenue_yoy_t_minus_2_pct"] = number(row, "yszs3");
        item["revenue_yoy_t_minus_3_pct"] = number(row, "yszs4");
        item["revenue_cagr_3y_pct"] = number(row, "ysfh");
        item["source_resource"] = resource;
        item["raw"] = row;
        item["record_id"] = "small-cap-growth:" + board + ":" +
            item.at("security").at("security_id").as_string() + ":" +
            item.at("report_period").as_string();
        result.push_back(std::move(item));
    }
    return result;
}

FinancialScreenService::FinancialScreenService(
    fs::path root, std::map<std::pair<int, std::string>, Security> securities,
    fs::path jsn_root)
    : root_(std::move(root)), jsn_root_(std::move(jsn_root)),
      securities_(std::move(securities)) {}

Json FinancialScreenService::fetch_master(
    const FinancialScreenQuery& options, bool& refreshed, int& age_seconds) {
    const auto now = std::time(nullptr);
    auto& cache = caches_[options.dataset];
    auto& cache_time = cache_times_[options.dataset];
    age_seconds = cache_time
        ? static_cast<int>(std::max<std::time_t>(0, now - cache_time)) : 0;
    if (!options.refresh && cache_time && age_seconds < options.cache_ttl_seconds) {
        refreshed = false;
        return cache;
    }
    const auto& resources = options.dataset == "small-cap-growth"
        ? kSmallCapGrowthResources : kResources;
    Json documents = Json::array();
    if (!options.refresh && !jsn_root_.empty()) {
        try {
            for (const auto& resource : resources)
                documents.push_back(load_local_resource_rows(jsn_root_, resource));
        } catch (...) { documents = Json::array(); }
    }
    if (documents.as_array().empty())
        documents = fetch_jsn_resources_rows(resources, "bi", options.timeout_ms);
    Json records = Json::array();
    Json sources = Json::array();
    std::map<std::string, std::set<std::string>, std::less<>> projection_ids;
    for (const auto& resource : resources) {
        const auto& document = document_for(documents, resource);
        auto normalized = options.dataset == "small-cap-growth"
            ? normalize_small_cap_growth_rows(resource, document.at("rows"), securities_)
            : normalize_financial_screen_rows(resource, document.at("rows"), securities_);
        sources.push_back(source_summary(document, normalized.size()));
        if (options.dataset == "small-cap-growth") {
            auto& ids = projection_ids[resource];
            for (const auto& row : normalized.as_array())
                ids.insert(row.at("security").at("security_id").as_string());
            if (resource != kSmallCapMain) continue;
        }
        for (auto& row : normalized.as_array()) records.push_back(std::move(row));
    }
    Json result = Json::object();
    result["records"] = std::move(records);
    result["sources"] = std::move(sources);
    if (options.dataset == "small-cap-growth") {
        Json reconciliation = Json::object();
        const auto& master = projection_ids[kSmallCapMain];
        for (const auto& [label, resource] : std::vector<std::pair<std::string, std::string>>{
                 {"chinext", kSmallCapChinext}, {"star", kSmallCapStar}}) {
            std::set<std::string> master_board;
            for (const auto& row : result.at("records").as_array())
                if (text_value(row, "board") == label)
                    master_board.insert(row.at("security").at("security_id").as_string());
            Json item = Json::object();
            item["master_count"] = static_cast<std::uint64_t>(master_board.size());
            item["projection_count"] =
                static_cast<std::uint64_t>(projection_ids[resource].size());
            item["exact_match"] = master_board == projection_ids[resource];
            reconciliation[label] = std::move(item);
        }
        reconciliation["master_unique_count"] =
            static_cast<std::uint64_t>(master.size());
        result["projection_reconciliation"] = std::move(reconciliation);
    }
    cache = result;
    cache_time = std::time(nullptr);
    refreshed = true;
    age_seconds = 0;
    return result;
}

Json FinancialScreenService::query(const FinancialScreenQuery& options) {
    if (options.dataset != "snapshot" && options.dataset != "small-cap-growth")
        throw Error("dataset must be snapshot or small-cap-growth");
    const std::set<std::string> views{
        "all", "sh-main", "sz-main", "chinext", "star", "beijing"};
    if (!views.count(options.view)) throw Error("unsupported financial-screen view");
    if (options.limit < 1 || options.limit > 20000)
        throw Error("limit must be in 1..20000");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    const auto selected_market = lower_ascii(trim(options.market));
    if (!selected_market.empty() && selected_market != "sz" &&
        selected_market != "sh" && selected_market != "bj")
        throw Error("market must be sz/sh/bj");
    if (!options.code.empty() && !digits(options.code, 6))
        throw Error("code must contain six digits");
    const auto field_name = sort_field(options.sort, options.dataset);
    if (options.order != "asc" && options.order != "desc")
        throw Error("order must be asc or desc");

    bool refreshed = false;
    int age_seconds = 0;
    const auto master = fetch_master(options, refreshed, age_seconds);
    const auto needle = lower_ascii(trim(options.query));
    Json records = Json::array();
    for (const auto& row : master.at("records").as_array()) {
        if (options.view != "all" && text_value(row, "board") != options.view) continue;
        if (!selected_market.empty()) {
            const auto& security = row.at("security");
            if (security.at("market").as_string() != selected_market ||
                security.at("code").as_string() != options.code) continue;
        }
        if (!needle.empty() && lower_ascii(row.dump(-1)).find(needle) == std::string::npos)
            continue;
        records.push_back(row);
    }
    const bool ascending = options.order == "asc";
    std::sort(records.as_array().begin(), records.as_array().end(),
              [&](const Json& left, const Json& right) {
        if (field_name.empty()) {
            const auto l = left.at("security").at("security_id").as_string();
            const auto r = right.at("security").at("security_id").as_string();
            return ascending ? l < r : l > r;
        }
        const auto lv = number_value(left, field_name);
        const auto rv = number_value(right, field_name);
        if (!lv) return false;
        if (!rv) return true;
        if (*lv != *rv) return ascending ? *lv < *rv : *lv > *rv;
        return left.at("record_id").as_string() < right.at("record_id").as_string();
    });

    std::map<std::string, std::uint64_t> counts;
    std::set<std::string> periods;
    for (const auto& row : records.as_array()) {
        ++counts[text_value(row, "board")];
        const auto period = text_value(row, "report_period");
        if (!period.empty()) periods.insert(period);
    }
    const auto matched = records.size();
    if (static_cast<int>(records.size()) > options.limit)
        records.as_array().resize(static_cast<std::size_t>(options.limit));
    if (!options.include_raw)
        for (auto& row : records.as_array()) row.as_object().erase("raw");

    Json summary = Json::object();
    for (const auto& view : views)
        if (view != "all") summary[view] = counts[view];
    summary["report_period_count"] = static_cast<std::uint64_t>(periods.size());
    summary["latest_report_period"] = periods.empty() ? "" : *periods.rbegin();

    Json result = Json::object();
    result["schema"] = "tdx-market-financial-screen-native-v1";
    result["generated_at"] = now_text();
    result["dataset"] = options.dataset;
    result["view"] = options.view;
    result["sort"] = options.sort;
    result["order"] = options.order;
    result["mode"] = selected_market.empty() ? "catalog" : "security";
    result["availability"] = matched ? "live" : "empty";
    result["match_count"] = static_cast<std::uint64_t>(matched);
    result["returned"] = static_cast<std::uint64_t>(records.size());
    result["records"] = std::move(records);
    result["summary"] = std::move(summary);
    result["sources"] = master.at("sources");
    result["projection_reconciliation"] = options.dataset == "small-cap-growth"
        ? master.at("projection_reconciliation") : Json(nullptr);
    result["semantics"] = options.dataset == "small-cap-growth"
        ? "XPCZ client-curated small-cap growth pool. jlzs1/yszs1 are the current "
          "report-period adjusted-net-profit/revenue YoY percentage points; suffixes "
          "2..4 retain T-1..T-3 annual observations; jlfh/ysfh are the client-provided "
          "three-year compound growth percentage points and are not recalculated."
        : "CWZB cross-sectional financial screen for five active A-share boards. "
          "The source SZ value uses the client formatter's ten-million-yuan unit and "
          "is exposed both raw and as yuan. CFG percentage fields remain percentage "
          "points. Inventory/current-asset turnover values retain neutral source names.";
    Json cache = Json::object();
    cache["refreshed"] = refreshed;
    cache["age_seconds"] = age_seconds;
    result["cache"] = std::move(cache);
    return result;
}

int command_market_financial_screen(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market financial-screen [options]\n\n"
            "Native five-board valuation, growth, quality and dividend screen.\n\n"
            "  --dataset snapshot|small-cap-growth\n"
            "  --view all|sh-main|sz-main|chinext|star|beijing\n"
            "  --sort market-cap|pe|pb|roe|revenue-growth|profit-growth|dividend-yield|profit-cagr|revenue-cagr|code\n"
            "  --order asc|desc --query TEXT --market sz|sh|bj --code CODE\n"
            "  --without-raw --refresh --root PATH --input-dir PATH --limit N\n"
            "  --output PATH --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    FinancialScreenQuery query;
    query.dataset = lower_ascii(trim(args.take_option("--dataset", "snapshot")));
    query.view = lower_ascii(trim(args.take_option("--view", "all")));
    query.sort = lower_ascii(trim(args.take_option(
        "--sort", query.dataset == "small-cap-growth" ? "profit-cagr" : "market-cap")));
    query.order = lower_ascii(trim(args.take_option("--order", "desc")));
    query.query = trim(args.take_option("--query"));
    query.market = lower_ascii(trim(args.take_option("--market")));
    query.code = trim(args.take_option("--code"));
    query.include_raw = !args.take_flag("--without-raw");
    query.refresh = args.take_flag("--refresh");
    query.limit = bounded(args.take_option("--limit", "10000"), "--limit", 1, 20000);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 60000);
    const auto input = native_path(args.take_option("--input-dir", "output/tdx-jsn"));
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-financial-screen-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    auto blocks = load_blocks(root, {});
    FinancialScreenService service(root, std::move(blocks.securities), input);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number()
              << " financial-screen rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
