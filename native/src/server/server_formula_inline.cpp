#include "server_formula_support_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/formula_context.hpp"
#include "tdx/formula_engine.hpp"
#include "tdx/formula_render_profile.hpp"
#include "tdx/level2.hpp"
#include "tdx/minute.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>

namespace tdx::server_detail {

namespace {

struct FutureReplayPlan {
    std::size_t max_observations{20};
    std::size_t max_events{1000};
};

std::size_t future_replay_integer(const Json& value, std::string_view name,
                                  std::size_t minimum, std::size_t maximum) {
    if (!value.is_number() || std::floor(value.as_number()) != value.as_number() ||
        value.as_number() < static_cast<double>(minimum) ||
        value.as_number() > static_cast<double>(maximum))
        throw Error(std::string("future_replay.") + std::string(name) +
                    " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    return static_cast<std::size_t>(value.as_number());
}

std::optional<FutureReplayPlan> parse_future_replay_plan(const Json& body) {
    const auto* replay = json_member(body, "future_replay");
    if (!replay) return std::nullopt;
    if (!replay->is_object()) throw Error("future_replay must be an object");
    for (const auto& [key, value] : replay->as_object()) {
        (void)value;
        if (key != "max_observations" && key != "max_events")
            throw Error("unknown future_replay field: " + key);
    }
    FutureReplayPlan result;
    if (const auto* value = json_member(*replay, "max_observations"))
        result.max_observations = future_replay_integer(
            *value, "max_observations", 1, 64);
    if (const auto* value = json_member(*replay, "max_events"))
        result.max_events = future_replay_integer(*value, "max_events", 1, 10000);
    return result;
}

} // namespace

void merge_inline_formula_context(Json& target, const Json& supplied) {
    if (!supplied.is_object()) throw Error("context must be an object");
    if (!target.is_object()) target = Json::object();
    for (const auto& [key, value] : supplied.as_object()) {
        auto found = target.as_object().find(key);
        if (found != target.as_object().end() && found->second.is_object() &&
            value.is_object())
            merge_inline_formula_context(found->second, value);
        else
            target[key] = value;
    }
}

bool inline_formula_needs_market_context(const Json& analysis) {
    if (const auto* automatic = json_member(analysis, "automatic_context_dependencies");
        automatic && automatic->is_array())
        return automatic->size() != 0;
    const auto* dependencies = json_member(analysis, "external_dependencies");
    if (!dependencies || !dependencies->is_array()) return false;
    for (const auto& dependency : dependencies->as_array())
        if (dependency.is_string() && dependency.as_string() != "SIGNALS_QS")
            return true;
    return false;
}

bool inline_formula_supplied_market_context_ready(const Json& analysis,
                                                  const Json& context) {
    const auto* bindable = json_member(analysis, "context_bindable");
    const auto* required = json_member(analysis, "context_bindings_required");
    const auto* dependencies = json_member(analysis, "automatic_context_dependencies");
    if (!bindable || !bindable->is_bool() || !bindable->as_bool() ||
        !required || !required->is_array() ||
        !dependencies || !dependencies->is_array() ||
        !context.is_object()) return false;

    // The scalar groups below have a stable public request representation and
    // are consumed directly by the formula evaluator.  Other automatically
    // recovered dependencies can carry richer point-in-time semantics, so do
    // not let callers suppress their resolver merely by providing a marker.
    for (const auto& dependency : dependencies->as_array()) {
        if (!dependency.is_string()) return false;
        const auto name = dependency.as_string();
        if (name != "FINANCE" && name != "FINVALUE" && name != "DYNAINFO")
            return false;
    }
    for (const auto& binding : required->as_array()) {
        if (!binding.is_string()) return false;
        const auto& name = binding.as_string();
        const auto separator = name.find('#');
        if (separator == std::string::npos || separator == 0 ||
            separator + 1 == name.size()) return false;
        const auto group = lower_ascii(name.substr(0, separator));
        if (group != "finance" && group != "finvalue" && group != "dynainfo")
            return false;
        const auto* values = json_member(context, group);
        if (!values || !values->is_object()) return false;
        const auto found = values->as_object().find(name.substr(separator + 1));
        if (found == values->as_object().end() || !found->second.is_number() ||
            !std::isfinite(found->second.as_number())) return false;
    }
    return true;
}

bool inline_formula_disables_automatic_context(const Json* supplied) {
    if (!supplied || !supplied->is_object()) return false;
    const auto* marker = json_member(*supplied, "automatic_market_context");
    return marker && marker->is_bool() && !marker->as_bool();
}

InlineFormulaRequest parse_inline_formula_request(const Json& body,
                                                  const std::string& formula_kind) {
    if (!body.is_object()) throw Error("formula request body must be a JSON object");
    InlineFormulaRequest request;
    request.source = json_body_string(body, "source");
    auto& source = request.source;
    if (trim(source).empty()) throw Error("source is required");
    if (source.size() > maximum_formula_source_bytes)
        throw Error("source exceeds the 16 KiB safety limit");
    request.code = trim(json_body_string(body, "formula", "CUSTOM"));
    auto& formula_code = request.code;
    if (formula_code.empty() || formula_code.size() > 64 ||
        std::any_of(formula_code.begin(), formula_code.end(), [](unsigned char ch) {
            return ch < 0x20 || ch == 0x7f;
        }))
        throw Error("formula must be a non-empty name of at most 64 bytes");

    if (const auto* values = json_member(body, "parameters")) {
        if (!values->is_object()) throw Error("parameters must be an object");
        if (values->size() > 64) throw Error("parameters may contain at most 64 entries");
        for (const auto& [raw_name, value] : values->as_object()) {
            const auto name = formula_upper_ascii(trim(raw_name));
            const bool valid_name = !name.empty() && name.size() <= 64 &&
                (std::isalpha(static_cast<unsigned char>(name.front())) || name.front() == '_') &&
                std::all_of(name.begin() + 1, name.end(), [](unsigned char ch) {
                    return std::isalnum(ch) || ch == '_';
                });
            if (!valid_name) throw Error("invalid formula parameter name: " + raw_name);
            if (!value.is_number() || !std::isfinite(value.as_number()) ||
                std::abs(value.as_number()) > 1000000000.0)
                throw Error("formula parameter " + raw_name + " must be a finite number");
            if (!request.parameters.emplace(name, value.as_number()).second)
                throw Error("duplicate formula parameter: " + raw_name);
        }
    }
    request.definition = make_formula_source_definition(
        source, formula_code, formula_kind, request.parameters);
    request.analysis = request.definition.at("analysis");
    if (!request.analysis.at("syntax_supported").as_bool()) {
        const auto* reason = json_member(request.analysis, "reason");
        throw Error("formula source rejected: " +
                    (reason && reason->is_string() ? reason->as_string()
                                                   : std::string("unsupported syntax")));
    }
    return request;
}

Json query_inline_formula_execution(const FormulaHttpState& state,
                                    const RequestTarget& original_target,
                                    const Json& body,
                                    const std::string& formula_kind = "technical",
                                    const std::string& source_mode = "inline-post") {
    RequestTarget target = original_target;
    merge_formula_request_target(target, body);
    merge_formula_adjustment_target(target, body);
    auto request = parse_inline_formula_request(body, formula_kind);
    const auto& source = request.source;
    const auto& formula_code = request.code;
    const auto& parameters = request.parameters;
    const auto& analysis = request.analysis;

    const auto [market, code] = query_kline_security(target);
    const auto kind = lower_ascii(trim(query_value(target, "kind", "auto")));
    const auto period = lower_ascii(trim(query_value(target, "period", "day")));
    const auto date = trim(query_value(target, "date", "all"));
    const int pages = parse_bounded(query_value(target, "pages", "1"),
                                    "pages", 1, 20);
    const int page_size = parse_bounded(query_value(target, "page_size", "800"),
                                        "page_size", 1, 800);
    const int start = parse_bounded(query_value(target, "start", "0"),
                                    "start", 0, 65535);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "10000"),
                                      "timeout_ms", 100, 30000);
    const bool allow_future = query_bool(target, "allow_future");
    const bool point_in_time_finance = query_bool(target, "point_in_time_finance");
    const auto future_replay = parse_future_replay_plan(body);

    auto kline = fetch_kline_document(market, code, kind, period, pages, page_size,
                                      start, date, timeout, state.root);
    attach_security_metadata(state.block_data, kline, market, code);
    kline = apply_requested_kline_adjustment(
        state.root, target, market, code, kind, timeout, std::move(kline));
    const auto option_name = trim(query_value(target, "option_name"));
    const auto option_expiry = trim(query_value(target, "expiry"));
    if (!option_name.empty()) kline["option_name"] = option_name;
    if (!option_expiry.empty()) kline["option_expiry"] = option_expiry;
    const auto risk_free = trim(query_value(target, "risk_free"));
    if (!risk_free.empty()) {
        const auto parsed = parse_formula_parameter(risk_free, "risk_free");
        if (parsed < -1.0 || parsed > 1.0)
            throw Error("risk_free is outside the supported range");
        kline["option_risk_free"] = parsed;
    }

    Json context = Json::object();
    const bool automatic_context = inline_formula_needs_market_context(analysis);
    Json materialized_context;
    const auto* supplied_context = json_member(body, "context");
    if (supplied_context) {
        materialized_context = materialize_tcalc_level2_formula_context(
            *supplied_context, kline);
        supplied_context = &materialized_context;
    }
    const bool supplied_market_context =
        automatic_context && inline_formula_disables_automatic_context(supplied_context);
    if (supplied_market_context &&
        !inline_formula_supplied_market_context_ready(analysis, *supplied_context))
        throw Error("context with automatic_market_context=false must provide every "
                    "FINANCE/FINVALUE/DYNAINFO context_bindings_required value");
    if (automatic_context && !supplied_market_context) {
        if (market != "sz" && market != "sh" && market != "bj") {
            for (const auto& dependency : analysis.at("external_dependencies").as_array())
                if (!formula_dependency_supported_in_expansion(
                        dependency.as_string(), market))
                    throw Error("formula external dependency is unavailable for expansion markets");
        }
        context = build_formula_market_context_document(
            state.root, market, code, analysis, timeout, &state.block_data, &kline,
            point_in_time_finance, &state.formulas, state.jsn_root, nullptr,
            &parameters);
        context["automatic_market_context"] = true;
    }
    if (supplied_context)
        merge_inline_formula_context(context, *supplied_context);

    const bool future = analysis.at("has_future_function").as_bool();
    if (future) {
        if (!analysis.at("read_only_future_executable").as_bool())
            throw Error("formula future mode still requires unsupported functions or data");
        if (!allow_future)
            throw Error("formula uses a future function; enable allow_future only for read-only chart rendering");
    } else if (future_replay) {
        throw Error("future_replay requires a formula containing a future function");
    } else if (!analysis.at("executable").as_bool() &&
               !(automatic_context && analysis.at("executable_with_context").as_bool()) &&
               !formula_explicit_context_ready(analysis, &context)) {
        const auto* explicit_bindable = json_member(analysis, "explicit_context_bindable");
        if (explicit_bindable && explicit_bindable->is_bool() && explicit_bindable->as_bool())
            throw Error("formula requires every explicit_context_bindings_required key in context.series or context.formula_scalar_bindings");
        const auto* reason = json_member(analysis, "reason");
        throw Error("formula is not executable by the current interpreter" +
                    (reason && reason->is_string() ? ": " + reason->as_string() : ""));
    }

    auto result = evaluate_formula_source_document(
        future_replay ? kline : std::move(kline), source, parameters, formula_code,
        context.size() ? &context : nullptr);
    result["render_environment"] = formula_render_environment_document(state.root);
    result["formula_source_mode"] = source_mode;
    result["source_bytes"] = static_cast<std::uint64_t>(source.size());
    result["request_body_retained"] = false;
    if (future) {
        result["future_execution_mode"] = "explicit-read-only-lookahead";
        result["future_functions"] = analysis.at("future_functions");
        result["scan_allowed"] = false;
        result["backtest_allowed"] = false;
    }
    if (future_replay) {
        FormulaFutureReplayRequest replay_request;
        replay_request.kline_document = std::move(kline);
        replay_request.source = source;
        replay_request.parameters = parameters;
        replay_request.formula_code = formula_code;
        replay_request.context = context;
        replay_request.context_provided = context.size() != 0;
        replay_request.max_observations = future_replay->max_observations;
        replay_request.max_events = future_replay->max_events;
        result["future_replay"] = replay_formula_future_document(replay_request);
    }
    return result;
}

Json query_post_formula_execution(const FormulaHttpState& state,
                                  const RequestTarget& target,
                                  const Json& body) {
    if (!body.is_object()) throw Error("formula request body must be a JSON object");
    if (const auto* source = json_member(body, "source"); source) {
        if (!source->is_string()) throw Error("source must be a string");
        return query_inline_formula_execution(state, target, body);
    }

    const auto wanted_text = trim(json_body_string(body, "formula"));
    if (wanted_text.empty()) throw Error("formula is required when source is omitted");
    const auto wanted = lower_ascii(wanted_text);
    const auto wanted_kind = lower_ascii(trim(
        json_body_string(body, "formula_kind")));
    const Json* selected = nullptr;
    for (const auto& formula : state.formulas.at("formulas").as_array()) {
        if (lower_ascii(formula.at("code").as_string()) == wanted &&
            (wanted_kind.empty() || formula.at("kind_key").as_string() == wanted_kind)) {
            selected = &formula;
            break;
        }
    }
    if (!selected) throw Error("formula not found: " + wanted_text);
    const auto* source = json_member(*selected, "source_text");
    if (!source || !source->is_string())
        throw Error("selected formula source text is unavailable");

    Json expanded = body;
    expanded["formula"] = selected->at("code");
    expanded["source"] = *source;
    const auto formula_kind = selected->at("kind_key").as_string();
    auto result = query_inline_formula_execution(
        state, target, expanded, formula_kind, "library-post");
    result["library_formula_kind"] = formula_kind;
    result["library_formula_code"] = selected->at("code");
    return result;
}


} // namespace tdx::server_detail
