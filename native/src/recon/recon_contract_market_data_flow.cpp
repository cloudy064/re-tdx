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

using FlowContractValidator = void (*)(const std::string& contract_id, const Json& document, Json& result);

void validate_intelligence_topics(const std::string& contract_id, const Json& document,
        Json& result) {
    const bool topics = contract_id == "intelligence-topics-live";
    const bool topic = contract_id == "intelligence-topic-detail-live";
    const bool news = contract_id == "intelligence-news-live";
    const bool anomaly = contract_id == "market-anomalies-live";
    const auto expected_view = topics ? "topics" : topic ? "topic" : news ? "news" :
        anomaly ? "market-anomalies" : "event";
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-intelligence-native-v1"),
                  "tdx-market-intelligence-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view", string_is(member(document, "view"), expected_view),
                  expected_view, value_or_null(member(document, "view")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available, "live or stale-cache",
                  value_or_null(availability));
    const auto* records = member(document, "records");
    const Json* first = records && records->is_array() && records->size()
        ? &records->as_array().front() : nullptr;
    add_assertion(result, "records_nonempty", first != nullptr, "non-empty array",
                  value_or_null(records));
    add_assertion(result, "raw_row_retained",
                  first && member(*first, "raw") && member(*first, "raw")->is_object(),
                  "object", first ? value_or_null(member(*first, "raw")) : Json(nullptr));
    bool exact_sources = false;
    if (topics) {
        exact_sources = source_exists(document, "list/func_ztxx101_1.jsn");
        add_assertion(result, "typed_topic",
                      first && member(*first, "topic_id") &&
                          member(*first, "topic_id")->is_string() &&
                          !member(*first, "topic_id")->as_string().empty() &&
                          member(*first, "name") && member(*first, "name")->is_string() &&
                          member(*first, "updated_date") &&
                          member(*first, "updated_date")->is_string() &&
                          member(*first, "updated_date")->as_string().size() == 8,
                      "topic id/name/YYYYMMDD", first ? *first : Json(nullptr));
    } else if (topic || news) {
        const auto* content = first ? member(*first, "content") : nullptr;
        const bool plain = content && content->is_string() &&
            !content->as_string().empty() &&
            content->as_string().find("<p") == std::string::npos &&
            content->as_string().find("<a") == std::string::npos;
        add_assertion(result, "safe_plain_text", plain,
                      "non-empty text without embedded p/a tags", value_or_null(content));
        add_assertion(result, "typed_timeline",
                      first && member(*first, "headline") &&
                          member(*first, "headline")->is_string() &&
                          member(*first, "date") && member(*first, "date")->is_string() &&
                          member(*first, "date")->as_string().size() == 8,
                      "headline and YYYYMMDD", first ? *first : Json(nullptr));
        exact_sources = news
            ? source_exists(document, "list/func_xwlb101_1.jsn")
            : source_exists(document, "list/func_ztxx101_1.jsn") &&
                source_exists(document, "ztxx/1039.jsn") &&
                string_is(member_path(document, {"selected_topic", "topic_id"}), "1039");
    } else if (anomaly) {
        const auto previous = numeric_value(first ? member(*first, "previous_close") : nullptr);
        const auto close = numeric_value(first ? member(*first, "close") : nullptr);
        const auto points = numeric_value(first ? member(*first, "change_points") : nullptr);
        const auto pct = numeric_value(first ? member(*first, "same_day_change_pct") : nullptr);
        const auto sh_turnover = numeric_value(first ? member(*first, "sh_turnover_yuan") : nullptr);
        const auto sz_turnover = numeric_value(first ? member(*first, "sz_turnover_yuan") : nullptr);
        const auto total_turnover = numeric_value(first ? member(*first, "market_turnover_yuan") : nullptr);
        const bool formulas = previous && close && *previous != 0 && points && pct &&
            std::abs(*points - (*close - *previous)) < 0.000001 &&
            std::abs(*pct - (*close - *previous) * 100.0 / *previous) < 0.000001 &&
            sh_turnover && sz_turnover && total_turnover &&
            std::abs(*total_turnover - (*sh_turnover + *sz_turnover)) < 0.01;
        add_assertion(result, "market_formulas", formulas,
                      "points, percent and combined turnover match source fields", formulas);
        exact_sources = source_exists(document, "list/func_dpyd101_1.jsn");
    } else {
        const auto* reconciliation = member(document, "event_reconciliation");
        add_assertion(result, "dynamic_member_reconciliation",
                      reconciliation && bool_is(member(*reconciliation, "exact_match"), true) &&
                          first && string_is(member(*first, "member_source"), "dynamic-detail") &&
                          member(*first, "members") && member(*first, "members")->is_array() &&
                          member(*first, "members")->size() >= 1,
                      "exact dynamic-detail member set", value_or_null(reconciliation));
        exact_sources = source_exists(document, "list/func_sjqd101_1.jsn") &&
                        source_exists(document, "sjqd/20115.jsn");
    }
    add_assertion(result, "exact_sources", exact_sources, true, exact_sources);
}

void validate_margin_classifications(const std::string& contract_id, const Json& document,
        Json& result) {
    const bool classifications = contract_id == "margin-classifications-live";
    const bool history = contract_id == "margin-classification-history-live";
    const bool transfer = contract_id == "margin-transfer-live";
    const auto expected_view = classifications ? "classifications" :
        history ? "classification-history" : transfer ? "transfer" : "security";
    add_assertion(result, "schema",
                  string_is(member(document, "schema"), "tdx-market-margin-native-v1"),
                  "tdx-market-margin-native-v1", value_or_null(member(document, "schema")));
    add_assertion(result, "view", string_is(member(document, "view"), expected_view),
                  expected_view, value_or_null(member(document, "view")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available, "live or stale-cache",
                  value_or_null(availability));
    const auto* records = member(document, "records");
    const Json* first = records && records->is_array() && records->size()
        ? &records->as_array().front() : nullptr;
    add_assertion(result, "typed_raw_record",
                  first && member(*first, "date") && member(*first, "date")->is_string() &&
                      member(*first, "date")->as_string().size() == 8 &&
                      member(*first, "raw") && member(*first, "raw")->is_object(),
                  "YYYYMMDD and raw row", first ? *first : Json(nullptr));
    bool exact_sources = false;
    if (classifications) {
        add_assertion(result, "classification_identity",
                      first && string_is(member(*first, "classification"), "industry") &&
                          member(*first, "classification_code") &&
                          member(*first, "classification_code")->is_string() &&
                          member(*first, "classification_name") &&
                          member(*first, "classification_name")->is_string(),
                      "industry code and name", first ? *first : Json(nullptr));
        exact_sources = source_exists(document, "list/func_rzrq101_1.jsn") &&
                        source_prefix_exists(document, "rzrq6/");
    } else if (history) {
        const auto raw_financing = numeric_value(first
            ? member_path(*first, {"raw", "hyrzye"}) : nullptr);
        const auto raw_short = numeric_value(first
            ? member_path(*first, {"raw", "hyrqye"}) : nullptr);
        const auto financing = numeric_value(first
            ? member(*first, "financing_balance_yuan") : nullptr);
        const auto short_balance = numeric_value(first
            ? member(*first, "short_balance_yuan") : nullptr);
        const bool units = raw_financing && raw_short && financing && short_balance &&
            std::abs(*financing - *raw_financing * 1e8) < 0.01 &&
            std::abs(*short_balance - *raw_short * 1e7) < 0.01;
        add_assertion(result, "classification_history_units", units,
                      "financing x1e8 and short x1e7 yuan", units);
        exact_sources = source_exists(document, "rzrq5/880868.jsn") &&
                        string_is(member(document, "group_id"), "880868");
    } else if (transfer) {
        const auto raw_repaid = numeric_value(first
            ? member_path(*first, {"raw", "zrz2"}) : nullptr);
        const auto raw_balance = numeric_value(first
            ? member_path(*first, {"raw", "zrz7"}) : nullptr);
        const auto repaid = numeric_value(first
            ? member(*first, "transfer_financing_repaid_yuan") : nullptr);
        const auto balance = numeric_value(first
            ? member(*first, "transfer_financing_balance_yuan") : nullptr);
        const bool raw_units = raw_repaid && raw_balance && repaid && balance &&
            std::abs(*repaid - *raw_repaid) < 0.01 &&
            std::abs(*balance - *raw_balance) < 0.01;
        add_assertion(result, "transfer_raw_units", raw_units,
                      "zrz2/zrz7 preserved as raw yuan", raw_units);
        exact_sources = source_exists(document, "list/func_rzt101_1.jsn");
    } else {
        const auto* trend = member(document, "trend");
        add_assertion(result, "etf_chart_nonempty",
                      trend && trend->is_array() && trend->size() >= 1,
                      "non-empty trend", value_or_null(trend));
        exact_sources = source_exists(document, "rzrq3/1510900.jsn") &&
                        source_exists(document, "rzrq4/1510900.jsn");
    }
    add_assertion(result, "exact_sources", exact_sources, true, exact_sources);
}

void validate_stock_connect_chart(const std::string& contract_id, const Json& document,
        Json& result) {
    const bool chart = contract_id == "stock-connect-chart-live";
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-stock-connect-native-v1"),
                  "tdx-market-stock-connect-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view",
                  string_is(member(document, "view"), chart ? "security" : "industry-detail"),
                  chart ? "security" : "industry-detail",
                  value_or_null(member(document, "view")));
    const auto* records = member(document, "records");
    const auto* trend = member(document, "trend");
    const Json* first = records && records->is_array() && records->size()
        ? &records->as_array().front() : nullptr;
    const Json* trend_first = trend && trend->is_array() && trend->size()
        ? &trend->as_array().front() : nullptr;
    add_assertion(result, "primary_and_chart_nonempty",
                  first && trend_first, "non-empty records and trend",
                  Json(first != nullptr && trend_first != nullptr));
    bool semantics = false, exact_sources = false;
    if (chart) {
        semantics = bool_is(member_path(document, {"reconciliation", "all_matched"}), true);
        exact_sources = source_exists(document, "hsgtcg1/jd000001.jsn") &&
                        source_exists(document, "hsgtcg2/jd000001.jsn");
    } else {
        const auto raw = numeric_value(trend_first
            ? member_path(*trend_first, {"raw", "drjlr"}) : nullptr);
        const auto yuan = numeric_value(trend_first
            ? member(*trend_first, "daily_net_inflow_yuan") : nullptr);
        semantics = raw && yuan && std::abs(*yuan - *raw * 1e8) < 0.01 &&
            first && member(*first, "security") && member(*first, "security")->is_object();
        exact_sources = source_exists(document, "ggthy/70HK0201.jsn") &&
                        source_exists(document, "ggthy1/70HK0201.jsn") &&
                        string_is(member(document, "group_id"), "70HK0201");
    }
    add_assertion(result, "projection_semantics", semantics,
                  chart ? "all chart rows reconcile" : "industry trend x1e8 yuan",
                  semantics);
    add_assertion(result, "exact_sources", exact_sources, true, exact_sources);
}

void validate_ownership_change_chart(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-ownership-native-v1"),
                  "tdx-market-ownership-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view", string_is(member(document, "view"), "statistics"),
                  "statistics", value_or_null(member(document, "view")));
    const auto* trend = member(document, "change_count_trend");
    const auto* reconciliation = member(document, "change_count_reconciliation");
    const auto chart_rows = numeric_value(reconciliation
        ? member(*reconciliation, "chart_rows") : nullptr);
    const auto matched = numeric_value(reconciliation
        ? member(*reconciliation, "matched_rows") : nullptr);
    const auto mismatch = numeric_value(reconciliation
        ? member(*reconciliation, "mismatch_rows") : nullptr);
    const bool shape = trend && trend->is_array() && trend->size() >= 1 &&
        chart_rows && *chart_rows == static_cast<double>(trend->size()) &&
        matched && mismatch && *matched + *mismatch <= *chart_rows &&
        member(trend->as_array().front(), "raw") &&
        member(trend->as_array().front(), "raw")->is_object();
    add_assertion(result, "chart_reconciliation_shape", shape,
                  "trend count and overlap accounting", value_or_null(reconciliation));
    const bool exact_source = source_prefix_exists(document, "gdzjc1/");
    add_assertion(result, "exact_chart_source", exact_source, "gdzjc1/*", exact_source);
}

void validate_stock_connect_activity(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-stock-connect-native-v1"),
                  "tdx-market-stock-connect-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view", string_is(member(document, "view"), "activity"),
                  "activity", value_or_null(member(document, "view")));
    add_assertion(result, "category",
                  string_is(member(document, "category"), "daily-increase"),
                  "daily-increase", value_or_null(member(document, "category")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available, "live or stale-cache",
                  value_or_null(availability));
    const auto* count = member(document, "count");
    add_assertion(result, "records_nonempty",
                  count && count->is_number() && count->as_number() > 0,
                  "number > 0", value_or_null(count));
    const auto* records = member(document, "records");
    const Json* record = records && records->is_array() && !records->as_array().empty()
        ? &records->as_array().front() : nullptr;
    const auto* date = record ? member(*record, "date") : nullptr;
    add_assertion(result, "snapshot_date",
                  date && date->is_string() && date->as_string().size() == 8,
                  "YYYYMMDD", value_or_null(date));
    const auto* freshness = record ? member(*record, "snapshot_freshness") : nullptr;
    const bool explicit_freshness = string_is(freshness, "current-window") ||
                                    string_is(freshness, "historical-snapshot");
    add_assertion(result, "snapshot_freshness", explicit_freshness,
                  "current-window or historical-snapshot", value_or_null(freshness));
    const auto* raw = record ? member(*record, "raw") : nullptr;
    add_assertion(result, "raw_row_retained", raw && raw->is_object(), "object",
                  value_or_null(raw));
    const auto* sources = member(document, "sources");
    bool source_shape = sources && sources->is_array() && !sources->as_array().empty();
    if (source_shape) {
        const auto& source = sources->as_array().front();
        const auto* attempts = member(source, "attempts");
        const auto* stale = member(source, "stale");
        const auto* error = member(source, "upstream_error");
        source_shape = string_is(member(source, "resource"),
                                 "list/func_hsgt205_1.jsn") &&
            attempts && attempts->is_number() && attempts->as_number() >= 1 &&
            stale && stale->is_bool() && error &&
            (error->is_null() || error->is_string());
    }
    add_assertion(result, "source_resilience_shape", source_shape, true, source_shape);
}

void validate_empty_stock_connect_industry(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-stock-connect-native-v1"),
                  "tdx-market-stock-connect-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view", string_is(member(document, "view"), "industry"),
                  "industry", value_or_null(member(document, "view")));
    add_assertion(result, "category",
                  string_is(member(document, "category"), "northbound-industry"),
                  "northbound-industry", value_or_null(member(document, "category")));
    add_assertion(result, "availability",
                  string_is(member(document, "availability"), "empty"), "empty",
                  value_or_null(member(document, "availability")));
    add_assertion(result, "count_zero", number_is(member(document, "count"), 0), 0,
                  value_or_null(member(document, "count")));
    add_assertion(result, "records_empty", array_empty(member(document, "records")),
                  Json::array(), value_or_null(member(document, "records")));
    const auto* sources = member(document, "sources");
    const Json* source = sources && sources->is_array() && !sources->as_array().empty()
        ? &sources->as_array().front() : nullptr;
    add_assertion(result, "configured_resource",
                  source && string_is(member(*source, "resource"),
                                      "list/func_hsgt107_1.jsn"),
                  "list/func_hsgt107_1.jsn",
                  source ? value_or_null(member(*source, "resource")) : Json(nullptr));
    add_assertion(result, "missing_explicit",
                  source && bool_is(member(*source, "missing"), true), true,
                  source ? value_or_null(member(*source, "missing")) : Json(nullptr));
}

void validate_ownership_rankings(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-ownership-native-v1"),
                  "tdx-market-ownership-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view", string_is(member(document, "view"), "rankings"),
                  "rankings", value_or_null(member(document, "view")));
    add_assertion(result, "category",
                  string_is(member(document, "category"), "decrease-ratio"),
                  "decrease-ratio", value_or_null(member(document, "category")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available, "live or stale-cache",
                  value_or_null(availability));
    const auto* ranking_views = member_path(document, {"summary", "ranking_views"});
    const auto* ranking_rows = member_path(document, {"summary", "ranking_rows"});
    add_assertion(result, "six_views", number_is(ranking_views, 6), 6,
                  value_or_null(ranking_views));
    add_assertion(result, "six_hundred_rows",
                  ranking_rows && ranking_rows->is_number() &&
                      ranking_rows->as_number() >= 600,
                  "number >= 600", value_or_null(ranking_rows));
    const auto* rankings = member(document, "rankings");
    const Json* row = rankings && rankings->is_array() &&
                              !rankings->as_array().empty()
        ? &rankings->as_array().front() : nullptr;
    add_assertion(result, "decrease_direction",
                  row && string_is(member(*row, "direction"), "decrease"),
                  "decrease",
                  row ? value_or_null(member(*row, "direction")) : Json(nullptr));
    const auto* pct = row ? member(*row, "signed_float_change_pct") : nullptr;
    add_assertion(result, "signed_decrease_pct",
                  pct && pct->is_number() && pct->as_number() <= 0,
                  "number <= 0", value_or_null(pct));
    const auto* raw = row ? member(*row, "raw") : nullptr;
    add_assertion(result, "raw_row_retained", raw && raw->is_object(), "object",
                  value_or_null(raw));
    const auto* sources = member(document, "sources");
    bool source_shape = false;
    if (sources && sources->is_array()) {
        for (const auto& candidate : sources->as_array()) {
            if (!string_is(member(candidate, "resource"),
                           "list/func_zcjc108_1.jsn"))
                continue;
            const auto* attempts = member(candidate, "attempts");
            const auto* stale = member(candidate, "stale");
            const auto* error = member(candidate, "upstream_error");
            source_shape = attempts && attempts->is_number() &&
                attempts->as_number() >= 1 && stale && stale->is_bool() &&
                error && (error->is_null() || error->is_string());
            break;
        }
    }
    add_assertion(result, "decrease_ratio_source", source_shape, true,
                  source_shape);
}

void validate_shareholder_counts(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-ownership-native-v1"),
                  "tdx-market-ownership-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view",
                  string_is(member(document, "view"), "shareholder-counts"),
                  "shareholder-counts", value_or_null(member(document, "view")));
    add_assertion(result, "category", string_is(member(document, "category"), "bj"),
                  "bj", value_or_null(member(document, "category")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available, "live or stale-cache",
                  value_or_null(availability));
    const auto* total_rows = member_path(document, {"summary", "shareholder_rows"});
    add_assertion(result, "market_coverage",
                  total_rows && total_rows->is_number() &&
                      total_rows->as_number() >= 5000,
                  "number >= 5000", value_or_null(total_rows));
    const auto* rows = member(document, "shareholder_counts");
    const Json* row = rows && rows->is_array() && !rows->as_array().empty()
        ? &rows->as_array().front() : nullptr;
    add_assertion(result, "bj_board",
                  row && string_is(member(*row, "board"), "bj"), "bj",
                  row ? value_or_null(member(*row, "board")) : Json(nullptr));
    const auto* security = row ? member(*row, "security") : nullptr;
    const auto* code = security ? member(*security, "code") : nullptr;
    add_assertion(result, "security_code",
                  code && code->is_string() && code->as_string().size() == 6,
                  "six-digit code", value_or_null(code));
    const auto* daily = row ? member(*row, "daily_household_change_pct") : nullptr;
    add_assertion(result, "daily_change_number",
                  daily && daily->is_number(), "number", value_or_null(daily));
    const auto* raw = row ? member(*row, "raw") : nullptr;
    add_assertion(result, "raw_row_retained", raw && raw->is_object(), "object",
                  value_or_null(raw));
    const auto* sources = member(document, "sources");
    bool source_shape = false;
    if (sources && sources->is_array()) {
        for (const auto& candidate : sources->as_array()) {
            if (!string_is(member(candidate, "resource"),
                           "list/func_gdrs107_1.jsn"))
                continue;
            const auto* attempts = member(candidate, "attempts");
            const auto* stale = member(candidate, "stale");
            source_shape = attempts && attempts->is_number() &&
                attempts->as_number() >= 1 && stale && stale->is_bool();
            break;
        }
    }
    add_assertion(result, "bj_source", source_shape, true, source_shape);
}

void validate_empty_shareholder_counts_legacy(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-ownership-native-v1"),
                  "tdx-market-ownership-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view",
                  string_is(member(document, "view"), "shareholder-counts"),
                  "shareholder-counts", value_or_null(member(document, "view")));
    add_assertion(result, "category",
                  string_is(member(document, "category"), "sz-sme-legacy"),
                  "sz-sme-legacy", value_or_null(member(document, "category")));
    add_assertion(result, "availability",
                  string_is(member(document, "availability"), "empty"), "empty",
                  value_or_null(member(document, "availability")));
    const auto* rows = member(document, "shareholder_counts");
    add_assertion(result, "records_empty", array_empty(rows), Json::array(),
                  value_or_null(rows));
    const auto* sources = member(document, "sources");
    const Json* legacy = nullptr;
    if (sources && sources->is_array()) {
        for (const auto& candidate : sources->as_array())
            if (string_is(member(candidate, "resource"),
                          "list/func_gdrs103_1.jsn")) {
                legacy = &candidate;
                break;
            }
    }
    add_assertion(result, "legacy_resource", legacy != nullptr, true,
                  legacy != nullptr);
    add_assertion(result, "missing_explicit",
                  legacy && bool_is(member(*legacy, "missing"), true), true,
                  legacy ? value_or_null(member(*legacy, "missing")) : Json(nullptr));
}

struct FlowContract {
    std::string_view id;
    FlowContractValidator validate;
};

constexpr std::array<FlowContract, 17> flow_contracts{{
    {"intelligence-topics-live", validate_intelligence_topics},
    {"intelligence-topic-detail-live", validate_intelligence_topics},
    {"intelligence-news-live", validate_intelligence_topics},
    {"market-anomalies-live", validate_intelligence_topics},
    {"intelligence-event-detail-live", validate_intelligence_topics},
    {"margin-classifications-live", validate_margin_classifications},
    {"margin-classification-history-live", validate_margin_classifications},
    {"margin-etf-history-live", validate_margin_classifications},
    {"margin-transfer-live", validate_margin_classifications},
    {"stock-connect-chart-live", validate_stock_connect_chart},
    {"stock-connect-industry-detail-live", validate_stock_connect_chart},
    {"ownership-change-chart-live", validate_ownership_change_chart},
    {"stock-connect-activity-live", validate_stock_connect_activity},
    {"empty-stock-connect-industry", validate_empty_stock_connect_industry},
    {"ownership-rankings-live", validate_ownership_rankings},
    {"shareholder-counts-live", validate_shareholder_counts},
    {"empty-shareholder-counts-legacy", validate_empty_shareholder_counts_legacy},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < flow_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < flow_contracts.size(); ++right)
            if (flow_contracts[left].id == flow_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_market_data_flow_contract(const std::string& contract_id,
    const Json& document, const Json& context, Json& result) {
    (void)context;
    for (const auto& contract : flow_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(contract_id, document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail