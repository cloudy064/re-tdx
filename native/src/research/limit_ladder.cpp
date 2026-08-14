#include "tdx/limit_ladder.hpp"
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

constexpr const char* resource = "list/func_lbtt101_1.jsn";

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

const Block* local_block(const BlockData& blocks, const std::string& code) {
    const Block* fallback = nullptr;
    for (const auto& block : blocks.blocks) {
        if (block.block_code != code) continue;
        if (!fallback) fallback = &block;
        if (block.family == "research-industry" || block.family == "concept")
            return &block;
    }
    return fallback;
}

std::string category_for(const Json& raw, const Block* block) {
    if (block) {
        if (block->family == "research-industry" || block->family == "industry")
            return "industry";
        if (block->family == "concept") return "concept";
    }
    const auto name = text_value(raw, "mc");
    return !name.empty() && name.front() == '*' ? "industry" : "concept";
}

std::string fallback_name(const Json& raw) {
    auto name = text_value(raw, "mc");
    if (!name.empty() && name.front() == '*') name.erase(name.begin());
    return name;
}

Json block_document(const Json& raw, const BlockData& blocks) {
    const auto code = text_value(raw, "$ZQDM");
    const auto* local = local_block(blocks, code);
    const auto category = category_for(raw, local);
    const auto fallback = fallback_name(raw);
    const auto name = local && !local->name.empty() ? local->name : fallback;
    Json result = Json::object();
    result["entity_type"] = "block";
    result["category"] = category;
    result["code"] = code;
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    result["upstream_name"] = text_value(raw, "mc");
    result["upstream_market_id"] = number_json(number_value(raw, "$SC"));
    result["security_id"] = text_value(raw, "$SC") == "1"
        ? Json("SH" + code) : Json(nullptr);
    result["block_id"] = local ? Json(local->block_id) : Json(nullptr);
    result["local_family"] = local ? Json(local->family) : Json(nullptr);
    result["local_family_name"] = local ? Json(local->family_name) : Json(nullptr);
    result["parent_block_id"] = local && !local->parent_block_id.empty()
        ? Json(local->parent_block_id) : Json(nullptr);
    result["level"] = local ? Json(local->level) : Json(nullptr);
    result["is_leaf"] = local ? Json(local->is_leaf) : Json(nullptr);
    result["member_count"] = local ? Json(local->member_count) : Json(nullptr);
    result["members_available"] = local != nullptr;
    result["members_api"] = local
        ? Json("/api/v1/blocks?q=" + code) : Json(nullptr);
    return result;
}

std::optional<double> metric(const Json& row, const std::string& sort) {
    if (sort == "sealed") return number_value(row, "sealed_limit_up_count");
    if (sort == "broken") return number_value(row, "broken_board_count");
    if (sort == "prior-limit") return number_value(row, "prior_limit_up_count");
    if (sort == "consecutive") return number_value(row, "consecutive_limit_up_count");
    if (sort == "max-height") return number_value(row, "max_streak_height");
    if (sort == "total-height") return number_value(row, "sum_streak_heights");
    if (sort == "advancement-rate") return number_value(row, "advancement_rate_pct");
    return std::nullopt;
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

bool matches_activity(const Json& row, const std::string& activity) {
    const auto sealed = number_value(row, "sealed_limit_up_count").value_or(0.0);
    const auto broken = number_value(row, "broken_board_count").value_or(0.0);
    const auto consecutive = number_value(row, "consecutive_limit_up_count").value_or(0.0);
    const auto rate = number_value(row, "advancement_rate_pct").value_or(0.0);
    if (activity == "all") return true;
    if (activity == "sealed") return sealed > 0.0;
    if (activity == "broken") return broken > 0.0;
    if (activity == "consecutive") return consecutive > 0.0;
    if (activity == "advanced") return rate > 0.0;
    return sealed <= 0.0 && broken <= 0.0;
}

Json summary_document(const Json& rows, const std::string& category_filter) {
    struct Aggregate {
        std::uint64_t blocks{}, resolved{}, sealed_blocks{}, broken_blocks{},
            consecutive_blocks{}, inactive_blocks{}, formula_checked{},
            formula_mismatches{};
        double sealed{}, broken{}, prior{}, consecutive{}, total_height{};
        double max_height{};
    };
    std::map<std::string, Aggregate> by_category;
    Aggregate total;
    std::string latest;
    for (const auto& row : rows.as_array()) {
        const auto category = row.at("block").at("category").as_string();
        auto& group = by_category[category];
        const auto sealed = number_value(row, "sealed_limit_up_count").value_or(0.0);
        const auto broken = number_value(row, "broken_board_count").value_or(0.0);
        const auto prior = number_value(row, "prior_limit_up_count").value_or(0.0);
        const auto consecutive = number_value(row, "consecutive_limit_up_count").value_or(0.0);
        const auto max_height = number_value(row, "max_streak_height").value_or(0.0);
        const auto total_height = number_value(row, "sum_streak_heights").value_or(0.0);
        const auto* formula = value_ptr(row, "advancement_rate_formula_matches");
        for (auto* value : {&group, &total}) {
            ++value->blocks;
            if (row.at("block").at("name_resolved").as_bool()) ++value->resolved;
            if (sealed > 0.0) ++value->sealed_blocks;
            if (broken > 0.0) ++value->broken_blocks;
            if (consecutive > 0.0) ++value->consecutive_blocks;
            if (sealed <= 0.0 && broken <= 0.0) ++value->inactive_blocks;
            if (formula && formula->is_bool()) {
                ++value->formula_checked;
                if (!formula->as_bool()) ++value->formula_mismatches;
            }
            value->sealed += sealed;
            value->broken += broken;
            value->prior += prior;
            value->consecutive += consecutive;
            value->total_height += total_height;
            value->max_height = std::max(value->max_height, max_height);
        }
        latest = std::max(latest, text_value(row, "date"));
    }
    auto render = [](const std::string& category, const Aggregate& value) {
        Json result = Json::object();
        result["category"] = category;
        result["blocks"] = value.blocks;
        result["names_resolved"] = value.resolved;
        result["sealed_blocks"] = value.sealed_blocks;
        result["broken_blocks"] = value.broken_blocks;
        result["consecutive_blocks"] = value.consecutive_blocks;
        result["inactive_blocks"] = value.inactive_blocks;
        result["advancement_formula_checked_blocks"] = value.formula_checked;
        result["advancement_formula_mismatch_blocks"] = value.formula_mismatches;
        result["sealed_limit_up_count"] = value.sealed;
        result["broken_board_count"] = value.broken;
        result["prior_limit_up_count"] = value.prior;
        result["consecutive_limit_up_count"] = value.consecutive;
        result["max_streak_height"] = value.max_height;
        result["sum_streak_heights"] = value.total_height;
        result["weighted_advancement_rate_pct"] = value.prior > 0.0
            ? Json(std::round(value.consecutive * 10000.0 / value.prior) / 100.0)
            : Json(nullptr);
        return result;
    };
    Json result = render(category_filter, total);
    result["latest_date"] = latest.empty() ? Json(nullptr) : Json(latest);
    Json categories = Json::array();
    for (const auto& [name, value] : by_category)
        categories.push_back(render(name, value));
    result["by_category"] = std::move(categories);
    return result;
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

Json normalize_limit_ladder_rows(const Json& rows, const BlockData& blocks) {
    if (!rows.is_array()) throw Error("limit-ladder rows must be an array");
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        const auto code = text_value(raw, "$ZQDM");
        if (!digits(code, 6)) continue;
        const auto prior = number_value(raw, "zrzt");
        const auto consecutive = number_value(raw, "lbs");
        const auto upstream_rate = number_value(raw, "lbjjl");
        const auto calculated = prior && *prior > 0.0 && consecutive
            ? std::optional<double>(std::round(*consecutive * 10000.0 / *prior) / 100.0)
            : std::nullopt;
        Json row = Json::object();
        row["block"] = block_document(raw, blocks);
        row["date"] = text_value(raw, "rq");
        row["sealed_limit_up_count"] = number_json(number_value(raw, "ztjs"));
        row["broken_board_count"] = number_json(number_value(raw, "zbs"));
        row["prior_limit_up_count"] = number_json(prior);
        row["consecutive_limit_up_count"] = number_json(consecutive);
        row["max_streak_height"] = number_json(number_value(raw, "lbgd"));
        row["sum_streak_heights"] = number_json(number_value(raw, "hzgd"));
        row["advancement_rate_pct"] = number_json(upstream_rate);
        row["calculated_advancement_rate_pct"] = number_json(calculated);
        row["advancement_rate_formula_matches"] = upstream_rate && calculated
            ? Json(std::abs(*upstream_rate - *calculated) <= 0.011) : Json(nullptr);
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

void sort_limit_ladder_rows(Json& rows, const std::string& sort_value,
                            const std::string& order_value) {
    if (!rows.is_array()) throw Error("limit-ladder sort requires an array");
    const auto sort = lower_ascii(trim(sort_value));
    const auto order = lower_ascii(trim(order_value));
    const std::set<std::string> sorts{"date", "sealed", "broken", "prior-limit",
        "consecutive", "max-height", "total-height", "advancement-rate"};
    if (!sorts.count(sort))
        throw Error("sort must be date, sealed, broken, prior-limit, consecutive, max-height, total-height, or advancement-rate");
    if (order != "asc" && order != "desc") throw Error("order must be asc or desc");
    const bool descending = order == "desc";
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            if (sort == "date") {
                const auto a = text_value(left, "date");
                const auto b = text_value(right, "date");
                if (a.empty() != b.empty()) return !a.empty();
                if (a != b) return descending ? a > b : a < b;
            } else {
                const auto a = metric(left, sort);
                const auto b = metric(right, sort);
                if (a.has_value() != b.has_value()) return a.has_value();
                if (a && b && *a != *b) return descending ? *a > *b : *a < *b;
            }
            return left.at("block").at("code").as_string() <
                   right.at("block").at("code").as_string();
        });
}

LimitLadderService::LimitLadderService(BlockData blocks)
    : blocks_(std::move(blocks)) {}

LimitLadderService::FetchResult LimitLadderService::fetch(
    const LimitLadderQuery& options) {
    const auto now = std::time(nullptr);
    const int age = cache_.fetched_at == 0 ? 0 :
        static_cast<int>(std::max<std::time_t>(0, now - cache_.fetched_at));
    if (!options.refresh && cache_.fetched_at != 0 && age < options.cache_ttl_seconds)
        return {cache_.document, false, age};
    const auto sources = fetch_jsn_resources_rows({resource}, "bi", options.timeout_ms);
    Json document = Json::object();
    document["rows"] = normalize_limit_ladder_rows(
        sources.as_array().front().at("rows"), blocks_);
    Json metadata = Json::array();
    metadata.push_back(jsn_source_metadata(sources.as_array().front()));
    document["sources"] = std::move(metadata);
    cache_ = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

Json LimitLadderService::query(const LimitLadderQuery& input) {
    LimitLadderQuery options = input;
    options.category = lower_ascii(trim(options.category));
    options.sort = lower_ascii(trim(options.sort));
    options.order = lower_ascii(trim(options.order));
    options.activity = lower_ascii(trim(options.activity));
    if (!std::set<std::string>{"all", "industry", "concept"}.count(options.category))
        throw Error("category must be all, industry, or concept");
    if (!std::set<std::string>{"all", "sealed", "broken", "consecutive",
                               "advanced", "inactive"}.count(options.activity))
        throw Error("activity must be all, sealed, broken, consecutive, advanced, or inactive");
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

    auto fetched = fetch(options);
    Json category_rows = Json::array();
    for (const auto& row : fetched.document.at("rows").as_array()) {
        if (options.category == "all" ||
            row.at("block").at("category").as_string() == options.category)
            category_rows.push_back(row);
    }
    const auto summary = summary_document(category_rows, options.category);
    sort_limit_ladder_rows(category_rows, options.sort, options.order);
    const auto needle = lower_ascii(trim(options.query));
    Json matched_rows = Json::array();
    for (const auto& row : category_rows.as_array()) {
        if (!options.code.empty() && row.at("block").at("code").as_string() != options.code)
            continue;
        if (!matches_activity(row, options.activity)) continue;
        if (!needle.empty() && !json_contains(row, needle)) continue;
        matched_rows.push_back(row);
    }
    const auto matched = matched_rows.size();
    Json records = Json::array();
    for (std::size_t index = static_cast<std::size_t>(options.offset);
         index < matched_rows.size() && records.size() < static_cast<std::size_t>(options.limit);
         ++index)
        records.push_back(matched_rows.as_array()[index]);

    const auto health = jsn_sources_health(fetched.document.at("sources"));
    Json result = Json::object();
    result["schema"] = "tdx-market-limit-ladder-native-v1";
    result["generated_at"] = now_text();
    result["mode"] = options.code.empty() ? "market" : "block";
    result["availability"] = health.at("stale").as_bool()
        ? "stale-cache" : matched == 0 ? "empty" : "live";
    Json filters = Json::object();
    filters["category"] = options.category;
    filters["sort"] = options.sort;
    filters["order"] = options.order;
    filters["activity"] = options.activity;
    filters["code"] = options.code.empty() ? Json(nullptr) : Json(options.code);
    filters["query"] = options.query;
    result["filters"] = std::move(filters);
    result["summary"] = summary;
    Json counts = Json::object();
    counts["source_rows"] = static_cast<std::uint64_t>(
        fetched.document.at("sources").as_array().front().at("row_count").as_number());
    counts["category_rows"] = static_cast<std::uint64_t>(category_rows.size());
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
    result["categories"] = Json::parse("[\"industry\",\"concept\"]");
    result["semantics"] =
        "TDX LBTT block-level limit ladder for research industries and concepts. advancement_rate_pct is the upstream lbjjl value and calculated_advancement_rate_pct verifies lbs/zrzt*100. max_streak_height and sum_streak_heights preserve the client labels height and total-height; live quote syscols are not fabricated.";
    return result;
}

int command_market_limit_ladder(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market limit-ladder [options]\n\n"
            "Typed TDX LBTT research-industry/concept limit ladder.\n\n"
            "  --root PATH             TDX installation root\n"
            "  --category NAME         all|industry|concept; default all\n"
            "  --sort NAME             date|sealed|broken|prior-limit|consecutive|max-height|total-height|advancement-rate\n"
            "  --order asc|desc        Default desc\n"
            "  --activity NAME         all|sealed|broken|consecutive|advanced|inactive\n"
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
    LimitLadderQuery query;
    query.category = args.take_option("--category", "all");
    query.sort = args.take_option("--sort", "total-height");
    query.order = args.take_option("--order", "desc");
    query.activity = args.take_option("--activity", "all");
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
    LimitLadderService service(load_blocks(root, {"research-industry", "concept"}));
    const auto rendered = service.query(query).dump(compact ? -1 : 2) + '\n';
    if (output_name.empty()) std::cout << rendered;
    else {
        const auto output = native_path(output_name);
        atomic_write_text(output, rendered);
        std::cout << "completed limit-ladder query -> " << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
