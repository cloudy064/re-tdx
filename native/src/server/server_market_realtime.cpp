#include "server_market_realtime_internal.hpp"

#include "tdx/auction.hpp"
#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/limit_quality.hpp"
#include "tdx/market.hpp"
#include "tdx/minute.hpp"
#include "tdx/options.hpp"
#include "tdx/professional_data.hpp"
#include "tdx/ranking.hpp"
#include "tdx/session_audit.hpp"
#include "tdx/stats.hpp"
#include "tdx/trades.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tdx::server_detail {

Json query_market_snapshot(const ApiState& state, const RequestTarget& target) {
    const auto [market, code] = query_kline_security(target);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "10000"),
                                      "timeout_ms", 100, 30000);
    if (market != "sz" && market != "sh" && market != "bj")
        return fetch_expansion_quote_document(market, code, timeout);
    return fetch_market_snapshot_document(state.root, {market + ":" + code}, timeout,
                                          &state.block_data);
}

Json query_market_expansion_timeline(const RequestTarget& target) {
    const auto [market, code] = query_kline_security(target);
    if (market == "sz" || market == "sh" || market == "bj")
        throw Error("expansion timeline requires market qz/qd/qs/cz/qg or numeric ID 3..255");
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "10000"),
                                      "timeout_ms", 100, 30000);
    return fetch_expansion_timeline_document(
        market, code, trim(query_value(target, "date")), timeout);
}

Json query_market_expansion_trades(const RequestTarget& target) {
    const auto [market, code] = query_kline_security(target);
    if (market == "sz" || market == "sh" || market == "bj")
        throw Error("expansion trades require market qz/qd/qs/cz/qg or numeric ID 3..255");
    const int start = parse_bounded(query_value(target, "start", "0"),
                                    "start", 0, std::numeric_limits<int>::max());
    const int page_size = parse_bounded(query_value(target, "page_size", "1800"),
                                        "page_size", 1, 1800);
    const int pages = parse_bounded(query_value(target, "pages", "1"),
                                    "pages", 1, 20);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "10000"),
                                      "timeout_ms", 100, 30000);
    return fetch_expansion_trades_document(
        market, code, trim(query_value(target, "date")),
        start, page_size, pages, timeout);
}

Json query_market_depth(const ApiState& state, const RequestTarget& target) {
    const auto [market, code] = query_security(target);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "10000"),
                                      "timeout_ms", 100, 30000);
    return fetch_market_depth_document(state.root, {market + ":" + code}, timeout,
                                       &state.block_data);
}

Json query_market_speed(const ApiState& state, const RequestTarget& target) {
    const auto [market, code] = query_security(target);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "10000"),
                                      "timeout_ms", 100, 30000);
    return fetch_market_speed_document(state.root, {market + ":" + code}, timeout,
                                       &state.block_data);
}

Json query_market_finance(const ApiState& state, const RequestTarget& target) {
    const auto [market, code] = query_security(target);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "10000"),
                                      "timeout_ms", 100, 30000);
    return fetch_finance_document({market + ":" + code},
                                  load_public_quote_endpoints(state.root).endpoints,
                                  timeout, 80,
                                  query_bool(target, "include_raw"));
}

Json query_market_capital(const ApiState& state, const RequestTarget& target) {
    const auto [market, code] = query_security(target);
    const auto source = lower_ascii(trim(query_value(target, "source", "online")));
    if (source == "local")
        return load_local_capital_changes_document(
            state.root, {market + ":" + code}, query_bool(target, "include_raw"));
    if (source != "online") throw Error("source must be online or local");
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "10000"),
                                      "timeout_ms", 100, 30000);
    return fetch_capital_changes_document(
        {market + ":" + code}, load_public_quote_endpoints(state.root).endpoints,
        timeout,
                                          query_bool(target, "include_raw"));
}

Json query_market_limits(const ApiState& state, const RequestTarget& target) {
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "10000"),
                                      "timeout_ms", 100, 60000);
    const int cache_ttl = parse_bounded(query_value(target, "cache_ttl_seconds", "86400"),
                                        "cache_ttl_seconds", 0, 604800);
    const bool refresh = query_bool(target, "refresh");
    const auto now = std::time(nullptr);
    const bool expired = !state.special_limits_cache || cache_ttl == 0 ||
        std::difftime(now, state.special_limits_cache_time) >= cache_ttl;
    if (expired) {
        bool loaded_disk = false;
        if (!refresh && !state.jsn_root.empty()) {
            const auto candidate = state.jsn_root.parent_path() / "tdx-market-limits.json";
            if (fs::is_regular_file(candidate)) {
                auto document = Json::parse(read_text_utf8(candidate));
                if (document.is_object() && document.as_object().find("records") !=
                        document.as_object().end()) {
                    state.special_limits_cache = std::make_shared<Json>(std::move(document));
                    state.special_limits_cache_source = path_utf8(candidate);
                    loaded_disk = true;
                }
            }
        }
        if (!loaded_disk) {
            state.special_limits_cache = std::make_shared<Json>(
                fetch_special_limits_document(
                    load_public_quote_endpoints(state.root).endpoints,
                    timeout, 0, 10000, false));
            state.special_limits_cache_source = "7709-live";
        }
        state.special_limits_cache_time = now;
    }
    if (!state.special_limits_cache) throw Error("special-limit cache is unavailable");
    Json document = *state.special_limits_cache;
    document["cache_source"] = state.special_limits_cache_source;
    document["cache_ttl_seconds"] = cache_ttl;
    document["cache_age_seconds"] = static_cast<std::int64_t>(
        std::max(0.0, std::difftime(std::time(nullptr), state.special_limits_cache_time)));
    document["cache_refreshed"] = expired;
    const bool has_market = target.query.find("market") != target.query.end();
    const bool has_code = target.query.find("code") != target.query.end();
    if (has_market != has_code) throw Error("market and code must be provided together");
    if (has_market) {
        const auto [market, code] = query_security(target);
        const auto wanted = (market == "sz" ? "SZ" : market == "sh" ? "SH" : "BJ") + code;
        Json filtered = Json::array();
        for (const auto& record : document.at("records").as_array())
            if (record.at("security_id").as_string() == wanted) filtered.push_back(record);
        document["total_count"] = static_cast<std::uint64_t>(
            document.at("records").as_array().size());
        document["count"] = static_cast<std::uint64_t>(filtered.size());
        document["records"] = std::move(filtered);
        document["selected_security_id"] = wanted;
    }
    return document;
}

Json query_market_auction(const ApiState& state, const RequestTarget& target) {
    const auto [market, code] = query_security(target);
    const auto selector = parse_bounded(query_value(target, "selector", "3"),
                                        "selector", 0, 1000000);
    const auto start_raw = parse_bounded(query_value(target, "start_raw", "0"),
                                         "start_raw", 0, 1000000);
    const auto limit = parse_bounded(query_value(target, "limit", "500"),
                                     "limit", 1, 5000);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "10000"),
                                      "timeout_ms", 100, 30000);
    return fetch_market_auction_document(state.root, {market + ":" + code},
        static_cast<std::uint32_t>(selector), static_cast<std::uint32_t>(start_raw),
        static_cast<std::uint32_t>(limit), timeout, &state.block_data);
}

Json query_market_ranking(const ApiState& state, const RequestTarget& target) {
    const auto category = trim(query_value(target, "category", "a-shares"));
    const auto sort = trim(query_value(target, "sort", "rise-speed"));
    const int start = parse_bounded(query_value(target, "start", "0"), "start", 0, 65535);
    const int count = parse_bounded(query_value(target, "count", "80"), "count", 1, 80);
    const int filter_raw = parse_bounded(query_value(target, "filter_raw", "0"),
                                         "filter_raw", 0, 65535);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "10000"),
                                      "timeout_ms", 100, 30000);
    return fetch_market_ranking_document(state.root, category, sort, start, count,
        query_bool(target, "ascending"), filter_raw, query_bool(target, "all_sealed"),
        timeout, &state.block_data);
}

Json query_market_trades(const ApiState& state, const RequestTarget& target) {
    const auto [market, code] = query_security(target);
    const auto date = trim(query_value(target, "date"));
    const int page_size = parse_bounded(query_value(target, "page_size", "0"),
                                        "page_size", 0, 65535);
    const int max_pages = parse_bounded(query_value(target, "max_pages", "100"),
                                        "max_pages", 1, 100);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "10000"),
                                      "timeout_ms", 100, 30000);
    return fetch_market_trades_document(state.root, {market + ":" + code}, date,
                                        page_size, max_pages, timeout, &state.block_data);
}

bool refresh_stats_cache(const ApiState& state, int timeout, bool refresh) {
    const auto now = std::time(nullptr);
    const bool expired = !state.stats_cache || state.stats_cache_time == 0 ||
                         std::difftime(now, state.stats_cache_time) >= 300.0;
    if (!refresh && !expired) return false;
    auto downloaded = download_stats_resource(
        {}, "zhb.zip", 30000, timeout, state.root);
    state.stats_cache = std::make_shared<TdxStatsResource>(std::move(downloaded.resource));
    state.stats_endpoint = downloaded.endpoint.address();
    state.stats_server_name = downloaded.server_name;
    state.stats_archive_size = downloaded.archive_size;
    state.stats_transport = std::move(downloaded.transport);
    state.stats_cache_time = now;
    return true;
}

Json query_market_stats(const ApiState& state, const RequestTarget& target) {
    const auto [market, code] = query_security(target);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "10000"),
                                      "timeout_ms", 100, 30000);
    const bool refresh = query_bool(target, "refresh");
    const bool refreshed = refresh_stats_cache(state, timeout, refresh);
    auto document = stats_resource_document(*state.stats_cache, state.stats_endpoint,
        state.stats_server_name, state.stats_archive_size, {market + ":" + code},
        &state.block_data, &state.stats_transport);
    if (query_bool(target, "valuation")) {
        const int market_id = market == "sz" ? 0 : market == "sh" ? 1 : 2;
        document["valuation"] = fetch_security_valuation_document(
            state.root, *state.stats_cache, market_id, code, timeout,
            &state.block_data);
    }
    document["cache_refreshed"] = refreshed;
    document["cache_ttl_seconds"] = 300;
    document["cache_age_seconds"] = static_cast<std::int64_t>(
        std::max(0.0, std::difftime(std::time(nullptr), state.stats_cache_time)));
    return document;
}

std::vector<int> query_professional_fields(const RequestTarget& target) {
    std::vector<int> result;
    for (const auto& item : split(query_value(target, "fields"), ',')) {
        const auto value = trim(item);
        if (!value.empty()) result.push_back(parse_bounded(value, "fields", 0, 584));
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

Json query_market_professional(const RequestTarget& target) {
    const auto kind = lower_ascii(trim(query_value(target, "kind", "catalog")));
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "30000"),
                                      "timeout_ms", 100, 60000);
    const bool refresh = query_bool(target, "refresh");
    const auto fields = query_professional_fields(target);
    if (kind == "catalog") return fetch_professional_catalog_document(timeout);
    if (kind == "market") {
        const auto records = fetch_professional_market_trading_data({}, timeout, refresh);
        return professional_trading_document(records, kind, "SH999999", fields,
            static_cast<std::uint32_t>(parse_bounded(query_value(target, "from", "0"),
                                       "from", 0, 99999999)),
            static_cast<std::uint32_t>(parse_bounded(query_value(target, "to", "99999999"),
                                       "to", 0, 99999999)),
            static_cast<std::size_t>(parse_bounded(query_value(target, "limit", "5000"),
                                                   "limit", 1, 100000)),
            query_bool(target, "history"));
    }
    if (kind != "stock" && kind != "board" && kind != "finance" &&
        kind != "finance-series")
        throw Error("kind must be catalog, finance, finance-series, stock, board, or market");
    const auto [market, code] = query_security(target);
    const int market_id = market == "sh" ? 1 : market == "bj" ? 2 : 0;
    if (kind == "finance-series") {
        return fetch_professional_finance_series_document(market_id, code, fields,
            static_cast<std::uint32_t>(parse_bounded(query_value(target, "from", "0"),
                                       "from", 0, 99999999)),
            static_cast<std::uint32_t>(parse_bounded(query_value(target, "to", "99999999"),
                                       "to", 0, 99999999)),
            static_cast<std::size_t>(parse_bounded(query_value(target, "limit", "8"),
                                                   "limit", 1, 40)),
            {}, timeout, refresh);
    }
    if (kind == "finance") {
        const int period = parse_bounded(query_value(target, "period"),
                                         "period", 19000101, 22001231);
        const auto data = fetch_professional_finance_data(
            static_cast<std::uint32_t>(period), {}, timeout, refresh);
        return professional_finance_document(data, market_id, code, fields);
    }
    const auto actual_market = kind == "board" ? "sh" : market;
    const auto records = fetch_professional_stock_trading_data(
        actual_market, code, {}, timeout, refresh);
    const auto security_id = (actual_market == "sh" ? "SH" : actual_market == "bj" ? "BJ" : "SZ") + code;
    return professional_trading_document(records, kind, security_id, fields,
        static_cast<std::uint32_t>(parse_bounded(query_value(target, "from", "0"),
                                   "from", 0, 99999999)),
        static_cast<std::uint32_t>(parse_bounded(query_value(target, "to", "99999999"),
                                   "to", 0, 99999999)),
        static_cast<std::size_t>(parse_bounded(query_value(target, "limit", "5000"),
                                               "limit", 1, 100000)),
        query_bool(target, "history"));
}

Json query_market_limit_quality(const ApiState& state, const RequestTarget& target) {
    const int limit = parse_bounded(query_value(target, "limit", "200"),
                                    "limit", 1, 500);
    const int auction_limit = parse_bounded(
        query_value(target, "auction_limit", "20"), "auction_limit", 0, 50);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "10000"),
                                      "timeout_ms", 100, 30000);
    const int cache_ttl = parse_bounded(
        query_value(target, "cache_ttl_seconds", "5"),
        "cache_ttl_seconds", 0, 300);
    const bool refresh = query_bool(target, "refresh");
    const auto now = std::time(nullptr);
    const bool expired = !state.limit_quality_cache ||
        state.limit_quality_cache_time == 0 ||
        std::difftime(now, state.limit_quality_cache_time) >= cache_ttl ||
        state.limit_quality_cache_limit != limit ||
        state.limit_quality_cache_auction_limit != auction_limit;
    bool refreshed = false;
    if (refresh || expired) {
        auto ranking = fetch_market_ranking_document(state.root, "a-shares",
            "seal-amount", 0, 80, false, 0, true, timeout, &state.block_data);
        std::vector<std::string> securities;
        for (const auto& row : ranking.at("records").as_array()) {
            if (securities.size() >= static_cast<std::size_t>(limit)) break;
            if (!row.is_object()) continue;
            const auto found = row.as_object().find("security_id");
            if (found != row.as_object().end() && found->second.is_string())
                securities.push_back(found->second.as_string());
        }
        Json empty = Json::object();
        empty["records"] = Json::array();
        auto depth = securities.empty() ? empty : fetch_market_depth_document(
            state.root, securities, timeout, &state.block_data);
        (void)refresh_stats_cache(state, timeout, false);
        auto stats = stats_resource_document(*state.stats_cache, state.stats_endpoint,
            state.stats_server_name, state.stats_archive_size, securities,
            &state.block_data, &state.stats_transport);
        Json auction = empty;
        Json source_errors = Json::array();
        if (auction_limit > 0 && !securities.empty()) {
            const auto count = std::min<std::size_t>(securities.size(), auction_limit);
            try {
                auction = fetch_market_auction_document(state.root,
                    std::vector<std::string>(securities.begin(), securities.begin() + count),
                    3, 0, 500, timeout, &state.block_data);
            } catch (const std::exception& error) {
                Json failure = Json::object();
                failure["source"] = "auction";
                failure["message"] = error.what();
                source_errors.push_back(std::move(failure));
            }
        }
        auto combined = compose_limit_quality_document(
            ranking, depth, stats, auction, limit);
        combined["source_errors"] = std::move(source_errors);
        combined["requested_limit"] = limit;
        combined["auction_limit"] = auction_limit;
        combined["statistics_cache_age_seconds"] = static_cast<std::int64_t>(
            std::max(0.0, std::difftime(std::time(nullptr), state.stats_cache_time)));
        state.limit_quality_cache = std::make_shared<Json>(std::move(combined));
        state.limit_quality_cache_time = now;
        state.limit_quality_cache_limit = limit;
        state.limit_quality_cache_auction_limit = auction_limit;
        refreshed = true;
    }
    auto document = *state.limit_quality_cache;
    document["cache_refreshed"] = refreshed;
    document["cache_ttl_seconds"] = cache_ttl;
    document["cache_age_seconds"] = static_cast<std::int64_t>(
        std::max(0.0, std::difftime(std::time(nullptr),
                                   state.limit_quality_cache_time)));
    return document;
}

Json query_market_instruments(const RequestTarget& target) {
    const int start = parse_bounded(query_value(target, "start", "0"),
                                    "start", 0, 1000000);
    int count = parse_bounded(query_value(target, "count", "100"),
                              "count", 1, 200000);
    if (query_bool(target, "all")) count = 200000;
    int market_filter = -1;
    const auto requested_market = trim(query_value(target, "market"));
    if (!requested_market.empty()) {
        RequestTarget security;
        security.query["market"] = requested_market;
        security.query["code"] = "X";
        const auto [market, code] = query_kline_security(security);
        (void)code;
        if (market == "sz" || market == "sh" || market == "bj")
            throw Error("instrument directory market filter must identify an expansion market");
        market_filter = std::stoi(market);
    }
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "10000"),
                                      "timeout_ms", 100, 30000);
    return fetch_expansion_instruments_document(start, count, market_filter,
                                                query_value(target, "query"), timeout);
}

double query_option_number(const RequestTarget& target, const std::string& key,
                           double fallback, double minimum, double maximum) {
    const auto text = trim(query_value(target, key, std::to_string(fallback)));
    try {
        std::size_t used = 0;
        const double value = std::stod(text, &used);
        if (used != text.size() || !std::isfinite(value) ||
            value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(key + " must be a finite number in the supported range");
    }
}

int query_option_market(const RequestTarget& target) {
    auto market = lower_ascii(trim(query_value(target, "market")));
    if (market.empty()) return -1;
    if (market == "czce" || market == "cz") return 4;
    if (market == "dce" || market == "dc") return 5;
    if (market == "shfe" || market == "sq") return 6;
    if (market == "cffex" || market == "cf") return 7;
    if (market == "gfex" || market == "gf") return 67;
    return parse_bounded(market, "market", 3, 255);
}

std::string option_root_relative_path(const fs::path& root,
                                      const std::string& source) {
    std::error_code error;
    const auto canonical_root = fs::weakly_canonical(root, error);
    if (error) throw Error("failed to canonicalize TDX root");
    auto candidate = fs::path(source);
    if (!candidate.is_absolute()) candidate = canonical_root / candidate;
    error.clear();
    candidate = fs::weakly_canonical(candidate, error);
    if (error) throw Error("failed to canonicalize option resource path");
    const auto relative = candidate.lexically_relative(canonical_root);
    if (relative.empty() || relative.is_absolute())
        throw Error("option resource path is outside the TDX root");
    for (const auto& part : relative)
        if (part == "..")
            throw Error("option resource path is outside the TDX root");
    auto result = path_utf8(relative);
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

void project_option_resource_object(Json& value, const fs::path& root) {
    if (!value.is_object()) return;
    for (const auto* key : {"rules_source", "holiday_source"}) {
        if (value.as_object().count(key) && value.at(key).is_string()) {
            value[key] = option_root_relative_path(
                root, value.at(key).as_string());
        }
    }
    if (value.as_object().count("expiry_resolution"))
        project_option_resource_object(value["expiry_resolution"], root);
}

void project_option_resource_paths(Json& document, const fs::path& root) {
    project_option_resource_object(document, root);
    document["path_scope"] = "tdx-root-relative";
}

Json query_market_options(const RequestTarget& target) {
    const int limit = parse_bounded(query_value(target, "limit", "20000"),
                                    "limit", 1, 200000);
    const int cache_ttl = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "30000"),
                                      "timeout_ms", 100, 60000);
    return fetch_option_catalog_document(
        query_option_market(target), trim(query_value(target, "underlying")),
        trim(query_value(target, "contract")),
        trim(query_value(target, "type", "all")),
        trim(query_value(target, "query")), limit,
        query_bool(target, "refresh"), cache_ttl, timeout);
}

Json query_market_option_expiry(const ApiState& state, const RequestTarget& target) {
    const auto [market, code] = query_kline_security(target);
    if (market == "sz" || market == "sh" || market == "bj")
        throw Error("option expiry requires a TDX expansion-market option");
    const int market_id = parse_bounded(market, "market", 3, 255);
    const auto name = trim(query_value(target, "name"));
    std::optional<OptionInstrument> option;
    if (!name.empty()) option = parse_option_instrument(market_id, code, name);
    else {
        const int timeout = parse_bounded(query_value(target, "timeout_ms", "30000"),
                                          "timeout_ms", 100, 60000);
        const auto catalog = fetch_option_catalog_document(
            market_id, {}, {}, "all", code, 100, query_bool(target, "refresh"),
            parse_bounded(query_value(target, "cache_ttl_seconds", "300"),
                          "cache_ttl_seconds", 0, 86400), timeout);
        for (const auto& row : catalog.at("options").as_array())
            if (static_cast<int>(row.at("market_id").as_number()) == market_id &&
                row.at("code").as_string() == code) {
                option = parse_option_instrument(market_id, code, row.at("name").as_string());
                break;
            }
    }
    if (!option) throw Error("selected security is absent from the active TDX option catalog");
    auto document = resolve_option_expiry_document(state.root, *option);
    project_option_resource_paths(document, state.root);
    return document;
}

Json query_market_option_volatility(const ApiState& state, const RequestTarget& target) {
    const auto [market, code] = query_kline_security(target);
    if (market == "sz" || market == "sh" || market == "bj")
        throw Error("option volatility requires a TDX expansion-market option");
    const int lookback = parse_bounded(query_value(target, "lookback", "60"),
                                       "lookback", 2, 800);
    const int cache_ttl = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "30000"),
                                      "timeout_ms", 100, 60000);
    const double risk_free = query_option_number(target, "risk_free", 0.0187, -1.0, 1.0);
    std::optional<double> option_price;
    if (!trim(query_value(target, "option_price")).empty())
        option_price = query_option_number(target, "option_price", 0.0, 0.000001, 1e12);
    auto document = fetch_option_volatility_document(
        market, code, trim(query_value(target, "name")),
        trim(query_value(target, "date")), trim(query_value(target, "expiry")),
        lookback, risk_free, option_price, query_bool(target, "refresh"),
        cache_ttl, timeout, state.root);
    project_option_resource_paths(document, state.root);
    return document;
}

Json query_market_option_chain(const ApiState& state, const RequestTarget& target) {
    const int market = query_option_market(target);
    if (market < 0) throw Error("option chain requires market");
    const auto contract = trim(query_value(target, "contract"));
    if (contract.empty()) throw Error("option chain requires contract");
    const int lookback = parse_bounded(query_value(target, "lookback", "60"),
                                       "lookback", 2, 800);
    const int limit = parse_bounded(query_value(target, "limit", "500"),
                                    "limit", 2, 2000);
    const int cache_ttl = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "30000"),
                                      "timeout_ms", 100, 60000);
    const double risk_free = query_option_number(
        target, "risk_free", 0.0187, -1.0, 1.0);
    auto document = fetch_option_chain_document(
        state.root, std::to_string(market), contract,
        trim(query_value(target, "expiry")), lookback, risk_free, limit,
        query_bool(target, "refresh"), cache_ttl, timeout);
    project_option_resource_paths(document, state.root);
    return document;
}

Json query_market_instruments_route(const ApiState&, const RequestTarget& target) {
    return query_market_instruments(target);
}

Json query_market_options_route(const ApiState&, const RequestTarget& target) {
    return query_market_options(target);
}

Json query_market_expansion_timeline_route(const ApiState&,
                                           const RequestTarget& target) {
    return query_market_expansion_timeline(target);
}

Json query_market_expansion_trades_route(const ApiState&,
                                         const RequestTarget& target) {
    return query_market_expansion_trades(target);
}

Json query_market_professional_route(const ApiState&,
                                     const RequestTarget& target) {
    return query_market_professional(target);
}

}  // namespace tdx::server_detail
