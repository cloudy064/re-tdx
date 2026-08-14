#include "tdx/overview_factors.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>

namespace fs = std::filesystem;
namespace tdx {
namespace {

constexpr const char* kResource = "list/func_dpfx101_1.jsn";

const Json* field(const Json& row, std::string_view name) {
    if (!row.is_object()) return nullptr;
    const auto found = row.as_object().find(name);
    return found == row.as_object().end() ? nullptr : &found->second;
}

std::string text_value(const Json& row, std::string_view name) {
    const auto* value = field(row, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

std::string signal_name(const std::string& value) {
    if (value == "利好") return "positive";
    if (value == "中性") return "neutral";
    if (value == "利空") return "negative";
    return "unrated";
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
        throw Error("local overview-factors resource is unavailable: " + path_utf8(path));
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

}  // namespace

Json normalize_overview_factor_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("overview-factor rows must be an array");
    Json result = Json::array();
    std::uint64_t rank = 0;
    for (const auto& row : rows.as_array()) {
        ++rank;
        const auto id = text_value(row, "$ZQDM");
        const auto name = text_value(row, "zbname");
        if (id.empty() || name.empty()) continue;
        const auto label = text_value(row, "lx");
        Json item = Json::object();
        item["factor_id"] = id;
        item["name"] = name;
        item["description"] = text_value(row, "ms");
        item["signal"] = signal_name(label);
        item["signal_label"] = label.empty() ? "未评级" : label;
        item["chart_indicator"] = text_value(row, "pname");
        item["source_rank"] = rank;
        item["source_resource"] = kResource;
        item["raw"] = row;
        result.push_back(std::move(item));
    }
    return result;
}

OverviewFactorsService::OverviewFactorsService(fs::path jsn_root)
    : jsn_root_(std::move(jsn_root)) {}

Json OverviewFactorsService::fetch_master(
    const OverviewFactorsQuery& options, bool& refreshed, int& age_seconds) {
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
    Json result = Json::object();
    result["records"] = normalize_overview_factor_rows(document.at("rows"));
    Json source = Json::object();
    for (const auto* name : {"resource", "size", "row_count", "endpoint"})
        source[name] = document.at(name);
    source["normalized_row_count"] = static_cast<std::uint64_t>(result.at("records").size());
    Json sources = Json::array();
    sources.push_back(std::move(source));
    result["sources"] = std::move(sources);
    cache_ = result;
    cache_time_ = std::time(nullptr);
    refreshed = true;
    age_seconds = 0;
    return result;
}

Json OverviewFactorsService::query(const OverviewFactorsQuery& options) {
    const std::set<std::string> signals{
        "all", "positive", "neutral", "negative", "unrated"};
    if (!signals.count(options.signal))
        throw Error("signal must be all, positive, neutral, negative or unrated");
    if (options.limit < 1 || options.limit > 1000)
        throw Error("limit must be in 1..1000");
    bool refreshed = false;
    int age_seconds = 0;
    const auto master = fetch_master(options, refreshed, age_seconds);
    const auto needle = lower_ascii(trim(options.query));
    Json records = Json::array();
    for (const auto& row : master.at("records").as_array()) {
        if (options.signal != "all" && text_value(row, "signal") != options.signal) continue;
        if (!needle.empty() && lower_ascii(row.dump(-1)).find(needle) == std::string::npos)
            continue;
        auto projected = row;
        if (!options.include_raw) projected.as_object().erase("raw");
        records.push_back(std::move(projected));
    }
    std::map<std::string, std::uint64_t> counts;
    for (const auto& row : records.as_array()) ++counts[text_value(row, "signal")];
    const auto matched = records.size();
    if (static_cast<int>(records.size()) > options.limit)
        records.as_array().resize(static_cast<std::size_t>(options.limit));
    Json summary = Json::object();
    summary["positive"] = counts["positive"];
    summary["neutral"] = counts["neutral"];
    summary["negative"] = counts["negative"];
    summary["unrated"] = counts["unrated"];
    Json result = Json::object();
    result["schema"] = "tdx-market-overview-factors-native-v1";
    result["generated_at"] = now_text();
    result["signal"] = options.signal;
    result["availability"] = matched ? "live" : "empty";
    result["match_count"] = static_cast<std::uint64_t>(matched);
    result["returned"] = static_cast<std::uint64_t>(records.size());
    result["records"] = std::move(records);
    result["summary"] = std::move(summary);
    result["sources"] = master.at("sources");
    result["semantics"] =
        "DPFX client-curated market factors. Descriptions and positive/neutral/negative "
        "labels are source snapshots with mixed observation dates; they are not live ticks.";
    Json cache = Json::object();
    cache["refreshed"] = refreshed;
    cache["age_seconds"] = age_seconds;
    result["cache"] = std::move(cache);
    return result;
}

int command_market_overview_factors(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout << "Usage: tdx-tool market overview-factors [options]\n\n"
            "  --signal all|positive|neutral|negative|unrated --query TEXT\n"
            "  --without-raw --refresh --input-dir PATH --limit N\n"
            "  --timeout-ms N --output PATH --compact\n";
        return 0;
    }
    OverviewFactorsQuery query;
    query.signal = lower_ascii(trim(args.take_option("--signal", "all")));
    query.query = trim(args.take_option("--query"));
    query.include_raw = !args.take_flag("--without-raw");
    query.refresh = args.take_flag("--refresh");
    query.limit = bounded(args.take_option("--limit", "100"), "--limit", 1, 1000);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 60000);
    const auto input = native_path(args.take_option("--input-dir", "output/tdx-jsn"));
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-overview-factors-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    OverviewFactorsService service(input);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number()
              << " overview factors -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
