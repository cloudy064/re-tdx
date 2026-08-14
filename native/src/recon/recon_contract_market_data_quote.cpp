#include "recon_contract_market_data_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace tdx::recon_contract_detail {

namespace {

using QuoteContractValidator = void (*)(const std::string& contract_id, const Json& document, Json& result);

void validate_market_kline_30m(const std::string& contract_id, const Json& document,
        Json& result) {
    const bool kline = contract_id == "market-kline-30m-live";
    const bool directory = contract_id == "market-securities-native-live";
    const auto expected_schema = kline ? "tdx-minute-v1" :
        contract_id == "market-auction-native-live"
            ? "tdx-auction-series-native-v1" :
        contract_id == "market-trades-native-live"
            ? "tdx-trades-native-v1" :
        contract_id == "market-ranking-native-live"
            ? "tdx-market-ranking-native-v1" :
              "tdx-market-security-directory-native-v1";
    add_assertion(result, "schema",
                  string_is(member(document, "schema"), expected_schema),
                  expected_schema, value_or_null(member(document, "schema")));
    const auto* transport = kline
        ? member(document, "transport_detail")
        : directory ? member_path(document, {"sources", "0", "transport"})
                    : member(document, "transport");
    // member_path does not index arrays, so resolve the directory source explicitly.
    if (directory) {
        transport = nullptr;
        const auto* sources = member(document, "sources");
        if (sources && sources->is_array() && !sources->as_array().empty())
            transport = member(sources->as_array().front(), "transport");
    }
    const auto attempts = transport
        ? numeric_value(member(*transport, "connection_attempts")) : std::nullopt;
    const auto available = transport
        ? numeric_value(member(*transport, "available_endpoint_count")) : std::nullopt;
    const bool configured_transport = transport &&
        string_is(member(*transport, "endpoint_source"),
                  "connect.cfg:hqhost-primary-first") &&
        bool_is(member(*transport, "primary_configured"), true) &&
        attempts && *attempts >= 1 && available && *available >= 2;
    add_assertion(result, "configured_primary_transport", configured_transport,
                  "connect.cfg primary-first, attempts>=1, available>=2",
                  configured_transport);
    bool payload = false;
    if (kline) {
        const auto* bars = member(document, "bars");
        std::set<std::string> dates;
        std::set<std::string> stamps;
        bool chronological = true;
        std::string previous_stamp;
        if (bars && bars->is_array()) {
            for (const auto& bar : bars->as_array()) {
                const auto* date = member(bar, "date");
                const auto* time = member(bar, "time");
                if (date && date->is_string()) dates.insert(date->as_string());
                if (date && date->is_string() && time && time->is_string()) {
                    const auto stamp = date->as_string() + "|" + time->as_string();
                    if (!previous_stamp.empty() && stamp < previous_stamp)
                        chronological = false;
                    previous_stamp = stamp;
                    stamps.insert(stamp);
                }
            }
        }
        add_assertion(result, "chronological_bars", chronological,
                      "non-decreasing date/time across page boundaries",
                      chronological);
        payload = number_is(member(document, "downloaded"), 160) && bars &&
            bars->is_array() && bars->as_array().size() == 160 &&
            dates.size() >= 2 && stamps.size() == 160;
    } else if (directory) {
        const auto* sources = member(document, "sources");
        payload = number_is(member(document, "returned"), 3) && sources &&
            sources->is_array() && !sources->as_array().empty() &&
            numeric_value(member(sources->as_array().front(), "received_count"))
                .value_or(0) > 1000;
    } else if (contract_id == "market-trades-native-live") {
        payload = number_is(member(document, "received"), 1) &&
            numeric_value(member(document, "tick_count")).value_or(0) > 0;
    } else {
        payload = numeric_value(member(document, "received")).value_or(0) > 0;
    }
    add_assertion(result, "complete_payload", payload, true, payload);
}

void validate_historical_securities(const std::string& contract_id,
                                    const Json& document, Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-historical-securities-native-v1"),
                  "tdx-market-historical-securities-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "local_availability",
                  string_is(member(document, "availability"), "local"),
                  "local", value_or_null(member(document, "availability")));
    add_assertion(result, "absent_filter",
                  string_is(member_path(document, {"filters", "presence"}),
                            "absent"),
                  "absent",
                  value_or_null(member_path(document,
                                             {"filters", "presence"})));

    const auto* records = member(document, "records");
    bool records_valid = records && records->is_array() &&
                         !records->as_array().empty();
    if (records_valid) {
        for (const auto& row : records->as_array()) {
            const auto* code = member_path(row, {"security", "code"});
            records_valid = records_valid &&
                bool_is(member(row, "current_directory_present"), false) &&
                nonempty_string(member(row, "compatibility_name")) &&
                code && code->is_string() && code->as_string().size() == 6;
        }
    }
    add_assertion(result, "absent_records", records_valid,
                  "non-empty absent records with six-digit codes and names",
                  records_valid);

    const bool local_transport =
        string_is(member_path(document, {"transport", "kind"}),
                  "local-files") &&
        number_is(member_path(document,
                              {"transport", "network_requests"}), 0);
    add_assertion(result, "zero_network_transport", local_transport,
                  "local-files with network_requests=0", local_transport);

    bool pttab_source = false;
    const auto* sources = member(document, "sources");
    if (sources && sources->is_array()) {
        for (const auto& source : sources->as_array())
            if (string_is(member(source, "file"), "pttab.dat")) {
                pttab_source = true;
                break;
            }
    }
    add_assertion(result, "pttab_source", pttab_source, "pttab.dat",
                  pttab_source);
}

void validate_invalid_market(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "error_code",
                  string_is(member(document, "error"), "bad_request"), "bad_request",
                  value_or_null(member(document, "error")));
    const auto* message = member(document, "message");
    const bool meaningful = message && message->is_string() &&
                            message->as_string().find("market") != std::string::npos;
    add_assertion(result, "market_message", meaningful, "message containing market",
                  value_or_null(message));
}

void validate_stock_native_valuation(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"), "tdx-stats-native-v1"),
                  "tdx-stats-native-v1", value_or_null(member(document, "schema")));
    const auto* stats_transport = member(document, "transport");
    const bool configured_stats_transport = stats_transport &&
        string_is(member(*stats_transport, "endpoint_source"),
                  "connect.cfg:hqhost-primary-first") &&
        bool_is(member(*stats_transport, "primary_configured"), true) &&
        numeric_value(member(*stats_transport, "connection_attempts")).value_or(0) >= 1;
    add_assertion(result, "stats_configured_primary_transport",
                  configured_stats_transport,
                  "connect.cfg primary-first with at least one attempt",
                  configured_stats_transport);
    const auto* valuation = member(document, "valuation");
    add_assertion(result, "valuation_schema",
                  valuation && string_is(member(*valuation, "schema"),
                                          "tdx-security-valuation-native-v1"),
                  "tdx-security-valuation-native-v1",
                  valuation ? value_or_null(member(*valuation, "schema")) : Json(nullptr));
    if (!valuation) return;

    const bool identity = string_is(member(*valuation, "security_id"), "SZ000001") &&
                          string_is(member(*valuation, "code"), "000001") &&
                          number_is(member(*valuation, "market_id"), 0);
    add_assertion(result, "security_identity", identity, "SZ000001 / 000001 / 0",
                  identity);
    const bool complete = string_is(member(*valuation, "availability"), "complete") &&
                          number_is(member(*valuation, "available_metric_count"), 4) &&
                          array_empty(member(*valuation, "errors"));
    add_assertion(result, "four_metrics_complete", complete,
                  "availability=complete, count=4, errors=[]", complete);

    const auto* metrics = member(*valuation, "metrics");
    const auto* pe_dynamic = metrics ? member(*metrics, "pe_dynamic") : nullptr;
    const auto* pe_static = metrics ? member(*metrics, "pe_static") : nullptr;
    const auto* pe_ttm = metrics ? member(*metrics, "pe_ttm") : nullptr;
    const auto* pb_mrq = metrics ? member(*metrics, "pb_mrq") : nullptr;
    const bool exact_codes =
        pe_dynamic && string_is(member(*pe_dynamic, "code"), "$PE") &&
        pe_static && string_is(member(*pe_static, "code"), "$PES") &&
        pe_ttm && string_is(member(*pe_ttm, "code"), "$PETTM") &&
        pb_mrq && string_is(member(*pb_mrq, "code"), "$PBMRQ");
    add_assertion(result, "exact_tdx_metric_codes", exact_codes,
                  "$PE,$PES,$PETTM,$PBMRQ", exact_codes);

    const auto dynamic_value = pe_dynamic
        ? numeric_value(member(*pe_dynamic, "value")) : std::nullopt;
    const auto static_value = pe_static
        ? numeric_value(member(*pe_static, "value")) : std::nullopt;
    const auto ttm_value = pe_ttm
        ? numeric_value(member(*pe_ttm, "value")) : std::nullopt;
    const auto pb_value = pb_mrq
        ? numeric_value(member(*pb_mrq, "value")) : std::nullopt;
    const bool positive_metrics = dynamic_value && std::isfinite(*dynamic_value) &&
                                  *dynamic_value > 0 && static_value &&
                                  std::isfinite(*static_value) && *static_value > 0 &&
                                  ttm_value && std::isfinite(*ttm_value) &&
                                  *ttm_value > 0 && pb_value &&
                                  std::isfinite(*pb_value) && *pb_value > 0;
    add_assertion(result, "positive_finite_metrics", positive_metrics,
                  "four positive finite values", positive_metrics);

    const auto* inputs = member(*valuation, "inputs");
    const auto price = inputs ? numeric_value(member(*inputs, "price")) : std::nullopt;
    const auto annualized_eps = inputs
        ? numeric_value(member(*inputs, "annualized_eps")) : std::nullopt;
    const auto report_months = inputs
        ? numeric_value(member(*inputs, "report_months")) : std::nullopt;
    const auto total_shares = inputs
        ? numeric_value(member(*inputs, "total_shares")) : std::nullopt;
    const auto net_assets_per_share = inputs
        ? numeric_value(member(*inputs, "net_assets_per_share")) : std::nullopt;
    const bool inputs_complete = price && *price > 0 && annualized_eps &&
                                 *annualized_eps > 0.0001 && report_months &&
                                 *report_months > 0 && total_shares &&
                                 *total_shares > 0 && net_assets_per_share &&
                                 *net_assets_per_share > 0;
    add_assertion(result, "formula_inputs", inputs_complete,
                  "positive price/report/shares/NAPS and EPS > 0.0001",
                  inputs_complete);
    if (inputs_complete && dynamic_value && pb_value) {
        const auto expected_pe = static_cast<double>(
            static_cast<float>(*price / *annualized_eps));
        const auto expected_pb = static_cast<double>(
            static_cast<float>(*price / *net_assets_per_share));
        const bool exact_formulas = std::abs(*dynamic_value - expected_pe) < 1e-6 &&
                                    std::abs(*pb_value - expected_pb) < 1e-6;
        add_assertion(result, "reversed_float_formulas", exact_formulas,
                      "float(price/annualized_eps), float(price/NAPS)",
                      exact_formulas);
    } else {
        add_assertion(result, "reversed_float_formulas", false,
                      "float(price/annualized_eps), float(price/NAPS)", false);
    }

    const auto* records = member(document, "records");
    const Json* stat = nullptr;
    if (records && records->is_array() && !records->as_array().empty())
        stat = member(records->as_array().front(), "stat");
    const auto record_static = stat
        ? numeric_value(member(*stat, "pe_static")) : std::nullopt;
    const auto record_ttm = stat
        ? numeric_value(member(*stat, "pe_ttm")) : std::nullopt;
    const bool stats_reconciled = static_value && record_static &&
        std::abs(*static_value - *record_static) < 1e-6 && ttm_value && record_ttm &&
        std::abs(*ttm_value - *record_ttm) < 1e-6 &&
        inputs && member(document, "stats_date") &&
        member(*inputs, "stats_date") &&
        value_or_null(member(document, "stats_date")).dump() ==
            value_or_null(member(*inputs, "stats_date")).dump();
    add_assertion(result, "downloaded_stats_reconciled", stats_reconciled,
                  "valuation PES/PETTM/date equal tdxstat record", stats_reconciled);
    const auto* upstream_transport = member(*valuation, "upstream_transport");
    const auto valid_transport = [](const Json* value) {
        const auto* attempts = value ? member(*value, "connection_attempts") : nullptr;
        const auto* retries = value ? member(*value, "transient_retries") : nullptr;
        return value && value->is_object() && attempts && attempts->is_number() &&
            attempts->as_number() >= 1 && attempts->as_number() <= 9 &&
            retries && retries->is_number() && retries->as_number() >= 0 &&
            retries->as_number() <= 6 &&
            number_is(member(*value, "max_attempts_per_endpoint"), 3) &&
            string_is(member(*value, "endpoint_source"),
                      "connect.cfg:hqhost-primary-first") &&
            member(*value, "recovered_after_retry") &&
            member(*value, "recovered_after_retry")->is_bool();
    };
    const bool retry_metadata = upstream_transport &&
        valid_transport(member(*upstream_transport, "snapshot")) &&
        valid_transport(member(*upstream_transport, "finance"));
    add_assertion(result, "bounded_transport_retry", retry_metadata,
                  "connect.cfg primary-first snapshot/finance retry provenance",
                  value_or_null(upstream_transport));
}

void validate_market_speed(const std::string& contract_id, const Json& document,
        Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"), "tdx-market-speed-native-v1"),
                  "tdx-market-speed-native-v1", value_or_null(member(document, "schema")));
    add_assertion(result, "command",
                  string_is(member(document, "command"), "0x053E"), "0x053E",
                  value_or_null(member(document, "command")));
    const auto* records = member(document, "records");
    const Json* quote = records && records->is_array() && !records->as_array().empty()
        ? &records->as_array().front() : nullptr;
    const auto* buys = quote ? member(*quote, "buy_levels") : nullptr;
    const auto* sells = quote ? member(*quote, "sell_levels") : nullptr;
    const auto expected_code = contract_id == "market-speed-etf-iopv-live"
        ? "510300" : "000001";
    const bool shape = quote && string_is(member(*quote, "code"), expected_code) &&
        member(*quote, "rise_speed_pct") && member(*quote, "rise_speed_pct")->is_number() &&
        buys && buys->is_array() && buys->size() == 5 &&
        sells && sells->is_array() && sells->size() == 5;
    add_assertion(result, "rise_speed_and_depth", shape,
                  "numeric rise_speed_pct and five bid/ask levels", shape);
    const bool decoder_provenance = quote && member(*quote, "rise_speed_raw") &&
        member(*quote, "rise_speed_raw")->is_number() &&
        member(*quote, "status_raw") && member(*quote, "status_raw")->is_number() &&
        member(*quote, "tail_raw") && member(*quote, "tail_raw")->is_number();
    add_assertion(result, "decoder_provenance", decoder_provenance, true,
                  decoder_provenance);
    const auto* transport = member(document, "transport");
    const auto* attempts = transport ? member(*transport, "connection_attempts") : nullptr;
    const auto* retries = transport ? member(*transport, "transient_retries") : nullptr;
    const bool bounded_retry = transport && transport->is_object() && attempts &&
        attempts->is_number() && attempts->as_number() >= 1 &&
        attempts->as_number() <= 9 && retries && retries->is_number() &&
        retries->as_number() >= 0 && retries->as_number() <= 6 &&
        number_is(member(*transport, "max_attempts_per_endpoint"), 3) &&
        string_is(member(*transport, "endpoint_source"),
                  "connect.cfg:hqhost-primary-first") &&
        bool_is(member(*transport, "primary_configured"), true);
    add_assertion(result, "bounded_transport_retry", bounded_retry,
                  "connect.cfg primary-first, attempts 1..9 and retries 0..6",
                  value_or_null(transport));
    if (contract_id == "market-speed-etf-iopv-live") {
        const auto* last = quote ? member(*quote, "last_price") : nullptr;
        const auto* iopv = quote ? member(*quote, "fund_iopv") : nullptr;
        const auto* raw = quote ? member(*quote, "auxiliary_price_delta_raw") : nullptr;
        const bool live_iopv_shape = last && last->is_number() && last->as_number() > 0 &&
            iopv && iopv->is_number() && iopv->as_number() > 0 &&
            raw && raw->is_number();
        bool zero_depth = buys && buys->is_array() && sells && sells->is_array();
        if (zero_depth) {
            for (const auto* levels : {buys, sells})
                for (const auto& level : levels->as_array())
                    zero_depth = zero_depth &&
                        number_is(member(level, "price"), 0.0) &&
                        number_is(member(level, "volume_hand"), 0.0);
        }
        const auto* pre_close = quote ? member(*quote, "pre_close_price") : nullptr;
        const bool premarket_reset = quote && last && number_is(last, 0.0) &&
            iopv && iopv->is_null() && number_is(raw, 0.0) && pre_close &&
            pre_close->is_number() && pre_close->as_number() > 0.0 &&
            number_is(member(*quote, "amount"), 0.0) &&
            number_is(member(*quote, "total_hand"), 0.0) && zero_depth;
        const bool auction_iopv_shape = quote && last && number_is(last, 0.0) &&
            iopv && iopv->is_number() && iopv->as_number() > 0.0 &&
            raw && raw->is_number() && pre_close && pre_close->is_number() &&
            pre_close->as_number() > 0.0;
        const bool iopv_shape = live_iopv_shape || premarket_reset ||
            auction_iopv_shape;
        add_assertion(result, "etf_iopv_shape", iopv_shape,
                      "positive live IOPV, explicit zero-depth premarket reset, or call-auction IOPV against pre-close",
                      iopv_shape);
        const bool live_iopv_scale = live_iopv_shape &&
            iopv->as_number() / last->as_number() > 0.8 &&
            iopv->as_number() / last->as_number() < 1.2;
        const bool auction_iopv_scale = auction_iopv_shape &&
            iopv->as_number() / pre_close->as_number() > 0.8 &&
            iopv->as_number() / pre_close->as_number() < 1.2;
        const bool iopv_scale = premarket_reset || live_iopv_scale ||
            auction_iopv_scale;
        add_assertion(result, "etf_iopv_scale", iopv_scale,
                      "IOPV/live-price or IOPV/pre-close in (0.8, 1.2), or premarket-reset branch",
                      iopv_scale);
    }
}

void validate_empty_abnormal(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "availability",
                  string_is(member(document, "availability"), "empty"), "empty",
                  value_or_null(member(document, "availability")));
    const auto* data = member(document, "data");
    add_assertion(result, "data_null", data && data->is_null(), Json(nullptr),
                  value_or_null(data));
}

void validate_empty_active_funds(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    const auto* selected = member(document, "selected_holding");
    add_assertion(result, "selected_holding_null", selected && selected->is_null(),
                  Json(nullptr), value_or_null(selected));
    add_assertion(result, "funds_zero",
                  number_is(member_path(document, {"counts", "funds"}), 0), 0,
                  value_or_null(member_path(document, {"counts", "funds"})));
    add_assertion(result, "securities_empty", array_empty(member(document, "securities")),
                  Json::array(), value_or_null(member(document, "securities")));
}

void validate_empty_institution_lhb(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    const auto* selected = member(document, "selected_ranking");
    add_assertion(result, "selected_ranking_null", selected && selected->is_null(),
                  Json(nullptr), value_or_null(selected));
    add_assertion(result, "rankings_zero",
                  number_is(member_path(document, {"counts", "rankings"}), 0), 0,
                  value_or_null(member_path(document, {"counts", "rankings"})));
    add_assertion(result, "events_empty", array_empty(member(document, "events")),
                  Json::array(), value_or_null(member(document, "events")));
}

void validate_empty_foreign_alerts(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    const auto* selected = member(document, "selected_alert");
    add_assertion(result, "selected_alert_null", selected && selected->is_null(),
                  Json(nullptr), value_or_null(selected));
    add_assertion(result, "alerts_zero",
                  number_is(member_path(document, {"counts", "alerts"}), 0), 0,
                  value_or_null(member_path(document, {"counts", "alerts"})));
    add_assertion(result, "history_empty", array_empty(member(document, "history")),
                  Json::array(), value_or_null(member(document, "history")));
}

void validate_empty_fund_holdings(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "availability",
                  string_is(member(document, "availability"), "empty"), "empty",
                  value_or_null(member(document, "availability")));
    add_assertion(result, "returned_zero",
                  number_is(member_path(document, {"counts", "returned"}), 0), 0,
                  value_or_null(member_path(document, {"counts", "returned"})));
    add_assertion(result, "records_empty", array_empty(member(document, "records")),
                  Json::array(), value_or_null(member(document, "records")));
    add_assertion(result, "fallback_disabled",
                  bool_is(member_path(document,
                      {"parameters", "latest_report_fallback_used"}), false), false,
                  value_or_null(member_path(document,
                      {"parameters", "latest_report_fallback_used"})));
}

void validate_intraday_funds(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    const auto* availability = member(document, "availability");
    const bool usable = string_is(availability, "live") ||
                        string_is(availability, "stale-cache");
    add_assertion(result, "availability", usable, "live or stale-cache",
                  value_or_null(availability));
    add_assertion(result, "mode",
                  string_is(member(document, "mode"), "security"), "security",
                  value_or_null(member(document, "mode")));
    add_assertion(result, "security_code",
                  string_is(member_path(document, {"security", "code"}), "000001"),
                  "000001", value_or_null(member_path(document, {"security", "code"})));
    add_assertion(result, "funds_found", bool_is(member(document, "found"), true), true,
                  value_or_null(member(document, "found")));
    const auto* stale = member_path(document, {"cache", "stale"});
    add_assertion(result, "cache_stale_boolean", stale && stale->is_bool(), "boolean",
                  value_or_null(stale));
    add_assertion(result, "master_request_id",
                  string_is(member_path(document, {"source", "master_request_id"}),
                            "200340"),
                  "200340", value_or_null(member_path(document,
                      {"source", "master_request_id"})));
    add_assertion(result, "detail_request_id",
                  string_is(member_path(document, {"source", "detail_request_id"}),
                            "200341"),
                  "200341", value_or_null(member_path(document,
                      {"source", "detail_request_id"})));
}

struct QuoteContract {
    std::string_view id;
    QuoteContractValidator validate;
};

constexpr std::array<QuoteContract, 16> quote_contracts{{
    {"market-kline-30m-live", validate_market_kline_30m},
    {"market-auction-native-live", validate_market_kline_30m},
    {"market-trades-native-live", validate_market_kline_30m},
    {"market-ranking-native-live", validate_market_kline_30m},
    {"market-securities-native-live", validate_market_kline_30m},
    {"historical-securities-local", validate_historical_securities},
    {"invalid-market", validate_invalid_market},
    {"stock-native-valuation-live", validate_stock_native_valuation},
    {"market-speed-live", validate_market_speed},
    {"market-speed-etf-iopv-live", validate_market_speed},
    {"empty-abnormal", validate_empty_abnormal},
    {"empty-active-funds", validate_empty_active_funds},
    {"empty-institution-lhb", validate_empty_institution_lhb},
    {"empty-foreign-alerts", validate_empty_foreign_alerts},
    {"empty-fund-holdings", validate_empty_fund_holdings},
    {"intraday-funds-live", validate_intraday_funds},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < quote_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < quote_contracts.size(); ++right)
            if (quote_contracts[left].id == quote_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_market_data_quote_contract(const std::string& contract_id,
    const Json& document, const Json& context, Json& result) {
    (void)context;
    for (const auto& contract : quote_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(contract_id, document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail
