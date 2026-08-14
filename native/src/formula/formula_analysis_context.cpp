#include "formula_analysis_internal.hpp"

#include "tdx/common.hpp"

#include <cstdint>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace tdx {

using namespace formula_engine_support;

namespace {

bool is_caller_host_raw_scalar_binding(const std::string& binding) {
    return binding == "HOST_TYPE120_SECURITY_CLASS_RAW" ||
           binding == "HOST_EVALUATOR_MARKET_WORD_RAW";
}

}  // namespace

Json make_formula_explicit_context_template_document(
    Json library,
    const std::vector<std::string>& formula_codes,
    const std::vector<std::string>& stamps) {
    library = analyze_formula_library_document(std::move(library));
    std::set<std::string> requested;
    for (const auto& code : formula_codes) {
        const auto value = lower_ascii(trim(code));
        if (value.empty()) throw Error("formula context template contains an empty formula code");
        requested.insert(value);
    }

    std::set<std::string> found;
    std::map<std::string, std::set<std::string>> binding_formulas;
    Json selected = Json::array();
    for (const auto& formula : library.at("formulas").as_array()) {
        const auto code = formula.at("code").as_string();
        const auto folded = lower_ascii(code);
        if (!requested.empty() && !requested.count(folded)) continue;
        if (!requested.empty()) found.insert(folded);
        const auto& analysis = formula.at("analysis");
        if (!analysis.at("explicit_context_bindable").as_bool()) {
            if (!requested.empty())
                throw Error("formula does not require caller-owned explicit context: " + code);
            continue;
        }
        selected.push_back(code);
        for (const auto& binding : analysis.at("explicit_context_bindings_required").as_array())
            binding_formulas[binding.as_string()].insert(code);
    }
    for (const auto& code : requested)
        if (!found.count(code)) throw Error("formula not found for context template: " + code);
    if (selected.as_array().empty())
        throw Error("no explicit-context formula was selected");

    std::vector<std::string> normalized_stamps;
    std::set<std::string> unique_stamps;
    for (const auto& raw : stamps) {
        const auto stamp = trim(raw);
        if (stamp.empty() || stamp.find('|') == std::string::npos)
            throw Error("--stamp must use DATE|TIME, for example 2026-08-07|15:00");
        if (unique_stamps.insert(stamp).second) normalized_stamps.push_back(stamp);
    }

    Json formula_scalar_bindings = Json::object();
    Json series = Json::object();
    Json bindings = Json::array();
    std::uint64_t scalar_binding_count = 0;
    std::uint64_t series_binding_count = 0;
    for (const auto& [binding, formulas] : binding_formulas) {
        const bool caller_host_raw_scalar =
            is_caller_host_raw_scalar_binding(binding);
        if (caller_host_raw_scalar) {
            formula_scalar_bindings[binding] = nullptr;
            ++scalar_binding_count;
        } else {
            Json values = Json::object();
            for (const auto& stamp : normalized_stamps) values[stamp] = nullptr;
            series[binding] = std::move(values);
            ++series_binding_count;
        }
        Json row = Json::object();
        row["name"] = binding;
        row["source_kind"] = caller_host_raw_scalar
            ? "caller-host-raw-scalar"
            : binding.rfind("SIGNALS_QS#", 0) == 0
                ? "broker-private"
                : formula_engine_detail::tcalc_registry_live_trading_context_names.count(binding)
                    ? "caller-account-strategy-state"
                    : "authorized-level2";
        row["value_shape"] = caller_host_raw_scalar
            ? "u16-scalar"
            : "date-time-series";
        row["formulas"] = strings_json(formulas);
        row["required"] = true;
        bindings.push_back(std::move(row));
    }

    Json metadata = Json::object();
    metadata["schema"] = "tdx-formula-explicit-context-template-v1";
    metadata["analysis_schema_version"] = library.at("analysis_schema_version");
    metadata["selected_formulas"] = std::move(selected);
    metadata["binding_count"] = static_cast<std::uint64_t>(binding_formulas.size());
    metadata["scalar_binding_count"] = scalar_binding_count;
    metadata["series_binding_count"] = series_binding_count;
    metadata["stamp_count"] = static_cast<std::uint64_t>(normalized_stamps.size());
    metadata["stamp_source"] = "explicit";
    metadata["bindings"] = std::move(bindings);
    metadata["fill_rule"] =
        "Replace null placeholders with caller-owned TCalc-compatible numeric values. "
        "formula_scalar_bindings entries are one caller-owned scalar each; series keys "
        "are exact DATE|TIME stamps from the evaluated K-line document.";
    metadata["authorization_boundary"] =
        "This template never downloads, derives, fabricates, or bypasses broker/Level2/account data; "
        "account and strategy state is caller-owned read-only input and no order action is executed.";

    Json result = Json::object();
    result["schema"] = "tdx-formula-explicit-context-v1";
    result["automatic_market_context"] = false;
    result["formula_scalar_bindings"] = std::move(formula_scalar_bindings);
    result["series"] = std::move(series);
    result["_template"] = std::move(metadata);
    return result;
}

std::vector<std::string> formula_context_stamps_from_kline(
    const Json& kline_document) {
    const auto* rows = optional(kline_document, "bars");
    if (!rows || !rows->is_array())
        throw Error("formula context template K-line document lacks a bars array");
    std::vector<std::string> result;
    std::set<std::string> seen;
    result.reserve(rows->size());
    for (const auto& row : rows->as_array()) {
        const auto* date = optional(row, "date");
        const auto* time = optional(row, "time");
        if (!date || !date->is_string() || date->as_string().empty() ||
            !time || !time->is_string() || time->as_string().empty())
            throw Error("formula context template K-line bar lacks date/time");
        const auto stamp = date->as_string() + "|" + time->as_string();
        if (seen.insert(stamp).second) result.push_back(stamp);
    }
    if (result.empty())
        throw Error("formula context template K-line document contains no bars");
    return result;
}


}  // namespace tdx
