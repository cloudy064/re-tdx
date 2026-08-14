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

using InstitutionContractValidator = void (*)(const std::string& contract_id, const Json& document, Json& result);

void validate_stock_panorama(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-panorama-native-v1"),
                  "tdx-market-panorama-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view",
                  string_is(member(document, "view"), "capital-flow"),
                  "capital-flow", value_or_null(member(document, "view")));
    add_assertion(result, "market",
                  string_is(member(document, "market"), "sz"), "sz",
                  value_or_null(member(document, "market")));
    add_assertion(result, "security_code",
                  string_is(member(document, "code"), "000001"), "000001",
                  value_or_null(member(document, "code")));
    add_assertion(result, "one_section",
                  number_is(member_path(document, {"counts", "sections"}), 1), 1,
                  value_or_null(member_path(document, {"counts", "sections"})));
    const auto* matched = member_path(document, {"counts", "matched"});
    const bool has_match = matched && matched->is_number() &&
                           matched->as_number() >= 1;
    add_assertion(result, "security_matched", has_match, "number >= 1",
                  value_or_null(matched));

    const auto* sections = member(document, "sections");
    const Json* section = sections && sections->is_array() && !sections->as_array().empty()
        ? &sections->as_array().front() : nullptr;
    add_assertion(result, "capital_flow_resource",
                  section && string_is(member(*section, "resource"),
                                       "list/func_gx_zjlx101_1.jsn"),
                  "list/func_gx_zjlx101_1.jsn",
                  section ? value_or_null(member(*section, "resource")) : Json(nullptr));
    const auto* records = section ? member(*section, "records") : nullptr;
    const Json* record = records && records->is_array() && !records->as_array().empty()
        ? &records->as_array().front() : nullptr;
    add_assertion(result, "record_code",
                  record && string_is(member(*record, "code"), "000001"), "000001",
                  record ? value_or_null(member(*record, "code")) : Json(nullptr));
    const auto* raw = record ? member(*record, "raw") : nullptr;
    add_assertion(result, "raw_row_retained", raw && raw->is_object(), "object",
                  value_or_null(raw));
    const auto* statistics_date = record
        ? member_path(*record, {"data", "statistics_date"}) : nullptr;
    add_assertion(result, "statistics_date",
                  statistics_date && statistics_date->is_string() &&
                      !statistics_date->as_string().empty(),
                  "non-empty string", value_or_null(statistics_date));

    const auto* sources = member(document, "sources");
    bool source_shape = sources && sources->is_array() && !sources->as_array().empty();
    if (source_shape) {
        const auto& source = sources->as_array().front();
        const auto* endpoint = member(source, "endpoint");
        const auto* attempts = member(source, "attempts");
        const auto* stale = member(source, "stale");
        const auto* error = member(source, "upstream_error");
        source_shape = source.is_object() &&
            string_is(member(source, "resource"), "list/func_gx_zjlx101_1.jsn") &&
            endpoint && endpoint->is_string() && !endpoint->as_string().empty() &&
            attempts && attempts->is_number() && attempts->as_number() >= 1 &&
            stale && stale->is_bool() && error &&
            (error->is_null() || error->is_string());
    }
    add_assertion(result, "source_resilience_shape", source_shape, true, source_shape);
    const auto* upstream_stale = member_path(document, {"upstream_health", "stale"});
    const auto* max_attempts = member_path(document,
                                           {"upstream_health", "max_attempts"});
    add_assertion(result, "upstream_stale_boolean",
                  upstream_stale && upstream_stale->is_bool(), "boolean",
                  value_or_null(upstream_stale));
    add_assertion(result, "max_attempts_number",
                  max_attempts && max_attempts->is_number() &&
                      max_attempts->as_number() >= 1,
                  "number >= 1", value_or_null(max_attempts));
}

void validate_stock_institution_analysis(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-institution-analysis-native-v1"),
                  "tdx-institution-analysis-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view", string_is(member(document, "view"), "all"),
                  "all", value_or_null(member(document, "view")));
    add_assertion(result, "market", string_is(member(document, "market"), "sz"),
                  "sz", value_or_null(member(document, "market")));
    add_assertion(result, "security_code",
                  string_is(member(document, "code"), "000001"), "000001",
                  value_or_null(member(document, "code")));
    add_assertion(result, "one_section",
                  number_is(member_path(document, {"counts", "sections"}), 1), 1,
                  value_or_null(member_path(document, {"counts", "sections"})));
    const auto* matched = member_path(document, {"counts", "matched"});
    const bool has_match = matched && matched->is_number() && matched->as_number() >= 1;
    add_assertion(result, "security_matched", has_match, "number >= 1",
                  value_or_null(matched));
    const auto* sections = member(document, "sections");
    const Json* section = sections && sections->is_array() && !sections->as_array().empty()
        ? &sections->as_array().front() : nullptr;
    add_assertion(result, "all_institutions_resource",
                  section && string_is(member(*section, "resource"),
                                       "list/func_cgfx101_1.jsn"),
                  "list/func_cgfx101_1.jsn",
                  section ? value_or_null(member(*section, "resource")) : Json(nullptr));
    const auto* records = section ? member(*section, "records") : nullptr;
    const Json* record = records && records->is_array() && !records->as_array().empty()
        ? &records->as_array().front() : nullptr;
    add_assertion(result, "record_code",
                  record && string_is(member(*record, "code"), "000001"), "000001",
                  record ? value_or_null(member(*record, "code")) : Json(nullptr));
    const auto* raw = record ? member(*record, "raw") : nullptr;
    add_assertion(result, "raw_row_retained", raw && raw->is_object(), "object",
                  value_or_null(raw));
    const auto* report_date = record
        ? member_path(*record, {"data", "report_date"}) : nullptr;
    add_assertion(result, "report_date",
                  report_date && report_date->is_string() &&
                      !report_date->as_string().empty(),
                  "non-empty string", value_or_null(report_date));
    const auto* institution_count = record
        ? member_path(*record, {"data", "institution_count"}) : nullptr;
    add_assertion(result, "institution_count_number",
                  institution_count && institution_count->is_number() &&
                      institution_count->as_number() >= 0,
                  "number >= 0", value_or_null(institution_count));
    const auto* sources = member(document, "sources");
    bool source_shape = sources && sources->is_array() && !sources->as_array().empty();
    if (source_shape) {
        const auto& source = sources->as_array().front();
        const auto* endpoint = member(source, "endpoint");
        const auto* attempts = member(source, "attempts");
        const auto* stale = member(source, "stale");
        const auto* error = member(source, "upstream_error");
        source_shape = source.is_object() &&
            string_is(member(source, "resource"), "list/func_cgfx101_1.jsn") &&
            endpoint && endpoint->is_string() && !endpoint->as_string().empty() &&
            attempts && attempts->is_number() && attempts->as_number() >= 1 &&
            stale && stale->is_bool() && error &&
            (error->is_null() || error->is_string());
    }
    add_assertion(result, "source_resilience_shape", source_shape, true, source_shape);
    const auto* upstream_stale = member_path(document, {"upstream_health", "stale"});
    const auto* max_attempts = member_path(document,
                                           {"upstream_health", "max_attempts"});
    add_assertion(result, "upstream_stale_boolean",
                  upstream_stale && upstream_stale->is_bool(), "boolean",
                  value_or_null(upstream_stale));
    add_assertion(result, "max_attempts_number",
                  max_attempts && max_attempts->is_number() &&
                      max_attempts->as_number() >= 1,
                  "number >= 1", value_or_null(max_attempts));
}

void validate_generic_tqlex(const std::string& contract_id, const Json& document,
        Json& result) {
    const bool tqlex = contract_id == "generic-tqlex-live";
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            tqlex ? "tdx-tqlex-native-v1"
                                  : "tdx-pbrpc-native-v1"),
                  tqlex ? "tdx-tqlex-native-v1" : "tdx-pbrpc-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "request_id",
                  string_is(member(document, "request_id"),
                            tqlex ? "200626" : "200340"),
                  tqlex ? "200626" : "200340",
                  value_or_null(member(document, "request_id")));
    const auto* attempts = member(document, "attempts");
    const auto* max_attempts = member(document, "max_attempts");
    add_assertion(result, "bounded_attempts",
                  attempts && attempts->is_number() &&
                      attempts->as_number() >= 1 &&
                      attempts->as_number() <= 3,
                  "1..3", value_or_null(attempts));
    add_assertion(result, "max_attempts",
                  number_is(max_attempts, 3), 3,
                  value_or_null(max_attempts));
    const auto* response = member(document, "response");
    add_assertion(result, "upstream_success",
                  response && response->is_object() &&
                      number_is(member(*response, "ErrorCode"), 0),
                  "response.ErrorCode = 0", value_or_null(response));
    const auto* result_sets = response && response->is_object()
        ? member(*response, "ResultSets") : nullptr;
    const Json* first_set = result_sets && result_sets->is_array() &&
            !result_sets->as_array().empty()
        ? &result_sets->as_array().front() : nullptr;
    const auto* rows = first_set ? member(*first_set, "RowNum") : nullptr;
    add_assertion(result, "non_empty_result",
                  rows && rows->is_number() && rows->as_number() >= 1,
                  "RowNum >= 1", value_or_null(rows));
    if (!tqlex) {
        const auto* rounds = member(document, "rounds");
        add_assertion(result, "rpc_rounds",
                      rounds && rounds->is_number() &&
                          rounds->as_number() >= 1,
                      "rounds >= 1", value_or_null(rounds));
    }
}

void validate_holder_cross_stock(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-holder-history-native-v1"),
                  "tdx-holder-history-native-v1",
                  value_or_null(member(document, "schema")));
    const auto* availability = member(document, "availability");
    add_assertion(result, "availability",
                  string_is(availability, "live") ||
                      string_is(availability, "stale-cache"),
                  "live or stale-cache", value_or_null(availability));
    add_assertion(result, "holder_id",
                  string_is(member_path(document, {"holder", "holder_id"}),
                            "GD011907"),
                  "GD011907",
                  value_or_null(member_path(document, {"holder", "holder_id"})));
    const auto* records = member_path(document, {"counts", "records"});
    const auto* stock_periods = member_path(document, {"counts", "stock_periods"});
    add_assertion(result, "cross_stock_records",
                  records && records->is_number() && records->as_number() >= 1,
                  "number >= 1", value_or_null(records));
    add_assertion(result, "stock_periods",
                  stock_periods && stock_periods->is_number() &&
                      stock_periods->as_number() >= 1,
                  "number >= 1", value_or_null(stock_periods));
    const auto* returned = member_path(document, {"pagination", "returned"});
    add_assertion(result, "bounded_page",
                  returned && returned->is_number() &&
                      returned->as_number() >= 1 && returned->as_number() <= 5,
                  "1..5", value_or_null(returned));
    const auto* history_attempts = member_path(
        document, {"cache", "history_attempts"});
    const auto* detail_attempts = member_path(
        document, {"cache", "detail_attempts"});
    const auto* history_stale = member_path(
        document, {"cache", "history_stale"});
    const auto* detail_stale = member_path(
        document, {"cache", "detail_stale"});
    add_assertion(result, "history_attempts",
                  history_attempts && history_attempts->is_number() &&
                      history_attempts->as_number() >= 0 &&
                      history_attempts->as_number() <= 3,
                  "0..3", value_or_null(history_attempts));
    add_assertion(result, "detail_attempts",
                  detail_attempts && detail_attempts->is_number() &&
                      detail_attempts->as_number() >= 0 &&
                      detail_attempts->as_number() <= 3,
                  "0..3", value_or_null(detail_attempts));
    add_assertion(result, "stale_flags",
                  history_stale && history_stale->is_bool() &&
                      detail_stale && detail_stale->is_bool(),
                  "two booleans",
                  Json(history_stale && history_stale->is_bool() &&
                       detail_stale && detail_stale->is_bool()));
    add_assertion(result, "max_attempts",
                  number_is(member_path(document, {"cache", "max_attempts"}), 3),
                  3, value_or_null(member_path(
                      document, {"cache", "max_attempts"})));
}

void validate_institution_exclusive_funds(const std::string& contract_id, const Json& document,
        Json& result) {
    const bool exclusive = contract_id == "institution-exclusive-funds-live";
    const std::string expected_view = exclusive ? "exclusive-funds" : "national-team";
    const std::string expected_resource = exclusive
        ? "list/func_tbgz108_1.jsn" : "list/func_tzcg104_1.jsn";
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-institution-analysis-native-v1"),
                  "tdx-institution-analysis-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view",
                  string_is(member(document, "view"), expected_view),
                  expected_view, value_or_null(member(document, "view")));
    const auto* matched = member_path(document, {"counts", "matched"});
    add_assertion(result, "market_rows",
                  matched && matched->is_number() && matched->as_number() >= 150,
                  "number >= 150", value_or_null(matched));
    const auto* sections = member(document, "sections");
    const Json* section = sections && sections->is_array() && sections->size() == 1
        ? &sections->as_array().front() : nullptr;
    add_assertion(result, "section_layout",
                  section && string_is(member(*section, "layout"), expected_view),
                  expected_view,
                  section ? value_or_null(member(*section, "layout")) : Json(nullptr));
    const auto* source_rows = section ? member(*section, "source_rows") : nullptr;
    add_assertion(result, "source_rows",
                  source_rows && source_rows->is_number() &&
                      source_rows->as_number() >= 150,
                  "number >= 150", value_or_null(source_rows));
    const auto* summary = section ? member(*section, "summary") : nullptr;
    const auto* summary_rows = summary ? member(*summary, "rows") : nullptr;
    const auto* unique_securities = summary
        ? member(*summary, "unique_securities") : nullptr;
    const auto* names_resolved = summary ? member(*summary, "names_resolved") : nullptr;
    add_assertion(result, "summary_population",
                  summary_rows && summary_rows->is_number() &&
                      summary_rows->as_number() >= 150 &&
                      unique_securities && unique_securities->is_number() &&
                      unique_securities->as_number() >= 80 &&
                      names_resolved && names_resolved->is_number() &&
                      names_resolved->as_number() >= 150,
                  "at least 150 named rows and 80 securities",
                  value_or_null(summary));
    if (exclusive) {
        const auto* companies = summary
            ? member(*summary, "fund_management_companies") : nullptr;
        const auto* duplicate_rows = summary
            ? member(*summary, "duplicate_company_security_rows") : nullptr;
        add_assertion(result, "fund_company_detail_preserved",
                      companies && companies->is_number() &&
                          companies->as_number() >= 50 &&
                          duplicate_rows && duplicate_rows->is_number() &&
                          duplicate_rows->as_number() >= 1,
                      "at least 50 companies with duplicate security-company rows",
                      value_or_null(summary));
    } else {
        const auto* checked = summary
            ? member(*summary, "combined_formula_checked") : nullptr;
        const auto* mismatches = summary
            ? member(*summary, "combined_formula_mismatches") : nullptr;
        add_assertion(result, "combined_ratio_formula",
                      checked && checked->is_number() &&
                          checked->as_number() >= 150 &&
                          number_is(mismatches, 0),
                      "at least 150 checked and zero mismatches",
                      value_or_null(summary));
    }
    const auto* records = section ? member(*section, "records") : nullptr;
    const Json* first = records && records->is_array() && records->size() >= 2
        ? &records->as_array()[0] : nullptr;
    const Json* second = records && records->is_array() && records->size() >= 2
        ? &records->as_array()[1] : nullptr;
    const auto* first_data = first ? member(*first, "data") : nullptr;
    const auto* second_data = second ? member(*second, "data") : nullptr;
    const auto* raw = first ? member(*first, "raw") : nullptr;
    add_assertion(result, "record_identity",
                  first && bool_is(member(*first, "name_resolved"), true) &&
                      raw && raw->is_object(),
                  "resolved security with raw row", value_or_null(first));
    const char* metric = exclusive ? "float_share_pct" : "combined_ratio_pct";
    const auto* first_metric = first_data ? member(*first_data, metric) : nullptr;
    const auto* second_metric = second_data ? member(*second_data, metric) : nullptr;
    add_assertion(result, "descending_ratio_sort",
                  first_metric && first_metric->is_number() &&
                      second_metric && second_metric->is_number() &&
                      first_metric->as_number() >= second_metric->as_number(),
                  true,
                  first_metric && second_metric && first_metric->is_number() &&
                          second_metric->is_number()
                      ? Json(first_metric->as_number() >= second_metric->as_number())
                      : Json(nullptr));
    if (exclusive) {
        const auto* company = first_data
            ? member(*first_data, "fund_management_company") : nullptr;
        const auto* shares_10k = first_data
            ? member(*first_data, "holding_shares_10k") : nullptr;
        const auto* shares = first_data
            ? member(*first_data, "holding_shares") : nullptr;
        add_assertion(result, "exclusive_fund_fields",
                      company && company->is_string() && !company->as_string().empty() &&
                          shares_10k && shares_10k->is_number() &&
                          shares && shares->is_number() && shares->as_number() > 0,
                      "company and both 10k-share/share values",
                      value_or_null(first_data));
    } else {
        add_assertion(result, "national_team_record_formula",
                      first_data && bool_is(member(*first_data,
                                                  "combined_ratio_formula_matches"), true),
                      true, first_data ? value_or_null(member(
                          *first_data, "combined_ratio_formula_matches")) : Json(nullptr));
    }
    const bool exact_source = source_exists(document, expected_resource);
    add_assertion(result, "exact_source", exact_source,
                  expected_resource, exact_source);
    const auto* sources = member(document, "sources");
    const Json* source = sources && sources->is_array() && sources->size() == 1
        ? &sources->as_array().front() : nullptr;
    const auto* endpoint = source ? member(*source, "endpoint") : nullptr;
    const auto* attempts = source ? member(*source, "attempts") : nullptr;
    const auto* stale = source ? member(*source, "stale") : nullptr;
    add_assertion(result, "source_health",
                  endpoint && endpoint->is_string() && !endpoint->as_string().empty() &&
                      attempts && attempts->is_number() && attempts->as_number() >= 1 &&
                      stale && stale->is_bool(),
                  true, source ? *source : Json(nullptr));
    const auto* upstream_stale = member_path(document, {"upstream_health", "stale"});
    const auto* max_attempts = member_path(document,
                                           {"upstream_health", "max_attempts"});
    add_assertion(result, "upstream_health",
                  upstream_stale && upstream_stale->is_bool() &&
                      max_attempts && max_attempts->is_number() &&
                      max_attempts->as_number() >= 1,
                  true, value_or_null(member(document, "upstream_health")));
}

void validate_institution_stake_building(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-institution-analysis-native-v1"),
                  "tdx-institution-analysis-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view",
                  string_is(member(document, "view"), "stake-building"),
                  "stake-building", value_or_null(member(document, "view")));
    const auto* sections = member(document, "sections");
    const Json* section = sections && sections->is_array() && sections->size() == 1
        ? &sections->as_array().front() : nullptr;
    add_assertion(result, "section_semantics",
                  section && string_is(member(*section, "layout"), "stake-building") &&
                      string_is(member(*section, "resource"),
                                "list/func_tzcg108_1.jsn"),
                  "stake-building / list/func_tzcg108_1.jsn",
                  section ? *section : Json(nullptr));
    const auto* records = section ? member(*section, "records") : nullptr;
    bool typed = records && records->is_array() && !records->as_array().empty();
    bool formula_reproduced = false;
    if (typed) for (const auto& record : records->as_array()) {
        const auto* data = member(record, "data");
        const auto* raw = member(record, "raw");
        typed = typed && data && data->is_object() && raw && raw->is_object() &&
            nonempty_string(member(*data, "announcement_date")) &&
            nonempty_string(member(*data, "shareholder")) &&
            member(*data, "increase_shares") &&
            member(*data, "post_holding_total_pct") &&
            nonempty_string(member(*data, "further_increase")) &&
            nonempty_string(member(*data, "insurance_capital"));
        const auto start = data ? numeric_value(member(*data, "start_adjusted_close"))
                                : std::nullopt;
        const auto end = data ? numeric_value(member(*data, "end_adjusted_close"))
                              : std::nullopt;
        const auto actual = data ? numeric_value(member(*data, "period_return_pct"))
                                 : std::nullopt;
        if (start && end && actual && *start != 0.0 &&
            std::abs(*actual - (*end / *start - 1.0) * 100.0) < 1e-8)
            formula_reproduced = true;
    }
    add_assertion(result, "typed_disclosures", typed,
                  "announcement, holder, shares, flags and raw evidence", typed);
    add_assertion(result, "client_period_return_formula", formula_reproduced,
                  "(price2/price1-1)*100", formula_reproduced);
    const auto* summary = section ? member(*section, "summary") : nullptr;
    add_assertion(result, "stake_summary",
                  summary && member(*summary, "further_increase_yes") &&
                      member(*summary, "further_increase_yes")->is_number() &&
                      member(*summary, "insurance_capital_yes") &&
                      member(*summary, "insurance_capital_yes")->is_number(),
                  "typed further-increase and insurance-capital counts",
                  value_or_null(summary));
    const bool exact_source = source_exists(document, "list/func_tzcg108_1.jsn") &&
        member(document, "sources") && member(document, "sources")->is_array() &&
        member(document, "sources")->size() == 1;
    add_assertion(result, "exact_source", exact_source,
                  "list/func_tzcg108_1.jsn", exact_source);
}

void validate_institution_notable_private_funds(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-institution-analysis-native-v1"),
                  "tdx-institution-analysis-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view",
                  string_is(member(document, "view"), "notable-private-funds"),
                  "notable-private-funds", value_or_null(member(document, "view")));
    const auto* matched = member_path(document, {"counts", "matched"});
    add_assertion(result, "market_rows",
                  matched && matched->is_number() && matched->as_number() >= 50,
                  "number >= 50", value_or_null(matched));
    const auto* sections = member(document, "sections");
    const Json* section = sections && sections->is_array() && sections->size() == 1
        ? &sections->as_array().front() : nullptr;
    add_assertion(result, "section_layout",
                  section && string_is(member(*section, "layout"),
                                       "notable-private-funds"),
                  "notable-private-funds",
                  section ? value_or_null(member(*section, "layout")) : Json(nullptr));
    const auto* summary = section ? member(*section, "summary") : nullptr;
    const auto* rows = summary ? member(*summary, "rows") : nullptr;
    const auto* securities = summary ? member(*summary, "unique_securities") : nullptr;
    const auto* managers = summary ? member(*summary, "private_fund_managers") : nullptr;
    const auto* industries = summary ? member(*summary, "industries") : nullptr;
    const auto* total_value = summary
        ? member(*summary, "holding_market_value") : nullptr;
    add_assertion(result, "relationship_population",
                  rows && rows->is_number() && rows->as_number() >= 50 &&
                      securities && securities->is_number() &&
                      securities->as_number() >= 45 &&
                      managers && managers->is_number() && managers->as_number() >= 10 &&
                      industries && industries->is_number() && industries->as_number() >= 20 &&
                      total_value && total_value->is_number() && total_value->as_number() > 0,
                  "at least 50 holdings, 45 securities, 10 managers and 20 industries",
                  value_or_null(summary));
    const auto* records = section ? member(*section, "records") : nullptr;
    const Json* first = records && records->is_array() && records->size() >= 2
        ? &records->as_array()[0] : nullptr;
    const Json* second = records && records->is_array() && records->size() >= 2
        ? &records->as_array()[1] : nullptr;
    const auto* first_data = first ? member(*first, "data") : nullptr;
    const auto* second_data = second ? member(*second, "data") : nullptr;
    const auto* manager = first_data ? member(*first_data, "private_fund_manager") : nullptr;
    const auto* report_date = first_data ? member(*first_data, "report_date") : nullptr;
    const auto* value_10k = first_data
        ? member(*first_data, "holding_market_value_10k") : nullptr;
    const auto* value_yuan = first_data
        ? member(*first_data, "holding_market_value") : nullptr;
    const bool units_match = value_10k && value_10k->is_number() &&
        value_yuan && value_yuan->is_number() &&
        std::abs(value_yuan->as_number() - value_10k->as_number() * 10000.0) < 0.01;
    add_assertion(result, "holding_semantics",
                  first && bool_is(member(*first, "name_resolved"), true) &&
                      manager && manager->is_string() && !manager->as_string().empty() &&
                      report_date && report_date->is_string() &&
                      report_date->as_string().size() == 8 && units_match &&
                      member(*first, "raw") && member(*first, "raw")->is_object(),
                  "resolved security, manager, YYYYMMDD, raw row and 10k-yuan conversion",
                  value_or_null(first));
    const auto* first_value = first_data
        ? member(*first_data, "holding_market_value") : nullptr;
    const auto* second_value = second_data
        ? member(*second_data, "holding_market_value") : nullptr;
    add_assertion(result, "descending_holding_value",
                  first_value && first_value->is_number() &&
                      second_value && second_value->is_number() &&
                      first_value->as_number() >= second_value->as_number(),
                  true, first_value && second_value ?
                      Json(first_value->as_number() >= second_value->as_number()) : Json(nullptr));
    const bool exact_source = source_exists(document, "list/func_jgcg108_1.jsn");
    add_assertion(result, "exact_source", exact_source,
                  "list/func_jgcg108_1.jsn", exact_source);
    const auto* sources = member(document, "sources");
    const Json* source = sources && sources->is_array() && sources->size() == 1
        ? &sources->as_array().front() : nullptr;
    add_assertion(result, "source_health",
                  source && member(*source, "endpoint") &&
                      member(*source, "endpoint")->is_string() &&
                      member(*source, "attempts") && member(*source, "attempts")->is_number() &&
                      member(*source, "attempts")->as_number() >= 1 &&
                      member(*source, "stale") && member(*source, "stale")->is_bool(),
                  true, source ? *source : Json(nullptr));
}

struct InstitutionContract {
    std::string_view id;
    InstitutionContractValidator validate;
};

constexpr std::array<InstitutionContract, 9> institution_contracts{{
    {"stock-panorama-live", validate_stock_panorama},
    {"stock-institution-analysis-live", validate_stock_institution_analysis},
    {"generic-tqlex-live", validate_generic_tqlex},
    {"generic-pbrpc-live", validate_generic_tqlex},
    {"holder-cross-stock-live", validate_holder_cross_stock},
    {"institution-exclusive-funds-live", validate_institution_exclusive_funds},
    {"institution-national-team-live", validate_institution_exclusive_funds},
    {"institution-stake-building-live", validate_institution_stake_building},
    {"institution-notable-private-funds-live", validate_institution_notable_private_funds},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < institution_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < institution_contracts.size(); ++right)
            if (institution_contracts[left].id == institution_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_market_data_institution_contract(const std::string& contract_id,
    const Json& document, const Json& context, Json& result) {
    (void)context;
    for (const auto& contract : institution_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(contract_id, document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail