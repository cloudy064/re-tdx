#include "tpool_internal.hpp"

#include "tdx/blocks.hpp"
#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/formula_calc.hpp"
#include "tdx/formula_context.hpp"
#include "tdx/formula_engine.hpp"
#include "tdx/formulas.hpp"
#include "tdx/market.hpp"
#include "tdx/minute.hpp"
#include "tdx/security_directory.hpp"
#include "tdx/session_audit.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <cmath>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace tdx::tpool_detail {

struct SecurityState {
    bool directory_checked{};
    bool present_in_directory{};
    std::string name;
    bool daily_checked{};
    std::string latest_daily_date;
    double latest_daily_volume{};
    double volume_ratio_base{std::numeric_limits<double>::quiet_NaN()};
    double trade_unit{std::numeric_limits<double>::quiet_NaN()};
    std::string directory_error;
    std::string daily_error;
};

void read_latest_daily_bar(const Json& document, SecurityState& state) {
    state.daily_checked = true;
    const auto* bars = optional(document, "bars");
    if (!bars || !bars->is_array() || bars->size() == 0) {
        state.daily_error = "daily K-line returned no bars";
        return;
    }
    const auto& bar = bars->as_array().back();
    const auto* date = optional(bar, "date");
    const auto* volume = optional(bar, "volume");
    if (!date || !date->is_string() || !volume || !volume->is_number()) {
        state.daily_error = "latest daily bar lacks date or volume";
        return;
    }
    state.latest_daily_date = date->as_string();
    state.latest_daily_volume = volume->as_number();
}

void query_directory_state(SecurityDirectoryService& service,
                           const std::string& market,
                           const std::string& code,
                           const fs::path& root,
                           int timeout_ms,
                           SecurityState& state) {
    state.directory_checked = true;
    try {
        SecurityDirectoryQuery query;
        query.market = market;
        query.query = market + code;
        query.root = root;
        query.limit = 4;
        query.timeout_ms = std::max(100, std::min(timeout_ms, 60000));
        const auto document = service.query(query);
        const auto& rows = document.at("securities").as_array();
        for (const auto& row : rows) {
            if (json_text(row, "code") != code) continue;
            state.present_in_directory = true;
            state.name = json_text(row, "name");
            if (const auto* base = optional(row, "volume_ratio_base");
                base && base->is_number())
                state.volume_ratio_base = base->as_number();
            break;
        }
    } catch (const std::exception& error) {
        state.directory_error = error.what();
    }
}

Json filter_rule_document(const Json& rule,
                          const SecurityState& state,
                          const std::string& reference_daily_date) {
    const bool exclude_st = json_bool(rule, "exclude_st");
    const bool exclude_suspended = json_bool(rule, "exclude_suspended");
    const bool exclude_delisted = json_bool(rule, "exclude_delisted");
    Json reasons = Json::array();
    Json unknown = Json::array();
    if (exclude_st) {
        if (!state.directory_checked || !state.directory_error.empty()) unknown.push_back("ST status unavailable");
        else {
            const auto name = upper_ascii(trim(state.name));
            if (name.find("ST") != std::string::npos) reasons.push_back("ST name marker");
        }
    }
    if (exclude_suspended) {
        if (!state.daily_checked || !state.daily_error.empty() || reference_daily_date.empty())
            unknown.push_back("suspension status unavailable");
        else if (state.latest_daily_date < reference_daily_date || state.latest_daily_volume <= 0.0)
            reasons.push_back("latest daily bar is stale or zero-volume");
    }
    if (exclude_delisted) {
        if (!state.directory_checked || !state.directory_error.empty()) unknown.push_back("delisting status unavailable");
        else if (!state.present_in_directory) reasons.push_back("absent from active server security directory");
        else if (state.name.find("退") != std::string::npos) reasons.push_back("delisting name marker");
    }
    Json result = Json::object();
    result["requested"] = exclude_st || exclude_suspended || exclude_delisted;
    result["exclude_st"] = exclude_st;
    result["exclude_suspended"] = exclude_suspended;
    result["exclude_delisted"] = exclude_delisted;
    result["evaluable"] = unknown.size() == 0;
    result["excluded"] = reasons.size() > 0;
    result["reasons"] = std::move(reasons);
    result["unknown_reasons"] = std::move(unknown);
    result["method"] = "server-directory name/presence plus cross-security latest daily-bar heuristic";
    return result;
}

}  // namespace tdx::tpool_detail

namespace tdx {

using namespace tpool_detail;

Json attach_tpool_formula_library_document(Json inspection, const Json& formula_library) {
    upgrade_tpool_formula_compatibility(inspection, &formula_library);
    return inspection;
}

namespace {

struct PeriodFetchShape {
    int pages{};
    int page_size{};
    int required_bars{};
};

PeriodFetchShape period_fetch_shape(int requested_pages,
                                    int requested_page_size,
                                    int required_bars) {
    PeriodFetchShape result{requested_pages, requested_page_size, required_bars};
    const auto capacity = [&] {
        return static_cast<long long>(result.pages) * result.page_size;
    };
    if (capacity() >= required_bars) return result;
    result.page_size = std::min(800, std::max(result.page_size,
        (required_bars + result.pages - 1) / result.pages));
    if (capacity() < required_bars)
        result.pages = std::max(result.pages,
            (required_bars + result.page_size - 1) / result.page_size);
    if (result.pages > 20 || capacity() < required_bars)
        throw Error("TPool rule history exceeds the bounded K-line request capacity");
    return result;
}

Json tail_kline_document(const Json& document, int calculation_bars) {
    if (calculation_bars <= 0) return document;
    const auto* source = optional(document, "bars");
    if (!source || !source->is_array() ||
        source->size() <= static_cast<std::size_t>(calculation_bars))
        return document;
    Json result = document;
    Json bars = Json::array();
    const auto begin = source->size() - static_cast<std::size_t>(calculation_bars);
    for (std::size_t index = begin; index < source->size(); ++index)
        bars.push_back(source->as_array()[index]);
    result["bars"] = std::move(bars);
    result["count"] = calculation_bars;
    result["downloaded"] = calculation_bars;
    return result;
}

std::map<std::string, Json, std::less<>> records_by_security(
    const Json& document) {
    std::map<std::string, Json, std::less<>> result;
    const auto* records = optional(document, "records");
    if (!records || !records->is_array()) return result;
    for (const auto& record : records->as_array()) {
        auto id = lower_ascii(json_text(record, "security_id"));
        id.erase(std::remove(id.begin(), id.end(), ':'), id.end());
        if (!id.empty()) result[id] = record;
    }
    return result;
}

const Json* security_record(
    const std::map<std::string, Json, std::less<>>& records,
    const std::string& market,
    const std::string& code) {
    const auto found = records.find(lower_ascii(market + code));
    return found == records.end() ? nullptr : &found->second;
}

int market_id(const std::string& market) {
    if (market == "sz") return 0;
    if (market == "sh") return 1;
    if (market == "bj") return 2;
    return -1;
}

void attach_rule_outcome(Json& rule, const RuleResult& outcome) {
    rule["requested_anchor_count"] = static_cast<std::uint64_t>(
        outcome.requested_anchor_count);
    rule["evaluated_anchor_count"] = static_cast<std::uint64_t>(
        outcome.evaluated_anchor_count);
    rule["selected_offset_from_latest"] = outcome.selected_offset >= 0
        ? Json(outcome.selected_offset) : Json(nullptr);
    rule["matched_offset_from_latest"] = outcome.matched_offset >= 0
        ? Json(outcome.matched_offset) : Json(nullptr);
    rule["left_output"] = outcome.left_name;
    rule["left_value"] = outcome.evaluated || outcome.ranking_pending
        ? Json(outcome.left) : Json(nullptr);
    rule["right_output"] = outcome.right_name;
    rule["right_value"] = outcome.evaluated || outcome.ranking_pending
        ? Json(outcome.right) : Json(nullptr);
    Json ranking_observations = Json::array();
    for (const auto& observation : outcome.ranking_observations) {
        Json row = Json::object();
        row["offset_from_latest"] = observation.offset_from_latest;
        row["value"] = observation.value;
        ranking_observations.push_back(std::move(row));
    }
    rule["ranking_observations"] = std::move(ranking_observations);
    if (outcome.ranking_pending) {
        rule["status"] = "ranking-pending";
        rule["matched"] = nullptr;
        rule["message"] = outcome.ranking_observations.size() > 1
            ? "awaiting per-offset cross-security ranking"
            : "awaiting cross-security ranking";
    } else if (!outcome.evaluated) {
        rule["status"] = "error";
        rule["matched"] = nullptr;
        rule["message"] = outcome.error;
    } else {
        rule["status"] = "evaluated";
        rule["matched"] = outcome.matched;
        rule["message"] = "";
    }
}

Json evaluate_tpool_inspection_document(Json inspected, const std::string& source,
                                        int pages, int page_size,
                                        int timeout_ms, int security_limit,
                                        const Json* formula_library,
                                        const fs::path& tdx_root) {
    if (pages < 1 || pages > 20 || page_size < 1 || page_size > 800 ||
        timeout_ms < 1 || timeout_ms > 600000 || security_limit < 1 || security_limit > 200)
        throw Error("pool evaluation options are outside the safe range");
    upgrade_tpool_formula_compatibility(inspected, formula_library);
    const auto& functions = inspected.at("functions").as_array();
    const auto& stocks = inspected.at("stocks").as_array();
    std::map<std::string, int, std::less<>> required_bars_by_period;
    bool needs_directory = false;
    bool needs_daily_status = false;
    bool needs_quote = false;
    bool needs_finance = false;
    bool needs_trade_unit = false;
    for (const auto& function : functions) {
        needs_directory = needs_directory || json_bool(function, "exclude_st") ||
                          json_bool(function, "exclude_delisted");
        needs_daily_status = needs_daily_status || json_bool(function, "exclude_suspended");
        if (!json_bool(function, "execution_ready")) continue;
        Json annotated = function;
        const auto builtin = annotate_tpool_builtin_rule(annotated);
        if (builtin.applicable) {
            needs_quote = needs_quote || builtin.needs_quote;
            needs_finance = needs_finance || builtin.needs_finance;
            needs_directory = needs_directory || builtin.needs_directory;
            needs_trade_unit = needs_trade_unit || builtin.needs_trade_unit;
            continue;
        }
        const auto period = json_text(function, "period");
        const auto history = tpool_rule_history_plan(
            json_integer_text(function, "noperate", -1),
            json_integer_text(function, "nbeginday", 0),
            json_integer_text(function, "nendday", 0),
            json_integer_text(function, "nperiodnum", 0));
        if (!period.empty() && history.supported)
            required_bars_by_period[period] = std::max(
                required_bars_by_period[period], history.fetch_bars);
    }
    std::map<std::string, PeriodFetchShape, std::less<>> fetch_shapes;
    for (const auto& [period, required_bars] : required_bars_by_period)
        fetch_shapes.emplace(period, period_fetch_shape(
            pages, page_size, required_bars));

    std::vector<std::string> selected_security_ids;
    selected_security_ids.reserve(static_cast<std::size_t>(security_limit));
    for (const auto& stock : stocks) {
        if (selected_security_ids.size() >=
            static_cast<std::size_t>(security_limit)) break;
        const auto market = json_text(stock, "market");
        const auto code = json_text(stock, "code");
        if (!market.empty() && code.size() == 6)
            selected_security_ids.push_back(market + code);
    }
    std::map<std::string, Json, std::less<>> quote_records;
    std::map<std::string, Json, std::less<>> finance_records;
    std::string quote_fetch_error;
    std::string finance_fetch_error;
    if (needs_quote && !selected_security_ids.empty()) {
        try {
            quote_records = records_by_security(fetch_market_snapshot_document(
                tdx_root, selected_security_ids, timeout_ms));
        } catch (const std::exception& error) {
            quote_fetch_error = error.what();
        }
    }
    if (needs_finance && !selected_security_ids.empty()) {
        try {
            finance_records = records_by_security(fetch_finance_document(
                selected_security_ids,
                load_public_quote_endpoints(tdx_root).endpoints,
                timeout_ms, 80, false));
        } catch (const std::exception& error) {
            finance_fetch_error = error.what();
        }
    }
    std::shared_ptr<const SecurityCatalog> security_catalog;
    std::string trade_unit_error;
    if (needs_trade_unit) {
        try {
            security_catalog = cached_security_catalog(tdx_root);
        } catch (const std::exception& error) {
            trade_unit_error = error.what();
        }
    }
    const int elapsed_trading_minutes =
        current_a_share_elapsed_trading_minutes();
    SecurityDirectoryService directory_service;
    std::vector<SecurityState> security_states;
    Json security_rows = Json::array();
    std::size_t selected = 0;
    for (const auto& stock : stocks) {
        if (selected >= static_cast<std::size_t>(security_limit)) break;
        const auto market = json_text(stock, "market");
        const auto code = json_text(stock, "code");
        if (market.empty() || code.size() != 6) continue;
        ++selected;
        Json row = Json::object();
        row["market"] = market;
        row["code"] = code;
        row["security_id"] = market + code;
        row["cell_id"] = json_text(stock, "cell_id");
        Json rules = Json::array();
        std::map<std::string, Json, std::less<>> kline_cache;
        std::map<std::string, Json, std::less<>> calculation_cache;
        std::map<std::string, Json, std::less<>> context_cache;
        SecurityState state;
        if (needs_directory)
            query_directory_state(directory_service, market, code, tdx_root,
                                  timeout_ms, state);
        if (security_catalog) {
            const auto found = security_catalog->find({market_id(market), code});
            if (found != security_catalog->end())
                state.trade_unit = found->second.trade_unit;
        }
        if (needs_daily_status) {
            try {
                auto daily = fetch_kline_document(market, code, "stock", "day",
                                                  1, 1, 0,
                                                  "all", timeout_ms);
                read_latest_daily_bar(daily, state);
            } catch (const std::exception& error) {
                state.daily_checked = true;
                state.daily_error = error.what();
            }
        }
        for (std::size_t rule_index = 0; rule_index < functions.size(); ++rule_index) {
            const auto& function = functions[rule_index];
            Json rule = Json::object();
            rule["rule_index"] = static_cast<std::uint64_t>(rule_index);
            rule["rule_kind"] = json_text(function, "rule_kind");
            rule["formula"] = json_text(function, "formula_code");
            rule["field_index"] = optional(function, "field_index")
                ? *optional(function, "field_index") : Json(nullptr);
            rule["field_key"] = json_text(function, "field_key");
            rule["field_name"] = json_text(function, "field_name");
            rule["field_unit"] = json_text(function, "field_unit");
            rule["operator"] = json_text(function, "operator");
            rule["period"] = json_text(function, "period");
            rule["cell_id"] = json_text(function, "cell_id");
            rule["exclude_st"] = json_bool(function, "exclude_st");
            rule["exclude_suspended"] = json_bool(function, "exclude_suspended");
            rule["exclude_delisted"] = json_bool(function, "exclude_delisted");
            rule["history_window_begin_offset"] =
                json_integer_text(function, "nbeginday", 0);
            rule["history_window_end_offset"] =
                json_integer_text(function, "nendday", 0);
            rule["calculation_lookback_bars"] =
                optional(function, "calculation_lookback_bars")
                    ? *optional(function, "calculation_lookback_bars") : Json(nullptr);
            if (!function.at("execution_ready").as_bool()) {
                rule["status"] = "unsupported";
                rule["matched"] = nullptr;
                rule["message"] = json_text(function, "blocking_reason");
                rules.push_back(std::move(rule));
                continue;
            }
            try {
                Json builtin_rule = function;
                const auto builtin = annotate_tpool_builtin_rule(builtin_rule);
                if (builtin.applicable) {
                    if (builtin.needs_quote && !quote_fetch_error.empty())
                        throw Error("TPool quote batch failed: " + quote_fetch_error);
                    if (builtin.needs_finance && !finance_fetch_error.empty())
                        throw Error("TPool finance batch failed: " + finance_fetch_error);
                    if (builtin.needs_trade_unit && !trade_unit_error.empty())
                        throw Error("TPool trade-unit catalog failed: " + trade_unit_error);
                    const auto outcome = evaluate_tpool_builtin_rule(
                        function,
                        security_record(quote_records, market, code),
                        security_record(finance_records, market, code),
                        state.volume_ratio_base,
                        state.trade_unit,
                        elapsed_trading_minutes);
                    attach_rule_outcome(rule, outcome);
                    rules.push_back(std::move(rule));
                    continue;
                }
                const auto formula = json_text(function, "formula_code");
                const auto period = json_text(function, "period");
                const auto history = tpool_rule_history_plan(
                    json_integer_text(function, "noperate", -1),
                    json_integer_text(function, "nbeginday", 0),
                    json_integer_text(function, "nendday", 0),
                    json_integer_text(function, "nperiodnum", 0));
                const auto shape = fetch_shapes.at(period);
                rule["history_fetch_bars"] = history.fetch_bars;
                rule["history_fetch_pages"] = shape.pages;
                rule["history_fetch_page_size"] = shape.page_size;
                auto kline = kline_cache.find(period);
                if (kline == kline_cache.end())
                    kline = kline_cache.emplace(period, fetch_kline_document(
                        market, code, "stock", period, shape.pages,
                        shape.page_size, 0, "all", timeout_ms)).first;
                auto calculation_input = tail_kline_document(
                    kline->second, history.calculation_bars);
                const auto cache_key = period + "|" + formula + "|" +
                    std::to_string(history.calculation_bars);
                auto calculation = calculation_cache.find(cache_key);
                if (calculation == calculation_cache.end()) {
                    if (const auto* source_formula = library_formula(formula_library, formula)) {
                        const auto analysis = formula_analysis(*source_formula);
                        const Json* context_pointer = nullptr;
                        if (analysis.at("has_external_dependency").as_bool()) {
                            auto context = context_cache.find(cache_key);
                            if (context == context_cache.end())
                                context = context_cache.emplace(cache_key,
                                    build_formula_market_context_document(tdx_root, market, code,
                                        analysis, timeout_ms, nullptr,
                                        &calculation_input)).first;
                            context_pointer = &context->second;
                        }
                        calculation = calculation_cache.emplace(cache_key,
                            evaluate_formula_document(calculation_input, *source_formula, {},
                                                      context_pointer)).first;
                    } else {
                        calculation = calculation_cache.emplace(cache_key,
                            calculate_formula_document(calculation_input, formula)).first;
                    }
                }
                rule["calculation_point_count"] = static_cast<std::uint64_t>(
                    calculation->second.at("points").size());
                const auto outcome = evaluate_tpool_rule(function, calculation->second);
                attach_rule_outcome(rule, outcome);
            } catch (const std::exception& error) {
                rule["status"] = "error";
                rule["matched"] = nullptr;
                rule["message"] = error.what();
            }
            rules.push_back(std::move(rule));
        }
        row["name"] = state.name;
        row["directory_checked"] = state.directory_checked;
        row["present_in_active_directory"] = state.directory_checked &&
                                                state.directory_error.empty()
                                            ? Json(state.present_in_directory) : Json(nullptr);
        row["directory_error"] = state.directory_error;
        row["volume_ratio_base"] = std::isfinite(state.volume_ratio_base)
            ? Json(state.volume_ratio_base) : Json(nullptr);
        row["trade_unit"] = std::isfinite(state.trade_unit)
            ? Json(state.trade_unit) : Json(nullptr);
        row["latest_daily_date"] = state.latest_daily_date;
        row["latest_daily_volume"] = state.daily_checked && state.daily_error.empty()
                                       ? Json(state.latest_daily_volume) : Json(nullptr);
        row["daily_status_error"] = state.daily_error;
        row["any_rule_matched"] = false;
        row["rules"] = std::move(rules);
        security_rows.push_back(std::move(row));
        security_states.push_back(std::move(state));
    }
    std::string reference_daily_date;
    for (const auto& state : security_states)
        if (state.daily_error.empty())
            reference_daily_date = std::max(reference_daily_date, state.latest_daily_date);
    for (std::size_t security_index = 0; security_index < security_rows.size(); ++security_index) {
        auto& rules = security_rows.as_array()[security_index]
                          .as_object().at("rules").as_array();
        for (auto& rule : rules) {
            auto filter = filter_rule_document(rule, security_states[security_index],
                                               reference_daily_date);
            const bool requested = filter.at("requested").as_bool();
            const bool evaluable = filter.at("evaluable").as_bool();
            const bool excluded = filter.at("excluded").as_bool();
            rule["filter"] = std::move(filter);
            if (!requested) continue;
            if (!evaluable) {
                rule["status"] = "filter-unavailable";
                rule["matched"] = nullptr;
                rule["message"] = "requested TPool exclusion filter could not be evaluated safely";
            } else if (excluded) {
                rule["status"] = "filtered";
                rule["matched"] = false;
                rule["message"] = "security excluded by TPool rule filter";
            }
        }
    }
    resolve_cross_security_rankings(
        security_rows, functions, selected, stocks.size());
    std::size_t evaluated_rules = 0;
    std::size_t matched_rules = 0;
    std::size_t any_matches = 0;
    std::size_t filtered_rules = 0;
    std::size_t unavailable_filters = 0;
    for (auto& security : security_rows.as_array()) {
        bool security_match = false;
        bool seed_cell_match = false;
        const auto seed_cell = json_text(security, "cell_id");
        for (const auto& rule : security.at("rules").as_array()) {
            const auto status = json_text(rule, "status");
            if (status == "filtered") { ++filtered_rules; continue; }
            if (status == "filter-unavailable") { ++unavailable_filters; continue; }
            if (status != "evaluated") continue;
            ++evaluated_rules;
            const auto* matched = optional(rule, "matched");
            if (matched && matched->is_bool() && matched->as_bool()) {
                ++matched_rules;
                security_match = true;
                if (json_text(rule, "cell_id") == seed_cell) seed_cell_match = true;
            }
        }
        security["any_rule_matched"] = security_match;
        security["seed_cell_any_rule_matched"] = seed_cell_match;
        if (security_match) ++any_matches;
    }
    Json result = Json::object();
    result["schema_version"] = 1;
    result["schema"] = "tdx-tpool-native-evaluation-v1";
    result["source"] = source;
    result["read_only"] = true;
    result["tdx_state_mutated"] = false;
    result["flow_graph_evaluated"] = false;
    result["scope"] = "native formula rules, latest-finance/realtime-quote built-in fields, ST/suspension/delisting filters, cross-security ranking and read-only acyclic flow projection";
    result["builtin_elapsed_trading_minutes"] = elapsed_trading_minutes;
    result["builtin_quote_requested"] = needs_quote;
    result["builtin_quote_record_count"] =
        static_cast<std::uint64_t>(quote_records.size());
    result["builtin_quote_error"] = quote_fetch_error;
    result["builtin_finance_requested"] = needs_finance;
    result["builtin_finance_record_count"] =
        static_cast<std::uint64_t>(finance_records.size());
    result["builtin_finance_error"] = finance_fetch_error;
    result["builtin_trade_unit_requested"] = needs_trade_unit;
    result["builtin_trade_unit_error"] = trade_unit_error;
    result["reference_daily_date"] = reference_daily_date;
    result["available_security_count"] = static_cast<std::uint64_t>(stocks.size());
    result["evaluated_security_count"] = static_cast<std::uint64_t>(selected);
    result["truncated"] = selected < stocks.size();
    result["evaluated_rule_count"] = static_cast<std::uint64_t>(evaluated_rules);
    result["matched_rule_count"] = static_cast<std::uint64_t>(matched_rules);
    result["filtered_rule_count"] = static_cast<std::uint64_t>(filtered_rules);
    result["filter_unavailable_rule_count"] = static_cast<std::uint64_t>(unavailable_filters);
    result["security_any_match_count"] = static_cast<std::uint64_t>(any_matches);
    result["securities"] = std::move(security_rows);
    result["inspection"] = inspected;
    result["flow_projection"] = project_tpool_flow_document(result);
    result["flow_graph_evaluated"] = result.at("flow_projection").at("evaluated");
    return result;
}

}  // namespace

Json evaluate_tpool_file_document(const fs::path& path, int pages, int page_size,
                                  int timeout_ms, int security_limit,
                                  const Json* formula_library,
                                  const fs::path& tdx_root) {
    auto inspected = inspect_tpool_file_document(path);
    return evaluate_tpool_inspection_document(
        std::move(inspected), path_utf8(path), pages, page_size,
        timeout_ms, security_limit, formula_library, tdx_root);
}

Json evaluate_tpool_xml_document(const std::string& xml,
                                 const std::string& source_name,
                                 int pages, int page_size,
                                 int timeout_ms, int security_limit,
                                 const Json* formula_library,
                                 const fs::path& tdx_root) {
    auto inspected = parse_tpool_xml_document(xml, source_name);
    inspected["size"] = static_cast<std::uint64_t>(xml.size());
    inspected["inline_source"] = true;
    auto result = evaluate_tpool_inspection_document(
        std::move(inspected), source_name, pages, page_size,
        timeout_ms, security_limit, formula_library, tdx_root);
    result["inline_source"] = true;
    result["request_body_retained"] = false;
    return result;
}

}  // namespace tdx
