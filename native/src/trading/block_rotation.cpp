#include "tdx/block_rotation.hpp"
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
#include <string_view>

namespace fs = std::filesystem;

namespace tdx {
namespace {

const std::map<std::string, std::string> resources{
    {"industry", "list/func_bkld101_1.jsn"},
    {"concept", "list/func_bkld102_1.jsn"},
    {"region", "list/func_bkld103_1.jsn"},
    {"style", "list/func_bkld104_1.jsn"},
};

const Json* value_ptr(const Json& value, std::string_view key) {
    if (!value.is_object()) return nullptr;
    const auto found = value.as_object().find(key);
    return found == value.as_object().end() ? nullptr : &found->second;
}

std::string text_value(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found ? trim(jsn_scalar_text(*found)) : std::string{};
}

std::optional<double> number_value(const Json& value, std::string_view key) {
    const auto text = text_value(value, key);
    if (text.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(text, &used);
        return used == text.size() && std::isfinite(parsed)
            ? std::optional<double>(parsed) : std::nullopt;
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

std::optional<int> market_id(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "0" || value == "sz") return 0;
    if (value == "1" || value == "sh") return 1;
    if (value == "2" || value == "44" || value == "bj") return 2;
    return std::nullopt;
}

std::string market_name(int id) {
    return id == 0 ? "sz" : id == 1 ? "sh" : id == 2 ? "bj"
                                                          : "m" + std::to_string(id);
}

std::string market_prefix(int id) {
    return id == 0 ? "SZ" : id == 1 ? "SH" : id == 2 ? "BJ"
                                                          : "M" + std::to_string(id);
}

const Block* local_block(const BlockData& blocks, const std::string& code,
                         const std::string& category) {
    const std::string expected = category == "region" ? "" : category;
    const Block* fallback = nullptr;
    for (const auto& block : blocks.blocks) {
        if (block.block_code != code) continue;
        if (!fallback) fallback = &block;
        if (!expected.empty() && block.family == expected) return &block;
    }
    return expected.empty() ? nullptr : fallback;
}

Json block_document(const Json& raw, const std::string& category,
                    const BlockData& blocks) {
    const auto code = text_value(raw, "$ZQDM");
    const auto upstream_market = market_id(text_value(raw, "$SC"));
    const auto* local = local_block(blocks, code, category);
    std::string name = local ? local->name : std::string{};
    if (name.empty() && upstream_market) {
        const auto known = blocks.securities.find({*upstream_market, code});
        if (known != blocks.securities.end()) name = known->second.name;
    }
    Json result = Json::object();
    result["entity_type"] = "block";
    result["category"] = category;
    result["code"] = code;
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    result["upstream_market_id"] = upstream_market
        ? Json(*upstream_market) : Json(nullptr);
    result["upstream_market"] = upstream_market
        ? Json(market_name(*upstream_market)) : Json(nullptr);
    result["security_id"] = upstream_market
        ? Json(market_prefix(*upstream_market) + code) : Json(nullptr);
    result["block_id"] = local ? Json(local->block_id) : Json(nullptr);
    result["local_family"] = local ? Json(local->family) : Json(nullptr);
    result["local_family_name"] = local ? Json(local->family_name) : Json(nullptr);
    result["member_count"] = local ? Json(local->member_count) : Json(nullptr);
    result["members_available"] = local != nullptr;
    result["members_api"] = local
        ? Json("/api/v1/blocks?q=" + code) : Json(nullptr);
    return result;
}

const Json* period_document(const Json& row, const std::string& period) {
    const auto* periods = value_ptr(row, "periods");
    return periods ? value_ptr(*periods, period) : nullptr;
}

std::optional<double> row_metric(const Json& row, const std::string& sort,
                                 const std::string& period) {
    if (sort == "days-since") return number_value(row, "days_since_last_anomaly");
    if (sort == "average-cycle") return number_value(row, "average_cycle_days");
    if (sort == "cycle-gap") return number_value(row, "cycle_gap_days");
    const auto* selected = period_document(row, period);
    if (!selected) return std::nullopt;
    if (sort == "anomalies") return number_value(*selected, "anomaly_count");
    if (sort == "up-anomalies") return number_value(*selected, "up_anomaly_count");
    if (sort == "down-anomalies") return number_value(*selected, "down_anomaly_count");
    if (sort == "imbalance") return number_value(*selected, "direction_imbalance");
    if (sort == "return") return number_value(*selected, "return_pct");
    return std::nullopt;
}

std::string signal_for(const Json& row, const std::string& period) {
    const auto* selected = period_document(row, period);
    if (!selected) return "inactive";
    const auto total = number_value(*selected, "anomaly_count").value_or(0.0);
    const auto up = number_value(*selected, "up_anomaly_count").value_or(0.0);
    const auto down = number_value(*selected, "down_anomaly_count").value_or(0.0);
    if (total <= 0.0) return "inactive";
    if (up > down) return "up-dominant";
    if (down > up) return "down-dominant";
    return "balanced";
}

bool json_contains(const Json& value, const std::string& needle) {
    if (value.is_string())
        return lower_ascii(value.as_string()).find(needle) != std::string::npos;
    if (value.is_number() || value.is_bool())
        return lower_ascii(jsn_scalar_text(value)).find(needle) != std::string::npos;
    if (value.is_array()) {
        for (const auto& child : value.as_array())
            if (json_contains(child, needle)) return true;
    } else if (value.is_object()) {
        for (const auto& [key, child] : value.as_object())
            if (lower_ascii(key).find(needle) != std::string::npos ||
                json_contains(child, needle)) return true;
    }
    return false;
}

Json summary_document(const Json& rows, const std::string& period) {
    struct Aggregate {
        std::uint64_t rows{}, resolved{}, up_dominant{}, down_dominant{}, balanced{}, inactive{};
        double anomalies{}, up{}, down{};
    };
    std::map<std::string, Aggregate> categories;
    Aggregate total;
    std::string latest;
    std::uint64_t positive = 0, negative = 0, flat = 0, missing_return = 0;
    for (const auto& row : rows.as_array()) {
        const auto& block = row.at("block");
        auto& category = categories[block.at("category").as_string()];
        for (auto* aggregate : {&category, &total}) {
            ++aggregate->rows;
            if (block.at("name_resolved").as_bool()) ++aggregate->resolved;
            const auto* selected = period_document(row, period);
            if (selected) {
                aggregate->anomalies += number_value(*selected, "anomaly_count").value_or(0.0);
                aggregate->up += number_value(*selected, "up_anomaly_count").value_or(0.0);
                aggregate->down += number_value(*selected, "down_anomaly_count").value_or(0.0);
            }
            const auto signal = signal_for(row, period);
            if (signal == "up-dominant") ++aggregate->up_dominant;
            else if (signal == "down-dominant") ++aggregate->down_dominant;
            else if (signal == "balanced") ++aggregate->balanced;
            else ++aggregate->inactive;
        }
        latest = std::max(latest, text_value(row, "last_anomaly_date"));
        const auto* selected = period_document(row, period);
        const auto value = selected ? number_value(*selected, "return_pct") : std::nullopt;
        if (!value) ++missing_return;
        else if (*value > 0) ++positive;
        else if (*value < 0) ++negative;
        else ++flat;
    }
    auto aggregate_json = [](const std::string& name, const Aggregate& value) {
        Json item = Json::object();
        item["category"] = name;
        item["blocks"] = value.rows;
        item["names_resolved"] = value.resolved;
        item["anomaly_count"] = value.anomalies;
        item["up_anomaly_count"] = value.up;
        item["down_anomaly_count"] = value.down;
        item["up_dominant_blocks"] = value.up_dominant;
        item["down_dominant_blocks"] = value.down_dominant;
        item["balanced_blocks"] = value.balanced;
        item["inactive_blocks"] = value.inactive;
        return item;
    };
    Json by_category = Json::array();
    for (const auto& [name, aggregate] : categories)
        by_category.push_back(aggregate_json(name, aggregate));
    Json result = aggregate_json("all", total);
    result["period"] = period;
    result["latest_anomaly_date"] = latest.empty() ? Json(nullptr) : Json(latest);
    result["positive_return_blocks"] = positive;
    result["negative_return_blocks"] = negative;
    result["flat_return_blocks"] = flat;
    result["missing_return_blocks"] = missing_return;
    result["by_category"] = std::move(by_category);
    return result;
}

std::vector<std::string> categories_for(const std::string& category) {
    if (category == "all") return {"industry", "concept", "region", "style"};
    if (!resources.count(category))
        throw Error("category must be all, industry, concept, region, or style");
    return {category};
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

int bounded(const std::string& text, const std::string& name,
            int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const auto value = std::stoi(text, &used);
        if (used != text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(name + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

}  // namespace

std::string block_rotation_resource_for(const std::string& category_value) {
    const auto category = lower_ascii(trim(category_value));
    const auto found = resources.find(category);
    if (found == resources.end())
        throw Error("category must be industry, concept, region, or style");
    return found->second;
}

Json normalize_block_rotation_rows(const Json& rows,
                                   const std::string& category_value,
                                   const BlockData& blocks) {
    if (!rows.is_array()) throw Error("block-rotation rows must be an array");
    const auto category = lower_ascii(trim(category_value));
    (void)block_rotation_resource_for(category);
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        const auto code = text_value(raw, "$ZQDM");
        if (!digits(code, 6)) continue;
        const auto days = number_value(raw, "ts");
        const auto average = number_value(raw, "pl");
        Json row = Json::object();
        row["block"] = block_document(raw, category, blocks);
        row["last_anomaly_date"] = text_value(raw, "date");
        row["days_since_last_anomaly"] = number_json(days);
        row["average_cycle_days"] = number_json(average);
        row["cycle_gap_days"] = days && average
            ? Json(*average - *days) : Json(nullptr);
        Json periods = Json::object();
        for (const auto& period : {"1w", "1m", "3m", "1y"}) {
            const auto anomalies = number_value(raw, std::string("yd_") + period);
            const auto up = number_value(raw, std::string("zyd_") + period);
            const auto down = number_value(raw, std::string("dyd_") + period);
            Json item = Json::object();
            item["anomaly_count"] = number_json(anomalies);
            item["up_anomaly_count"] = number_json(up);
            item["down_anomaly_count"] = number_json(down);
            item["direction_imbalance"] = up && down
                ? Json(*up - *down) : Json(nullptr);
            item["return_pct"] = number_json(
                number_value(raw, std::string("zf_") + period));
            periods[period] = std::move(item);
        }
        row["periods"] = std::move(periods);
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

void sort_block_rotation_rows(Json& rows, const std::string& sort_value,
                              const std::string& period_value,
                              const std::string& order_value) {
    if (!rows.is_array()) throw Error("block-rotation sort requires an array");
    const auto sort = lower_ascii(trim(sort_value));
    const auto period = lower_ascii(trim(period_value));
    const auto order = lower_ascii(trim(order_value));
    const std::set<std::string> sorts{"last-date", "days-since", "average-cycle",
        "cycle-gap", "anomalies", "up-anomalies", "down-anomalies",
        "imbalance", "return"};
    if (!sorts.count(sort))
        throw Error("sort must be last-date, days-since, average-cycle, cycle-gap, anomalies, up-anomalies, down-anomalies, imbalance, or return");
    if (!std::set<std::string>{"1w", "1m", "3m", "1y"}.count(period))
        throw Error("period must be 1w, 1m, 3m, or 1y");
    if (order != "asc" && order != "desc") throw Error("order must be asc or desc");
    const bool descending = order == "desc";
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            if (sort == "last-date") {
                const auto a = text_value(left, "last_anomaly_date");
                const auto b = text_value(right, "last_anomaly_date");
                if (a.empty() != b.empty()) return !a.empty();
                if (a != b) return descending ? a > b : a < b;
            } else {
                const auto a = row_metric(left, sort, period);
                const auto b = row_metric(right, sort, period);
                if (a.has_value() != b.has_value()) return a.has_value();
                if (a && b && *a != *b) return descending ? *a > *b : *a < *b;
            }
            return left.at("block").at("code").as_string() <
                   right.at("block").at("code").as_string();
        });
}

BlockRotationService::BlockRotationService(BlockData blocks)
    : blocks_(std::move(blocks)) {}

BlockRotationService::FetchResult BlockRotationService::fetch(
    const std::string& category, const BlockRotationQuery& options) {
    const auto now = std::time(nullptr);
    const auto cached = cache_.find(category);
    const int age = cached == cache_.end() ? 0 :
        static_cast<int>(std::max<std::time_t>(0, now - cached->second.fetched_at));
    if (!options.refresh && cached != cache_.end() && age < options.cache_ttl_seconds)
        return {cached->second.document, false, age};
    const auto categories = categories_for(category);
    std::vector<std::string> requested;
    for (const auto& value : categories) requested.push_back(resources.at(value));
    const auto sources = fetch_jsn_resources_rows(requested, "bi", options.timeout_ms);
    Json rows = Json::array();
    Json metadata = Json::array();
    for (std::size_t index = 0; index < sources.size(); ++index) {
        const auto normalized = normalize_block_rotation_rows(
            sources.as_array()[index].at("rows"), categories[index], blocks_);
        for (const auto& row : normalized.as_array()) rows.push_back(row);
        metadata.push_back(jsn_source_metadata(sources.as_array()[index]));
    }
    Json document = Json::object();
    document["rows"] = std::move(rows);
    document["sources"] = std::move(metadata);
    cache_[category] = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

Json BlockRotationService::query(const BlockRotationQuery& input) {
    BlockRotationQuery options = input;
    options.category = lower_ascii(trim(options.category));
    options.period = lower_ascii(trim(options.period));
    options.sort = lower_ascii(trim(options.sort));
    options.order = lower_ascii(trim(options.order));
    options.signal = lower_ascii(trim(options.signal));
    (void)categories_for(options.category);
    if (!std::set<std::string>{"1w", "1m", "3m", "1y"}.count(options.period))
        throw Error("period must be 1w, 1m, 3m, or 1y");
    if (!std::set<std::string>{"all", "up-dominant", "down-dominant",
                               "balanced", "inactive"}.count(options.signal))
        throw Error("signal must be all, up-dominant, down-dominant, balanced, or inactive");
    if (!options.code.empty() && !digits(options.code, 6))
        throw Error("code must contain six digits");
    if (options.offset < 0 || options.offset > 1000000)
        throw Error("offset must be in 0..1000000");
    if (options.limit < 1 || options.limit > 10000)
        throw Error("limit must be in 1..10000");
    if (options.cache_ttl_seconds < 0 || options.cache_ttl_seconds > 86400)
        throw Error("cache_ttl_seconds must be in 0..86400");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");

    auto fetched = fetch(options.category, options);
    auto candidates = fetched.document.at("rows");
    sort_block_rotation_rows(candidates, options.sort, options.period, options.order);
    const auto needle = lower_ascii(trim(options.query));
    Json matched_rows = Json::array();
    for (const auto& row : candidates.as_array()) {
        if (!options.code.empty() && row.at("block").at("code").as_string() != options.code)
            continue;
        if (options.signal != "all" && signal_for(row, options.period) != options.signal)
            continue;
        if (!needle.empty() && !json_contains(row, needle)) continue;
        auto record = row;
        record["selected_period"] = options.period;
        record["selected_signal"] = signal_for(row, options.period);
        matched_rows.push_back(std::move(record));
    }
    const auto matched = matched_rows.size();
    Json records = Json::array();
    for (std::size_t index = static_cast<std::size_t>(options.offset);
         index < matched_rows.size() && records.size() < static_cast<std::size_t>(options.limit);
         ++index)
        records.push_back(matched_rows.as_array()[index]);

    const auto health = jsn_sources_health(fetched.document.at("sources"));
    Json result = Json::object();
    result["schema"] = "tdx-market-block-rotation-native-v1";
    result["generated_at"] = now_text();
    result["mode"] = options.code.empty() ? "market" : "block";
    result["availability"] = health.at("stale").as_bool()
        ? "stale-cache" : matched == 0 ? "empty" : "live";
    Json filters = Json::object();
    filters["category"] = options.category;
    filters["period"] = options.period;
    filters["sort"] = options.sort;
    filters["order"] = options.order;
    filters["signal"] = options.signal;
    filters["code"] = options.code.empty() ? Json(nullptr) : Json(options.code);
    filters["query"] = options.query;
    result["filters"] = std::move(filters);
    result["summary"] = summary_document(candidates, options.period);
    Json counts = Json::object();
    std::uint64_t source_rows = 0;
    for (const auto& source : fetched.document.at("sources").as_array())
        source_rows += static_cast<std::uint64_t>(source.at("row_count").as_number());
    counts["source_rows"] = source_rows;
    counts["normalized_rows"] = static_cast<std::uint64_t>(candidates.size());
    counts["matched"] = static_cast<std::uint64_t>(matched);
    counts["returned"] = static_cast<std::uint64_t>(records.size());
    result["counts"] = std::move(counts);
    result["records"] = std::move(records);
    result["sources"] = fetched.document.at("sources");
    result["upstream_health"] = health;
    Json cache = Json::object();
    cache["ttl_seconds"] = options.cache_ttl_seconds;
    cache["refreshed"] = fetched.refreshed;
    cache["age_seconds"] = fetched.age_seconds;
    result["cache"] = std::move(cache);
    Json categories = Json::array();
    for (const auto& value : {"industry", "concept", "region", "style"})
        categories.push_back(value);
    Json periods = Json::array();
    for (const auto& value : {"1w", "1m", "3m", "1y"}) periods.push_back(value);
    result["categories"] = std::move(categories);
    result["periods"] = std::move(periods);
    result["semantics"] =
        "TDX BKLD block-rotation anomaly counts and returns for industry, concept, region and style blocks. cycle_gap_days reproduces the client formula pl-ts. Current quote syscols are not fabricated; region names may resolve from the local block-index security directory even when local membership is unavailable.";
    return result;
}

int command_market_block_rotation(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market block-rotation [options]\n\n"
            "Typed TDX BKLD industry/concept/region/style rotation statistics.\n\n"
            "  --root PATH             TDX installation root\n"
            "  --category NAME         all|industry|concept|region|style; default all\n"
            "  --period NAME           1w|1m|3m|1y; default 1w\n"
            "  --sort NAME             last-date|days-since|average-cycle|cycle-gap|anomalies|up-anomalies|down-anomalies|imbalance|return\n"
            "  --order asc|desc        Default desc\n"
            "  --signal NAME           all|up-dominant|down-dominant|balanced|inactive\n"
            "  --code CODE             Optional six-digit block code\n"
            "  --query TEXT            Filter normalized content\n"
            "  --offset N              Default 0\n"
            "  --limit N               Default 500, maximum 10000\n"
            "  --refresh               Bypass service cache\n"
            "  --cache-ttl N           Default 300 seconds\n"
            "  --timeout-ms N          Default 15000\n"
            "  --output FILE           Write JSON instead of stdout\n"
            "  --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    BlockRotationQuery query;
    query.category = lower_ascii(trim(args.take_option("--category", "all")));
    query.period = lower_ascii(trim(args.take_option("--period", "1w")));
    query.sort = lower_ascii(trim(args.take_option("--sort", "last-date")));
    query.order = lower_ascii(trim(args.take_option("--order", "desc")));
    query.signal = lower_ascii(trim(args.take_option("--signal", "all")));
    query.code = trim(args.take_option("--code"));
    query.query = trim(args.take_option("--query"));
    query.offset = bounded(args.take_option("--offset", "0"), "offset", 0, 1000000);
    query.limit = bounded(args.take_option("--limit", "500"), "limit", 1, 10000);
    query.refresh = args.take_flag("--refresh");
    query.cache_ttl_seconds = bounded(
        args.take_option("--cache-ttl", "300"), "cache-ttl", 0, 86400);
    query.timeout_ms = bounded(
        args.take_option("--timeout-ms", "15000"), "timeout-ms", 100, 60000);
    const auto output_name = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    BlockRotationService service(load_blocks(
        root, {"industry", "research-industry", "concept", "style", "index"}));
    const auto result = service.query(query);
    const auto rendered = result.dump(compact ? -1 : 2) + '\n';
    if (output_name.empty()) std::cout << rendered;
    else {
        const auto output = native_path(output_name);
        atomic_write_text(output, rendered);
        std::cout << "completed block-rotation query -> " << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
