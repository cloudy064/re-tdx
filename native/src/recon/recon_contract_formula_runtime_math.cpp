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

using MathContractValidator = void (*)(const Json& document, Json& result);

void validate_formula_custom_core_inline_post(const Json& document, Json& result) {
    const bool identity =
        string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
        string_is(member(document, "execution_mode"), "native-cpp") &&
        string_is(member(document, "formula_source_mode"), "inline-post") &&
        string_is(member(document, "formula"), "CONTRACT_CUSTOM_CORE");
    add_assertion(result, "native_formula_identity", identity,
                  "inline-post native-cpp CONTRACT_CUSTOM_CORE", identity);
    const auto* analysis = member(document, "analysis");
    const bool executable = analysis &&
        bool_is(member(*analysis, "syntax_supported"), true) &&
        bool_is(member(*analysis, "executable"), true) &&
        bool_is(member(*analysis, "has_machine_clock_dependency"), true) &&
        bool_is(member(*analysis, "pure_ohlcv"), false);
    add_assertion(result, "custom_core_analysis", executable, true, executable);

    const auto* points = member(document, "points");
    bool dates = false, status = false, fixed = false, numeric_core = false;
    bool reverse_core = false, machine_clock = false;
    if (points && points->is_array() && points->size() == 120) {
        const auto& first = points->as_array().front();
        const auto& last = points->as_array().back();
        const auto* first_values = member(first, "values");
        const auto* last_values = member(last, "values");
        const auto* date = member(last, "date");
        if (first_values && last_values && date && date->is_string() &&
            date->as_string().size() >= 10) {
            try {
                const auto year = std::stoi(date->as_string().substr(0, 4));
                const auto month = std::stoi(date->as_string().substr(5, 2));
                const auto day = std::stoi(date->as_string().substr(8, 2));
                const auto weekday = numeric_value(member(*last_values, "WD"));
                dates = number_is(member(*last_values, "YR"), year) &&
                    number_is(member(*last_values, "MO"), month) &&
                    number_is(member(*last_values, "DY"), day) && weekday &&
                    *weekday >= 0.0 && *weekday <= 6.0;
            } catch (...) {}
            status = number_is(member(*first_values, "BS"), 1.0) &&
                number_is(member(*last_values, "BS"), 2.0) &&
                number_is(member(*first_values, "TB"), 120.0) &&
                number_is(member(*last_values, "TB"), 120.0);
            const auto raw_last = numeric_value(member(*last_values, "RAW"));
            const auto fixed_first = numeric_value(member(*first_values, "FIXED"));
            const auto fixed_last = numeric_value(member(*last_values, "FIXED"));
            fixed = raw_last && fixed_first && fixed_last &&
                std::abs(*raw_last - *fixed_first) < 0.000001 &&
                std::abs(*raw_last - *fixed_last) < 0.000001;
            const auto inside = numeric_value(member(*last_values, "IN"));
            const auto fraction = numeric_value(member(*last_values, "FR"));
            const auto sign = numeric_value(member(*last_values, "SG"));
            const auto trig = numeric_value(member(*last_values, "TRIG"));
            numeric_core = inside && *inside == 1.0 && fraction &&
                *fraction > -1.0 && *fraction < 1.0 && sign && *sign == 1.0 &&
                trig && std::abs(*trig - (1.0 + 3.14159265358979323846 * 0.75)) <
                            0.00001;
            const auto back2 = numeric_value(member(*last_values, "BACK2"));
            const auto consta_first = numeric_value(member(*first_values, "CA"));
            const auto consta_last = numeric_value(member(*last_values, "CA"));
            const auto rounded = numeric_value(member(*last_values, "R2"));
            const auto reverse_exists = numeric_value(member(*last_values, "EV"));
            reverse_core = back2 && consta_first && consta_last && rounded &&
                reverse_exists && std::abs(*back2 - *consta_first) < 0.000001 &&
                std::abs(*back2 - *consta_last) < 0.000001 &&
                std::abs(*rounded - 1.24) < 0.000001 && *reverse_exists == 1.0;
            const auto machine_date = numeric_value(member(*last_values, "MD"));
            const auto machine_time = numeric_value(member(*last_values, "MT"));
            const auto machine_week = numeric_value(member(*last_values, "MW"));
            const auto first_machine_date = numeric_value(member(*first_values, "MD"));
            const auto first_machine_time = numeric_value(member(*first_values, "MT"));
            const auto first_machine_week = numeric_value(member(*first_values, "MW"));
            if (machine_date && machine_time && machine_week &&
                first_machine_date && first_machine_time && first_machine_week) {
                const int encoded_time = static_cast<int>(*machine_time);
                const int hour = encoded_time / 10000;
                const int minute = (encoded_time / 100) % 100;
                const int second = encoded_time % 100;
                const int response_seconds = hour * 3600 + minute * 60 + second;
                const auto matches_snapshot = [&](std::time_t stamp) {
                    std::tm local{};
#ifdef _WIN32
                    localtime_s(&local, &stamp);
#else
                    localtime_r(&stamp, &local);
#endif
                    const int expected_date = local.tm_year * 10000 +
                        (local.tm_mon + 1) * 100 + local.tm_mday;
                    const int expected_seconds = local.tm_hour * 3600 +
                        local.tm_min * 60 + local.tm_sec;
                    const int direct = std::abs(response_seconds - expected_seconds);
                    const int distance = std::min(direct, 86400 - direct);
                    return static_cast<int>(*machine_date) == expected_date &&
                           static_cast<int>(*machine_week) == local.tm_wday &&
                           distance <= 120;
                };
                const auto now = std::time(nullptr);
                machine_clock = hour >= 0 && hour <= 23 &&
                    minute >= 0 && minute <= 59 && second >= 0 && second <= 59 &&
                    (*machine_date == *first_machine_date) &&
                    (*machine_time == *first_machine_time) &&
                    (*machine_week == *first_machine_week) &&
                    (matches_snapshot(now) || matches_snapshot(now - 120));
            }
        }
    }
    add_assertion(result, "native_bar_date_symbols", dates,
                  "YEAR/MONTH/DAY match latest bar; WEEKDAY in 0..6", dates);
    add_assertion(result, "native_bar_status", status,
                  "BARSTATUS first=1,last=2; TOTALBARSCOUNT=120", status);
    add_assertion(result, "native_const", fixed,
                  "CONST(CLOSE) repeats final CLOSE", fixed);
    add_assertion(result, "native_math_core", numeric_core,
                  "RANGE/FRACPART/SIGN and trig native results", numeric_core);
    add_assertion(result, "native_reverse_utility_core", reverse_core,
                  "CONSTA final offset, ROUND2 bias and EXISTR reverse window",
                  reverse_core);
    add_assertion(result, "native_machine_clock", machine_clock,
                  "MACHINEDATE/HHMMSS/Sunday-zero weekday match one broadcast local snapshot",
                  machine_clock);
}

void validate_formula_sequence_statistics_inline_post(const Json& document, Json& result) {
    const bool identity =
        string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
        string_is(member(document, "execution_mode"), "native-cpp") &&
        string_is(member(document, "formula_source_mode"), "inline-post") &&
        string_is(member(document, "formula"), "CONTRACT_SEQUENCE_STATS");
    add_assertion(result, "native_formula_identity", identity,
                  "inline-post native-cpp CONTRACT_SEQUENCE_STATS", identity);
    const auto* analysis = member(document, "analysis");
    const bool future_analysis = analysis &&
        bool_is(member(*analysis, "syntax_supported"), true) &&
        bool_is(member(*analysis, "has_future_function"), true) &&
        bool_is(member(*analysis, "read_only_future_executable"), true);
    add_assertion(result, "sequence_statistics_analysis", future_analysis,
                  "future members admitted only in explicit read-only mode",
                  future_analysis);

    const auto* points = member(document, "points");
    bool occurrence = false, filterx = false, recurrence = false;
    bool statistics = false, ranked_search = false, transforms = false;
    bool capital_turnover = false;
    if (points && points->is_array() && points->size() == 120) {
        const auto& values117 = points->as_array()[116].at("values");
        const auto& values118 = points->as_array()[117].at("values");
        const auto& values119 = points->as_array()[118].at("values");
        const auto& last = points->as_array().back().at("values");
        occurrence = number_is(member(last, "BL"), 3.0) &&
            number_is(member(last, "BSN"), 3.0);
        filterx = number_is(member(values117, "FX"), 0.0) &&
            number_is(member(values119, "FX"), 1.0);
        const auto tma = numeric_value(member(last, "TM"));
        recurrence = tma && std::isfinite(*tma) &&
            number_is(member(last, "XM"), 119.5);
        const auto covariance = numeric_value(member(last, "CV"));
        const auto correlation = numeric_value(member(last, "RL"));
        const auto betaex = numeric_value(member(last, "BE"));
        statistics = covariance && correlation && betaex &&
            std::abs(*covariance - 5.0) < 0.0001 &&
            std::abs(*correlation - 1.0) < 0.0001 &&
            std::abs(*betaex - 0.5) < 0.0001;
        ranked_search = number_is(member(last, "FH"), 118.0) &&
            number_is(member(last, "FHB"), 2.0) &&
            number_is(member(last, "FL"), 116.0) &&
            number_is(member(last, "FLB"), 4.0);
        const auto newsar = numeric_value(member(last, "NS"));
        const auto transformed_tail = numeric_value(member(last, "FT"));
        transforms = number_is(member(values117, "FT"), 474.0) &&
            number_is(member(values118, "FT"), -4.0) &&
            number_is(member(values119, "FT"), -2.0) &&
            transformed_tail && std::abs(*transformed_tail) < 0.0001 &&
            newsar && std::isfinite(*newsar);
        const auto lfs = numeric_value(member(last, "LF"));
        const auto* context_metadata = member(document, "context_metadata");
        capital_turnover = lfs && std::isfinite(*lfs) && context_metadata &&
            string_is(member(*context_metadata, "capital_series_mode"),
                      "tdx-0x000f-historical");
    }
    add_assertion(result, "reverse_occurrence_windows", occurrence,
                  "BARSLASTS=3 and BARSSINCEN=3", occurrence);
    add_assertion(result, "native_filterx", filterx,
                  "later signal retained and preceding two bars cleared", filterx);
    add_assertion(result, "native_tma_xma", recurrence,
                  "TMA finite and centered XMA boundary=119.5", recurrence);
    add_assertion(result, "native_covariance_family", statistics,
                  "COVAR=5, RELATE=1, BETAEX=0.5", statistics);
    add_assertion(result, "native_ranked_search", ranked_search,
                  "FINDHIGH/FINDLOW values and bar distances", ranked_search);
    add_assertion(result, "native_fftrans_newsar", transforms,
                  "FFTRANS final [474,-4,-2,0] and NEWSAR finite",
                  transforms);
    add_assertion(result, "native_lfs_capital_turnover", capital_turnover,
                  "LFS finite with date-effective 0x000f circulating-capital context",
                  capital_turnover);
}

void validate_formula_rolling_variance_inline_post(const Json& document, Json& result) {
    const bool identity =
        string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
        string_is(member(document, "execution_mode"), "native-cpp") &&
        string_is(member(document, "formula_source_mode"), "inline-post") &&
        string_is(member(document, "formula"), "CONTRACT_ROLLING_VARIANCE");
    add_assertion(result, "native_formula_identity", identity,
                  "inline-post native-cpp CONTRACT_ROLLING_VARIANCE", identity);
    const auto* analysis = member(document, "analysis");
    const bool executable = analysis &&
        bool_is(member(*analysis, "syntax_supported"), true) &&
        bool_is(member(*analysis, "executable"), true) &&
        bool_is(member(*analysis, "has_future_function"), false);
    add_assertion(result, "rolling_variance_analysis", executable,
                  "pure current/history custom source", executable);

    bool reference = false, rolling = false, ranks = false;
    bool branches = false, sample_statistics = false;
    bool population_statistics = false, log_volatility = false;
    if (const auto* points = member(document, "points");
        points && points->is_array() && points->size() == 120) {
        const auto& p0 = points->as_array()[0].at("values");
        const auto& p1 = points->as_array()[1].at("values");
        const auto& p2 = points->as_array()[2].at("values");
        const auto& p3 = points->as_array()[3].at("values");
        const auto& p4 = points->as_array()[4].at("values");
        const auto close_to = [](const Json* value, double expected,
                                 double tolerance = 0.0001) {
            const auto number = numeric_value(value);
            return number && std::abs(*number - expected) < tolerance;
        };
        reference = number_is(member(p0, "RV"), 1.0) &&
            number_is(member(p1, "RV"), 1.0) &&
            number_is(member(p2, "RV"), 1.0) &&
            number_is(member(p3, "RV"), 8.0);
        rolling = number_is(member(p3, "MU"), 128.0) &&
            close_to(member(p3, "AM"), 6.203125, 0.00001);
        ranks = number_is(member(p4, "HR"), 3.0) &&
            number_is(member(p4, "LR"), 1.0) &&
            number_is(member(p4, "HV"), 16.0) &&
            number_is(member(p4, "LV"), 8.0);
        branches = number_is(member(p0, "IV"), 0.0) &&
            number_is(member(p2, "IV"), 1.0) &&
            number_is(member(p0, "IT"), 20.0) &&
            number_is(member(p2, "IT"), 10.0) &&
            number_is(member(p0, "IN"), 10.0) &&
            number_is(member(p2, "IN"), 20.0) &&
            number_is(member(p0, "N"), 1.0) &&
            number_is(member(p2, "N"), 0.0) &&
            number_is(member(p2, "MX"), 6.0) &&
            number_is(member(p2, "MN"), 1.0);
        sample_statistics = close_to(member(p2, "DS"), 86.0 / 3.0) &&
            close_to(member(p2, "VS"), 43.0 / 3.0) &&
            close_to(member(p2, "SS"), std::sqrt(43.0 / 3.0));
        population_statistics = number_is(member(p2, "VP"), 0.0) &&
            number_is(member(p2, "SP"), 0.0) &&
            close_to(member(p3, "VP"), 296.0 / 9.0) &&
            close_to(member(p3, "SP"), std::sqrt(296.0) / 3.0);
        const auto* early_volatility = member(p2, "DV");
        log_volatility = early_volatility && early_volatility->is_null() &&
            close_to(member(p3, "DV"), std::log(2.0) / 2.0, 0.00001);
    }
    add_assertion(result, "native_refv", reference,
                  "REFV variable-offset smoothing = 1,1,1,8", reference);
    add_assertion(result, "native_mular_ama", rolling,
                  "MULAR=128 and AMA=6.203125", rolling);
    add_assertion(result, "native_rank_interval", ranks,
                  "HOD/LOD=3/1 and HHVLLV=16/8", ranks);
    add_assertion(result, "native_condition_extrema", branches,
                  "ISVALID/IFF/IFN/NOT/MAX6/MIN6", branches);
    add_assertion(result, "native_sample_statistics", sample_statistics,
                  "DEVSQ=86/3, VAR=43/3, STD=sqrt(43/3)",
                  sample_statistics);
    add_assertion(result, "native_population_statistics", population_statistics,
                  "VARP/STDP warmup zero then 296/9 and sqrt(296)/3",
                  population_statistics);
    add_assertion(result, "native_stddev_log_returns", log_volatility,
                  "first output lagged; log-return volatility=log(2)/2",
                  log_volatility);
}

void validate_formula_benchmark_cumulative_inline_post(const Json& document, Json& result) {
    const bool identity =
        string_is(member(document, "engine"), "tdx-source-interpreter-v1") &&
        string_is(member(document, "execution_mode"), "native-cpp") &&
        string_is(member(document, "formula_source_mode"), "inline-post") &&
        string_is(member(document, "formula"), "CONTRACT_BENCHMARK_CUMULATIVE");
    add_assertion(result, "native_formula_identity", identity,
                  "inline-post native-cpp CONTRACT_BENCHMARK_CUMULATIVE", identity);
    const auto* analysis = member(document, "analysis");
    bool beta_external = false;
    if (analysis) {
        const auto* dependencies = member(*analysis, "automatic_context_dependencies");
        if (dependencies && dependencies->is_array())
            for (const auto& value : dependencies->as_array())
                if (value.is_string() && value.as_string() == "BETA") beta_external = true;
    }
    const bool contextual = analysis &&
        bool_is(member(*analysis, "syntax_supported"), true) &&
        bool_is(member(*analysis, "executable"), false) &&
        bool_is(member(*analysis, "executable_with_context"), true) &&
        bool_is(member(*analysis, "requires_automatic_market_context"), true) &&
        beta_external;
    add_assertion(result, "beta_context_analysis", contextual,
                  "BETA is automatic-context executable", contextual);

    bool benchmark_binding = false;
    if (const auto* bindings = member(document, "context_bindings");
        bindings && bindings->is_array())
        for (const auto& value : bindings->as_array())
            if (value.is_string() && value.as_string() == "__BETA_BENCHMARK_CLOSE")
                benchmark_binding = true;
    bool benchmark_metadata = false;
    if (const auto* metadata = member(document, "context_metadata");
        metadata && metadata->is_object())
        if (const auto* benchmark = member(*metadata, "beta_benchmark");
            benchmark && benchmark->is_object())
            benchmark_metadata =
                string_is(member(*benchmark, "security"), "sz:399001") &&
                string_is(member(*benchmark, "mode"),
                          "TCalc-sub_10023F40-native-security-market-benchmark-selection");
    add_assertion(result, "native_beta_benchmark", benchmark_binding && benchmark_metadata,
                  "sz:000001 -> sz:399001 aligned CLOSE", benchmark_binding && benchmark_metadata);

    bool cumulative = false, insufficient = false, beta_value = false;
    if (const auto* points = member(document, "points");
        points && points->is_array() && points->size() == 120) {
        const auto& first = points->as_array().front().at("values");
        const auto& second = points->as_array()[1].at("values");
        const auto& last = points->as_array().back().at("values");
        cumulative = number_is(member(first, "SB"), 0.0) &&
            number_is(member(first, "SX"), -1.0) &&
            number_is(member(second, "SB"), 1.0) &&
            number_is(member(second, "SX"), 1.0) &&
            number_is(member(last, "SB"), 5.0) &&
            number_is(member(last, "SX"), 4.0);
        const auto* early_missing = member(first, "NX");
        const auto* last_missing = member(last, "NX");
        insufficient = number_is(member(first, "NB"), 0.0) &&
            number_is(member(last, "NB"), 119.0) &&
            early_missing && early_missing->is_null() &&
            last_missing && last_missing->is_null();
        if (const auto beta = numeric_value(member(last, "BT")); beta)
            beta_value = std::isfinite(*beta) && std::abs(*beta) < 20.0;
    }
    add_assertion(result, "native_sumbars_distance", cumulative,
                  "SUMBARS=0,1,...,5; SUMBARSX=-1,1,...,4", cumulative);
    add_assertion(result, "native_sumbars_insufficient", insufficient,
                  "SUMBARS falls back to 119 while SUMBARSX stays missing", insufficient);
    add_assertion(result, "native_beta_output", beta_value,
                  "latest 20-period BETA is finite", beta_value);
}

struct MathContract {
    std::string_view id;
    MathContractValidator validate;
};

constexpr std::array<MathContract, 4> math_contracts{{
    {"formula-custom-core-inline-post", validate_formula_custom_core_inline_post},
    {"formula-sequence-statistics-inline-post", validate_formula_sequence_statistics_inline_post},
    {"formula-rolling-variance-inline-post", validate_formula_rolling_variance_inline_post},
    {"formula-benchmark-cumulative-inline-post", validate_formula_benchmark_cumulative_inline_post},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < math_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < math_contracts.size(); ++right)
            if (math_contracts[left].id == math_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_formula_runtime_math_contract(const std::string& contract_id,
    const Json& document, Json& result) {
    for (const auto& contract : math_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail