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

using FutureContractValidator = void (*)(const Json& document, Json& result);

void validate_formula_calendar_filter_inline_post(const Json& document, Json& result) {
    const bool identity =
        string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
        string_is(member(document, "execution_mode"), "native-cpp") &&
        string_is(member(document, "formula_source_mode"), "inline-post") &&
        string_is(member(document, "formula"), "CONTRACT_CALENDAR_FILTER");
    add_assertion(result, "native_formula_identity", identity,
                  "inline-post native-cpp CONTRACT_CALENDAR_FILTER", identity);
    const auto* analysis = member(document, "analysis");
    const bool future_read_only = analysis &&
        bool_is(member(*analysis, "syntax_supported"), true) &&
        bool_is(member(*analysis, "executable"), false) &&
        bool_is(member(*analysis, "has_external_dependency"), false) &&
        bool_is(member(*analysis, "has_future_function"), true) &&
        bool_is(member(*analysis, "read_only_future_executable"), true) &&
        bool_is(member(*analysis, "pure_ohlcv"), false);
    add_assertion(result, "calendar_filter_analysis", future_read_only,
                  "DATETOCUR isolated as context-free read-only future execution",
                  future_read_only);

    bool calendar = false, symbols = false, alignment = false,
         paired = false, four_way = false, date_to_cur = false;
    if (const auto* points = member(document, "points");
        points && points->is_array() && points->size() == 120) {
        const auto& values0 = points->as_array()[0].at("values");
        const auto& values117 = points->as_array()[117].at("values");
        const auto& values118 = points->as_array()[118].at("values");
        const auto& values119 = points->as_array()[119].at("values");
        calendar = number_is(member(values0, "D0"), 0.0) &&
            number_is(member(values0, "D1"), 1.0) &&
            number_is(member(values0, "DT"), 910101.0) &&
            number_is(member(values0, "TS"), 34200.0) &&
            number_is(member(values0, "ST"), 93000.0) &&
            number_is(member(values119, "RT"),
                      numeric_value(member(values119, "BD")).value_or(-1.0));
        const auto week = numeric_value(member(values0, "WOY"));
        symbols = week && *week >= 1.0 && *week <= 54.0 &&
            number_is(member(values0, "T2"), 150000.0);
        const auto* before_latest = member(values118, "LATEST");
        alignment = number_is(member(values117, "AR"), 2.0) &&
            number_is(member(values118, "AR"), 4.0) &&
            number_is(member(values119, "AR"), 5.0) &&
            before_latest && before_latest->is_null() &&
            number_is(member(values119, "LATEST"), 120.0);
        date_to_cur = number_is(member(values0, "DC"), 1.0) &&
            number_is(member(values119, "DC"), 120.0) &&
            number_is(member(values0, "DL"), 1.0) &&
            number_is(member(values119, "DL"), 120.0) &&
            number_is(member(values0, "DN"), 1.0) &&
            number_is(member(values119, "DN"), 120.0) &&
            number_is(member(values0, "DF"), 0.0) &&
            number_is(member(values119, "DF"), 0.0);

        constexpr double f0[]{1, 0, 2, 0, 1, 2, 1, 0, 2, 0};
        constexpr double f1[]{1, 0, 0, 0, 1, 0, 1, 0, 0, 0};
        constexpr double f2[]{0, 0, 1, 0, 1, 1, 0, 0, 1, 0};
        constexpr double t0[]{1, 0, 2, 4, 1, 2, 0, 4, 1, 0};
        constexpr double t1[]{1, 0, 0, 0, 1, 0, 0, 0, 1, 0};
        constexpr double t2[]{0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
        constexpr double t3[]{0, 0, 1, 0, 0, 1, 0, 0, 0, 0};
        constexpr double t4[]{0, 0, 0, 1, 0, 0, 0, 1, 0, 0};
        paired = true;
        four_way = true;
        for (std::size_t i = 0; i < 10; ++i) {
            const auto& values = points->as_array()[i].at("values");
            paired = paired && number_is(member(values, "F0"), f0[i]) &&
                number_is(member(values, "F1"), f1[i]) &&
                number_is(member(values, "F2"), f2[i]);
            four_way = four_way && number_is(member(values, "TT0"), t0[i]) &&
                number_is(member(values, "TT1"), t1[i]) &&
                number_is(member(values, "TT2"), t2[i]) &&
                number_is(member(values, "TT3"), t3[i]) &&
                number_is(member(values, "TT4"), t4[i]);
        }
    }
    add_assertion(result, "native_calendar_conversion", calendar,
                  "DATETODAY epoch 0 and DAYTODATE/TIMETOSEC/SECTOTIME round trips",
                  calendar);
    add_assertion(result, "native_calendar_symbols", symbols,
                  "WEEKOFYEAR in 1..54 and daily TIME2=150000", symbols);
    add_assertion(result, "native_align_tfilt", alignment,
                  "ALIGNRIGHT tail 2,4,5 and TFILT latest-date sentinel", alignment);
    add_assertion(result, "native_datetocur", date_to_cur,
                  "DATETOCUR final/missing target and complete-bar date counts",
                  date_to_cur);
    add_assertion(result, "native_tfilter", paired,
                  "TFILTER modes 0/1/2 native paired-state vector", paired);
    add_assertion(result, "native_ttfilter", four_way,
                  "TTFILTER modes 0..4 native four-way state vector", four_way);
}

void validate_formula_directional_bars_inline_post(const Json& document, Json& result) {
    const bool identity =
        string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
        string_is(member(document, "execution_mode"), "native-cpp") &&
        string_is(member(document, "formula_source_mode"), "inline-post") &&
        string_is(member(document, "formula"), "CONTRACT_DIRECTIONAL_BARS");
    add_assertion(result, "native_formula_identity", identity,
                  "inline-post native-cpp CONTRACT_DIRECTIONAL_BARS", identity);

    const auto* analysis = member(document, "analysis");
    std::set<std::string> future_names, dependencies;
    if (analysis) {
        if (const auto* values = member(*analysis, "future_functions");
            values && values->is_array())
            for (const auto& value : values->as_array())
                if (value.is_string()) future_names.insert(value.as_string());
        if (const auto* values = member(*analysis, "market_dependencies");
            values && values->is_array())
            for (const auto& value : values->as_array())
                if (value.is_string()) dependencies.insert(value.as_string());
    }
    const std::set<std::string> expected_names{
        "DCLOSE", "DHIGH", "DLOW", "DOPEN", "DVOL"};
    const std::set<std::string> expected_dependencies{
        "CLOSE", "HIGH", "LOW", "OPEN", "VOL"};
    const bool future_read_only = analysis &&
        bool_is(member(*analysis, "syntax_supported"), true) &&
        bool_is(member(*analysis, "executable"), false) &&
        bool_is(member(*analysis, "has_external_dependency"), false) &&
        bool_is(member(*analysis, "has_future_function"), true) &&
        bool_is(member(*analysis, "read_only_future_executable"), true) &&
        future_names == expected_names && dependencies == expected_dependencies;
    add_assertion(result, "directional_bar_analysis", future_read_only,
                  "five OHLCV-only entries isolated as read-only future functions",
                  future_read_only);

    bool shape = false, invariants = false;
    if (const auto* points = member(document, "points");
        points && points->is_array() && points->size() == 120) {
        shape = true;
        invariants = true;
        for (const auto& point : points->as_array()) {
            const auto* values = member(point, "values");
            const auto dh = values ? numeric_value(member(*values, "DH")) : std::nullopt;
            const auto dhf = values ? numeric_value(member(*values, "DHF")) : std::nullopt;
            const auto op = values ? numeric_value(member(*values, "DO")) : std::nullopt;
            const auto dl = values ? numeric_value(member(*values, "DL")) : std::nullopt;
            const auto dc = values ? numeric_value(member(*values, "DC")) : std::nullopt;
            const auto dv = values ? numeric_value(member(*values, "DV")) : std::nullopt;
            const auto raw = values ? numeric_value(member(*values, "R")) : std::nullopt;
            if (!dh || !dhf || !op || !dl || !dc || !dv || !raw) {
                invariants = false;
                break;
            }
            constexpr double epsilon = 0.001;
            constexpr double volume_float_epsilon = 1.0;
            invariants = invariants && std::abs(*dh - *dhf) <= epsilon &&
                *dh + epsilon >= std::max(*op, *dc) &&
                *dl - epsilon <= std::min(*op, *dc) &&
                *dh + epsilon >= *dl && *dv + volume_float_epsilon >= *raw &&
                *dv >= -epsilon;
        }
    }
    add_assertion(result, "directional_bar_shape", shape,
                  "120 complete directional OHLCV points", shape);
    add_assertion(result, "directional_bar_invariants", invariants,
                  "DLOW <= DOPEN/DCLOSE <= DHIGH; DVOL >= raw VOL; DHIGH() == DHIGH",
                  invariants);
}

void validate_formula_ziga_inline_post(const Json& document, Json& result) {
    const bool identity =
        string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
        string_is(member(document, "execution_mode"), "native-cpp") &&
        string_is(member(document, "formula_source_mode"), "inline-post") &&
        string_is(member(document, "formula"), "CONTRACT_ZIGA") &&
        string_is(member(document, "market"), "sz") &&
        string_is(member(document, "code"), "000001");
    add_assertion(result, "native_formula_identity", identity,
                  "inline-post native-cpp absolute-price ZIGA for SZ000001",
                  identity);

    const auto* analysis = member(document, "analysis");
    std::set<std::string> future_names;
    if (analysis) {
        if (const auto* values = member(*analysis, "future_functions");
            values && values->is_array())
            for (const auto& value : values->as_array())
                if (value.is_string()) future_names.insert(value.as_string());
    }
    const bool future_read_only = analysis &&
        bool_is(member(*analysis, "syntax_supported"), true) &&
        bool_is(member(*analysis, "executable"), false) &&
        bool_is(member(*analysis, "has_external_dependency"), false) &&
        bool_is(member(*analysis, "has_future_function"), true) &&
        bool_is(member(*analysis, "read_only_future_executable"), true) &&
        future_names == std::set<std::string>{"ZIGA"};
    add_assertion(result, "ziga_analysis", future_read_only,
                  "ZIGA is the sole local read-only future dependency",
                  future_read_only);

    bool shape = false, values_match = false, repainted = false;
    if (const auto* points = member(document, "points");
        points && points->is_array() && points->size() == 120) {
        shape = true;
        values_match = true;
        for (const auto& point : points->as_array()) {
            const auto close = numeric_value(member(point, "close"));
            const auto* values = member(point, "values");
            const auto raw = values ? numeric_value(member(*values, "R")) : std::nullopt;
            const auto ziga = values ? numeric_value(member(*values, "Z")) : std::nullopt;
            const auto selector = values ? numeric_value(member(*values, "S")) : std::nullopt;
            if (!close || !raw || !ziga || !selector ||
                !std::isfinite(*ziga) || !std::isfinite(*selector) ||
                std::abs(*close - *raw) > 1e-6 ||
                std::abs(*ziga - *selector) > 1e-6) {
                values_match = false;
                break;
            }
            if (std::abs(*ziga - *raw) > 1e-4) repainted = true;
        }
        const auto* first = &points->as_array().front();
        const auto* last = &points->as_array().back();
        const auto* first_values = member(*first, "values");
        const auto* last_values = member(*last, "values");
        const auto first_close = numeric_value(member(*first, "close"));
        const auto last_close = numeric_value(member(*last, "close"));
        const auto first_ziga = first_values
            ? numeric_value(member(*first_values, "Z")) : std::nullopt;
        const auto last_ziga = last_values
            ? numeric_value(member(*last_values, "Z")) : std::nullopt;
        values_match = values_match && first_close && last_close &&
            first_ziga && last_ziga &&
            std::abs(*first_close - *first_ziga) <= 1e-6 &&
            std::abs(*last_close - *last_ziga) <= 1e-6;
    }
    add_assertion(result, "ziga_shape", shape,
                  "120 complete daily ZIGA points", shape);
    add_assertion(result, "ziga_values", values_match && repainted,
                  "explicit CLOSE and selector-3 vectors match, endpoints anchor CLOSE and at least one interior bar is repainted",
                  values_match && repainted);
}

void validate_formula_random_security_score_inline_post(const Json& document, Json& result) {
    const bool identity =
        string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
        string_is(member(document, "execution_mode"), "native-cpp") &&
        string_is(member(document, "formula_source_mode"), "inline-post") &&
        string_is(member(document, "formula"),
                  "CONTRACT_RANDOM_SECURITY_SCORE") &&
        string_is(member(document, "market"), "sz") &&
        string_is(member(document, "code"), "000001");
    add_assertion(result, "native_formula_identity", identity,
                  "inline-post native-cpp RAND/SAFESCORE/SHINESCORE for sz000001",
                  identity);

    const auto* analysis = member(document, "analysis");
    std::set<std::string> dependencies, random_functions;
    if (analysis) {
        if (const auto* values = member(*analysis,
                "automatic_context_dependencies");
            values && values->is_array())
            for (const auto& value : values->as_array())
                if (value.is_string()) dependencies.insert(value.as_string());
        if (const auto* values = member(*analysis, "random_functions");
            values && values->is_array())
            for (const auto& value : values->as_array())
                if (value.is_string()) random_functions.insert(value.as_string());
    }
    const bool contextual = analysis &&
        bool_is(member(*analysis, "syntax_supported"), true) &&
        bool_is(member(*analysis, "executable"), false) &&
        bool_is(member(*analysis, "executable_with_context"), true) &&
        bool_is(member(*analysis, "has_external_dependency"), true) &&
        bool_is(member(*analysis, "has_future_function"), false) &&
        bool_is(member(*analysis, "has_random_dependency"), true) &&
        bool_is(member(*analysis, "random_seed_bindable"), true) &&
        dependencies == std::set<std::string>{"SAFESCORE", "SHINESCORE"} &&
        random_functions == std::set<std::string>{"RAND"};
    add_assertion(result, "random_score_analysis", contextual,
                  "RAND seed is bindable and two score functions are automatic local dependencies",
                  contextual);

    const auto* random = member(document, "random");
    const bool replay = random &&
        string_is(member(*random, "algorithm"), "msvc-crt-rand-lcg-v1") &&
        number_is(member(*random, "seed"), 1.0) &&
        string_is(member(*random, "seed_mode"), "explicit-context") &&
        bool_is(member(*random, "invalid_input_advances_state"), false);
    add_assertion(result, "random_replay_metadata", replay,
                  "explicit seed 1 selects the reconstructed MSVC CRT LCG",
                  replay);

    bool shape = false, values_ok = false;
    if (const auto* points = member(document, "points");
        points && points->is_array() && points->size() == 120) {
        shape = true;
        values_ok = true;
        const std::array<double, 4> prefix{2.0, 8.0, 5.0, 1.0};
        for (std::size_t index = 0; index < points->size(); ++index) {
            const auto* values = member(points->as_array()[index], "values");
            const auto random_value = values
                ? numeric_value(member(*values, "R")) : std::nullopt;
            values_ok = values_ok && random_value && *random_value >= 1.0 &&
                *random_value <= 10.0 &&
                number_is(member(*values, "SAFE"), 92.0) &&
                number_is(member(*values, "SHINE"), 7.0);
            if (index < prefix.size())
                values_ok = values_ok &&
                    std::abs(*random_value - prefix[index]) <= 1e-6;
            if (!values_ok) break;
        }
    }
    add_assertion(result, "random_score_shape", shape,
                  "120 complete RAND/security-score points", shape);
    add_assertion(result, "random_score_values", values_ok,
                  "seed-1 prefix is 2,8,5,1 and local scores broadcast 92/7",
                  values_ok);

    bool provenance = false;
    if (const auto* metadata = member(document, "context_metadata");
        metadata && metadata->is_object()) {
        const auto* source = member(*metadata, "security_score_source");
        const auto* values = member(*metadata, "security_score_values");
        const std::string suffix = "specgpext.txt";
        provenance = source && source->is_string() &&
            source->as_string().size() >= suffix.size() &&
            source->as_string().compare(source->as_string().size() - suffix.size(),
                                        suffix.size(), suffix) == 0 &&
            string_is(member(*metadata, "security_score_mode"),
                      "tdxw-type167-specgpext-fields4-5-local-reconstruction") &&
            values && bool_is(member(*values, "record_found"), true) &&
            number_is(member(*values, "safety_score"), 92.0) &&
            number_is(member(*values, "shine_score"), 7.0);
    }
    add_assertion(result, "security_score_provenance", provenance,
                  "TdxW type-167 specgpext fields 4/5 provenance is preserved",
                  provenance);
}

void validate_formula_future_replay_inline_post(const Json& document, Json& result) {
    const auto* replay = member(document, "future_replay");
    const bool identity =
        string_is(member(document, "formula"), "CONTRACT_FUTURE_REPLAY") &&
        string_is(member(document, "future_execution_mode"),
                  "explicit-read-only-lookahead") &&
        replay && replay->is_object() &&
        string_is(member(*replay, "schema"), "tdx-formula-future-replay-v1") &&
        string_is(member(*replay, "mode"), "successive-prefix-read-only") &&
        number_is(member(*replay, "max_observations"), 3.0) &&
        number_is(member(*replay, "max_events"), 100.0);
    add_assertion(result, "future_replay_identity", identity,
                  "bounded successive-prefix replay is attached to explicit future execution",
                  identity);

    bool timeline = false;
    if (replay && replay->is_object()) {
        const auto* bars = member(*replay, "source_bar_count");
        const auto* baseline = member(*replay, "baseline_bar_count");
        const auto* observations = member(*replay, "observations");
        const auto* events = member(*replay, "events");
        timeline = bars && baseline && bars->is_number() && baseline->is_number() &&
            bars->as_number() >= 4.0 &&
            baseline->as_number() == bars->as_number() - 3.0 &&
            observations && observations->is_array() && observations->size() == 3 &&
            events && events->is_array() && events->size() > 0 &&
            number_is(member(*replay, "observation_count"), 3.0) &&
            number_is(member(*replay, "evaluation_count"), 4.0) &&
            bool_is(member(*replay, "newly_appended_points_counted_as_repaint"), false);
    }
    add_assertion(result, "future_replay_timeline", timeline,
                  "three observations report only changes to pre-existing historical points",
                  timeline);

    const bool offline = replay &&
        bool_is(member(*replay, "scan_allowed"), false) &&
        bool_is(member(*replay, "backtest_allowed"), false) &&
        bool_is(member(*replay, "account_accessed"), false) &&
        bool_is(member(*replay, "orders_submitted"), false) &&
        bool_is(member(*replay, "sdk_called"), false) &&
        bool_is(member(*replay, "subscription_sent"), false) &&
        number_is(member(*replay, "network_requests"), 0.0) &&
        bool_is(member(*replay, "entitlement_bypass"), false);
    add_assertion(result, "future_replay_offline", offline,
                  "replay does not enable scan, backtest, account, order, SDK, subscription, or bypass",
                  offline);
}

void validate_formula_autofilter_trace_inline_post(
    const Json& document, Json& result) {
    const auto* trade_ir = member(document, "trade_event_ir");
    const auto* filter = trade_ir && trade_ir->is_object()
        ? member(*trade_ir, "autofilter") : nullptr;
    const auto* trace = filter && filter->is_object()
        ? member(*filter, "decision_trace") : nullptr;
    const bool summary =
        string_is(member(document, "formula"), "CONTRACT_AUTOFILTER_TRACE") &&
        filter && filter->is_object() &&
        bool_is(member(*filter, "enabled"), true) &&
        number_is(member(*filter, "decision_count"), 7.0) &&
        number_is(member(*filter, "accepted_candidate_count"), 4.0) &&
        number_is(member(*filter, "filtered_out_candidate_count"), 3.0) &&
        number_is(member(*filter, "position_change_count"), 4.0) &&
        string_is(member(*filter, "final_position"), "flat");
    add_assertion(result, "autofilter_trace_summary", summary,
                  "seven ordered decisions contain four accepted position changes and three rejections",
                  summary);

    bool ordered = false;
    if (trace && trace->is_array() && trace->size() == 7) {
        const auto& entries = trace->as_array();
        ordered = string_is(member(entries[0], "function"), "SELL") &&
            bool_is(member(entries[0], "accepted"), false) &&
            string_is(member(entries[0], "rejection_reason"), "requires-long") &&
            string_is(member(entries[1], "function"), "BUY") &&
            bool_is(member(entries[1], "accepted"), true) &&
            string_is(member(entries[1], "position_after"), "long") &&
            string_is(member(entries[2], "rejection_reason"), "already-long") &&
            string_is(member(entries[6], "rejection_reason"), "requires-short");
    }
    add_assertion(result, "autofilter_trace_order", ordered,
                  "trace preserves bar-outer and source-statement-inner decision order",
                  ordered);

    const bool offline = trade_ir &&
        bool_is(member(*trade_ir, "execution_side_effects"), false) &&
        bool_is(member(*trade_ir, "order_submission"), false) &&
        bool_is(member(*trade_ir, "account_access"), false) &&
        bool_is(member(*trade_ir, "network_access"), false) &&
        trace && trace->is_array() && trace->size() == 7 &&
        bool_is(member(trace->as_array()[6], "native_host_action"), false) &&
        bool_is(member(trace->as_array()[6], "execution_side_effects"), false);
    add_assertion(result, "autofilter_trace_offline", offline,
                  "decision explanation remains read-only with no host action, account, order, or network effects",
                  offline);
}

struct FutureContract {
    std::string_view id;
    FutureContractValidator validate;
};

constexpr std::array<FutureContract, 6> future_contracts{{
    {"formula-calendar-filter-inline-post", validate_formula_calendar_filter_inline_post},
    {"formula-directional-bars-inline-post", validate_formula_directional_bars_inline_post},
    {"formula-ziga-inline-post", validate_formula_ziga_inline_post},
    {"formula-random-security-score-inline-post", validate_formula_random_security_score_inline_post},
    {"formula-future-replay-inline-post", validate_formula_future_replay_inline_post},
    {"formula-autofilter-trace-inline-post", validate_formula_autofilter_trace_inline_post},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < future_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < future_contracts.size(); ++right)
            if (future_contracts[left].id == future_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_formula_runtime_future_contract(const std::string& contract_id,
    const Json& document, Json& result) {
    for (const auto& contract : future_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail
