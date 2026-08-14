#include "recon_contract_internal.hpp"
#include "recon_contract_formula_internal.hpp"
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

using CalculationContractValidator = void (*)(const std::string& contract_id, const Json& document, Json& result);

void validate_formula_cloud_calc_audit(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-tbigdata-cloud-calc-audit-v1"),
                  "tdx-tbigdata-cloud-calc-audit-v1",
                  value_or_null(member(document, "schema")));
    const auto* summary = member(document, "summary");
    const bool complete = summary && summary->is_object() &&
        number_is(member(*summary, "cfg_files"), 660) &&
        number_is(member(*summary, "calc_columns"), 1370) &&
        number_is(member(*summary, "current_config_executable"), 1370) &&
        number_is(member(*summary, "current_config_unimplemented"), 0) &&
        number_is(member(*summary, "registered_builtin_implemented"), 36) &&
        number_is(member(*summary, "parse_errors"), 0) &&
        number_is(member(*summary, "dependency_cycles"), 0) &&
        number_is(member(*summary, "unresolved_host_columns"), 0);
    add_assertion(result, "complete_native_coverage", complete,
                  "660 CFG / 1370 calc / 36 builtins / 0 gaps", complete);
    const bool native_no_dll = string_is(member(document, "execution_mode"),
                                          "native-cpp-offline") &&
                               bool_is(member(document, "dll_loaded"), false);
    add_assertion(result, "native_no_dll", native_no_dll,
                  "native-cpp-offline, dll_loaded=false", native_no_dll);
}

void validate_formula_cloud_calc_template(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-tbigdata-cloud-calc-template-v1"),
                  "tdx-tbigdata-cloud-calc-template-v1",
                  value_or_null(member(document, "schema")));
    const auto* counts = member(document, "counts");
    const bool exact_counts = counts && counts->is_object() &&
        number_is(member(*counts, "input_fields"), 19) &&
        number_is(member(*counts, "host_fields"), 3) &&
        number_is(member(*counts, "derived_fields"), 1) &&
        number_is(member(*counts, "calculated_fields"), 15) &&
        number_is(member(*counts, "units"), 1);
    add_assertion(result, "minimal_partition", exact_counts,
                  "19 input / 3 host / 1 derived / 15 calculated", exact_counts);
    const auto* row_template = member(document, "row_template");
    const bool skeleton = row_template && row_template->is_object() &&
        row_template->size() == 19 && object_has(row_template, "$SC") &&
        object_has(row_template, "$ZQDM") && object_has(row_template, "$SC1") &&
        object_has(row_template, "$ZQDM1") && object_has(row_template, "MZ") &&
        object_has(row_template, "SYFXLLXL") && !object_has(row_template, "QJ") &&
        !object_has(row_template, "$NOW3");
    add_assertion(result, "minimal_row_skeleton", skeleton,
                  "identity/raw inputs present; calculated/host fields absent", skeleton);
    const bool sanitized = string_is(member(document, "execution_mode"),
                                      "native-cpp-offline") &&
        bool_is(member(document, "dll_loaded"), false) &&
        string_is(member(document, "cfg_name"), "func_kzz_kzzsy101.cfg") &&
        !object_has(&document, "path") && !object_has(&document, "cfg");
    add_assertion(result, "native_sanitized_template", sanitized, true, sanitized);
}

void validate_formula_cloud_calc_live_post(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-tbigdata-cloud-calc-evaluation-v1"),
                  "tdx-tbigdata-cloud-calc-evaluation-v1",
                  value_or_null(member(document, "schema")));
    const auto* counts = member(document, "counts");
    const bool all_evaluated_live = counts && counts->is_object() &&
        number_is(member(*counts, "calculated"), 15) &&
        number_is(member(*counts, "evaluated"), 15) &&
        number_is(member(*counts, "unavailable"), 0) &&
        number_is(member(*counts, "errors"), 0);
    const bool all_evaluated_premarket = counts && counts->is_object() &&
        number_is(member(*counts, "calculated"), 15) &&
        number_is(member(*counts, "evaluated"), 10) &&
        number_is(member(*counts, "unavailable"), 5) &&
        number_is(member(*counts, "errors"), 0);
    const bool all_evaluated = all_evaluated_live || all_evaluated_premarket;
    add_assertion(result, "all_calculated", all_evaluated,
                  "15/15 live, or exact 10 evaluated + 5 unavailable during public-L1 premarket reset",
                  all_evaluated);
    const auto* host = member(document, "host_context");
    const auto* bindings = host ? member(*host, "bindings") : nullptr;
    const auto* unresolved = host ? member(*host, "unresolved") : nullptr;
    const bool public_context_live = host &&
        string_is(member(*host, "snapshot_source"), "public-l1-0x054c") &&
        string_is(member(*host, "finance_source"), "public-finance-0x0010") &&
        bindings && bindings->is_array() && bindings->size() >= 8 &&
        unresolved && unresolved->is_array() && unresolved->size() == 0;
    bool exact_premarket_unresolved = unresolved && unresolved->is_array() &&
        unresolved->size() == 4;
    std::set<std::string> premarket_codes;
    if (exact_premarket_unresolved) {
        for (const auto& entry : unresolved->as_array()) {
            exact_premarket_unresolved = exact_premarket_unresolved &&
                string_is(member(entry, "reason"),
                          "requested L1 field is unavailable");
            const auto* code = member(entry, "code");
            if (code && code->is_string()) premarket_codes.insert(code->as_string());
        }
        exact_premarket_unresolved = exact_premarket_unresolved &&
            premarket_codes == std::set<std::string>{"$NOW3", "$ZAF", "ZGXJ", "ZGZF"};
    }
    const bool public_context_premarket = host && all_evaluated_premarket &&
        string_is(member(*host, "snapshot_source"), "public-l1-0x054c") &&
        string_is(member(*host, "finance_source"), "public-finance-0x0010") &&
        bindings && bindings->is_array() && bindings->size() >= 5 &&
        exact_premarket_unresolved;
    const bool public_context = public_context_live || public_context_premarket;
    add_assertion(result, "public_host_context", public_context,
                  "public L1/finance live context, or exact four-field premarket reset",
                  public_context);
    const bool sanitized = string_is(member(document, "row_source"), "inline-request") &&
        string_is(member(document, "execution_mode"),
                  "native-cpp-public-l1-finance") &&
        bool_is(member(document, "dll_loaded"), false) &&
        !object_has(&document, "row") && !object_has(&document, "request_body");
    add_assertion(result, "native_sanitized_request", sanitized, true, sanitized);
}

void validate_formula_cloud_calc_batch_post(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-tbigdata-cloud-calc-batch-v1"),
                  "tdx-tbigdata-cloud-calc-batch-v1",
                  value_or_null(member(document, "schema")));
    const auto* counts = member(document, "counts");
    const bool all_evaluated_live = counts && counts->is_object() &&
        number_is(member(*counts, "rows"), 2) &&
        number_is(member(*counts, "succeeded"), 2) &&
        number_is(member(*counts, "failed"), 0) &&
        number_is(member(*counts, "calculated"), 30) &&
        number_is(member(*counts, "evaluated"), 30) &&
        number_is(member(*counts, "unavailable"), 0) &&
        number_is(member(*counts, "errors"), 0);
    const bool all_evaluated_premarket = counts && counts->is_object() &&
        number_is(member(*counts, "rows"), 2) &&
        number_is(member(*counts, "succeeded"), 2) &&
        number_is(member(*counts, "failed"), 0) &&
        number_is(member(*counts, "calculated"), 30) &&
        number_is(member(*counts, "evaluated"), 20) &&
        number_is(member(*counts, "unavailable"), 10) &&
        number_is(member(*counts, "errors"), 0);
    const bool all_evaluated = all_evaluated_live || all_evaluated_premarket;
    add_assertion(result, "all_rows_calculated", all_evaluated,
                  "2/2 rows; 30/30 live, or exact 20 evaluated + 10 unavailable during premarket reset",
                  all_evaluated);
    const auto* fetch_plan = member(document, "fetch_plan");
    const bool shared_fetch = fetch_plan && fetch_plan->is_object() &&
        string_is(member(*fetch_plan, "quote_mode"), "snapshot-0x054c") &&
        number_is(member(*fetch_plan, "unique_quote_securities"), 2) &&
        number_is(member(*fetch_plan, "quote_document_fetches"), 1);
    add_assertion(result, "shared_deduplicated_quote_plan", shared_fetch,
                  "2 unique securities / 1 shared quote document fetch", shared_fetch);
    bool rows_clean = false;
    if (const auto* rows = member(document, "rows");
        rows && rows->is_array() && rows->size() == 2) {
        rows_clean = true;
        for (std::size_t index = 0; index < rows->size(); ++index) {
            const auto& entry = rows->as_array()[index];
            const auto* evaluation = member(entry, "result");
            const auto* host = evaluation ? member(*evaluation, "host_context") : nullptr;
            const auto* unresolved = host ? member(*host, "unresolved") : nullptr;
            const auto* evaluation_counts = evaluation ? member(*evaluation, "counts") : nullptr;
            bool valid_unresolved = unresolved && unresolved->is_array() &&
                unresolved->size() == 0;
            if (!valid_unresolved && all_evaluated_premarket && unresolved &&
                unresolved->is_array() && unresolved->size() == 4) {
                std::set<std::string> codes;
                valid_unresolved = true;
                for (const auto& item : unresolved->as_array()) {
                    valid_unresolved = valid_unresolved &&
                        string_is(member(item, "reason"),
                                  "requested L1 field is unavailable");
                    const auto* code = member(item, "code");
                    if (code && code->is_string()) codes.insert(code->as_string());
                }
                valid_unresolved = valid_unresolved &&
                    codes == std::set<std::string>{"$NOW3", "$ZAF", "ZGXJ", "ZGZF"} &&
                    evaluation_counts &&
                    number_is(member(*evaluation_counts, "calculated"), 15) &&
                    number_is(member(*evaluation_counts, "evaluated"), 10) &&
                    number_is(member(*evaluation_counts, "unavailable"), 5) &&
                    number_is(member(*evaluation_counts, "errors"), 0);
            }
            rows_clean = rows_clean &&
                number_is(member(entry, "row_index"), static_cast<double>(index)) &&
                string_is(member(entry, "status"), "ok") && evaluation &&
                !object_has(evaluation, "row") && !object_has(evaluation, "request_body") &&
                host && valid_unresolved;
        }
    }
    const bool sanitized = rows_clean &&
        string_is(member(document, "row_source"), "inline-request-batch") &&
        string_is(member(document, "execution_mode"),
                  "native-cpp-public-l1-finance") &&
        bool_is(member(document, "request_body_retained"), false) &&
        bool_is(member(document, "dll_loaded"), false) &&
        !object_has(&document, "request_body");
    add_assertion(result, "native_sanitized_rows", sanitized, true, sanitized);
}

void validate_tpool_inline_evaluate_post(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-tpool-native-evaluation-v1"),
                  "tdx-tpool-native-evaluation-v1",
                  value_or_null(member(document, "schema")));
    const bool executed =
        number_is(member(document, "available_security_count"), 1) &&
        number_is(member(document, "evaluated_security_count"), 1) &&
        number_is(member(document, "evaluated_rule_count"), 1) &&
        number_is(member(document, "matched_rule_count"), 1) &&
        bool_is(member(document, "flow_graph_evaluated"), true);
    add_assertion(result, "rule_and_flow_evaluated", executed,
                  "1 security / 1 matched rule / flow projected", executed);
    const auto* inspection = member(document, "inspection");
    const auto* projection = member(document, "flow_projection");
    const auto* transitions = projection ? member(*projection, "transitions") : nullptr;
    const Json* planned_actions = nullptr;
    if (transitions && transitions->is_array() && transitions->size() == 1)
        planned_actions = member(transitions->as_array().front(), "planned_actions");
    const bool action_plan =
        inspection && number_is(member(*inspection, "action_policy_count"), 1) &&
        number_is(member(*inspection, "configured_action_count"), 3) &&
        projection && number_is(member(*projection, "planned_action_count"), 3) &&
        bool_is(member(*projection, "host_actions_executed"), false) &&
        planned_actions && planned_actions->is_array() && planned_actions->size() == 3 &&
        string_is(member(planned_actions->as_array()[0], "kind"),
                  "record-entry-log") &&
        string_is(member(planned_actions->as_array()[0], "path_template"),
                  "tpool/<pool>/<cell>/<YYYYMMDD>.log") &&
        string_is(member(planned_actions->as_array()[1], "kind"), "sound") &&
        string_is(member(planned_actions->as_array()[1], "effective_file"),
                  "sound\\default.wav") &&
        string_is(member(planned_actions->as_array()[2], "kind"), "save-block") &&
        number_is(member(planned_actions->as_array()[2], "security_record_size_bytes"), 7) &&
        number_is(member(planned_actions->as_array()[2], "security_buffer_bytes"), 7) &&
        number_is(member(planned_actions->as_array()[2], "callback_argument_count"), 7) &&
        number_is(member(planned_actions->as_array()[2], "callback_operation_id"), 88) &&
        bool_is(member(planned_actions->as_array()[2], "clear_before_save"), true) &&
        bool_is(member(planned_actions->as_array()[0], "executed"), false) &&
        bool_is(member(planned_actions->as_array()[1], "executed"), false) &&
        bool_is(member(planned_actions->as_array()[2], "executed"), false);
    add_assertion(result, "psatt_planned_only", action_plan,
                  "entry-log, default-sound and 7-byte block callback planned / 0 host actions executed", action_plan);
    const bool sanitized =
        string_is(member(document, "source"), "contract-inline.xml") &&
        bool_is(member(document, "inline_source"), true) &&
        bool_is(member(document, "read_only"), true) &&
        bool_is(member(document, "tdx_state_mutated"), false) &&
        bool_is(member(document, "request_body_retained"), false) &&
        inspection && string_is(member(*inspection, "source"),
                                 "contract-inline.xml") &&
        bool_is(member(*inspection, "inline_source"), true) &&
        !object_has(&document, "xml") && !object_has(&document, "request_body") &&
        !object_has(inspection, "xml");
    add_assertion(result, "inline_read_only_sanitized", sanitized, true, sanitized);
}

void validate_formula_cross_market_audit_hk(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "engine",
                  string_is(member(document, "engine"),
                            "tdx-source-interpreter-v1"),
                  "tdx-source-interpreter-v1",
                  value_or_null(member(document, "engine")));
    const auto* audit = member(document, "audit");
    const bool summary = audit && audit->is_object() &&
        member(*audit, "passed") && member(*audit, "passed")->is_number() &&
        member(*audit, "passed")->as_number() > 0 &&
        number_is(member(*audit, "errors"), 0) &&
        number_is(member(*audit, "unreported"), 0);
    add_assertion(result, "clean_complete_audit", summary,
                  "passed>0, errors=0, unreported=0", summary);
    const bool identity = string_is(member(document, "market"), "31") &&
        string_is(member(document, "code"), "00700") &&
        string_is(member(document, "market_scope"), "tdx-expansion");
    add_assertion(result, "expansion_identity", identity,
                  "31:00700 / tdx-expansion", identity);
    bool shortvol_passed = false;
    if (const auto* formulas = member(document, "formulas");
        formulas && formulas->is_array())
        for (const auto& formula : formulas->as_array())
            if (string_is(member(formula, "code"), "SHORTVOL") &&
                string_is(member(formula, "status"), "passed")) {
                shortvol_passed = true;
                break;
            }
    add_assertion(result, "shortvol_passed", shortvol_passed,
                  true, shortvol_passed);
}

void validate_formula_contract_multiplier(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "engine",
                  string_is(member(document, "engine"),
                            "tdx-source-interpreter-v1"),
                  "tdx-source-interpreter-v1",
                  value_or_null(member(document, "engine")));
    const bool identity =
        string_is(member(document, "market"), "47") &&
        string_is(member(document, "code"), "IFL9") &&
        string_is(member(document, "formula"), "CONTRACT_MULTIPLIER") &&
        bool_is(member(document, "expansion_market"), true);
    add_assertion(result, "expansion_identity", identity,
                  "47:IFL9 / CONTRACT_MULTIPLIER", identity);
    bool numeric_latest = false;
    if (const auto* points = member(document, "points");
        points && points->is_array() && points->size()) {
        const auto* values = member(points->as_array().back(), "values");
        const auto* value = values ? member(*values, "合约乘数") : nullptr;
        numeric_latest = number_is(value, 300.0);
    }
    add_assertion(result, "numeric_multiplier", numeric_latest,
                  300.0, numeric_latest);
    const auto* metadata = member(document, "context_metadata");
    const bool provenance = metadata && metadata->is_object() &&
        number_is(member(*metadata, "contract_multiplier"), 300.0) &&
        number_is(member(*metadata, "contract_multiplier_raw"), 300.0) &&
        number_is(member(*metadata, "contract_multiplier_category"), 3.0) &&
        string_is(member(*metadata, "contract_multiplier_mode"),
                  "tcalc-opcode1252-type105-offset42-signed-int16-broadcast") &&
        string_is(member(*metadata, "contract_multiplier_source"),
                  "tdx-7727-0x23f5-offset56-u32-low-word");
    add_assertion(result, "multiplier_provenance", provenance,
                  true, provenance);
}

void validate_formula_kline_auxiliary_fields(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "engine",
                  string_is(member(document, "engine"),
                            "tdx-source-interpreter-v1"),
                  "tdx-source-interpreter-v1",
                  value_or_null(member(document, "engine")));
    const bool identity =
        string_is(member(document, "market"), "47") &&
        string_is(member(document, "code"), "IFL9") &&
        string_is(member(document, "formula"), "CONTRACT_KLINE_AUXILIARY") &&
        bool_is(member(document, "expansion_market"), true) &&
        string_is(member(document, "auxiliary_field"), "settlement_price");
    add_assertion(result, "expansion_identity", identity,
                  "47:IFL9 / settlement_price", identity);
    bool numeric_aliases = false;
    if (const auto* points = member(document, "points");
        points && points->is_array() && points->size() >= 2) {
        bool completed_aliases = true;
        for (std::size_t index = 0; index + 1 < points->size(); ++index) {
            const auto* values = member(points->as_array()[index], "values");
            const auto* average = values ? member(*values, "分时均价") : nullptr;
            const auto* settlement = values ? member(*values, "结算价") : nullptr;
            completed_aliases = completed_aliases && average && settlement &&
                average->is_number() && settlement->is_number() &&
                std::isfinite(average->as_number()) && average->as_number() > 0.0 &&
                average->as_number() == settlement->as_number();
        }
        const auto& latest = points->as_array().back();
        const auto* values = member(latest, "values");
        const auto* average = values ? member(*values, "分时均价") : nullptr;
        const auto* settlement = values ? member(*values, "结算价") : nullptr;
        const bool latest_positive = average && settlement && average->is_number() &&
            settlement->is_number() && std::isfinite(average->as_number()) &&
            average->as_number() > 0.0 &&
            average->as_number() == settlement->as_number();
        const bool current_day_placeholder =
            string_is(member(latest, "date"), current_local_date()) &&
            number_is(average, 0.0) && number_is(settlement, 0.0);
        numeric_aliases = completed_aliases &&
            (latest_positive || current_day_placeholder);
    }
    add_assertion(result, "shared_auxiliary_float", numeric_aliases,
                  "completed bars ZSTJJ == QHJSJ > 0; current-day pre-settlement placeholder may be 0/0",
                  numeric_aliases);
}

void validate_formula_cross_market_futures(const std::string& contract_id, const Json& document,
        Json& result) {
    const bool option = contract_id == "formula-cross-market-option-live";
    add_assertion(result, "engine",
                  string_is(member(document, "engine"),
                            "tdx-source-interpreter-v1"),
                  "tdx-source-interpreter-v1",
                  value_or_null(member(document, "engine")));
    const bool identity =
        string_is(member(document, "market"), option ? "7" : "47") &&
        string_is(member(document, "formula"), option ? "VOLATILITY" : "CCL") &&
        bool_is(member(document, "expansion_market"), true);
    add_assertion(result, "expansion_identity", identity,
                  option ? "7/VOLATILITY" : "47/CCL", identity);
    bool numeric_latest = false;
    if (const auto* points = member(document, "points");
        points && points->is_array() && points->size()) {
        const auto* values = member(points->as_array().back(), "values");
        const auto* value = values
            ? member(*values, option ? "隐含波动率" : "持仓量") : nullptr;
        numeric_latest = value && value->is_number() &&
                         std::isfinite(value->as_number());
    }
    add_assertion(result, "numeric_latest", numeric_latest,
                  option ? "numeric 隐含波动率" : "numeric 持仓量",
                  numeric_latest);
    if (!option) {
        bool center_half_stick = false;
        const auto* ir = member(document, "render_ir");
        const auto* primitives = ir ? member(*ir, "primitives") : nullptr;
        if (primitives && primitives->is_array()) {
            for (const auto& primitive : primitives->as_array()) {
                if (!string_is(member(primitive, "function"), "STICKLINE")) continue;
                const auto* events = member(primitive, "events");
                if (!events || !events->is_array()) continue;
                for (const auto& event : events->as_array())
                    if (string_is(member(event, "stick_mode"), "center-half") &&
                        number_is(member(event, "stick_width"), 2.0) &&
                        number_is(member(event, "stick_width_ratio"), 0.5) &&
                        bool_is(member(event, "stick_price1_used"), false) &&
                        string_is(member(event, "stick_anchor"), "pane-middle") &&
                        string_is(member(event, "stick_occupancy"), "half"))
                        center_half_stick = true;
            }
        }
        add_assertion(result, "center_half_stickline", center_half_stick,
                      true, center_half_stick);
    }
}

struct CalculationContract {
    std::string_view id;
    CalculationContractValidator validate;
};

constexpr std::array<CalculationContract, 10> calculation_contracts{{
    {"formula-cloud-calc-audit", validate_formula_cloud_calc_audit},
    {"formula-cloud-calc-template", validate_formula_cloud_calc_template},
    {"formula-cloud-calc-live-post", validate_formula_cloud_calc_live_post},
    {"formula-cloud-calc-batch-post", validate_formula_cloud_calc_batch_post},
    {"tpool-inline-evaluate-post", validate_tpool_inline_evaluate_post},
    {"formula-cross-market-audit-hk-live", validate_formula_cross_market_audit_hk},
    {"formula-contract-multiplier-live", validate_formula_contract_multiplier},
    {"formula-kline-auxiliary-fields-live", validate_formula_kline_auxiliary_fields},
    {"formula-cross-market-futures-live", validate_formula_cross_market_futures},
    {"formula-cross-market-option-live", validate_formula_cross_market_futures},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < calculation_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < calculation_contracts.size(); ++right)
            if (calculation_contracts[left].id == calculation_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_formula_calculation_contract(const std::string& contract_id,
                                           const Json& document,
                                           Json& result) {
    for (const auto& contract : calculation_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(contract_id, document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail
