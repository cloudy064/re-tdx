#include "server_formula_support_internal.hpp"

#include "formula_context_hk_finance_internal.hpp"
#include "formula_context_support_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/formula_calc.hpp"
#include "tdx/formula_context.hpp"
#include "tdx/formula_engine.hpp"
#include "tdx/minute.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace tdx::server_detail {

Json query_formulas(const FormulaHttpState& state, const RequestTarget& target) {
    const auto kind = lower_ascii(trim(query_value(target, "kind", "all")));
    const auto origin = lower_ascii(trim(query_value(target, "origin", "all")));
    const auto query = lower_ascii(trim(query_value(target, "q")));
    const int limit = parse_bounded(query_value(target, "limit", "200"), "limit", 1, 1000);
    if (kind != "all" && kind != "technical" && kind != "selection" &&
        kind != "expert" && kind != "color-k")
        throw Error("kind must be all, technical, selection, expert, or color-k");
    if (origin != "all" && origin != "system" && origin != "user")
        throw Error("origin must be all, system, or user");
    Json rows = Json::array();
    std::size_t matched = 0;
    for (const auto& formula : state.formulas.at("formulas").as_array()) {
        if (kind != "all" && formula.at("kind_key").as_string() != kind) continue;
        const auto* source_origin = json_member(formula, "source_text_origin");
        const bool user_formula = source_origin && source_origin->is_string() &&
            source_origin->as_string() == "user-file-decrypted";
        if ((origin == "user" && !user_formula) ||
            (origin == "system" && user_formula))
            continue;
        if (!query.empty()) {
            std::string source_text;
            const auto& source = formula.at("source_text");
            if (source.is_string()) source_text = source.as_string();
            const auto haystack = lower_ascii(formula.at("code").as_string() + " " +
                formula.at("name").as_string() + " " + formula.at("category_name").as_string() +
                " " + source_text);
            if (haystack.find(query) == std::string::npos) continue;
        }
        ++matched;
        if (rows.size() < static_cast<std::size_t>(limit)) rows.push_back(formula);
    }
    Json result = Json::object();
    result["kind"] = kind;
    result["origin"] = origin;
    result["query"] = query;
    result["match_count"] = static_cast<std::uint64_t>(matched);
    result["returned"] = static_cast<std::uint64_t>(rows.size());
    result["truncated"] = matched > rows.size();
    result["source_text_count"] = state.formulas.at("source_text_count");
    result["source_text_missing_count"] = state.formulas.at("source_text_missing_count");
    result["library_schema"] = state.formulas.at("schema");
    result["library_origin"] = state.formulas.at("runtime_library_origin");
    result["runtime_dll_accessed"] = state.formulas.at("runtime_dll_accessed");
    const auto* private_user_data = json_member(
        state.formulas, "private_user_data");
    const bool user_library_enabled = private_user_data &&
        private_user_data->is_bool() && private_user_data->as_bool();
    result["user_library_enabled"] = user_library_enabled;
    std::uint64_t user_formula_count = 0;
    if (const auto* user_library = json_member(state.formulas, "user_library");
        user_library && user_library->is_object()) {
        if (const auto* count = json_member(*user_library, "formula_count");
            count && count->is_number() && count->as_number() >= 0.0)
            user_formula_count = static_cast<std::uint64_t>(count->as_number());
    }
    result["user_formula_count"] = user_formula_count;
    result["formulas"] = std::move(rows);
    return result;
}

Json query_formula_calculation(const RequestTarget& target) {
    const auto [market, code] = query_kline_security(target);
    const auto formula = lower_ascii(trim(query_value(target, "formula", "macd")));
    const auto kind = lower_ascii(trim(query_value(target, "kind", "auto")));
    const auto period = lower_ascii(trim(query_value(target, "period", "day")));
    const auto date = trim(query_value(target, "date", "all"));
    const int pages = parse_bounded(query_value(target, "pages", "1"), "pages", 1, 20);
    const int page_size = parse_bounded(query_value(target, "page_size", "800"),
                                        "page_size", 1, 800);
    const int start = parse_bounded(query_value(target, "start", "0"), "start", 0, 65535);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "10000"),
                                      "timeout_ms", 100, 30000);
    std::map<std::string, int> parameters;
    for (const auto name : {"n", "p", "n1", "n2", "n3", "n4", "m", "m1", "m2",
                            "m3", "m4", "short", "long", "mid", "signal"}) {
        const auto found = target.query.find(name);
        if (found != target.query.end())
            parameters.emplace(name, parse_bounded(found->second, name, 1, 1000));
    }
    auto kline = fetch_kline_document(market, code, kind, period, pages, page_size,
                                      start, date, timeout);
    return calculate_formula_document(std::move(kline), formula, parameters);
}

Json query_formula_coverage(const FormulaHttpState& state, const RequestTarget& target) {
    const auto kind = lower_ascii(trim(query_value(target, "kind", "all")));
    if (kind == "all") return state.formulas.at("coverage");
    const auto& by_kind = state.formulas.at("coverage").at("by_kind").as_object();
    const auto found = by_kind.find(kind);
    if (found == by_kind.end()) throw Error("unknown formula kind: " + kind);
    Json result = Json::object(); result["kind"] = kind; result["coverage"] = found->second;
    result["engine"] = state.formulas.at("formula_engine"); return result;
}

Json query_formula_context_template(const FormulaHttpState& state,
                                    const RequestTarget& target) {
    const auto formula = trim(query_value(target, "formula"));
    if (formula.empty()) throw Error("formula is required");
    const auto stamp = trim(query_value(target, "stamp"));
    std::vector<std::string> stamps;
    if (!stamp.empty()) stamps.push_back(stamp);
    auto document = make_formula_explicit_context_template_document(
        state.formulas, {formula}, stamps);
    const auto code_value = trim(query_value(target, "code"));
    if (!code_value.empty()) {
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
        auto& preliminary_metadata = document["_template"];
        if (preliminary_metadata.at("series_binding_count").as_number() == 0) {
            preliminary_metadata["stamp_source"] = "not-required-scalar-only";
            preliminary_metadata["kline_fetch_skipped"] = true;
            preliminary_metadata["requested_market"] = market;
            preliminary_metadata["requested_code"] = code;
            preliminary_metadata["requested_period"] = period;
            preliminary_metadata["bar_count"] = 0;
            return document;
        }
        const auto kline = fetch_kline_document(
            market, code, kind, period, pages, page_size,
            start, date, timeout, state.root);
        const auto derived = formula_context_stamps_from_kline(kline);
        stamps.insert(stamps.end(), derived.begin(), derived.end());
        document = make_formula_explicit_context_template_document(
            state.formulas, {formula}, stamps);
        auto& metadata = document["_template"];
        metadata["stamp_source"] = "kline";
        metadata["kline_fetch_skipped"] = false;
        metadata["market"] = kline.at("market");
        metadata["code"] = kline.at("code");
        metadata["period"] = kline.at("period");
        metadata["bar_count"] = static_cast<std::uint64_t>(
            formula_context_stamps_from_kline(kline).size());
    }
    return document;
}

Json formula_audit_context_requirements(const Json& formulas,
                                        bool expansion_market,
                                        std::string_view market) {
    std::set<std::string> dependencies, bindings;
    const int expansion_market_id =
        formula_context_detail::formula_market_id(std::string(market));
    const bool hk_finance_market = expansion_market &&
        formula_context_detail::is_tcalc_hk_finance_market(
            expansion_market_id);
    for (const auto& formula : formulas.at("formulas").as_array()) {
        const auto& analysis = formula.at("analysis");
        if (!analysis.at("executable_with_context").as_bool()) continue;
        const bool supported_hk_finance = hk_finance_market &&
            formula_context_detail::supports_tcalc_hk_finance_analysis(
                analysis, expansion_market_id);
        for (const auto& dependency :
             analysis.at("external_dependencies").as_array()) {
            const auto name = dependency.as_string();
            if (!expansion_market ||
                formula_dependency_supported_in_expansion(name, market) ||
                (name == "FINANCE" && supported_hk_finance))
                dependencies.insert(name);
        }
        for (const auto& binding :
             analysis.at("context_bindings_required").as_array()) {
            if (!expansion_market) {
                bindings.insert(binding.as_string());
                continue;
            }
            const auto selector =
                formula_context_detail::tcalc_finance_binding_selector(
                    binding.as_string());
            if (supported_hk_finance && selector &&
                formula_context_detail::is_tcalc_hk_finance_selector(
                    expansion_market_id, *selector))
                bindings.insert(binding.as_string());
        }
    }
    Json aggregate = Json::object();
    aggregate["external_dependencies"] = Json::array();
    aggregate["context_bindings_required"] = Json::array();
    for (const auto& dependency : dependencies)
        aggregate["external_dependencies"].push_back(dependency);
    for (const auto& binding : bindings)
        aggregate["context_bindings_required"].push_back(binding);
    return aggregate;
}

Json query_formula_audit(const FormulaHttpState& state, const RequestTarget& target) {
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

    Json context;
    const Json* context_pointer = nullptr;
    if (query_bool(target, "with_context")) {
        const auto* expansion = json_member(kline, "expansion_market");
        const bool expansion_market = expansion && expansion->is_bool() &&
                                      expansion->as_bool();
        const auto aggregate = formula_audit_context_requirements(
            state.formulas, expansion_market, market);
        context = build_formula_market_context_document(
            state.root, market, code, aggregate, timeout, &state.block_data,
            &kline, false, &state.formulas, state.jsn_root);
        context["automatic_market_context"] = true;
        context_pointer = &context;
    }
    auto result = audit_formula_library_document(
        std::move(kline), state.formulas, context_pointer,
        query_bool(target, "allow_future"));
    result["market_scope"] = market == "sz" || market == "sh" || market == "bj"
        ? "a-share" : "tdx-expansion";
    return result;
}


} // namespace tdx::server_detail
