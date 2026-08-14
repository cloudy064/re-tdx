#include "tdx/patent_statistics.hpp"
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

constexpr const char* kResource = "list/func_gszl101_1.jsn";

const Json* field(const Json& row, std::string_view name) {
    if (!row.is_object()) return nullptr;
    const auto found = row.as_object().find(name);
    return found == row.as_object().end() ? nullptr : &found->second;
}

std::string text_value(const Json& row, std::string_view name) {
    const auto* value = field(row, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

std::optional<std::int64_t> count_value(const Json& row, std::string_view name) {
    const auto value = text_value(row, name);
    if (value.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(value, &used);
        if (used != value.size() || !std::isfinite(parsed) || parsed < 0.0)
            return std::nullopt;
        return static_cast<std::int64_t>(std::llround(parsed));
    } catch (...) {
        return std::nullopt;
    }
}

Json count(const Json& row, std::string_view name) {
    const auto value = count_value(row, name);
    return value ? Json(*value) : Json(nullptr);
}

bool digits(const std::string& value, std::size_t size) {
    return value.size() == size && std::all_of(value.begin(), value.end(),
        [](char ch) { return ch >= '0' && ch <= '9'; });
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

void add_count_group(Json& item, const Json& row, std::string_view prefix,
                     const char* invention, const char* utility,
                     const char* design, const char* total) {
    const std::string base(prefix);
    item[base + "_invention"] = count(row, invention);
    item[base + "_utility_model"] = count(row, utility);
    item[base + "_design"] = count(row, design);
    item[base + "_total"] = count(row, total);

    std::int64_t classified = 0;
    bool has_classified = false;
    for (const auto* name : {invention, utility, design}) {
        if (const auto value = count_value(row, name)) {
            classified += *value;
            has_classified = true;
        }
    }
    item[base + "_classified_total"] = has_classified
        ? Json(classified) : Json(nullptr);
    const auto source_total = count_value(row, total);
    item[base + "_total_delta"] = source_total && has_classified
        ? Json(*source_total - classified) : Json(nullptr);
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

Json load_local(const fs::path& root) {
    const auto path = root / native_path(kResource);
    if (!fs::is_regular_file(path))
        throw Error("local patent-statistics resource is unavailable: " + path_utf8(path));
    const auto tables = load_jsn_tables(path);
    Json rows = Json::array();
    for (std::size_t group = 0; group < tables.size(); ++group) {
        for (const auto& cells : tables[group].rows) {
            Json row = Json::object();
            for (std::size_t column = 0; column < tables[group].headers.size(); ++column)
                row[tables[group].headers[column]] = cells[column];
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

double sort_number(const Json& row, std::string_view name) {
    const auto* value = field(row, name);
    return value && value->is_number()
        ? value->as_number() : -std::numeric_limits<double>::infinity();
}

std::string sort_field(const std::string& sort) {
    if (sort == "period-applications") return "period_application_total";
    if (sort == "period-grants") return "period_grant_total";
    if (sort == "cumulative-invention") return "cumulative_grant_invention";
    return "cumulative_grant_total";
}

}  // namespace

Json normalize_patent_statistic_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("patent-statistics rows must be an array");
    Json result = Json::array();
    std::uint64_t rank = 0;
    for (const auto& row : rows.as_array()) {
        ++rank;
        auto security = security_document(integer_value(row, "$SC"),
                                          text_value(row, "$ZQDM"), securities);
        if (security.is_null()) continue;
        Json item = Json::object();
        item["security"] = std::move(security);
        item["report_date"] = text_value(row, "jzrq");
        add_count_group(item, row, "period_application",
                        "bqfm", "bqsy", "bqwg", "bqhj");
        add_count_group(item, row, "period_grant",
                        "bqhdfm", "bqhdsy", "bqhdwg", "bqhdhj");
        add_count_group(item, row, "cumulative_grant",
                        "ljfm", "ljsy", "ljwg", "ljhj");
        item["source_resource"] = kResource;
        item["source_rank"] = rank;
        item["raw"] = row;
        result.push_back(std::move(item));
    }
    return result;
}

PatentStatisticsService::PatentStatisticsService(
    std::map<std::pair<int, std::string>, Security> securities,
    fs::path jsn_root)
    : jsn_root_(std::move(jsn_root)), securities_(std::move(securities)) {}

Json PatentStatisticsService::fetch_master(
    const PatentStatisticsQuery& options, bool& refreshed, int& age_seconds) {
    const auto now = std::time(nullptr);
    age_seconds = cache_time_
        ? static_cast<int>(std::max<std::time_t>(0, now - cache_time_)) : 0;
    if (!options.refresh && cache_time_ && age_seconds < options.cache_ttl_seconds) {
        refreshed = false;
        return cache_;
    }
    Json document;
    if (!options.refresh && !jsn_root_.empty()) {
        try { document = load_local(jsn_root_); } catch (...) {}
    }
    if (!document.is_object())
        document = fetch_jsn_resource_rows(kResource, "bi", options.timeout_ms);
    auto records = normalize_patent_statistic_rows(document.at("rows"), securities_);
    Json source = Json::object();
    for (const auto* name : {"resource", "size", "row_count", "endpoint"})
        source[name] = document.at(name);
    source["normalized_row_count"] = static_cast<std::uint64_t>(records.size());
    Json result = Json::object();
    result["records"] = std::move(records);
    Json sources = Json::array();
    sources.push_back(std::move(source));
    result["sources"] = std::move(sources);
    cache_ = result;
    cache_time_ = std::time(nullptr);
    refreshed = true;
    age_seconds = 0;
    return result;
}

Json PatentStatisticsService::query(const PatentStatisticsQuery& options) {
    const std::set<std::string> sorts{
        "report-date", "period-applications", "period-grants",
        "cumulative-total", "cumulative-invention", "code"};
    if (!sorts.count(options.sort)) throw Error("unsupported patent-statistics sort");
    if (options.order != "asc" && options.order != "desc")
        throw Error("order must be asc or desc");
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
    for (const auto& [value, name] : std::vector<std::pair<std::string, const char*>>{
            {options.report_from, "report_from"}, {options.report_to, "report_to"}})
        if (!value.empty() && !digits(value, 8))
            throw Error(std::string(name) + " must be YYYYMMDD");
    if (!options.report_from.empty() && !options.report_to.empty() &&
        options.report_from > options.report_to)
        throw Error("report_from must not exceed report_to");

    bool refreshed = false;
    int age_seconds = 0;
    const auto master = fetch_master(options, refreshed, age_seconds);
    const auto needle = lower_ascii(trim(options.query));
    Json records = Json::array();
    for (const auto& row : master.at("records").as_array()) {
        const auto& security = row.at("security");
        if (!selected_market.empty() &&
            (security.at("market").as_string() != selected_market ||
             security.at("code").as_string() != options.code)) continue;
        const auto report_date = text_value(row, "report_date");
        if (!options.report_from.empty() && report_date < options.report_from) continue;
        if (!options.report_to.empty() && report_date > options.report_to) continue;
        if (!needle.empty() && lower_ascii(row.dump(-1)).find(needle) == std::string::npos)
            continue;
        auto projected = row;
        if (!options.include_raw) projected.as_object().erase("raw");
        records.push_back(std::move(projected));
    }

    const auto ascending = options.order == "asc";
    std::sort(records.as_array().begin(), records.as_array().end(),
        [&](const Json& left, const Json& right) {
            int comparison = 0;
            if (options.sort == "report-date") {
                comparison = text_value(left, "report_date").compare(
                    text_value(right, "report_date"));
            } else if (options.sort == "code") {
                comparison = left.at("security").at("security_id").as_string().compare(
                    right.at("security").at("security_id").as_string());
            } else {
                const auto name = sort_field(options.sort);
                const auto lv = sort_number(left, name);
                const auto rv = sort_number(right, name);
                comparison = lv < rv ? -1 : lv > rv ? 1 : 0;
            }
            if (!comparison)
                comparison = left.at("security").at("security_id").as_string().compare(
                    right.at("security").at("security_id").as_string());
            return ascending ? comparison < 0 : comparison > 0;
        });

    std::map<std::string, std::uint64_t> markets;
    std::set<std::string> securities;
    std::string first_report, last_report;
    std::uint64_t with_applications = 0, with_grants = 0, negative_deltas = 0;
    for (const auto& row : records.as_array()) {
        const auto& security = row.at("security");
        ++markets[security.at("market").as_string()];
        securities.insert(security.at("security_id").as_string());
        const auto report = text_value(row, "report_date");
        if (!report.empty() && (first_report.empty() || report < first_report)) first_report = report;
        if (!report.empty() && report > last_report) last_report = report;
        if (sort_number(row, "period_application_total") > 0) ++with_applications;
        if (sort_number(row, "period_grant_total") > 0) ++with_grants;
        for (const auto* name : {"period_application_total_delta",
                                 "period_grant_total_delta",
                                 "cumulative_grant_total_delta"})
            if (sort_number(row, name) < 0 && std::isfinite(sort_number(row, name)))
                ++negative_deltas;
    }
    const auto matched = records.size();
    if (static_cast<int>(records.size()) > options.limit)
        records.as_array().resize(static_cast<std::size_t>(options.limit));

    Json summary = Json::object();
    summary["unique_securities"] = static_cast<std::uint64_t>(securities.size());
    summary["sz"] = markets["sz"];
    summary["sh"] = markets["sh"];
    summary["bj"] = markets["bj"];
    summary["first_report_date"] = first_report;
    summary["last_report_date"] = last_report;
    summary["records_with_period_applications"] = with_applications;
    summary["records_with_period_grants"] = with_grants;
    summary["source_total_below_classified_count"] = negative_deltas;

    Json result = Json::object();
    result["schema"] = "tdx-market-patent-statistics-native-v1";
    result["generated_at"] = now_text();
    result["mode"] = selected_market.empty() ? "catalog" : "security";
    result["availability"] = matched ? "live" : "empty";
    result["match_count"] = static_cast<std::uint64_t>(matched);
    result["returned"] = static_cast<std::uint64_t>(records.size());
    result["sort"] = options.sort;
    result["order"] = options.order;
    result["records"] = std::move(records);
    result["summary"] = std::move(summary);
    result["sources"] = master.at("sources");
    result["semantics"] =
        "GSZL company patent applications, period grants and cumulative grants. "
        "Source totals may include patent categories not projected by the client; "
        "classified sums and source-total deltas are exposed without rewriting totals.";
    Json cache = Json::object();
    cache["refreshed"] = refreshed;
    cache["age_seconds"] = age_seconds;
    result["cache"] = std::move(cache);
    return result;
}

int command_market_patent_statistics(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout << "Usage: tdx-tool market patent-statistics [options]\n\n"
            "  --query TEXT --market sz|sh|bj --code CODE\n"
            "  --report-from YYYYMMDD --report-to YYYYMMDD\n"
            "  --sort report-date|period-applications|period-grants|cumulative-total|cumulative-invention|code\n"
            "  --order asc|desc --without-raw --refresh --root PATH\n"
            "  --input-dir PATH --limit N --output PATH --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    PatentStatisticsQuery query;
    query.query = trim(args.take_option("--query"));
    query.market = lower_ascii(trim(args.take_option("--market")));
    query.code = trim(args.take_option("--code"));
    query.report_from = trim(args.take_option("--report-from"));
    query.report_to = trim(args.take_option("--report-to"));
    query.sort = lower_ascii(trim(args.take_option("--sort", "cumulative-total")));
    query.order = lower_ascii(trim(args.take_option("--order", "desc")));
    query.include_raw = !args.take_flag("--without-raw");
    query.refresh = args.take_flag("--refresh");
    query.limit = bounded(args.take_option("--limit", "10000"), "--limit", 1, 20000);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 60000);
    const auto input = native_path(args.take_option("--input-dir", "output/tdx-jsn"));
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-patent-statistics-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    auto blocks = load_blocks(root, {});
    PatentStatisticsService service(std::move(blocks.securities), input);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number()
              << " patent-statistics rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
