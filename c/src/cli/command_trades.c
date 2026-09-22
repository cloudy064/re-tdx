/* trades: command orchestration. */
#include "cli_commands.h"
#include "cli_date.h"
#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_trades.h"
#include "tdx_trades_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



int cli_command_trades(const cli_trades_options *options, tdx_error *err) {
    tdx_trade_series series;
    tdx_trade_summary summary;
    tdx_buf line = {0};
    FILE *stream = NULL;
    char endpoint[80];
    char server_date[TDX_TRADES_DATE_LENGTH + 1];
    char trading_date[TDX_TRADES_DATE_LENGTH + 1];
    char requested[TDX_TRADES_DATE_LENGTH + 1] = {0};
    const char *code;
    int market_id;
    int page_size;
    int status = TDX_ERR;
    size_t index;

    if (options->common.security_count != 1) {
        tdx_error_set(err, "trades needs exactly one --security");
        return TDX_ERR;
    }
    if (options->date && cli_resolve_date(options->date, requested, NULL, err) != TDX_OK)
        return TDX_ERR;
    market_id = options->common.securities[0].market_id;
    code = options->common.securities[0].code;
    page_size = options->page_size
                    ? options->page_size
                    : (*requested ? TDX_TRADES_PAGE_SIZE_HISTORY : TDX_TRADES_PAGE_SIZE_TODAY);

    tdx_buf_init(&line);
    tdx_trades_series_init(&series);
    memset(&summary, 0, sizeof(summary));
    memset(endpoint, 0, sizeof(endpoint));
    server_date[0] = '\0';

    if (tdx_trades_fetch_from_pool(&options->common.pool, options->common.timeout_ms, market_id, code,
                                   requested, (uint16_t)page_size,
                                   (size_t)(options->max_pages ? options->max_pages
                                                               : TDX_TRADES_MAX_PAGES),
                                   &series, endpoint, sizeof(endpoint), server_date,
                                   err) != TDX_OK)
        goto done;
    /* 0x0FC5 carries no date of its own; 0x0004 supplied the server's. */
    snprintf(trading_date, sizeof(trading_date), "%s",
             *requested ? requested : server_date);

    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->common.output ? options->common.output : "<stdout>");
        goto done;
    }
    for (index = 0; index < series.count; ++index) {
        tdx_buf_clear(&line);
        if (tdx_trades_format_tick(&line, &series.ticks[index], market_id, code, trading_date,
                                   err) != TDX_OK)
            goto close_output;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto close_output;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the trade stream");
            goto close_output;
        }
    }
    tdx_trades_summarize(&series, &summary);
    tdx_buf_clear(&line);
    if (tdx_trades_format_summary(&line, &series, &summary, market_id, code, trading_date,
                                  endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the trade summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    status = cli_finish_output(stream, status, err);
    stream = NULL;
    if (status != TDX_OK)
        goto done;
    if (!options->common.quiet) {
        char first_label[8];
        char last_label[8];
        if (summary.has_times) {
            (void)tdx_trades_time_label(summary.first_time_minutes, first_label,
                                        sizeof(first_label));
            (void)tdx_trades_time_label(summary.last_time_minutes, last_label,
                                        sizeof(last_label));
        } else {
            snprintf(first_label, sizeof(first_label), "-");
            snprintf(last_label, sizeof(last_label), "-");
        }
        fprintf(stderr,
                "trades %s %s: command=%s pages=%zu ticks=%zu minutes=%zu volume_hand=%lld "
                "amount=%.2f vwap=%.4f %s..%s endpoint=%s date=%s\n",
                *requested ? requested : "today", code,
                series.history ? "0x0FC6" : "0x0FC5", series.pages, series.count,
                summary.minute_count, (long long)summary.volume_hand, summary.amount_yuan,
                summary.vwap, first_label, last_label, endpoint,
                trading_date[0] ? trading_date : "-");
    }

done:
    tdx_buf_free(&line);
    tdx_trades_series_free(&series);
    return status;
}
