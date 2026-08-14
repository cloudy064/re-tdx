#include "tdx/etf_flows.hpp"
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
#include <limits>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr const char* stock_resource = "list/func_etfsg101_1.jsn";
constexpr const char* industry_resource = "list/func_etfsg102_1.jsn";

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
    if (!object.is_object()) throw Error("ETF-flow row must be an object");
    const auto found = object.as_object().find(name);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string text_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

std::optional<double> number_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    if (!value || value->is_null()) return std::nullopt;
    if (value->is_number()) return value->as_number();
    const auto text = trim(jsn_scalar_text(*value));
    if (text.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto result = std::stod(text, &used);
        return used == text.size() && std::isfinite(result)
            ? std::optional<double>(result) : std::nullopt;
    } catch (...) { return std::nullopt; }
}

Json number_json(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

bool digits(const std::string& value, std::size_t size) {
    return value.size() == size &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

int market_id(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "0" || value == "sz") return 0;
    if (value == "1" || value == "sh") return 1;
    if (value == "2" || value == "44" || value == "bj") return 2;
    throw Error("market must be sz, sh, bj, 0, 1, 2, or 44");
}

std::string market_name(int id) {
    return id == 0 ? "sz" : id == 1 ? "sh" : id == 2 ? "bj"
                                                             : std::to_string(id);
}

std::string market_prefix(int id) {
    return id == 0 ? "SZ" : id == 1 ? "SH" : id == 2 ? "BJ"
                                                             : "M" + std::to_string(id);
}

Json entity_document(
    int id, const std::string& code, bool industry,
    const std::map<std::pair<int, std::string>, Security>& securities,
    const std::map<std::string, std::string>& industry_names) {
    Json result = Json::object();
    result["type"] = industry ? "industry" : "security";
    result["market_id"] = id;
    result["market"] = market_name(id);
    result["code"] = code;
    result["security_id"] = market_prefix(id) + code;
    std::string name;
    if (industry) {
        const auto found = industry_names.find(code);
        if (found != industry_names.end()) name = found->second;
    } else {
        const auto found = securities.find({id, code});
        if (found != securities.end()) name = found->second.name;
    }
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    return result;
}

bool contains(const Json& value, const std::string& needle) {
    if (value.is_string())
        return lower_ascii(value.as_string()).find(needle) != std::string::npos;
    if (value.is_number() || value.is_bool())
        return lower_ascii(jsn_scalar_text(value)).find(needle) != std::string::npos;
    if (value.is_array()) for (const auto& child : value.as_array())
        if (contains(child, needle)) return true;
    if (value.is_object()) for (const auto& [key, child] : value.as_object())
        if (lower_ascii(key).find(needle) != std::string::npos || contains(child, needle))
            return true;
    return false;
}

double sort_value(const Json& row, const std::string& sort) {
    const char* field = sort == "weekly-flow" ? "weekly_net_inflow_yuan"
        : sort == "holding-value" ? "holding_market_value_yuan"
        : sort == "etf-count" ? "etf_count"
        : "total_net_inflow_yuan";
    return number_value(row, field).value_or(-std::numeric_limits<double>::infinity());
}

bool direction_matches(const Json& row, const std::string& direction) {
    if (direction == "all") return true;
    const auto flow = number_value(row, "total_net_inflow_yuan").value_or(0.0);
    if (direction == "inflow") return flow > 0.000001;
    if (direction == "outflow") return flow < -0.000001;
    return std::abs(flow) <= 0.000001;
}

Json filtered_rows(Json rows, const EtfFlowQuery& options) {
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            return sort_value(left, options.sort) > sort_value(right, options.sort);
        });
    Json result = Json::array();
    const auto needle = lower_ascii(trim(options.query));
    for (const auto& row : rows.as_array()) {
        if (!direction_matches(row, options.direction)) continue;
        if (!needle.empty() && !contains(row.at("entity"), needle)) continue;
        if (static_cast<int>(result.size()) >= options.limit) break;
        result.push_back(row);
    }
    return result;
}

Json source_summary(const Json& document) {
    Json result = Json::object();
    for (const auto* key : {"resource", "size", "row_count", "endpoint"})
        result[key] = document.at(key);
    return result;
}

Json summary_document(const Json& rows) {
    double holding_shares = 0.0, holding_value = 0.0;
    double broad_flow = 0.0, theme_flow = 0.0, total_flow = 0.0, weekly_flow = 0.0;
    std::uint64_t inflow = 0, outflow = 0, flat = 0, etf_relationships = 0;
    std::set<std::string> dates;
    for (const auto& row : rows.as_array()) {
        holding_shares += number_value(row, "holding_shares").value_or(0.0);
        holding_value += number_value(row, "holding_market_value_yuan").value_or(0.0);
        broad_flow += number_value(row, "broad_net_inflow_yuan").value_or(0.0);
        theme_flow += number_value(row, "theme_net_inflow_yuan").value_or(0.0);
        total_flow += number_value(row, "total_net_inflow_yuan").value_or(0.0);
        weekly_flow += number_value(row, "weekly_net_inflow_yuan").value_or(0.0);
        etf_relationships += static_cast<std::uint64_t>(std::max(
            0.0, number_value(row, "etf_count").value_or(0.0)));
        const auto flow = number_value(row, "total_net_inflow_yuan").value_or(0.0);
        if (flow > 0.000001) ++inflow;
        else if (flow < -0.000001) ++outflow;
        else ++flat;
        dates.insert(text_value(row, "cutoff_date"));
    }
    Json result = Json::object();
    result["rows"] = static_cast<std::uint64_t>(rows.size());
    result["etf_relationships"] = etf_relationships;
    result["inflow"] = inflow;
    result["outflow"] = outflow;
    result["flat"] = flat;
    result["holding_shares"] = holding_shares;
    result["holding_market_value_yuan"] = holding_value;
    result["broad_net_inflow_yuan"] = broad_flow;
    result["theme_net_inflow_yuan"] = theme_flow;
    result["total_net_inflow_yuan"] = total_flow;
    result["weekly_net_inflow_yuan"] = weekly_flow;
    Json values = Json::array();
    for (const auto& date : dates) if (!date.empty()) values.push_back(date);
    result["cutoff_dates"] = std::move(values);
    return result;
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

}  // namespace

Json normalize_etf_flow_rows(
    const Json& rows, bool industry_rows,
    const std::map<std::pair<int, std::string>, Security>& securities,
    const std::map<std::string, std::string>& industry_names) {
    if (!rows.is_array()) throw Error("ETF-flow rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        int id = -1;
        try { id = market_id(text_value(row, "$SC")); }
        catch (...) { continue; }
        const auto code = text_value(row, "$ZQDM");
        if (!digits(code, 6)) continue;
        const auto broad = number_value(row, "kjjlr");
        const auto total = number_value(row, "drsghz");
        const auto turnover = number_value(row, "cje");
        Json item = Json::object();
        item["entity"] = entity_document(
            id, code, industry_rows, securities, industry_names);
        item["cutoff_date"] = text_value(row, "jzrq");
        item["etf_count"] = number_json(number_value(row, "etfzs"));
        item["holding_shares"] = number_json(number_value(row, "etfzcg"));
        item["holding_market_value_yuan"] =
            number_json(number_value(row, "etfcgzsz"));
        item["broad_net_inflow_yuan"] = number_json(broad);
        item["theme_net_inflow_yuan"] = broad && total
            ? Json(*total - *broad) : Json(nullptr);
        item["total_net_inflow_yuan"] = number_json(total);
        item["turnover_yuan"] = number_json(turnover);
        item["net_inflow_share_turnover_pct"] = total && turnover &&
                std::abs(*turnover) > 0.000001
            ? Json(*total * 100.0 / *turnover) : Json(nullptr);
        item["weekly_net_inflow_yuan"] = number_json(number_value(row, "jyzjlr"));
        result.push_back(std::move(item));
    }
    return result;
}

EtfFlowService::EtfFlowService(BlockData data)
    : securities_(std::move(data.securities)) {
    std::set<std::string> first_level;
    for (const auto& block : data.blocks) {
        if (!block.block_code.empty()) industry_names_[block.block_code] = block.name;
        if (block.family == "research-industry" && block.level == 1 &&
            block.block_code.rfind("881", 0) == 0)
            first_level.insert(block.block_code);
    }
    for (const auto& member : data.members)
        if (member.family == "research-industry" && first_level.count(member.block_code))
            security_industries_[{member.market_id, member.code}] = member.block_code;
}

EtfFlowService::FetchResult EtfFlowService::fetch(
    bool industries, const EtfFlowQuery& options) {
    auto& cache = industries ? industry_cache_ : stock_cache_;
    const auto now = std::time(nullptr);
    const int age = cache.fetched_at
        ? static_cast<int>(std::max<std::time_t>(0, now - cache.fetched_at)) : 0;
    if (!options.refresh && cache.fetched_at && age < options.cache_ttl_seconds)
        return {cache.document, false, age};
    const auto source = fetch_jsn_resource_rows(
        industries ? industry_resource : stock_resource, "bi", options.timeout_ms);
    Json document = Json::object();
    document["rows"] = normalize_etf_flow_rows(
        source.at("rows"), industries, securities_, industry_names_);
    document["summary"] = summary_document(document.at("rows"));
    document["source"] = source_summary(source);
    cache = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

Json EtfFlowService::query(const EtfFlowQuery& options) {
    const std::set<std::string> views{"stocks", "industries"};
    if (!views.count(options.view)) throw Error("view must be stocks or industries");
    const std::set<std::string> directions{"all", "inflow", "outflow", "flat"};
    if (!directions.count(options.direction))
        throw Error("direction must be all, inflow, outflow, or flat");
    const std::set<std::string> sorts{
        "total-flow", "weekly-flow", "holding-value", "etf-count"};
    if (!sorts.count(options.sort))
        throw Error("sort must be total-flow, weekly-flow, holding-value, or etf-count");
    if (options.limit < 1 || options.limit > 10000)
        throw Error("limit must be in 1..10000");
    if (options.cache_ttl_seconds < 0 || options.cache_ttl_seconds > 86400)
        throw Error("cache_ttl_seconds must be in 0..86400");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    if (!options.market.empty() && options.view != "stocks")
        throw Error("market/code selection requires stocks view");
    if (!options.industry.empty() && options.view != "industries")
        throw Error("industry selection requires industries view");

    auto stocks = fetch(false, options);
    auto industries = fetch(true, options);
    const bool security_mode = !options.code.empty();
    const bool industry_mode = !options.industry.empty();
    int selected_market = -1;
    if (security_mode) {
        selected_market = market_id(options.market);
        if (!digits(options.code, 6)) throw Error("code must contain six digits");
    }
    if (industry_mode && (!digits(options.industry, 6) ||
        (options.industry.rfind("880", 0) != 0 && options.industry.rfind("881", 0) != 0)))
        throw Error("industry must be a six-digit 880xxx or 881xxx code");

    const auto& base = options.view == "stocks" ? stocks.document : industries.document;
    Json selected = Json(nullptr);
    if (security_mode || industry_mode) {
        for (const auto& row : base.at("rows").as_array()) {
            const auto& entity = row.at("entity");
            if (security_mode &&
                static_cast<int>(entity.at("market_id").as_number()) == selected_market &&
                entity.at("code").as_string() == options.code) {
                selected = row;
                break;
            }
            if (industry_mode && entity.at("code").as_string() == options.industry) {
                selected = row;
                break;
            }
        }
    }

    Json rows;
    if (security_mode || industry_mode) {
        rows = Json::array();
        if (!selected.is_null()) rows.push_back(selected);
    } else rows = filtered_rows(base.at("rows"), options);

    Json related_industry = Json(nullptr);
    if (security_mode) {
        const auto found = security_industries_.find({selected_market, options.code});
        if (found != security_industries_.end())
            for (const auto& row : industries.document.at("rows").as_array())
                if (row.at("entity").at("code").as_string() == found->second) {
                    related_industry = row;
                    break;
                }
    }

    Json result = Json::object();
    result["schema"] = "tdx-market-etf-flows-native-v1";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["mode"] = security_mode ? "security" : industry_mode ? "industry" : "catalog";
    result["found"] = !(security_mode || industry_mode) || !selected.is_null();
    result["rows"] = std::move(rows);
    result["selected"] = std::move(selected);
    result["related_industry"] = std::move(related_industry);
    Json summaries = Json::object();
    summaries["stocks"] = stocks.document.at("summary");
    summaries["industries"] = industries.document.at("summary");
    result["summaries"] = std::move(summaries);
    Json sources = Json::array();
    sources.push_back(stocks.document.at("source"));
    sources.push_back(industries.document.at("source"));
    result["sources"] = std::move(sources);
    Json filters = Json::object();
    filters["direction"] = options.direction;
    filters["sort"] = options.sort;
    filters["query"] = options.query;
    result["filters"] = std::move(filters);
    Json cache = Json::object();
    cache["ttl_seconds"] = options.cache_ttl_seconds;
    cache["stocks_refreshed"] = stocks.refreshed;
    cache["stocks_age_seconds"] = stocks.age_seconds;
    cache["industries_refreshed"] = industries.refreshed;
    cache["industries_age_seconds"] = industries.age_seconds;
    result["cache"] = std::move(cache);
    Json counts = Json::object();
    counts["available_stocks"] =
        static_cast<std::uint64_t>(stocks.document.at("rows").size());
    counts["available_industries"] =
        static_cast<std::uint64_t>(industries.document.at("rows").size());
    counts["returned"] = static_cast<std::uint64_t>(result.at("rows").size());
    result["counts"] = std::move(counts);
    return result;
}

int command_market_etf_flows(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market etf-flows [options]\n\n"
            "Native ETF holdings, subscription/redemption and trading fund flows.\n\n"
            "Options:\n"
            "  --root PATH             TDX installation root\n"
            "  --view NAME             stocks|industries\n"
            "  --direction NAME        all|inflow|outflow|flat\n"
            "  --sort NAME             total-flow|weekly-flow|holding-value|etf-count\n"
            "  --query TEXT            Filter entity name or code\n"
            "  --market sz|sh|bj       Select one stock with --code\n"
            "  --code CODE             Six-digit stock code\n"
            "  --industry CODE         Select one 880xxx/881xxx industry\n"
            "  --limit N               Default 5000\n"
            "  --timeout-ms N          Default 15000\n"
            "  --output PATH           Default output/tdx-market-etf-flows-native.json\n"
            "  --compact               Write compact JSON\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    EtfFlowQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "stocks")));
    query.direction = lower_ascii(trim(args.take_option("--direction", "all")));
    query.sort = lower_ascii(trim(args.take_option("--sort", "total-flow")));
    query.query = trim(args.take_option("--query"));
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.industry = trim(args.take_option("--industry"));
    query.limit = bounded_integer(args.take_option("--limit", "5000"),
                                  "--limit", 1, 10000);
    query.timeout_ms = bounded_integer(args.take_option("--timeout-ms", "15000"),
                                       "--timeout-ms", 100, 60000);
    query.refresh = true;
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-etf-flows-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    EtfFlowService service(load_blocks(root, {"research-industry", "industry"}));
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "completed " << document.at("view").as_string()
              << " ETF-flow query -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
