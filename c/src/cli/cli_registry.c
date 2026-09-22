/* Typed adapters and command-local option contracts. */
#include "cli_commands.h"
#include "tdx_pool.h"
#include "tdx_hub.h"
#include <string.h>

static const cli_option_spec options_probe[] = {
    {"--endpoints", CLI_SIZE, offsetof(cli_probe_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_probe_options, common.pool), 0, 0, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_probe_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_probe_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_probe_options, common.root), 0, 0, 0, 0},
    {"--security", CLI_SECURITY, offsetof(cli_probe_options, common.securities), 0, 0, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_probe_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_probe(void *raw) {
    cli_probe_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_probe(const void *raw, tdx_error *err) {
    return cli_command_probe((const cli_probe_options *)raw, err);
}

static const cli_option_spec options_securities[] = {
    {"--category", CLI_CATEGORY, offsetof(cli_securities_options, common.category), 0, 0, 0, 0},
    {"--endpoints", CLI_SIZE, offsetof(cli_securities_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_securities_options, common.pool), 0, 0, 0, 0},
    {"--limit", CLI_SIZE, offsetof(cli_securities_options, common.limit), 1, 100000, 0, 0},
    {"--market", CLI_MARKETS, offsetof(cli_securities_options, common.markets), 0, 0, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_securities_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_securities_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_securities_options, common.root), 0, 0, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_securities_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_securities(void *raw) {
    cli_securities_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_securities(const void *raw, tdx_error *err) {
    return cli_command_securities((const cli_securities_options *)raw, err);
}

static const cli_option_spec options_sweep[] = {
    {"--batch-size", CLI_SIZE, offsetof(cli_sweep_options, batch_size), 1, 100, 0, 0},
    {"--category", CLI_CATEGORY, offsetof(cli_sweep_options, common.category), 0, 0, 0, 0},
    {"--connections", CLI_SIZE, offsetof(cli_sweep_options, connections), 1, 32, 0, 0},
    {"--endpoints", CLI_SIZE, offsetof(cli_sweep_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_sweep_options, common.pool), 0, 0, 0, 0},
    {"--iterations", CLI_SIZE, offsetof(cli_sweep_options, iterations), 0, 1000000, 0, 0},
    {"--limit", CLI_SIZE, offsetof(cli_sweep_options, common.limit), 1, 100000, 0, 0},
    {"--market", CLI_MARKETS, offsetof(cli_sweep_options, common.markets), 0, 0, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_sweep_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_sweep_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_sweep_options, common.root), 0, 0, 0, 0},
    {"--security", CLI_SECURITY, offsetof(cli_sweep_options, common.securities), 0, 0, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_sweep_options, common.timeout_ms), 100, 600000, 0, 0},
    {"-j", CLI_SIZE, offsetof(cli_sweep_options, connections), 1, 32, 0, 0},
};
static void initialize_sweep(void *raw) {
    cli_sweep_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
    options->connections = 6;
    options->batch_size = TDX_DEPTH_BATCH_MAX;
    options->iterations = 1;
}
static int run_sweep(const void *raw, tdx_error *err) {
    return cli_command_sweep((const cli_sweep_options *)raw, err);
}

static const cli_option_spec options_watch[] = {
    {"--batch-size", CLI_SIZE, offsetof(cli_watch_options, batch_size), 1, 100, 0, 0},
    {"--category", CLI_CATEGORY, offsetof(cli_watch_options, common.category), 0, 0, 0, 0},
    {"--connections", CLI_SIZE, offsetof(cli_watch_options, connections), 1, 32, 0, 0},
    {"--endpoints", CLI_SIZE, offsetof(cli_watch_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_watch_options, common.pool), 0, 0, 0, 0},
    {"--interval-ms", CLI_INT, offsetof(cli_watch_options, interval_ms), 0, 600000, 0, 0},
    {"--iterations", CLI_SIZE, offsetof(cli_watch_options, iterations), 0, 1000000, 0, 0},
    {"--limit", CLI_SIZE, offsetof(cli_watch_options, common.limit), 1, 100000, 0, 0},
    {"--market", CLI_MARKETS, offsetof(cli_watch_options, common.markets), 0, 0, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_watch_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_watch_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_watch_options, common.root), 0, 0, 0, 0},
    {"--security", CLI_SECURITY, offsetof(cli_watch_options, common.securities), 0, 0, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_watch_options, common.timeout_ms), 100, 600000, 0, 0},
    {"-j", CLI_SIZE, offsetof(cli_watch_options, connections), 1, 32, 0, 0},
};
static void initialize_watch(void *raw) {
    cli_watch_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
    options->connections = 6;
    options->batch_size = TDX_DEPTH_BATCH_MAX;
    options->iterations = 1;
    options->interval_ms = 1000;
}
static int run_watch(const void *raw, tdx_error *err) {
    return cli_command_watch((const cli_watch_options *)raw, err);
}

static const cli_option_spec options_serve[] = {
    {"--batch-size", CLI_SIZE, offsetof(cli_serve_options, batch_size), 1, 100, 0, 0},
    {"--category", CLI_CATEGORY, offsetof(cli_serve_options, common.category), 0, 0, 0, 0},
    {"--connections", CLI_SIZE, offsetof(cli_serve_options, connections), 1, 32, 0, 0},
    {"--endpoints", CLI_SIZE, offsetof(cli_serve_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--heartbeat-ms", CLI_INT, offsetof(cli_serve_options, heartbeat_ms), 0, 600000, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_serve_options, common.pool), 0, 0, 0, 0},
    {"--idle-interval-ms", CLI_INT, offsetof(cli_serve_options, idle_interval_ms), 0, 600000, 0, 0},
    {"--idle-rounds", CLI_INT, offsetof(cli_serve_options, idle_rounds), 0, 1000000, 0, 0},
    {"--interval-ms", CLI_INT, offsetof(cli_serve_options, interval_ms), 1, 600000, 0, 0},
    {"--limit", CLI_SIZE, offsetof(cli_serve_options, common.limit), 1, 100000, 0, 0},
    {"--market", CLI_MARKETS, offsetof(cli_serve_options, common.markets), 0, 0, 0, 0},
    {"--max-subscribers", CLI_SIZE, offsetof(cli_serve_options, max_subscribers), 1, 64, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_serve_options, common.output), 0, 0, 0, 0},
    {"--port", CLI_INT, offsetof(cli_serve_options, port), 1, 65535, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_serve_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_serve_options, common.root), 0, 0, 0, 0},
    {"--security", CLI_SECURITY, offsetof(cli_serve_options, common.securities), 0, 0, 0, 0},
    {"--tier-warm-ms", CLI_INT, offsetof(cli_serve_options, tier_warm_ms), 0, 600000, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_serve_options, common.timeout_ms), 100, 600000, 0, 0},
    {"-j", CLI_SIZE, offsetof(cli_serve_options, connections), 1, 32, 0, 0},
};
static void initialize_serve(void *raw) {
    cli_serve_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
    options->connections = 6;
    options->batch_size = TDX_DEPTH_BATCH_MAX;
    options->interval_ms = 1000;
    options->max_subscribers = 16;
    options->heartbeat_ms = TDX_HUB_DEFAULT_HEARTBEAT_MS;
    options->idle_rounds = 30;
    options->port = 8790;
}
static int run_serve(const void *raw, tdx_error *err) {
    return cli_command_serve((const cli_serve_options *)raw, err);
}

static const cli_option_spec options_day[] = {
    {"--cache-dir", CLI_STRING, offsetof(cli_day_options, cache_dir), 0, 0, 0, 0},
    {"--changed-only", CLI_FLAG, offsetof(cli_day_options, changed_only), 0, 0, 1, 0},
    {"--date", CLI_STRING, offsetof(cli_day_options, date), 0, 0, 0, 0},
    {"--endpoints", CLI_SIZE, offsetof(cli_day_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_day_options, common.pool), 0, 0, 0, 0},
    {"--max-records", CLI_SIZE, offsetof(cli_day_options, max_records), 1, 10000000, 0, 0},
    {"--no-cache", CLI_FLAG, offsetof(cli_day_options, no_cache), 0, 0, 1, 0},
    {"--output", CLI_STRING, offsetof(cli_day_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_day_options, common.quiet), 0, 0, 1, 0},
    {"--raw", CLI_FLAG, offsetof(cli_day_options, raw_tags), 0, 0, 1, 0},
    {"--refresh", CLI_FLAG, offsetof(cli_day_options, refresh), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_day_options, common.root), 0, 0, 0, 0},
    {"--security", CLI_SECURITY, offsetof(cli_day_options, common.securities), 0, 0, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_day_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_day(void *raw) {
    cli_day_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_day(const void *raw, tdx_error *err) {
    return cli_command_day((const cli_day_options *)raw, err);
}

static const cli_option_spec options_trades[] = {
    {"--date", CLI_STRING, offsetof(cli_trades_options, date), 0, 0, 0, 0},
    {"--endpoints", CLI_SIZE, offsetof(cli_trades_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_trades_options, common.pool), 0, 0, 0, 0},
    {"--max-pages", CLI_INT, offsetof(cli_trades_options, max_pages), 1, 100, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_trades_options, common.output), 0, 0, 0, 0},
    {"--page-size", CLI_INT, offsetof(cli_trades_options, page_size), 1, 65535, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_trades_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_trades_options, common.root), 0, 0, 0, 0},
    {"--security", CLI_SECURITY, offsetof(cli_trades_options, common.securities), 0, 0, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_trades_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_trades(void *raw) {
    cli_trades_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_trades(const void *raw, tdx_error *err) {
    return cli_command_trades((const cli_trades_options *)raw, err);
}

static const cli_option_spec options_kline[] = {
    {"--date", CLI_STRING, offsetof(cli_kline_options, date), 0, 0, 0, 0},
    {"--endpoints", CLI_SIZE, offsetof(cli_kline_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_kline_options, common.pool), 0, 0, 0, 0},
    {"--index", CLI_FLAG, offsetof(cli_kline_options, index_mode), 0, 0, 1, 0},
    {"--lc1-output", CLI_STRING, offsetof(cli_kline_options, lc1_output), 0, 0, 0, 0},
    {"--max-pages", CLI_INT, offsetof(cli_kline_options, max_pages), 1, 100, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_kline_options, common.output), 0, 0, 0, 0},
    {"--page-size", CLI_INT, offsetof(cli_kline_options, page_size), 1, 65535, 0, 0},
    {"--period", CLI_STRING, offsetof(cli_kline_options, period), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_kline_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_kline_options, common.root), 0, 0, 0, 0},
    {"--security", CLI_SECURITY, offsetof(cli_kline_options, common.securities), 0, 0, 0, 0},
    {"--start", CLI_INT, offsetof(cli_kline_options, start), 0, 65535, 0, 0},
    {"--stock", CLI_FLAG, offsetof(cli_kline_options, index_mode), 0, 0, -1, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_kline_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_kline(void *raw) {
    cli_kline_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_kline(const void *raw, tdx_error *err) {
    return cli_command_kline((const cli_kline_options *)raw, err);
}

static const cli_option_spec options_timeline[] = {
    {"--date", CLI_STRING, offsetof(cli_timeline_options, date), 0, 0, 0, 0},
    {"--endpoints", CLI_SIZE, offsetof(cli_timeline_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_timeline_options, common.pool), 0, 0, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_timeline_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_timeline_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_timeline_options, common.root), 0, 0, 0, 0},
    {"--security", CLI_SECURITY, offsetof(cli_timeline_options, common.securities), 0, 0, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_timeline_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_timeline(void *raw) {
    cli_timeline_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_timeline(const void *raw, tdx_error *err) {
    return cli_command_timeline((const cli_timeline_options *)raw, err);
}

static const cli_option_spec options_auction[] = {
    {"--endpoints", CLI_SIZE, offsetof(cli_auction_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_auction_options, common.pool), 0, 0, 0, 0},
    {"--limit", CLI_SIZE, offsetof(cli_auction_options, common.limit), 1, 100000, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_auction_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_auction_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_auction_options, common.root), 0, 0, 0, 0},
    {"--security", CLI_SECURITY, offsetof(cli_auction_options, common.securities), 0, 0, 0, 0},
    {"--selector", CLI_UNSIGNED, offsetof(cli_auction_options, selector), 0, 1000000, 0, 0},
    {"--start", CLI_INT, offsetof(cli_auction_options, start), 0, 65535, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_auction_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_auction(void *raw) {
    cli_auction_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_auction(const void *raw, tdx_error *err) {
    return cli_command_auction((const cli_auction_options *)raw, err);
}

static const cli_option_spec options_snapshot[] = {
    {"--batch-size", CLI_SIZE, offsetof(cli_snapshot_options, batch_size), 1, 100, 0, 0},
    {"--category", CLI_CATEGORY, offsetof(cli_snapshot_options, common.category), 0, 0, 0, 0},
    {"--endpoints", CLI_SIZE, offsetof(cli_snapshot_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_snapshot_options, common.pool), 0, 0, 0, 0},
    {"--limit", CLI_SIZE, offsetof(cli_snapshot_options, common.limit), 1, 100000, 0, 0},
    {"--market", CLI_MARKETS, offsetof(cli_snapshot_options, common.markets), 0, 0, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_snapshot_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_snapshot_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_snapshot_options, common.root), 0, 0, 0, 0},
    {"--security", CLI_SECURITY, offsetof(cli_snapshot_options, common.securities), 0, 0, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_snapshot_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_snapshot(void *raw) {
    cli_snapshot_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
    options->batch_size = TDX_DEPTH_BATCH_MAX;
}
static int run_snapshot(const void *raw, tdx_error *err) {
    return cli_command_snapshot((const cli_snapshot_options *)raw, err);
}

static const cli_option_spec options_finance[] = {
    {"--batch-size", CLI_SIZE, offsetof(cli_finance_options, batch_size), 1, 100, 0, 0},
    {"--category", CLI_CATEGORY, offsetof(cli_finance_options, common.category), 0, 0, 0, 0},
    {"--endpoints", CLI_SIZE, offsetof(cli_finance_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_finance_options, common.pool), 0, 0, 0, 0},
    {"--limit", CLI_SIZE, offsetof(cli_finance_options, common.limit), 1, 100000, 0, 0},
    {"--market", CLI_MARKETS, offsetof(cli_finance_options, common.markets), 0, 0, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_finance_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_finance_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_finance_options, common.root), 0, 0, 0, 0},
    {"--security", CLI_SECURITY, offsetof(cli_finance_options, common.securities), 0, 0, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_finance_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_finance(void *raw) {
    cli_finance_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
    options->batch_size = TDX_DEPTH_BATCH_MAX;
}
static int run_finance(const void *raw, tdx_error *err) {
    return cli_command_finance((const cli_finance_options *)raw, err);
}

static const cli_option_spec options_capital[] = {
    {"--category", CLI_CATEGORY, offsetof(cli_capital_options, common.category), 0, 0, 0, 0},
    {"--endpoints", CLI_SIZE, offsetof(cli_capital_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--gbbq", CLI_STRING, offsetof(cli_capital_options, gbbq_path), 0, 0, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_capital_options, common.pool), 0, 0, 0, 0},
    {"--limit", CLI_SIZE, offsetof(cli_capital_options, common.limit), 1, 100000, 0, 0},
    {"--local", CLI_FLAG, offsetof(cli_capital_options, capital_local), 0, 0, 1, 0},
    {"--market", CLI_MARKETS, offsetof(cli_capital_options, common.markets), 0, 0, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_capital_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_capital_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_capital_options, common.root), 0, 0, 0, 0},
    {"--security", CLI_SECURITY, offsetof(cli_capital_options, common.securities), 0, 0, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_capital_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_capital(void *raw) {
    cli_capital_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_capital(const void *raw, tdx_error *err) {
    return cli_command_capital((const cli_capital_options *)raw, err);
}

static const cli_option_spec options_limits[] = {
    {"--endpoints", CLI_SIZE, offsetof(cli_limits_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_limits_options, common.pool), 0, 0, 0, 0},
    {"--max-records", CLI_SIZE, offsetof(cli_limits_options, max_records), 1, 10000000, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_limits_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_limits_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_limits_options, common.root), 0, 0, 0, 0},
    {"--start", CLI_INT, offsetof(cli_limits_options, start), 0, 65535, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_limits_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_limits(void *raw) {
    cli_limits_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_limits(const void *raw, tdx_error *err) {
    return cli_command_limits((const cli_limits_options *)raw, err);
}

static const cli_option_spec options_jsn[] = {
    {"--bonds", CLI_FLAG, offsetof(cli_jsn_options, bonds), 0, 0, 1, 0},
    {"--convertible", CLI_FLAG, offsetof(cli_jsn_options, convertible), 0, 0, 1, 0},
    {"--endpoints", CLI_SIZE, offsetof(cli_jsn_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_jsn_options, common.pool), 0, 0, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_jsn_options, common.output), 0, 0, 0, 0},
    {"--prefix", CLI_STRING, offsetof(cli_jsn_options, prefix), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_jsn_options, common.quiet), 0, 0, 1, 0},
    {"--resource", CLI_STRING, offsetof(cli_jsn_options, resource), 0, 0, 0, 0},
    {"--root", CLI_ROOT, offsetof(cli_jsn_options, common.root), 0, 0, 0, 0},
    {"--schedule", CLI_FLAG, offsetof(cli_jsn_options, schedule), 0, 0, 1, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_jsn_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_jsn(void *raw) {
    cli_jsn_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_jsn(const void *raw, tdx_error *err) {
    return cli_command_jsn((const cli_jsn_options *)raw, err);
}

static const cli_option_spec options_convertible[] = {
    {"--endpoints", CLI_SIZE, offsetof(cli_convertible_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_convertible_options, common.pool), 0, 0, 0, 0},
    {"--max-records", CLI_SIZE, offsetof(cli_convertible_options, max_records), 1, 10000000, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_convertible_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_convertible_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_convertible_options, common.root), 0, 0, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_convertible_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_convertible(void *raw) {
    cli_convertible_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_convertible(const void *raw, tdx_error *err) {
    return cli_command_convertible((const cli_convertible_options *)raw, err);
}

static const cli_option_spec options_pending[] = {
    {"--endpoints", CLI_SIZE, offsetof(cli_pending_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_pending_options, common.pool), 0, 0, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_pending_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_pending_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_pending_options, common.root), 0, 0, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_pending_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_pending(void *raw) {
    cli_pending_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_pending(const void *raw, tdx_error *err) {
    return cli_command_pending((const cli_pending_options *)raw, err);
}

static const cli_option_spec options_subscription[] = {
    {"--endpoints", CLI_SIZE, offsetof(cli_subscription_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_subscription_options, common.pool), 0, 0, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_subscription_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_subscription_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_subscription_options, common.root), 0, 0, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_subscription_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_subscription(void *raw) {
    cli_subscription_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_subscription(const void *raw, tdx_error *err) {
    return cli_command_subscription((const cli_subscription_options *)raw, err);
}

static const cli_option_spec options_pricing[] = {
    {"--date", CLI_STRING, offsetof(cli_pricing_options, date), 0, 0, 0, 0},
    {"--endpoints", CLI_SIZE, offsetof(cli_pricing_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_pricing_options, common.pool), 0, 0, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_pricing_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_pricing_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_pricing_options, common.root), 0, 0, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_pricing_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_pricing(void *raw) {
    cli_pricing_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_pricing(const void *raw, tdx_error *err) {
    return cli_command_pricing((const cli_pricing_options *)raw, err);
}

static const cli_option_spec options_newbond[] = {
    {"--endpoints", CLI_SIZE, offsetof(cli_newbond_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_newbond_options, common.pool), 0, 0, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_newbond_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_newbond_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_newbond_options, common.root), 0, 0, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_newbond_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_newbond(void *raw) {
    cli_newbond_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_newbond(const void *raw, tdx_error *err) {
    return cli_command_newbond((const cli_newbond_options *)raw, err);
}

static const cli_option_spec options_professional[] = {
    {"--code", CLI_STRING, offsetof(cli_professional_options, code), 0, 0, 0, 0},
    {"--field", CLI_UNSIGNED, offsetof(cli_professional_options, field_id), 0, 65535, 0, 0},
    {"--from", CLI_DATE, offsetof(cli_professional_options, from_date), 0, 99991231, 0, 0},
    {"--input", CLI_STRING, offsetof(cli_professional_options, input), 0, 0, 0, 0},
    {"--kind", CLI_STRING, offsetof(cli_professional_options, kind), 0, 0, 0, 0},
    {"--limit", CLI_SIZE, offsetof(cli_professional_options, common.limit), 1, 100000, 0, 0},
    {"--member", CLI_STRING, offsetof(cli_professional_options, zip_member), 0, 0, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_professional_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_professional_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_professional_options, common.root), 0, 0, 0, 0},
    {"--to", CLI_DATE, offsetof(cli_professional_options, to_date), 0, 99991231, 0, 0},
    {"--zip", CLI_STRING, offsetof(cli_professional_options, zip), 0, 0, 0, 0},
};
static void initialize_professional(void *raw) {
    cli_professional_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_professional(const void *raw, tdx_error *err) {
    return cli_command_professional((const cli_professional_options *)raw, err);
}

static const cli_option_spec options_daily[] = {
    {"--input", CLI_STRING, offsetof(cli_daily_options, input), 0, 0, 0, 0},
    {"--max-records", CLI_SIZE, offsetof(cli_daily_options, max_records), 1, 10000000, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_daily_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_daily_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_daily_options, common.root), 0, 0, 0, 0},
    {"--security", CLI_SECURITY, offsetof(cli_daily_options, common.securities), 0, 0, 0, 0},
};
static void initialize_daily(void *raw) {
    cli_daily_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_daily(const void *raw, tdx_error *err) {
    return cli_command_daily((const cli_daily_options *)raw, err);
}

static const cli_option_spec options_minute[] = {
    {"--input", CLI_STRING, offsetof(cli_minute_options, input), 0, 0, 0, 0},
    {"--max-records", CLI_SIZE, offsetof(cli_minute_options, max_records), 1, 10000000, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_minute_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_minute_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_minute_options, common.root), 0, 0, 0, 0},
    {"--security", CLI_SECURITY, offsetof(cli_minute_options, common.securities), 0, 0, 0, 0},
};
static void initialize_minute(void *raw) {
    cli_minute_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_minute(const void *raw, tdx_error *err) {
    return cli_command_minute((const cli_minute_options *)raw, err);
}

static const cli_option_spec options_industry[] = {
    {"--bonds", CLI_FLAG, offsetof(cli_industry_options, bonds), 0, 0, 1, 0},
    {"--endpoints", CLI_SIZE, offsetof(cli_industry_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_industry_options, common.pool), 0, 0, 0, 0},
    {"--max-records", CLI_SIZE, offsetof(cli_industry_options, max_records), 1, 10000000, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_industry_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_industry_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_industry_options, common.root), 0, 0, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_industry_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_industry(void *raw) {
    cli_industry_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_industry(const void *raw, tdx_error *err) {
    return cli_command_industry((const cli_industry_options *)raw, err);
}

static const cli_option_spec options_limit[] = {
    {"--date", CLI_STRING, offsetof(cli_limit_options, date), 0, 0, 0, 0},
    {"--name", CLI_STRING, offsetof(cli_limit_options, name), 0, 0, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_limit_options, common.output), 0, 0, 0, 0},
    {"--prev", CLI_REAL, offsetof(cli_limit_options, previous_close), 0, 0, 0, offsetof(cli_limit_options, has_previous_close)},
    {"--quiet", CLI_FLAG, offsetof(cli_limit_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_limit_options, common.root), 0, 0, 0, 0},
    {"--security", CLI_SECURITY, offsetof(cli_limit_options, common.securities), 0, 0, 0, 0},
};
static void initialize_limit(void *raw) {
    cli_limit_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_limit(const void *raw, tdx_error *err) {
    return cli_command_limit((const cli_limit_options *)raw, err);
}

static const cli_option_spec options_valuation[] = {
    {"--endpoints", CLI_SIZE, offsetof(cli_valuation_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_valuation_options, common.pool), 0, 0, 0, 0},
    {"--max-records", CLI_SIZE, offsetof(cli_valuation_options, max_records), 1, 10000000, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_valuation_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_valuation_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_valuation_options, common.root), 0, 0, 0, 0},
    {"--security", CLI_SECURITY, offsetof(cli_valuation_options, common.securities), 0, 0, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_valuation_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_valuation(void *raw) {
    cli_valuation_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_valuation(const void *raw, tdx_error *err) {
    return cli_command_valuation((const cli_valuation_options *)raw, err);
}

static const cli_option_spec options_ranking[] = {
    {"--ascending", CLI_FLAG, offsetof(cli_ranking_options, ascending), 0, 0, 1, 0},
    {"--category", CLI_CATEGORY, offsetof(cli_ranking_options, common.category), 0, 0, 0, 0},
    {"--endpoints", CLI_SIZE, offsetof(cli_ranking_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--field", CLI_UNSIGNED, offsetof(cli_ranking_options, field_id), 0, 65535, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_ranking_options, common.pool), 0, 0, 0, 0},
    {"--limit", CLI_SIZE, offsetof(cli_ranking_options, common.limit), 1, 100000, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_ranking_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_ranking_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_ranking_options, common.root), 0, 0, 0, 0},
    {"--sort", CLI_STRING, offsetof(cli_ranking_options, sort), 0, 0, 0, 0},
    {"--start", CLI_INT, offsetof(cli_ranking_options, start), 0, 65535, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_ranking_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_ranking(void *raw) {
    cli_ranking_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_ranking(const void *raw, tdx_error *err) {
    return cli_command_ranking((const cli_ranking_options *)raw, err);
}

static const cli_option_spec options_seal[] = {
    {"--date", CLI_STRING, offsetof(cli_seal_options, date), 0, 0, 0, 0},
    {"--endpoints", CLI_SIZE, offsetof(cli_seal_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_seal_options, common.pool), 0, 0, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_seal_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_seal_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_seal_options, common.root), 0, 0, 0, 0},
    {"--security", CLI_SECURITY, offsetof(cli_seal_options, common.securities), 0, 0, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_seal_options, common.timeout_ms), 100, 600000, 0, 0},
};
static void initialize_seal(void *raw) {
    cli_seal_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_seal(const void *raw, tdx_error *err) {
    return cli_command_seal((const cli_seal_options *)raw, err);
}

static const cli_option_spec options_panorama[] = {
    {"--endpoints", CLI_SIZE, offsetof(cli_panorama_options, common.endpoint_limit), 1, 64, 0, 0},
    {"--host", CLI_HOST, offsetof(cli_panorama_options, common.pool), 0, 0, 0, 0},
    {"--max-records", CLI_SIZE, offsetof(cli_panorama_options, max_records), 1, 10000000, 0, 0},
    {"--output", CLI_STRING, offsetof(cli_panorama_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_panorama_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_panorama_options, common.root), 0, 0, 0, 0},
    {"--timeout-ms", CLI_INT, offsetof(cli_panorama_options, common.timeout_ms), 100, 600000, 0, 0},
    {"--view", CLI_STRING, offsetof(cli_panorama_options, view), 0, 0, 0, 0},
};
static void initialize_panorama(void *raw) {
    cli_panorama_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_panorama(const void *raw, tdx_error *err) {
    return cli_command_panorama((const cli_panorama_options *)raw, err);
}

static const cli_option_spec options_blocks[] = {
    {"--assignments", CLI_FLAG, offsetof(cli_blocks_options, show_assignments), 0, 0, 1, 0},
    {"--expanded", CLI_FLAG, offsetof(cli_blocks_options, show_expanded), 0, 0, 1, 0},
    {"--max-records", CLI_SIZE, offsetof(cli_blocks_options, max_records), 1, 10000000, 0, 0},
    {"--members", CLI_FLAG, offsetof(cli_blocks_options, show_members), 0, 0, 1, 0},
    {"--output", CLI_STRING, offsetof(cli_blocks_options, common.output), 0, 0, 0, 0},
    {"--quiet", CLI_FLAG, offsetof(cli_blocks_options, common.quiet), 0, 0, 1, 0},
    {"--root", CLI_ROOT, offsetof(cli_blocks_options, common.root), 0, 0, 0, 0},
};
static void initialize_blocks(void *raw) {
    cli_blocks_options *options = raw;
    memset(options, 0, sizeof(*options));
    cli_common_init(&options->common);
}
static int run_blocks(const void *raw, tdx_error *err) {
    return cli_command_blocks((const cli_blocks_options *)raw, err);
}

const cli_command cli_commands[] = {
    {"probe", sizeof(cli_probe_options), options_probe, sizeof(options_probe) / sizeof(options_probe[0]), initialize_probe, run_probe, 1},
    {"securities", sizeof(cli_securities_options), options_securities, sizeof(options_securities) / sizeof(options_securities[0]), initialize_securities, run_securities, 1},
    {"sweep", sizeof(cli_sweep_options), options_sweep, sizeof(options_sweep) / sizeof(options_sweep[0]), initialize_sweep, run_sweep, 1},
    {"watch", sizeof(cli_watch_options), options_watch, sizeof(options_watch) / sizeof(options_watch[0]), initialize_watch, run_watch, 1},
    {"serve", sizeof(cli_serve_options), options_serve, sizeof(options_serve) / sizeof(options_serve[0]), initialize_serve, run_serve, 1},
    {"day", sizeof(cli_day_options), options_day, sizeof(options_day) / sizeof(options_day[0]), initialize_day, run_day, 1},
    {"trades", sizeof(cli_trades_options), options_trades, sizeof(options_trades) / sizeof(options_trades[0]), initialize_trades, run_trades, 1},
    {"kline", sizeof(cli_kline_options), options_kline, sizeof(options_kline) / sizeof(options_kline[0]), initialize_kline, run_kline, 1},
    {"timeline", sizeof(cli_timeline_options), options_timeline, sizeof(options_timeline) / sizeof(options_timeline[0]), initialize_timeline, run_timeline, 1},
    {"auction", sizeof(cli_auction_options), options_auction, sizeof(options_auction) / sizeof(options_auction[0]), initialize_auction, run_auction, 1},
    {"snapshot", sizeof(cli_snapshot_options), options_snapshot, sizeof(options_snapshot) / sizeof(options_snapshot[0]), initialize_snapshot, run_snapshot, 1},
    {"finance", sizeof(cli_finance_options), options_finance, sizeof(options_finance) / sizeof(options_finance[0]), initialize_finance, run_finance, 1},
    {"capital", sizeof(cli_capital_options), options_capital, sizeof(options_capital) / sizeof(options_capital[0]), initialize_capital, run_capital, 1},
    {"limits", sizeof(cli_limits_options), options_limits, sizeof(options_limits) / sizeof(options_limits[0]), initialize_limits, run_limits, 1},
    {"jsn", sizeof(cli_jsn_options), options_jsn, sizeof(options_jsn) / sizeof(options_jsn[0]), initialize_jsn, run_jsn, 1},
    {"convertible", sizeof(cli_convertible_options), options_convertible, sizeof(options_convertible) / sizeof(options_convertible[0]), initialize_convertible, run_convertible, 1},
    {"pending", sizeof(cli_pending_options), options_pending, sizeof(options_pending) / sizeof(options_pending[0]), initialize_pending, run_pending, 1},
    {"subscription", sizeof(cli_subscription_options), options_subscription, sizeof(options_subscription) / sizeof(options_subscription[0]), initialize_subscription, run_subscription, 1},
    {"pricing", sizeof(cli_pricing_options), options_pricing, sizeof(options_pricing) / sizeof(options_pricing[0]), initialize_pricing, run_pricing, 1},
    {"newbond", sizeof(cli_newbond_options), options_newbond, sizeof(options_newbond) / sizeof(options_newbond[0]), initialize_newbond, run_newbond, 1},
    {"professional", sizeof(cli_professional_options), options_professional, sizeof(options_professional) / sizeof(options_professional[0]), initialize_professional, run_professional, 0},
    {"daily", sizeof(cli_daily_options), options_daily, sizeof(options_daily) / sizeof(options_daily[0]), initialize_daily, run_daily, 0},
    {"minute", sizeof(cli_minute_options), options_minute, sizeof(options_minute) / sizeof(options_minute[0]), initialize_minute, run_minute, 0},
    {"industry", sizeof(cli_industry_options), options_industry, sizeof(options_industry) / sizeof(options_industry[0]), initialize_industry, run_industry, 1},
    {"limit", sizeof(cli_limit_options), options_limit, sizeof(options_limit) / sizeof(options_limit[0]), initialize_limit, run_limit, 0},
    {"valuation", sizeof(cli_valuation_options), options_valuation, sizeof(options_valuation) / sizeof(options_valuation[0]), initialize_valuation, run_valuation, 1},
    {"ranking", sizeof(cli_ranking_options), options_ranking, sizeof(options_ranking) / sizeof(options_ranking[0]), initialize_ranking, run_ranking, 1},
    {"seal", sizeof(cli_seal_options), options_seal, sizeof(options_seal) / sizeof(options_seal[0]), initialize_seal, run_seal, 1},
    {"panorama", sizeof(cli_panorama_options), options_panorama, sizeof(options_panorama) / sizeof(options_panorama[0]), initialize_panorama, run_panorama, 1},
    {"blocks", sizeof(cli_blocks_options), options_blocks, sizeof(options_blocks) / sizeof(options_blocks[0]), initialize_blocks, run_blocks, 0},
};
const size_t cli_command_count = sizeof(cli_commands) / sizeof(cli_commands[0]);
const cli_command *cli_find_command(const char *name) {
    for (size_t i = 0; i < cli_command_count; ++i)
        if (strcmp(name, cli_commands[i].name) == 0) return &cli_commands[i];
    return NULL;
}
