#include "tdx/block_backtest.hpp"
#include "tdx/time.hpp"

#include "tdx/cloud_workflow.hpp"
#include "tdx/common.hpp"
#include "tdx/pbrpc.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace tdx {
namespace {

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
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream output;
    output << local_timestamp_text(local);
    return output.str();
}

const Json* field(const Json& row, std::string_view key) {
    if (!row.is_object()) return nullptr;
    const auto exact = row.as_object().find(key);
    if (exact != row.as_object().end()) return &exact->second;
    const auto wanted = lower_ascii(std::string(key));
    for (const auto& [name, value] : row.as_object())
        if (lower_ascii(name) == wanted) return &value;
    return nullptr;
}

std::string text(const Json& row, std::string_view key) {
    const auto* value = field(row, key);
    if (!value || value->is_null()) return {};
    if (value->is_string()) return trim(value->as_string());
    if (value->is_number()) {
        std::ostringstream output; output << std::setprecision(15) << value->as_number();
        return output.str();
    }
    return {};
}

std::optional<double> number(const Json& row, std::string_view key) {
    const auto* value = field(row, key);
    if (!value || value->is_null()) return std::nullopt;
    if (value->is_number()) return value->as_number();
    if (!value->is_string()) return std::nullopt;
    auto raw = trim(value->as_string());
    raw.erase(std::remove(raw.begin(), raw.end(), ','), raw.end());
    if (!raw.empty() && raw.back() == '%') raw.pop_back();
    if (raw.empty() || raw == "--" || raw == "-") return std::nullopt;
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(raw, &used);
        if (used == raw.size() && std::isfinite(parsed)) return parsed;
    } catch (...) {}
    return std::nullopt;
}

Json number_json(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

std::string compact_date(std::string value, std::string_view name) {
    value = trim(std::move(value));
    if (value.size() == 10 && value[4] == '-' && value[7] == '-')
        value = value.substr(0, 4) + value.substr(5, 2) + value.substr(8, 2);
    if (value.size() != 8 || !std::all_of(value.begin(), value.end(), ::isdigit))
        throw Error(std::string(name) + " must be YYYYMMDD or YYYY-MM-DD");
    const int year = std::stoi(value.substr(0, 4));
    const int month = std::stoi(value.substr(4, 2));
    const int day = std::stoi(value.substr(6, 2));
    static const int month_days[]{0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
    const int maximum_day = month == 2 ? month_days[month] + (leap ? 1 : 0) :
                            (month >= 1 && month <= 12 ? month_days[month] : 0);
    if (year < 1990 || year > 2099 || month < 1 || month > 12 ||
        day < 1 || day > maximum_day)
        throw Error(std::string(name) + " is outside the supported calendar range");
    return value;
}

std::string display_date(const std::string& value) {
    if (value.size() == 8 && std::all_of(value.begin(), value.end(), ::isdigit))
        return value.substr(0, 4) + "-" + value.substr(4, 2) + "-" + value.substr(6, 2);
    return value;
}

int category_id(const std::string& raw) {
    const auto value = lower_ascii(trim(raw));
    if (value == "all" || value == "0") return 0;
    if (value == "industry" || value == "1") return 1;
    if (value == "concept" || value == "2") return 2;
    if (value == "style" || value == "3") return 3;
    if (value == "region" || value == "4") return 4;
    throw Error("category must be all, industry, concept, style, or region");
}

std::string category_name(int value) {
    return value == 0 ? "all" : value == 1 ? "industry" : value == 2 ? "concept" :
           value == 3 ? "style" : "region";
}

std::string category_family(int value) {
    return value == 1 ? "industry" : value == 2 ? "concept" :
           value == 3 ? "style" : std::string{};
}

int adjustment_id(const std::string& raw) {
    const auto value = lower_ascii(trim(raw));
    if (value == "none" || value == "0") return 0;
    if (value == "forward" || value == "qfq" || value == "1") return 1;
    if (value == "backward" || value == "hfq" || value == "2") return 2;
    throw Error("adjustment must be none, forward, or backward");
}

std::string adjustment_name(int value) {
    return value == 0 ? "none" : value == 1 ? "forward" : "backward";
}

int market_id(std::string raw) {
    raw = trim(std::move(raw));
    if (raw == "0") return 0;
    if (raw == "1") return 1;
    if (raw == "2" || raw == "44") return 2;
    throw Error("block backtest row has an invalid security market: " + raw);
}

std::string market_name(int value) { return value == 0 ? "sz" : value == 1 ? "sh" : "bj"; }
std::string market_prefix(int value) { return value == 0 ? "SZ" : value == 1 ? "SH" : "BJ"; }

Json block_identity(const Json& row, int category, const BlockData& blocks) {
    const auto code = text(row, "code");
    auto name = text(row, "name");
    const auto wanted_family = category_family(category);
    std::vector<const Block*> matches;
    for (const auto& block : blocks.blocks) {
        if (block.block_code != code) continue;
        if (!wanted_family.empty() && block.family != wanted_family) continue;
        matches.push_back(&block);
    }
    if (matches.empty() && category == 0) {
        for (const auto& block : blocks.blocks)
            if (block.block_code == code) matches.push_back(&block);
    }
    if (name.empty() && !matches.empty()) name = matches.front()->name;
    Json result = Json::object();
    result["entity_type"] = "block";
    result["code"] = code;
    result["name"] = name;
    result["upstream_setcode"] = text(row, "setcode");
    result["name_resolved"] = !name.empty();
    result["block_id"] = matches.empty() ? Json(nullptr) : Json(matches.front()->block_id);
    result["family"] = matches.empty() ? Json(nullptr) : Json(matches.front()->family);
    result["family_name"] = matches.empty() ? Json(nullptr) : Json(matches.front()->family_name);
    result["local_match_count"] = static_cast<std::uint64_t>(matches.size());
    return result;
}

Json security_identity(const Json& row, const BlockData& blocks) {
    const auto code = text(row, "code");
    const int market = market_id(text(row, "setcode"));
    auto name = text(row, "name");
    const auto known = blocks.securities.find({market, code});
    if (name.empty() && known != blocks.securities.end()) name = known->second.name;
    Json result = Json::object();
    result["entity_type"] = "security";
    result["market"] = market_name(market);
    result["market_id"] = market;
    result["code"] = code;
    result["security_id"] = market_prefix(market) + code;
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    return result;
}

double sortable_number(const Json& row, std::string_view key, bool descending) {
    const auto value = number(row, key);
    if (value) return *value;
    return descending ? -std::numeric_limits<double>::infinity()
                      : std::numeric_limits<double>::infinity();
}

std::string sort_field(const std::string& raw) {
    const auto value = lower_ascii(trim(raw));
    if (value == "return") return "return_pct";
    if (value == "net-inflow") return "net_inflow";
    if (value == "main-net-inflow") return "main_net_inflow";
    if (value == "max-drawdown") return "max_drawdown_pct";
    if (value == "turnover") return "turnover_pct";
    if (value == "code") return "code";
    throw Error("sort must be return, net-inflow, main-net-inflow, max-drawdown, turnover, or code");
}

std::string cache_key(const BlockBacktestQuery& query) {
    std::ostringstream output;
    output << lower_ascii(query.category) << '|' << query.block_code << '|'
           << query.begin_date << '|' << query.end_date << '|'
           << lower_ascii(query.adjustment) << '|' << lower_ascii(query.sort) << '|'
           << lower_ascii(query.order) << '|' << query.limit;
    return output.str();
}

int bounded(const std::string& raw, std::string_view name, int minimum, int maximum) {
    try {
        std::size_t used = 0; const int value = std::stoi(raw, &used);
        if (used != raw.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

}  // namespace

Json normalize_block_backtest_rows(
    const Json& rows, const BlockBacktestQuery& input, const BlockData& blocks) {
    if (!rows.is_array()) throw Error("block backtest rows must be an array");
    BlockBacktestQuery query = input;
    const int category = category_id(query.category);
    const bool detail = !trim(query.block_code).empty();
    const auto order = lower_ascii(trim(query.order));
    if (order != "asc" && order != "desc") throw Error("order must be asc or desc");
    const auto sort = sort_field(query.sort);

    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        if (!row.is_object() || text(row, "code").empty()) continue;
        Json item = Json::object();
        if (detail) item["security"] = security_identity(row, blocks);
        else item["block"] = block_identity(row, category, blocks);
        item["trade_date"] = display_date(text(row, "trade_date"));
        item["base_close"] = number_json(number(row, "qspj"));
        item["high"] = number_json(number(row, "zgj"));
        item["low"] = number_json(number(row, "zdj"));
        item["close"] = number_json(number(row, "spj"));
        item["return_pct"] = number_json(number(row, "qjzdf"));
        item["max_daily_gain_pct"] = number_json(number(row, "qjzdzf"));
        item["amplitude_pct"] = number_json(number(row, "qjzf"));
        item["max_drawdown_pct"] = number_json(number(row, "qjzdhc"));
        item["volume"] = number_json(number(row, "qjcjl"));
        item["amount"] = number_json(number(row, "qjcje"));
        item["turnover_pct"] = number_json(number(row, "qjhsl"));
        item["net_inflow"] = number_json(number(row, "qjjlr"));
        item["main_net_inflow"] = number_json(number(row, "qjzllr"));
        result.push_back(std::move(item));
    }

    auto& values = result.as_array();
    const bool descending = order == "desc";
    std::stable_sort(values.begin(), values.end(), [&](const Json& left, const Json& right) {
        if (sort == "code") {
            const auto& left_entity = detail ? left.at("security") : left.at("block");
            const auto& right_entity = detail ? right.at("security") : right.at("block");
            const auto left_code = text(left_entity, "code");
            const auto right_code = text(right_entity, "code");
            return descending ? left_code > right_code : left_code < right_code;
        }
        const auto left_value = sortable_number(left, sort, descending);
        const auto right_value = sortable_number(right, sort, descending);
        return descending ? left_value > right_value : left_value < right_value;
    });
    for (std::size_t index = 0; index < values.size(); ++index)
        values[index]["rank"] = static_cast<std::uint64_t>(index + 1);
    return result;
}

BlockBacktestService::BlockBacktestService(fs::path root, BlockData blocks)
    : root_(std::move(root)), blocks_(std::move(blocks)) {}

Json BlockBacktestService::query(const BlockBacktestQuery& input) {
    BlockBacktestQuery options = input;
    const int category = category_id(options.category);
    options.category = category_name(category);
    options.block_code = trim(options.block_code);
    if (!options.block_code.empty() &&
        (options.block_code.size() != 6 ||
         !std::all_of(options.block_code.begin(), options.block_code.end(), ::isdigit)))
        throw Error("block_code must contain exactly six digits");
    options.begin_date = compact_date(options.begin_date, "begin_date");
    options.end_date = compact_date(options.end_date, "end_date");
    if (options.begin_date > options.end_date) throw Error("begin_date must not exceed end_date");
    options.adjustment = adjustment_name(adjustment_id(options.adjustment));
    options.sort = lower_ascii(trim(options.sort));
    (void)sort_field(options.sort);
    options.order = lower_ascii(trim(options.order));
    if (options.order != "asc" && options.order != "desc") throw Error("order must be asc or desc");
    if (options.limit < 1 || options.limit > 20000 || options.cache_ttl_seconds < 0 ||
        options.cache_ttl_seconds > 86400 || options.timeout_ms < 100 ||
        options.timeout_ms > 600000)
        throw Error("block backtest query limits are invalid");

    const auto key = cache_key(options);
    const auto now = std::time(nullptr);
    const auto cached = cache_.find(key);
    if (!options.refresh && cached != cache_.end()) {
        const int age = static_cast<int>(std::max<std::time_t>(0, now - cached->second.fetched_at));
        if (age < options.cache_ttl_seconds) {
            auto result = cached->second.document;
            result["cache"]["hit"] = true;
            result["cache"]["age_seconds"] = age;
            return result;
        }
    }

    std::map<std::string, std::string> replacements{
        {"AdjustType", std::to_string(adjustment_id(options.adjustment))},
        {"BeginDate", options.begin_date}, {"EndDate", options.end_date}};
    std::vector<std::string> selectors;
    if (options.block_code.empty()) {
        selectors.push_back("12:" + std::to_string(category));
    } else {
        replacements["code"] = options.block_code;
        selectors.push_back("2:$$code$$|1");
    }

    Json upstream;
    for (int attempt = 0; attempt < 3; ++attempt) {
        try {
            upstream = execute_pbrpc_config(
                root_, "200199", replacements, {}, {}, {}, "BK_BKLSHC.xml", selectors,
                cloud_endpoints::tqlex, options.timeout_ms);
            break;
        } catch (const Error& error) {
            const std::string message = error.what();
            const bool transient = message.find("PBRPC HTTP status 429") != std::string::npos ||
                                   message.find("PBRPC HTTP status 502") != std::string::npos ||
                                   message.find("PBRPC HTTP status 503") != std::string::npos ||
                                   message.find("PBRPC HTTP status 504") != std::string::npos ||
                                   message.find("PBRPC business ErrorCode 4") != std::string::npos;
            if (!transient) throw;
            if (attempt == 2) {
                if (cached != cache_.end()) {
                    auto stale = cached->second.document;
                    stale["availability"] = "stale-cache";
                    stale["cache"]["hit"] = true;
                    stale["cache"]["stale"] = true;
                    stale["cache"]["age_seconds"] = static_cast<std::uint64_t>(
                        std::max<std::time_t>(0, now - cached->second.fetched_at));
                    stale["cache"]["upstream_error"] = message;
                    return stale;
                }
                throw;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(250 * (attempt + 1)));
        }
    }

    const auto raw_rows = cloud_result_rows(upstream.at("response"));
    auto records = normalize_block_backtest_rows(raw_rows, options, blocks_);
    const auto full_count = records.size();
    std::size_t gainers = 0, losers = 0, flat = 0, resolved = 0;
    std::vector<double> returns;
    std::set<std::string> observed_dates;
    for (const auto& record : records.as_array()) {
        const auto value = number(record, "return_pct");
        if (value) {
            returns.push_back(*value);
            if (*value > 0) ++gainers; else if (*value < 0) ++losers; else ++flat;
        }
        observed_dates.insert(text(record, "trade_date"));
        const auto& entity = options.block_code.empty() ? record.at("block") : record.at("security");
        if (field(entity, options.block_code.empty() ? "block_id" : "security_id") &&
            !field(entity, options.block_code.empty() ? "block_id" : "security_id")->is_null())
            ++resolved;
    }
    const bool truncated = records.size() > static_cast<std::size_t>(options.limit);
    if (truncated) records.as_array().resize(static_cast<std::size_t>(options.limit));

    Json summary = Json::object();
    summary["gainers"] = static_cast<std::uint64_t>(gainers);
    summary["losers"] = static_cast<std::uint64_t>(losers);
    summary["flat"] = static_cast<std::uint64_t>(flat);
    summary["positive_ratio_pct"] = returns.empty() ? Json(nullptr) :
        Json(100.0 * static_cast<double>(gainers) / static_cast<double>(returns.size()));
    if (!returns.empty()) {
        const double sum = std::accumulate(returns.begin(), returns.end(), 0.0);
        auto ordered = returns; std::sort(ordered.begin(), ordered.end());
        const auto middle = ordered.size() / 2;
        const double median = ordered.size() % 2 ? ordered[middle] :
            (ordered[middle - 1] + ordered[middle]) / 2.0;
        summary["average_return_pct"] = sum / static_cast<double>(returns.size());
        summary["median_return_pct"] = median;
    } else {
        summary["average_return_pct"] = Json(nullptr);
        summary["median_return_pct"] = Json(nullptr);
    }

    Json dates = Json::array();
    for (const auto& date : observed_dates) if (!date.empty()) dates.push_back(date);
    Json period = Json::object();
    period["requested_begin"] = display_date(options.begin_date);
    period["requested_end"] = display_date(options.end_date);
    period["observed_trade_dates"] = std::move(dates);
    period["adjustment"] = options.adjustment;

    Json selection = Json::object();
    selection["mode"] = options.block_code.empty() ? "blocks" : "members";
    selection["category"] = options.category;
    selection["block_code"] = options.block_code.empty() ? Json(nullptr) : Json(options.block_code);
    selection["membership_basis"] = options.block_code.empty() ? Json(nullptr) :
        Json("upstream-server-selection");
    selection["historical_membership_reconstructed"] = false;

    Json source = Json::object();
    source["transport"] = "PBRPC reqformat=22";
    source["request_id"] = "200199";
    source["source_file"] = upstream.at("source_file");
    source["module"] = upstream.at("module");
    source["rpc_id"] = upstream.at("rpc_id");
    source["rounds"] = upstream.at("rounds");
    source["raw_size"] = upstream.at("raw_size");

    Json counts = Json::object();
    counts["upstream_rows"] = static_cast<std::uint64_t>(raw_rows.size());
    counts["normalized"] = static_cast<std::uint64_t>(full_count);
    counts["identity_resolved"] = static_cast<std::uint64_t>(resolved);
    counts["returned"] = static_cast<std::uint64_t>(records.size());
    counts["truncated"] = truncated;
    Json cache = Json::object(); cache["hit"] = false; cache["stale"] = false;
    cache["age_seconds"] = 0; cache["ttl_seconds"] = options.cache_ttl_seconds;

    Json result = Json::object();
    result["schema"] = "tdx-block-backtest-native-v1";
    result["availability"] = "live";
    result["generated_at"] = now_text();
    result["selection"] = std::move(selection);
    result["period"] = std::move(period);
    result["sort"] = options.sort;
    result["order"] = options.order;
    result["summary"] = std::move(summary);
    result["counts"] = std::move(counts);
    result["records"] = std::move(records);
    result["source"] = std::move(source);
    result["cache"] = std::move(cache);
    result["raw_response_retained"] = false;
    cache_[key] = CachedDocument{result, now};
    return result;
}

int command_market_block_backtest(const std::vector<std::string>& values) {
    Args args(values);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market block-backtest --begin DATE --end DATE [options]\n\n"
            "Options:\n"
            "  --category all|industry|concept|style|region\n"
            "  --block-code CODE            Return server-selected members of one block\n"
            "  --adjustment none|forward|backward\n"
            "  --sort return|net-inflow|main-net-inflow|max-drawdown|turnover|code\n"
            "  --order asc|desc --limit N --refresh --cache-ttl-seconds N\n"
            "  --root PATH --timeout-ms N --output FILE --compact\n";
        return 0;
    }
    BlockBacktestQuery query;
    query.category = lower_ascii(trim(args.take_option("--category", "all")));
    query.block_code = trim(args.take_option("--block-code"));
    query.begin_date = trim(args.take_option("--begin"));
    query.end_date = trim(args.take_option("--end"));
    query.adjustment = lower_ascii(trim(args.take_option("--adjustment", "forward")));
    query.sort = lower_ascii(trim(args.take_option("--sort", "return")));
    query.order = lower_ascii(trim(args.take_option("--order", "desc")));
    query.refresh = args.take_flag("--refresh");
    query.limit = bounded(args.take_option("--limit", "5000"), "--limit", 1, 20000);
    query.cache_ttl_seconds = bounded(args.take_option("--cache-ttl-seconds", "300"),
                                      "--cache-ttl-seconds", 0, 86400);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 600000);
    const auto root_text = args.take_option("--root");
    const bool compact = args.take_flag("--compact");
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-block-backtest.json"));
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    BlockBacktestService service(root, load_blocks(
        root, {"industry", "research-industry", "concept", "style", "index"}));
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "received " << document.at("counts").at("returned").as_number()
              << " block backtest rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
