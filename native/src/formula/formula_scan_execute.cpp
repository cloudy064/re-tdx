#include "formula_scan_internal.hpp"
#include "formula_engine_support_internal.hpp"

namespace tdx::formula_scan_detail {
namespace fs = std::filesystem;

FormulaScanInvocation execute_formula_scan_once(const std::vector<std::string>& raw_args,
                                                bool force_refresh) {
    Args args(raw_args);
    const auto source_file = args.take_option("--source-file");
    const auto formula_code = trim(args.take_option("--formula", source_file.empty() ? "" : "CUSTOM"));
    if (formula_code.empty()) throw Error("formulas scan requires --formula or --source-file");
    const auto library_text = args.take_option("--library"), root_text = args.take_option("--root");
    const auto input_text = args.take_option("--input"), securities_text = args.take_option("--securities");
    const bool all = args.take_flag("--all");
    const bool requested_refresh = args.take_flag("--refresh");
    const bool include_user = args.take_flag("--include-user");
    const bool refresh = requested_refresh || force_refresh;
    const bool refresh_adjustment = args.take_flag("--refresh-adjustment");
    const bool point_in_time_finance = args.take_flag("--point-in-time-finance");
    const bool compact = args.take_flag("--compact");
    const auto period = lower_ascii(trim(args.take_option("--period", "day"))), cache_text = args.take_option("--cache-dir");
    const auto adjustment_mode = normalize_kline_adjustment_mode(
        args.take_option("--adjust", "none"));
    const auto anchor_date = trim(args.take_option("--anchor-date"));
    const int pages = integer_option(args, "--pages", 1, 1, 20), page_size = integer_option(args, "--page-size", 800, 1, 800);
    const int lookback = integer_option(args, "--lookback", 1, 1, 10000), workers = integer_option(args, "--workers", 4, 1, 32);
    const int timeout = integer_option(args, "--timeout-ms", 10000, 100, 60000), limit = integer_option(args, "--limit", 100000, 1, 100000);
    const int adjustment_cache_ttl = integer_option(
        args, "--adjust-cache-ttl-seconds", 900, 0, 86400);
    const auto parameters = parse_parameters(args.take_options("--param"));
    const auto output = args.take_option("--output");
    args.require_empty();
    if (!source_file.empty() && !library_text.empty())
        throw Error("--source-file cannot be combined with --library");
    if (include_user && (!source_file.empty() || !library_text.empty()))
        throw Error("--include-user requires a root-backed library, not --library/--source-file");
    if ((input_text.empty() ? 0 : 1) + (securities_text.empty() ? 0 : 1) + (all ? 1 : 0) != 1)
        throw Error("choose exactly one of --input, --securities, or --all");
    Json library, custom_formula; fs::path tdx_root;
    const Json* formula = nullptr;
    if (!source_file.empty()) {
        custom_formula = make_formula_source_definition(
            read_text_utf8(from_utf8(source_file)), formula_code, "selection", parameters);
        formula = &custom_formula;
    } else {
        if (!library_text.empty()) library = Json::parse(read_text_utf8(from_utf8(library_text)));
        else {
            if (include_user)
                tdx_root = find_tdx_root(
                    root_text.empty() ? fs::path{} : from_utf8(root_text));
            library = load_bundled_installed_formula_library_document(
                tdx_root, include_user);
        }
        formula = &find_formula(library, formula_code);
    }
    const auto* source = optional(*formula, "source_text"); if (!source || !source->is_string()) throw Error("selected formula source text is unavailable");
    auto analysis = analyze_formula_source(source->as_string(), [&] {
        std::vector<std::string> names;
        if (const auto* specs = optional(*formula, "parameters")) {
            for (const auto& spec : specs->as_array())
                names.push_back(spec.at("name").as_string());
        }
        return names;
    }());
    analysis["parameter_defaults"] = Json::object();
    for (const auto& [name, value] :
         formula_engine_support::effective_formula_parameters(*formula))
        analysis["parameter_defaults"][name] = value;
    if (!analysis.at("executable_with_context").as_bool())
        throw Error("selected formula is not executable; run formulas analyze for dependencies");
    if (point_in_time_finance && !has_finance_dependency(analysis))
        throw Error("--point-in-time-finance requires FINANCE or FINVALUE dependencies");
    if (analysis.at("has_external_dependency").as_bool() && tdx_root.empty())
        tdx_root = find_tdx_root(root_text.empty() ? fs::path{} : from_utf8(root_text));
    if (!root_text.empty() && tdx_root.empty())
        tdx_root = find_tdx_root(from_utf8(root_text));
    std::vector<Json> klines; std::vector<SecurityKey> securities;
    if (!input_text.empty()) {
        const auto input = Json::parse(read_text_utf8(from_utf8(input_text)));
        klines = klines_from_document(input); if (klines.empty()) securities = securities_from_document(input);
        if (klines.empty() && securities.empty()) throw Error("--input contains neither K-lines nor securities");
    } else if (!securities_text.empty()) {
        for (const auto& item : split(securities_text, ',')) if (!trim(item).empty()) securities.push_back(parse_security(item));
    } else {
        SecurityDirectoryQuery query; query.market = "all"; query.category = "a_share"; query.limit = limit;
        query.timeout_ms = timeout; query.refresh = true; SecurityDirectoryService service;
        securities = securities_from_document(service.query(query));
    }
    if (klines.empty() && securities.empty())
        throw Error("formula scan universe resolved to zero securities");
    Json fetch_errors = Json::array();
    if (!klines.empty()) {
        std::vector<Json> contextualized;
        contextualized.reserve(klines.size());
        for (auto& kline : klines) {
            try {
                const auto* market = optional(kline, "market"); const auto* code = optional(kline, "code");
                if (!market || !market->is_string() || !code || !code->is_string())
                    throw Error("K-line document needs market/code for adjustment/context");
                const auto market_text = market->as_string();
                const auto code_text = code->as_string();
                kline = adjust_security_kline_document(
                    std::move(kline), market_text, code_text, "stock",
                    adjustment_mode, anchor_date, tdx_root, {}, timeout,
                    adjustment_cache_ttl, refresh_adjustment);
                if (analysis.at("has_external_dependency").as_bool())
                    kline["formula_context"] = build_formula_market_context_document(
                        tdx_root, market_text, code_text, analysis, timeout,
                        nullptr, &kline, point_in_time_finance,
                        library.is_object() ? &library : nullptr, {}, nullptr,
                        &parameters);
                contextualized.push_back(std::move(kline));
            } catch (const std::exception& error) {
                Json row = Json::object(); row["error"] = error.what(); fetch_errors.push_back(std::move(row));
            }
        }
        klines = std::move(contextualized);
    }
    if (!securities.empty()) {
        const auto cache_dir = cache_text.empty() ? fs::path{} : from_utf8(cache_text);
        if (!cache_dir.empty()) fs::create_directories(cache_dir);
        for (auto& outcome : fetch_klines(securities, period, pages, page_size, timeout, workers,
                                          cache_dir, refresh, tdx_root, analysis,
                                          library.is_object() ? &library : nullptr,
                                          parameters,
                                          point_in_time_finance, adjustment_mode,
                                          anchor_date, adjustment_cache_ttl,
                                          refresh_adjustment || requested_refresh)) {
            if (outcome.error.empty()) klines.push_back(std::move(outcome.kline));
            else { Json error = Json::object(); error["market"] = securities[outcome.index].market; error["code"] = securities[outcome.index].code; error["error"] = outcome.error; fetch_errors.push_back(std::move(error)); }
        }
    }
    auto result = scan_formula_documents(klines, *formula, parameters, lookback);
    const auto& source_text = source->as_string();
    result["formula_source_md5"] = md5_bytes(Bytes(source_text.begin(), source_text.end()));
    result["period"] = period;
    result["pages"] = pages;
    result["page_size"] = page_size;
    Json parameter_values = Json::object();
    for (const auto& [name, value] : parameters) parameter_values[name] = value;
    result["parameters"] = std::move(parameter_values);
    if (!source_file.empty()) {
        result["formula_source_mode"] = "source-file";
        result["source_file"] = path_utf8(from_utf8(source_file));
    }
    result["requested_count"] = static_cast<std::uint64_t>(klines.size() + fetch_errors.size());
    result["fetch_error_count"] = static_cast<std::uint64_t>(fetch_errors.size()); result["fetch_errors"] = std::move(fetch_errors);
    result["point_in_time_finance"] = point_in_time_finance;
    result["include_user"] = include_user;
    const auto effective_adjustment_mode =
        uniform_kline_adjustment_mode(klines, adjustment_mode);
    result["adjustment_mode"] = effective_adjustment_mode;
    if (effective_adjustment_mode != "none")
        result["adjustment_summary"] =
            summarize_kline_adjustments(klines, effective_adjustment_mode);
    return FormulaScanInvocation{std::move(result), compact, output};
}

}  // namespace tdx::formula_scan_detail
