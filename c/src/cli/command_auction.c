/* auction: command orchestration. */
#include "cli_commands.h"
#include "tdx_auction.h"
#include "tdx_auction_json.h"
#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_trades.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



int cli_command_auction(const cli_auction_options *options, tdx_error *err) {
    tdx_auction_series series;
    tdx_auction_summary summary = {0};
    tdx_buf line = {0};
    FILE *stream = NULL;
    char endpoint[80];
    char server_date[TDX_TRADES_DATE_LENGTH + 1];
    const char *code;
    int market_id;
    unsigned limit;
    int status = TDX_ERR;
    size_t index;

    if (options->common.security_count != 1) {
        tdx_error_set(err, "auction needs exactly one --security");
        return TDX_ERR;
    }
    market_id = options->common.securities[0].market_id;
    code = options->common.securities[0].code;
    /* --limit defaults to 0, which the command reads as "use the usual 200". */
    limit = options->common.limit ? (unsigned)options->common.limit : 200u;
    if (limit > TDX_AUCTION_LIMIT_MAX) {
        tdx_error_set(err, "--limit must be in 1..%u for the auction",
                      (unsigned)TDX_AUCTION_LIMIT_MAX);
        return TDX_ERR;
    }

    tdx_buf_init(&line);
    tdx_auction_series_init(&series);
    memset(endpoint, 0, sizeof(endpoint));
    server_date[0] = '\0';
    if (tdx_auction_fetch_from_pool(&options->common.pool, options->common.timeout_ms, market_id, code,
                                    options->selector, (uint32_t)options->start, limit, &series,
                                    endpoint, sizeof(endpoint), server_date, err) != TDX_OK)
        goto done;

    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->common.output ? options->common.output : "<stdout>");
        goto done;
    }
    for (index = 0; index < series.count; ++index) {
        tdx_buf_clear(&line);
        if (tdx_auction_format_point(&line, &series.points[index], market_id, code, server_date,
                                     err) != TDX_OK)
            goto close_output;
        if (tdx_buf_push(&line, '\n', err) != TDX_OK)
            goto close_output;
        if (fwrite(line.data, 1, line.len, stream) != line.len) {
            tdx_error_set(err, "cannot write the auction stream");
            goto close_output;
        }
    }
    tdx_auction_summarize(&series, &summary);
    tdx_buf_clear(&line);
    if (tdx_auction_format_summary(&line, &series, &summary, market_id, code, server_date,
                                   endpoint, err) != TDX_OK)
        goto close_output;
    if (tdx_buf_push(&line, '\n', err) != TDX_OK)
        goto close_output;
    if (fwrite(line.data, 1, line.len, stream) != line.len) {
        tdx_error_set(err, "cannot write the auction summary");
        goto close_output;
    }
    status = TDX_OK;

close_output:
    status = cli_finish_output(stream, status, err);
    stream = NULL;
    if (status != TDX_OK)
        goto done;
    if (!options->common.quiet)
        fprintf(stderr,
                "auction %s %s: command=0x056A selector=%u points=%zu opening=%zu closing=%zu "
                "flips=%zu/%zu endpoint=%s date=%s\n",
                code, server_date[0] ? server_date : "today", options->selector,
                summary.point_count, summary.opening.point_count, summary.closing.point_count,
                summary.opening.unmatched_direction_flips,
                summary.closing.unmatched_direction_flips, endpoint,
                server_date[0] ? server_date : "-");

done:
    tdx_buf_free(&line);
    tdx_auction_series_free(&series);
    return status;
}
