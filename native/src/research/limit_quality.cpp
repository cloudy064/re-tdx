#include "tdx/limit_quality.hpp"

#include "tdx/auction.hpp"
#include "tdx/common.hpp"
#include "tdx/market.hpp"
#include "tdx/ranking.hpp"
#include "tdx/stats.hpp"

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

const Json* member(const Json& value, const std::string& key) {
    if (!value.is_object()) return nullptr;
    const auto found = value.as_object().find(key);
    return found == value.as_object().end() ? nullptr : &found->second;
}

std::string text(const Json& value, const std::string& key) {
    const auto* found = member(value, key);
    return found && found->is_string() ? found->as_string() : std::string{};
}

std::optional<double> number(const Json& value, const std::string& key) {
    const auto* found = member(value, key);
    if (!found || !found->is_number()) return std::nullopt;
    const auto result = found->as_number();
    return std::isfinite(result) ? std::optional<double>(result) : std::nullopt;
}

Json optional_number(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

std::string normalize_date(std::string value) {
    value = trim(std::move(value));
    if (value.size() >= 10 && (value[4] == '-' || value[4] == '/')) {
        const auto separator = value[4];
        const auto second = value.find(separator, 5);
        if (second == std::string::npos) return {};
        try {
            const int year = std::stoi(value.substr(0, 4));
            const int month = std::stoi(value.substr(5, second - 5));
            const int day = std::stoi(value.substr(second + 1));
            if (year < 1990 || year > 2200 || month < 1 || month > 12 ||
                day < 1 || day > 31) return {};
            std::ostringstream output;
            output << std::setw(4) << std::setfill('0') << year << '-'
                   << std::setw(2) << month << '-' << std::setw(2) << day;
            return output.str();
        } catch (...) { return {}; }
    }
    std::string digits;
    for (const char ch : value) {
        if (ch >= '0' && ch <= '9') digits.push_back(ch);
        else if (!digits.empty()) break;
    }
    if (digits.size() < 8) return {};
    digits.resize(8);
    return digits.substr(0, 4) + '-' + digits.substr(4, 2) + '-' + digits.substr(6, 2);
}

// Howard Hinnant 的 civil-date 映射；只用于比较两个业务日期的自然日间隔。
long long civil_days(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<long long>(era) * 146097 + static_cast<long long>(doe);
}

std::optional<int> date_gap(const std::string& newer, const std::string& older) {
    if (newer.size() != 10 || older.size() != 10) return std::nullopt;
    try {
        const auto days = [](const std::string& value) {
            return civil_days(std::stoi(value.substr(0, 4)),
                              static_cast<unsigned>(std::stoi(value.substr(5, 2))),
                              static_cast<unsigned>(std::stoi(value.substr(8, 2))));
        };
        return static_cast<int>(days(newer) - days(older));
    } catch (...) { return std::nullopt; }
}

std::map<std::string, const Json*> index_records(const Json& document) {
    std::map<std::string, const Json*> result;
    const auto* records = member(document, "records");
    if (!records || !records->is_array()) return result;
    for (const auto& row : records->as_array()) {
        const auto id = text(row, "security_id");
        if (!id.empty()) result[id] = &row;
    }
    return result;
}

std::optional<double> nested_number(const Json* value, const std::string& child,
                                    const std::string& key) {
    if (!value) return std::nullopt;
    const auto* nested = member(*value, child);
    return nested ? number(*nested, key) : std::nullopt;
}

Json empty_source() {
    Json value = Json::object();
    value["records"] = Json::array();
    return value;
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
    output << std::put_time(&local, "%Y-%m-%dT%H:%M:%S");
    return output.str();
}

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

int bounded(const std::string& value, std::string_view name, int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int parsed = std::stoi(value, &used);
        if (used != value.size() || parsed < minimum || parsed > maximum)
            throw std::invalid_argument("range");
        return parsed;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

}  // namespace

Json compose_limit_quality_document(const Json& ranking_document,
                                    const Json& depth_document,
                                    const Json& stats_document,
                                    const Json& auction_document,
                                    int limit) {
    if (limit < 1 || limit > 500) throw Error("limit-quality limit must be in 1..500");
    const auto* rankings = member(ranking_document, "records");
    if (!rankings || !rankings->is_array())
        throw Error("limit-quality ranking document has no records");
    const auto depths = index_records(depth_document);
    const auto stats = index_records(stats_document);
    const auto auctions = index_records(auction_document);
    auto trade_date = normalize_date(text(auction_document, "server_trade_date"));
    if (trade_date.empty()) trade_date = normalize_date(text(ranking_document, "generated_at"));

    Json records = Json::array();
    double total_seal = 0.0;
    std::uint64_t depth_sealed = 0, depth_changed = 0, stats_linked = 0;
    std::uint64_t stats_comparable = 0, stats_matched = 0, history_comparable = 0;
    std::uint64_t auction_linked = 0;
    for (const auto& ranking : rankings->as_array()) {
        if (records.size() >= static_cast<std::size_t>(limit)) break;
        const auto id = text(ranking, "security_id");
        if (id.empty()) continue;
        const auto depth_it = depths.find(id);
        const auto stats_it = stats.find(id);
        const auto auction_it = auctions.find(id);
        const Json* depth = depth_it == depths.end() ? nullptr : depth_it->second;
        const Json* stat_record = stats_it == stats.end() ? nullptr : stats_it->second;
        const Json* auction = auction_it == auctions.end() ? nullptr : auction_it->second;

        Json value = ranking;
        const auto last = depth ? number(*depth, "last_price") : number(ranking, "last_price");
        const auto bid_price = depth ? nested_number(depth, "buy_levels", "price")
                                     : number(ranking, "bid1_price");
        // buy_levels 是数组，直接读取首档。
        std::optional<double> depth_bid_price, depth_bid_volume, depth_ask_price,
                              depth_ask_volume;
        if (depth) {
            const auto* buys = member(*depth, "buy_levels");
            const auto* sells = member(*depth, "sell_levels");
            if (buys && buys->is_array() && !buys->as_array().empty()) {
                depth_bid_price = number(buys->as_array().front(), "price");
                depth_bid_volume = number(buys->as_array().front(), "volume_hand");
            }
            if (sells && sells->is_array() && !sells->as_array().empty()) {
                depth_ask_price = number(sells->as_array().front(), "price");
                depth_ask_volume = number(sells->as_array().front(), "volume_hand");
            }
        }
        (void)bid_price;
        const auto current_seal = depth ? number(*depth, "bid1_amount_yuan")
                                        : number(ranking, "seal_amount_yuan");
        const bool sealed = depth && last && depth_bid_price && depth_bid_volume &&
            *last > 0.0 && *depth_bid_volume > 0.0 &&
            std::abs(*last - *depth_bid_price) < 0.0001 &&
            (!depth_ask_price || *depth_ask_price == 0.0) &&
            (!depth_ask_volume || *depth_ask_volume == 0.0);
        if (sealed) ++depth_sealed;
        else if (depth) ++depth_changed;
        if (current_seal) total_seal += *current_seal;

        const auto free_shares = nested_number(stat_record, "stat", "free_float_shares");
        const auto free_value = free_shares && last && *free_shares > 0.0 && *last > 0.0
            ? std::optional<double>(*free_shares * *last) : std::nullopt;
        const auto seal_free_pct = current_seal && free_value && *free_value > 0.0
            ? std::optional<double>(*current_seal * 100.0 / *free_value) : std::nullopt;

        std::string alignment = "stats-unavailable";
        std::optional<double> previous_seal, statistics_current_seal;
        std::string stats_date;
        if (stat_record) {
            ++stats_linked;
            const auto* stat2 = member(*stat_record, "stat2");
            if (stat2) {
                stats_date = normalize_date(text(*stat2, "stats_date"));
                const auto gap = date_gap(trade_date, stats_date);
                if (gap && *gap == 0) {
                    alignment = "same-day";
                    statistics_current_seal = number(*stat2, "seal_amount_yuan");
                    previous_seal = number(*stat2, "prev_seal_amount_yuan");
                } else if (gap && *gap >= 1 && *gap <= 7) {
                    alignment = "previous-resource-day";
                    previous_seal = number(*stat2, "seal_amount_yuan");
                } else alignment = "stats-date-unaligned";
            }
        }
        const auto seal_to_previous = current_seal && previous_seal && *previous_seal > 0.0
            ? std::optional<double>(*current_seal / *previous_seal) : std::nullopt;
        const auto seal_change = seal_to_previous
            ? std::optional<double>((*seal_to_previous - 1.0) * 100.0) : std::nullopt;
        const auto seal_decay = seal_to_previous
            ? std::optional<double>((1.0 - *seal_to_previous) * 100.0) : std::nullopt;
        if (seal_to_previous) ++history_comparable;
        const auto consistency_error = current_seal && statistics_current_seal
            ? std::optional<double>(std::abs(*current_seal - *statistics_current_seal))
            : std::nullopt;
        if (consistency_error) {
            ++stats_comparable;
            if (*consistency_error <= 100.0) ++stats_matched;
        }

        Json auction_quality = Json(nullptr);
        if (auction) {
            ++auction_linked;
            auction_quality = Json::object();
            const auto* summary = member(*auction, "summary");
            const auto* opening = summary ? member(*summary, "opening") : nullptr;
            const auto virtual_price = opening ? number(*opening, "last_sample_price")
                                               : std::nullopt;
            const auto formal_open = number(ranking, "open_price");
            const auto recomputed_rush = virtual_price && formal_open && *virtual_price > 0.0
                ? std::optional<double>((*formal_open / *virtual_price - 1.0) * 100.0)
                : std::nullopt;
            const auto native_rush = number(ranking, "opening_rush");
            auction_quality["last_virtual_price"] = optional_number(virtual_price);
            auction_quality["formal_open_price"] = optional_number(formal_open);
            auction_quality["opening_rush_pct"] = optional_number(native_rush);
            auction_quality["recomputed_opening_rush_pct"] = optional_number(recomputed_rush);
            auction_quality["opening_rush_error_pct_point"] =
                native_rush && recomputed_rush
                    ? Json(std::abs(*native_rush - *recomputed_rush)) : Json(nullptr);
            if (opening) {
                for (const auto* key : {"last_sample_matched_volume_hand",
                                        "last_sample_matched_amount_yuan",
                                        "last_sample_unmatched_signed_hand",
                                        "last_sample_unmatched_volume_hand",
                                        "last_sample_unmatched_direction_raw",
                                        "unmatched_direction_flips"}) {
                    const auto* found = member(*opening, key);
                    auction_quality[key] = found ? *found : Json(nullptr);
                }
            }
        }

        Json metrics = Json::object();
        metrics["depth_status"] = depth ? (sealed ? "sealed" : "ranking-stale-or-unsealed")
                                        : "depth-missing";
        metrics["current_seal_amount_yuan"] = optional_number(current_seal);
        metrics["free_float_market_value_yuan"] = optional_number(free_value);
        metrics["seal_free_float_pct"] = optional_number(seal_free_pct);
        metrics["stats_date"] = stats_date.empty() ? Json(nullptr) : Json(stats_date);
        metrics["stats_alignment"] = alignment;
        metrics["previous_positive_seal_amount_yuan"] =
            previous_seal && *previous_seal > 0.0 ? Json(*previous_seal) : Json(nullptr);
        metrics["seal_to_previous"] = optional_number(seal_to_previous);
        metrics["seal_change_pct"] = optional_number(seal_change);
        metrics["seal_decay_pct"] = optional_number(seal_decay);
        metrics["statistics_current_seal_amount_yuan"] =
            optional_number(statistics_current_seal);
        metrics["statistics_consistency_error_yuan"] = optional_number(consistency_error);
        metrics["statistics_matches_depth"] = consistency_error
            ? Json(*consistency_error <= 100.0) : Json(nullptr);
        metrics["auction"] = std::move(auction_quality);
        value["quality"] = std::move(metrics);
        records.push_back(std::move(value));
    }

    Json summary = Json::object();
    summary["records"] = static_cast<std::uint64_t>(records.size());
    summary["depth_sealed"] = depth_sealed;
    summary["ranking_stale_or_unsealed"] = depth_changed;
    summary["total_current_seal_amount_yuan"] = total_seal;
    summary["stats_linked"] = stats_linked;
    summary["history_comparable"] = history_comparable;
    summary["statistics_comparable"] = stats_comparable;
    summary["statistics_matched"] = stats_matched;
    summary["auction_linked"] = auction_linked;
    Json result = Json::object();
    result["schema"] = "tdx-market-limit-quality-native-v1";
    result["generated_at"] = now_text();
    result["trade_date"] = trade_date.empty() ? Json(nullptr) : Json(trade_date);
    result["sources"] = Json::parse(
        "{\"ranking\":\"0x054B\",\"depth\":\"0x0547\","
        "\"statistics\":\"0x06B9/zhb.zip\",\"auction\":\"0x056A\"}");
    result["summary"] = std::move(summary);
    result["records"] = std::move(records);
    return result;
}

Json fetch_market_limit_quality_document(const fs::path& root, int limit,
                                         int auction_limit, int timeout_ms,
                                         const BlockData* block_data,
                                         const std::vector<std::string>& hosts) {
    if (limit < 1 || limit > 500) throw Error("limit must be in 1..500");
    if (auction_limit < 0 || auction_limit > 50)
        throw Error("auction_limit must be in 0..50");
    auto ranking = fetch_market_ranking_document(root, "a-shares", "seal-amount",
        0, 80, false, 0, true, timeout_ms, block_data, hosts);
    std::vector<std::string> securities;
    for (const auto& row : ranking.at("records").as_array()) {
        if (securities.size() >= static_cast<std::size_t>(limit)) break;
        const auto id = text(row, "security_id");
        if (!id.empty()) securities.push_back(id);
    }
    if (securities.empty()) return compose_limit_quality_document(
        ranking, empty_source(), empty_source(), empty_source(), limit);
    const auto depth = fetch_market_depth_document(root, securities, timeout_ms, block_data);
    Json source_errors = Json::array();
    Json stats = empty_source();
    try {
        stats = fetch_market_stats_document(root, securities, "zhb.zip", 30000,
                                            timeout_ms, block_data, hosts);
    } catch (const std::exception& error) {
        Json failure = Json::object();
        failure["source"] = "statistics";
        failure["message"] = error.what();
        source_errors.push_back(std::move(failure));
    }
    Json auction = empty_source();
    if (auction_limit > 0) {
        const auto count = std::min<std::size_t>(securities.size(), auction_limit);
        try {
            auction = fetch_market_auction_document(root,
                std::vector<std::string>(securities.begin(), securities.begin() + count),
                3, 0, 500, timeout_ms, block_data, hosts);
        } catch (const std::exception& error) {
            Json failure = Json::object();
            failure["source"] = "auction";
            failure["message"] = error.what();
            source_errors.push_back(std::move(failure));
        }
    }
    auto result = compose_limit_quality_document(ranking, depth, stats, auction, limit);
    result["source_errors"] = std::move(source_errors);
    result["requested_limit"] = limit;
    result["auction_limit"] = auction_limit;
    return result;
}

int command_market_limit_quality(const std::vector<std::string>& values) {
    Args args(values);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market limit-quality [options]\n\n"
            "Combine the live sealed ranking, five-level depth, statistics, and auction series.\n\n"
            "Options:\n"
            "  --limit N              Maximum sealed records, 1..500 (default 200)\n"
            "  --auction-limit N      Enrich top 0..50 with auction series (default 20)\n"
            "  --host HOST[:PORT]     Repeatable quote endpoint\n"
            "  --timeout-ms N         Default 10000\n"
            "  --root PATH            TDX installation root\n"
            "  --compact              Write compact JSON\n"
            "  --output PATH           Default output/tdx-market-limit-quality.json\n";
        return 0;
    }
    const int limit = bounded(args.take_option("--limit", "200"), "--limit", 1, 500);
    const int auction_limit = bounded(args.take_option("--auction-limit", "20"),
                                      "--auction-limit", 0, 50);
    const int timeout_ms = bounded(args.take_option("--timeout-ms", "10000"),
                                   "--timeout-ms", 1, 600000);
    const auto hosts = args.take_options("--host");
    const auto root_text = args.take_option("--root");
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-limit-quality.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    const auto document = fetch_market_limit_quality_document(
        root, limit, auction_limit, timeout_ms, nullptr, hosts);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "combined " << document.at("summary").at("records").as_number()
              << " sealed candidates -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
