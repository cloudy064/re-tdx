/* Command-specific configuration. Owned by the CLI invocation. */
#ifndef TDX_CLI_COMMANDS_H
#define TDX_CLI_COMMANDS_H
#include "cli.h"

typedef struct cli_probe_options {
    cli_common_options common;
} cli_probe_options;
int cli_command_probe(const cli_probe_options *options, tdx_error *err);

typedef struct cli_securities_options {
    cli_common_options common;
} cli_securities_options;
int cli_command_securities(const cli_securities_options *options, tdx_error *err);

typedef struct cli_sweep_options {
    cli_common_options common;
    size_t connections;
    size_t batch_size;
    size_t iterations;
} cli_sweep_options;
int cli_command_sweep(const cli_sweep_options *options, tdx_error *err);

typedef struct cli_watch_options {
    cli_common_options common;
    size_t connections;
    size_t batch_size;
    size_t iterations;
    int interval_ms;
} cli_watch_options;
int cli_command_watch(const cli_watch_options *options, tdx_error *err);

typedef struct cli_serve_options {
    cli_common_options common;
    size_t connections;
    size_t batch_size;
    int interval_ms;
    size_t max_subscribers;
    int heartbeat_ms;
    int tier_warm_ms;
    int idle_interval_ms;
    int idle_rounds;
    int port;
} cli_serve_options;
int cli_command_serve(const cli_serve_options *options, tdx_error *err);

typedef struct cli_day_options {
    cli_common_options common;
    const char *date;
    const char *cache_dir;
    size_t max_records;
    int refresh;
    int no_cache;
    int raw_tags;
    int changed_only;
} cli_day_options;
int cli_command_day(const cli_day_options *options, tdx_error *err);

typedef struct cli_trades_options {
    cli_common_options common;
    const char *date;
    int page_size;
    int max_pages;
} cli_trades_options;
int cli_command_trades(const cli_trades_options *options, tdx_error *err);

typedef struct cli_kline_options {
    cli_common_options common;
    const char *date;
    int page_size;
    int max_pages;
    const char *period;
    int start;
    int index_mode;
    const char *lc1_output;
} cli_kline_options;
int cli_command_kline(const cli_kline_options *options, tdx_error *err);

typedef struct cli_timeline_options {
    cli_common_options common;
    const char *date;
} cli_timeline_options;
int cli_command_timeline(const cli_timeline_options *options, tdx_error *err);

typedef struct cli_auction_options {
    cli_common_options common;
    int start;
    unsigned selector;
} cli_auction_options;
int cli_command_auction(const cli_auction_options *options, tdx_error *err);

typedef struct cli_snapshot_options {
    cli_common_options common;
    size_t batch_size;
} cli_snapshot_options;
int cli_command_snapshot(const cli_snapshot_options *options, tdx_error *err);

typedef struct cli_finance_options {
    cli_common_options common;
    size_t batch_size;
} cli_finance_options;
int cli_command_finance(const cli_finance_options *options, tdx_error *err);

typedef struct cli_capital_options {
    cli_common_options common;
    int capital_local;
    const char *gbbq_path;
} cli_capital_options;
int cli_command_capital(const cli_capital_options *options, tdx_error *err);

typedef struct cli_limits_options {
    cli_common_options common;
    size_t max_records;
    int start;
} cli_limits_options;
int cli_command_limits(const cli_limits_options *options, tdx_error *err);

typedef struct cli_jsn_options {
    cli_common_options common;
    const char *resource;
    const char *prefix;
    int bonds;
    int convertible;
    int schedule;
} cli_jsn_options;
int cli_command_jsn(const cli_jsn_options *options, tdx_error *err);

typedef struct cli_convertible_options {
    cli_common_options common;
    size_t max_records;
} cli_convertible_options;
int cli_command_convertible(const cli_convertible_options *options, tdx_error *err);

typedef struct cli_pending_options {
    cli_common_options common;
} cli_pending_options;
int cli_command_pending(const cli_pending_options *options, tdx_error *err);

typedef struct cli_subscription_options {
    cli_common_options common;
} cli_subscription_options;
int cli_command_subscription(const cli_subscription_options *options, tdx_error *err);

typedef struct cli_pricing_options {
    cli_common_options common;
    const char *date;
} cli_pricing_options;
int cli_command_pricing(const cli_pricing_options *options, tdx_error *err);

typedef struct cli_newbond_options {
    cli_common_options common;
} cli_newbond_options;
int cli_command_newbond(const cli_newbond_options *options, tdx_error *err);

typedef struct cli_professional_options {
    cli_common_options common;
    const char *input;
    const char *zip;
    const char *code;
    const char *kind;
    unsigned field_id;
    int from_date;
    int to_date;
    const char *zip_member; /* --member; --kind remains a legacy alias with --zip. */
} cli_professional_options;
int cli_command_professional(const cli_professional_options *options, tdx_error *err);

typedef struct cli_daily_options {
    cli_common_options common;
    size_t max_records;
    const char *input;
} cli_daily_options;
int cli_command_daily(const cli_daily_options *options, tdx_error *err);

typedef struct cli_minute_options {
    cli_common_options common;
    size_t max_records;
    const char *input;
} cli_minute_options;
int cli_command_minute(const cli_minute_options *options, tdx_error *err);

typedef struct cli_industry_options {
    cli_common_options common;
    size_t max_records;
    int bonds;
} cli_industry_options;
int cli_command_industry(const cli_industry_options *options, tdx_error *err);

typedef struct cli_limit_options {
    cli_common_options common;
    int has_previous_close;
    const char *date;
    const char *name;
    double previous_close;
} cli_limit_options;
int cli_command_limit(const cli_limit_options *options, tdx_error *err);

typedef struct cli_valuation_options {
    cli_common_options common;
    size_t max_records;
} cli_valuation_options;
int cli_command_valuation(const cli_valuation_options *options, tdx_error *err);

typedef struct cli_ranking_options {
    cli_common_options common;
    int start;
    const char *sort;
    int ascending;
    unsigned field_id;
} cli_ranking_options;
int cli_command_ranking(const cli_ranking_options *options, tdx_error *err);

typedef struct cli_seal_options {
    cli_common_options common;
    const char *date;
} cli_seal_options;
int cli_command_seal(const cli_seal_options *options, tdx_error *err);

typedef struct cli_panorama_options {
    cli_common_options common;
    size_t max_records;
    const char *view;
} cli_panorama_options;
int cli_command_panorama(const cli_panorama_options *options, tdx_error *err);

typedef struct cli_blocks_options {
    cli_common_options common;
    size_t max_records;
    int show_members;
    int show_assignments;
    int show_expanded;
} cli_blocks_options;
int cli_command_blocks(const cli_blocks_options *options, tdx_error *err);

#endif
