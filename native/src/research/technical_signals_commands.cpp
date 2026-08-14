#include "technical_signals_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {
using namespace technical_signals_detail;
int command_market_technical_signals(const std::vector<std::string>& values) {
    Args args(values);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market technical-signals --view VIEW [options]\n\n"
            "Views: nine-turn, rps-stock, rps-block, new-high, new-low, breakout,\n"
            "       strong-start, trend-up, trend-down, event-driven, model-new-high,\n"
            "       two-day-event, liquidity-space, limit-break, low-turnover-chase,\n"
            "       high-turnover-chase, high-liquidity-enhance, weak-limit-reversal,\n"
            "       failed-limit-reversal, auction-bottom-reversal, upper-shadow-engulf,\n"
            "       auction-volume-spike, limit-up-gap, five-minute-volume-surge,\n"
            "       accumulation-surge, new-high-low-120, ma120-cross,\n"
            "       ma-break-reclaim, pullback-rally, ma-support-pressure,\n"
            "       intraday-opportunity, t0-opportunity\n\n"
            "Options:\n"
            "  --market sz|sh|bj --code N   Reverse-query all thirty-two views for one security\n"
            "  --direction all|up|down       Nine-turn direction\n"
            "  --duration1/2/3 N --rps1/2/3 N\n"
            "  --index-period N --history-period N --retracement N\n"
            "  --sideways-period N --amplitude N --breakout-period N\n"
            "  --board main|gem|star|bj      Trend page board branch\n"
            "  --raw                          Skip client XML filters\n"
            "  --quotes                       Optionally enrich 200661/200662 lists with public L1 quotes\n"
            "  --snapshot FILE                Compare and atomically update a view snapshot\n"
            "  --refresh --limit N --timeout-ms N --output FILE --compact\n";
        return 0;
    }
    TechnicalSignalsQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "nine-turn")));
    query.direction = lower_ascii(trim(args.take_option("--direction", "all")));
    query.board = lower_ascii(trim(args.take_option("--board", "main")));
    const int default_rps = query.view == "rps-block" ? 85 : 90;
    query.duration1 = bounded(args.take_option("--duration1", "10"), "--duration1", 1, 10000);
    query.rps1 = bounded(args.take_option("--rps1", std::to_string(default_rps)), "--rps1", 0, 100);
    query.duration2 = bounded(args.take_option("--duration2", "20"), "--duration2", 1, 10000);
    query.rps2 = bounded(args.take_option("--rps2", std::to_string(default_rps)), "--rps2", 0, 100);
    query.duration3 = bounded(args.take_option("--duration3", "60"), "--duration3", 1, 10000);
    query.rps3 = bounded(args.take_option("--rps3", std::to_string(default_rps)), "--rps3", 0, 100);
    query.index_period = bounded(args.take_option("--index-period", "2"), "--index-period", 1, 10000);
    query.history_period = bounded(args.take_option("--history-period", "120"), "--history-period", 1, 10000);
    query.retracement = bounded(args.take_option("--retracement", "5"), "--retracement", 0, 100);
    query.sideways_period = bounded(args.take_option("--sideways-period", "20"), "--sideways-period", 1, 10000);
    query.amplitude = bounded(args.take_option("--amplitude", "10"), "--amplitude", 0, 100);
    query.breakout_period = bounded(args.take_option("--breakout-period", "2"), "--breakout-period", 1, 10000);
    query.apply_client_filters = !args.take_flag("--raw");
    query.enrich_quotes = args.take_flag("--quotes");
    query.refresh = args.take_flag("--refresh");
    query.limit = bounded(args.take_option("--limit", "5000"), "--limit", 1, 10000);
    query.cache_ttl_seconds = bounded(args.take_option("--cache-ttl-seconds", "15"),
                                      "--cache-ttl-seconds", 0, 3600);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 600000);
    const auto market = lower_ascii(trim(args.take_option("--market")));
    const auto code = trim(args.take_option("--code"));
    const auto snapshot_text = args.take_option("--snapshot");
    const auto root_text = args.take_option("--root");
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-technical-signals.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    TechnicalSignalsService service(root, load_blocks(
        root, {"industry", "research-industry", "concept", "style", "index"}));
    if (market.empty() != code.empty())
        throw Error("--market and --code must be provided together");
    if (!market.empty() && !snapshot_text.empty())
        throw Error("--snapshot is only valid for one list view, not a security reverse query");
    if (!market.empty() && query.enrich_quotes)
        throw Error("--quotes is only valid for one 200661/200662 list view");
    Json document;
    if (!market.empty()) {
        TechnicalSignalSecurityQuery security_query;
        security_query.market = market;
        security_query.code = code;
        security_query.apply_client_filters = query.apply_client_filters;
        security_query.refresh = query.refresh;
        security_query.cache_ttl_seconds = query.cache_ttl_seconds;
        security_query.timeout_ms = query.timeout_ms;
        document = service.query_security(security_query);
    } else {
        document = service.query(query);
        if (!snapshot_text.empty()) {
            const auto snapshot = native_path(snapshot_text);
            if (fs::absolute(snapshot).lexically_normal() ==
                fs::absolute(output).lexically_normal())
                throw Error("--snapshot and --output must use different files");
            document["snapshot"] = update_technical_signal_snapshot(snapshot, document);
        }
    }
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    if (!market.empty()) {
        std::cout << "matched " << document.at("counts").at("hits").as_number()
                  << " signals across "
                  << document.at("counts").at("views_successful").as_number()
                  << " available views -> " << path_utf8(output) << '\n';
    } else {
        std::cout << "received " << document.at("counts").at("returned").as_number()
                  << " technical signal rows -> " << path_utf8(output) << '\n';
    }
    return 0;
}


}  // namespace tdx
