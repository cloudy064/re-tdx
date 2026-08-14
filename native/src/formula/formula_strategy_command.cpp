#include "formula_strategy_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/formulas.hpp"
#include "tdx/minute.hpp"
#include "tdx/security_directory.hpp"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {

using namespace formula_strategy_detail;

int command_formulas_strategy(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool formulas strategy --strategy FILE --mode scan|backtest [universe] [options]\n\n"
            "Universe (choose one):\n"
            "  --input PATH             K-line array/bundle or market-securities JSON\n"
            "  --securities LIST        Comma-separated sz000001,sh600000,...\n"
            "  --all                    Download the server A-share directory\n\n"
            "Options:\n"
            "  --library PATH | --root TDX   Required only for manifest library rules\n"
            "  --include-user          Include root/T0002/PriGS.dat (CLI only)\n"
            "  --period day --pages 1 --page-size 800 --lookback 1\n"
            "  --workers 4 --limit 100000 --timeout-ms 10000\n"
            "  --adjust none|qfq|hfq|fixed_qfq|fixed_hfq --anchor-date DATE\n"
            "  --adjust-cache-ttl-seconds 900 --refresh-adjustment\n"
            "  --point-in-time-finance  Use archived actual disclosure dates\n"
            "  --initial-capital 100000 --commission-bps 2.5 --slippage-bps 1\n"
            "  --output PATH --compact\n";
        return 0;
    }
    const auto strategy_file = args.take_option("--strategy");
    if (strategy_file.empty()) throw Error("formulas strategy requires --strategy");
    const auto mode = lower_ascii(trim(args.take_option("--mode", "scan")));
    if (mode != "scan" && mode != "backtest")
        throw Error("formulas strategy mode must be scan or backtest");
    const auto input_file = args.take_option("--input");
    const auto securities_text = args.take_option("--securities");
    const bool all = args.take_flag("--all");
    if ((input_file.empty() ? 0 : 1) + (securities_text.empty() ? 0 : 1) +
        (all ? 1 : 0) != 1)
        throw Error("choose exactly one of --input, --securities, or --all");
    const auto library_file = args.take_option("--library");
    const auto root_text = args.take_option("--root");
    const auto period = lower_ascii(trim(args.take_option("--period", "day")));
    const auto adjustment_mode = normalize_kline_adjustment_mode(
        args.take_option("--adjust", "none"));
    const auto anchor_date = trim(args.take_option("--anchor-date"));
    const int pages = integer_option(args, "--pages", mode == "backtest" ? 5 : 1, 1, 20);
    const int page_size = integer_option(args, "--page-size", 800, 1, 800);
    const int lookback = integer_option(args, "--lookback", 1, 1, 10000);
    const int workers = integer_option(args, "--workers", 4, 1, 32);
    const int limit = integer_option(args, "--limit", 100000, 1, 100000);
    const int timeout = integer_option(args, "--timeout-ms", 10000, 100, 60000);
    const int adjustment_cache_ttl = integer_option(
        args, "--adjust-cache-ttl-seconds", 900, 0, 86400);
    const double initial = number_option(args, "--initial-capital", 100000, 1, 1e15);
    const double commission = number_option(args, "--commission-bps", 2.5, 0, 1000);
    const double slippage = number_option(args, "--slippage-bps", 1, 0, 1000);
    const bool point_in_time_finance = args.take_flag("--point-in-time-finance");
    const bool include_user = args.take_flag("--include-user");
    const bool refresh_adjustment = args.take_flag("--refresh-adjustment");
    const bool compact = args.take_flag("--compact");
    const auto output = args.take_option("--output");
    args.require_empty();
    if (include_user && !library_file.empty())
        throw Error("--include-user requires --root instead of --library");

    const auto manifest = Json::parse(read_text_utf8(from_utf8(strategy_file)));
    fs::path root;
    Json library;
    const Json* library_pointer = nullptr;
    if (manifest_needs_library(manifest)) {
        if (!library_file.empty()) {
            library = Json::parse(read_text_utf8(from_utf8(library_file)));
            if (!root_text.empty()) root = find_tdx_root(from_utf8(root_text));
        } else {
            if (include_user)
                root = find_tdx_root(
                    root_text.empty() ? fs::path{} : from_utf8(root_text));
            library = load_bundled_installed_formula_library_document(
                root, include_user);
        }
        library_pointer = &library;
    } else if (include_user) {
        throw Error("--include-user requires strategy rules selected from a library");
    }
    auto strategy = normalize_formula_strategy_document(manifest, library_pointer);
    bool external = false;
    for (const auto& rule : strategy.at("rules").as_array())
        external = external || has_external_dependency(rule.at("analysis"));
    if (external && root.empty())
        root = find_tdx_root(root_text.empty() ? fs::path{} : from_utf8(root_text));
    if (!root_text.empty() && root.empty())
        root = find_tdx_root(from_utf8(root_text));
    std::vector<Json> klines;
    std::vector<SecurityKey> securities;
    if (!input_file.empty()) {
        const auto input = Json::parse(read_text_utf8(from_utf8(input_file)));
        klines = kline_documents(input);
        if (klines.empty()) securities = securities_from_document(input);
    } else if (!securities_text.empty()) {
        for (const auto& value : split(securities_text, ','))
            if (!trim(value).empty()) securities.push_back(parse_security(value));
    } else {
        SecurityDirectoryQuery query;
        query.market = "all"; query.category = "a_share"; query.limit = limit;
        query.timeout_ms = timeout; query.refresh = true;
        SecurityDirectoryService service;
        securities = securities_from_document(service.query(query));
    }
    if (klines.empty() && securities.empty())
        throw Error("formula strategy universe resolved to zero securities");
    Json fetch_errors = Json::array();
    if (!securities.empty()) {
        struct Outcome { Json kline; std::string error; };
        std::vector<Outcome> outcomes(securities.size());
        std::atomic<std::size_t> next{0};
        auto worker = [&] {
            while (true) {
                const auto index = next.fetch_add(1);
                if (index >= securities.size()) break;
                try {
                    auto& key = securities[index];
                    outcomes[index].kline = fetch_kline_document(
                        key.market, key.code, "stock", period, pages, page_size,
                        0, "all", timeout, root);
                    if (!key.name.empty()) outcomes[index].kline["name"] = key.name;
                    outcomes[index].kline = adjust_security_kline_document(
                        std::move(outcomes[index].kline), key.market, key.code,
                        "stock", adjustment_mode, anchor_date, root, {}, timeout,
                        adjustment_cache_ttl, refresh_adjustment);
                    attach_strategy_contexts(outcomes[index].kline, strategy, root,
                        timeout, nullptr, point_in_time_finance, mode == "backtest",
                        library_pointer);
                } catch (const std::exception& error) {
                    outcomes[index].error = error.what();
                }
            }
        };
        std::vector<std::thread> threads;
        const auto count = std::min<std::size_t>(workers, securities.size());
        for (std::size_t index = 0; index < count; ++index) threads.emplace_back(worker);
        for (auto& thread : threads) thread.join();
        for (std::size_t index = 0; index < outcomes.size(); ++index) {
            if (outcomes[index].error.empty()) klines.push_back(std::move(outcomes[index].kline));
            else {
                Json row = Json::object(); row["market"] = securities[index].market;
                row["code"] = securities[index].code; row["error"] = outcomes[index].error;
                fetch_errors.push_back(std::move(row));
            }
        }
    } else {
        std::vector<Json> contextualized;
        for (auto& kline : klines) {
            try {
                const auto* market = optional(kline, "market");
                const auto* code = optional(kline, "code");
                if (!market || !market->is_string() ||
                    !code || !code->is_string())
                    throw Error("K-line document needs market/code for adjustment/strategy");
                const auto market_text = market->as_string();
                const auto code_text = code->as_string();
                kline = adjust_security_kline_document(
                    std::move(kline), market_text, code_text, "stock",
                    adjustment_mode, anchor_date, root, {}, timeout,
                    adjustment_cache_ttl, refresh_adjustment);
                attach_strategy_contexts(kline, strategy, root, timeout, nullptr,
                                         point_in_time_finance, mode == "backtest",
                                         library_pointer);
                contextualized.push_back(std::move(kline));
            } catch (const std::exception& error) {
                Json row = Json::object(); row["error"] = error.what();
                for (const auto* key : {"market", "code", "name"})
                    if (const auto* value = optional(kline, key)) row[key] = *value;
                fetch_errors.push_back(std::move(row));
            }
        }
        klines = std::move(contextualized);
    }
    if (mode == "backtest" && fetch_errors.size())
        throw Error("strategy portfolio backtest requires a complete fixed universe");
    Json result = mode == "scan"
        ? scan_formula_strategy_documents(klines, strategy, lookback)
        : backtest_formula_strategy_documents(klines, strategy, initial,
                                              commission, slippage);
    result["manifest"] = path_utf8(from_utf8(strategy_file));
    result["requested_count"] = static_cast<std::uint64_t>(klines.size() + fetch_errors.size());
    result["fetch_error_count"] = static_cast<std::uint64_t>(fetch_errors.size());
    result["fetch_errors"] = std::move(fetch_errors);
    result["point_in_time_finance"] = point_in_time_finance;
    result["include_user"] = include_user;
    const auto effective_adjustment_mode =
        uniform_kline_adjustment_mode(klines, adjustment_mode);
    result["adjustment_mode"] = effective_adjustment_mode;
    if (effective_adjustment_mode != "none")
        result["adjustment_summary"] =
            summarize_kline_adjustments(klines, effective_adjustment_mode);
    const auto text = result.dump(compact ? -1 : 2) + "\n";
    if (output.empty()) std::cout << text;
    else atomic_write_text(from_utf8(output), text);
    return 0;
}

}  // namespace tdx
