#include "technical_signals_internal.hpp"

#include "tdx/cloud_resilience.hpp"
#include "tdx/cloud_workflow.hpp"
#include "tdx/common.hpp"
#include "tdx/pbrpc.hpp"
#include "tdx/tqlex.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <map>
#include <thread>
#include <utility>

namespace fs = std::filesystem;

namespace tdx {
using namespace technical_signals_detail;
TechnicalSignalsService::TechnicalSignalsService(fs::path root, BlockData blocks)
    : root_(std::move(root)), blocks_(std::move(blocks)) {}

Json TechnicalSignalsService::query(const TechnicalSignalsQuery& input) {
    TechnicalSignalsQuery options = input;
    options.view = lower_ascii(trim(options.view));
    options.direction = lower_ascii(trim(options.direction));
    options.board = lower_ascii(trim(options.board));
    if (!known_view(options.view)) throw Error("unknown technical signal view: " + options.view);
    const auto* factor_definition = factor_signal(options.view);
    const auto* opportunity_definition = opportunity_signal(options.view);
    if (options.enrich_quotes && !factor_definition && !opportunity_definition)
        throw Error("quote enrichment is only available for the 200661/200662 intraday signal views");
    if (options.direction != "all" && options.direction != "up" && options.direction != "down")
        throw Error("direction must be all, up, or down");
    if (options.rps1 < 0) options.rps1 = options.view == "rps-block" ? 85 : 90;
    if (options.rps2 < 0) options.rps2 = options.view == "rps-block" ? 85 : 90;
    if (options.rps3 < 0) options.rps3 = options.view == "rps-block" ? 85 : 90;
    for (const auto value : {options.duration1, options.duration2, options.duration3,
                             options.index_period, options.history_period,
                             options.sideways_period, options.breakout_period})
        if (value < 1 || value > 10000) throw Error("technical signal periods must be in 1..10000");
    for (const auto value : {options.rps1, options.rps2, options.rps3,
                             options.retracement, options.amplitude})
        if (value < 0 || value > 100) throw Error("technical signal percentages must be in 0..100");
    if (options.limit < 1 || options.limit > 10000 || options.cache_ttl_seconds < 0 ||
        options.cache_ttl_seconds > 3600 || options.timeout_ms < 100 ||
        options.timeout_ms > 600000)
        throw Error("technical signal query limits are invalid");
    if (options.view == "trend-up" || options.view == "trend-down")
        options.board = board_name(board_id(options.board));

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

    std::string request_id, source_file;
    std::map<std::string, std::string> replacements, overrides;
    if (options.view == "nine-turn") {
        request_id = "200451"; source_file = "func_sqjz101.xml";
    } else if (options.view == "rps-stock" || options.view == "rps-block") {
        request_id = options.view == "rps-stock" ? "200452" : "200453";
        source_file = "gp_gz_rpsxg.xml";
        replacements = {{"A", std::to_string(options.duration1)},
                        {"B", std::to_string(options.rps1)},
                        {"C", std::to_string(options.duration2)},
                        {"D", std::to_string(options.rps2)},
                        {"E", std::to_string(options.duration3)},
                        {"F", std::to_string(options.rps3)}};
    } else if (options.view == "new-high" || options.view == "new-low") {
        request_id = "200327"; source_file = "gp_gz_xgxd.xml";
        replacements = {{"N", std::to_string(options.index_period)},
                        {"M", std::to_string(options.history_period)},
                        {"R", std::to_string(options.retracement)}};
        overrides = {{"highLowFlag", options.view == "new-high" ? "1" : "0"},
                     {"PageSize", options.view == "new-high" ? "100" : "1000000"}};
    } else if (options.view == "breakout") {
        request_id = "200329"; source_file = "gp_gz_xgxd.xml";
        replacements = {{"M", std::to_string(options.sideways_period)},
                        {"N", std::to_string(options.amplitude)},
                        {"R", std::to_string(options.breakout_period)}};
    } else if (options.view == "strong-start") {
        request_id = "200316"; source_file = "gp_gz_xgxd.xml";
    } else if (options.view == "event-driven") {
        request_id = "200225"; source_file = "tdxxgcl6.xml";
    } else if (const auto* strategy = model_strategy(options.view)) {
        request_id = "200250"; source_file = "tdxxgcl6.xml";
        overrides = {{"XgName", strategy->xg_name}};
    } else if (const auto* strategy = auction_strategy(options.view)) {
        request_id = strategy->request_id; source_file = "sc_jjcl.xml";
    } else if (opportunity_definition) {
        request_id = "200661"; source_file = "gp_gz_dxjh.xml";
        overrides = {{"flag", opportunity_definition->flag}, {"sortType", "3"},
                     {"desc", "1"}, {"Page", "0"}, {"PageSize", "500"}};
    } else if (factor_definition) {
        request_id = "200662"; source_file = "gp_gz_dxjh.xml";
        overrides = {{"flag", factor_definition->flag}, {"sortType", "3"},
                     {"desc", "1"}, {"Page", "0"}, {"PageSize", "500"}};
    } else {
        request_id = options.view == "trend-up" ? "200320" : "200325";
        source_file = options.view == "trend-up" ? "gp_gz_SSTD.xml" : "gp_gz_cdgg.xml";
        overrides = {{"market", std::to_string(board_id(options.board))}};
    }

    const bool tqlex_transport = factor_definition || opportunity_definition;
    std::vector<std::string> body_selectors;
    if (const auto* strategy = model_strategy(options.view))
        body_selectors.push_back(strategy->xg_name);
    if (factor_definition)
        body_selectors.push_back("'flag':'" + std::string(factor_definition->flag) + "'");
    if (opportunity_definition)
        body_selectors.push_back("'flag':'" + std::string(opportunity_definition->flag) + "'");
    Json upstream;
    for (int attempt = 0; attempt < 5; ++attempt) {
        try {
            if (tqlex_transport) {
                upstream = execute_tqlex_config(
                    root_, request_id, replacements, overrides, {}, source_file,
                    body_selectors, false, -1, 0, 100,
                    cloud_endpoints::tqlex, options.timeout_ms);
            } else {
                upstream = execute_pbrpc_config(
                    root_, request_id, replacements, overrides, {}, {}, source_file,
                    body_selectors, cloud_endpoints::tqlex,
                    options.timeout_ms);
            }
            break;
        } catch (const Error& error) {
            const std::string message = error.what();
            if (!detail::is_transient_cloud_error(message)) throw;
            if (attempt == 4) {
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
            std::this_thread::sleep_for(
                std::chrono::milliseconds(250 * (1 << attempt)));
        }
    }
    const auto raw_rows = cloud_result_rows(upstream.at("response"));
    auto unfiltered_options = options;
    unfiltered_options.apply_client_filters = false;
    const auto normalized_rows = normalize_technical_signal_rows(
        raw_rows, unfiltered_options, blocks_);
    auto records = options.apply_client_filters
        ? normalize_technical_signal_rows(raw_rows, options, blocks_)
        : normalized_rows;
    const auto filtered_count = records.size();
    const bool truncated = records.size() > static_cast<std::size_t>(options.limit);
    if (truncated) records.as_array().resize(static_cast<std::size_t>(options.limit));

    Json quote_enrichment = Json(nullptr);
    Json warnings = Json::array();
    if (factor_definition || opportunity_definition) {
        quote_enrichment = Json::object();
        quote_enrichment["requested"] = options.enrich_quotes;
        quote_enrichment["status"] = options.enrich_quotes ? "pending" : "not-requested";
        if (options.enrich_quotes) {
            try {
                quote_enrichment = enrich_factor_quotes(
                    root_, records, blocks_, options.timeout_ms);
            } catch (const std::exception& error) {
                quote_enrichment["status"] = "unavailable";
                quote_enrichment["error"] = error.what();
                warnings.push_back(
                    "The optional public L1 quote enrichment failed; source signals remain valid.");
            }
        }
    }

    Json source = Json::object();
    source["transport"] = tqlex_transport ? "TQLEX reqformat=2" : "PBRPC reqformat=22";
    source["request_id"] = request_id;
    source["source_file"] = upstream.at("source_file");
    if (tqlex_transport) {
        source["entry"] = upstream.at("entry");
        source["page"] = 0;
        source["page_size"] = 500;
    } else {
        source["module"] = upstream.at("module");
        source["rpc_id"] = upstream.at("rpc_id");
        source["rounds"] = upstream.at("rounds");
        source["raw_size"] = upstream.at("raw_size");
    }

    Json counts = Json::object();
    counts["upstream_rows"] = static_cast<std::uint64_t>(raw_rows.size());
    counts["normalization_skipped"] =
        static_cast<std::uint64_t>(raw_rows.size() - normalized_rows.size());
    counts["client_filtered_out"] =
        static_cast<std::uint64_t>(normalized_rows.size() - filtered_count);
    counts["after_client_filter"] = static_cast<std::uint64_t>(filtered_count);
    counts["returned"] = static_cast<std::uint64_t>(records.size());
    counts["truncated"] = truncated;

    Json cache = Json::object(); cache["hit"] = false; cache["stale"] = false;
    cache["age_seconds"] = 0;
    cache["ttl_seconds"] = options.cache_ttl_seconds;
    Json result = Json::object();
    result["schema"] = "tdx-technical-signals-native-v1";
    result["availability"] = "live";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["parameters"] = parameters_document(options);
    result["client_filters"] = client_filters(options);
    result["quote_enrichment"] = std::move(quote_enrichment);
    result["warnings"] = std::move(warnings);
    result["source"] = std::move(source);
    result["counts"] = std::move(counts);
    result["records"] = std::move(records);
    result["cache"] = std::move(cache);
    result["raw_response_retained"] = false;
    cache_[key] = CachedDocument{result, now};
    return result;
}

}  // namespace tdx
