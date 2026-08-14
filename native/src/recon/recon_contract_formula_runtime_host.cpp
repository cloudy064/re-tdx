#include "recon_contract_formula_runtime_internal.hpp"

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

using HostContractValidator = void (*)(const Json& document, Json& result);

void validate_formula_host_summary_inline_post(const Json& document, Json& result) {
    const bool identity =
        string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
        string_is(member(document, "execution_mode"), "native-cpp") &&
        string_is(member(document, "formula_source_mode"), "inline-post") &&
        string_is(member(document, "formula"), "CONTRACT_HOST_SUMMARY") &&
        string_is(member(document, "market"), "sz") &&
        string_is(member(document, "code"), "000001");
    add_assertion(result, "native_formula_identity", identity,
                  "inline-post native-cpp CONTRACT_HOST_SUMMARY for sz000001",
                  identity);

    const auto* analysis = member(document, "analysis");
    std::set<std::string> dependencies;
    std::set<std::string> unavailable;
    if (analysis) {
        if (const auto* values = member(*analysis, "automatic_context_dependencies");
            values && values->is_array())
            for (const auto& value : values->as_array())
                if (value.is_string()) dependencies.insert(value.as_string());
        if (const auto* values = member(*analysis, "context_bindings_unavailable");
            values && values->is_array())
            for (const auto& value : values->as_array())
                if (value.is_string()) unavailable.insert(value.as_string());
    }
    const bool contextual = analysis &&
        bool_is(member(*analysis, "syntax_supported"), true) &&
        bool_is(member(*analysis, "context_bindable"), true) &&
        bool_is(member(*analysis, "executable_with_context"), true) &&
        dependencies == std::set<std::string>{
            "BETAVALUE", "HYSJL", "HYSYL", "MAINZSHQ", "SHAPE_LONG", "SHAPE_MID",
            "SHAPE_SHORT", "TOTALHQINFO", "TOTALMMPAMO"} && unavailable.empty();
    add_assertion(result, "host_summary_analysis", contextual,
                  "nine exact automatic host dependencies with no unavailable binding",
                  contextual);

    const auto* metadata = member(document, "context_metadata");
    const auto* stats = metadata ? member(*metadata, "security_stat_functions") : nullptr;
    const auto* summary = metadata
        ? member(*metadata, "public_market_summary_functions") : nullptr;
    const auto* valuation = metadata
        ? member(*metadata, "industry_valuation") : nullptr;
    const auto* valuation_bindings = valuation
        ? member(*valuation, "bindings") : nullptr;
    const auto* pe_binding = valuation_bindings
        ? member(*valuation_bindings, "HYSYL") : nullptr;
    const auto* pb_binding = valuation_bindings
        ? member(*valuation_bindings, "HYSJL") : nullptr;
    const auto packed = stats ? numeric_value(member(*stats, "shape_packed"))
                              : std::nullopt;
    const bool provenance = stats && summary && valuation &&
        pe_binding && pb_binding && packed &&
        bool_is(member(*stats, "security_found"), true) &&
        string_is(member(*stats, "mode"),
                  "TCalc-opcodes1333-1345-1347-TdxW-type163-tdxstat-columns3-23") &&
        string_is(member(*summary, "command"), "0x0547") &&
        string_is(member(*summary, "mainzshq_mode"),
                  "TCalc-opcode1368-TdxW-type102-index-L1-broadcast") &&
        string_is(member(*summary, "totalhqinfo_mode"),
                  "TCalc-opcode1384-SH880005-public-L1-broadcast") &&
        string_is(member(*summary, "totalmmpamo_mode"),
                  "TCalc-opcode1376-TdxW-type168-SH999997-public-L1-selectors1-4") &&
        string_is(member(*summary, "totalmmpamo_unit"), "100m-yuan") &&
        number_is(member(*summary, "received"), 7.0) &&
        string_is(member(*valuation, "schema"),
                  "tdx-formula-industry-valuation-v1") &&
        string_is(member(*valuation, "selected_code"), "880471") &&
        (string_is(member(*valuation, "availability"), "public-hyzt-record") ||
         string_is(member(*valuation, "availability"),
                   "public-hyzt-normal-industry-fallback")) &&
        string_is(member(*valuation, "source_resource"),
                  "list/func_gx_hyzt101_1.jsn") &&
        bool_is(member(*pe_binding, "available"), true) &&
        string_is(member(*pe_binding, "source_field"), "hyPE") &&
        number_is(member(*pe_binding, "tcalc_opcode"), 1328.0) &&
        number_is(member(*pe_binding, "tdxw_callback_type"), 120.0) &&
        number_is(member(*pe_binding, "tdxw_return_offset"), 60.0) &&
        bool_is(member(*pb_binding, "available"), true) &&
        string_is(member(*pb_binding, "source_field"), "hyPB") &&
        number_is(member(*pb_binding, "tcalc_opcode"), 1344.0) &&
        number_is(member(*pb_binding, "tdxw_callback_type"), 163.0) &&
        number_is(member(*pb_binding, "tdxw_return_offset"), 380.0);
    add_assertion(result, "host_summary_provenance", provenance,
                  "local tdxstat, public HYZT industry valuation and 0x0547 market summaries",
                  provenance);

    bool values_match = false;
    if (packed) {
        const auto packed_value = static_cast<long long>(std::llround(*packed));
        const double expected_short = static_cast<double>(packed_value / 10000);
        const double expected_mid = static_cast<double>((packed_value / 100) % 100);
        const double expected_long = static_cast<double>(packed_value % 100);
        if (const auto* points = member(document, "points");
            points && points->is_array() && points->size() == 120) {
            static const std::array<const char*, 16> numeric_names{
                "BETA", "S1", "S2", "S3",
                "M0", "M1", "M2", "M3", "M4", "M5", "M6", "M7",
                "M8", "M9", "T1", "T2"};
            static const std::array<const char*, 10> remaining_names{
                "T3", "T4", "T5", "T6", "A1", "A2", "A3", "A4",
                "PE", "PB"};
            const auto* first_values = member(points->as_array().front(), "values");
            values_match = first_values &&
                number_is(member(*first_values, "S1"), expected_short) &&
                number_is(member(*first_values, "S2"), expected_mid) &&
                number_is(member(*first_values, "S3"), expected_long) &&
                numeric_value(member(*first_values, "PE")).value_or(0.0) > 0.0 &&
                numeric_value(member(*first_values, "PB")).value_or(0.0) > 0.0 &&
                numeric_value(member(*first_values, "M0")).value_or(0.0) > 0.0 &&
                numeric_value(member(*first_values, "M1")).value_or(0.0) > 0.0;
            for (const auto& point : points->as_array()) {
                const auto* values = member(point, "values");
                if (!values || !first_values) {
                    values_match = false;
                    break;
                }
                for (const auto* name : numeric_names) {
                    const auto value = numeric_value(member(*values, name));
                    const auto expected = numeric_value(member(*first_values, name));
                    values_match = values_match && value && expected &&
                        std::isfinite(*value) && *value == *expected;
                }
                for (const auto* name : remaining_names) {
                    const auto value = numeric_value(member(*values, name));
                    const auto expected = numeric_value(member(*first_values, name));
                    values_match = values_match && value && expected &&
                        std::isfinite(*value) && *value == *expected;
                }
            }
        }
    }
    add_assertion(result, "native_host_summary_values", values_match,
                  "120 bars broadcast beta, shapes, HYSYL/HYSJL, MAINZSHQ, TOTALHQINFO and TOTALMMPAMO",
                  values_match);
}

void validate_formula_security_status_inline_post(const Json& document, Json& result) {
    const bool identity =
        string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
        string_is(member(document, "execution_mode"), "native-cpp") &&
        string_is(member(document, "formula_source_mode"), "inline-post") &&
        string_is(member(document, "formula"), "CONTRACT_SECURITY_STATUS") &&
        string_is(member(document, "market"), "sz") &&
        string_is(member(document, "code"), "000001");
    add_assertion(result, "native_formula_identity", identity,
                  "inline-post native-cpp CONTRACT_SECURITY_STATUS for sz000001",
                  identity);

    std::set<std::string> dependencies;
    const auto* analysis = member(document, "analysis");
    if (analysis) {
        if (const auto* values = member(*analysis, "automatic_context_dependencies");
            values && values->is_array())
            for (const auto& value : values->as_array())
                if (value.is_string()) dependencies.insert(value.as_string());
    }
    const bool contextual = analysis &&
        bool_is(member(*analysis, "syntax_supported"), true) &&
        bool_is(member(*analysis, "context_bindable"), true) &&
        bool_is(member(*analysis, "executable_with_context"), true) &&
        dependencies == std::set<std::string>{
            "IST0CODE", "ISSTCODE", "ISQUITCODE", "ISQHQQCODE",
            "ISJYDATE", "LOCALDAYNUM"};
    add_assertion(result, "security_status_analysis", contextual,
                  "six exact automatic TdxW host-status/calendar dependencies", contextual);

    const auto* metadata = member(document, "context_metadata");
    const auto* host_values = metadata ? member(*metadata, "host_calendar_values") : nullptr;
    const auto current_trading_date = metadata
        ? numeric_value(member(*metadata, "host_calendar_current_trading_date"))
        : std::nullopt;
    const auto machine_date = metadata
        ? numeric_value(member(*metadata, "host_calendar_machine_date"))
        : std::nullopt;
    const auto local_day_count = metadata
        ? numeric_value(member(*metadata, "host_calendar_local_day_effective_count"))
        : std::nullopt;
    const std::optional<double> expected_isjydate =
        current_trading_date && machine_date
            ? std::optional<double>(*current_trading_date == *machine_date ? 1.0 : 0.0)
            : std::nullopt;
    bool values_match = false;
    if (const auto* points = member(document, "points");
        points && points->is_array() && points->size() == 120 &&
        expected_isjydate && local_day_count) {
        values_match = true;
        for (const auto& point : points->as_array()) {
            const auto* values = member(point, "values");
            values_match = values_match && values &&
                number_is(member(*values, "T0"), 0.0) &&
                number_is(member(*values, "ST"), 0.0) &&
                number_is(member(*values, "QUIT"), 0.0) &&
                number_is(member(*values, "QHQQ"), 0.0) &&
                number_is(member(*values, "JY"), *expected_isjydate) &&
                number_is(member(*values, "LD"), *local_day_count);
        }
    }
    add_assertion(result, "native_security_status_values", values_match,
                  "sz000001 broadcasts status plus type-122/168 host-calendar values", values_match);

    bool provenance = false;
    if (metadata && metadata->is_object()) {
        const auto* values = member(*metadata, "security_status_values");
        const auto* spblock = member(*metadata, "security_status_spblock_source");
        const auto* quit = member(*metadata, "security_status_quit_source");
        provenance =
            string_is(member(*metadata, "security_status_mode"),
                      "tdxw-types167-120-105-exact-host-field-reconstruction") &&
            string_is(member(*metadata, "security_status_category_source"),
                      "local-tnf-offset-282") &&
            number_is(member(*metadata, "security_status_category"), 1.0) &&
            values && values->is_object() &&
            number_is(member(*values, "IST0CODE"), 0.0) &&
            number_is(member(*values, "ISSTCODE"), 0.0) &&
            number_is(member(*values, "ISQUITCODE"), 0.0) &&
            number_is(member(*values, "ISQHQQCODE"), 0.0) &&
            spblock && spblock->is_string() && !spblock->as_string().empty() &&
            quit && quit->is_string() && !quit->as_string().empty();
    }
    add_assertion(result, "security_status_provenance", provenance,
                  "spblock + infoharbor_spec + TNF category provenance", provenance);

    bool host_calendar_provenance = false;
    if (metadata && metadata->is_object() && host_values &&
        host_values->is_object() && expected_isjydate && local_day_count) {
        const auto record_count = numeric_value(
            member(*metadata, "host_calendar_local_day_record_count"));
        const bool synthetic = bool_is(
            member(*metadata, "host_calendar_local_day_synthetic_current"), true);
        const bool not_synthetic = bool_is(
            member(*metadata, "host_calendar_local_day_synthetic_current"), false);
        const auto* local_file = member(*metadata, "host_calendar_local_day_file");
        host_calendar_provenance =
            string_is(member(*metadata, "host_calendar_mode"),
                      "tdxw-types122-168-exact-host-field-reconstruction") &&
            string_is(member(*metadata, "host_calendar_current_trading_date_source"),
                      "latest-request-kline-date-proxy-for-tdxw-global-trading-date") &&
            bool_is(member(*metadata, "host_calendar_local_day_file_exists"), true) &&
            local_file && local_file->is_string() && !local_file->as_string().empty() &&
            record_count && (synthetic || not_synthetic) &&
            *local_day_count == *record_count + (synthetic ? 1.0 : 0.0) &&
            number_is(member(*host_values, "ISJYDATE"), *expected_isjydate) &&
            number_is(member(*host_values, "LOCALDAYNUM"), *local_day_count);
    }
    add_assertion(result, "host_calendar_provenance", host_calendar_provenance,
                  "type 122 current date plus type 168 local .day record provenance",
                  host_calendar_provenance);
}

struct HostContract {
    std::string_view id;
    HostContractValidator validate;
};

constexpr std::array<HostContract, 2> host_contracts{{
    {"formula-host-summary-inline-post", validate_formula_host_summary_inline_post},
    {"formula-security-status-inline-post", validate_formula_security_status_inline_post},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < host_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < host_contracts.size(); ++right)
            if (host_contracts[left].id == host_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_formula_runtime_host_contract(const std::string& contract_id,
    const Json& document, Json& result) {
    for (const auto& contract : host_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail