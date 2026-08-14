#include "tdx/formula_engine.hpp"
#include "tdx/formula_context.hpp"

#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/formulas.hpp"
#include "tdx/minute.hpp"

#include "formula_engine_support_internal.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {
namespace {

const Json* optional(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

fs::path from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::string upper_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return ch < 0x80 ? static_cast<char>(std::toupper(ch)) : static_cast<char>(ch);
    });
    return value;
}

const Json& find_formula(const Json& library, const std::string& code,
                         const std::string& wanted_kind) {
    const auto wanted = upper_ascii(trim(code)); const Json* fallback = nullptr;
    for (const auto& formula : library.at("formulas").as_array()) {
        if (upper_ascii(formula.at("code").as_string()) != wanted) continue;
        if (!fallback) fallback = &formula;
        if (wanted_kind.empty() || formula.at("kind_key").as_string() == wanted_kind) return formula;
    }
    if (fallback && wanted_kind.empty()) return *fallback;
    throw Error("formula not found in library: " + code + (wanted_kind.empty() ? "" : " (kind " + wanted_kind + ")"));
}

std::map<std::string, double> parse_parameters(const std::vector<std::string>& items) {
    std::map<std::string, double> result;
    for (const auto& item : items) {
        const auto separator = item.find('=');
        if (separator == std::string::npos || separator == 0 || separator + 1 == item.size()) throw Error("--param must use NAME=NUMBER");
        const auto name = upper_ascii(trim(item.substr(0, separator))), text = trim(item.substr(separator + 1));
        std::size_t used = 0; double value = 0.0; try { value = std::stod(text, &used); } catch (...) { throw Error("--param value must be numeric: " + item); }
        if (used != text.size() || !std::isfinite(value)) throw Error("--param value must be numeric: " + item);
        if (!result.emplace(name, value).second) throw Error("duplicate formula parameter: " + name);
    }
    return result;
}

int integer_option(Args& args, std::string_view name, int fallback, int minimum, int maximum) {
    const auto text = args.take_option(name, std::to_string(fallback)); std::size_t used = 0; int value = 0;
    try { value = std::stoi(text, &used); } catch (...) { throw Error(std::string(name) + " must be an integer"); }
    if (used != text.size() || value < minimum || value > maximum)
        throw Error(std::string(name) + " is outside the safe range");
    return value;
}

double number_option(Args& args, std::string_view name, double fallback,
                     double minimum, double maximum) {
    const auto text = args.take_option(name, std::to_string(fallback)); std::size_t used = 0; double value = 0.0;
    try { value = std::stod(text, &used); } catch (...) { throw Error(std::string(name) + " must be numeric"); }
    if (used != text.size() || !std::isfinite(value) || value < minimum || value > maximum)
        throw Error(std::string(name) + " is outside the safe range");
    return value;
}

bool signal(const Json& point, std::string_view name) {
    const auto found = point.at("values").as_object().find(name);
    return found != point.at("values").as_object().end() && found->second.is_number() &&
           std::abs(found->second.as_number()) > 1e-12;
}

std::set<std::string> external_dependencies(const Json& analysis) {
    std::set<std::string> result;
    const auto* values = optional(analysis, "external_dependencies");
    if (values && values->is_array())
        for (const auto& value : values->as_array())
            if (value.is_string()) result.insert(value.as_string());
    return result;
}

Json analyze_selected_formula(const Json& formula) {
    const auto* source = optional(formula, "source_text");
    if (!source || !source->is_string())
        throw Error("selected formula source text is unavailable");
    std::vector<std::string> parameter_names;
    if (const auto* parameters = optional(formula, "parameters");
        parameters && parameters->is_array()) {
        for (const auto& parameter : parameters->as_array())
            if (const auto* name = optional(parameter, "name"); name && name->is_string())
                parameter_names.push_back(name->as_string());
    }
    return analyze_formula_source(source->as_string(), parameter_names);
}

}  // namespace

Json backtest_formula_document(Json kline_document, const Json& formula,
                               const std::map<std::string, double>& parameters,
                               double initial_capital, double commission_bps,
                               double slippage_bps, const Json* context) {
    if (!std::isfinite(initial_capital) || initial_capital <= 0) throw Error("initial capital must be positive");
    if (commission_bps < 0 || commission_bps > 1000 || slippage_bps < 0 || slippage_bps > 1000)
        throw Error("commission/slippage bps must be in 0..1000");
    const auto formula_analysis = analyze_selected_formula(formula);
    if (!formula_analysis.at("numeric_signal_safe").as_bool())
        throw Error("formula backtest rejected degraded numeric output; inspect "
                    "degraded_numeric_output_causes with formulas analyze");
    const auto execution = evaluate_formula_document(
        std::move(kline_document), formula, parameters, context);
    const auto& points = execution.at("points").as_array();
    if (points.size() < 2) throw Error("backtest requires at least two K-line bars");
    bool has_entry = false, has_exit = false;
    for (const auto& output : execution.at("outputs").as_array()) {
        has_entry = has_entry || upper_ascii(output.as_string()) == "ENTERLONG";
        has_exit = has_exit || upper_ascii(output.as_string()) == "EXITLONG";
    }
    if (!has_entry || !has_exit) throw Error("backtest formula must output ENTERLONG and EXITLONG");
    const double commission = commission_bps / 10000.0, slippage = slippage_bps / 10000.0;
    double cash = initial_capital, quantity = 0.0, entry_cost = 0.0, peak = initial_capital;
    double max_drawdown = 0.0, gross_profit = 0.0, gross_loss = 0.0;
    int wins = 0, losses = 0; Json trades = Json::array(), equity_curve = Json::array();
    std::string entry_date, entry_time;
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (i > 0) {
            const auto& prior = points[i - 1]; const double open = points[i].at("open").as_number();
            if (quantity > 0 && signal(prior, "EXITLONG")) {
                const double price = open * (1.0 - slippage), proceeds = quantity * price;
                const double fee = proceeds * commission, net = proceeds - fee, profit = net - entry_cost;
                cash += net; Json trade = Json::object(); trade["entry_date"] = entry_date; trade["entry_time"] = entry_time;
                trade["exit_date"] = points[i].at("date"); trade["exit_time"] = points[i].at("time");
                trade["exit_price"] = price; trade["quantity"] = quantity; trade["profit"] = profit;
                trade["return_pct"] = entry_cost > 0 ? profit * 100.0 / entry_cost : 0.0; trade["exit_reason"] = "EXITLONG";
                trades.push_back(std::move(trade)); if (profit >= 0) { ++wins; gross_profit += profit; } else { ++losses; gross_loss += -profit; }
                quantity = 0.0; entry_cost = 0.0;
            }
            if (quantity == 0 && signal(prior, "ENTERLONG")) {
                const double price = open * (1.0 + slippage); quantity = cash / (price * (1.0 + commission));
                const double gross = quantity * price, fee = gross * commission; entry_cost = gross + fee; cash -= entry_cost;
                entry_date = points[i].at("date").as_string(); entry_time = points[i].at("time").as_string();
            }
        }
        const double equity = cash + quantity * points[i].at("close").as_number(); peak = std::max(peak, equity);
        if (peak > 0) max_drawdown = std::max(max_drawdown, (peak - equity) * 100.0 / peak);
        Json row = Json::object(); row["date"] = points[i].at("date"); row["time"] = points[i].at("time"); row["equity"] = equity;
        equity_curve.push_back(std::move(row));
    }
    if (quantity > 0) {
        const auto& last = points.back(); const double price = last.at("close").as_number() * (1.0 - slippage);
        const double proceeds = quantity * price, net = proceeds - proceeds * commission, profit = net - entry_cost; cash += net;
        Json trade = Json::object(); trade["entry_date"] = entry_date; trade["entry_time"] = entry_time;
        trade["exit_date"] = last.at("date"); trade["exit_time"] = last.at("time"); trade["exit_price"] = price;
        trade["quantity"] = quantity; trade["profit"] = profit; trade["return_pct"] = entry_cost > 0 ? profit * 100.0 / entry_cost : 0.0;
        trade["exit_reason"] = "end_of_data"; trades.push_back(std::move(trade));
        if (profit >= 0) { ++wins; gross_profit += profit; } else { ++losses; gross_loss += -profit; }
    }
    const int trade_count = wins + losses; Json result = Json::object(); result["schema_version"] = 1;
    result["engine"] = "tdx-source-backtest-v1"; result["formula"] = formula.at("code"); result["kind"] = formula.at("kind_key");
    result["signal_timing"] = "signal at close, execute at next open"; result["initial_capital"] = initial_capital;
    result["final_equity"] = cash; result["total_return_pct"] = (cash / initial_capital - 1.0) * 100.0;
    result["max_drawdown_pct"] = max_drawdown; result["commission_bps"] = commission_bps; result["slippage_bps"] = slippage_bps;
    result["trade_count"] = trade_count; result["wins"] = wins; result["losses"] = losses;
    result["win_rate_pct"] = trade_count ? wins * 100.0 / trade_count : 0.0;
    result["profit_factor"] = gross_loss > 1e-12 ? Json(gross_profit / gross_loss) : Json(nullptr);
    result["trades"] = std::move(trades); result["equity_curve"] = std::move(equity_curve);
    result["analysis"] = formula_analysis;
    for (const auto key : {"market", "code", "name", "period", "count",
                           "adjustment_mode", "adjustment"})
        if (const auto* value = optional(execution, key)) result[key] = *value;
    if (const auto* value = optional(execution, "context_bindings"))
        result["context_bindings"] = *value;
    if (const auto* value = optional(execution, "context_metadata"))
        result["context_metadata"] = *value;
    return result;
}

int command_formulas_backtest(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout << "Usage: tdx-tool formulas backtest --formula CODE (--input kline.json | --code SECURITY) [options]\n"
                     "  --kind expert --library formulas.json | --root TDX\n"
                     "  --include-user          Include root/T0002/PriGS.dat (CLI only)\n"
                     "  --source-file PATH       Backtest caller-supplied expert formula source\n"
                     "  --initial-capital 100000 --commission-bps 2.5 --slippage-bps 1\n"
                     "  --market sz|sh|bj --period day --pages 5 --param NAME=NUMBER\n"
                     "  --adjust none|qfq|hfq|fixed_qfq|fixed_hfq --anchor-date DATE\n"
                     "  --adjust-cache-ttl-seconds 900 --refresh-adjustment\n"
                     "  --point-in-time-finance  Use archived actual disclosure dates; no current fallback\n";
        return 0;
    }
    const auto source_file = args.take_option("--source-file");
    const auto formula_code = trim(args.take_option("--formula", source_file.empty() ? "" : "CUSTOM"));
    if (formula_code.empty()) throw Error("formulas backtest requires --formula or --source-file");
    const auto wanted_kind = lower_ascii(trim(args.take_option("--formula-kind", "expert")));
    const auto library_text = args.take_option("--library"), root_text = args.take_option("--root"), input = args.take_option("--input");
    const auto code = trim(args.take_option("--code")), market = lower_ascii(trim(args.take_option("--market")));
    const auto period = lower_ascii(trim(args.take_option("--period", "day"))), kind = lower_ascii(trim(args.take_option("--kind", "auto")));
    const auto adjustment_mode = normalize_kline_adjustment_mode(
        args.take_option("--adjust", "none"));
    const auto anchor_date = trim(args.take_option("--anchor-date"));
    const int pages = integer_option(args, "--pages", 5, 1, 20), page_size = integer_option(args, "--page-size", 800, 1, 800);
    const int timeout = integer_option(args, "--timeout-ms", 10000, 100, 60000);
    const int adjustment_cache_ttl = integer_option(
        args, "--adjust-cache-ttl-seconds", 900, 0, 86400);
    const double initial = number_option(args, "--initial-capital", 100000, 1, 1e15);
    const double commission = number_option(args, "--commission-bps", 2.5, 0, 1000), slippage = number_option(args, "--slippage-bps", 1, 0, 1000);
    const auto parameters = parse_parameters(args.take_options("--param")); const auto output = args.take_option("--output");
    const bool point_in_time_finance = args.take_flag("--point-in-time-finance");
    const bool include_user = args.take_flag("--include-user");
    const bool refresh_adjustment = args.take_flag("--refresh-adjustment");
    const bool compact = args.take_flag("--compact"); args.require_empty();
    if (!source_file.empty() && !library_text.empty())
        throw Error("--source-file cannot be combined with --library");
    if (include_user && (!source_file.empty() || !library_text.empty()))
        throw Error("--include-user requires a root-backed library, not --library/--source-file");
    if (input.empty() == code.empty()) throw Error("choose exactly one of --input or --code");
    Json library, custom_formula; fs::path tdx_root;
    const Json* selected = nullptr;
    if (!source_file.empty()) {
        custom_formula = make_formula_source_definition(
            read_text_utf8(from_utf8(source_file)), formula_code, "expert", parameters);
        selected = &custom_formula;
    } else if (!library_text.empty()) {
        library = Json::parse(read_text_utf8(from_utf8(library_text)));
    } else {
        if (include_user)
            tdx_root = find_tdx_root(
                root_text.empty() ? fs::path{} : from_utf8(root_text));
        library = load_bundled_installed_formula_library_document(
            tdx_root, include_user);
    }
    if (!root_text.empty() && tdx_root.empty())
        tdx_root = find_tdx_root(from_utf8(root_text));
    Json kline = !input.empty() ? Json::parse(read_text_utf8(from_utf8(input))) :
        fetch_kline_document(market, code, kind, period, pages, page_size, 0, "all",
                             timeout, tdx_root);
    const auto* actual_market = optional(kline, "market");
    const auto* actual_code = optional(kline, "code");
    if (!actual_market || !actual_market->is_string() ||
        !actual_code || !actual_code->is_string())
        throw Error("K-line document needs market/code for adjustment/backtest");
    const auto actual_market_text = actual_market->as_string();
    const auto actual_code_text = actual_code->as_string();
    kline = adjust_security_kline_document(
        std::move(kline), actual_market_text, actual_code_text, kind,
        adjustment_mode, anchor_date, tdx_root, {}, timeout,
        adjustment_cache_ttl, refresh_adjustment);
    if (!selected) selected = &find_formula(library, formula_code, wanted_kind);
    const auto* source = optional(*selected, "source_text");
    if (!source || !source->is_string())
        throw Error("selected formula source text is unavailable");
    std::vector<std::string> parameter_names;
    if (const auto* specs = optional(*selected, "parameters"); specs && specs->is_array())
        for (const auto& spec : specs->as_array())
            parameter_names.push_back(spec.at("name").as_string());
    auto analysis = analyze_formula_source(source->as_string(), parameter_names);
    analysis["parameter_defaults"] = Json::object();
    for (const auto& [name, value] :
         formula_engine_support::effective_formula_parameters(*selected))
        analysis["parameter_defaults"][name] = value;
    if (!analysis.at("executable_with_context").as_bool())
        throw Error("selected backtest formula requires unsupported data or a future function");
    const auto dependencies = external_dependencies(analysis);
    const bool finance_dependency = dependencies.count("FINANCE") ||
                                    dependencies.count("FINVALUE");
    if (point_in_time_finance && !finance_dependency)
        throw Error("--point-in-time-finance requires FINANCE or FINVALUE dependencies");
    if (finance_dependency && !point_in_time_finance)
        throw Error("historical backtest finance requires --point-in-time-finance; "
                    "current report constants are not backtest-safe");
    for (const auto& dependency : dependencies)
        if (dependency != "FINANCE" && dependency != "FINVALUE" &&
            !formula_engine_support::supported_formula_reference_dependency(
                dependency))
            throw Error("historical backtest does not yet admit external dependency " +
                        dependency + "; its point-in-time safety is unproven");
    Json context; const Json* context_pointer = nullptr;
    if (analysis.at("has_external_dependency").as_bool()) {
        if (tdx_root.empty())
            tdx_root = find_tdx_root(root_text.empty() ? fs::path{} : from_utf8(root_text));
        context = build_formula_market_context_document(
            tdx_root, actual_market_text, actual_code_text,
            analysis, timeout, nullptr, &kline, point_in_time_finance,
            library.is_object() ? &library : nullptr, {}, nullptr,
            &parameters);
        context_pointer = &context;
    }
    auto result = backtest_formula_document(std::move(kline), *selected,
                                             parameters, initial, commission, slippage,
                                             context_pointer);
    result["point_in_time_finance"] = point_in_time_finance;
    result["include_user"] = include_user;
    if (!source_file.empty()) {
        result["formula_source_mode"] = "source-file";
        result["source_file"] = path_utf8(from_utf8(source_file));
    }
    const auto text = result.dump(compact ? -1 : 2) + "\n"; if (output.empty()) std::cout << text;
    else atomic_write_text(from_utf8(output), text);
    return 0;
}

}  // namespace tdx
