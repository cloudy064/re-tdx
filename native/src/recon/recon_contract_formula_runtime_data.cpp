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

using DataContractValidator = void (*)(const Json& document, Json& result);

void validate_formula_single_point_data_inline_post(const Json& document, Json& result) {
    const bool identity =
        string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
        string_is(member(document, "execution_mode"), "native-cpp") &&
        string_is(member(document, "formula_source_mode"), "inline-post") &&
        string_is(member(document, "formula"), "CONTRACT_SINGLE_POINT_DATA") &&
        string_is(member(document, "market"), "sz") &&
        string_is(member(document, "code"), "000001");
    add_assertion(result, "native_formula_identity", identity,
                  "inline-post native-cpp CONTRACT_SINGLE_POINT_DATA for sz000001",
                  identity);
    std::set<std::string> dependencies, bindings;
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
            "BKJYONE", "FINONE", "GPJYONE", "GPONEDAT", "SCJYONE"} &&
        bindings == std::set<std::string>{
            "BKJYONE#5#1#0#0", "FINONE#183#0#0",
            "GPJYONE#1#1#0#0", "GPONEDAT#7", "SCJYONE#1#1#0#0"};
    add_assertion(result, "single_point_analysis", contextual,
                  "five exact automatic scalar bindings", contextual);
    bool values_ok = false;
    if (const auto* points = member(document, "points");
        points && points->is_array() && points->size() == 120) {
        const auto* values = member(points->as_array().back(), "values");
        const auto finance = values ? numeric_value(member(*values, "F")) : std::nullopt;
        const auto stock = values ? numeric_value(member(*values, "G")) : std::nullopt;
        const auto board = values ? numeric_value(member(*values, "B")) : std::nullopt;
        const auto market_value = values ? numeric_value(member(*values, "S")) : std::nullopt;
        const auto local_value = values ? numeric_value(member(*values, "P")) : std::nullopt;
        values_ok = finance && stock && board && market_value && local_value &&
            std::isfinite(*finance) && *stock > 0.0 && *board > 0.0 &&
            *market_value > 0.0 && std::isfinite(*local_value);
    }
    add_assertion(result, "single_point_values", values_ok,
                  "latest finance/stock/board/market values and deterministic local value",
                  values_ok);
    bool provenance = false;
    if (const auto* metadata = member(document, "context_metadata");
        metadata && metadata->is_object()) {
        const auto* source = member(*metadata, "gponedat_source");
        const std::string suffix = "gpszone.dat";
        const bool source_ok = source && source->is_string() &&
            source->as_string().size() >= suffix.size() &&
            source->as_string().compare(source->as_string().size() - suffix.size(),
                                        suffix.size(), suffix) == 0;
        provenance =
            string_is(member(*metadata, "finone_mode"),
                      "tcalc-type172-quarter-year-mmdd-single-point-official-gpcw") &&
            number_is(member(*metadata, "finone_period_count"), 1.0) &&
            number_is(member(*metadata, "finone_relative_period_limit"), 1.0) &&
            string_is(member(*metadata, "gpjyone_mode"),
                      "tdxw-type175-exact-date-or-reverse-ordinal-official-tdxgp") &&
            string_is(member(*metadata, "bkjyone_target_market"), "sh") &&
            string_is(member(*metadata, "bkjyone_target_code"), "880471") &&
            string_is(member(*metadata, "scjyone_mode"),
                      "tdxw-type175-sh999999-exact-date-or-reverse-ordinal-official-tdxgp") &&
            string_is(member(*metadata, "gponedat_mode"),
                      "tdxw-type170-local-10-byte-record-zero-when-absent") &&
            source_ok;
    }
    add_assertion(result, "single_point_provenance", provenance,
                  "type172/type175 official packages plus type170 local 10-byte cache",
                  provenance);
}

void validate_formula_type167_text_inline_post(const Json& document, Json& result) {
    const bool identity =
        string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
        string_is(member(document, "execution_mode"), "native-cpp") &&
        string_is(member(document, "formula_source_mode"), "inline-post") &&
        string_is(member(document, "formula"), "CONTRACT_TYPE167_TEXT") &&
        string_is(member(document, "market"), "sz") &&
        string_is(member(document, "code"), "000001");
    add_assertion(result, "native_formula_identity", identity,
                  "inline-post native-cpp CONTRACT_TYPE167_TEXT for sz000001",
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
            "LEVEL1HYBLOCK", "MAINBUSINESS", "MOREHYBLOCK", "SIMIBLOCK",
            "ZDBLOCK", "ZDBLOCKNUM", "ZHBLOCK", "ZHBLOCKNUM"};
    add_assertion(result, "type167_text_analysis", contextual,
                  "eight exact automatic type-167/custom/combination-block dependencies", contextual);

    bool values_ok = false;
    if (const auto* points = member(document, "points");
        points && points->is_array() && points->size() == 120) {
        const auto* values = member(points->as_array().back(), "values");
        values_ok = values && number_is(member(*values, "L"), 1.0) &&
            number_is(member(*values, "M"), 1.0) &&
            number_is(member(*values, "H"), 1.0) &&
            number_is(member(*values, "Z"), 1.0) &&
            number_is(member(*values, "N"), 0.0) &&
            number_is(member(*values, "Q"), 1.0) &&
            number_is(member(*values, "R"), 0.0) &&
            number_is(member(*values, "S"), 1.0);
    }
    add_assertion(result, "native_type167_text_values", values_ok,
                  "平安银行 research leaf=股份制银行 and empty local custom/combination memberships",
                  values_ok);

    bool provenance = false;
    if (const auto* metadata = member(document, "context_metadata");
        metadata && metadata->is_object()) {
        const auto* texts = member(*metadata, "formula_text_symbols");
        const auto* sources = member(*metadata, "formula_text_symbol_sources");
        const auto* level1 = member(*metadata, "level1_research_industry");
        const auto* more = member(*metadata, "more_industry");
        const auto* source = member(*metadata, "main_business_source");
        const auto* combination_source =
            member(*metadata, "combination_block_directory_source");
        const auto count = numeric_value(member(*metadata, "main_business_record_count"));
        const std::string suffix = "specgpext.txt";
        const bool source_ok = source && source->is_string() &&
            source->as_string().size() >= suffix.size() &&
            source->as_string().compare(
                source->as_string().size() - suffix.size(), suffix.size(), suffix) == 0;
        const std::string combination_suffix = "lcidx.lii";
        const bool combination_source_ok = combination_source &&
            combination_source->is_string() &&
            combination_source->as_string().size() >= combination_suffix.size() &&
            combination_source->as_string().compare(
                combination_source->as_string().size() - combination_suffix.size(),
                combination_suffix.size(), combination_suffix) == 0;
        provenance = texts && sources && level1 && level1->is_object() &&
            more && more->is_object() &&
            string_is(member(*texts, "LEVEL1HYBLOCK"), "银行") &&
            string_is(member(*texts, "MAINBUSINESS"), "零售金融业务") &&
            string_is(member(*texts, "MOREHYBLOCK"), "股份制银行") &&
            string_is(member(*texts, "ZDBLOCK"), " ") &&
            string_is(member(*texts, "ZHBLOCK"), " ") &&
            string_is(member(*texts, "SIMIBLOCK"), " ") &&
            string_is(member(*sources, "LEVEL1HYBLOCK"),
                      "tdxw-type167-research-industry-code-prefix3-local-hierarchy") &&
            string_is(member(*sources, "MAINBUSINESS"),
                      "tdxw-type167-specgpext-third-field") &&
            string_is(member(*sources, "MOREHYBLOCK"),
                      "tdxw-type167-user-ini-industry-mode-leaf-name") &&
            string_is(member(*sources, "ZDBLOCK"),
                      "tdxw-command8-category2-blocknew-membership-pipe-to-space") &&
            string_is(member(*sources, "ZHBLOCK"),
                      "tdxw-command8-category3-lcidx-cis-membership-pipe-to-space") &&
            string_is(member(*sources, "SIMIBLOCK"),
                      "tdxw-command8-category0-no-provider-branch-pipe-to-space") &&
            string_is(member(*level1, "family"), "research-industry") &&
            number_is(member(*level1, "level"), 1.0) &&
            string_is(member(*level1, "source_key"), "X50") &&
            number_is(member(*more, "configured_mode"), 2.0) &&
            number_is(member(*more, "type167_mode_byte"), 1.0) &&
            string_is(member(*more, "selected_family"), "research-industry") &&
            string_is(member(*more, "source_key"), "X500102") &&
            number_is(member(*metadata, "custom_block_membership_count"), 0.0) &&
            number_is(member(*metadata, "custom_block_directory_count"), 2.0) &&
            string_is(member(*metadata, "custom_block_mode"),
                      "tdxw-command8-category2-zxg-tjg-blocknew-local-reconstruction") &&
            number_is(member(*metadata, "combination_block_directory_count"), 0.0) &&
            number_is(member(*metadata, "combination_block_readable_member_file_count"), 0.0) &&
            number_is(member(*metadata, "combination_block_membership_count"), 0.0) &&
            string_is(member(*metadata, "combination_block_mode"),
                      "tdxw-command8-category3-lcidx-lii-cis-local-reconstruction") &&
            string_is(member(*metadata, "similar_block_mode"),
                      "tdxw-command8-category0-empty-in-current-host-build") &&
            source_ok &&
            combination_source_ok &&
            count && *count > 0.0 &&
            string_is(member(*metadata, "main_business_mode"),
                      "tdxw-type167-specgpext-enhanced-function-local-reconstruction");
    }
    add_assertion(result, "native_type167_text_provenance", provenance,
                  "user.ini research leaf, custom/combination directories and specgpext provenance",
                  provenance);
}

void validate_formula_market_breadth_dyna_inline_post(const Json& document, Json& result) {
    const bool identity =
        string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
        string_is(member(document, "execution_mode"), "native-cpp") &&
        string_is(member(document, "formula_source_mode"), "inline-post") &&
        string_is(member(document, "formula"),
                  "CONTRACT_MARKET_BREADTH_DYNA") &&
        string_is(member(document, "market"), "sz") &&
        string_is(member(document, "code"), "000001");
    add_assertion(result, "native_formula_identity", identity,
                  "inline-post native-cpp CONTRACT_MARKET_BREADTH_DYNA for sz000001",
                  identity);

    const auto* analysis = member(document, "analysis");
    std::set<std::string> dependencies;
    std::set<std::string> unavailable;
    if (analysis) {
        if (const auto* values = member(*analysis,
                "automatic_context_dependencies");
            values && values->is_array())
            for (const auto& value : values->as_array())
                if (value.is_string()) dependencies.insert(value.as_string());
        if (const auto* values = member(*analysis,
                "context_bindings_unavailable");
            values && values->is_array())
            for (const auto& value : values->as_array())
                if (value.is_string()) unavailable.insert(value.as_string());
    }
    const bool contextual = analysis &&
        bool_is(member(*analysis, "syntax_supported"), true) &&
        bool_is(member(*analysis, "context_bindable"), true) &&
        bool_is(member(*analysis, "executable_with_context"), true) &&
        dependencies == std::set<std::string>{
            "DYNA_LB", "DYNA_NOW", "DYNA_ZAF", "DYNA_ZAS",
            "INDEXADV", "INDEXDEC"} && unavailable.empty();
    add_assertion(result, "market_breadth_dyna_analysis", contextual,
                  "six exact automatic public-L1 dependencies with no unavailable binding",
                  contextual);

    const auto* metadata = member(document, "context_metadata");
    const auto* aliases = metadata ? member(*metadata, "dynamic_aliases") : nullptr;
    const auto alias_matches = [&](std::string_view name, double selector,
                                   double opcode,
                                   std::string_view command) {
        const auto* item = aliases ? member(*aliases, name) : nullptr;
        return item &&
            number_is(member(*item, "dynainfo_selector"), selector) &&
            number_is(member(*item, "tcalc_opcode"), opcode) &&
            bool_is(member(*item, "broadcast"), true) &&
            string_is(member(*item, "source_command"), command);
    };
    const bool provenance = metadata && aliases &&
        string_is(member(*metadata, "benchmark"), "sz:399001") &&
        string_is(member(*metadata, "benchmark_mode"),
                  "TCalc-opcodes1201-1202-native-market-code-selection-offsets31-33") &&
        string_is(member(*metadata, "dynamic_alias_mode"),
                  "TCalc-opcodes1380-1383-DYNAINFO-selectors7-14-17-24-public-L1-broadcast") &&
        string_is(member(*metadata, "dynamic_quote_command"), "0x054C") &&
        string_is(member(*metadata, "dynamic_speed_command"), "0x053E") &&
        alias_matches("DYNA_NOW", 7.0, 1380.0, "0x054C") &&
        alias_matches("DYNA_ZAF", 14.0, 1381.0, "0x054C") &&
        alias_matches("DYNA_LB", 17.0, 1382.0, "0x054C") &&
        alias_matches("DYNA_ZAS", 24.0, 1383.0, "0x053E");
    add_assertion(result, "market_breadth_dyna_provenance", provenance,
                  "SZ399001 K-line offsets31/33 plus public 0x054C and 0x053E quote selectors",
                  provenance);

    bool values_match = false;
    if (const auto* points = member(document, "points");
        points && points->is_array() && points->size() == 120) {
        const auto* first = member(points->as_array().front(), "values");
        const auto first_now = first ? numeric_value(member(*first, "N"))
                                     : std::nullopt;
        const auto first_zaf = first ? numeric_value(member(*first, "Z"))
                                     : std::nullopt;
        const auto first_lb = first ? numeric_value(member(*first, "L"))
                                    : std::nullopt;
        const auto first_zas = first ? numeric_value(member(*first, "S"))
                                     : std::nullopt;
        values_match = first_now && first_zaf && first_lb && first_zas &&
            std::isfinite(*first_now) && *first_now > 0.0 &&
            std::isfinite(*first_zaf) && std::isfinite(*first_lb) &&
            std::isfinite(*first_zas);
        for (const auto& point : points->as_array()) {
            const auto* values = member(point, "values");
            if (!values) { values_match = false; break; }
            const auto ia = numeric_value(member(*values, "IA"));
            const auto iac = numeric_value(member(*values, "IAC"));
            const auto id = numeric_value(member(*values, "ID"));
            const auto idc = numeric_value(member(*values, "IDC"));
            const auto now = numeric_value(member(*values, "N"));
            const auto now_call = numeric_value(member(*values, "NC"));
            const auto zaf = numeric_value(member(*values, "Z"));
            const auto lb = numeric_value(member(*values, "L"));
            const auto zas = numeric_value(member(*values, "S"));
            values_match = values_match && ia && iac && id && idc && now &&
                now_call && zaf && lb && zas && std::isfinite(*ia) &&
                std::isfinite(*id) && *ia >= 0.0 && *id >= 0.0 &&
                *ia == *iac && *id == *idc && *now == *now_call &&
                *now == *first_now && *zaf == *first_zaf &&
                *lb == *first_lb && *zas == *first_zas;
            if (!values_match) break;
        }
    }
    add_assertion(result, "native_market_breadth_dyna_values", values_match,
                  "120 aligned breadth points and four finite broadcast quote aliases",
                  values_match);
}

void validate_formula_security_relation_divfactor_inline_post(const Json& document, Json& result) {
    const bool identity =
        string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
        string_is(member(document, "execution_mode"), "native-cpp") &&
        string_is(member(document, "formula_source_mode"), "inline-post") &&
        string_is(member(document, "formula"),
                  "CONTRACT_SECURITY_RELATION_DIVFACTOR") &&
        string_is(member(document, "market"), "sh") &&
        string_is(member(document, "code"), "110075");
    add_assertion(result, "native_formula_identity", identity,
                  "inline-post native-cpp security relation formula for SH110075",
                  identity);

    const auto* analysis = member(document, "analysis");
    std::set<std::string> dependencies, unavailable;
    if (analysis) {
        if (const auto* values = member(
                *analysis, "automatic_context_dependencies");
            values && values->is_array())
            for (const auto& value : values->as_array())
                if (value.is_string()) dependencies.insert(value.as_string());
        if (const auto* values = member(
                *analysis, "context_bindings_unavailable");
            values && values->is_array())
            for (const auto& value : values->as_array())
                if (value.is_string()) unavailable.insert(value.as_string());
    }
    const bool contextual = analysis &&
        bool_is(member(*analysis, "syntax_supported"), true) &&
        bool_is(member(*analysis, "context_bindable"), true) &&
        bool_is(member(*analysis, "executable_with_context"), true) &&
        dependencies == std::set<std::string>{
            "DIVFACTOR", "DPZSCODE", "DPZSNAME", "UNDERCODE", "UNDERLYC"} &&
        unavailable.empty();
    add_assertion(result, "security_relation_divfactor_analysis", contextual,
                  "five exact automatic public dependencies with no unavailable binding",
                  contextual);

    const auto* metadata = member(document, "context_metadata");
    const auto* index = metadata ? member(*metadata, "main_index_identity") : nullptr;
    const auto* underlying = metadata ? member(*metadata, "underlying_identity") : nullptr;
    const bool provenance = index && underlying &&
        string_is(member(*index, "security"), "sh:999999") &&
        string_is(member(*index, "code"), "999999") &&
        string_is(member(*index, "name"), "上证指数") &&
        string_is(member(*index, "mode"),
                  "TCalc-sub_10044AA0-sub_10044C00-exact-market-code-selection") &&
        bool_is(member(*underlying, "available"), true) &&
        number_is(member(*underlying, "market_id"), 1.0) &&
        string_is(member(*underlying, "code"), "600029") &&
        string_is(member(*underlying, "source"),
                  "T0002/hq_cache/speckzzdata.txt-client-convertible-master") &&
        string_is(member(*underlying, "host_layout"),
                  "type120-market-u16-at176-code-6bytes-at178") &&
        string_is(member(*underlying, "close_mode"),
                  "TCalc-opcode1320-date-time-aligned-35-byte-kline-close-offset19") &&
        string_is(member(*metadata, "divfactor_mode"),
                  "TCalc-opcode1359-type164-category1-offset21-bonus-only-float32") &&
        numeric_value(member(*metadata, "divfactor_event_date_count")).has_value() &&
        numeric_value(member(*metadata, "divfactor_matched_bar_count")).has_value();
    add_assertion(result, "security_relation_divfactor_provenance", provenance,
                  "native main-index constants, type120 underlying layout, local convertible master and public 0x000F factors",
                  provenance);

    bool values_match = false;
    if (const auto* points = member(document, "points");
        points && points->is_array() && points->size() == 120) {
        values_match = true;
        std::size_t positive_underlying = 0;
        for (const auto& point : points->as_array()) {
            const auto* values = member(point, "values");
            if (!values) { values_match = false; break; }
            const auto dp_code = numeric_value(member(*values, "DPC"));
            const auto dp_name = numeric_value(member(*values, "DPN"));
            const auto under_code = numeric_value(member(*values, "UC"));
            const auto under_code_call = numeric_value(member(*values, "UCC"));
            const auto close = numeric_value(member(*values, "U"));
            const auto close_call = numeric_value(member(*values, "U2"));
            const auto front = numeric_value(member(*values, "F"));
            const auto back = numeric_value(member(*values, "H"));
            values_match = dp_code && dp_name && under_code &&
                under_code_call && close && close_call && front && back &&
                *dp_code == 1.0 && *dp_name == 1.0 && *under_code == 1.0 &&
                *under_code_call == 1.0 && *close == *close_call &&
                std::isfinite(*front) && *front > 0.0 &&
                std::isfinite(*back) && *back > 0.0;
            if (!values_match) break;
            if (*close > 0.0) ++positive_underlying;
        }
        values_match = values_match && positive_underlying >= 100;
    }
    add_assertion(result, "native_security_relation_divfactor_values", values_match,
                  "120 relation/factor points and at least 100 aligned positive underlying closes",
                  values_match);
}

struct DataContract {
    std::string_view id;
    DataContractValidator validate;
};

constexpr std::array<DataContract, 4> data_contracts{{
    {"formula-single-point-data-inline-post", validate_formula_single_point_data_inline_post},
    {"formula-type167-text-inline-post", validate_formula_type167_text_inline_post},
    {"formula-market-breadth-dyna-inline-post", validate_formula_market_breadth_dyna_inline_post},
    {"formula-security-relation-divfactor-inline-post", validate_formula_security_relation_divfactor_inline_post},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < data_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < data_contracts.size(); ++right)
            if (data_contracts[left].id == data_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_formula_runtime_data_contract(const std::string& contract_id,
    const Json& document, Json& result) {
    for (const auto& contract : data_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail