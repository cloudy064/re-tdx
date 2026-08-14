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

using SecurityContractValidator = void (*)(const Json& document, Json& result);

void validate_formula_adjustment_flag_inline_post(const Json& document, Json& result) {
    const bool identity =
        string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
        string_is(member(document, "execution_mode"), "native-cpp") &&
        string_is(member(document, "formula_source_mode"), "inline-post") &&
        string_is(member(document, "formula"), "CONTRACT_ADJUSTMENT_FLAG");
    add_assertion(result, "native_formula_identity", identity,
                  "inline-post native-cpp CONTRACT_ADJUSTMENT_FLAG", identity);

    const auto* analysis = member(document, "analysis");
    const bool context_dependency = analysis &&
        bool_is(member(*analysis, "syntax_supported"), true) &&
        bool_is(member(*analysis, "executable"), true) &&
        bool_is(member(*analysis, "pure_ohlcv"), false) &&
        bool_is(member(*analysis, "has_adjustment_mode_dependency"), true) &&
        bool_is(member(*analysis, "has_external_dependency"), false) &&
        bool_is(member(*analysis, "has_future_function"), false);
    add_assertion(result, "adjustment_context_analysis", context_dependency,
                  "TQFLAG is executable and depends only on K-line adjustment context",
                  context_dependency);

    const auto* adjustment = member(document, "adjustment");
    const bool qfq_metadata =
        string_is(member(document, "adjustment_mode"), "qfq") && adjustment &&
        string_is(member(*adjustment, "mode"), "qfq") &&
        string_is(member(*adjustment, "source_command"), "0x000F") &&
        string_is(member(*adjustment, "method"),
                  "local-corporate-action-factor-v1");
    add_assertion(result, "adjustment_flag_metadata", qfq_metadata,
                  "formula response preserves qfq corporate-action metadata",
                  qfq_metadata);

    bool shape = false, qfq_mode = false;
    if (const auto* points = member(document, "points");
        points && points->is_array() && points->size() == 120) {
        shape = true;
        qfq_mode = true;
        for (const auto& point : points->as_array()) {
            const auto* values = member(point, "values");
            qfq_mode = qfq_mode && values &&
                number_is(member(*values, "T"), 1.0) &&
                number_is(member(*values, "TF"), 1.0);
        }
    }
    add_assertion(result, "adjustment_flag_shape", shape,
                  "120 complete TQFLAG points", shape);
    add_assertion(result, "adjustment_flag_qfq", qfq_mode,
                  "qfq HTTP formula path broadcasts TQFLAG/TQFLAG() as mode 1",
                  qfq_mode);
}

void validate_formula_adjustment_qfq_get(const Json& document, Json& result) {
    const bool identity =
        string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
        string_is(member(document, "execution_mode"), "native-cpp") &&
        string_is(member(document, "formula"), "MACD");
    add_assertion(result, "native_formula_identity", identity,
                  "GET native-cpp MACD", identity);

    const auto* adjustment = member(document, "adjustment");
    const bool metadata =
        string_is(member(document, "adjustment_mode"), "qfq") && adjustment &&
        string_is(member(*adjustment, "mode"), "qfq") &&
        string_is(member(*adjustment, "source_command"), "0x000F") &&
        string_is(member(*adjustment, "method"),
                  "local-corporate-action-factor-v1") &&
        numeric_value(member(*adjustment, "applied_event_count")).has_value();
    add_assertion(result, "qfq_adjustment_metadata", metadata,
                  "qfq mode and local 0x000F corporate-action metadata", metadata);

    bool shape = false;
    if (const auto* points = member(document, "points");
        points && points->is_array() && points->size() == 120) {
        const auto* first_values = member(points->as_array().front(), "values");
        const auto* last_values = member(points->as_array().back(), "values");
        shape = first_values && last_values &&
            member(*first_values, "DIF") && member(*first_values, "DEA") &&
            member(*first_values, "MACD") && member(*last_values, "DIF") &&
            member(*last_values, "DEA") && member(*last_values, "MACD");
    }
    add_assertion(result, "qfq_formula_shape", shape,
                  "120 MACD points evaluated on the adjusted K-line document", shape);
}

void validate_formula_security_string_inline_post(const Json& document, Json& result) {
    const bool identity =
        string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
        string_is(member(document, "execution_mode"), "native-cpp") &&
        string_is(member(document, "formula_source_mode"), "inline-post") &&
        string_is(member(document, "formula"), "CONTRACT_SECURITY_STRING");
    add_assertion(result, "native_formula_identity", identity,
                  "inline-post native-cpp CONTRACT_SECURITY_STRING", identity);
    const auto* analysis = member(document, "analysis");
    const auto* unsupported = analysis ? member(*analysis, "unsupported") : nullptr;
    const bool pure = analysis &&
        bool_is(member(*analysis, "syntax_supported"), true) &&
        bool_is(member(*analysis, "executable"), true) &&
        bool_is(member(*analysis, "numeric_signal_safe"), true) &&
        bool_is(member(*analysis, "has_external_dependency"), false) &&
        bool_is(member(*analysis, "has_future_function"), false) &&
        unsupported && unsupported->is_array() && unsupported->size() == 0;
    add_assertion(result, "security_string_analysis", pure,
                  "pure exact security metadata/string semantics", pure);

    bool matches = false, conversion = false, direction = false;
    if (const auto* points = member(document, "points");
        points && points->is_array() && points->size() == 120) {
        matches = true;
        conversion = true;
        direction = true;
        for (std::size_t i = 0; i < points->size(); ++i) {
            const auto& values = points->as_array()[i].at("values");
            matches = matches &&
                number_is(member(values, "N0"), 1.0) &&
                number_is(member(values, "N1"), 0.0) &&
                number_is(member(values, "C0"), 1.0) &&
                number_is(member(values, "C1"), 0.0) &&
                number_is(member(values, "I0"), 1.0) &&
                number_is(member(values, "I1"), 0.0) &&
                number_is(member(values, "F0"), 1.0) &&
                number_is(member(values, "F1"), 0.0);
            const auto parsed = numeric_value(member(values, "S0"));
            conversion = conversion && parsed &&
                std::abs(*parsed - 2365.02) < 0.001 &&
                number_is(member(values, "S1"), 0.0) &&
                number_is(member(values, "Z"), 0.0);
            const auto* updown = member(values, "U");
            direction = direction && updown &&
                (i == 0 ? updown->is_null() : number_is(updown, 1.0));
        }
    }
    add_assertion(result, "native_security_string_match", matches,
                  "name/code prefix and substring constants for 平安银行/000001",
                  matches);
    add_assertion(result, "native_string_conversion", conversion,
                  "STR2CON atof semantics and NOT(nonzero)=0", conversion);
    add_assertion(result, "native_updown", direction,
                  "UPDOWN first bar missing then rising bars=1", direction);

    bool name_text = false;
    if (const auto* render = member(document, "render_ir");
        render && render->is_object()) {
        if (const auto* primitives = member(*render, "primitives");
            primitives && primitives->is_array()) {
            for (const auto& primitive : primitives->as_array()) {
                if (!string_is(member(primitive, "function"), "DRAWTEXT_FIX"))
                    continue;
                const auto* events = member(primitive, "events");
                if (!events || !events->is_array()) continue;
                for (const auto& event : events->as_array())
                    if (string_is(member(event, "annotation_text"), "平安银行"))
                        name_text = true;
            }
        }
    }
    add_assertion(result, "native_stkname_render", name_text,
                  "STKNAME renders exact selected security name 平安银行", name_text);
}

void validate_formula_block_metadata_inline_post(const Json& document, Json& result) {
    const bool identity =
        string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
        string_is(member(document, "execution_mode"), "native-cpp") &&
        string_is(member(document, "formula_source_mode"), "inline-post") &&
        string_is(member(document, "formula"), "CONTRACT_BLOCK_METADATA") &&
        string_is(member(document, "market"), "sz") &&
        string_is(member(document, "code"), "000001");
    add_assertion(result, "native_formula_identity", identity,
                  "inline-post native-cpp CONTRACT_BLOCK_METADATA for sz000001",
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
        bool_is(member(*analysis, "executable"), false) &&
        bool_is(member(*analysis, "executable_with_context"), true) &&
        dependencies == std::set<std::string>{
            "FGBLOCK", "FGBLOCKNUM", "GNBLOCKNUM", "HYZSCODE", "INBLOCK",
            "ZSBLOCK", "ZSBLOCKNUM"};
    add_assertion(result, "block_metadata_analysis", contextual,
                  "7 automatic local block context dependencies", contextual);
    bool values_ok = false;
    if (const auto* points = member(document, "points");
        points && points->is_array() && points->size() == 120) {
        const auto* values = member(points->as_array().back(), "values");
        const auto count = values ? numeric_value(member(*values, "N")) : std::nullopt;
        values_ok = values && number_is(member(*values, "M"), 1.0) &&
            number_is(member(*values, "MISS"), 0.0) && count && *count > 0.0;
    }
    add_assertion(result, "native_inblock_and_counts", values_ok,
                  "平安银行 INBLOCK(银行)=1, missing=0 and local counts > 0",
                  values_ok);
    bool text_ok = false;
    if (const auto* metadata = member(document, "context_metadata")) {
        const auto* texts = member(*metadata, "formula_text_symbols");
        const auto* style = texts ? member(*texts, "FGBLOCK") : nullptr;
        const auto* indices = texts ? member(*texts, "ZSBLOCK") : nullptr;
        text_ok = texts && string_is(member(*texts, "HYZSCODE"), "880471") &&
            style && style->is_string() && !style->as_string().empty() &&
            indices && indices->is_string() && !indices->as_string().empty();
    }
    add_assertion(result, "native_block_text_and_index_code", text_ok,
                  "style/index text non-empty and HYZSCODE=880471", text_ok);
}

void validate_formula_block_code_name_inline_post(const Json& document, Json& result) {
    const bool identity =
        string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
        string_is(member(document, "execution_mode"), "native-cpp") &&
        string_is(member(document, "formula_source_mode"), "inline-post") &&
        string_is(member(document, "formula"), "CONTRACT_BLOCK_CODE_NAME") &&
        string_is(member(document, "market"), "sz") &&
        string_is(member(document, "code"), "000001");
    add_assertion(result, "native_formula_identity", identity,
                  "inline-post native-cpp CONTRACT_BLOCK_CODE_NAME for sz000001",
                  identity);
    std::set<std::string> dependencies;
    std::set<std::string> bindings;
    const auto* analysis = member(document, "analysis");
    if (analysis) {
        if (const auto* values = member(*analysis, "automatic_context_dependencies");
            values && values->is_array())
            for (const auto& value : values->as_array())
                if (value.is_string()) dependencies.insert(value.as_string());
        if (const auto* values = member(*analysis, "context_bindings_required");
            values && values->is_array())
            for (const auto& value : values->as_array())
                if (value.is_string()) bindings.insert(value.as_string());
    }
    const bool contextual = analysis &&
        bool_is(member(*analysis, "syntax_supported"), true) &&
        bool_is(member(*analysis, "executable"), false) &&
        bool_is(member(*analysis, "executable_with_context"), true) &&
        dependencies == std::set<std::string>{
            "BLOCKSETNUM", "FGBKZSCODE", "GETNAMEOFCODE", "GNBKZSCODE",
            "HORCALC", "HYZSCODE", "INSORT", "INSUM"} &&
        bindings == std::set<std::string>{
            "BLOCKSETNUM#GN.跨境支付", "BLOCKSETNUM#HY.银行", "BLOCKSETNUM#不存在",
            "HORCALC#HY.银行#103#0#2", "HORCALC#HY.银行#103#1#2",
            "HORCALC#HY.银行#103#2#2",
            "INSORT#HY.银行#KDJ#3#0", "INSORT#HY.银行#KDJ#3#1",
            "INSUM#HY.银行#KDJ#3#0", "INSUM#HY.银行#KDJ#3#1",
            "INSUM#HY.银行#KDJ#3#2", "INSUM#HY.银行#KDJ#3#3",
            "INSUM#HY.银行#KDJ#3#4", "INSUM#HY.银行#KDJ#3#5"};
    add_assertion(result, "block_code_name_analysis", contextual,
                  "eight exact automatic dependencies and fourteen horizontal bindings",
                  contextual);
    bool values_ok = false;
    if (const auto* points = member(document, "points");
        points && points->is_array() && points->size() == 120) {
        const auto* values = member(points->as_array().back(), "values");
        const auto horizontal_sum = values
            ? numeric_value(member(*values, "HS")) : std::nullopt;
        const auto horizontal_rank = values
            ? numeric_value(member(*values, "HR")) : std::nullopt;
        const auto horizontal_average = values
            ? numeric_value(member(*values, "HA")) : std::nullopt;
        const auto indicator_rank_descending = values
            ? numeric_value(member(*values, "IRD")) : std::nullopt;
        const auto indicator_rank_ascending = values
            ? numeric_value(member(*values, "IRA")) : std::nullopt;
        const auto indicator_sum = values
            ? numeric_value(member(*values, "ISS")) : std::nullopt;
        const auto indicator_average = values
            ? numeric_value(member(*values, "IAV")) : std::nullopt;
        const auto indicator_maximum = values
            ? numeric_value(member(*values, "IMX")) : std::nullopt;
        const auto indicator_minimum = values
            ? numeric_value(member(*values, "IMN")) : std::nullopt;
        const auto indicator_maximum_index = values
            ? numeric_value(member(*values, "IMXI")) : std::nullopt;
        const auto indicator_minimum_index = values
            ? numeric_value(member(*values, "IMNI")) : std::nullopt;
        values_ok = values && number_is(member(*values, "G"), 1.0) &&
            number_is(member(*values, "F"), 1.0) &&
            number_is(member(*values, "H"), 1.0) &&
            number_is(member(*values, "CG"), 1.0) &&
            number_is(member(*values, "CF"), 1.0) &&
            number_is(member(*values, "MISS"), 1.0) &&
            number_is(member(*values, "BSH"), 42.0) &&
            number_is(member(*values, "BSG"), 75.0) &&
            number_is(member(*values, "BSM"), 0.0) &&
            horizontal_sum && horizontal_rank && horizontal_average &&
            *horizontal_rank >= 1.0 && *horizontal_rank <= 42.0 &&
            std::fabs(*horizontal_sum - *horizontal_average * 42.0) < 0.02 &&
            indicator_rank_descending && indicator_rank_ascending &&
            indicator_sum && indicator_average && indicator_maximum &&
            indicator_minimum && indicator_maximum_index &&
            indicator_minimum_index &&
            *indicator_rank_descending >= 1.0 &&
            *indicator_rank_descending <= 42.0 &&
            *indicator_rank_ascending >= 1.0 &&
            *indicator_rank_ascending <= 42.0 &&
            std::fabs(*indicator_sum - *indicator_average * 42.0) < 0.02 &&
            *indicator_maximum >= *indicator_minimum &&
            *indicator_maximum_index >= 1.0 &&
            *indicator_maximum_index <= 42.0 &&
            *indicator_minimum_index >= 1.0 &&
            *indicator_minimum_index <= 42.0;
    }
    add_assertion(result, "native_block_code_name_values", values_ok,
                  "block codes/counts plus 42-member HORCALC and KDJ INSORT/INSUM values",
                  values_ok);
    bool provenance = false;
    if (const auto* metadata = member(document, "context_metadata");
        metadata && metadata->is_object()) {
        const auto* concepts = member(*metadata, "type167_concept_codes");
        const auto* styles = member(*metadata, "type167_style_codes");
        const auto override_count = numeric_value(
            member(*metadata, "code_name_override_count"));
        const auto* resolutions = member(*metadata, "blocksetnum_resolutions");
        const auto* horizontal_resolutions =
            member(*metadata, "horcalc_resolutions");
        const auto* aggregate_resolutions =
            member(*metadata, "indicator_aggregate_resolutions");
        bool bank = false;
        bool concept_result = false;
        bool missing = false;
        if (resolutions && resolutions->is_array()) {
            for (const auto& item : resolutions->as_array()) {
                if (string_is(member(item, "binding"), "BLOCKSETNUM#HY.银行"))
                    bank = bool_is(member(item, "found"), true) &&
                        string_is(member(item, "family"), "research-industry") &&
                        string_is(member(item, "block_code"), "881385") &&
                        number_is(member(item, "member_count"), 42.0);
                else if (string_is(member(item, "binding"), "BLOCKSETNUM#GN.跨境支付"))
                    concept_result = bool_is(member(item, "found"), true) &&
                        string_is(member(item, "family"), "concept") &&
                        string_is(member(item, "block_code"), "880609") &&
                        number_is(member(item, "member_count"), 75.0);
                else if (string_is(member(item, "binding"), "BLOCKSETNUM#不存在"))
                    missing = bool_is(member(item, "found"), false) &&
                        number_is(member(item, "member_count"), 0.0);
            }
        }
        bool horizontal = horizontal_resolutions &&
            horizontal_resolutions->is_array() &&
            horizontal_resolutions->size() == 3;
        if (horizontal)
            for (const auto& item : horizontal_resolutions->as_array())
                horizontal = horizontal &&
                    bool_is(member(item, "found"), true) &&
                    string_is(member(item, "family"), "research-industry") &&
                    string_is(member(item, "block_code"), "881385") &&
                    number_is(member(item, "member_count"), 42.0) &&
                    number_is(member(item, "member_series_count"), 42.0);
        bool indicator_aggregates = aggregate_resolutions &&
            aggregate_resolutions->is_array() &&
            aggregate_resolutions->size() == 8;
        if (indicator_aggregates)
            for (const auto& item : aggregate_resolutions->as_array()) {
                const auto used = numeric_value(member(item, "member_series_count"));
                indicator_aggregates = indicator_aggregates &&
                    bool_is(member(item, "found"), true) &&
                    string_is(member(item, "family"), "research-industry") &&
                    string_is(member(item, "block_code"), "881385") &&
                    string_is(member(item, "formula_code"), "KDJ") &&
                    string_is(member(item, "formula_output_name"), "J") &&
                    number_is(member(item, "member_count"), 42.0) &&
                    used && *used >= 41.0 && *used <= 42.0;
            }
        provenance =
            string_is(member(*metadata, "type167_block_code_mode"),
                      "tdxw-type167-direct-membership-dword-ascending-concept-then-style") &&
            number_is(member(*metadata, "type167_block_code_limit"), 60.0) &&
            number_is(member(*metadata, "type167_concept_code_count"), 1.0) &&
            number_is(member(*metadata, "type167_style_code_count"), 10.0) &&
            number_is(member(*metadata, "type167_block_code_total"), 11.0) &&
            concepts && concepts->is_array() && concepts->size() == 1 &&
            string_is(&concepts->as_array().front(), "880609") &&
            styles && styles->is_array() && styles->size() == 10 &&
            string_is(&styles->as_array().front(), "880679") &&
            string_is(member(*metadata, "code_name_lookup_mode"),
                      "tdxw-type120-security-directory-name-offset31-native-tnf-cache") &&
            string_is(member(*metadata, "code_name_security_catalog_source"),
                      "T0002/hq_cache/{szs,shs,bjs}.tnf") &&
            override_count && *override_count > 0.0 &&
            string_is(member(*metadata, "blocksetnum_mode"),
                      "tcalc-opcode1244-command8-type5-offset1004-local-catalog-first-match") &&
            number_is(member(*metadata, "blocksetnum_binding_count"), 3.0) &&
            number_is(member(*metadata, "blocksetnum_industry_mode"), 2.0) &&
            resolutions && resolutions->is_array() && resolutions->size() == 3 &&
            bank && concept_result && missing && horizontal &&
            string_is(member(*metadata, "horcalc_mode"),
                      "tcalc-opcode1245-command8-type7-local-day-date-aligned-horizontal-aggregate") &&
            number_is(member(*metadata, "horcalc_binding_count"), 3.0) &&
            number_is(member(*metadata, "horcalc_member_file_count"), 42.0) &&
            number_is(member(*metadata, "horcalc_member_series_count"), 42.0) &&
            number_is(member(*metadata, "horcalc_industry_mode"), 2.0) &&
            indicator_aggregates &&
            string_is(member(*metadata, "indicator_aggregate_mode"),
                      "tcalc-opcodes1246-1247-active-technical-indicator-local-day-horizontal-evaluation") &&
            number_is(member(*metadata, "insort_binding_count"), 2.0) &&
            number_is(member(*metadata, "insum_binding_count"), 6.0) &&
            number_is(member(*metadata, "indicator_aggregate_member_file_count"), 42.0) &&
            number_is(member(*metadata, "indicator_aggregate_member_series_count"), 42.0) &&
            number_is(member(*metadata, "indicator_aggregate_formula_evaluation_count"), 42.0) &&
            number_is(member(*metadata, "indicator_aggregate_warmup_bars"), 100.0) &&
            number_is(member(*metadata, "indicator_aggregate_industry_mode"), 2.0);
    }
    add_assertion(result, "native_block_code_name_provenance", provenance,
                  "type-167/type-120 provenance plus opcode-1244..1247 horizontal evaluation",
                  provenance);
}

struct SecurityContract {
    std::string_view id;
    SecurityContractValidator validate;
};

constexpr std::array<SecurityContract, 5> security_contracts{{
    {"formula-adjustment-flag-inline-post", validate_formula_adjustment_flag_inline_post},
    {"formula-adjustment-qfq-get", validate_formula_adjustment_qfq_get},
    {"formula-security-string-inline-post", validate_formula_security_string_inline_post},
    {"formula-block-metadata-inline-post", validate_formula_block_metadata_inline_post},
    {"formula-block-code-name-inline-post", validate_formula_block_code_name_inline_post},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < security_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < security_contracts.size(); ++right)
            if (security_contracts[left].id == security_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_formula_runtime_security_contract(const std::string& contract_id,
    const Json& document, Json& result) {
    for (const auto& contract : security_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail