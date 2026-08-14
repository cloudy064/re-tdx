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

using ContextContractValidator = void (*)(const std::string& contract_id, const Json& document, Json& result);

void validate_formula_string_builders_inline_post(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    const bool identity =
        string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
        string_is(member(document, "execution_mode"), "native-cpp") &&
        string_is(member(document, "formula_source_mode"), "inline-post") &&
        string_is(member(document, "formula"), "CONTRACT_STRING_BUILDERS");
    add_assertion(result, "native_formula_identity", identity,
                  "inline-post native-cpp CONTRACT_STRING_BUILDERS", identity);
    const auto* analysis = member(document, "analysis");
    const auto* unsupported = analysis ? member(*analysis, "unsupported") : nullptr;
    const bool pure = analysis &&
        bool_is(member(*analysis, "syntax_supported"), true) &&
        bool_is(member(*analysis, "executable"), true) &&
        bool_is(member(*analysis, "numeric_signal_safe"), true) &&
        bool_is(member(*analysis, "string_semantics_faithful"), true) &&
        bool_is(member(*analysis, "has_external_dependency"), false) &&
        bool_is(member(*analysis, "has_future_function"), false) &&
        unsupported && unsupported->is_array() && unsupported->size() == 0;
    add_assertion(result, "string_builder_analysis", pure,
                  "pure exact materialized string-builder semantics", pure);

    bool numeric = false;
    if (const auto* points = member(document, "points");
        points && points->is_array() && points->size() == 120) {
        numeric = true;
        for (const auto& point : points->as_array()) {
            const auto& values = point.at("values");
            const auto parsed = numeric_value(member(values, "N"));
            numeric = numeric &&
                number_is(member(values, "L"), 6.0) &&
                number_is(member(values, "S0"), 1.0) &&
                number_is(member(values, "S1"), 1.0) &&
                number_is(member(values, "S2"), 1.0) &&
                number_is(member(values, "S3"), 1.0) &&
                number_is(member(values, "S4"), 1.0) &&
                number_is(member(values, "S5"), 1.0) && parsed &&
                std::abs(*parsed - 2365.02) < 0.001;
        }
    }
    add_assertion(result, "native_gbk_string_operations", numeric,
                  "STRLEN=GBK bytes; SUBSTR/STRSPACE/STRCAT6/VARCAT exact",
                  numeric);

    bool variable_text = false, constant_text = false;
    if (const auto* render = member(document, "render_ir");
        render && render->is_object()) {
        if (const auto* primitives = member(*render, "primitives");
            primitives && primitives->is_array()) {
            for (const auto& primitive : primitives->as_array()) {
                if (!string_is(member(primitive, "function"), "DRAWTEXT"))
                    continue;
                const auto* events = member(primitive, "events");
                if (!events || !events->is_array() || events->size() != 120)
                    continue;
                const auto* first = member(events->as_array().front(),
                                           "annotation_text");
                const auto* last = member(events->as_array().back(),
                                          "annotation_text");
                variable_text = variable_text ||
                    (string_is(first, "1|120|达 平安银行") &&
                     string_is(last, "120|120|达 平安银行"));
                constant_text = constant_text ||
                    (string_is(first, "120|120|定 值") &&
                     string_is(last, "120|120|定 值"));
            }
        }
    }
    add_assertion(result, "native_variable_string_series", variable_text,
                  "VAR2STR/VARCAT6 retain per-bar values", variable_text);
    add_assertion(result, "native_constant_string_broadcast", constant_text,
                  "CON2STR/STRCAT6 use last bar then broadcast", constant_text);
}

void validate_formula_inline_post(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "engine",
                  string_is(member(document, "engine"),
                            "tdx-source-interpreter-v1"),
                  "tdx-source-interpreter-v1",
                  value_or_null(member(document, "engine")));
    add_assertion(result, "native_execution",
                  string_is(member(document, "execution_mode"), "native-cpp"),
                  "native-cpp", value_or_null(member(document, "execution_mode")));
    add_assertion(result, "inline_source_mode",
                  string_is(member(document, "formula_source_mode"), "inline-post"),
                  "inline-post", value_or_null(member(document, "formula_source_mode")));
    add_assertion(result, "request_not_retained",
                  bool_is(member(document, "request_body_retained"), false), false,
                  value_or_null(member(document, "request_body_retained")));
    const auto* outputs = member(document, "outputs");
    const bool exact_outputs = outputs && outputs->is_array() && outputs->size() == 3 &&
        outputs->as_array()[0].is_string() &&
        outputs->as_array()[0].as_string() == "FAST" &&
        outputs->as_array()[1].is_string() &&
        outputs->as_array()[1].as_string() == "SIGNAL" &&
        outputs->as_array()[2].is_string() &&
        outputs->as_array()[2].as_string() == "HIST";
    add_assertion(result, "custom_outputs", exact_outputs,
                  "FAST,SIGNAL,HIST", value_or_null(outputs));
    const auto* points = member(document, "points");
    add_assertion(result, "market_points",
                  points && points->is_array() && points->size() == 120 &&
                      number_is(member(document, "count"), 120),
                  120, points ? Json(static_cast<std::uint64_t>(points->size()))
                              : Json(nullptr));
    const auto* analysis = member(document, "analysis");
    add_assertion(result, "source_analysis",
                  analysis && bool_is(member(*analysis, "syntax_supported"), true) &&
                      bool_is(member(*analysis, "executable"), true),
                  true, value_or_null(analysis));
}

void validate_formula_explicit_context_post(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "engine",
                  string_is(member(document, "engine"),
                            "tdx-source-interpreter-v1"),
                  "tdx-source-interpreter-v1",
                  value_or_null(member(document, "engine")));
    add_assertion(result, "inline_source_mode",
                  string_is(member(document, "formula_source_mode"), "inline-post"),
                  "inline-post", value_or_null(member(document, "formula_source_mode")));
    add_assertion(result, "request_not_retained",
                  bool_is(member(document, "request_body_retained"), false), false,
                  value_or_null(member(document, "request_body_retained")));
    const auto* analysis = member(document, "analysis");
    const bool explicit_analysis = analysis &&
        bool_is(member(*analysis, "syntax_supported"), true) &&
        bool_is(member(*analysis, "executable"), false) &&
        bool_is(member(*analysis, "executable_with_context"), false) &&
        bool_is(member(*analysis, "explicit_context_bindable"), true) &&
        bool_is(member(*analysis, "requires_automatic_market_context"), false);
    add_assertion(result, "explicit_context_analysis", explicit_analysis, true,
                  value_or_null(analysis));
    bool required_keys = false;
    if (analysis) {
        const auto* required = member(*analysis, "explicit_context_bindings_required");
        if (required && required->is_array()) {
            std::set<std::string> keys;
            for (const auto& value : required->as_array())
                if (value.is_string()) keys.insert(value.as_string());
            required_keys = keys == std::set<std::string>{
                "L2_AMO#0#2", "LARGEINTRDVOL", "LARGEOUTTRDVOL"};
        }
    }
    add_assertion(result, "exact_explicit_bindings", required_keys,
                  "L2_AMO#0#2,LARGEINTRDVOL,LARGEOUTTRDVOL", required_keys);
    bool injected_values = false;
    if (const auto* points = member(document, "points"); points && points->is_array()) {
        for (const auto& point : points->as_array()) {
            if (!string_is(member(point, "date"), "2026-08-05") ||
                !string_is(member(point, "time"), "15:00")) continue;
            const auto* values = member(point, "values");
            injected_values = values &&
                number_is(member(*values, "AMO0"), 123456) &&
                number_is(member(*values, "FLOW"), 180);
            break;
        }
    }
    add_assertion(result, "caller_values_preserved", injected_values,
                  "AMO0=123456 and FLOW=180 at 2026-08-05|15:00", injected_values);
}

void validate_formula_library_context_post(const std::string& contract_id, const Json& document,
        Json& result) {
    (void)contract_id;
    add_assertion(result, "engine",
                  string_is(member(document, "engine"),
                            "tdx-source-interpreter-v1"),
                  "tdx-source-interpreter-v1",
                  value_or_null(member(document, "engine")));
    const bool library_identity =
        string_is(member(document, "formula_source_mode"), "library-post") &&
        string_is(member(document, "library_formula_kind"), "technical") &&
        string_is(member(document, "library_formula_code"), "ZJLX") &&
        string_is(member(document, "formula"), "ZJLX");
    add_assertion(result, "library_identity", library_identity,
                  "library-post technical/ZJLX", library_identity);
    const bool sanitized = bool_is(member(document, "request_body_retained"), false) &&
        !object_has(&document, "source_text") &&
        !object_has(&document, "request_body");
    add_assertion(result, "request_and_formula_source_not_retained", sanitized,
                  true, sanitized);
    bool injected_values = false;
    if (const auto* points = member(document, "points"); points && points->is_array()) {
        for (const auto& point : points->as_array()) {
            if (!string_is(member(point, "date"), "2026-08-07") ||
                !string_is(member(point, "time"), "15:00")) continue;
            const auto* values = member(point, "values");
            const auto* main_buy = values ? member(*values, "主买净额") : nullptr;
            injected_values = main_buy && main_buy->is_number() &&
                std::isfinite(main_buy->as_number());
            break;
        }
    }
    add_assertion(result, "authorized_values_reach_builtin_formula", injected_values,
                  "numeric 主买净额 at 2026-08-07|15:00", injected_values);
}

void validate_formula_context_template(const std::string& contract_id, const Json& document,
        Json& result) {
    const bool kline_mode = contract_id == "formula-context-template-kline-live";
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-formula-explicit-context-v1"),
                  "tdx-formula-explicit-context-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "no_automatic_market_context",
                  bool_is(member(document, "automatic_market_context"), false),
                  false, value_or_null(member(document, "automatic_market_context")));
    const auto* metadata = member(document, "_template");
    const auto* series = member(document, "series");
    const bool shape = metadata && metadata->is_object() &&
        string_is(member(*metadata, "schema"),
                  "tdx-formula-explicit-context-template-v1") &&
        member(*metadata, "binding_count") &&
        member(*metadata, "binding_count")->is_number() &&
        member(*metadata, "binding_count")->as_number() == 8 &&
        series && series->is_object() && series->size() == 8;
    add_assertion(result, "exact_zjlx_binding_shape", shape,
                  "8 exact L2_AMO bindings", shape);
    bool placeholders = shape;
    if (series && series->is_object()) {
        for (const auto& [name, values] : series->as_object()) {
            if (name.rfind("L2_AMO#", 0) != 0 || !values.is_object()) {
                placeholders = false;
                break;
            }
            if (kline_mode) {
                if (values.size() != 20) { placeholders = false; break; }
                for (const auto& [stamp, value] : values.as_object())
                    if (stamp.find('|') == std::string::npos || !value.is_null()) {
                        placeholders = false;
                        break;
                    }
            } else {
                const auto* value = member(values, "2026-08-08|15:00");
                if (!value || !value->is_null()) {
                    placeholders = false;
                    break;
                }
            }
        }
    }
    add_assertion(result, "null_placeholders_only", placeholders,
                  "caller must replace nulls with authorized values", placeholders);
    if (kline_mode) {
        const bool provenance = metadata && metadata->is_object() &&
            string_is(member(*metadata, "stamp_source"), "kline") &&
            string_is(member(*metadata, "market"), "sz") &&
            string_is(member(*metadata, "code"), "000001") &&
            string_is(member(*metadata, "period"), "day") &&
            number_is(member(*metadata, "stamp_count"), 20) &&
            number_is(member(*metadata, "bar_count"), 20);
        add_assertion(result, "kline_provenance", provenance,
                      "sz000001 day / 20 exact K-line stamps", provenance);
    }
}

void validate_formula_limit_price_context_template(
        const std::string& contract_id, const Json& document, Json& result) {
    (void)contract_id;
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-formula-explicit-context-v1"),
                  "tdx-formula-explicit-context-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "no_automatic_market_context",
                  bool_is(member(document, "automatic_market_context"), false),
                  false, value_or_null(member(document, "automatic_market_context")));
    const auto* scalars = member(document, "formula_scalar_bindings");
    const auto* series = member(document, "series");
    const auto* metadata = member(document, "_template");
    const bool scalar_shape = scalars && scalars->is_object() && scalars->size() == 2 &&
        member(*scalars, "HOST_TYPE120_SECURITY_CLASS_RAW") &&
        member(*scalars, "HOST_TYPE120_SECURITY_CLASS_RAW")->is_null() &&
        member(*scalars, "HOST_EVALUATOR_MARKET_WORD_RAW") &&
        member(*scalars, "HOST_EVALUATOR_MARKET_WORD_RAW")->is_null() &&
        series && series->is_object() && series->as_object().empty() &&
        metadata && metadata->is_object() &&
        number_is(member(*metadata, "binding_count"), 2) &&
        number_is(member(*metadata, "scalar_binding_count"), 2) &&
        number_is(member(*metadata, "series_binding_count"), 0) &&
        number_is(member(*metadata, "stamp_count"), 0) &&
        string_is(member(*metadata, "stamp_source"),
                  "not-required-scalar-only") &&
        bool_is(member(*metadata, "kline_fetch_skipped"), true) &&
        string_is(member(*metadata, "requested_market"), "sz") &&
        string_is(member(*metadata, "requested_code"), "000001") &&
        string_is(member(*metadata, "requested_period"), "day") &&
        number_is(member(*metadata, "bar_count"), 0);
    add_assertion(result, "exact_limit_price_scalar_shape", scalar_shape,
                  "2 caller-owned raw u16 scalars and no per-bar series", scalar_shape);
    bool binding_metadata = scalar_shape;
    const auto* bindings = metadata ? member(*metadata, "bindings") : nullptr;
    if (!bindings || !bindings->is_array() || bindings->size() != 2) {
        binding_metadata = false;
    } else {
        for (const auto& binding : bindings->as_array()) {
            if (!string_is(member(binding, "source_kind"),
                           "caller-host-raw-scalar") ||
                !string_is(member(binding, "value_shape"), "u16-scalar") ||
                !bool_is(member(binding, "required"), true)) {
                binding_metadata = false;
                break;
            }
        }
    }
    add_assertion(result, "raw_scalar_metadata", binding_metadata,
                  "required caller-host-raw-scalar / u16-scalar", binding_metadata);
}

void validate_formula_context_import_post(const std::string& contract_id,
                                          const Json& document,
                                          Json& result) {
    (void)contract_id;
    const auto* metadata = member(document, "_capture_import");
    const bool identity =
        string_is(member(document, "schema"),
                  "tdx-formula-explicit-context-v1") &&
        metadata &&
        string_is(member(*metadata, "schema"),
                  "tdx-formula-context-capture-import-v1") &&
        string_is(member(*metadata, "capture_id"), "contract-owned-001") &&
        bool_is(member(*metadata, "complete"), true) &&
        bool_is(member(*metadata, "evaluation_ready"), true);
    add_assertion(result, "capture_import_identity", identity,
                  "complete reusable explicit context", identity);
    const auto* value = member_path(
        document, {"series", "SIGNALS_QS#102#0", "2026-08-14|15:00"});
    const bool aligned = number_is(value, 42.0) &&
        number_is(member(*metadata, "expected_series_point_count"), 1.0) &&
        number_is(member(*metadata, "matched_series_point_count"), 1.0) &&
        number_is(member(*metadata, "missing_series_point_count"), 0.0);
    add_assertion(result, "capture_exact_alignment", aligned,
                  "one exact DATE|TIME value", value_or_null(value));
    const bool offline =
        bool_is(member(*metadata, "capture_document_retained"), false) &&
        bool_is(member(*metadata, "request_body_retained"), false) &&
        bool_is(member(*metadata, "file_accessed"), false) &&
        bool_is(member(*metadata, "path_accepted"), false) &&
        bool_is(member(*metadata, "sdk_called"), false) &&
        bool_is(member(*metadata, "subscription_sent"), false) &&
        bool_is(member(*metadata, "account_accessed"), false) &&
        bool_is(member(*metadata, "order_sent"), false) &&
        bool_is(member(*metadata, "authorization_bypass"), false) &&
        number_is(member(*metadata, "network_requests"), 0.0) &&
        !object_has(&document, "template") &&
        !object_has(&document, "capture") &&
        !object_has(&document, "records");
    add_assertion(result, "capture_offline_boundary", offline,
                  "no retained envelope/file/SDK/account/order/network", offline);
}

void validate_formula_context_import_partial_post(
    const std::string& contract_id, const Json& document, Json& result) {
    (void)contract_id;
    const auto* metadata = member(document, "_capture_import");
    const bool diagnosed = metadata &&
        bool_is(member(*metadata, "allow_partial"), true) &&
        bool_is(member(*metadata, "complete"), false) &&
        bool_is(member(*metadata, "evaluation_ready"), false) &&
        number_is(member(*metadata, "missing_series_point_count"), 1.0) &&
        member(*metadata, "missing_series_points_preview") &&
        member(*metadata, "missing_series_points_preview")->is_array() &&
        member(*metadata, "missing_series_points_preview")->size() == 1 &&
        number_is(member(*metadata,
                         "missing_series_points_preview_omitted"), 0.0);
    add_assertion(result, "capture_partial_diagnostics", diagnosed,
                  "one missing point with bounded preview", diagnosed);
    const auto* point = member_path(
        document, {"series", "SIGNALS_QS#102#0", "2026-08-14|15:00"});
    add_assertion(result, "capture_partial_never_fabricates", point && point->is_null(),
                  nullptr, value_or_null(point));
}

struct ContextContract {
    std::string_view id;
    ContextContractValidator validate;
};

constexpr std::array<ContextContract, 9> context_contracts{{
    {"formula-string-builders-inline-post", validate_formula_string_builders_inline_post},
    {"formula-inline-post", validate_formula_inline_post},
    {"formula-explicit-context-post", validate_formula_explicit_context_post},
    {"formula-library-context-post", validate_formula_library_context_post},
    {"formula-context-template-live", validate_formula_context_template},
    {"formula-context-template-kline-live", validate_formula_context_template},
    {"formula-context-import-post", validate_formula_context_import_post},
    {"formula-context-import-partial-post",
     validate_formula_context_import_partial_post},
    {"formula-limit-price-context-template-live",
     validate_formula_limit_price_context_template},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < context_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < context_contracts.size(); ++right)
            if (context_contracts[left].id == context_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_formula_runtime_context_contract(const std::string& contract_id,
    const Json& document, Json& result) {
    for (const auto& contract : context_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(contract_id, document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail
