#include "tdx/special_attention.hpp"
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

constexpr const char* kEquityDispersion = "list/func_tbgz102_1.jsn";
constexpr const char* kStRisk = "list/func_tbgz103_1.jsn";
constexpr const char* kStarCapRemoval = "list/func_tbgz104_1.jsn";
constexpr const char* kInvestigations = "list/func_tbgz106_1.jsn";
constexpr const char* kGoodwillRisk = "list/func_tbgz110_1.jsn";

const std::vector<std::string> kResources{
    kEquityDispersion, kStRisk, kStarCapRemoval, kInvestigations,
    kGoodwillRisk};

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
    auto name = std::string{};
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
    if (resource == kEquityDispersion) return "equity-dispersion";
    if (resource == kStRisk) return "st-risk";
    if (resource == kStarCapRemoval) return "star-cap-removal";
    if (resource == kInvestigations) return "investigations";
    if (resource == kGoodwillRisk) return "goodwill-risk";
    throw Error("unknown special-attention resource: " + resource);
}

std::string label_for(const std::string& kind) {
    if (kind == "equity-dispersion") return "股权分散";
    if (kind == "st-risk") return "可能成为*ST";
    if (kind == "star-cap-removal") return "摘星摘帽分析";
    if (kind == "investigations") return "被立案调查";
    return "商誉风险";
}

std::string excerpt(const std::string& value, std::size_t limit = 260) {
    if (value.size() <= limit) return value;
    auto end = limit;
    while (end && (static_cast<unsigned char>(value[end]) & 0xc0) == 0x80) --end;
    return value.substr(0, end) + "…";
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
        throw Error("local special-attention resource is unavailable: " +
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
    throw Error("missing special-attention resource: " + std::string(resource));
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

Json normalize_special_attention_rows(
    const std::string& resource, const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array())
        throw Error("special-attention rows must be an array");
    const auto kind = kind_for(resource);
    Json result = Json::array();
    std::size_t index = 0;
    for (const auto& row : rows.as_array()) {
        const bool investigation = kind == "investigations";
        const auto code = text_value(row, investigation ? "$ZQDM1" : "$ZQDM");
        const auto market = integer_value(row, investigation ? "$SC1" : "$SC");
        auto security = security_document(market, code, securities);
        if (security.is_null()) continue;

        Json item = Json::object();
        item["kind"] = kind;
        item["kind_label"] = label_for(kind);
        item["source_resource"] = resource;
        item["security"] = std::move(security);
        item["raw"] = row;
        std::string date;

        if (kind == "equity-dispersion") {
            date = text_value(row, "jzrq");
            item["industry"] = text_value(row, "hy");
            item["region"] = text_value(row, "dq");
            item["largest_shareholder"] = text_value(row, "dgd");
            item["largest_shareholder_pct"] = number(row, "cgbl");
            item["cutoff_date"] = date;
        } else if (kind == "st-risk") {
            date = text_value(row, "bgq");
            item["risk_type"] = text_value(row, "fxlx");
            item["risk_reason"] = text_value(row, "blyy");
            item["reference_close"] = number(row, "price1");
            item["report_period"] = date;
            item["shareholder_households"] = number(row, "zxgdhs");
            item["shareholder_households_10k"] = scaled(row, "zxgdhs", 1e-4);
            item["net_profit_yuan"] = number(row, "jlr");
            item["prior_annual_net_profit_yuan"] = number(row, "jlr1");
            item["net_assets_yuan"] = number(row, "jzc");
            item["revenue_yuan"] = number(row, "yysr");
            item["deducted_net_profit_yuan"] = number(row, "kfjlr");
            item["risk_type_id"] = text_value(row, "ID");
        } else if (kind == "star-cap-removal") {
            date = text_value(row, "date");
            item["report_period"] = text_value(row, "bgq");
            item["current_net_profit_10k_yuan"] = number(row, "jlr1");
            item["current_net_profit_yuan"] = scaled(row, "jlr1", 1e4);
            item["price_to_book_ratio"] = number(row, "mgjzc");
            item["prior_net_profit_10k_yuan"] = number(row, "jlr2");
            item["prior_net_profit_yuan"] = scaled(row, "jlr2", 1e4);
            item["two_years_prior_net_profit_10k_yuan"] = number(row, "jlr3");
            item["two_years_prior_net_profit_yuan"] = scaled(row, "jlr3", 1e4);
            item["status_or_forecast"] = text_value(row, "zxzm");
            item["implementation_date"] = date;
            item["explanation"] = text_value(row, "bkcs");
            item["explanation_excerpt"] = excerpt(text_value(row, "bkcs"));
        } else if (kind == "investigations") {
            date = text_value(row, "blarq");
            item["filing_date"] = date;
            item["filing_close"] = number(row, "price");
            item["reason"] = text_value(row, "YY");
            item["case_detail"] = text_value(row, "fxts");
            item["case_detail_excerpt"] = excerpt(text_value(row, "fxts"));
            item["progress"] = text_value(row, "aqjz");
            item["penalty_date"] = text_value(row, "cfplr");
            item["occurrence_count_10y"] = number(row, "cs");
            item["source_event_key"] = text_value(row, "$ZQDM");
            item["active"] = text_value(row, "cfplr").empty() &&
                text_value(row, "aqjz").find("调查中") != std::string::npos;
        } else {
            date = text_value(row, "bgq");
            item["report_period"] = date;
            item["goodwill_current_yuan"] = number(row, "sy1");
            item["goodwill_prior_yuan"] = number(row, "sy2");
            const auto current = number_value(row, "sy1");
            const auto prior = number_value(row, "sy2");
            item["goodwill_change_yuan"] = current && prior
                ? Json(*current - *prior) : Json(nullptr);
            item["goodwill_change_pct"] = current && prior &&
                    std::abs(*prior) > 1e-12
                ? Json((*current - *prior) * 100.0 / *prior) : Json(nullptr);
            item["goodwill_to_net_profit_pct"] = number(row, "syzjl");
            item["goodwill_to_total_assets_pct"] = number(row, "syzgd");
            item["net_profit_current_yuan"] = number(row, "jlr1");
            item["net_profit_prior_yuan"] = number(row, "jlr2");
            item["net_profit_growth_pct"] = number(row, "jlrzs");
            item["revenue_current_yuan"] = number(row, "ys1");
            item["revenue_prior_yuan"] = number(row, "ys2");
            item["revenue_growth_pct"] = number(row, "yszs");
            item["industry"] = text_value(row, "hy");
        }

        item["date"] = date;
        item["event_id"] = kind + ":" +
            item.at("security").at("security_id").as_string() + ":" + date +
            ":" + std::to_string(index++);
        result.push_back(std::move(item));
    }
    return result;
}

SpecialAttentionService::SpecialAttentionService(
    fs::path root,
    std::map<std::pair<int, std::string>, Security> securities,
    fs::path jsn_root)
    : root_(std::move(root)), jsn_root_(std::move(jsn_root)),
      securities_(std::move(securities)) {}

Json SpecialAttentionService::fetch_master(
    const SpecialAttentionQuery& options, bool& refreshed, int& age_seconds) {
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
        auto normalized = normalize_special_attention_rows(
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

Json SpecialAttentionService::query(const SpecialAttentionQuery& options) {
    const std::set<std::string> views{
        "all", "equity-dispersion", "st-risk", "star-cap-removal",
        "investigations", "goodwill-risk"};
    if (!views.count(options.view))
        throw Error("unsupported special-attention view");
    if (options.limit < 1 || options.limit > 10000)
        throw Error("limit must be in 1..10000");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    int selected_market = -1;
    if (!options.market.empty()) {
        selected_market = parsed_market(options.market);
        if (selected_market < 0)
            throw Error("market must be sz/sh/bj or 0/1/2");
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
            const auto row_market =
                static_cast<int>(security->at("market_id").as_number());
            if (row_market != selected_market ||
                security->at("code").as_string() != options.code) continue;
        }
        if (!needle.empty() &&
            lower_ascii(row.dump(-1)).find(needle) == std::string::npos) continue;
        records.push_back(row);
    }

    std::map<std::string, std::uint64_t> counts;
    std::set<std::string> securities;
    std::string earliest, latest;
    std::uint64_t active_investigations = 0;
    for (const auto& row : records.as_array()) {
        const auto kind = text_value(row, "kind");
        ++counts[kind];
        const auto* security = field(row, "security");
        if (security && !security->is_null())
            securities.insert(security->at("security_id").as_string());
        const auto date = text_value(row, "date");
        if (!date.empty()) {
            if (earliest.empty() || date < earliest) earliest = date;
            if (date > latest) latest = date;
        }
        const auto* active = field(row, "active");
        if (kind == "investigations" && active && active->is_bool() &&
            active->as_bool()) ++active_investigations;
    }
    const auto matched = records.size();
    if (static_cast<int>(records.size()) > options.limit)
        records.as_array().resize(static_cast<std::size_t>(options.limit));

    Json summary = Json::object();
    summary["equity_dispersion"] = counts["equity-dispersion"];
    summary["st_risk"] = counts["st-risk"];
    summary["star_cap_removal"] = counts["star-cap-removal"];
    summary["investigations"] = counts["investigations"];
    summary["goodwill_risk"] = counts["goodwill-risk"];
    summary["active_investigations"] = active_investigations;
    summary["unique_securities"] = static_cast<std::uint64_t>(securities.size());
    summary["earliest_date"] = earliest;
    summary["latest_date"] = latest;

    Json result = Json::object();
    result["schema"] = "tdx-market-special-attention-native-v1";
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
        "Five currently non-empty TBGZ client tables are normalized as public "
        "risk and ownership snapshots, not live quotes or investment advice. "
        "Current-price returns are not fabricated. CFG display conversions are "
        "made explicit while raw rows remain available for audit.";
    Json cache = Json::object();
    cache["refreshed"] = refreshed;
    cache["age_seconds"] = age_seconds;
    result["cache"] = std::move(cache);
    return result;
}

int command_market_special_attention(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market special-attention [options]\n\n"
            "Native TBGZ ownership, ST, investigation and goodwill-risk tables.\n\n"
            "  --view all|equity-dispersion|st-risk|star-cap-removal\n"
            "         |investigations|goodwill-risk\n"
            "  --query TEXT --market sz|sh|bj --code CODE\n"
            "  --refresh --root PATH --input-dir PATH --limit N --output PATH --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    SpecialAttentionQuery query;
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
        "--output", "output/tdx-market-special-attention-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    auto blocks = load_blocks(root, {});
    SpecialAttentionService service(root, std::move(blocks.securities), input);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number()
              << " special-attention rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
