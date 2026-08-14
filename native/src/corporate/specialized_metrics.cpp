#include "tdx/specialized_metrics.hpp"
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

constexpr const char* kBanks = "list/func_hyjyfx101_1.jsn";
constexpr const char* kSecurities = "list/func_hyjyfx102_1.jsn";
constexpr const char* kInsurers = "list/func_hyjyfx103_1.jsn";
const std::vector<std::string> kResources{kBanks, kSecurities, kInsurers};

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

Json percent_from_ratio(const Json& row, std::string_view name) {
    const auto value = number_value(row, name);
    return value ? Json(*value * 100.0) : Json(nullptr);
}

Json growth_percent(const Json& row, std::string_view current,
                    std::string_view prior) {
    const auto left = number_value(row, current);
    const auto right = number_value(row, prior);
    if (!left || !right || *right == 0.0) return Json(nullptr);
    return Json((*left - *right) * 100.0 / std::abs(*right));
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

int parsed_market(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "0" || value == "sz") return 0;
    if (value == "1" || value == "sh") return 1;
    if (value == "2" || value == "44" || value == "bj") return 2;
    return -1;
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
    if (market < 0 || !digits(code, 6)) return Json(nullptr);
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

std::string kind_for(const std::string& resource) {
    if (resource == kBanks) return "banks";
    if (resource == kSecurities) return "securities";
    if (resource == kInsurers) return "insurers";
    throw Error("unknown specialized-metrics resource: " + resource);
}

std::string label_for(const std::string& kind) {
    if (kind == "banks") return "银行经营指标";
    if (kind == "securities") return "券商经营指标";
    return "保险经营指标";
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
        throw Error("local specialized-metrics resource is unavailable: " +
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
    throw Error("missing specialized-metrics resource: " + std::string(resource));
}

Json source_summary(const Json& document, std::size_t normalized_rows) {
    Json result = Json::object();
    for (const auto* name : {"resource", "size", "row_count", "endpoint"})
        result[name] = document.at(name);
    result["normalized_row_count"] = static_cast<std::uint64_t>(normalized_rows);
    return result;
}

void add_number(Json& item, const Json& row, const char* output,
                const char* input) {
    item[output] = number(row, input);
}

void add_ratio(Json& item, const Json& row, const char* output,
               const char* input) {
    item[output] = number(row, input);
    item[std::string(output) + "_pct"] = percent_from_ratio(row, input);
}

}  // namespace

Json normalize_specialized_metrics_rows(
    const std::string& resource, const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("specialized-metrics rows must be an array");
    const auto kind = kind_for(resource);
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto market = integer_value(row, "$SC");
        const auto code = text_value(row, "$ZQDM");
        auto security = security_document(market, code, securities);
        if (security.is_null()) continue;
        Json item = Json::object();
        item["kind"] = kind;
        item["kind_label"] = label_for(kind);
        item["security"] = std::move(security);
        item["source_resource"] = resource;
        item["raw"] = row;
        std::string date;

        if (kind == "banks") {
            date = text_value(row, "date");
            add_number(item, row, "capital_net_yuan", "zbje");
            add_ratio(item, row, "capital_adequacy_ratio", "zbczl");
            add_number(item, row, "core_tier1_capital_net_yuan", "hxzbje");
            add_ratio(item, row, "core_tier1_adequacy_ratio", "hxzbczl");
            add_number(item, row, "tier1_capital_net_yuan", "yjzbje");
            add_ratio(item, row, "tier1_adequacy_ratio", "yjzbczl");
            add_number(item, row, "deposits_yuan", "ckze");
            add_number(item, row, "loans_yuan", "dkze");
            add_ratio(item, row, "loan_to_deposit_ratio", "cdbl");
            add_ratio(item, row, "nonperforming_loan_ratio", "bldkbl");
            add_number(item, row, "loan_loss_reserve_yuan", "dkdzzbj");
            add_ratio(item, row, "provision_coverage_ratio", "bldkbbl");
            add_ratio(item, row, "net_interest_margin_ratio", "jxc");
            add_ratio(item, row, "net_interest_spread_ratio", "jlc");
            add_number(item, row, "average_interest_earning_assets_yuan", "sxzc");
            add_number(item, row, "average_interest_bearing_liabilities_yuan", "jxfz");
            add_number(item, row, "interest_income_yuan", "lxsr");
            add_number(item, row, "interest_expense_yuan", "lxzc");
        } else if (kind == "securities") {
            date = text_value(row, "yf");
            item["report_period"] = text_value(row, "bgq");
            add_number(item, row, "monthly_revenue_yuan", "yysr");
            add_number(item, row, "prior_monthly_revenue_yuan", "snyysr");
            item["monthly_revenue_yoy_pct"] = growth_percent(row, "yysr", "snyysr");
            add_number(item, row, "monthly_net_profit_yuan", "jlr");
            add_number(item, row, "prior_monthly_net_profit_yuan", "snjlr");
            item["monthly_net_profit_yoy_pct"] = growth_percent(row, "jlr", "snjlr");
            add_number(item, row, "net_assets_yuan", "jzc");
            add_number(item, row, "net_capital_yuan", "jzb");
            add_ratio(item, row, "net_capital_to_liabilities_ratio", "jzbfzl");
            add_ratio(item, row, "proprietary_equity_to_net_capital_ratio", "zyqylzb");
            add_ratio(item, row, "proprietary_fixed_income_to_net_capital_ratio", "zygslzb");
            add_number(item, row, "commission_income_yuan", "sxfyj");
            add_number(item, row, "brokerage_income_yuan", "zqjj");
            add_number(item, row, "underwriting_income_yuan", "zqcx");
            add_number(item, row, "asset_management_income_yuan", "stzcgl");
            add_number(item, row, "futures_income_yuan", "qhjj");
            add_number(item, row, "fund_management_income_yuan", "jjgl");
            add_number(item, row, "seat_lease_income_yuan", "xwzl");
            add_number(item, row, "other_fee_income_yuan", "qtsxfsr");
            add_number(item, row, "interest_income_yuan", "lxsr");
            add_number(item, row, "financial_enterprise_flows_yuan", "jrqywl");
            add_number(item, row, "margin_financing_income_yuan", "rzrq");
            add_number(item, row, "repo_income_yuan", "zchg");
            add_number(item, row, "entrusted_asset_scale_yuan", "stglzcgm");
        } else {
            date = text_value(row, "T002");
            add_number(item, row, "embedded_value_yuan", "T003");
            add_number(item, row, "adjusted_net_assets_yuan", "T004");
            add_number(item, row, "in_force_value_before_cost_yuan", "T005");
            add_number(item, row, "in_force_value_after_cost_yuan", "T006");
            add_number(item, row, "new_business_value_before_cost_yuan", "T007");
            add_number(item, row, "new_business_value_after_cost_yuan", "T008");
            add_number(item, row, "core_capital_yuan", "T009");
            add_number(item, row, "actual_capital_yuan", "T010");
            add_number(item, row, "minimum_capital_yuan", "T011");
            add_ratio(item, row, "core_solvency_adequacy_ratio", "T012");
            add_ratio(item, row, "combined_solvency_adequacy_ratio", "T013");
            add_ratio(item, row, "life_market_share_ratio", "T014");
            add_number(item, row, "life_premium_income_yuan", "T015");
            add_ratio(item, row, "persistency_13m_ratio", "T021");
            add_ratio(item, row, "persistency_25m_ratio", "T022");
            add_ratio(item, row, "surrender_ratio", "T023");
            add_ratio(item, row, "property_market_share_ratio", "T024");
            add_number(item, row, "property_premium_income_yuan", "T025");
            add_ratio(item, row, "combined_cost_ratio", "T029");
            add_ratio(item, row, "loss_ratio", "T030");
            add_ratio(item, row, "net_investment_yield_ratio", "T031");
            add_ratio(item, row, "total_investment_yield_ratio", "T032");
            add_number(item, row, "total_investments_yuan", "T033");
            add_number(item, row, "fixed_income_investments_yuan", "T034");
            add_number(item, row, "deposits_investment_yuan", "T035");
            add_number(item, row, "bond_investments_yuan", "T036");
            add_number(item, row, "other_fixed_income_yuan", "T037");
            add_number(item, row, "equity_investments_yuan", "T038");
            add_number(item, row, "stock_investments_yuan", "T039");
            add_number(item, row, "fund_investments_yuan", "T040");
            add_number(item, row, "long_term_equity_investments_yuan", "T041");
            add_number(item, row, "other_equity_investments_yuan", "T042");
            add_number(item, row, "property_investments_yuan", "T043");
            add_number(item, row, "cash_equivalents_yuan", "T044");
            add_number(item, row, "other_financial_assets_yuan", "T045");
        }
        item["date"] = date;
        item["event_id"] = kind + ":" +
            item.at("security").at("security_id").as_string() + ":" + date;
        result.push_back(std::move(item));
    }
    return result;
}

SpecializedMetricsService::SpecializedMetricsService(
    fs::path root,
    std::map<std::pair<int, std::string>, Security> securities,
    fs::path jsn_root)
    : root_(std::move(root)), jsn_root_(std::move(jsn_root)),
      securities_(std::move(securities)) {}

Json SpecializedMetricsService::fetch_master(
    const SpecializedMetricsQuery& options, bool& refreshed, int& age_seconds) {
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
        } catch (...) { documents = Json::array(); }
    }
    if (documents.as_array().empty())
        documents = fetch_jsn_resources_rows(kResources, "bi", options.timeout_ms);

    Json records = Json::array();
    Json sources = Json::array();
    for (const auto& resource : kResources) {
        const auto& document = document_for(documents, resource);
        auto normalized = normalize_specialized_metrics_rows(
            resource, document.at("rows"), securities_);
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

Json SpecializedMetricsService::query(const SpecializedMetricsQuery& options) {
    const std::set<std::string> views{"all", "banks", "securities", "insurers"};
    if (!views.count(options.view)) throw Error("unsupported specialized-metrics view");
    if (options.limit < 1 || options.limit > 10000)
        throw Error("limit must be in 1..10000");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    int selected_market = -1;
    if (!options.market.empty()) {
        selected_market = parsed_market(options.market);
        if (selected_market < 0) throw Error("market must be sz/sh/bj or 0/1/2");
        if (!digits(options.code, 6)) throw Error("code must contain six digits");
    }

    bool refreshed = false;
    int age_seconds = 0;
    const auto master = fetch_master(options, refreshed, age_seconds);
    const auto needle = lower_ascii(trim(options.query));
    Json records = Json::array();
    for (const auto& row : master.at("records").as_array()) {
        if (options.view != "all" && text_value(row, "kind") != options.view) continue;
        if (selected_market >= 0) {
            const auto& security = row.at("security");
            if (static_cast<int>(security.at("market_id").as_number()) != selected_market ||
                security.at("code").as_string() != options.code) continue;
        }
        if (!needle.empty() &&
            lower_ascii(row.dump(-1)).find(needle) == std::string::npos) continue;
        records.push_back(row);
    }

    std::map<std::string, std::uint64_t> counts;
    std::set<std::string> securities;
    std::string earliest, latest;
    for (const auto& row : records.as_array()) {
        ++counts[text_value(row, "kind")];
        securities.insert(row.at("security").at("security_id").as_string());
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
    summary["banks"] = counts["banks"];
    summary["securities"] = counts["securities"];
    summary["insurers"] = counts["insurers"];
    summary["unique_securities"] = static_cast<std::uint64_t>(securities.size());
    summary["earliest_date"] = earliest;
    summary["latest_date"] = latest;

    Json result = Json::object();
    result["schema"] = "tdx-market-specialized-metrics-native-v1";
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
        "HYJYFX bank, securities-company and insurer snapshots are normalized "
        "from the client CFG semantics. Amounts are yuan. Ratio fields preserve "
        "their decimal source value and expose an explicit *_pct companion; raw "
        "rows remain available for audit.";
    Json cache = Json::object();
    cache["refreshed"] = refreshed;
    cache["age_seconds"] = age_seconds;
    result["cache"] = std::move(cache);
    return result;
}

int command_market_specialized_metrics(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market specialized-metrics [options]\n\n"
            "Native bank, securities-company and insurer operating metrics.\n\n"
            "  --view all|banks|securities|insurers\n"
            "  --query TEXT --market sz|sh|bj --code CODE\n"
            "  --refresh --root PATH --input-dir PATH --limit N --output PATH --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    SpecializedMetricsQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "all")));
    query.query = trim(args.take_option("--query"));
    query.market = lower_ascii(trim(args.take_option("--market")));
    query.code = trim(args.take_option("--code"));
    query.refresh = args.take_flag("--refresh");
    query.limit = bounded(args.take_option("--limit", "5000"), "--limit", 1, 10000);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 60000);
    const auto input = native_path(args.take_option("--input-dir", "output/tdx-jsn"));
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-specialized-metrics-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    auto blocks = load_blocks(root, {});
    SpecializedMetricsService service(root, std::move(blocks.securities), input);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number()
              << " specialized-metrics rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
