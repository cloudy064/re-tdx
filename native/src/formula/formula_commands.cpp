#include "tdx/formula_engine.hpp"
#include "formula_engine_support_internal.hpp"
#include "formula_context_hk_finance_internal.hpp"
#include "formula_context_support_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/formula_context.hpp"
#include "tdx/formula_render_profile.hpp"
#include "tdx/formulas.hpp"
#include "tdx/level2.hpp"
#include "tdx/minute.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {
using namespace formula_engine_support;
namespace {

const Json& find_formula(const Json& library, const std::string& code) {
    const auto wanted = upper_ascii(trim(code));
    for (const auto& formula : library.at("formulas").as_array())
        if (upper_ascii(formula.at("code").as_string()) == wanted) return formula;
    throw Error("formula not found in library: " + code);
}

std::map<std::string, double> parse_parameters(const std::vector<std::string>& items) {
    std::map<std::string, double> result;
    for (const auto& item : items) {
        const auto separator = item.find('=');
        if (separator == std::string::npos || separator == 0 || separator + 1 == item.size())
            throw Error("--param must use NAME=NUMBER");
        const auto name = upper_ascii(trim(item.substr(0, separator)));
        std::size_t used = 0; double value = 0.0;
        try { value = std::stod(trim(item.substr(separator + 1)), &used); }
        catch (...) { throw Error("--param value must be numeric: " + item); }
        if (used != trim(item.substr(separator + 1)).size() || !std::isfinite(value))
            throw Error("--param value must be numeric: " + item);
        if (!result.emplace(name, value).second) throw Error("duplicate formula parameter: " + name);
    }
    return result;
}

int integer_option(Args& args, std::string_view name, int fallback, int minimum, int maximum) {
    const auto text = args.take_option(name, std::to_string(fallback));
    std::size_t used = 0; int value = 0;
    try { value = std::stoi(text, &used); } catch (...) { throw Error(std::string(name) + " must be an integer"); }
    if (used != text.size() || value < minimum || value > maximum) throw Error(std::string(name) + " is outside the safe range");
    return value;
}

void write_or_print(const Json& document, const std::string& output, bool compact) {
    const auto text = document.dump(compact ? -1 : 2) + "\n";
    if (output.empty()) std::cout << text;
    else atomic_write_text(formula_path_from_utf8(output), text);
}

}  // namespace

int command_formulas_analyze(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout << "Usage: tdx-tool formulas analyze [--input formulas.json] [--root TDX --include-user] [--output PATH] [--compact]\n"
                     "Uses the bundled formula snapshot by default. DLL extraction is isolated in tdx-formula-extractor.\n";
        return 0;
    }
    const auto input = args.take_option("--input"), root_text = args.take_option("--root");
    const auto output = args.take_option("--output");
    const bool include_user = args.take_flag("--include-user");
    const bool compact = args.take_flag("--compact"); args.require_empty();
    if (include_user && !input.empty())
        throw Error("--include-user requires --root instead of --input");
    Json library;
    if (!input.empty()) library = Json::parse(read_text_utf8(formula_path_from_utf8(input)));
    else {
        fs::path root;
        if (include_user)
            root = find_tdx_root(root_text.empty() ? fs::path{} :
                formula_path_from_utf8(root_text));
        library = load_bundled_installed_formula_library_document(
            root, include_user);
    }
    auto analyzed = analyze_formula_library_document(std::move(library));
    analyzed["formula_reference_graph"] =
        analyze_formula_reference_graph(analyzed);
    write_or_print(analyzed, output, compact); return 0;
}

int command_formulas_context_template(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout << "Usage: tdx-tool formulas context-template [options]\n"
                     "  --library FILE          Formula JSON override; otherwise use the bundled snapshot\n"
                     "  --root TDX              Used only for market data or optional PriGS.dat user formulas\n"
                     "  --include-user          Include root/T0002/PriGS.dat (CLI only)\n"
                     "  --formula CODE          Repeat to select formulas; default all explicit-context formulas\n"
                     "  --stamp DATE|TIME       Repeat to add null placeholders for exact K-line series stamps\n"
                     "  --market sz|sh|bj --code CODE   Derive stamps only when the template contains series\n"
                     "  --period day|week|month|1m|5m|15m|30m|60m --pages N --page-size N\n"
                     "  --output FILE --compact\n";
        return 0;
    }
    const auto library_text = args.take_option("--library");
    const auto root_text = args.take_option("--root");
    const auto formula_codes = args.take_options("--formula");
    auto stamps = args.take_options("--stamp");
    const auto market = lower_ascii(trim(args.take_option("--market")));
    const auto code = trim(args.take_option("--code"));
    const auto kind = lower_ascii(trim(args.take_option("--kind", "auto")));
    const auto period = lower_ascii(trim(args.take_option("--period", "day")));
    const auto date = trim(args.take_option("--date", "all"));
    const int pages = integer_option(args, "--pages", 1, 1, 20);
    const int page_size = integer_option(args, "--page-size", 800, 1, 800);
    const int start = integer_option(args, "--start", 0, 0, 65535);
    const int timeout = integer_option(args, "--timeout-ms", 10000, 1, 600000);
    const auto output = args.take_option("--output");
    const bool include_user = args.take_flag("--include-user");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    if (include_user && !library_text.empty())
        throw Error("--include-user requires --root instead of --library");
    Json library;
    if (!library_text.empty()) library = Json::parse(read_text_utf8(formula_path_from_utf8(library_text)));
    else {
        fs::path root;
        if (include_user)
            root = find_tdx_root(root_text.empty() ? fs::path{} :
                formula_path_from_utf8(root_text));
        library = load_bundled_installed_formula_library_document(
            root, include_user);
    }
    if (code.empty() && !market.empty()) {
        throw Error("--market requires --code for formula context-template");
    }
    auto document = make_formula_explicit_context_template_document(
        library, formula_codes, stamps);
    if (!code.empty()) {
        auto& preliminary_metadata = document["_template"];
        if (preliminary_metadata.at("series_binding_count").as_number() == 0) {
            preliminary_metadata["stamp_source"] = "not-required-scalar-only";
            preliminary_metadata["kline_fetch_skipped"] = true;
            preliminary_metadata["requested_market"] = market;
            preliminary_metadata["requested_code"] = code;
            preliminary_metadata["requested_period"] = period;
            preliminary_metadata["bar_count"] = 0;
            write_or_print(document, output, compact);
            return 0;
        }
        const auto kline = fetch_kline_document(
            market, code, kind, period, pages, page_size, start, date, timeout);
        const auto derived = formula_context_stamps_from_kline(kline);
        stamps.insert(stamps.end(), derived.begin(), derived.end());
        document = make_formula_explicit_context_template_document(
            std::move(library), formula_codes, stamps);
        auto& metadata = document["_template"];
        metadata["stamp_source"] = "kline";
        metadata["kline_fetch_skipped"] = false;
        metadata["market"] = kline.at("market");
        metadata["code"] = kline.at("code");
        metadata["period"] = kline.at("period");
        metadata["bar_count"] = static_cast<std::uint64_t>(
            formula_context_stamps_from_kline(kline).size());
    }
    write_or_print(document, output, compact);
    return 0;
}

int command_formulas_audit(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout << "Usage: tdx-tool formulas audit [--library formulas.json | --root TDX] "
                     "(--input kline.json | --code SECURITY) [options]\n"
                     "  --market MARKET --period time|day|week|month|1m|5m|15m|30m|60m\n"
                     "  MARKET accepts sz/sh/bj, qz/qd/qs/cz/qg, or TDX expansion ID 3..255.\n"
                     "  --option-name NAME       Visible option name for archived IVOLAT contracts\n"
                     "  --expiry YYYY-MM-DD      Exact option expiry used by IVOLAT(N,1)\n"
                     "  --risk-free RATE         IVOLAT annual rate (default 0.0187)\n"
                     "  --adjust MODE            none|qfq|hfq|fixed_qfq|fixed_hfq\n"
                     "  --anchor-date DATE       Required by fixed_qfq/fixed_hfq\n"
                     "  --adjust-cache-ttl-seconds 900 --refresh-adjustment\n"
                     "  --with-context          Also audit FINANCE/DYNAINFO/index/breadth formulas\n"
                     "  --include-user          Include root/T0002/PriGS.dat (CLI only)\n"
                     "  --context-file FILE     Merge caller-supplied series, including SIGNALS_QS#id#mode\n"
                     "  --allow-future          Include explicit read-only future-function formulas\n"
                     "  --output FILE --compact\n";
        return 0;
    }
    const auto library_text = args.take_option("--library"), root_text = args.take_option("--root");
    const auto input = args.take_option("--input"), code = trim(args.take_option("--code"));
    const auto market = lower_ascii(trim(args.take_option("--market")));
    const auto period = lower_ascii(trim(args.take_option("--period", "day")));
    const auto kind = lower_ascii(trim(args.take_option("--kind", "auto")));
    const auto date = trim(args.take_option("--date", "all"));
    const auto option_name = trim(args.take_option("--option-name"));
    const auto option_expiry = trim(args.take_option("--expiry"));
    const auto risk_free_text = trim(args.take_option("--risk-free"));
    const auto adjustment_mode = normalize_kline_adjustment_mode(
        args.take_option("--adjust", "none"));
    const auto anchor_date = trim(args.take_option("--anchor-date"));
    const int pages = integer_option(args, "--pages", 1, 1, 20);
    const int page_size = integer_option(args, "--page-size", 800, 1, 800);
    const int start = integer_option(args, "--start", 0, 0, 65535);
    const int timeout = integer_option(args, "--timeout-ms", 10000, 1, 600000);
    const int adjustment_cache_ttl = integer_option(
        args, "--adjust-cache-ttl-seconds", 900, 0, 86400);
    const auto output = args.take_option("--output"); const bool compact = args.take_flag("--compact");
    const bool with_context = args.take_flag("--with-context");
    const auto context_file_text = args.take_option("--context-file");
    const bool include_user = args.take_flag("--include-user");
    const bool allow_future = args.take_flag("--allow-future");
    const bool refresh_adjustment = args.take_flag("--refresh-adjustment");
    args.require_empty();
    if (include_user && !library_text.empty())
        throw Error("--include-user requires --root instead of --library");
    if (input.empty() && code.empty()) throw Error("formulas audit requires --input or --code");
    fs::path tdx_root;
    const auto ensure_root = [&]() -> const fs::path& {
        if (tdx_root.empty())
            tdx_root = find_tdx_root(root_text.empty() ? fs::path{} : formula_path_from_utf8(root_text));
        return tdx_root;
    };
    if (!root_text.empty()) (void)ensure_root();
    Json kline = !input.empty() ? Json::parse(read_text_utf8(formula_path_from_utf8(input))) :
        fetch_kline_document(market, code, kind, period, pages, page_size, start,
                             date, timeout, tdx_root);
    const auto* actual_market = optional(kline, "market");
    const auto* actual_code = optional(kline, "code");
    if (!actual_market || !actual_market->is_string() ||
        !actual_code || !actual_code->is_string())
        throw Error("K-line document needs market/code for adjustment/audit");
    const auto actual_market_text = actual_market->as_string();
    const auto actual_code_text = actual_code->as_string();
    kline = adjust_security_kline_document(
        std::move(kline), actual_market_text, actual_code_text, kind,
        adjustment_mode, anchor_date, tdx_root, {}, timeout,
        adjustment_cache_ttl, refresh_adjustment);
    if (!option_name.empty()) kline["option_name"] = option_name;
    if (!option_expiry.empty()) kline["option_expiry"] = option_expiry;
    if (!risk_free_text.empty()) {
        const auto risk_free =
            parse_parameters({"RISKFREE=" + risk_free_text}).at("RISKFREE");
        if (risk_free < -1.0 || risk_free > 1.0)
            throw Error("risk-free rate is outside the supported range");
        kline["option_risk_free"] = risk_free;
    }
    Json library;
    if (!library_text.empty()) library = Json::parse(read_text_utf8(formula_path_from_utf8(library_text)));
    else {
        library = load_bundled_installed_formula_library_document(
            include_user ? ensure_root() : tdx_root, include_user);
    }
    Json context; const Json* context_pointer = nullptr;
    if (with_context) {
        library = analyze_formula_library_document(std::move(library));
        std::set<std::string> dependencies, bindings;
        const auto* expansion = optional(kline, "expansion_market");
        const bool expansion_market = expansion && expansion->is_bool() &&
                                      expansion->as_bool();
        const int expansion_market_id =
            formula_context_detail::formula_market_id(actual_market_text);
        const bool hk_finance_market = expansion_market &&
            formula_context_detail::is_tcalc_hk_finance_market(
                expansion_market_id);
        for (const auto& formula : library.at("formulas").as_array()) {
            const auto& analysis = formula.at("analysis");
            if (!analysis.at("executable_with_context").as_bool() &&
                !(allow_future && analysis.at("read_only_future_executable").as_bool())) continue;
            const bool supported_hk_finance = hk_finance_market &&
                formula_context_detail::supports_tcalc_hk_finance_analysis(
                    analysis, expansion_market_id);
            for (const auto& dependency : analysis.at("external_dependencies").as_array()) {
                const auto name = dependency.as_string();
                if (!expansion_market || expansion_context_dependency(name) ||
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
        Json aggregate = Json::object(); aggregate["external_dependencies"] = strings_json(dependencies);
        aggregate["context_bindings_required"] = strings_json(bindings);
        const auto* actual_market = optional(kline, "market"); const auto* actual_code = optional(kline, "code");
        if (!actual_market || !actual_market->is_string() || !actual_code || !actual_code->is_string())
            throw Error("K-line document needs market/code for formula context audit");
        context = build_formula_market_context_document(
            ensure_root(), actual_market_text, actual_code_text, aggregate,
            timeout, nullptr, &kline, false, &library);
        context["automatic_market_context"] = true;
        context_pointer = &context;
    }
    if (!context_file_text.empty()) {
        auto supplied_context = Json::parse(
            read_text_utf8(formula_path_from_utf8(context_file_text)));
        if (!supplied_context.is_object())
            throw Error("formula context file root must be an object");
        supplied_context = materialize_tcalc_level2_formula_context(
            std::move(supplied_context), kline);
        if (!context.is_object()) context = Json::object();
        merge_formula_context(context, supplied_context);
        context["automatic_market_context"] = with_context;
        context_pointer = &context;
    }
    write_or_print(audit_formula_library_document(std::move(kline), std::move(library),
                                                  context_pointer, allow_future), output, compact);
    return 0;
}

int command_formulas_evaluate(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout << "Usage: tdx-tool formulas evaluate --formula CODE [--library formulas.json | --root TDX] (--input kline.json | --code SECURITY) [options]\n"
                     "  --source-file PATH       Execute source directly instead of selecting a library formula\n"
                     "  --market MARKET --period time|day|week|month|1m|5m|15m|30m|60m\n"
                     "  --param NAME=NUMBER      Repeatable parameter override\n"
                     "  --option-name NAME       Visible option name for IVOLAT archived contracts\n"
                     "  --expiry YYYY-MM-DD      Exact option expiry used by IVOLAT(N,1)\n"
                     "  --risk-free RATE         IVOLAT annual rate (default 0.0187)\n"
                     "  --adjust MODE            none|qfq|hfq|fixed_qfq|fixed_hfq\n"
                     "  --anchor-date DATE       Required by fixed_qfq/fixed_hfq\n"
                     "  --adjust-cache-ttl-seconds 900 --refresh-adjustment\n"
                     "  --context-file FILE     Merge explicit date|time series bindings; supports SIGNALS_QS#id#mode\n"
                     "  --include-user          Include root/T0002/PriGS.dat (CLI only)\n"
                     "  --point-in-time-finance  Use archived actual disclosure dates for FINANCE(43/44)/FINVALUE\n"
                     "  --allow-future           Read-only chart rendering; never enables scan/backtest\n"
                     "  --future-replay-observations N  Replay the last 1..64 successive K-line prefixes\n"
                     "  --future-replay-events N        Retain at most 1..10000 historical changes (default 1000)\n"
                     "Result includes sparse tdx-formula-render-ir-v1 primitives for chart functions.\n";
        return 0;
    }
    const auto formula_code = trim(args.take_option("--formula", "CUSTOM"));
    const auto library_text = args.take_option("--library"), source_file = args.take_option("--source-file");
    const auto root_text = args.take_option("--root"), input = args.take_option("--input"), code = trim(args.take_option("--code"));
    const auto market = lower_ascii(trim(args.take_option("--market"))), period = lower_ascii(trim(args.take_option("--period", "day")));
    const auto kind = lower_ascii(trim(args.take_option("--kind", "auto"))), date = trim(args.take_option("--date", "all"));
    const auto option_name = trim(args.take_option("--option-name"));
    const auto option_expiry = trim(args.take_option("--expiry"));
    const auto risk_free_text = trim(args.take_option("--risk-free"));
    const auto adjustment_mode = normalize_kline_adjustment_mode(
        args.take_option("--adjust", "none"));
    const auto anchor_date = trim(args.take_option("--anchor-date"));
    const auto context_file_text = args.take_option("--context-file");
    const int pages = integer_option(args, "--pages", 1, 1, 20), page_size = integer_option(args, "--page-size", 800, 1, 800);
    const int start = integer_option(args, "--start", 0, 0, 65535), timeout = integer_option(args, "--timeout-ms", 10000, 1, 600000);
    const int adjustment_cache_ttl = integer_option(
        args, "--adjust-cache-ttl-seconds", 900, 0, 86400);
    const auto supplied = parse_parameters(args.take_options("--param")); const auto output = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    const bool include_user = args.take_flag("--include-user");
    const bool allow_future = args.take_flag("--allow-future");
    const auto replay_observations_text =
        trim(args.take_option("--future-replay-observations"));
    const auto replay_events_text = trim(args.take_option("--future-replay-events"));
    const bool point_in_time_finance = args.take_flag("--point-in-time-finance");
    const bool refresh_adjustment = args.take_flag("--refresh-adjustment");
    args.require_empty();
    int replay_observations = 0;
    int replay_events = 1000;
    if (!replay_events_text.empty()) {
        std::size_t used = 0;
        try { replay_events = std::stoi(replay_events_text, &used); }
        catch (...) { throw Error("--future-replay-events must be an integer"); }
        if (used != replay_events_text.size() || replay_events < 1 ||
            replay_events > 10000)
            throw Error("--future-replay-events is outside the safe range");
        if (replay_observations_text.empty())
            throw Error("--future-replay-events requires --future-replay-observations");
    }
    if (!replay_observations_text.empty()) {
        std::size_t used = 0;
        try { replay_observations = std::stoi(replay_observations_text, &used); }
        catch (...) { throw Error("--future-replay-observations must be an integer"); }
        if (used != replay_observations_text.size() ||
            replay_observations < 1 || replay_observations > 64)
            throw Error("--future-replay-observations is outside the safe range");
        if (!allow_future)
            throw Error("--future-replay-observations requires --allow-future");
    }
    if (include_user && (!library_text.empty() || !source_file.empty()))
        throw Error("--include-user requires a root-backed library, not --library/--source-file");
    if (input.empty() && code.empty()) throw Error("formulas evaluate requires --input or --code");
    Json result; fs::path tdx_root;
    const auto ensure_root = [&]() -> const fs::path& {
        if (tdx_root.empty())
            tdx_root = find_tdx_root(root_text.empty() ? fs::path{} : formula_path_from_utf8(root_text));
        return tdx_root;
    };
    if (!root_text.empty()) (void)ensure_root();
    Json kline = !input.empty() ? Json::parse(read_text_utf8(formula_path_from_utf8(input))) :
        fetch_kline_document(market, code, kind, period, pages, page_size, start,
                             date, timeout, tdx_root);
    const auto* adjustment_market = optional(kline, "market");
    const auto* adjustment_code = optional(kline, "code");
    if (!adjustment_market || !adjustment_market->is_string() ||
        !adjustment_code || !adjustment_code->is_string())
        throw Error("K-line document needs market/code for adjustment/evaluation");
    const auto adjustment_market_text = adjustment_market->as_string();
    const auto adjustment_code_text = adjustment_code->as_string();
    kline = adjust_security_kline_document(
        std::move(kline), adjustment_market_text, adjustment_code_text,
        kind, adjustment_mode, anchor_date,
        tdx_root, {}, timeout, adjustment_cache_ttl, refresh_adjustment);
    if (!option_name.empty()) kline["option_name"] = option_name;
    if (!option_expiry.empty()) kline["option_expiry"] = option_expiry;
    if (!risk_free_text.empty())
        kline["option_risk_free"] = parse_parameters({"RISKFREE=" + risk_free_text}).at("RISKFREE");
    Json supplied_context;
    if (!context_file_text.empty()) {
        supplied_context = Json::parse(read_text_utf8(formula_path_from_utf8(context_file_text)));
        if (!supplied_context.is_object())
            throw Error("formula context file root must be an object");
        supplied_context = materialize_tcalc_level2_formula_context(
            std::move(supplied_context), kline);
    }
    const auto build_context = [&](const Json& analysis,
                                   const Json* formula_library) {
        Json context = Json::object();
        if (needs_automatic_formula_context(analysis)) {
            const auto* actual_market = optional(kline, "market");
            const auto* actual_code = optional(kline, "code");
            if (!actual_market || !actual_market->is_string() ||
                !actual_code || !actual_code->is_string())
                throw Error("K-line document needs market/code for automatic formula context");
            context = build_formula_market_context_document(ensure_root(),
                actual_market->as_string(), actual_code->as_string(), analysis,
                timeout, nullptr, &kline, point_in_time_finance,
                formula_library, {}, nullptr, &supplied);
            context["automatic_market_context"] = true;
        }
        if (supplied_context.is_object()) merge_formula_context(context, supplied_context);
        return context;
    };
    const auto require_execution_mode = [&](const Json& analysis, const Json* context) {
        if (analysis.at("has_future_function").as_bool()) {
            if (!analysis.at("read_only_future_executable").as_bool())
                throw Error("selected future formula still requires unsupported functions or data");
            if (!allow_future)
                throw Error("selected formula uses a future function; pass --allow-future only for read-only chart rendering");
            return;
        }
        if (!analysis.at("executable_with_context").as_bool() &&
            !explicit_formula_context_ready(analysis, context)) {
            if (const auto* explicit_bindable = optional(
                    analysis, "explicit_context_bindable");
                explicit_bindable && explicit_bindable->is_bool() &&
                explicit_bindable->as_bool())
                throw Error("selected formula requires explicit context bindings; pass --context-file with every explicit_context_bindings_required key in series or formula_scalar_bindings");
            throw Error("selected formula is not executable by the current interpreter");
        }
    };
    Json execution_analysis;
    std::string replay_source;
    std::map<std::string, double> replay_parameters;
    Json replay_context;
    bool replay_context_provided = false;
    if (!source_file.empty()) {
        const auto source = read_text_utf8(formula_path_from_utf8(source_file));
        std::vector<std::string> names; for (const auto& [name, value] : supplied) { (void)value; names.push_back(name); }
        execution_analysis = analyze_formula_source(source, names);
        auto context = build_context(execution_analysis, nullptr);
        require_execution_mode(execution_analysis,
            context.is_object() && context.size() ? &context : nullptr);
        replay_source = source;
        replay_parameters = supplied;
        replay_context = context;
        replay_context_provided = context.is_object() && context.size();
        result = evaluate_formula_source_document(replay_observations ? kline : std::move(kline), source, supplied, formula_code,
            context.is_object() && context.size() ? &context : nullptr);
    }
    else {
        Json library;
        if (!library_text.empty()) library = Json::parse(read_text_utf8(formula_path_from_utf8(library_text)));
        else {
            library = load_bundled_installed_formula_library_document(
                include_user ? ensure_root() : tdx_root, include_user);
        }
        const auto& selected = find_formula(library, formula_code);
        execution_analysis = analyze_formula_source(selected.at("source_text").as_string(),
                                                     formula_parameter_names(selected));
        execution_analysis["parameter_defaults"] = Json::object();
        for (const auto& [name, value] :
             effective_formula_parameters(selected))
            execution_analysis["parameter_defaults"][name] = value;
        auto context = build_context(execution_analysis, &library);
        require_execution_mode(execution_analysis,
            context.is_object() && context.size() ? &context : nullptr);
        replay_source = selected.at("source_text").as_string();
        replay_parameters = effective_formula_parameters(selected, supplied);
        replay_context = context;
        replay_context_provided = context.is_object() && context.size();
        result = evaluate_formula_document(replay_observations ? kline : std::move(kline), selected, supplied,
            context.is_object() && context.size() ? &context : nullptr);
    }
    if (allow_future && execution_analysis.at("has_future_function").as_bool()) {
        result["future_execution_mode"] = "explicit-read-only-lookahead";
        result["future_functions"] = execution_analysis.at("future_functions");
        result["scan_allowed"] = false;
        result["backtest_allowed"] = false;
    }
    if (replay_observations) {
        FormulaFutureReplayRequest replay;
        replay.kline_document = std::move(kline);
        replay.source = std::move(replay_source);
        replay.parameters = std::move(replay_parameters);
        replay.formula_code = formula_code;
        replay.context = std::move(replay_context);
        replay.context_provided = replay_context_provided;
        replay.max_observations = static_cast<std::size_t>(replay_observations);
        replay.max_events = static_cast<std::size_t>(replay_events);
        result["future_replay"] = replay_formula_future_document(replay);
    }
    if (!root_text.empty() && tdx_root.empty()) (void)ensure_root();
    result["render_environment"] = formula_render_environment_document(
        tdx_root.empty() ? fs::path{} : tdx_root);
    write_or_print(result, output, compact); return 0;
}

}  // namespace tdx

